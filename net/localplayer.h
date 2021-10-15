#pragma once

enum eSpectatingMode { 
	SPECTATING_MODE_VEHICLE = 3, 
	SPECTATING_MODE_PLAYER = 4, 
	SPECTATING_MODE_FIXED = 15,
	SPECTATING_MODE_SIDE = 14
}; 

enum eSpectatingType { 
	SPECTATING_TYPE_NONE, 
	SPECTATING_TYPE_PLAYER, 
	SPECTATING_TYPE_VEHICLE
}; 

enum eSurfingMode { 
	SURFING_MODE_NONE, 
	SURFING_MODE_UNFIXED, 
	SURFING_MODE_FIXED
};

enum eWeaponState
{
	WS_NO_BULLETS = 0,
	WS_LAST_BULLET = 1,
	WS_MORE_BULLETS = 2,
	WS_RELOADING = 3,
};

typedef struct _PLAYER_SPAWN_INFO
{
	uint8_t byteTeam;
	int iSkin;
	uint8_t unk;
	VECTOR vecPos;
	float fRotation;
	int iSpawnWeapons[3];
	int iSpawnWeaponsAmmo[3];
} PLAYER_SPAWN_INFO;

typedef struct _ONFOOT_SYNC_DATA
{
	uint16_t lrAnalog;				// +0
	uint16_t udAnalog;				// +2
	uint16_t wKeys;					// +4
	VECTOR vecPos;					// +6
	CQuaternion quat;				// +18
	uint8_t byteHealth;				// +34
	uint8_t byteArmour;				// +35
	uint8_t byteCurrentWeapon;		// +36
	uint8_t byteSpecialAction;		// +37
	VECTOR vecMoveSpeed;			// +38
	VECTOR vecSurfOffsets;			// +50
	uint16_t wSurfInfo;				// +62
	uint32_t dwAnimation;			// +64
} ONFOOT_SYNC_DATA;					// size = 68

typedef struct _INCAR_SYNC_DATA
{
	VEHICLEID VehicleID;			// +0
	uint16_t lrAnalog;				// +2
	uint16_t udAnalog;				// +4
	uint16_t wKeys;					// +6
	CQuaternion quat;				// +8
	VECTOR vecPos;					// +24
	VECTOR vecMoveSpeed;			// +36
	float fCarHealth;				// +48
	uint8_t bytePlayerHealth;		// +52
	uint8_t bytePlayerArmour;		// +53
	uint8_t byteCurrentWeapon;		// +54
	uint8_t byteSirenOn;			// +55
	uint8_t byteLandingGearState;	// +56
	VEHICLEID TrailerID;			// +57
	union {
		uint16_t wHydraThrustAngle[2];	// +59
		float fTrainSpeed;				// +63
	};
} INCAR_SYNC_DATA;					// size = 67

typedef struct _PASSENGER_SYNC_DATA
{
	VEHICLEID VehicleID;			// +0
	uint8_t byteSeatFlags : 6;		// +2
	uint8_t byteDriveBy : 1;		// +2
	uint8_t byteCurrentWeapon;		// +4
	uint8_t bytePlayerHealth;		// +5
	uint8_t bytePlayerArmour;		// +6
	uint16_t lrAnalog;				// +7
	uint16_t udAnalog;				// +9
	uint16_t wKeys;					// +11
	VECTOR vecPos;					// +13
} PASSENGER_SYNC_DATA;				// size = 25

typedef struct _UNOCCUPIED_SYNC_DATA
{
	uint16_t vehicleId;
	uint8_t seatId;
	VECTOR roll;
	VECTOR direction;
	VECTOR position;
	VECTOR moveSpeed;
	VECTOR turnSpeed;
	float vehicleHealth;
} UNOCCUPIED_SYNC_DATA;

typedef struct _TRAILER_SYNC_DATA
{
	VEHICLEID trailerId;	// +0
	VECTOR vecPos;			// +2
	CQuaternion quat;		// +14
	VECTOR vecMoveSpeed;	// +30
	VECTOR vecTurnSpeed;	// +42
} TRAILER_SYNC_DATA; 		// size = 54

typedef struct _AIM_SYNC_DATA
{
	uint8_t	byteCamMode;
	VECTOR vecAimf;
	VECTOR vecAimPos;
	float fAimZ;
	uint8_t byteCamExtZoom : 6;
	uint8_t byteWeaponState : 2;
	uint8_t aspect_ratio;
} AIM_SYNC_DATA;

typedef struct _BULLET_SYNC_DATA
{
	uint8_t targetType;	// +0
	uint16_t targetId;	// +1
	VECTOR vecOrigin;	// +3
	VECTOR vecPos;		// +15
	VECTOR vecOffset;	// +27
	uint8_t weaponId;	// +28
} BULLET_SYNC_DATA;		// size = 28

typedef struct _SPECTATOR_SYNC_DATA
{
	uint16_t lrAnalog;	// +0
	uint16_t udAnalog;	// +2
	uint16_t wKeys;		// +4
	VECTOR vecPos;		// +6
} SPECTATOR_SYNC_DATA;	// size = 18

typedef struct _STATS_SYNC_DATA
{
	uint32_t dwMoney;		// +0
	uint16_t wDrunkLevel;	// +4
} STATS_SYNC_DATA;			// size = 6

class CLocalPlayer
{
public:
	CLocalPlayer();
	~CLocalPlayer();

	void ResetAllSyncAttributes();

	bool Process();

	void SendWastedNotification();

	void HandleClassSelection();
	void HandleClassSelectionOutcome();
	void ReturnToClassSelection() { m_selectionClassData.m_bWantsAnotherClass = true; }

	void RequestClass(int iClass);
	void SendNextClass();
	void SendPrevClass();

	uint32_t GetPlayerColor();
	uint32_t GetPlayerColorAsARGB();
	void SetPlayerColor(uint32_t dwColor);

	void SetSpawnInfo(PLAYER_SPAWN_INFO *pSpawn);
	void RequestSpawn();
	void SendSpawn();
	bool Spawn();

	void UpdateSurfing();
	
	bool HandlePassengerEntry();
	bool HandlePassengerEntryByCommand();
	void SendEnterVehicleNotification(VEHICLEID VehicleID, bool bPassenger);
	void SendExitVehicleNotification(VEHICLEID VehicleID);

	void UpdateRemoteInterior(uint8_t byteInterior);

	void ApplySpecialAction(uint8_t byteSpecialAction);
	uint8_t GetSpecialAction();
	
	int GetOptimumOnFootSendRate();
	int GetOptimumInCarSendRate(uint8_t iPlayersEffected);
	uint8_t DetermineNumberOfPlayersInLocalRange();

	void SendOnFootFullSyncData();
	void SendInCarFullSyncData();
	void SendPassengerFullSyncData();
	void SendAimSyncData();

	void ProcessSpectating();
	void ToggleSpectating(bool bToggle);
	void SpectatePlayer(PLAYERID playerId);
	void SpectateVehicle(VEHICLEID VehicleID);
	bool IsSpectating() { return m_spectateData.m_bIsSpectating; }

	void GiveTakeDamage(bool bGiveOrTake, uint16_t wPlayerID, float damage_amount, uint32_t weapon_id, uint32_t bodypart);

	void ProcessVehicleDamageUpdates(uint16_t CurrentVehicle);

	void UpdateStats();
	void UpdateWeapons();

	CPlayerPed *GetPlayerPed() { return m_pPlayerPed; };
	bool IsInRCMode() { return m_bInRCMode; };

public:
	struct {
		bool	m_bWaitingForSpawnRequestReply;
		int		m_iSelectedClass;
		bool	m_bHasSpawnInfo;
		bool	m_bWantsAnotherClass;
		bool	m_bClearedToSpawn;
	} m_selectionClassData;

	struct {
		bool		m_bIsSpectating;
		uint8_t		m_byteSpectateMode;
		uint8_t		m_byteSpectateType;
		uint32_t	m_dwSpectateId;
		bool		m_bSpectateProcessed;
	} m_spectateData;

	VEHICLEID			m_CurrentVehicle;
	VEHICLEID 			m_LastVehicle;

private:
	CPlayerPed			*m_pPlayerPed;
	bool				m_bIsActive;
	bool				m_bIsWasted;
	uint8_t				m_byteCurInterior;
	bool				m_bInRCMode;

	bool					m_bSurfingMode;
	VECTOR					m_vecLockedSurfingOffsets;
	VEHICLEID				m_SurfingID;

	PLAYER_SPAWN_INFO 	m_SpawnInfo;
	ONFOOT_SYNC_DATA 	m_OnFootData;
	INCAR_SYNC_DATA 	m_InCarData;
	TRAILER_SYNC_DATA 	m_TrailerData;
	PASSENGER_SYNC_DATA m_PassengerData;
	AIM_SYNC_DATA 		m_aimSync;
	BULLET_SYNC_DATA 	m_BulletData;

	uint32_t m_dwLastSendSyncTick;

	struct {
		uint32_t m_dwLastSendTick;
		uint32_t m_dwLastSendAimTick;
		uint32_t m_dwLastSendSpecTick;
		uint32_t m_dwLastUpdateOnFootData;
		uint32_t m_dwLastUpdateInCarData;
		uint32_t m_dwLastUpdatePassengerData;
		uint32_t m_dwLastStatsUpdateTick;
		uint32_t m_dwPassengerEnterExit;
	} m_tickData;

	struct {
		VEHICLEID	m_VehicleUpdating;
		uint32_t	m_dwLastPanelStatus;
		uint32_t	m_dwLastDoorStatus;
		uint8_t		m_byteLastLightsStatus;
		uint8_t		m_byteLastTireStatus;
	} m_damageVehicle;

	struct {
		uint32_t 	dwLastMoney;
		uint8_t		byteLastDrunkLevel;
	} m_statsData;

	struct {
		uint8_t 	m_byteLastWeapon[13];
		uint32_t	m_dwLastAmmo[13];
	} m_weaponData;

	bool				m_bPassengerDriveByMode;
};