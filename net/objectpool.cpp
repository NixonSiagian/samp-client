#include "../main.h"
#include "../game/game.h"
#include "netgame.h"

extern CGame *pGame;

CObjectPool::CObjectPool()
{
	for(uint16_t ObjectID = 0; ObjectID < MAX_OBJECTS; ObjectID++)
	{
		m_bObjectSlotState[ObjectID] = false;
		m_pObjects[ObjectID] = 0;
	}
}

CObjectPool::~CObjectPool()
{
	for(uint16_t i = 0; i < MAX_OBJECTS; i++)
	{
		Delete(i);
	}
}

bool CObjectPool::Delete(uint16_t ObjectID)
{
	if(!GetSlotState(ObjectID) || !m_pObjects[ObjectID])
		return false;

	m_bObjectSlotState[ObjectID] = false;
	delete m_pObjects[ObjectID];
	m_pObjects[ObjectID] = 0;

	return true;
}

bool CObjectPool::New(uint16_t ObjectID, int iModel, VECTOR vecPos, VECTOR vecRot, float fDrawDistance)
{
	if(m_pObjects[ObjectID] != 0)
		Delete(ObjectID);

	m_pObjects[ObjectID] = pGame->NewObject(iModel, vecPos.X, vecPos.Y, vecPos.Z, vecRot, fDrawDistance);

	if(m_pObjects[ObjectID])
	{
		m_bObjectSlotState[ObjectID] = true;

		return true;
	}

	return false;
}

CObject *CObjectPool::GetObjectFromGtaPtr(ENTITY_TYPE *pGtaObject)
{
	uint16_t x=1;

	while(x!=MAX_OBJECTS)
	{
		if(m_pObjects[x])
			if(pGtaObject == m_pObjects[x]->m_pEntity) return m_pObjects[x];
		
		x++;
	}

	return 0;
}

uint16_t CObjectPool::FindIDFromGtaPtr(ENTITY_TYPE* pGtaObject)
{
	uint16_t x=1;

	while(x!=MAX_OBJECTS)
	{
		if(m_pObjects[x])
			if(pGtaObject == m_pObjects[x]->m_pEntity) return x;

		x++;
	}

	return INVALID_OBJECT_ID;
}

void CObjectPool::Process()
{
	static unsigned long s_ulongLastCall = 0;
	if(!s_ulongLastCall) s_ulongLastCall = GetTickCount();
	unsigned long ulongTick = GetTickCount();
	float fElapsedTime = ((float)(ulongTick - s_ulongLastCall)) / 1000.0f;
	s_ulongLastCall = ulongTick;

	// Get elapsed time in seconds
	for(unsigned short i = 0; i < MAX_OBJECTS; i++)
	{
		if(m_bObjectSlotState[i])
			m_pObjects[i]->Process(fElapsedTime);
	}
}

void CObjectPool::AttachObjectToObject(uint16_t objectId, uint16_t attachToId, VECTOR offset, VECTOR rotation, bool syncRot) 
{
	if((objectId < 0 || objectId > MAX_OBJECTS) || attachToId < 0 || attachToId > MAX_OBJECTS)
		return;

	CObject *pObject = GetAt(objectId);
	if(pObject) 
	{
		CObject *pObjectTo = GetAt(attachToId);
		if(pObjectTo)
			ScriptCommand(&attach_object_to_object, pObject->m_dwGTAId, pObjectTo->m_dwGTAId, offset.X, offset.Y, offset.Z, rotation.X, rotation.Y, rotation.Z);
	}
}

void CObjectPool::MaterialTextProcess()
{
	for(unsigned short i = 0; i < MAX_OBJECTS; i++)
	{
		if(m_bObjectSlotState[i]) 
			m_pObjects[i]->MaterialTextProcess();
	}
}