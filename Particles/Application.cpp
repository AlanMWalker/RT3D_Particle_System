#include "Application.h"
#include "HeightMap.h"
#include "Aeroplane.h"
#include "ParticleSystem.h"

Application* Application::s_pApp = NULL;


const int CAMERA_MAP = 0;
const int CAMERA_PLANE = 1;
const int CAMERA_GUN = 2;
const int CAMERA_LIGHT = 3;
const int CAMERA_MAX = 4;

static const int RENDER_TARGET_WIDTH = 512;
static const int RENDER_TARGET_HEIGHT = 512;

static const float AEROPLANE_RADIUS = 6.f;

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

bool Application::HandleStart()
{
	s_pApp = this;

	this->SetWindowTitle("Hierarchy");

	m_pHeightMap = NULL;

	m_pAeroplane = NULL;
	m_pAeroplaneDefaultMeshes = NULL;
	m_pAeroplaneShadowMeshes = NULL;

	m_pRenderTargetColourTexture = NULL;
	m_pRenderTargetDepthStencilTexture = NULL;
	m_pRenderTargetColourTargetView = NULL;
	m_pRenderTargetColourTextureView = NULL;
	m_pRenderTargetDepthStencilTargetView = NULL;

	m_drawHeightMapShaderVSConstants.pCB = NULL;
	m_drawHeightMapShaderPSConstants.pCB = NULL;

	m_pRenderTargetDebugDisplayBuffer = NULL;

	m_shadowCastingLightPosition = XMFLOAT3(4.0f, 10.f, 0.f);
	m_shadowColour = XMFLOAT4(0.f, 0.f, 0.f, .25f);

	m_pShadowSamplerState = NULL;

	if (!this->CommonApp::HandleStart())
		return false;

	char aMaxNumLightsStr[100];
	_snprintf_s(aMaxNumLightsStr, sizeof aMaxNumLightsStr, _TRUNCATE, "%d", MAX_NUM_LIGHTS);

	const D3D_SHADER_MACRO aMacros[] = {
		{"MAX_NUM_LIGHTS", aMaxNumLightsStr, },
		{NULL},
	};

	if (!this->CompileShaderFromFile(&m_drawShadowCasterShader, "DrawShadowCaster.hlsl", aMacros, g_aVertexDesc_Pos3fColour4ubNormal3f, g_vertexDescSize_Pos3fColour4ubNormal3f))
		return false;

	// DrawHeightMap.hlsl
	{
		ID3D11VertexShader *pVS = NULL;
		ID3D11PixelShader *pPS = NULL;
		ID3D11InputLayout *pIL = NULL;
		ShaderDescription vs, ps;

		if (!CompileShadersFromFile(m_pD3DDevice, "DrawHeightMap.hlsl",
			"VSMain", &pVS, &vs,
			g_aVertexDesc_Pos3fColour4ubNormal3f, g_vertexDescSize_Pos3fColour4ubNormal3f, &pIL,
			"PSMain", &pPS, &ps,
			aMacros))
		{
			return false;
		}

		this->CreateShaderFromCompiledShader(&m_drawHeightMapShader, pVS, &vs, pIL, pPS, &ps);

		vs.FindCBuffer("DrawHeightMap", &m_drawHeightMapShaderVSConstants.slot);
		vs.FindFloat4x4(m_drawHeightMapShaderVSConstants.slot, "g_shadowMatrix", &m_drawHeightMapShaderVSConstants.shadowMatrix);
		vs.FindFloat4(m_drawHeightMapShaderVSConstants.slot, "g_shadowColour", &m_drawHeightMapShaderVSConstants.shadowColour);
		m_drawHeightMapShaderVSConstants.pCB = CreateBuffer(m_pD3DDevice, vs.GetCBufferSizeBytes(m_drawHeightMapShaderVSConstants.slot), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, NULL);

		ps.FindCBuffer("DrawHeightMap", &m_drawHeightMapShaderPSConstants.slot);
		ps.FindFloat4x4(m_drawHeightMapShaderPSConstants.slot, "g_shadowMatrix", &m_drawHeightMapShaderPSConstants.shadowMatrix);
		ps.FindFloat4(m_drawHeightMapShaderPSConstants.slot, "g_shadowColour", &m_drawHeightMapShaderPSConstants.shadowColour);
		m_drawHeightMapShaderPSConstants.pCB = CreateBuffer(m_pD3DDevice, ps.GetCBufferSizeBytes(m_drawHeightMapShaderPSConstants.slot), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, NULL);

		ps.FindTexture("g_shadowTexture", &m_heightMapShaderShadowTexture);
		ps.FindSamplerState("g_shadowSampler", &m_heightMapShaderShadowSampler);
	}

	if (!this->CreateRenderTarget())
		return false;

	m_bWireframe = false;

	m_pHeightMap = new HeightMap( "Resources/heightmap.bmp", 2.0f, &m_drawHeightMapShader );
	m_pAeroplane = new Aeroplane( 0.0f, 3.5f, 0.0f, 105.0f );

	m_pAeroplaneDefaultMeshes = AeroplaneMeshes::Load();
	if (!m_pAeroplaneDefaultMeshes)
		return false;

	// And the shadow copy.
	//
	// For a larger-scale project, with more complicated meshes, there would
    // probably be simplified meshes used for drawing the shadows. This example
    // just draws the same meshes using a different shader.
	m_pAeroplaneShadowMeshes = AeroplaneMeshes::Load();
	if (!m_pAeroplaneShadowMeshes)
		return false;

	m_pAeroplaneShadowMeshes->pGunMesh->SetShaderForAllSubsets(&m_drawShadowCasterShader);
	m_pAeroplaneShadowMeshes->pPlaneMesh->SetShaderForAllSubsets(&m_drawShadowCasterShader);
	m_pAeroplaneShadowMeshes->pPropMesh->SetShaderForAllSubsets(&m_drawShadowCasterShader);
	m_pAeroplaneShadowMeshes->pTurretMesh->SetShaderForAllSubsets(&m_drawShadowCasterShader);

	m_cameraZ = 50.0f;
	m_rotationAngle = 0.f;
	m_rotationSpeed = 0.01f;

	m_cameraState = CAMERA_MAP;

	// Initialise Particle Effects
	Effects::InitAll(m_pD3DDevice);

	m_pFire = new ParticleSystem();
	m_pRain = new ParticleSystem();

	m_RandomTexSRV = CreateRandomTexture1DSRV(m_pD3DDevice);

	std::vector<std::wstring> flares;
	flares.push_back(L"Textures\\flare0.dds");
	flares.push_back(L"Textures\\smoke.dds");
	m_FlareTexSRV = CreateTexture2DArraySRV(m_pD3DDevice, m_pD3DDeviceContext, flares, DXGI_FORMAT_R8G8B8A8_UNORM );
	
	m_pFire->Init(m_pD3DDevice, Effects::FireFX, m_FlareTexSRV, m_RandomTexSRV, 500); 
	m_pFire->SetEmitPos(XMFLOAT3(2.0f, 5.0f, 0.0f));

	std::vector<std::wstring> raindrops;
	raindrops.push_back(L"Textures\\raindrop.dds");
	m_RainTexSRV = CreateTexture2DArraySRV(m_pD3DDevice, m_pD3DDeviceContext, raindrops);

	m_pRain->Init(m_pD3DDevice, Effects::RainFX, m_RainTexSRV, m_RandomTexSRV, 10000); 


	return true;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

void Application::HandleStop()
{
	Release(m_pRenderTargetDebugDisplayBuffer);

	Release(m_pRenderTargetColourTexture);
	Release(m_pRenderTargetDepthStencilTexture);
	Release(m_pRenderTargetColourTargetView);
	Release(m_pRenderTargetColourTextureView);
	Release(m_pRenderTargetDepthStencilTargetView);

	Release(m_pShadowSamplerState);

	Release(m_drawHeightMapShaderVSConstants.pCB);
	Release(m_drawHeightMapShaderPSConstants.pCB);

	delete m_pHeightMap;

	delete m_pAeroplane;
	delete m_pAeroplaneDefaultMeshes;
	delete m_pAeroplaneShadowMeshes;

	m_drawShadowCasterShader.Reset();

	Release(m_FlareTexSRV);
	Release(m_RainTexSRV);

	Effects::DestroyAll();

	delete m_pFire;
	delete m_pRain;

	this->CommonApp::HandleStop();
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

void Application::HandleUpdate()
{
	m_rotationAngle += m_rotationSpeed;

	if( m_cameraState == CAMERA_MAP )
	{
		if (this->IsKeyPressed('Q'))
			m_cameraZ -= 2.0f;
		
		if (this->IsKeyPressed('A'))
			m_cameraZ += 2.0f;

		static bool dbS = false;

		if (this->IsKeyPressed(VK_SPACE))
		{
			if( !dbS )
			{
				if( m_rotationSpeed == 0.0f )
					m_rotationSpeed = 0.01f;
				else
					m_rotationSpeed = 0.0f;

				dbS = true;
			}
		}
		else
		{
			dbS = false;
		}
	}

	
	static bool dbC = false;

	if (this->IsKeyPressed('C') )	
	{
		if( !dbC )
		{
			if( ++m_cameraState == CAMERA_MAX )
				m_cameraState = CAMERA_MAP;

			dbC = true;
		}
	}
	else
	{
		dbC = false;
	}

	if( m_cameraState != CAMERA_PLANE && m_cameraState != CAMERA_GUN )
	{
		if( this->IsKeyPressed(VK_LEFT) )
			m_shadowCastingLightPosition.x+=.2f;

		if( this->IsKeyPressed(VK_RIGHT) )
			m_shadowCastingLightPosition.x-=.2f;

		if( this->IsKeyPressed(VK_UP ) )
			m_shadowCastingLightPosition.z+=.2f;

		if( this->IsKeyPressed(VK_DOWN ) )
			m_shadowCastingLightPosition.z-=.2f;

		if( this->IsKeyPressed(VK_PRIOR ) )
			m_shadowCastingLightPosition.y-=.2f;

		if( this->IsKeyPressed(VK_NEXT ) )
			m_shadowCastingLightPosition.y+=.2f;
	}

	static bool dbW = false;
	if (this->IsKeyPressed('W') )	
	{
		if( !dbW )
		{
			m_bWireframe = !m_bWireframe;
			this->SetRasterizerState( false, m_bWireframe );
			dbW = true;
		}
	}
	else
	{
		dbW = false;
	}

	
	m_pAeroplane->Update( m_cameraState != CAMERA_MAP );
	
	// This should be based on a real timer!
	static float seed = 0.0f;
	static float dt = 0.02f;
	seed += 0.001f;
	m_pFire->Update(dt, seed);
	m_pRain->Update(dt, seed);

}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

void Application::HandleRender()
{
	this->Render3D();
}


void Application::Render3D()
{
	this->SetRasterizerState( false, m_bWireframe );
	this->SetDepthStencilState(true);

	XMFLOAT3 vUpVector(0.0f, 1.0f, 0.0f);
	XMFLOAT3 vCamera, vLookat;

	switch( m_cameraState )
	{
		case CAMERA_MAP:
			vCamera = XMFLOAT3(sin(m_rotationAngle)*m_cameraZ, m_cameraZ / 4, cos(m_rotationAngle)*m_cameraZ);
			vLookat = XMFLOAT3(0.0f, 4.0f, 0.0f);
			break;
		case CAMERA_PLANE:
			m_pAeroplane->SetGunCamera( false );
			vCamera = XMFLOAT3(m_pAeroplane->GetCameraPosition().x, m_pAeroplane->GetCameraPosition().y, m_pAeroplane->GetCameraPosition().z);
			vLookat = XMFLOAT3(m_pAeroplane->GetFocusPosition().x, m_pAeroplane->GetFocusPosition().y, m_pAeroplane->GetFocusPosition().z);
			break;
		case CAMERA_GUN:
			m_pAeroplane->SetGunCamera( true );
			vCamera = XMFLOAT3(m_pAeroplane->GetCameraPosition().x, m_pAeroplane->GetCameraPosition().y, m_pAeroplane->GetCameraPosition().z);
			vLookat = XMFLOAT3(m_pAeroplane->GetFocusPosition().x, m_pAeroplane->GetFocusPosition().y, m_pAeroplane->GetFocusPosition().z);
			break;
		case CAMERA_LIGHT:
			vCamera = m_shadowCastingLightPosition;
			vLookat = XMFLOAT3(m_pAeroplane->GetFocusPosition().x, m_pAeroplane->GetFocusPosition().y, m_pAeroplane->GetFocusPosition().z);
			break;
			
	}

	XMMATRIX  matView;
	matView = XMMatrixLookAtLH(XMLoadFloat3(&vCamera), XMLoadFloat3(&vLookat), XMLoadFloat3(&vUpVector));

	XMMATRIX matProj;
	matProj = XMMatrixPerspectiveFovLH(float(XM_PI / 4), 2, 1.5f, 5000.0f);


	this->SetViewMatrix(matView);
	this->SetProjectionMatrix(matProj);

	this->EnablePointLight(0, XMFLOAT3(100.0f, 100.f, -100.f), XMFLOAT3(1.f, 1.f, 1.f));
	this->SetLightAttenuation(0, 200.f, 2.f, 2.f, 2.f);
	this->EnableDirectionalLight(1, XMFLOAT3(-1.f, -1.f, -1.f), XMFLOAT3(0.65f, 0.55f, 0.65f));

	this->Clear(XMFLOAT4(.2f, .2f, .6f, 1.f));

	D3DXMATRIX  matWorld;
	D3DXMatrixIdentity( &matWorld );
	this->SetWorldMatrix(matWorld);

	m_pHeightMap->Draw();
	m_pAeroplane->Draw(m_pAeroplaneDefaultMeshes);

	D3DXMATRIX worldMtx;
	D3DXMatrixTranslation(&worldMtx, m_shadowCastingLightPosition.x,  m_shadowCastingLightPosition.y,  m_shadowCastingLightPosition.z);
	this->SetWorldMatrix(worldMtx);
	

	// Draw particle systems last so it is blended with scene.
	float blendFactor[] = {0.0f, 0.0f, 0.0f, 0.0f};

	m_pFire->SetEyePos(vCamera);
	XMMATRIX matVP = matView*matProj;
	m_pFire->Draw(m_pD3DDeviceContext, matVP);
	
	m_pD3DDeviceContext->OMSetBlendState(0, blendFactor, 0xffffffff); // restore default


	m_pRain->SetEyePos(vCamera);
	m_pRain->SetEmitPos(vCamera);
	//m_pRain->Draw(m_pD3DDeviceContext, matVP);

	// restore default states.

	m_pD3DDeviceContext->RSSetState(0);
	m_pD3DDeviceContext->OMSetDepthStencilState(0, 0);
	
	m_pD3DDeviceContext->OMSetBlendState(0, blendFactor, 0xffffffff); 
	



}


bool Application::CreateRenderTarget()
{
	// Create render target texture, and two views for it - one for using it as
    // a render target, and one for using it as a texture.
	{
		HRESULT hr;

		D3D11_TEXTURE2D_DESC td;

		td.Width = RENDER_TARGET_WIDTH;
		td.Height = RENDER_TARGET_HEIGHT;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		td.SampleDesc.Count = 1;
		td.SampleDesc.Quality = 0;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		td.CPUAccessFlags = 0;
		td.MiscFlags = 0;

		hr = m_pD3DDevice->CreateTexture2D(&td, NULL, &m_pRenderTargetColourTexture);
		if (FAILED(hr))
		{
			this->SetStartErrorMessage("Failed to create render target colour texture.");
			return false;
		}

		D3D11_RENDER_TARGET_VIEW_DESC rtvd;

		rtvd.Format = td.Format;
		rtvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvd.Texture2D.MipSlice = 0;

		hr = m_pD3DDevice->CreateRenderTargetView(m_pRenderTargetColourTexture, &rtvd, &m_pRenderTargetColourTargetView);
		if (FAILED(hr))
		{
			this->SetStartErrorMessage("Failed to create render target colour target view.");
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC svd;

		svd.Format = td.Format;
		svd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		svd.Texture2D.MostDetailedMip = 0;
		svd.Texture2D.MipLevels = 1;

		hr = m_pD3DDevice->CreateShaderResourceView(m_pRenderTargetColourTexture, &svd, &m_pRenderTargetColourTextureView);
		if (FAILED(hr))
		{
			this->SetStartErrorMessage("Failed to create render target colour texture view.");
			return false;
		}
	}

	// Do the same again, roughly, for its matching depth buffer. (There is a
	// default depth buffer, but it can't really be re-used for this, because it
	// tracks the window size and the window could end up too small.)
	{
		HRESULT hr;

		D3D11_TEXTURE2D_DESC td;

		td.Width = RENDER_TARGET_WIDTH;
		td.Height = RENDER_TARGET_HEIGHT;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_D16_UNORM;
		td.SampleDesc.Count = 1;
		td.SampleDesc.Quality = 0;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		td.CPUAccessFlags = 0;
		td.MiscFlags = 0;

		hr = m_pD3DDevice->CreateTexture2D(&td, NULL, &m_pRenderTargetDepthStencilTexture);
		if (FAILED(hr))
		{
			this->SetStartErrorMessage("Failed to create render target depth/stencil texture.");
			return false;
		}

		D3D11_DEPTH_STENCIL_VIEW_DESC dsvd;

		dsvd.Format = td.Format;
		dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		dsvd.Flags = 0;
		dsvd.Texture2D.MipSlice = 0;

		hr = m_pD3DDevice->CreateDepthStencilView(m_pRenderTargetDepthStencilTexture, &dsvd, &m_pRenderTargetDepthStencilTargetView);
		if (FAILED(hr))
		{
			this->SetStartErrorMessage("Failed to create render target depth/stencil view.");
			return false;
		}
	}

	// VB for drawing render target on screen.
	static const Vertex_Pos3fColour4ubTex2f aRenderTargetDebugDisplayBufferVtxs[] = {
		Vertex_Pos3fColour4ubTex2f(XMFLOAT3(0.f, 0.f, 0.f), VertexColour(255, 255, 255, 255), XMFLOAT2(0.f, 1.f)),
		Vertex_Pos3fColour4ubTex2f(XMFLOAT3(256.f, 0.f, 0.f), VertexColour(255, 255, 255, 255), XMFLOAT2(1.f, 1.f)),
		Vertex_Pos3fColour4ubTex2f(XMFLOAT3(0.f, 256.f, 0.f), VertexColour(255, 255, 255, 255), XMFLOAT2(0.f, 0.f)),
		Vertex_Pos3fColour4ubTex2f(XMFLOAT3(256.f, 256.f, 0.f), VertexColour(255, 255, 255, 255), XMFLOAT2(1.f, 0.f)),
	};

	m_pRenderTargetDebugDisplayBuffer = CreateImmutableVertexBuffer(m_pD3DDevice, sizeof aRenderTargetDebugDisplayBufferVtxs, aRenderTargetDebugDisplayBufferVtxs);

	// Sampler state for using the shadow texture to draw with.
	{
		D3D11_SAMPLER_DESC sd;

		sd.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;

		// Use BORDER addressing, so that anything outside the area the shadow
        // texture casts on can be given a specific fixed colour.
		sd.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		sd.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		sd.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;

		sd.MipLODBias = 0.f;
		sd.MaxAnisotropy = 16;
		sd.ComparisonFunc = D3D11_COMPARISON_NEVER;

		// Set the border colour to transparent, corresponding to unshadowed.
		sd.BorderColor[0] = 0.f;
		sd.BorderColor[1] = 0.f;
		sd.BorderColor[2] = 0.f;
		sd.BorderColor[3] = 0.f;

		sd.MinLOD = 0.f;
		sd.MaxLOD = D3D11_FLOAT32_MAX;

		if (FAILED(m_pD3DDevice->CreateSamplerState(&sd, &m_pShadowSamplerState)))
			return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////


int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int)
{
	Application application;

	Run(&application);

	return 0;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
