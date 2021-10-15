#include "main.h"
#include "game.h"

CAMERA_AIM * pcaInternalAim = nullptr;
uint8_t *pbyteCameraMode = nullptr;
float *pfCameraExtZoom = nullptr;
float *pfAspectRatio = nullptr;
uint8_t *pbyteCurrentPlayer = nullptr;

CAMERA_AIM caLocalPlayerAim;
CAMERA_AIM caRemotePlayerAim[MAX_PLAYERS];

uint8_t byteCameraMode[MAX_PLAYERS];

float fCameraExtZoom[MAX_PLAYERS];
float fLocalCameraExtZoom;

float fCameraAspectRatio[MAX_PLAYERS];
float fLocalAspectRatio;

void GameAimSyncInit()
{
	memset(&caLocalPlayerAim, 0, sizeof(CAMERA_AIM));
	memset(caRemotePlayerAim, 0, sizeof(CAMERA_AIM) * MAX_PLAYERS);
	memset(byteCameraMode, 4, MAX_PLAYERS);

	for(int i = 0; i < MAX_PLAYERS; i++) {
		fCameraExtZoom[i] = 1.0f;
		fCameraAspectRatio[i] = 0.333333f;	
	}

	pcaInternalAim = (CAMERA_AIM *)(g_libGTASA + 0x008B0AE0);
	pbyteCameraMode = (uint8_t *)(g_libGTASA + 0x008B0808 + 0x17E);
	pfAspectRatio = (float *)(g_libGTASA + 0x0098525C);
	pfCameraExtZoom = (float *)(g_libGTASA + 0x008B0808 + 0x1FC);

	pbyteCurrentPlayer = (uint8_t *)(g_libGTASA + 0x008E864C);
}

CAMERA_AIM* GameGetInternalAim()
{
	return pcaInternalAim;
}

uint8_t GameGetLocalPlayerCameraMode()
{
	// TheCamera.m_aCams[0].m_wMode
	// CWeapon::FireSniper *((unsigned __int16 *)&TheCamera + 0x108 * (unsigned __int8)TheCamera_nActiveCam + 0xBF
	// TheCamera = 0x00951FA8
	return *pbyteCameraMode;
}

float GameGetAspectRatio()
{
	return *pfAspectRatio;
}

float GameGetLocalPlayerCameraExtZoom()
{
	return (*pfCameraExtZoom - 35.0f) * 0.028571429f;
}

void GameStoreLocalPlayerCameraExtZoom()
{
	fLocalCameraExtZoom = *pfCameraExtZoom;
	fLocalAspectRatio = *pfAspectRatio;
}

void GameSetRemotePlayerCameraExtZoom(uint8_t bytePlayerID)
{
	*pfCameraExtZoom = fCameraExtZoom[bytePlayerID] * 35.0 + 35.0;
	*pfAspectRatio = fCameraAspectRatio[bytePlayerID] + 1.0;
}

uint8_t GameGetPlayerCameraMode(uint8_t bytePlayerID)
{
	return byteCameraMode[bytePlayerID];
}

void GameStoreLocalPlayerAim()
{
	memcpy(&caLocalPlayerAim, pcaInternalAim, sizeof(CAMERA_AIM));
}

void GameSetRemotePlayerAim(uint8_t bytePlayerID)
{
	memcpy(pcaInternalAim, &caRemotePlayerAim[bytePlayerID], sizeof(CAMERA_AIM));
}

void GameSetLocalPlayerCameraExtZoom()
{
	*pfCameraExtZoom = fLocalCameraExtZoom;
	*pfAspectRatio = fLocalAspectRatio;
}

void GameSetLocalPlayerAim()
{
	memcpy(pcaInternalAim, &caLocalPlayerAim, sizeof(CAMERA_AIM));
}

void GameSetPlayerCameraMode(uint8_t byteMode, uint8_t bytePlayerNumber)
{
	byteCameraMode[bytePlayerNumber] = byteMode;
}

void GameStoreRemotePlayerAim(uint8_t bytePlayerNumber, CAMERA_AIM* pAim)
{
	memcpy(&caRemotePlayerAim[bytePlayerNumber], pAim, sizeof(CAMERA_AIM));
}

void GameSetPlayerCameraExtZoom(uint8_t bytePlayerID, float fExtZoom, float fAspectRatio)
{
	fCameraExtZoom[bytePlayerID] = fExtZoom;
	fCameraAspectRatio[bytePlayerID] = fAspectRatio;
}