#pragma once

extern uint8_t *pbyteCameraMode;
extern float *pfCameraExtZoom;
extern uint8_t *pbyteCurrentPlayer;

typedef struct _CAMERA_AIM
{
	float f1x, f1y, f1z;
	float pos1x, pos1y, pos1z;
	float pos2x, pos2y, pos2z;
	float f2x, f2y, f2z;
} CAMERA_AIM;

CAMERA_AIM* GameGetInternalAim();
uint8_t GameGetLocalPlayerCameraMode();
float GameGetAspectRatio();
float GameGetLocalPlayerCameraExtZoom();

void GameAimSyncInit();

void GameStoreLocalPlayerCameraExtZoom();
uint8_t GameGetPlayerCameraMode(uint8_t bytePlayerID);
void GameSetRemotePlayerCameraExtZoom(uint8_t bytePlayerID);
void GameStoreLocalPlayerAim();
void GameSetRemotePlayerAim(uint8_t bytePlayerID);
void GameSetLocalPlayerCameraExtZoom();
void GameSetLocalPlayerAim();
void GameSetPlayerCameraMode(uint8_t byteMode, uint8_t bytePlayerNumber);
void GameStoreRemotePlayerAim(uint8_t bytePlayerID, CAMERA_AIM *pAim);
void GameSetPlayerCameraExtZoom(uint8_t bytePlayerID, float fExtZoom, float fAspectRatio);