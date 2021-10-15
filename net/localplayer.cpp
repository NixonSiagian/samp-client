#include "../main.h"
#include "../game/game.h"
#include "netgame.h"
#include "../spawnscreen.h"
#include "../extrakeyboard.h"
#include "../util/armhook.h"
#include "../chatwindow.h"

// voice
#include "../voice/MicroIcon.h"
#include "../voice/SpeakerList.h"

extern CGame *pGame;
extern CNetGame *pNetGame;
extern CSpawnScreen *pSpawnScreen;
extern CExtraKeyBoard *pExtraKeyBoard;
extern CChatWindow *pChatWindow;

bool bFirstSpawn = true;

extern int iNetModeNormalOnFootSendRate;
extern int iNetModeNormalInCarSendRate;
extern int iNetModeFiringSendRate;
extern int iNetModeSendMultiplier;
extern bool bUsedPlayerSlots[];

CLocalPlayer::CLocalPlayer()
{
	ResetAllSyncAttributes();

	m_selectionClassData.m_iSelectedClass = 0;
	m_selectionClassData.m_bHasSpawnInfo = false;
	m_selectionClassData.m_bWaitingForSpawnRequestReply = false;
	m_selectionClassData.m_bWantsAnotherClass = false;

	m_tickData.m_dwLastSendTick = GetTickCount();
	m_tickData.m_dwLastSendAimTick = GetTickCount();
	m_tickData.m_dwLastSendSpecTick = GetTickCount();
	m_tickData.m_dwLastUpdateOnFootData = GetTickCount();
	m_tickData.m_dwLastUpdateInCarData = GetTickCount();
	m_tickData.m_dwLastUpdatePassengerData = GetTickCount();
	m_tickData.m_dwPassengerEnterExit = GetTickCount();
	m_tickData.m_dwLastStatsUpdateTick = GetTickCount();

	m_spectateData.m_bIsSpectating = false;
	m_spectateData.m_byteSpectateType = SPECTATING_TYPE_NONE;
	m_spectateData.m_dwSpectateId = 0xFFFFFFFF;

	m_statsData.dwLastMoney = 0;
	m_statsData.byteLastDrunkLevel = 0;

	for(int i; i < 13; i++)
	{
		m_weaponData.m_byteLastWeapon[i] = 0;
		m_weaponData.m_dwLastAmmo[i] = 0;
	}

	m_damageVehicle.m_VehicleUpdating = INVALID_VEHICLE_ID;
}

CLocalPlayer::~CLocalPlayer()
{
	// ~
}

void CLocalPlayer::ResetAllSyncAttributes()
{
	memset(&m_SpawnInfo, 0, sizeof(PLAYER_SPAWN_INFO));
	memset(&m_OnFootData, 0, sizeof(ONFOOT_SYNC_DATA));
	memset(&m_InCarData, 0, sizeof(INCAR_SYNC_DATA));
	memset(&m_TrailerData, 0, sizeof(TRAILER_SYNC_DATA));
	memset(&m_PassengerData, 0, sizeof(PASSENGER_SYNC_DATA));
	memset(&m_aimSync, 0, sizeof(AIM_SYNC_DATA));
	memset(&m_BulletData, 0, sizeof(BULLET_SYNC_DATA));

	m_pPlayerPed = pGame->FindPlayerPed();
	m_bIsActive = false;
	m_bIsWasted = false;
	m_bInRCMode = false;
	
	m_byteCurInterior = 0;

	m_CurrentVehicle = INVALID_VEHICLE_ID;
	m_LastVehicle = INVALID_VEHICLE_ID;
}

bool CLocalPlayer::Process()
{
	uint32_t dwThisTick = GetTickCount();

	if(m_bIsActive && m_pPlayerPed)
	{
		// handle dead
		if(!m_bIsWasted && m_pPlayerPed->GetActionTrigger() == ACTION_DEATH || m_pPlayerPed->IsDead())
		{
			ToggleSpectating(false);

			if(m_pPlayerPed->IsDancing() || m_pPlayerPed->HasHandsUp()) {
				m_pPlayerPed->StopDancing(); // there's no need to dance when you're dead
			}

			if(m_pPlayerPed->IsCellphoneEnabled()) {
				m_pPlayerPed->ToggleCellphone(0);
			}

			// reset tasks/anims
			m_pPlayerPed->TogglePlayerControllable(true);

			if(m_pPlayerPed->IsInVehicle() && !m_pPlayerPed->IsAPassenger())
			{
				SendInCarFullSyncData();
				m_LastVehicle = pNetGame->GetVehiclePool()->FindIDFromGtaPtr(m_pPlayerPed->GetGtaVehicle());
			}

			SendWastedNotification();

			m_bIsActive = false;
			m_bIsWasted = true;

			if(m_pPlayerPed->IsHaveAttachedObject())
				m_pPlayerPed->RemoveAllAttachedObjects();

			return true;
		}

		if(m_pPlayerPed->IsInVehicle() && (m_pPlayerPed->IsDancing() || m_pPlayerPed->HasHandsUp())) 
			m_pPlayerPed->StopDancing(); // can't dance in vehicle

		if(m_pPlayerPed->HasHandsUp() && LocalPlayerKeys.bKeys[KEY_FIRE])
			m_pPlayerPed->TogglePlayerControllable(1);

		// server checkpoints update
		pGame->UpdateCheckpoints();

		// check weapons
		UpdateWeapons();

		// handle interior changing
		uint8_t byteInterior = pGame->GetActiveInterior();
		if(byteInterior != m_byteCurInterior)
			UpdateRemoteInterior(byteInterior);

		// The regime for adjusting sendrates is based on the number
		// of players that will be effected by this update. The more players
		// there are within a small radius, the more we must scale back
		// the number of sends.
		int iNumberOfPlayersInLocalRange = DetermineNumberOfPlayersInLocalRange();
		if(!iNumberOfPlayersInLocalRange) iNumberOfPlayersInLocalRange = 20;

		// SPECTATING
		if(m_spectateData.m_bIsSpectating)
		{
			ProcessSpectating();
		}

		// DRIVER
		else if(m_pPlayerPed->IsInVehicle() && !m_pPlayerPed->IsAPassenger())
		{
			CVehiclePool *pVehiclePool = pNetGame->GetVehiclePool();
			if(pVehiclePool)
			{
				m_CurrentVehicle = pVehiclePool->FindIDFromGtaPtr(m_pPlayerPed->GetGtaVehicle());
				CVehicle *pVehicle = pVehiclePool->GetAt(m_CurrentVehicle);
				if(pVehicle)
				{	
					if((dwThisTick - m_tickData.m_dwLastSendTick) > (unsigned int)GetOptimumInCarSendRate(iNumberOfPlayersInLocalRange))
					{
						m_tickData.m_dwLastSendTick = GetTickCount();
						SendInCarFullSyncData();
					}

					ProcessVehicleDamageUpdates(m_CurrentVehicle);
				}
			}
		}

		// ONFOOT
		else if(m_pPlayerPed->GetActionTrigger() == ACTION_NORMAL || m_pPlayerPed->GetActionTrigger() == ACTION_SCOPE)
		{
			UpdateSurfing();

			if(m_CurrentVehicle != INVALID_VEHICLE_ID)
			{
				m_LastVehicle = m_CurrentVehicle;
				m_CurrentVehicle = INVALID_VEHICLE_ID;
			}

			if((dwThisTick - m_tickData.m_dwLastSendTick) > (unsigned int)GetOptimumOnFootSendRate() || LocalPlayerKeys.bKeys[ePadKeys::KEY_YES] || LocalPlayerKeys.bKeys[ePadKeys::KEY_NO] || LocalPlayerKeys.bKeys[ePadKeys::KEY_CTRL_BACK])
			{
				m_tickData.m_dwLastSendTick = GetTickCount();
				SendOnFootFullSyncData();
			}

			// TIMING FOR ONFOOT AIM SENDS
			uint16_t lrAnalog, udAnalog;
			uint8_t additionalKey = 0;
			uint16_t wKeys = m_pPlayerPed->GetKeys(&lrAnalog, &udAnalog, &additionalKey);
			
			// Not targeting or firing. We need a very slow rate to sync the head.
			if (!IS_TARGETING(wKeys) && !IS_FIRING(wKeys)) 
			{
				if ((dwThisTick - m_tickData.m_dwLastSendAimTick) > NETMODE_HEADSYNC_SENDRATE) 
				{
					m_tickData.m_dwLastSendAimTick = dwThisTick;
					SendAimSyncData();
				}
			}

			// Targeting only. Just synced for show really, so use a slower rate
			else if (IS_TARGETING(wKeys) && !IS_FIRING(wKeys))
			{
				if ((dwThisTick - m_tickData.m_dwLastSendAimTick) > (uint32_t)NETMODE_AIM_SENDRATE + (iNumberOfPlayersInLocalRange))
				{
					m_tickData.m_dwLastSendAimTick = dwThisTick;
					SendAimSyncData();
				}
			}

			// Targeting and Firing. Needs a very accurate send rate.
			else if (IS_TARGETING(wKeys) && IS_FIRING(wKeys)) 
			{
				if ((dwThisTick - m_tickData.m_dwLastSendAimTick) > (uint32_t)NETMODE_FIRING_SENDRATE + (iNumberOfPlayersInLocalRange))
				{
					m_tickData.m_dwLastSendAimTick = dwThisTick;
					SendAimSyncData();
				}
			}

			// Firing without targeting. Needs a normal onfoot sendrate.
			else if (!IS_TARGETING(wKeys) && IS_FIRING(wKeys)) 
			{
				if ((dwThisTick - m_tickData.m_dwLastSendAimTick) > (uint32_t)GetOptimumOnFootSendRate())
				{
					m_tickData.m_dwLastSendAimTick = dwThisTick;
					SendAimSyncData();
				}
			}
		}

		// PASSENGER
		else if(m_pPlayerPed->IsInVehicle() && m_pPlayerPed->IsAPassenger())
		{
			if((dwThisTick - m_tickData.m_dwLastSendTick) > (unsigned int)GetOptimumInCarSendRate(iNumberOfPlayersInLocalRange))
			{
				m_tickData.m_dwLastSendTick = GetTickCount();
				SendPassengerFullSyncData();
			}
		}
	}

	// handle !IsActive spectating
	if(m_spectateData.m_bIsSpectating && !m_bIsActive)
	{
		ProcessSpectating();
		return true;
	}

	// handle needs to respawn
	if(m_bIsWasted && (m_pPlayerPed->GetActionTrigger() != ACTION_WASTED) && 
		(m_pPlayerPed->GetActionTrigger() != ACTION_DEATH) )
	{
		if(m_selectionClassData.m_bClearedToSpawn && !m_selectionClassData.m_bWantsAnotherClass &&
			pNetGame->GetGameState() == GAMESTATE_CONNECTED)
		{
			if(m_pPlayerPed->GetHealth() > 0.0f)
				Spawn();
		}
		else
		{
			m_bIsWasted = false;
			HandleClassSelection();
			m_selectionClassData.m_bWantsAnotherClass = false;
		}

		return true;
	}

	return true;
}

void CLocalPlayer::SendWastedNotification()
{
	RakNet::BitStream bsPlayerDeath;
	PLAYERID WhoWasResponsible = INVALID_PLAYER_ID;
	
	uint8_t byteDeathReason = m_pPlayerPed->FindDeathReasonAndResponsiblePlayer(&WhoWasResponsible);

	bsPlayerDeath.Write(byteDeathReason);
	bsPlayerDeath.Write(WhoWasResponsible);
	pNetGame->GetRakClient()->RPC(&RPC_Death, &bsPlayerDeath, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false, UNASSIGNED_NETWORK_ID, nullptr);
}

void CLocalPlayer::HandleClassSelection()
{
	m_selectionClassData.m_bClearedToSpawn = false;

	if(m_pPlayerPed)
	{
		m_pPlayerPed->SetInitialState();
		m_pPlayerPed->SetHealth(100.0f);
		m_pPlayerPed->TogglePlayerControllable(0);
	}
	
	RequestClass(m_selectionClassData.m_iSelectedClass);
	pSpawnScreen->Show(true);

	return;
}

void CLocalPlayer::HandleClassSelectionOutcome()
{
	if(m_pPlayerPed)
	{
		m_pPlayerPed->ClearAllWeapons();
		m_pPlayerPed->SetModelIndex(m_SpawnInfo.iSkin);
	}

	m_selectionClassData.m_bClearedToSpawn = true;
}

void CLocalPlayer::RequestClass(int iClass)
{
	RakNet::BitStream bsSpawnRequest;
	bsSpawnRequest.Write(iClass);
	pNetGame->GetRakClient()->RPC(&RPC_RequestClass, &bsSpawnRequest, HIGH_PRIORITY, RELIABLE, 0, false, UNASSIGNED_NETWORK_ID, 0);
}

void CLocalPlayer::SendNextClass()
{
	MATRIX4X4 matPlayer;
	m_pPlayerPed->GetMatrix(&matPlayer);

	if(m_selectionClassData.m_iSelectedClass == (pNetGame->m_iSpawnsAvailable - 1)) m_selectionClassData.m_iSelectedClass = 0;
		else m_selectionClassData.m_iSelectedClass++;

	pGame->PlaySound(1052, matPlayer.pos.X, matPlayer.pos.Y, matPlayer.pos.Z);
	RequestClass(m_selectionClassData.m_iSelectedClass);
}

void CLocalPlayer::SendPrevClass()
{
	MATRIX4X4 matPlayer;
	m_pPlayerPed->GetMatrix(&matPlayer);
	
	if(m_selectionClassData.m_iSelectedClass == 0) m_selectionClassData.m_iSelectedClass = (pNetGame->m_iSpawnsAvailable - 1);
		else m_selectionClassData.m_iSelectedClass--;		

	pGame->PlaySound(1053, matPlayer.pos.X, matPlayer.pos.Y, matPlayer.pos.Z);
	RequestClass(m_selectionClassData.m_iSelectedClass);
}

uint32_t CLocalPlayer::GetPlayerColor()
{
	return TranslateColorCodeToRGBA(pNetGame->GetPlayerPool()->GetLocalPlayerID());
}

uint32_t CLocalPlayer::GetPlayerColorAsARGB()
{
	return (TranslateColorCodeToRGBA(pNetGame->GetPlayerPool()->GetLocalPlayerID()) >> 8) | 0xFF000000;	
}

void CLocalPlayer::SetPlayerColor(uint32_t dwColor)
{
	SetRadarColor(pNetGame->GetPlayerPool()->GetLocalPlayerID(), dwColor);
}

void CLocalPlayer::SetSpawnInfo(PLAYER_SPAWN_INFO *pSpawn)
{
	memcpy(&m_SpawnInfo, pSpawn, sizeof(PLAYER_SPAWN_INFO));
	m_selectionClassData.m_bHasSpawnInfo = true;
}

void CLocalPlayer::RequestSpawn()
{
	RakNet::BitStream bsSpawnRequest;
	pNetGame->GetRakClient()->RPC(&RPC_RequestSpawn, &bsSpawnRequest, HIGH_PRIORITY, RELIABLE, 0, false, UNASSIGNED_NETWORK_ID, 0);
}

void CLocalPlayer::SendSpawn()
{
	RequestSpawn();
	m_selectionClassData.m_bWaitingForSpawnRequestReply = true;
}

bool CLocalPlayer::Spawn()
{
	if(!m_selectionClassData.m_bHasSpawnInfo) return false;

	// voice
	SpeakerList::Show();
    MicroIcon::Show();

	pSpawnScreen->Show(false);
	pExtraKeyBoard->Show(true);

	CCamera *pGameCamera;
	pGameCamera = pGame->GetCamera();
	pGameCamera->Restore();
	pGameCamera->SetBehindPlayer();
	pGame->DisplayWidgets(true);
	pGame->DisplayHUD(true);
	m_pPlayerPed->TogglePlayerControllable(true);

	if(!bFirstSpawn)
		m_pPlayerPed->SetInitialState();
	else bFirstSpawn = false;

	pGame->RefreshStreamingAt(m_SpawnInfo.vecPos.X,m_SpawnInfo.vecPos.Y);

	m_pPlayerPed->RestartIfWastedAt(&m_SpawnInfo.vecPos, m_SpawnInfo.fRotation);
	m_pPlayerPed->SetModelIndex(m_SpawnInfo.iSkin);
	m_pPlayerPed->ClearAllWeapons();
	m_pPlayerPed->ResetDamageEntity();

	pGame->DisableTrainTraffic();

	// CCamera::Fade
	WriteMemory(g_libGTASA+0x36EA2C, (uintptr_t)"\x70\x47", 2); // bx lr

	m_pPlayerPed->TeleportTo(m_SpawnInfo.vecPos.X, m_SpawnInfo.vecPos.Y, (m_SpawnInfo.vecPos.Z + 0.5f));

	m_pPlayerPed->ForceTargetRotation(m_SpawnInfo.fRotation);

	m_bIsWasted = false;
	m_bIsActive = true;
	m_selectionClassData.m_bWaitingForSpawnRequestReply = false;

	RakNet::BitStream bsSendSpawn;
	pNetGame->GetRakClient()->RPC(&RPC_Spawn, &bsSendSpawn, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, false, UNASSIGNED_NETWORK_ID, NULL);
	return true;
}

void CLocalPlayer::UpdateSurfing()
{	
	VEHICLE_TYPE *Contact = m_pPlayerPed->GetGtaContactVehicle();

	if(!Contact) {
		m_bSurfingMode = false;
		m_SurfingID = INVALID_VEHICLE_ID;
		return;
	}

	VEHICLEID vehID = pNetGame->GetVehiclePool()->FindIDFromGtaPtr(Contact);

	if(vehID && vehID != INVALID_VEHICLE_ID) {

		CVehicle *pVehicle = pNetGame->GetVehiclePool()->GetAt(vehID);

		if( pVehicle && pVehicle->IsOccupied() && 
			pVehicle->GetDistanceFromLocalPlayerPed() < 5.0f ) {

			VECTOR vecSpeed;
			VECTOR vecTurn;
			MATRIX4X4 matPlayer;
			MATRIX4X4 matVehicle;
			VECTOR vecVehiclePlane;
			WORD lr, ud;

			pVehicle->GetMatrix(&matVehicle);
			m_pPlayerPed->GetMatrix(&matPlayer);

			m_bSurfingMode = true;
			m_SurfingID = vehID;

			m_vecLockedSurfingOffsets.X = matPlayer.pos.X - matVehicle.pos.X;
			m_vecLockedSurfingOffsets.Y = matPlayer.pos.Y - matVehicle.pos.Y;
			m_vecLockedSurfingOffsets.Z = matPlayer.pos.Z - matVehicle.pos.Z;

			vecSpeed = Contact->entity.vecMoveSpeed;
			vecTurn = Contact->entity.vecTurnSpeed;

			uint16_t lrAnalog, udAnalog;
			uint8_t additionalKey = 0;
			m_pPlayerPed->GetKeys(&lrAnalog, &udAnalog, &additionalKey);

			if(&lrAnalog, &udAnalog, &additionalKey) {
				// if they're not trying to translate, keep their
				// move and turn speeds identical to the surfing vehicle
				m_pPlayerPed->SetMoveSpeedVector(vecSpeed);
				m_pPlayerPed->SetTurnSpeedVector(vecTurn);
			}
			
			return;
		}
	}
	m_bSurfingMode = false;
	m_SurfingID = INVALID_VEHICLE_ID;
}

bool CLocalPlayer::HandlePassengerEntry()
{
	if(GetTickCount() - m_tickData.m_dwPassengerEnterExit < 1000)
		return true;

	CVehiclePool *pVehiclePool = pNetGame->GetVehiclePool();

	// CTouchInterface::IsHoldDown
    int isHoldDown = (( int (*)(int, int, int))(g_libGTASA+0x270818+1))(0, 1, 1);

	if(isHoldDown)
	{
		VEHICLEID ClosetVehicleID = pVehiclePool->FindNearestToLocalPlayerPed();
		if(ClosetVehicleID < MAX_VEHICLES && pVehiclePool->GetSlotState(ClosetVehicleID))
		{
			CVehicle* pVehicle = pVehiclePool->GetAt(ClosetVehicleID);
			if(pVehicle->GetDistanceFromLocalPlayerPed() < 4.0f)
			{
				m_pPlayerPed->EnterVehicle(pVehicle->m_dwGTAId, true);
				SendEnterVehicleNotification(ClosetVehicleID, true);
				m_tickData.m_dwPassengerEnterExit = GetTickCount();
				return true;
			}
		}
	}

	return false;
}

bool CLocalPlayer::HandlePassengerEntryByCommand()
{
	if(GetTickCount() - m_tickData.m_dwPassengerEnterExit < 1000)
		return true;

	CVehiclePool *pVehiclePool = pNetGame->GetVehiclePool();

	VEHICLEID ClosetVehicleID = pVehiclePool->FindNearestToLocalPlayerPed();
	if(ClosetVehicleID < MAX_VEHICLES && pVehiclePool->GetSlotState(ClosetVehicleID))
	{
		CVehicle* pVehicle = pVehiclePool->GetAt(ClosetVehicleID);

		if(pVehicle->GetDistanceFromLocalPlayerPed() < 4.0f)
		{
			m_pPlayerPed->EnterVehicle(pVehicle->m_dwGTAId, true);
			SendEnterVehicleNotification(ClosetVehicleID, true);
			m_tickData.m_dwPassengerEnterExit = GetTickCount();
			return true;
		}
	}

	return false;
}

void CLocalPlayer::SendEnterVehicleNotification(VEHICLEID VehicleID, bool bPassenger)
{
	RakNet::BitStream bsSend;
	bsSend.Write(VehicleID);
	bsSend.Write(bPassenger);

	pNetGame->GetRakClient()->RPC(&RPC_EnterVehicle, &bsSend, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, false, UNASSIGNED_NETWORK_ID, nullptr);

	CVehiclePool *pVehiclePool = pNetGame->GetVehiclePool();
	if(pVehiclePool)
	{
		CVehicle* pVehicle = pVehiclePool->GetAt(VehicleID);
		if(pVehicle && pVehicle->IsATrainPart()) 
		{
			uint32_t dwVehicle = pVehicle->m_dwGTAId;
			ScriptCommand(&camera_on_vehicle, dwVehicle, 3, 2);
		}
	}
}

void CLocalPlayer::SendExitVehicleNotification(VEHICLEID VehicleID)
{
	CVehiclePool *pVehiclePool = pNetGame->GetVehiclePool();
	if(pVehiclePool)
	{
		CVehicle* pVehicle = pVehiclePool->GetAt(VehicleID);
		if(pVehicle)
		{ 
			if(!m_pPlayerPed->IsAPassenger()) m_LastVehicle = VehicleID;
			if(pVehicle->IsATrainPart()) pGame->GetCamera()->SetBehindPlayer();
			
			RakNet::BitStream bsSend;
			bsSend.Write(VehicleID);

			pNetGame->GetRakClient()->RPC(&RPC_ExitVehicle, &bsSend, HIGH_PRIORITY,RELIABLE_SEQUENCED, 0, false, UNASSIGNED_NETWORK_ID, NULL);
		}
	}
}

void CLocalPlayer::UpdateRemoteInterior(uint8_t byteInterior)
{
	m_byteCurInterior = byteInterior;

	RakNet::BitStream bsUpdateInterior;
	bsUpdateInterior.Write(byteInterior);

	pNetGame->GetRakClient()->RPC(&RPC_SetInteriorId, &bsUpdateInterior, HIGH_PRIORITY, RELIABLE, 0, false, UNASSIGNED_NETWORK_ID, NULL);
}

void CLocalPlayer::ApplySpecialAction(uint8_t byteSpecialAction)
{
	switch(byteSpecialAction)
	{
		default:
		case SPECIAL_ACTION_NONE:
			// ~
		break;

		case SPECIAL_ACTION_USECELLPHONE:
			if(!m_pPlayerPed->IsInVehicle()) m_pPlayerPed->ToggleCellphone(1);
		break;

		case SPECIAL_ACTION_STOPUSECELLPHONE:
			if(m_pPlayerPed->IsCellphoneEnabled()) m_pPlayerPed->ToggleCellphone(0);
		break;

		case SPECIAL_ACTION_USEJETPACK:
			if(!m_pPlayerPed->IsInJetpackMode()) m_pPlayerPed->StartJetpack();
		break;

		case SPECIAL_ACTION_HANDSUP:
			if(!m_pPlayerPed->HasHandsUp()) m_pPlayerPed->HandsUp();
		break;

		case SPECIAL_ACTION_DANCE1:
			m_pPlayerPed->PlayDance(1);
		break;

		case SPECIAL_ACTION_DANCE2:
			m_pPlayerPed->PlayDance(2);
		break;

		case SPECIAL_ACTION_DANCE3:
			m_pPlayerPed->PlayDance(3);
		break;

		case SPECIAL_ACTION_DANCE4:
			m_pPlayerPed->PlayDance(4);
		break;
	}
}

uint8_t CLocalPlayer::GetSpecialAction()
{
	if(GetPlayerPed()->IsCrouching())
	{
		return SPECIAL_ACTION_DUCK;
	}
		
	if(m_pPlayerPed->IsInJetpackMode())
		return SPECIAL_ACTION_USEJETPACK;

	if(m_pPlayerPed->IsDancing()) 
	{
		switch(m_pPlayerPed->m_iDanceStyle) 
		{
			case 0:
				return SPECIAL_ACTION_DANCE1;
			case 1:
				return SPECIAL_ACTION_DANCE2;
			case 2:
				return SPECIAL_ACTION_DANCE3;
			case 3:
				return SPECIAL_ACTION_DANCE4;
		}
	}

	if(m_pPlayerPed->HasHandsUp())
		return SPECIAL_ACTION_HANDSUP;

	if(m_pPlayerPed->IsCellphoneEnabled())
		return SPECIAL_ACTION_USECELLPHONE;

	return SPECIAL_ACTION_NONE;
}

int CLocalPlayer::GetOptimumOnFootSendRate()
{
	if(!m_pPlayerPed) return 1000;

	return (iNetModeNormalOnFootSendRate + DetermineNumberOfPlayersInLocalRange());
}

int CLocalPlayer::GetOptimumInCarSendRate(uint8_t iPlayersEffected)
{
	if(!m_pPlayerPed) return 1000;

	//return (iNetModeNormalInCarSendRate + DetermineNumberOfPlayersInLocalRange());

	CVehiclePool *pVehiclePool = pNetGame->GetVehiclePool();
	if(pVehiclePool)
	{
		VEHICLEID VehicleID = pVehiclePool->FindIDFromGtaPtr(m_pPlayerPed->GetGtaVehicle());
		if(VehicleID != INVALID_VEHICLE_ID)
		{
			CVehicle *pVehicle = pVehiclePool->GetAt(VehicleID);
			if(pVehicle) 
			{
				VECTOR vecMoveSpeed;
				pVehicle->GetMoveSpeedVector(&vecMoveSpeed);

				if( (vecMoveSpeed.X == 0.0f) &&
					(vecMoveSpeed.Y == 0.0f) &&
					(vecMoveSpeed.Z == 0.0f) ) {

					if(pNetGame->m_bLanMode) return LANMODE_IDLE_INCAR_SENDRATE;
					else return (iNetModeNormalInCarSendRate + (int)iPlayersEffected * iNetModeSendMultiplier);
				}
				else {
					if(pNetGame->m_bLanMode) return LANMODE_NORMAL_INCAR_SENDRATE;
					else return (iNetModeNormalInCarSendRate + (int)iPlayersEffected * iNetModeSendMultiplier);
				}
			}
		}
	}
}

int iNumPlayersInRange = 0;
int iCyclesUntilNextCount = 0;
uint8_t CLocalPlayer::DetermineNumberOfPlayersInLocalRange()
{
	/*int iNumPlayersInRange = 0;
	for(int i = 2; i < PLAYER_PED_SLOTS; i++)
		if(bUsedPlayerSlots[i]) iNumPlayersInRange++;

	return iNumPlayersInRange;*/

	int iRet = 0;
	PLAYERID x = 0;

	// We only want to perform this operation
	// once every few cycles. Doing it every frame
	// would be a little bit too CPU intensive.

	if(iCyclesUntilNextCount) 
	{
		iCyclesUntilNextCount--;
		return iNumPlayersInRange;
	}

	// This part is only processed when iCyclesUntilNextCount is 0
	iCyclesUntilNextCount = 30;
	iNumPlayersInRange = 0;

	CPlayerPool *pPlayerPool = pNetGame->GetPlayerPool();
	if(pPlayerPool) 
	{		
		while(x != MAX_PLAYERS) 
		{
			if(pPlayerPool->GetSlotState(x)) 
			{
				if(pPlayerPool->GetAt(x)->IsActive()) 
				{
					iNumPlayersInRange++;
				}
			}
			x++;
		}
	}

	return iNumPlayersInRange;
}

void CLocalPlayer::SendOnFootFullSyncData()
{
	RakNet::BitStream bsPlayerSync;
	MATRIX4X4 matPlayer;
	VECTOR vecMoveSpeed;
	uint16_t lrAnalog, udAnalog;
	uint8_t additionalKey = 0;
	uint16_t wKeys = m_pPlayerPed->GetKeys(&lrAnalog, &udAnalog, &additionalKey);

	ONFOOT_SYNC_DATA ofSync;

	m_pPlayerPed->GetMatrix(&matPlayer);
	m_pPlayerPed->GetMoveSpeedVector(&vecMoveSpeed);

	ofSync.lrAnalog = lrAnalog;
	ofSync.udAnalog = udAnalog;
	ofSync.wKeys = wKeys;
	ofSync.vecPos.X = matPlayer.pos.X;
	ofSync.vecPos.Y = matPlayer.pos.Y;
	ofSync.vecPos.Z = matPlayer.pos.Z;

	ofSync.quat.SetFromMatrix(matPlayer);
	ofSync.quat.Normalize();

	if( FloatOffset(ofSync.quat.w, m_OnFootData.quat.w) < 0.00001 &&
		FloatOffset(ofSync.quat.x, m_OnFootData.quat.x) < 0.00001 &&
		FloatOffset(ofSync.quat.y, m_OnFootData.quat.y) < 0.00001 &&
		FloatOffset(ofSync.quat.z, m_OnFootData.quat.z) < 0.00001)
	{
		ofSync.quat.Set(m_OnFootData.quat);
	}

	ofSync.byteHealth = (uint8_t)m_pPlayerPed->GetHealth();
	ofSync.byteArmour = (uint8_t)m_pPlayerPed->GetArmour();

	uint8_t exKeys = GetPlayerPed()->GetExtendedKeys();
	ofSync.byteCurrentWeapon = (additionalKey << 6) | ofSync.byteCurrentWeapon & 0x3F;
	ofSync.byteCurrentWeapon ^= (ofSync.byteCurrentWeapon ^ GetPlayerPed()->GetCurrentWeapon()) & 0x3F;
	ofSync.byteSpecialAction = GetSpecialAction();

	ofSync.vecMoveSpeed.X = vecMoveSpeed.X;
	ofSync.vecMoveSpeed.Y = vecMoveSpeed.Y;
	ofSync.vecMoveSpeed.Z = vecMoveSpeed.Z;

	ofSync.vecSurfOffsets.X = 0.0f;
	ofSync.vecSurfOffsets.Y = 0.0f;
	ofSync.vecSurfOffsets.Z = 0.0f;
	ofSync.wSurfInfo = 0;

	ofSync.dwAnimation = 0;

	if( (GetTickCount() - m_tickData.m_dwLastUpdateOnFootData) > 500 || memcmp(&m_OnFootData, &ofSync, sizeof(ONFOOT_SYNC_DATA)))
	{
		m_tickData.m_dwLastUpdateOnFootData = GetTickCount();

		bsPlayerSync.Write((uint8_t)ID_PLAYER_SYNC);
		bsPlayerSync.Write((char*)&ofSync, sizeof(ONFOOT_SYNC_DATA));
		pNetGame->GetRakClient()->Send(&bsPlayerSync, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 0);

		memcpy(&m_OnFootData, &ofSync, sizeof(ONFOOT_SYNC_DATA));
	}
}

void CLocalPlayer::SendInCarFullSyncData()
{
	RakNet::BitStream bsVehicleSync;
	CVehiclePool *pVehiclePool = pNetGame->GetVehiclePool();
	if(!pVehiclePool) return;

	MATRIX4X4 matPlayer;
	VECTOR vecMoveSpeed;

	uint16_t lrAnalog, udAnalog;
	uint8_t additionalKey = 0;
	uint16_t wKeys = m_pPlayerPed->GetKeys(&lrAnalog, &udAnalog, &additionalKey);
	CVehicle *pVehicle;

	INCAR_SYNC_DATA icSync;
	memset(&icSync, 0, sizeof(INCAR_SYNC_DATA));

	if(m_pPlayerPed)
	{
		icSync.VehicleID = pVehiclePool->FindIDFromGtaPtr(m_pPlayerPed->GetGtaVehicle());

		if(icSync.VehicleID == INVALID_VEHICLE_ID) return;

		icSync.lrAnalog = lrAnalog;
		icSync.udAnalog = udAnalog;
		icSync.wKeys = wKeys;

		pVehicle = pVehiclePool->GetAt(icSync.VehicleID);
		if(!pVehicle) return;

		pVehicle->GetMatrix(&matPlayer);
		pVehicle->GetMoveSpeedVector(&vecMoveSpeed);

		icSync.quat.SetFromMatrix(matPlayer);
		icSync.quat.Normalize();

		if(	FloatOffset(icSync.quat.w, m_InCarData.quat.w) < 0.00001 &&
			FloatOffset(icSync.quat.x, m_InCarData.quat.x) < 0.00001 &&
			FloatOffset(icSync.quat.y, m_InCarData.quat.y) < 0.00001 &&
			FloatOffset(icSync.quat.z, m_InCarData.quat.z) < 0.00001)
		{
			icSync.quat.Set(m_InCarData.quat);
		}

		// pos
		icSync.vecPos.X = matPlayer.pos.X;
		icSync.vecPos.Y = matPlayer.pos.Y;
		icSync.vecPos.Z = matPlayer.pos.Z;

		// move speed
		icSync.vecMoveSpeed.X = vecMoveSpeed.X;
		icSync.vecMoveSpeed.Y = vecMoveSpeed.Y;
		icSync.vecMoveSpeed.Z = vecMoveSpeed.Z;

		icSync.fCarHealth = pVehicle->GetHealth();
		icSync.bytePlayerHealth = (uint8_t)m_pPlayerPed->GetHealth();
		icSync.bytePlayerArmour = (uint8_t)m_pPlayerPed->GetArmour();

		uint8_t exKeys = GetPlayerPed()->GetExtendedKeys();
		icSync.byteCurrentWeapon = (exKeys << 6) | icSync.byteCurrentWeapon & 0x3F;
		icSync.byteCurrentWeapon ^= (icSync.byteCurrentWeapon ^ GetPlayerPed()->GetCurrentWeapon()) & 0x3F;

		icSync.byteSirenOn = pVehicle->IsSirenOn();
		//icSync.byteLandingGearState = pVehicle->GetLandingGearState();

		/*icSync.TrailerID = 0;
		VEHICLE_TYPE* vehTrailer = (VEHICLE_TYPE*)pVehicle->m_pVehicle->dwTrailer;
		if(vehTrailer != NULL)	
		{
			if(ScriptCommand(&is_trailer_on_cab, pVehiclePool->FindIDFromGtaPtr(vehTrailer), pVehicle->m_dwGTAId))
				icSync.TrailerID = pVehiclePool->FindIDFromGtaPtr(vehTrailer);
			else icSync.TrailerID = 0;
		}*/

		// Note: Train Speed and Tire Popping values are mutually exclusive, which means
		//       if one is set, the other one will be affected.

		if( pVehicle->GetModelIndex() != TRAIN_PASSENGER_LOCO ||
			pVehicle->GetModelIndex() != TRAIN_FREIGHT_LOCO ||
			pVehicle->GetModelIndex() != TRAIN_TRAM) {
			if( pVehicle->GetVehicleSubtype() != 2 && pVehicle->GetVehicleSubtype() != 6) {
				if( pVehicle->GetModelIndex() == HYDRA)
				{
					icSync.fTrainSpeed = (float)pVehicle->GetHydraThrusters();
				} else {
					icSync.fTrainSpeed = 0;
				}
			} else {
				icSync.fTrainSpeed = (float)pVehicle->GetBikeLean();
			}
		} else {
			icSync.fTrainSpeed = (float)pVehicle->GetTrainSpeed();
		}
		
		icSync.TrailerID = 0;
		VEHICLE_TYPE* vehTrailer = (VEHICLE_TYPE*)pVehicle->m_pVehicle->dwTrailer;
		CVehicle* pTrailer = pVehicle->GetTrailer();
		if (pTrailer == NULL && vehTrailer == NULL)
		{
			pVehicle->SetTrailer(NULL);
		}
		else
		{
			pVehicle->SetTrailer(pTrailer);
			icSync.TrailerID = pVehiclePool->FindIDFromGtaPtr(vehTrailer);
		}
		

		// SPECIAL STUFF
		/*if(pGameVehicle->GetModelIndex() == HYDRA)
			icSync.dwHydraThrustAngle = pVehicle->GetHydraThrusters();
		else icSync.dwHydraThrustAngle = 0;*/

		// send
		if( (GetTickCount() - m_tickData.m_dwLastUpdateInCarData) > 500 || memcmp(&m_InCarData, &icSync, sizeof(INCAR_SYNC_DATA)))
		{
			m_tickData.m_dwLastUpdateInCarData = GetTickCount();

			bsVehicleSync.Write((uint8_t)ID_VEHICLE_SYNC);
			bsVehicleSync.Write((char*)&icSync, sizeof(INCAR_SYNC_DATA));
			pNetGame->GetRakClient()->Send(&bsVehicleSync, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 0);

			memcpy(&m_InCarData, &icSync, sizeof(INCAR_SYNC_DATA));

			// For the tank/firetruck, we need some info on aiming
			if (pVehicle->HasTurret()) SendAimSyncData();
		}

		/*if(icSync.TrailerID && icSync.TrailerID < MAX_VEHICLES)
		{
			MATRIX4X4 matTrailer;
			TRAILER_SYNC_DATA trSync;
			CVehicle* pTrailer = pVehiclePool->GetAt(icSync.TrailerID);
			if(pTrailer)
			{
				pTrailer->GetMatrix(&matTrailer);

				trSync.quat.SetFromMatrix(matTrailer);
				trSync.quat.Normalize();

				if(	FloatOffset(trSync.quat.w, m_TrailerData.quat.w) < 0.00001 &&
					FloatOffset(trSync.quat.x, m_TrailerData.quat.x) < 0.00001 &&
					FloatOffset(trSync.quat.y, m_TrailerData.quat.y) < 0.00001 &&
					FloatOffset(trSync.quat.z, m_TrailerData.quat.z) < 0.00001)
				{
					trSync.quat.Set(m_TrailerData.quat);
				}
				
				trSync.vecPos.X = matTrailer.pos.X;
				trSync.vecPos.Y = matTrailer.pos.Y;
				trSync.vecPos.Z = matTrailer.pos.Z;
				
				pTrailer->GetMoveSpeedVector(&trSync.vecMoveSpeed);
				pTrailer->GetTurnSpeedVector(&trSync.vecTurnSpeed);

				RakNet::BitStream bsTrailerSync;
				bsTrailerSync.Write((uint8_t)ID_TRAILER_SYNC);
				bsTrailerSync.Write((char*)&trSync, sizeof (TRAILER_SYNC_DATA));
				pNetGame->GetRakClient()->Send(&bsTrailerSync,HIGH_PRIORITY,UNRELIABLE_SEQUENCED,0);

				memcpy(&m_TrailerData, &trSync, sizeof(TRAILER_SYNC_DATA));
			}
		}*/
	}
}

void CLocalPlayer::SendPassengerFullSyncData()
{
	RakNet::BitStream bsPassengerSync;
	CVehiclePool *pVehiclePool = pNetGame->GetVehiclePool();

	uint16_t lrAnalog, udAnalog;
	uint8_t additionalKey = 0;
	uint16_t wKeys = m_pPlayerPed->GetKeys(&lrAnalog, &udAnalog, &additionalKey);
	PASSENGER_SYNC_DATA psSync;
	MATRIX4X4 mat;

	psSync.VehicleID = pVehiclePool->FindIDFromGtaPtr(m_pPlayerPed->GetGtaVehicle());

	if(psSync.VehicleID == INVALID_VEHICLE_ID) return;

	psSync.lrAnalog = lrAnalog;
	psSync.udAnalog = udAnalog;
	psSync.wKeys = wKeys;
	psSync.bytePlayerHealth = (uint8_t)m_pPlayerPed->GetHealth();
	psSync.bytePlayerArmour = (uint8_t)m_pPlayerPed->GetArmour();

	psSync.byteDriveBy = 0;//m_bPassengerDriveByMode;
	psSync.byteSeatFlags = m_pPlayerPed->GetVehicleSeatID();

	uint8_t exKeys = GetPlayerPed()->GetExtendedKeys();
	psSync.byteCurrentWeapon = (exKeys << 6) | psSync.byteCurrentWeapon & 0x3F;
	psSync.byteCurrentWeapon ^= (psSync.byteCurrentWeapon ^ GetPlayerPed()->GetCurrentWeapon()) & 0x3F;

	m_pPlayerPed->GetMatrix(&mat);
	psSync.vecPos.X = mat.pos.X;
	psSync.vecPos.Y = mat.pos.Y;
	psSync.vecPos.Z = mat.pos.Z;

	// send
	if((GetTickCount() - m_tickData.m_dwLastUpdatePassengerData) > 500 || memcmp(&m_PassengerData, &psSync, sizeof(PASSENGER_SYNC_DATA)))
	{
		m_tickData.m_dwLastUpdatePassengerData = GetTickCount();

		bsPassengerSync.Write((uint8_t)ID_PASSENGER_SYNC);
		bsPassengerSync.Write((char*)&psSync, sizeof(PASSENGER_SYNC_DATA));
		pNetGame->GetRakClient()->Send(&bsPassengerSync, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 0);

		memcpy(&m_PassengerData, &psSync, sizeof(PASSENGER_SYNC_DATA));
	}
}

void CLocalPlayer::SendAimSyncData()
{
	AIM_SYNC_DATA aimSync;

	CAMERA_AIM* caAim = m_pPlayerPed->GetCurrentAim();

	aimSync.byteCamMode = m_pPlayerPed->GetCameraMode();
	aimSync.vecAimf.X = caAim->f1x;
	aimSync.vecAimf.Y = caAim->f1y;
	aimSync.vecAimf.Z = caAim->f1z;
	aimSync.vecAimPos.X = caAim->pos1x;
	aimSync.vecAimPos.Y = caAim->pos1y;
	aimSync.vecAimPos.Z = caAim->pos1z;
	aimSync.fAimZ = m_pPlayerPed->GetAimZ();
	aimSync.aspect_ratio = GameGetAspectRatio() * 255.0;
	aimSync.byteCamExtZoom = (uint8_t)(m_pPlayerPed->GetCameraExtendedZoom() * 63.0f);

	WEAPON_SLOT_TYPE* pwstWeapon = m_pPlayerPed->GetCurrentWeaponSlot();
	if (pwstWeapon->dwState == 2) {
		aimSync.byteWeaponState = WS_RELOADING;
	} else {
		aimSync.byteWeaponState = (pwstWeapon->dwAmmoInClip > 1) ? WS_MORE_BULLETS : pwstWeapon->dwAmmoInClip;
	}

	if ((GetTickCount() - m_dwLastSendSyncTick) > 500 || memcmp(&m_aimSync, &aimSync, sizeof(AIM_SYNC_DATA)))
	{
		m_dwLastSendSyncTick = GetTickCount();
		RakNet::BitStream bsAimSync;
		bsAimSync.Write((char)ID_AIM_SYNC);
		bsAimSync.Write((char*)&aimSync, sizeof(AIM_SYNC_DATA));
		pNetGame->GetRakClient()->Send(&bsAimSync, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 1);
		memcpy(&m_aimSync, &aimSync, sizeof(AIM_SYNC_DATA));
	}
}

void CLocalPlayer::ProcessSpectating()
{
	RakNet::BitStream bsSpectatorSync;
	SPECTATOR_SYNC_DATA spSync;
	MATRIX4X4 matPos;

	uint16_t lrAnalog, udAnalog;
	uint8_t additionalKey = 0;
	uint16_t wKeys = m_pPlayerPed->GetKeys(&lrAnalog, &udAnalog, &additionalKey);
	pGame->GetCamera()->GetMatrix(&matPos);

	CPlayerPool *pPlayerPool = pNetGame->GetPlayerPool();
	CVehiclePool *pVehiclePool = pNetGame->GetVehiclePool();

	if(!pPlayerPool || !pVehiclePool) return;

	spSync.vecPos.X = matPos.pos.X;
	spSync.vecPos.Y = matPos.pos.Y;
	spSync.vecPos.Z = matPos.pos.Z;
	spSync.lrAnalog = lrAnalog;
	spSync.udAnalog = udAnalog;
	spSync.wKeys = wKeys;

	if((GetTickCount() - m_tickData.m_dwLastSendSpecTick) > 200)
	{
		m_tickData.m_dwLastSendSpecTick = GetTickCount();
		
		bsSpectatorSync.Write((uint8_t)ID_SPECTATOR_SYNC);
		bsSpectatorSync.Write((char*)&spSync, sizeof(SPECTATOR_SYNC_DATA));
		pNetGame->GetRakClient()->Send(&bsSpectatorSync, HIGH_PRIORITY, UNRELIABLE, 0);

		if((GetTickCount() - m_tickData.m_dwLastSendAimTick) > 400)
		{
			m_tickData.m_dwLastSendAimTick = GetTickCount();
			SendAimSyncData();
		}
	}

	pGame->DisplayHUD(false);

	m_pPlayerPed->SetHealth(100.0f);
	GetPlayerPed()->TeleportTo(spSync.vecPos.X, spSync.vecPos.Y, spSync.vecPos.Z + 20.0f);

	// handle spectate player left the server
	if(m_spectateData.m_byteSpectateType == SPECTATING_TYPE_PLAYER &&
		!pPlayerPool->GetSlotState(m_spectateData.m_dwSpectateId))
	{
		m_spectateData.m_byteSpectateType = SPECTATING_TYPE_NONE;
		m_spectateData.m_bSpectateProcessed = false;
	}

	// handle spectate player is no longer active (ie Died)
	if(m_spectateData.m_byteSpectateType == SPECTATING_TYPE_PLAYER &&
		pPlayerPool->GetSlotState(m_spectateData.m_dwSpectateId) &&
		(!pPlayerPool->GetAt(m_spectateData.m_dwSpectateId)->IsActive() ||
		pPlayerPool->GetAt(m_spectateData.m_dwSpectateId)->GetState() == PLAYER_STATE_WASTED))
	{
		m_spectateData.m_byteSpectateType = SPECTATING_TYPE_NONE;
		m_spectateData.m_bSpectateProcessed = false;
	}

	if(m_spectateData.m_bSpectateProcessed) return;

	if(m_spectateData.m_byteSpectateType == SPECTATING_TYPE_NONE)
	{
		GetPlayerPed()->RemoveFromVehicleAndPutAt(0.0f, 0.0f, 10.0f);
		pGame->GetCamera()->SetPosition(50.0f, 50.0f, 50.0f, 0.0f, 0.0f, 0.0f);
		pGame->GetCamera()->LookAtPoint(60.0f, 60.0f, 50.0f, 2);
		m_spectateData.m_bSpectateProcessed = true;
	}
	else if(m_spectateData.m_byteSpectateType == SPECTATING_TYPE_PLAYER)
	{
		uint32_t dwGTAId = 0;
		CPlayerPed *pPlayerPed = 0;

		if(pPlayerPool->GetSlotState(m_spectateData.m_dwSpectateId))
		{
			pPlayerPed = pPlayerPool->GetAt(m_spectateData.m_dwSpectateId)->GetPlayerPed();
			if(pPlayerPed)
			{
				dwGTAId = pPlayerPed->m_dwGTAId;
				ScriptCommand(&camera_on_actor, dwGTAId, m_spectateData.m_byteSpectateMode, 2);
				m_spectateData.m_bSpectateProcessed = true;
			}
		}
	}
	else if(m_spectateData.m_byteSpectateType == SPECTATING_TYPE_VEHICLE)
	{
		CVehicle *pVehicle = nullptr;
		uint32_t dwGTAId = 0;

		if (pVehiclePool->GetSlotState((VEHICLEID)m_spectateData.m_dwSpectateId)) 
		{
			pVehicle = pVehiclePool->GetAt((VEHICLEID)m_spectateData.m_dwSpectateId);
			if(pVehicle) 
			{
				dwGTAId = pVehicle->m_dwGTAId;
				ScriptCommand(&camera_on_vehicle, dwGTAId, m_spectateData.m_byteSpectateMode, 2);
				m_spectateData.m_bSpectateProcessed = true;
			}
		}
	}	
}

void CLocalPlayer::ToggleSpectating(bool bToggle)
{
	if(m_spectateData.m_bIsSpectating && !bToggle)
		Spawn();

	m_spectateData.m_bIsSpectating = bToggle;
	m_spectateData.m_byteSpectateType = SPECTATING_TYPE_NONE;
	m_spectateData.m_dwSpectateId = 0xFFFFFFFF;
	m_spectateData.m_bSpectateProcessed = false;
}

void CLocalPlayer::SpectatePlayer(PLAYERID playerId)
{
	CPlayerPool *pPlayerPool = pNetGame->GetPlayerPool();
	if(pPlayerPool && pPlayerPool->GetSlotState(playerId))
	{
		if(pPlayerPool->GetAt(playerId)->GetState() != PLAYER_STATE_NONE &&
			pPlayerPool->GetAt(playerId)->GetState() != PLAYER_STATE_WASTED)
		{
			m_spectateData.m_byteSpectateType = SPECTATING_TYPE_PLAYER;
			m_spectateData.m_dwSpectateId = playerId;
			m_spectateData.m_bSpectateProcessed = false;
		}
	}
}

void CLocalPlayer::SpectateVehicle(VEHICLEID VehicleID)
{
	CVehiclePool *pVehiclePool = pNetGame->GetVehiclePool();
	if(pVehiclePool && pVehiclePool->GetSlotState(VehicleID)) 
	{
		m_spectateData.m_byteSpectateType = SPECTATING_TYPE_VEHICLE;
		m_spectateData.m_dwSpectateId = VehicleID;
		m_spectateData.m_bSpectateProcessed = false;
	}
}

void CLocalPlayer::GiveTakeDamage(bool bGiveOrTake, uint16_t wPlayerID, float damage_amount, uint32_t weapon_id, uint32_t bodypart)
{
	RakNet::BitStream bitStream;
	
	bitStream.Write((bool)bGiveOrTake);
	bitStream.Write((uint16_t)wPlayerID);
	bitStream.Write((float)damage_amount);
	bitStream.Write((uint32_t)weapon_id);
	bitStream.Write((uint32_t)bodypart);
	
	pNetGame->GetRakClient()->RPC(&RPC_PlayerGiveTakeDamage, &bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false, UNASSIGNED_NETWORK_ID, nullptr);
}

void CLocalPlayer::ProcessVehicleDamageUpdates(uint16_t CurrentVehicle)
{
	CVehicle *pVehicle = pNetGame->GetVehiclePool()->GetAt(CurrentVehicle);
	if(!pVehicle) 
		return;
	
	// If this isn't the vehicle we were last monitoring for damage changes
	// update our stored data and return.
	if(CurrentVehicle != m_damageVehicle.m_VehicleUpdating) 
	{
        m_damageVehicle.m_dwLastPanelStatus = pVehicle->GetPanelDamageStatus();
		m_damageVehicle.m_dwLastDoorStatus = pVehicle->GetDoorDamageStatus();
		m_damageVehicle.m_byteLastLightsStatus = pVehicle->GetLightDamageStatus();
		m_damageVehicle.m_byteLastTireStatus = pVehicle->GetWheelPoppedStatus();
		m_damageVehicle.m_VehicleUpdating = CurrentVehicle;
		return;
	}

	if(m_damageVehicle.m_dwLastPanelStatus != pVehicle->GetPanelDamageStatus() ||
		m_damageVehicle.m_dwLastDoorStatus != pVehicle->GetDoorDamageStatus() ||
		m_damageVehicle.m_byteLastLightsStatus != pVehicle->GetLightDamageStatus() ||
		m_damageVehicle.m_byteLastTireStatus != pVehicle->GetWheelPoppedStatus()) 
	{			
		m_damageVehicle.m_dwLastPanelStatus = pVehicle->GetPanelDamageStatus();
		m_damageVehicle.m_dwLastDoorStatus = pVehicle->GetDoorDamageStatus();
		m_damageVehicle.m_byteLastLightsStatus = pVehicle->GetLightDamageStatus();
		m_damageVehicle.m_byteLastTireStatus = pVehicle->GetWheelPoppedStatus();

		// We need to update the server that the vehicle we're driving
		// has had its damage model modified.
		//pChatWindow->AddDebugMessage("Local::DamageModelChanged");
		RakNet::BitStream bsData;
		bsData.Write(m_damageVehicle.m_VehicleUpdating);
		bsData.Write(m_damageVehicle.m_dwLastPanelStatus);
		bsData.Write(m_damageVehicle.m_dwLastDoorStatus);
        bsData.Write(m_damageVehicle.m_byteLastLightsStatus);
        bsData.Write(m_damageVehicle.m_byteLastTireStatus);
		pNetGame->GetRakClient()->RPC(&RPC_DamageVehicle, &bsData, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false, UNASSIGNED_NETWORK_ID, NULL);
	} 
}

void CLocalPlayer::UpdateStats() 
{
	if(m_statsData.dwLastMoney != pGame->GetLocalMoney() ||
		m_statsData.byteLastDrunkLevel != pGame->GetDrunkBlur())
	{
		m_statsData.dwLastMoney = pGame->GetLocalMoney();
		m_statsData.byteLastDrunkLevel = pGame->GetDrunkBlur();

		RakNet::BitStream bsStats;
		bsStats.Write((uint8_t)ID_STATS_UPDATE);
		bsStats.Write(m_statsData.dwLastMoney);
		bsStats.Write(m_statsData.byteLastDrunkLevel);
		pNetGame->GetRakClient()->Send(&bsStats, HIGH_PRIORITY, UNRELIABLE, 0);
	}
}

void CLocalPlayer::UpdateWeapons() 
{
	if(m_pPlayerPed->IsInVehicle())
		return;
	
	bool bSend = false;
	for (uint8_t i = 0; i <= 12; i++) 
	{
		if(m_weaponData.m_byteLastWeapon[i] != m_pPlayerPed->m_pPed->WeaponSlots[i].dwType) 
		{
			m_weaponData.m_byteLastWeapon[i] = m_pPlayerPed->m_pPed->WeaponSlots[i].dwType;
			bSend = true;
		}

		if(m_weaponData.m_dwLastAmmo[i] != m_pPlayerPed->m_pPed->WeaponSlots[i].dwAmmo) 
		{
			m_weaponData.m_dwLastAmmo[i] = m_pPlayerPed->m_pPed->WeaponSlots[i].dwAmmo;
			bSend = true;
		}
	}
	
	if(bSend) 
	{
		RakNet::BitStream bsWeapons;
		bsWeapons.Write((uint8_t)ID_WEAPONS_UPDATE);
		for(uint8_t i = 0; i <= 12; ++i) 
		{
			bsWeapons.Write((uint8_t)i);
			bsWeapons.Write((uint8_t)m_weaponData.m_byteLastWeapon[i]);
			bsWeapons.Write((uint16_t)m_weaponData.m_dwLastAmmo[i]);
		}
		
		pNetGame->GetRakClient()->Send(&bsWeapons, HIGH_PRIORITY, UNRELIABLE, 0);
	}
}