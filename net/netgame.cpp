#include "../main.h"
#include "../game/game.h"
#include "netgame.h"

#include "../chatwindow.h"
#include "../spawnscreen.h"
#include "../extrakeyboard.h"

#include "../vendor/RakNet/SAMP/samp_auth.h"
#include "../vendor/RakNet/RakClient.h"

// voice
#include "../voice/MicroIcon.h"
#include "../voice/SpeakerList.h"
#include "../voice/Network.h"

int iExceptMessageDisplayed = 0;

#define NETGAME_VERSION 4057

//const auto encAuthKey = cryptor::create("FF2BE5E6F5D9392F57C4E66F7AD78767277C6E4F6B"); //Build 69
//const auto encAuthKey = cryptor::create("10EF38095514DBF164C8FD10438A3C9BCB4DA4A9E15");
//const auto encAuthKey = cryptor::create("E02262CF28BC542486C558D4BE9EFB716592AFAF8B");
const auto encAuthKey = cryptor::create("15121F6F18550C00AC4B4F8A167D0379BB0ACA99043");

extern CGame *pGame;
extern CSpawnScreen *pSpawnScreen;
extern CChatWindow *pChatWindow;
extern CExtraKeyBoard *pExtraKeyBoard;

int iVehiclePoolProcessFlag = 0;
int iPickupPoolProcessFlag = 0;

void RegisterRPCs(RakClientInterface* pRakClient);
void UnRegisterRPCs(RakClientInterface* pRakClient);
void RegisterScriptRPCs(RakClientInterface* pRakClient);
void UnRegisterScriptRPCs(RakClientInterface* pRakClient);

unsigned char GetPacketID(Packet *p)
{
	if(p == 0) return 255;

	if ((unsigned char)p->data[0] == ID_TIMESTAMP)
		return (unsigned char) p->data[sizeof(unsigned char) + sizeof(unsigned long)];
	else
		return (unsigned char) p->data[0];
}

CNetGame::CNetGame(const char* szHostOrIp, int iPort, const char* szPlayerName, const char* szPass)
{
	// voice
	Network::OnRaknetConnect(szHostOrIp, iPort);

	strcpy(m_szHostName, "San Andreas Multiplayer");
	strncpy(m_szHostOrIp, szHostOrIp, sizeof(m_szHostOrIp));
	m_iPort = iPort;

	m_pPlayerPool = new CPlayerPool();
	m_pPlayerPool->SetLocalPlayerName(szPlayerName);
	
	m_pVehiclePool = new CVehiclePool();
	m_pObjectPool = new CObjectPool();
	m_pPickupPool = new CPickupPool();
	m_pGangZonePool = new CGangZonePool();
	m_pLabelPool = new CText3DLabelsPool();
	m_pTextDrawPool = new CTextDrawPool();
	m_pChatBubblePool = new CChatBubblePool();
	m_pActorPool = new CActorPool();
	
	m_pRakClient = RakNetworkFactory::GetRakClientInterface();

	// Register RPC
	RegisterRPCs(m_pRakClient);

	// Register SCRIPT RPC
	RegisterScriptRPCs(m_pRakClient);

	// Server password
	m_pRakClient->SetPassword(szPass);

	m_dwLastConnectAttempt = GetTickCount();
	m_iGameState = GAMESTATE_WAIT_CONNECT;

	m_iSpawnsAvailable = 0;
	m_bHoldTime = true;
	m_byteWorldMinute = 0;
	m_byteWorldTime = 12;
	m_byteWeather =	10;
	m_fGravity = (float)0.008000000;
	m_bUseCJWalk = false;
	m_bDisableEnterExits = false;
	m_fNameTagDrawDistance = 70.0f;
	m_bZoneNames = true;
	m_bInstagib = false;
	m_iDeathDropMoney = 0;
	m_bNameTagLOS = false;

	for(int i = 0; i < 100; i++)
		m_dwMapIcons[i] = 0;

	pGame->EnableClock(false);
	pGame->EnableZoneNames(true);
	
	if(pChatWindow) 
	{
		pChatWindow->AddDebugMessage("{B9C9BF}");
		pChatWindow->AddDebugMessage("{FFFFFF}SA-MP {B9C9BF}" SAMP_VERSION " {FFFFFF}Started");
	}
}

CNetGame::~CNetGame()
{
	m_pRakClient->Disconnect(0);

	// voice
	Network::OnRaknetDisconnect();

	// Unregister RPCs
	UnRegisterRPCs(m_pRakClient);

	// Unregister SCRIPT RPCs
	UnRegisterScriptRPCs(m_pRakClient);
	
	RakNetworkFactory::DestroyRakClientInterface(m_pRakClient);

	if(m_pPlayerPool) 
	{
		delete m_pPlayerPool;
		m_pPlayerPool = nullptr;
	}

	if(m_pVehiclePool)
	{
		delete m_pVehiclePool;
		m_pVehiclePool = nullptr;
	}

	if(m_pPickupPool)
	{
		delete m_pPickupPool;
		m_pPickupPool = nullptr;
	}

	if(m_pGangZonePool)
	{
		delete m_pGangZonePool;
		m_pGangZonePool = nullptr;
	}

	if(m_pLabelPool)
	{
		delete m_pLabelPool;
		m_pLabelPool = nullptr;
	}

	if(m_pTextDrawPool)
	{
		delete m_pTextDrawPool;
		m_pTextDrawPool = nullptr;
	}

	if(m_pChatBubblePool)
	{
		delete m_pChatBubblePool;
		m_pChatBubblePool = nullptr;
	}
}

void CNetGame::Process()
{
	UpdateNetwork();

	if(m_bHoldTime)
		pGame->SetWorldTime(m_byteWorldTime, m_byteWorldMinute);

	// keep the throwable weapon models loaded
	if(!pGame->IsModelLoaded(WEAPON_MODEL_TEARGAS))
		pGame->RequestModel(WEAPON_MODEL_TEARGAS);
	if(!pGame->IsModelLoaded(WEAPON_MODEL_GRENADE))
		pGame->RequestModel(WEAPON_MODEL_GRENADE);
	if(!pGame->IsModelLoaded(WEAPON_MODEL_MOLOTOV))
		pGame->RequestModel(WEAPON_MODEL_MOLOTOV);

	// cellphone
	if(!pGame->IsModelLoaded(330)) pGame->RequestModel(330);

	if(!pGame->IsModelLoaded(OBJECT_NOMODELFILE)) pGame->RequestModel(OBJECT_NOMODELFILE);

	if(GetGameState() == GAMESTATE_CONNECTED)
	{
		// pool process
		if(m_pPlayerPool) m_pPlayerPool->Process();

		if(m_pVehiclePool && iVehiclePoolProcessFlag > 5)
		{
			try { m_pVehiclePool->Process(); }
			catch(...) { 
				if(!iExceptMessageDisplayed) {				
					pChatWindow->AddDebugMessage("Warning: Error processing vehicle pool"); 
					iExceptMessageDisplayed++;
				}
			}
			iVehiclePoolProcessFlag = 0;
		}
		else
			++iVehiclePoolProcessFlag;

		if(m_pPickupPool && iPickupPoolProcessFlag > 10) 
		{
			try { m_pPickupPool->Process(); }
			catch(...) {
				if(!iExceptMessageDisplayed) {				
					pChatWindow->AddDebugMessage("Warning: Error processing pickup pool"); 
					iExceptMessageDisplayed++;
				}
			}
			iPickupPoolProcessFlag = 0;
		}
		else
			++iPickupPoolProcessFlag;

		if(m_pObjectPool)
		{
			try { m_pObjectPool->Process(); }
			catch(...) { 
				if(!iExceptMessageDisplayed) {				
					pChatWindow->AddDebugMessage("Warning: Error processing object pool"); 
					iExceptMessageDisplayed++;
				}
			}
		}
	}
	else
	{
		CPlayerPed *pPlayer = pGame->FindPlayerPed();
		CCamera *pCamera = pGame->GetCamera();

		if(pPlayer && pCamera)
		{
			if(pPlayer->IsInVehicle())
				pPlayer->RemoveFromVehicleAndPutAt(1093.4f, -2036.5f, 82.7106f);
			else
				pPlayer->TeleportTo(1093.4f, -2036.5f, 82.7106f);

			pCamera->SetPosition(1093.0f, -2036.0f, 90.0f, 0.0f, 0.0f, 0.0f);
			pCamera->LookAtPoint(384.0f, -1557.0f, 20.0f, 2);
			pGame->SetWorldWeather(m_byteWeather);
			pGame->DisplayWidgets(false);
		}
	}

	if(GetGameState() == GAMESTATE_WAIT_CONNECT && (GetTickCount() - m_dwLastConnectAttempt) > 4000)
	{
		if(pChatWindow) pChatWindow->AddDebugMessage("Connecting to Server...", m_szHostOrIp, m_iPort);
		
		m_pRakClient->Connect(m_szHostOrIp, m_iPort, 0, 0, 5);
		m_dwLastConnectAttempt = GetTickCount();
		SetGameState(GAMESTATE_CONNECTING);
	}
}

void CNetGame::UpdateNetwork()
{
	bool breakStatus = false;
	Packet* pkt = nullptr;
	unsigned char packetIdentifier;

	while(pkt = m_pRakClient->Receive())
	{
		packetIdentifier = GetPacketID(pkt);

		switch(packetIdentifier)
		{
			case ID_AUTH_KEY:
			Log("Incoming packet: ID_AUTH_KEY");
			Packet_AuthKey(pkt);
			break;

			case ID_CONNECTION_ATTEMPT_FAILED:
			pChatWindow->AddDebugMessage("The server didn't respond. Retrying..");
			pExtraKeyBoard->Show(false);
			SpeakerList::Hide();
    		MicroIcon::Hide();
			SetGameState(GAMESTATE_WAIT_CONNECT);
			break;

			case ID_NO_FREE_INCOMING_CONNECTIONS:
			pChatWindow->AddDebugMessage("The server is full. Retrying...");
			pExtraKeyBoard->Show(false);
			SpeakerList::Hide();
    		MicroIcon::Hide();
			SetGameState(GAMESTATE_WAIT_CONNECT);
			break;

			case ID_DISCONNECTION_NOTIFICATION:
			Packet_DisconnectionNotification(pkt);
			pExtraKeyBoard->Show(false);
			SpeakerList::Hide();
    		MicroIcon::Hide();
			break;

			case ID_CONNECTION_LOST:
			Packet_ConnectionLost(pkt);
			pExtraKeyBoard->Show(false);
			SpeakerList::Hide();
    		MicroIcon::Hide();
			break;

			case ID_CONNECTION_REQUEST_ACCEPTED:
			Packet_ConnectionSucceeded(pkt);
			pExtraKeyBoard->Show(true);
			SpeakerList::Hide();
    		MicroIcon::Hide();
			break;

			case ID_FAILED_INITIALIZE_ENCRIPTION:
			pChatWindow->AddDebugMessage("Failed to initialize encryption.");
			pExtraKeyBoard->Show(false);
			SpeakerList::Hide();
    		MicroIcon::Hide();
			break;

			case ID_CONNECTION_BANNED:
			pChatWindow->AddDebugMessage("You are banned from this server.");
			pExtraKeyBoard->Show(false);
			SpeakerList::Hide();
    		MicroIcon::Hide();
			break;

			case ID_INVALID_PASSWORD:
			pChatWindow->AddDebugMessage("Wrong server password.");
			pExtraKeyBoard->Show(false);
			SpeakerList::Hide();
    		MicroIcon::Hide();
			break;

			case ID_PLAYER_SYNC:
			Packet_PlayerSync(pkt);
			break;

			case ID_VEHICLE_SYNC:
			Packet_VehicleSync(pkt);
			break;

			case ID_PASSENGER_SYNC:
			Packet_PassengerSync(pkt);
			break;

			case ID_MARKERS_SYNC:
			Packet_MarkersSync(pkt);
			break;

			case ID_AIM_SYNC:
			Packet_AimSync(pkt);
			break;

			case ID_BULLET_SYNC:
			Packet_BulletSync(pkt);
			break;
		}

		// voice
		if(!Network::OnRaknetReceive(*pkt)) breakStatus = true;
		if(breakStatus) return;
		m_pRakClient->DeallocatePacket(pkt);
	}
}

void CNetGame::ResetVehiclePool()
{
	Log("ResetVehiclePool");
	if(m_pVehiclePool)
		delete m_pVehiclePool;

	m_pVehiclePool = new CVehiclePool();
}

void CNetGame::ResetObjectPool()
{
	Log("ResetObjectPool");
	if(m_pObjectPool)
		delete m_pObjectPool;

	m_pObjectPool = new CObjectPool();
}

void CNetGame::ResetPickupPool()
{
	Log("ResetPickupPool");
	if(m_pPickupPool)
		delete m_pPickupPool;

	m_pPickupPool = new CPickupPool();
}

void CNetGame::ResetGangZonePool()
{
	Log("ResetGangZonePool");
	if(m_pGangZonePool)
		delete m_pGangZonePool;

	m_pGangZonePool = new CGangZonePool();
}

void CNetGame::ResetLabelPool()
{
	Log("ResetLabelPool");
	if(m_pLabelPool)
		delete m_pLabelPool;

	m_pLabelPool = new CText3DLabelsPool();
}

void CNetGame::ResetBubblePool()
{
	Log("ResetBubblePool");
	if(m_pChatBubblePool)
		delete m_pChatBubblePool;

	m_pChatBubblePool = new CChatBubblePool();
}

void CNetGame::ShutDownForGameRestart()
{
	// voice
	SpeakerList::Hide();
    MicroIcon::Hide();
	Network::OnRaknetDisconnect();

	for(PLAYERID playerId = 0; playerId < MAX_PLAYERS; playerId++)
	{
		CRemotePlayer* pPlayer = m_pPlayerPool->GetAt(playerId);
	}

	CLocalPlayer *pLocalPlayer = m_pPlayerPool->GetLocalPlayer();
	if(pLocalPlayer)
	{
		pLocalPlayer->ResetAllSyncAttributes();
		pLocalPlayer->ToggleSpectating(false);
	}

	m_iGameState = GAMESTATE_RESTARTING;

	m_pPlayerPool->Process();

	ResetVehiclePool();
	ResetObjectPool();
	ResetPickupPool();
	ResetGangZonePool();
	ResetLabelPool();

	m_bDisableEnterExits = false;
	m_fNameTagDrawDistance = 70.0f;
	m_byteWorldTime = 12;
	m_byteWorldMinute = 0;
	m_byteWeather = 1;
	m_bHoldTime = true;
	m_bNameTagLOS = true;
	m_bUseCJWalk = false;
	m_fGravity = 0.008f;
	m_iDeathDropMoney = 0;

	if(pSpawnScreen) pSpawnScreen->Show(false);

	CPlayerPed *pPlayerPed = pGame->FindPlayerPed();
	if(pPlayerPed)
	{
		pPlayerPed->SetInterior(0);
		pPlayerPed->SetArmour(0.0f);
	}

	pGame->ToggleCJWalk(false);
	pGame->ResetLocalMoney();
	pGame->EnableZoneNames(true);
	m_bZoneNames = true;
	GameResetRadarColors();
	pGame->SetGravity(m_fGravity);
	pGame->EnableClock(false);
}

void CNetGame::SendChatMessage(const char* szMsg)
{
	if (GetGameState() != GAMESTATE_CONNECTED) return;

	RakNet::BitStream bsSend;
	uint8_t byteTextLen = strlen(szMsg);

	bsSend.Write(byteTextLen);
	bsSend.Write(szMsg, byteTextLen);

	m_pRakClient->RPC(&RPC_Chat, &bsSend, HIGH_PRIORITY, RELIABLE, 0, false, UNASSIGNED_NETWORK_ID, NULL);
}
void CNetGame::SendClickTextDraw(uint16_t tdId)
{
	RakNet::BitStream bsSend;
	bsSend.Write(tdId);
	m_pRakClient->RPC(&RPC_TextDrawSetString, &bsSend, HIGH_PRIORITY, RELIABLE, 0, false, UNASSIGNED_NETWORK_ID, 0);
}


void CNetGame::SendChatCommand(const char* szCommand)
{
	if (GetGameState() != GAMESTATE_CONNECTED) return;

	RakNet::BitStream bsParams;
	int iStrlen = strlen(szCommand);

	bsParams.Write(iStrlen);
	bsParams.Write(szCommand, iStrlen);
	m_pRakClient->RPC(&RPC_ServerCommand, &bsParams, HIGH_PRIORITY, RELIABLE, 0, false, UNASSIGNED_NETWORK_ID, NULL);
}
void CNetGame::ProcessUpdatePingAndScore(int playerId)
{
	static unsigned int dwLastUpdateTick = 0;

	if ((GetTickCount() - dwLastUpdateTick) > 3000) {
		dwLastUpdateTick = GetTickCount();
		RakNet::BitStream bsParams;
		m_pRakClient->RPC(&RPC_UpdateScoresPingsIPs, &bsParams, HIGH_PRIORITY, RELIABLE, 0, FALSE, UNASSIGNED_NETWORK_ID, NULL);
	}
}

void CNetGame::SendDialogResponse(uint16_t wDialogID, uint8_t byteButtonID, uint16_t wListBoxItem, char* szInput)
{
	uint8_t respLen = strlen(szInput);

	RakNet::BitStream bsSend;
	bsSend.Write(wDialogID);
	bsSend.Write(byteButtonID);
	bsSend.Write(wListBoxItem);
	bsSend.Write(respLen);
	bsSend.Write(szInput, respLen);
	m_pRakClient->RPC(&RPC_DialogResponse, &bsSend, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false, UNASSIGNED_NETWORK_ID, NULL);
}

void CNetGame::SetMapIcon(uint8_t byteIndex, float fX, float fY, float fZ, uint8_t byteIcon, int iColor, int style)
{
	if(byteIndex >= 100) return;
	if(m_dwMapIcons[byteIndex]) DisableMapIcon(byteIndex);

	m_dwMapIcons[byteIndex] = pGame->CreateRadarMarkerIcon(byteIcon, fX, fY, fZ, iColor, style);
}

void CNetGame::DisableMapIcon(uint8_t byteIndex)
{
	if(byteIndex >= 100) return;
	ScriptCommand(&disable_marker, m_dwMapIcons[byteIndex]);
	m_dwMapIcons[byteIndex] = 0;
}

void gen_auth_key(char buf[260], char* auth_in);
void CNetGame::Packet_AuthKey(Packet* pkt)
{
	RakNet::BitStream bsAuth((unsigned char *)pkt->data, pkt->length, false);

	uint8_t byteAuthLen;
	char szAuth[260];

	bsAuth.IgnoreBits(8);
	bsAuth.Read(byteAuthLen);
	bsAuth.Read(szAuth, byteAuthLen);
	szAuth[byteAuthLen] = '\0';

	char szAuthKey[260];
	gen_auth_key(szAuthKey, szAuth);

	RakNet::BitStream bsKey;
	uint8_t byteAuthKeyLen = (uint8_t)strlen(szAuthKey);

	bsKey.Write((uint8_t)ID_AUTH_KEY);
	bsKey.Write((uint8_t)byteAuthKeyLen);
	bsKey.Write(szAuthKey, byteAuthKeyLen);

	m_pRakClient->Send(&bsKey, SYSTEM_PRIORITY, RELIABLE, 0);

	Log("(AUTH_KEY) %s", szAuthKey);
}

void CNetGame::Packet_DisconnectionNotification(Packet* pkt)
{
	if(pChatWindow)
		pChatWindow->AddDebugMessage("Server closed the connection.");

	m_pRakClient->Disconnect(2000);
}

void CNetGame::Packet_ConnectionLost(Packet* pkt)
{
	if(m_pRakClient) m_pRakClient->Disconnect(0);

	if(pChatWindow)
		pChatWindow->AddDebugMessage("Lost connection to the server. Reconnecting..");

	ShutDownForGameRestart();

	for(PLAYERID playerId = 0; playerId < MAX_PLAYERS; playerId++)
	{
		CRemotePlayer *pPlayer = m_pPlayerPool->GetAt(playerId);
		if(pPlayer) m_pPlayerPool->Delete(playerId, 0);
	}

	SetGameState(GAMESTATE_WAIT_CONNECT);
}

void CNetGame::Packet_ConnectionSucceeded(Packet* pkt)
{
	if(pChatWindow)
		pChatWindow->AddDebugMessage("Connected. Joining the game...");
	SetGameState(GAMESTATE_AWAIT_JOIN);

	RakNet::BitStream bsSuccAuth((unsigned char *)pkt->data, pkt->length, false);
	PLAYERID MyPlayerID;
	unsigned int uiChallenge;

	bsSuccAuth.IgnoreBits(8); // ID_CONNECTION_REQUEST_ACCEPTED
	bsSuccAuth.IgnoreBits(32); // binaryAddress
	bsSuccAuth.IgnoreBits(16); // port
	bsSuccAuth.Read(MyPlayerID);
	bsSuccAuth.Read(uiChallenge);

	m_pPlayerPool->SetLocalPlayerID(MyPlayerID);

	int iVersion = NETGAME_VERSION;
	char byteMod = 0x01;
	unsigned int uiClientChallengeResponse = uiChallenge ^ iVersion;

	char byteAuthBSLen = (char)strlen(encAuthKey.decrypt());
	char byteNameLen = (char)strlen(m_pPlayerPool->GetLocalPlayerName());
	char byteClientverLen = (char)strlen(SAMP_VERSION);

	RakNet::BitStream bsSend;
	bsSend.Write(iVersion);
	bsSend.Write(byteMod);
	bsSend.Write(byteNameLen);
	bsSend.Write(m_pPlayerPool->GetLocalPlayerName(), byteNameLen);
	bsSend.Write(uiClientChallengeResponse);
	bsSend.Write(byteAuthBSLen);
	bsSend.Write(encAuthKey.decrypt(), byteAuthBSLen);
	bsSend.Write(byteClientverLen);
	bsSend.Write(SAMP_VERSION, byteClientverLen);
	Network::OnRaknetRpc(RPC_ClientJoin, bsSend);
	m_pRakClient->RPC(&RPC_ClientJoin, &bsSend, HIGH_PRIORITY, RELIABLE, 0, false, UNASSIGNED_NETWORK_ID, NULL);
}

void CNetGame::Packet_PlayerSync(Packet* pkt)
{
	CRemotePlayer * pPlayer;
	RakNet::BitStream bsPlayerSync((unsigned char *)pkt->data, pkt->length, false);
	ONFOOT_SYNC_DATA ofSync;
	uint8_t bytePacketID=0;
	PLAYERID playerId;

	bool bHasLR,bHasUD;
	bool bHasVehicleSurfingInfo;

	if(GetGameState() != GAMESTATE_CONNECTED) return;

	memset(&ofSync, 0, sizeof(ONFOOT_SYNC_DATA));

	bsPlayerSync.Read(bytePacketID);
	bsPlayerSync.Read(playerId);

	// LEFT/RIGHT KEYS
	bsPlayerSync.Read(bHasLR);
	if(bHasLR) bsPlayerSync.Read(ofSync.lrAnalog);

	// UP/DOWN KEYS
	bsPlayerSync.Read(bHasUD);
	if(bHasUD) bsPlayerSync.Read(ofSync.udAnalog);

	// GENERAL KEYS
	bsPlayerSync.Read(ofSync.wKeys);

	// VECTOR POS
	bsPlayerSync.Read((char*)&ofSync.vecPos,sizeof(VECTOR));

	// QUATERNION
	float tw, tx, ty, tz;
	bsPlayerSync.ReadNormQuat(tw, tx, ty, tz);
	ofSync.quat.w = tw;
	ofSync.quat.x = tx;
	ofSync.quat.y = ty;
	ofSync.quat.z = tz;

	// HEALTH/ARMOUR (COMPRESSED INTO 1 BYTE)
	uint8_t byteHealthArmour;
	uint8_t byteArmTemp=0,byteHlTemp=0;

	bsPlayerSync.Read(byteHealthArmour);
	byteArmTemp = (byteHealthArmour & 0x0F);
	byteHlTemp = (byteHealthArmour >> 4);

	if(byteArmTemp == 0xF) ofSync.byteArmour = 100;
	else if(byteArmTemp == 0) ofSync.byteArmour = 0;
	else ofSync.byteArmour = byteArmTemp * 7;

	if(byteHlTemp == 0xF) ofSync.byteHealth = 100;
	else if(byteHlTemp == 0) ofSync.byteHealth = 0;
	else ofSync.byteHealth = byteHlTemp * 7;

	// CURRENT WEAPON
    bsPlayerSync.Read(ofSync.byteCurrentWeapon);
    
    // SPECIAL ACTION
    bsPlayerSync.Read(ofSync.byteSpecialAction);

    // READ MOVESPEED VECTORS
    bsPlayerSync.ReadVector(tx, ty, tz);
    ofSync.vecMoveSpeed.X = tx;
    ofSync.vecMoveSpeed.Y = ty;
    ofSync.vecMoveSpeed.Z = tz;

    bsPlayerSync.Read(bHasVehicleSurfingInfo);
    if (bHasVehicleSurfingInfo) 
    {
        bsPlayerSync.Read(ofSync.wSurfInfo);
        bsPlayerSync.Read(ofSync.vecSurfOffsets.X);
        bsPlayerSync.Read(ofSync.vecSurfOffsets.Y);
        bsPlayerSync.Read(ofSync.vecSurfOffsets.Z);
    } 
    else
    	ofSync.wSurfInfo = INVALID_VEHICLE_ID;

    if(m_pPlayerPool)
    {
    	pPlayer = m_pPlayerPool->GetAt(playerId);
    	if(pPlayer)
    		pPlayer->StoreOnFootFullSyncData(&ofSync, 0);
    }
}

void CNetGame::Packet_AimSync(Packet *pkt)
{
	uint8_t bytePacketID;
	uint16_t PlayerID;
	AIM_SYNC_DATA aimSync;
	RakNet::BitStream  bsAimSync((unsigned char*)pkt->data, pkt->length, false);

	if(GetGameState() != GAMESTATE_CONNECTED) return;

	bsAimSync.Read(bytePacketID);
	bsAimSync.Read(PlayerID);
	bsAimSync.Read((char*)&aimSync, sizeof(AIM_SYNC_DATA));

	CRemotePlayer *pRemotePlayer = m_pPlayerPool->GetAt(PlayerID);
	if(pRemotePlayer)
	{
		pRemotePlayer->StoreAimFullSyncData(&aimSync);
	}
}

void CNetGame::Packet_BulletSync(Packet *pkt)
{  
	RakNet::BitStream bsBulletSync((unsigned char*)pkt->data, pkt->length, false);

	if(GetGameState() != GAMESTATE_CONNECTED) return;

	uint8_t bytePacketID = 0;
	PLAYERID PlayerId;

	BULLET_SYNC_DATA btSync;
	memset(&btSync, 0, sizeof(BULLET_SYNC_DATA));

	bsBulletSync.Read(bytePacketID);
	bsBulletSync.Read(PlayerId);
	bsBulletSync.Read((char*)&btSync, sizeof(BULLET_SYNC_DATA));

	CRemotePlayer *pPlayer = m_pPlayerPool->GetAt(PlayerId);
	if(pPlayer)
		pPlayer->StoreBulletFullSyncData(&btSync);
}

void CNetGame::Packet_VehicleSync(Packet* pkt)
{
	CRemotePlayer *pPlayer;
	RakNet::BitStream bsSync((unsigned char *)pkt->data, pkt->length, false);
	uint8_t bytePacketID = 0;
	PLAYERID playerId;
	INCAR_SYNC_DATA icSync;

	if(GetGameState() != GAMESTATE_CONNECTED) return;

	memset(&icSync, 0, sizeof(INCAR_SYNC_DATA));

	bsSync.Read(bytePacketID);
	bsSync.Read(playerId);
	bsSync.Read(icSync.VehicleID);

	// keys
	bsSync.Read(icSync.lrAnalog);
	bsSync.Read(icSync.udAnalog);
	bsSync.Read(icSync.wKeys);

	// quaternion
	bsSync.ReadNormQuat(
		icSync.quat.w,
		icSync.quat.x,
		icSync.quat.y,
		icSync.quat.z);

	// position
	bsSync.Read((char*)&icSync.vecPos, sizeof(VECTOR));

	// speed
	bsSync.ReadVector(
		icSync.vecMoveSpeed.X,
		icSync.vecMoveSpeed.Y,
		icSync.vecMoveSpeed.Z);

	// vehicle health
	uint16_t wTempVehicleHealth;
	bsSync.Read(wTempVehicleHealth);
	icSync.fCarHealth = (float)wTempVehicleHealth;

	// health/armour
	uint8_t byteHealthArmour;
	uint8_t byteArmTemp=0, byteHlTemp=0;

	bsSync.Read(byteHealthArmour);
	byteArmTemp = (byteHealthArmour & 0x0F);
	byteHlTemp = (byteHealthArmour >> 4);

	if(byteArmTemp == 0xF) icSync.bytePlayerArmour = 100;
	else if(byteArmTemp == 0) icSync.bytePlayerArmour = 0;
	else icSync.bytePlayerArmour = byteArmTemp * 7;

	if(byteHlTemp == 0xF) icSync.bytePlayerHealth = 100;
	else if(byteHlTemp == 0) icSync.bytePlayerHealth = 0;
	else icSync.bytePlayerHealth = byteHlTemp * 7;

	// CURRENT WEAPON
	uint8_t byteTempWeapon;
	bsSync.Read(byteTempWeapon);
	icSync.byteCurrentWeapon ^= (byteTempWeapon ^ icSync.byteCurrentWeapon) & 0x3F;

	bool bCheck;

	// siren
	bsSync.Read(bCheck);
	if(bCheck) icSync.byteSirenOn = 1;
	// landinggear
	bsSync.Read(bCheck);
	if(bCheck) icSync.byteLandingGearState = 1;
	// train speed
	bsSync.Read(bCheck);
	if(bCheck) bsSync.Read(icSync.fTrainSpeed);
	// triler id
	bsSync.Read(bCheck);
	if(bCheck) bsSync.Read(icSync.TrailerID);

	if(m_pPlayerPool)
	{
		pPlayer = m_pPlayerPool->GetAt(playerId);
		if(pPlayer)
		{
			pPlayer->StoreInCarFullSyncData(&icSync, 0);
		}
	}
}

void CNetGame::Packet_PassengerSync(Packet* pkt)
{
	CRemotePlayer *pPlayer;
	uint8_t bytePacketID;
	PLAYERID playerId;
	PASSENGER_SYNC_DATA psSync;

	if(GetGameState() != GAMESTATE_CONNECTED) return;

	RakNet::BitStream bsPassengerSync((unsigned char *)pkt->data, pkt->length, false);
	bsPassengerSync.Read(bytePacketID);
	bsPassengerSync.Read(playerId);
	bsPassengerSync.Read((char*)&psSync, sizeof(PASSENGER_SYNC_DATA));

	if(m_pPlayerPool)
	{
		pPlayer = m_pPlayerPool->GetAt(playerId);
		if(pPlayer)
			pPlayer->StorePassengerFullSyncData(&psSync);
	}
}

void CNetGame::Packet_MarkersSync(Packet *pkt)
{
	CRemotePlayer *pPlayer;
	int			iNumberOfPlayers = 0;
	PLAYERID	playerId;
	short		sPos[3];
	bool		bIsPlayerActive;
	uint8_t 	unk0 = 0;

	if(GetGameState() != GAMESTATE_CONNECTED) return;

	RakNet::BitStream bsMarkersSync((unsigned char *)pkt->data, pkt->length, false);
	bsMarkersSync.Read(unk0);
	bsMarkersSync.Read(iNumberOfPlayers);

	if(iNumberOfPlayers)
	{
		for(int i=0; i<iNumberOfPlayers; i++)
		{
			bsMarkersSync.Read(playerId);
			bsMarkersSync.ReadCompressed(bIsPlayerActive);

			if(bIsPlayerActive)
			{
				bsMarkersSync.Read(sPos[0]);
				bsMarkersSync.Read(sPos[1]);
				bsMarkersSync.Read(sPos[2]);
			}

			if(playerId < MAX_PLAYERS && m_pPlayerPool->GetSlotState(playerId))
			{
				pPlayer = m_pPlayerPool->GetAt(playerId);
				if(pPlayer)
				{
					if(bIsPlayerActive)
						pPlayer->ShowGlobalMarker(sPos[0], sPos[1], sPos[2]);
					else
						pPlayer->HideGlobalMarker();
				}
			}
		}
	}
}

void CNetGame::UpdatePlayerScoresAndPings()
{
	static uint32_t dwLastUpdateTick = 0;
	if((GetTickCount() - dwLastUpdateTick) >= 3000)
	{
		dwLastUpdateTick = GetTickCount();
		RakNet::BitStream bsParams;
		m_pRakClient->RPC(&RPC_UpdateScoresPingsIPs, &bsParams, HIGH_PRIORITY,RELIABLE,0,false, UNASSIGNED_NETWORK_ID, NULL);
	}
}