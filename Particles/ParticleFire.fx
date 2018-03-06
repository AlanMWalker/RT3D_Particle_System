//=============================================================================
// Fire.fx by Frank Luna (C) 2011 All Rights Reserved.
//
// Fire particle system.  Particles are emitted directly in world space.
//=============================================================================


//***********************************************
// GLOBALS                                      *
//***********************************************

cbuffer cbPerFrame
{
	float3 gEyePosW;

	// for when the emit position/direction is varying
	float3 gEmitPosW;
	float3 gEmitDirW;

	float gGameTime;
	float gTimeStep;
	float4x4 gViewProj;
};

cbuffer cbFixed
{
	// Net constant acceleration used to accerlate the particles.
	float3 gAccelW = { 0.0f, 3.0f, 0.0f };

	// Texture coordinates used to stretch texture over quad 
	// when we expand point particle into a quad.
	float2 gQuadTexC[4] =
	{
		float2(0.0f, 1.0f),
		float2(1.0f, 1.0f),
		float2(0.0f, 0.0f),
		float2(1.0f, 0.0f)
	};
};

// Array of textures for texturing the particles.
Texture2DArray gTexArray;

// Random texture used to generate random numbers in shaders.
Texture1D gRandomTex;

SamplerState samLinear
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = WRAP;
	AddressV = WRAP;
};

DepthStencilState DisableDepth
{
	DepthEnable = FALSE;
	DepthWriteMask = ZERO;
};

DepthStencilState NoDepthWrites
{
	DepthEnable = TRUE;
	DepthWriteMask = ZERO;
};

BlendState AdditiveBlending
{
	AlphaToCoverageEnable = FALSE;
	BlendEnable[0] = TRUE;
	SrcBlend = SRC_ALPHA;
	DestBlend = ONE;
	BlendOp = ADD;
	SrcBlendAlpha = ZERO;
	DestBlendAlpha = ZERO;
	BlendOpAlpha = ADD;
	RenderTargetWriteMask[0] = 0x0F;
};

//***********************************************
// HELPER FUNCTIONS                             *
//***********************************************
float3 RandUnitVec3(float offset)
{
	// Use game time plus offset to sample random texture.
	float u = (gGameTime + offset);

	// coordinates in [-1,1]
	float3 v = gRandomTex.SampleLevel(samLinear, u, 0).xyz;

	// project onto unit sphere
	return normalize(v);
}

//***********************************************
// STREAM-OUT TECH                              *
//***********************************************

#define PT_EMITTER 0
//#define PT_FLARE 1
#define PT_SMOKE 2

//#define DEBUG_SHADER

struct Particle
{
	float3 InitialPosW : POSITION;
	float3 InitialVelW : VELOCITY;
	float2 SizeW       : SIZE;
	float Age : AGE;
	float RotationSpeed : ROTATION_SPEED;
	uint Type          : TYPE;
};

Particle StreamOutVS(Particle vin)
{//Pass through vertex shader
	return vin;
}

// The stream-out GS is just responsible for emitting 
// new particles and destroying old particles.  The logic
// programed here will generally vary from particle system
// to particle system, as the destroy/spawn rules will be 
// different.
[maxvertexcount(2)]
void StreamOutGS(point Particle gin[1],
	inout PointStream<Particle> ptStream)
{
	gin[0].Age += gTimeStep;

	if (gin[0].Type == PT_EMITTER)
	{
		// time to emit a new particle?
#ifndef DEBUG_SHADER
		if (gin[0].Age > 0.005f)
#else
		if (gin[0].Age > 1.0f)

#endif
		{
			float3 vRandom = RandUnitVec3(0.0f);
			float3 vRandom2 = RandUnitVec3(0.01f);
			vRandom.x *= 0.5f;
			vRandom.z *= 0.5f;

			Particle p;
			p.InitialPosW = gEmitPosW.xyz + (vRandom * 1.0);
			p.InitialVelW = vRandom2;
			p.SizeW = float2(3.0f, 3.0f);
			p.Age = 0.0f;
#ifdef PT_FLARE
			p.Type = PT_FLARE;
#else
			p.Type = PT_SMOKE;
#endif
			p.RotationSpeed = radians(720) *vRandom.x;

			ptStream.Append(p);

			// reset the time to emit
			gin[0].Age = 0.0f;
		}

		// always keep emitters
		ptStream.Append(gin[0]);
	}
	else
	{
		// Specify conditions to keep particle; this may vary from system to system.
		if (gin[0].Age <= 1.0f)
			ptStream.Append(gin[0]);
	}
}

GeometryShader gsStreamOut = ConstructGSWithSO(
	CompileShader(gs_5_0, StreamOutGS()),
	"POSITION.xyz; VELOCITY.xyz; SIZE.xy; AGE.x; ROTATION_SPEED.x; TYPE.x");

technique11 StreamOutTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, StreamOutVS()));
		SetGeometryShader(gsStreamOut);

		// disable pixel shader for stream-out only
		SetPixelShader(NULL);

		// we must also disable the depth buffer for stream-out only
		SetDepthStencilState(DisableDepth, 0);
	}
}

//***********************************************
// DRAW TECH                                    *
//***********************************************

struct VertexOut
{
	float3 PosW  : POSITION;
	float2 SizeW : SIZE;
	float4 Color : COLOR;
	float RotAng : ROT_ANGLE;
	uint   Type : TYPE;
};

VertexOut DrawVS(Particle vin)
{
	VertexOut vout;

	float t = vin.Age;

	// constant acceleration equation
	vout.PosW = 0.5f*t*t*gAccelW + t*vin.InitialVelW + vin.InitialPosW;

	// fade color with time
	float opacity = 1.0f - smoothstep(0.0f, 1.0f, t / 1.0f);
	vout.Color = float4(1.0f, 1.0f, 1.0f, opacity);

	vout.SizeW = vin.SizeW;
	vout.RotAng = t * vin.RotationSpeed;

	vout.Type = vin.Type;
	return vout;
}

struct GeoOut
{
	float4 PosH  : SV_Position;
	float4 Color : COLOR;
	float2 Tex   : TEXCOORD;
};

// The draw GS just expands points into camera facing quads.
[maxvertexcount(4)]
void DrawGS(point VertexOut gin[1],
	inout TriangleStream<GeoOut> triStream)
{
	// do not draw emitter particles.
	if (gin[0].Type != PT_EMITTER)
	{
		//
		// Compute world matrix so that billboard faces the camera.
		//
		float3 look = normalize(gEyePosW.xyz - gin[0].PosW);
		float3 right = normalize(cross(float3(0, 1, 0), look));
		float3 up = cross(look, right);

		//
		// Compute triangle strip vertices (quad) in world space.
		//
		const float halfWidth = 0.5f*gin[0].SizeW.x;
		const float halfHeight = 0.5f*gin[0].SizeW.y;
		
		const float sinVerts[4] =
		{
			sin(radians(135.0f) + gin[0].RotAng),
			sin(radians(45.0f) + gin[0].RotAng),
			sin(radians(225.0f) + gin[0].RotAng),
			sin(radians(315.0f) + gin[0].RotAng)
		};

		const float cosVerts[4] =
		{
			cos(radians(135.0f) + gin[0].RotAng),
			cos(radians(45.0f) + gin[0].RotAng),
			cos(radians(225.0f) + gin[0].RotAng),
			cos(radians(315.0f) + gin[0].RotAng)
		};

		const float4 v[4] =
		{
			float4(gin[0].PosW + sinVerts[0] * (halfWidth*right) + cosVerts[0] * (halfHeight * up), 1.0f),
			float4(gin[0].PosW + sinVerts[1] * (halfWidth*right) + cosVerts[1] * (halfHeight * up), 1.0f),
			float4(gin[0].PosW + sinVerts[2] * (halfWidth*right) + cosVerts[2] * (halfHeight * up), 1.0f),
			float4(gin[0].PosW + sinVerts[3] * (halfWidth*right) + cosVerts[3] * (halfHeight * up), 1.0f)
		};

		//
		// Transform quad vertices to world space and output 
		// them as a triangle strip.
		//
		GeoOut gout;
		[unroll]
		for (int i = 0; i < 4; ++i)
		{
			gout.PosH = mul(v[i], gViewProj);
			gout.Tex = gQuadTexC[i];
#ifdef PT_SMOKE
			float4 col = gin[0].Color;
			col.w = 0.01;
			//float percent = degrees(gin[0].RotAng) / 360.0;
			//col.x = percent * 256;
			gout.Color = col;

#else
			gout.Color = gin[0].Color;
#endif 
			triStream.Append(gout);

		}
	}
}

float4 DrawPS(GeoOut pin) : SV_TARGET
{
#ifdef DEBUG_SHADER
	return float4(1.0, 0.0, 0.0, 1.0);
#endif

#ifdef PT_FLARE
	return gTexArray.Sample(samLinear, float3(pin.Tex, 0))*pin.Color;
#else
	return gTexArray.Sample(samLinear, float3(pin.Tex, 1))*pin.Color;
#endif
}

technique11 DrawTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, DrawVS()));
		SetGeometryShader(CompileShader(gs_5_0, DrawGS()));
		SetPixelShader(CompileShader(ps_5_0, DrawPS()));

		SetBlendState(AdditiveBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xffffffff);
		SetDepthStencilState(NoDepthWrites, 0);
	}
}