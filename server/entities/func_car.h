//===========================================================
// diffusion - func_car stuff
//===========================================================
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
	void Camera( void );
	int ObjectCaps( void );
	void ClearEffects( void );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	bool ExitCar( CBaseEntity *pPlayer );
	void TryUnstick( void );
	void SetupTireSoundAtSurface( int wheelnum, float AbsCarSpeed, float *ChassisShake, float *surf_Mult );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	void CarExplode( void );
	Vector SafeSpawnPosition( void );
	float LastSafeSpawnCollectTime; // not saved
	Vector SafeSpawnPos;

	EHANDLE hDriver;

	string_t wheel1;
	string_t wheel2;
	string_t wheel3;
	string_t wheel4;
	string_t chassis;
	string_t carhurt;
	string_t drivermdl;
	string_t chassismdl;
	string_t camera1;
	string_t camera2;
	string_t tank_tower;

	Vector pWheel1Org;
	Vector pWheel2Org;
	Vector pWheel3Org;
	Vector pWheel4Org;
	CBaseEntity *pWheel1;
	CBaseEntity *pWheel2;
	CBaseEntity *pWheel3;
	CBaseEntity *pWheel4;
	CBaseEntity *pChassis;
	CBaseEntity *pCamera1;
	CBaseEntity *pCamera2;
	CBaseEntity *pFreeCam;
	CBaseEntity *pCarHurt;
	CBaseAnimating *pDriverMdl;
	CBaseAnimating *pChassisMdl;
	CBaseEntity *pTankTower;

	int m_iszEngineSnd;
	static const char *pTireSounds[];
	int m_iszIdleSnd;

	string_t m_iszAltTarget;

	float CarSpeed;
	float Turning;
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
	float CollisionAddTurn; // not saved
	float SuspHardness;
	int FrontBumperLength;
	int RearBumperLength;
	bool AllowCamera; // when Car is used with value 2.0, the camera toggles. this is to allow using trigger_camera while in car
	bool CamUnlocked;
	bool CarInAir;
	Vector Camera1LocalOrigin; // remember camera origins and angles'
	Vector Camera2LocalOrigin;
	Vector Camera1LocalAngles;
	Vector Camera2LocalAngles;
	float CameraBrakeOffsetX; // not saved
	int MaxCamera1Sway;
	int MaxCamera2Sway;
	bool SecondaryCamera; // 2nd camera is active
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
	bool ExitCar( CBaseEntity *pPlayer );
	Vector SafeSpawnPosition( void );

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