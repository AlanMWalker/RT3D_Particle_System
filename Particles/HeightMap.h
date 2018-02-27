#ifndef HEIGHTMAP_H
#define HEIGHTMAP_H

//**********************************************************************
// File:			HeightMap.h
// Description:		A simple class to represent a heightmap
// Module:			Real-Time 3D Techniques for Games
// Created:			Jake - 2010-2011
// Notes:			
//**********************************************************************

#include "Application.h"

class HeightMap
{
public:
	// If pShader is NULL, a default shader will be used.
	HeightMap(char* filename, float gridSize, CommonApp::Shader *pShader);
	~HeightMap();

	void Draw( void );

private:
	bool LoadHeightMap(char* filename, float gridSize);

	ID3D11Buffer *m_pHeightMapBuffer;

	int m_HeightMapWidth;
	int m_HeightMapLength;
	int m_HeightMapVtxCount;
	int m_HeightMapFaceCount;
	int m_FacesPerRow;
	XMFLOAT3* m_pHeightMap;
	Vertex_Pos3fColour4ubNormal3f* m_pMapVtxs;
	CommonApp::Shader *m_pShader;

};

#endif