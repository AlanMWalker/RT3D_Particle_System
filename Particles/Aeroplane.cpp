//*********************************************************************************************
// File:			Aeroplane.cpp
// Description:		A very simple class to represent an aeroplane as one object with all the 
//					hierarchical components stored internally within the class.
// Module:			Real-Time 3D Techniques for Games
// Created:			Jake - 2010-2011
// Notes:			
//*********************************************************************************************

#include "Aeroplane.h"

AeroplaneMeshes *AeroplaneMeshes::Load()
{
	AeroplaneMeshes *pMeshes = new AeroplaneMeshes;

	pMeshes->pPlaneMesh = CommonMesh::LoadFromXFile(Application::s_pApp, "Resources/Plane/plane.x");
	pMeshes->pPropMesh = CommonMesh::LoadFromXFile(Application::s_pApp, "Resources/Plane/prop.x");
	pMeshes->pTurretMesh = CommonMesh::LoadFromXFile(Application::s_pApp, "Resources/Plane/turret.x");
	pMeshes->pGunMesh = CommonMesh::LoadFromXFile(Application::s_pApp, "Resources/Plane/gun.x");

	if (!pMeshes->pPlaneMesh || !pMeshes->pPropMesh || !pMeshes->pTurretMesh || !pMeshes->pGunMesh)
	{
		delete pMeshes;
		return NULL;
	}

	return pMeshes;
}

AeroplaneMeshes::AeroplaneMeshes() :
pPlaneMesh(NULL),
pPropMesh(NULL),
pTurretMesh(NULL),
pGunMesh(NULL)
{
}

AeroplaneMeshes::~AeroplaneMeshes()
{
	delete this->pPlaneMesh;
	delete this->pPropMesh;
	delete this->pTurretMesh;
	delete this->pGunMesh;
}

Aeroplane::Aeroplane( float fX, float fY, float fZ, float fRotY )
{
	m_mWorldMatrix = XMMatrixIdentity();
	m_mPropWorldMatrix = XMMatrixIdentity();
	m_mTurretWorldMatrix = XMMatrixIdentity();
	m_mGunWorldMatrix = XMMatrixIdentity();
	m_mCamWorldMatrix = XMMatrixIdentity();

	m_v4Rot = XMFLOAT4(0.0f, fRotY, 0.0f, 0.0f);
	m_v4Pos = XMFLOAT4(fX, fY, fZ, 0.0f);

	m_v4PropOff = XMFLOAT4(0.0f, 0.0f, 1.9f, 0.0f);
	m_v4PropRot = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

	m_v4TurretOff = XMFLOAT4(0.0f, 1.05f, -1.3f, 0.0f);
	m_v4TurretRot = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

	m_v4GunOff = XMFLOAT4(0.0f, 0.5f, 0.0f, 0.0f);
	m_v4GunRot = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

	m_v4CamOff = XMFLOAT4(0.0f, 4.5f, -15.0f, 0.0f);
	m_v4CamRot = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

	m_vCamWorldPos = XMVectorZero();
	m_vForwardVector = XMVectorZero();

	m_fSpeed = 0.0f;

	m_bGunCam = false;
}

Aeroplane::~Aeroplane( void )
{
}

void Aeroplane::SetWorldPosition( float fX, float fY, float fZ  )
{
	m_v4Pos = XMFLOAT4(fX, fY, fZ, 0.0f);
	UpdateMatrices();
}


void Aeroplane::UpdateMatrices( void )
{
	XMMATRIX mRotX, mRotY, mRotZ, mTrans;
	XMMATRIX mPlaneCameraRot;
	
	m_mWorldTrans = XMMatrixTranslation(m_v4Pos.x, m_v4Pos.y, m_v4Pos.z);
	m_mWorldMatrix = XMMatrixRotationRollPitchYaw(XMConvertToRadians(m_v4Rot.x), XMConvertToRadians(m_v4Rot.y), XMConvertToRadians(m_v4Rot.z)) *m_mWorldTrans;

	mTrans = XMMatrixTranslation(m_v4PropOff.x, m_v4PropOff.y, m_v4PropOff.z);
	m_mPropWorldMatrix =  mTrans * m_mWorldMatrix;

	mTrans = XMMatrixTranslation(m_v4TurretOff.x, m_v4TurretOff.y, m_v4TurretOff.z);
	m_mTurretWorldMatrix =  mTrans * m_mWorldMatrix;

	mTrans = XMMatrixTranslation(m_v4GunOff.x, m_v4GunOff.y, m_v4GunOff.z);
	m_mGunWorldMatrix =  mTrans * m_mTurretWorldMatrix;

	mTrans = XMMatrixTranslation(m_v4CamOff.x, m_v4CamOff.y, m_v4CamOff.z);
	m_mCamWorldMatrix =  mTrans * mPlaneCameraRot;
	
	m_vCamWorldPos = m_mCamWorldMatrix.r[3];
}

void Aeroplane::Update( bool bPlayerControl )
{
	UpdateMatrices();
}



void Aeroplane::Draw(const AeroplaneMeshes *pMeshes)
{
	Application::s_pApp->SetWorldMatrix(m_mWorldMatrix);
	pMeshes->pPlaneMesh->Draw();

	Application::s_pApp->SetWorldMatrix(m_mPropWorldMatrix);
	pMeshes->pPropMesh->Draw();

	Application::s_pApp->SetWorldMatrix(m_mTurretWorldMatrix);
	pMeshes->pTurretMesh->Draw();

	Application::s_pApp->SetWorldMatrix(m_mGunWorldMatrix);
	pMeshes->pGunMesh->Draw();
}



