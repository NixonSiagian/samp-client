#pragma once

class CCamera
{
public:
	CCamera() { m_matPos = (MATRIX4X4 *)(g_libGTASA + 0x008B1104); }
	~CCamera() {}

	void SetBehindPlayer();
	void Restore();
	void SetPosition(float fX, float fY, float fZ, float fRotationX, float fRotationY, float fRotationZ);
	void LookAtPoint(float fX, float fY, float fZ, int iType);

	void GetMatrix(PMATRIX4X4 mat);
	void InterpolateCameraPos(VECTOR *posFrom, VECTOR *posTo, int time, uint8_t mode);
	void InterpolateCameraLookAt(VECTOR *posFrom, VECTOR *posTo, int time, uint8_t mode);

public:
	ENTITY_TYPE* m_pEntity;
	MATRIX4X4 *m_matPos;
};