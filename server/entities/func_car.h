//===========================================================
// diffusion - func_car stuff
//===========================================================

#define ADDDIRT_SPEED_THRESHOLD 125.0f // car speed when body dirt state is affected (excluding water)

#define SF_CAR_ONLYTRIGGER		BIT(0) // only trigger or external button
#define SF_CAR_TURBO			BIT(1) // turbo sound (*pfsshhh...*)
#define SF_CAR_STARTSILENT		BIT(2) // StartSilent = true
#define SF_CAR_DRIFTMODE		BIT(3) // DriftMode = true
#define SF_CAR_GEARWHINE		BIT(4) // whining transmission sound, like a race car (hello NFS MW)
#define SF_CAR_ELECTRIC			BIT(5) // for now this only makes different sounds
#define SF_CAR_CANTBEDIRTY		BIT(6) // don't accumulate dirt or wetness
#define SF_CAR_TURNREARWHEELS	BIT(7) // rear wheels will turn too
#define SF_CAR_SQUEAKYBRAKES	BIT(8) // brakes make squeaky sound
#define SF_CAR_EXHAUSTPOPS		BIT(9) // do exhaust pops when player releases acceleration pedal

#define SF_CAR_DOIDLEUNSTICK	BIT(31) // internal flag

#define CAR_FOV 85

#define CAR_CAMERA_MOUSE_WAIT_ON 0.2f // mouse must be moved for this amount of seconds to trigger free camera
#define CAR_CAMERA_MOUSE_WAIT_OFF 1.25f // camera will go back to normal when mouse isn't moved for this amount of seconds

class CCar : public CBaseDelay
{
	DECLARE_CLASS( CCar, CBaseDelay );
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Setup( void );
	void Drive( void );
	void Idle( void );
	void ActivateSelfdrive( void );
	void DeactivateSelfdrive( void );
	void GetCollision( const float AbsCarSpeed, const int Forward, Vector *Collision, float *ColDotProduct, Vector *ColPoint );
	void Wheels( int *FRW_InAir, int *FLW_InAir, int *RRW_InAir, int *RLW_InAir );
	void Camera( void );
	int ObjectCaps( void );
	void ClearEffects( void );
	void OnRemove( void );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	bool ExitCar( CBaseEntity *pPlayer );
	void TryUnstick( void );
	void SetupTireSoundAtSurface( int wheelnum, float AbsCarSpeed, float *ChassisShake, float *surf_Mult );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	void CarExplode( void );
	Vector SafeSpawnPosition( void );
	float LastSafeSpawnCollectTime; // not saved
	Vector SafeSpawnPos;
	Vector SafeCarPos;

	EHANDLE hDriver;

	string_t wheel1;
	string_t wheel2;
	string_t wheel3;
	string_t wheel4;
	string_t chassis;
	string_t carhurt;
	string_t drivermdl;
	string_t chassismdl;
	string_t camera2;
	string_t tank_tower;
	string_t door_handle;
	string_t door_handle2;
	string_t exhaust1;
	string_t exhaust2;

	Vector pWheel1Org;
	Vector pWheel2Org;
	Vector pWheel3Org;
	Vector pWheel4Org;
	CBaseEntity *pWheel1;
	CBaseEntity *pWheel2;
	CBaseEntity *pWheel3;
	CBaseEntity *pWheel4;
	CBaseEntity *pChassis;
	EHANDLE pCamera1;
	CBaseEntity *pCamera2;
	EHANDLE pFreeCam;
	CBaseEntity *pCarHurt;
	CBaseAnimating *pDriverMdl;
	CBaseAnimating *pChassisMdl;
	CBaseEntity *pTankTower;
	CBaseEntity *pDoorHandle1;
	CBaseEntity *pDoorHandle2;
	CBaseEntity *pExhaust1;
	CBaseEntity *pExhaust2;

	int m_iszEngineSnd;
	static const char *pTireSounds[];
	int m_iszIdleSnd;

	string_t m_iszAltTarget;

	float CarSpeed;
	float Turning;
	bool TurningOverride;
	float CameraMoving;
	int FrontWheelRadius;
	int RearWheelRadius;
	int MaxLean;
	int FrontSuspDist; // from center
	int RearSuspDist; // from center
	int FrontSuspWidth;
	int RearSuspWidth;
	int MaxCarSpeed;
	int MaxCarSpeedBackwards;
	int AccelRate;
	int BrakeRate;
	int BackAccelRate;
	int TurningRate;
	int MaxTurn;
	float DownForce; // not saved
	float time; // not saved
	float dmgtime; // not saved
	float driveranimtime; // not saved
	float SuspHardness;
	int FrontBumperLength;
	int RearBumperLength;
	bool AllowCamera; // when Car is used with value 2.0, the camera toggles. this is to allow using trigger_camera while in car
	bool CamUnlocked;
	bool CarInAir;
	float EnteringShake; // shake the car when driver enters it (not saved)
	Vector Camera2LocalOrigin;
	Vector Camera2LocalAngles;
	Vector CameraAngles;
	float Camera2RotationY; // not saved
	float Camera2RotationX; // not saved
	float CameraBrakeOffsetX; // not saved
	int MaxCamera2Sway;
	bool SecondaryCamera; // 2nd camera is active
	// default camera view
	int CameraHeight;
	int CameraDistance;
	int FreeCameraDistance;
	float CameraModeAddDist_Main;
	float TmpCameraModeAddDist_Main; // used for lerping
	float CameraModeAddDist_Free;
	float TmpCameraModeAddDist_Free; // used for lerping
	float NewCameraAngleY;
	float NewCameraAngleX;
	float CamDistAdjust;
	float BrakeAddX; // not saved
	float AccelAddX; // not saved
	int DriverMdlSequence; // not saved
	float surf_DirtMult;
	float surf_GrassMult;
	float surf_GravelMult;
	float surf_SnowMult;
	float surf_WoodMult;
	bool BrokenCar; // special mode for intro - use car with value 3.0 to activate it
	bool StartSilent; // no idle sound until driver enters the car for the first time
	Vector PrevOrigin; // for calculating travelled distance (achievement)
	float LastTrailTime; // last time I sent a message with trail sprite (not saved)
	float StuckTime; // do not abuse stuck fix (not saved)
	float DamageMult; // if hurt, player will receive flDamage x DamageMult when in this car (can make armored cars this way...)

	float LastCarSpeed;
	float NewAccelAddX;
	float AccelAddX_ShiftAdd;

	int MaxGears;
	int GearStep; // maxspeed / 5
	float EngPitch; // engine sound pitch - not saved
	int Gear; // not saved
	int LastGear; // to track the gear change, for sound, or maybe some effects

	float LastShootTime; // not saved
	float TurboAccum; // not saved, ranged 0-1
	bool CanPlayTurboSound;

	bool DriftMode; // test
	Vector DriftAngles;
	float DriftAmount; // 1/dotproduct. range is 1.0 - 2.0, 1: going straight, 2: sideways 90 deg
	bool CarTouch;

	int m_iSpriteTexture;
	int m_iExplode;

	// check for buttons
	bool bForward( void );
	bool bBack( void );
	bool bLeft( void );
	bool bRight( void );
	bool bUp( void );
	bool bDown( void );

	bool HeatingTires; // not saved
	float HeatingMult; // not saved

	bool IsShifting;
	float ShiftingTime; // full time of the shifting sequence (usually second or less etc.)
	float ShiftStartTime;

	float AddTurnWheelShake;

	static const char *pExhaustPopSounds[];
	int num_exhausts;
	float num_pops; // not saved
	float poptime; // not saved
	CBaseEntity *currentExhaust; // not saved

	float BrakeSqueak; // volume of the squeaky sound (0-1), not saved

	int TankTowerRotationOffset; // sometimes car body model can be angled differently...MUST COMPENSATE, sigh...

	bool IsBoat;
	bool IsHeli;

	float fMouseTouchedON;
	float fMouseTouchedOFF;
	Vector LastPlayerAngles;

	DECLARE_DATADESC();
};

class CBoat : public CCar
{
	DECLARE_CLASS( CBoat, CBaseDelay );
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Setup( void );
	void Drive( void );
	void Idle( void );
	int ObjectCaps( void );
	void ClearEffects( void );
	void OnRemove( void );
	bool ExitCar( CBaseEntity *pPlayer );
	Vector SafeSpawnPosition( void );
	void GetCollision( const float AbsCarSpeed, const int Forward, Vector *Collision, float *ColDotProduct, Vector *ColPoint );

	int BoatLength;
	int BoatWidth;
	int BoatDepth; // how much boat sinks into the water. the origin of func_boat is the waterline

	Vector PushVelocity; // not saved
	float landing_effect_time; // not saved
	float AddZ;

	DECLARE_DATADESC();
};

class CHelicopter : public CCar
{
	DECLARE_CLASS( CHelicopter, CBaseDelay );
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Setup( void );
	void Drive( void );
	void Idle( void );
	int ObjectCaps( void );
	void ClearEffects( void );
	void OnRemove( void );
	bool ExitCar( CBaseEntity *pPlayer );
	void CarExplode( void );

	int HeliLength;
	int HeliWidth;
	float VerticalVelocity;
	float BladeSpeed;

	CBaseEntity *pBlade;
	string_t blade;
	CBaseEntity *pBlade2; // vertical small blade on the tail
	string_t blade2;
	float BladeAddVel; // not saved

	DECLARE_DATADESC();
};