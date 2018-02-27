#include "HeightMap.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

HeightMap::HeightMap(char* filename, float gridSize, CommonApp::Shader *pShader)
{
	m_pShader = pShader;

	LoadHeightMap(filename, gridSize);

	m_pHeightMapBuffer = NULL;

	static const VertexColour MAP_COLOUR(200, 255, 255, 255);

	m_HeightMapVtxCount = (m_HeightMapLength - 1)*(m_HeightMapWidth - 1) * 6;
	m_pMapVtxs = new Vertex_Pos3fColour4ubNormal3f[ m_HeightMapVtxCount ];
	
	int vtxIndex = 0;
	int mapIndex = 0;

	XMVECTOR v0, v1, v2, v3;

	for( int l = 0; l < m_HeightMapLength; ++l )
	{
		for( int w = 0; w < m_HeightMapWidth; ++w )
		{	
			if( w < m_HeightMapWidth-1 && l < m_HeightMapLength-1 )
			{
				v0 = XMLoadFloat3(&m_pHeightMap[mapIndex]);
				v1 = XMLoadFloat3(&m_pHeightMap[mapIndex + m_HeightMapWidth]);
				v2 = XMLoadFloat3(&m_pHeightMap[mapIndex + 1]);
				v3 = XMLoadFloat3(&m_pHeightMap[mapIndex + m_HeightMapWidth + 1]);

				XMVECTOR vA = v0 - v1;
				XMVECTOR vB = v1 - v2;
				XMVECTOR vC = v3 - v1;
			
				XMVECTOR vN1, vN2;
				vN1 = XMVector3Cross(vA, vB);
				vN1 = XMVector3Normalize(vN1);

				vN2 = XMVector3Cross(vB, vC);
				vN2 = XMVector3Normalize(vN2);

				VertexColour CUBE_COLOUR;

				m_pMapVtxs[vtxIndex+0] = Vertex_Pos3fColour4ubNormal3f(v0, MAP_COLOUR, vN1 );
				m_pMapVtxs[vtxIndex+1] = Vertex_Pos3fColour4ubNormal3f(v1, MAP_COLOUR, vN1 );
				m_pMapVtxs[vtxIndex+2] = Vertex_Pos3fColour4ubNormal3f(v2, MAP_COLOUR, vN1 );
				m_pMapVtxs[vtxIndex+3] = Vertex_Pos3fColour4ubNormal3f(v2, MAP_COLOUR, vN2 );
				m_pMapVtxs[vtxIndex+4] = Vertex_Pos3fColour4ubNormal3f(v1, MAP_COLOUR, vN2 );
				m_pMapVtxs[vtxIndex+5] = Vertex_Pos3fColour4ubNormal3f(v3, MAP_COLOUR, vN2 );

				vtxIndex += 6;
			}
			mapIndex++;

		}
	}

	m_pHeightMapBuffer = CreateImmutableVertexBuffer(Application::s_pApp->GetDevice(), sizeof Vertex_Pos3fColour4ubNormal3f * m_HeightMapVtxCount, m_pMapVtxs);
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

HeightMap::~HeightMap()
{
	Release(m_pHeightMapBuffer);
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

void HeightMap::Draw( void )
{
	CommonApp::Shader *pShader = m_pShader;
	if (!pShader)
		pShader = Application::s_pApp->GetUntexturedLitShader();

	Application::s_pApp->DrawWithShader(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, m_pHeightMapBuffer, sizeof(Vertex_Pos3fColour4ubNormal3f), NULL, 0, m_HeightMapVtxCount, NULL, NULL, pShader);

}

//////////////////////////////////////////////////////////////////////
// LoadHeightMap
// Original code sourced from rastertek.com
//////////////////////////////////////////////////////////////////////
bool HeightMap::LoadHeightMap(char* filename, float gridSize )
{
	FILE* filePtr;
	int error;
	unsigned int count;
	BITMAPFILEHEADER bitmapFileHeader;
	BITMAPINFOHEADER bitmapInfoHeader;
	int imageSize, i, j, k, index;
	unsigned char* bitmapImage;
	unsigned char height;


	// Open the height map file in binary.
	error = fopen_s(&filePtr, filename, "rb");
	if(error != 0)
	{
		return false;
	}

	// Read in the file header.
	count = fread(&bitmapFileHeader, sizeof(BITMAPFILEHEADER), 1, filePtr);
	if(count != 1)
	{
		return false;
	}

	// Read in the bitmap info header.
	count = fread(&bitmapInfoHeader, sizeof(BITMAPINFOHEADER), 1, filePtr);
	if(count != 1)
	{
		return false;
	}

	// Save the dimensions of the terrain.
	m_HeightMapWidth = bitmapInfoHeader.biWidth;
	m_HeightMapLength = bitmapInfoHeader.biHeight;

	// Calculate the size of the bitmap image data.
	imageSize = m_HeightMapWidth * m_HeightMapLength * 3;

	// Allocate memory for the bitmap image data.
	bitmapImage = new unsigned char[imageSize];
	if(!bitmapImage)
	{
		return false;
	}

	// Move to the beginning of the bitmap data.
	fseek(filePtr, bitmapFileHeader.bfOffBits, SEEK_SET);

	// Read in the bitmap image data.
	count = fread(bitmapImage, 1, imageSize, filePtr);
	if(count != imageSize)
	{
		return false;
	}

	// Close the file.
	error = fclose(filePtr);
	if(error != 0)
	{
		return false;
	}

	// Create the structure to hold the height map data.
	XMFLOAT3* pUnsmoothedMap = new XMFLOAT3[m_HeightMapWidth * m_HeightMapLength];
	m_pHeightMap = new XMFLOAT3[m_HeightMapWidth * m_HeightMapLength];

	if(!m_pHeightMap)
	{
		return false;
	}

	// Initialize the position in the image data buffer.
	k=0;

	// Read the image data into the height map.
	for(j=0; j<m_HeightMapLength; j++)
	{
		for(i=0; i<m_HeightMapWidth; i++)
		{
			height = bitmapImage[k];
			
			index = (m_HeightMapWidth * j) + i;

			m_pHeightMap[index].x = (float)(i-(m_HeightMapWidth/2))*gridSize;
			m_pHeightMap[index].y = (float)height/6*gridSize;
			m_pHeightMap[index].z = (float)(j-(m_HeightMapLength/2))*gridSize;

			pUnsmoothedMap[index].y = (float)height/6*gridSize;

			k+=3;
		}
	}


	// Smoothing the landscape makes a big difference to the look of the shading
	for( int s=0; s<2; ++s )
	{
		for(j=0; j<m_HeightMapLength; j++)
		{
			for(i=0; i<m_HeightMapWidth; i++)
			{	
				index = (m_HeightMapWidth * j) + i;

				if( j>0 && j<m_HeightMapLength-1 && i>0 && i<m_HeightMapWidth-1 )
				{
					m_pHeightMap[index].y = 0.0f;
					m_pHeightMap[index].y += pUnsmoothedMap[index-m_HeightMapWidth-1].y	+ pUnsmoothedMap[index-m_HeightMapWidth].y + pUnsmoothedMap[index-m_HeightMapWidth+1].y;
					m_pHeightMap[index].y += pUnsmoothedMap[index-1].y	+ pUnsmoothedMap[index].y + pUnsmoothedMap[index+1].y;
					m_pHeightMap[index].y += pUnsmoothedMap[index+m_HeightMapWidth-1].y	+ pUnsmoothedMap[index+m_HeightMapWidth].y + pUnsmoothedMap[index+m_HeightMapWidth+1].y;
					m_pHeightMap[index].y /= 9;
				}		
			}
		}

		for(j=0; j<m_HeightMapLength; j++)
		{
			for(i=0; i<m_HeightMapWidth; i++)
			{	
				index = (m_HeightMapWidth * j) + i;
				pUnsmoothedMap[index].y = m_pHeightMap[index].y;
			}
		}

	}

	// Release the bitmap image data.
	delete [] bitmapImage;
	delete [] pUnsmoothedMap;
	bitmapImage = 0;

	return true;
}


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////


