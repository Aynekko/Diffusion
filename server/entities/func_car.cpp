#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "triggers.h"
#include "game/gamerules.h"
#include "weapons/weapons.h"

//============================================================
// diffusion - something similar to func_vehicle,
// maybe slightly more advanced...
// A lot of Magic Numbers(tm)
// USE AT YOUR OWN RISK.
//============================================================

// iuser1 is used for car spawn angle
// fuser2 is used for both body and wheels' models to control the amount of dirt on the car
// fuser3 on wheels is used to desync the sounds...thanks to FWGS update I have to do this, otherwise I'd need to use 4 different sounds :|

BEGIN_DATADESC( CCar )
	DEFINE_KEYFIELD( wheel1, FIELD_STRING, "wheel1" ),
	DEFINE_KEYFIELD( wheel2, FIELD_STRING, "wheel2" ),
	DEFINE_KEYFIELD( wheel3, FIELD_STRING, "wheel3" ),
	DEFINE_KEYFIELD( wheel4, FIELD_STRING, "wheel4" ),
	DEFINE_KEYFIELD( chassis, FIELD_STRING, "chassis" ),
	DEFINE_KEYFIELD( carhurt, FIELD_STRING, "carhurt" ),
	DEFINE_KEYFIELD( camera2, FIELD_STRING, "camera2" ),
	DEFINE_KEYFIELD( tank_tower, FIELD_STRING, "tank_tower" ),
	DEFINE_KEYFIELD( TankTowerRotationOffset, FIELD_INTEGER, "tank_tower_rotation" ),
	DEFINE_KEYFIELD( door_handle, FIELD_STRING, "door_handle" ),
	DEFINE_KEYFIELD( door_handle2, FIELD_STRING, "door_handle2" ),
	DEFINE_KEYFIELD( exhaust1, FIELD_STRING, "exhaust1" ),
	DEFINE_KEYFIELD( exhaust2, FIELD_STRING, "exhaust2" ),
	DEFINE_FIELD( Camera2LocalOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( Camera2LocalAngles, FIELD_VECTOR ),
	DEFINE_FIELD( CameraAngles, FIELD_VECTOR ),
	DEFINE_KEYFIELD( MaxCamera2Sway, FIELD_INTEGER, "maxcam2sway" ),
	DEFINE_KEYFIELD( drivermdl, FIELD_STRING, "drivermdl" ),
	DEFINE_KEYFIELD( chassismdl, FIELD_STRING, "chassismdl" ),
	DEFINE_KEYFIELD( FrontWheelRadius, FIELD_INTEGER, "frontwheelradius" ),
	DEFINE_KEYFIELD( RearWheelRadius, FIELD_INTEGER, "rearwheelradius" ),
	DEFINE_KEYFIELD( MaxLean, FIELD_INTEGER, "maxlean" ),
	DEFINE_KEYFIELD( FrontSuspDist, FIELD_INTEGER, "frontsuspdist" ),
	DEFINE_KEYFIELD( RearSuspDist, FIELD_INTEGER, "rearsuspdist" ),
	DEFINE_KEYFIELD( FrontSuspWidth, FIELD_INTEGER, "frontsuspwidth" ),
	DEFINE_KEYFIELD( RearSuspWidth, FIELD_INTEGER, "rearsuspwidth" ),
	DEFINE_KEYFIELD( MaxCarSpeed, FIELD_INTEGER, "maxspeed" ),
	DEFINE_KEYFIELD( MaxCarSpeedBackwards, FIELD_INTEGER, "maxspeedback" ),
	DEFINE_KEYFIELD( AccelRate, FIELD_INTEGER, "accelrate" ),
	DEFINE_KEYFIELD( BackAccelRate, FIELD_INTEGER, "backaccelrate" ),
	DEFINE_KEYFIELD( BrakeRate, FIELD_INTEGER, "brakerate" ),
	DEFINE_KEYFIELD( TurningRate, FIELD_INTEGER, "turningrate" ),
	DEFINE_KEYFIELD( MaxTurn, FIELD_INTEGER, "maxturn" ),
	DEFINE_KEYFIELD( SuspHardness, FIELD_FLOAT, "susphardness"),
	DEFINE_KEYFIELD( FrontBumperLength, FIELD_INTEGER, "frontbumplen" ),
	DEFINE_KEYFIELD( RearBumperLength, FIELD_INTEGER, "rearbumplen" ),
	DEFINE_KEYFIELD( surf_DirtMult, FIELD_FLOAT, "surfdirt" ),
	DEFINE_KEYFIELD( surf_GrassMult, FIELD_FLOAT, "surfgrass" ),
	DEFINE_KEYFIELD( surf_GravelMult, FIELD_FLOAT, "surfgravel" ),
	DEFINE_KEYFIELD( surf_SnowMult, FIELD_FLOAT, "surfsnow" ),
	DEFINE_KEYFIELD( surf_WoodMult, FIELD_FLOAT, "surfwood" ),
	DEFINE_KEYFIELD( DamageMult, FIELD_FLOAT, "damagemult" ),
	DEFINE_KEYFIELD( CameraHeight, FIELD_INTEGER, "camheight" ),
	DEFINE_KEYFIELD( CameraDistance, FIELD_INTEGER, "camdistance" ),
	DEFINE_KEYFIELD( FreeCameraDistance, FIELD_INTEGER, "freecamdistance" ),
	DEFINE_FIELD( hDriver, FIELD_EHANDLE ),
	DEFINE_FUNCTION( Setup ),
	DEFINE_FUNCTION( Drive ),
	DEFINE_FUNCTION( Idle ),
	DEFINE_FIELD( pWheel1, FIELD_EHANDLE ),
	DEFINE_FIELD( pWheel2, FIELD_EHANDLE ),
	DEFINE_FIELD( pWheel3, FIELD_EHANDLE ),
	DEFINE_FIELD( pWheel4, FIELD_EHANDLE ),
	DEFINE_FIELD( pChassis, FIELD_EHANDLE ),
	DEFINE_FIELD( pCamera1, FIELD_EHANDLE ),
	DEFINE_FIELD( pCamera2, FIELD_EHANDLE ),
	DEFINE_FIELD( pFreeCam, FIELD_EHANDLE ),
	DEFINE_FIELD( pCarHurt, FIELD_EHANDLE ),
	DEFINE_FIELD( pDriverMdl, FIELD_EHANDLE ),
	DEFINE_FIELD( pChassisMdl, FIELD_EHANDLE ),
	DEFINE_FIELD( pTankTower, FIELD_EHANDLE ),
	DEFINE_FIELD( pDoorHandle1, FIELD_EHANDLE ),
	DEFINE_FIELD( pDoorHandle2, FIELD_EHANDLE ),
	DEFINE_FIELD( pExhaust1, FIELD_EHANDLE ),
	DEFINE_FIELD( pExhaust2, FIELD_EHANDLE ),
	DEFINE_KEYFIELD( m_iszEngineSnd, FIELD_STRING, "enginesnd" ),
	DEFINE_KEYFIELD( m_iszIdleSnd, FIELD_STRING, "idlesnd" ),
	DEFINE_FIELD( AllowCamera, FIELD_BOOLEAN ),
	DEFINE_FIELD( CarInAir, FIELD_BOOLEAN ),
	DEFINE_FIELD( BrokenCar, FIELD_BOOLEAN ),
	DEFINE_FIELD( StartSilent, FIELD_BOOLEAN ),
	DEFINE_FIELD( PrevOrigin, FIELD_VECTOR ),
	DEFINE_KEYFIELD( m_iszAltTarget, FIELD_STRING, "m_iszAltTarget" ),
	DEFINE_FIELD( GearStep, FIELD_INTEGER ),
	DEFINE_FIELD( LastGear, FIELD_INTEGER ),
	DEFINE_KEYFIELD( MaxGears, FIELD_INTEGER, "maxgears" ),
	DEFINE_FIELD( DriftAngles, FIELD_VECTOR ),
	DEFINE_FIELD( DriftMode, FIELD_BOOLEAN ),
	DEFINE_FIELD( DriftAmount, FIELD_FLOAT ),
	DEFINE_KEYFIELD( ShiftingTime, FIELD_FLOAT, "shiftingtime" ),
	DEFINE_FIELD( num_exhausts, FIELD_INTEGER ),
	DEFINE_FIELD( CameraModeAddDist_Main, FIELD_FLOAT ),
	DEFINE_FIELD( TmpCameraModeAddDist_Main, FIELD_FLOAT ),
	DEFINE_FIELD( CameraModeAddDist_Free, FIELD_FLOAT ),
	DEFINE_FIELD( TmpCameraModeAddDist_Free, FIELD_FLOAT ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( func_car, CCar );

bool CCar::bBack(void)
{
	if( hDriver == NULL )
		return false;

	if( hDriver->pev->button & IN_BACK )
		return true;

	return false;
}

bool CCar::bForward( void )
{
	if( hDriver == NULL )
		return false;

	if( hDriver->pev->button & IN_FORWARD )
		return true;

	return false;
}

bool CCar::bLeft( void )
{
	if( hDriver == NULL )
		return false;

	if( hDriver->pev->button & IN_MOVELEFT )
		return true;

	return false;
}

bool CCar::bRight( void )
{
	if( hDriver == NULL )
		return false;

	if( hDriver->pev->button & IN_MOVERIGHT )
		return true;

	return false;
}

bool CCar::bUp( void )
{
	if( hDriver == NULL )
		return false;

	if( hDriver->pev->button & IN_JUMP )
		return true;

	return false;
}

bool CCar::bDown( void )
{
	if( hDriver == NULL )
		return false;

	if( hDriver->pev->button & IN_DUCK )
		return true;

	return false;
}

const char *CCar::pTireSounds[] =
{
	"func_car/tires/default.wav",
	"func_car/tires/dirt.wav",
	"func_car/tires/grass.wav",
	"func_car/tires/gravel.wav",
	"func_car/tires/snow.wav",
	"func_car/tires/wood.wav",
	"func_car/tires/water.wav",
	"func_car/tires/brake_dirt.wav",
	"func_car/tires/brake_asphalt.wav",
};

const char *CCar::pExhaustPopSounds[] =
{
	"func_car/pop01.wav",
	"func_car/pop02.wav",
	"func_car/pop03.wav",
};

int CCar::ObjectCaps( void )
{
	if( HasSpawnFlags( SF_CAR_ONLYTRIGGER ) )
		return 0;

	if( pDoorHandle1 )
		return 0;

	return FCAP_IMPULSE_USE;
}

void CCar::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "wheel1" ) )
	{
		wheel1 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "wheel2" ) )
	{
		wheel2 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "wheel3" ) )
	{
		wheel3 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "wheel4" ) )
	{
		wheel4 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "chassis" ) )
	{
		chassis = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "drivermdl" ) )
	{
		drivermdl = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "chassismdl" ) )
	{
		chassismdl = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "carhurt" ) )
	{
		carhurt = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "camera2" ) )
	{
		camera2 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "enginesnd" ) )
	{
		m_iszEngineSnd = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "idlesnd" ) )
	{
		m_iszIdleSnd = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "frontsuspdist" ) )
	{
		FrontSuspDist = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "rearsuspdist" ) )
	{
		RearSuspDist = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "frontsuspwidth" ) )
	{
		FrontSuspWidth = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "rearsuspwidth" ) )
	{
		RearSuspWidth = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "maxlean" ) )
	{
		MaxLean = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "maxspeed" ) )
	{
		MaxCarSpeed = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "maxspeedback" ) )
	{
		MaxCarSpeedBackwards = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "accelrate" ) )
	{
		AccelRate = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "backaccelrate" ) )
	{
		BackAccelRate = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "brakerate" ) )
	{
		BrakeRate = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "turningrate" ) )
	{
		TurningRate = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "maxturn" ) )
	{
		MaxTurn = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "maxcam2sway" ) )
	{
		MaxCamera2Sway = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "camheight" ) )
	{
		CameraHeight = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "camdistance" ) )
	{
		CameraDistance = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "freecamdistance" ) )
	{
		FreeCameraDistance = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "frontwheelradius" ) )
	{
		FrontWheelRadius = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "rearwheelradius" ) )
	{
		RearWheelRadius = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "susphardness" ) )
	{
		SuspHardness = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "frontbumplen" ) )
	{
		FrontBumperLength = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "rearbumplen" ) )
	{
		RearBumperLength = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "surfdirt" ) )
	{
		surf_DirtMult = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "surfgrass" ) )
	{
		surf_GrassMult = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "surfgravel" ) )
	{
		surf_GravelMult = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "surfsnow" ) )
	{
		surf_SnowMult = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "surfwood" ) )
	{
		surf_WoodMult = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "damagemult" ) )
	{
		DamageMult = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_iszAltTarget" ) )
	{
		m_iszAltTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "maxgears" ) )
	{
		MaxGears = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "tank_tower" ) )
	{
		tank_tower = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "door_handle" ) )
	{
		door_handle = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "door_handle2" ) )
	{
		door_handle2 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "exhaust1" ) )
	{
		exhaust1 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "exhaust2" ) )
	{
		exhaust2 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "shiftingtime" ) )
	{
		ShiftingTime = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "tank_tower_rotation" ) )
	{
		TankTowerRotationOffset = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}

void CCar::Precache( void )
{
	if( m_iszEngineSnd )
		PRECACHE_SOUND( (char *)STRING( m_iszEngineSnd ) );

	if( m_iszIdleSnd )
		PRECACHE_SOUND( (char *)STRING( m_iszIdleSnd ) );

	PRECACHE_SOUND( "func_car/car_impact1.wav" );
	PRECACHE_SOUND( "func_car/car_impact2.wav" );
	PRECACHE_SOUND( "func_car/car_impact3.wav" );
	PRECACHE_SOUND( "func_car/car_impact4.wav" );
	PRECACHE_SOUND( "func_car/eng_start.wav" );
	PRECACHE_SOUND( "func_car/eng_start_elec.wav" );
	PRECACHE_SOUND( "func_car/eng_stop.wav" );
	PRECACHE_SOUND( "func_car/eng_stop_elec.wav" );
	PRECACHE_SOUND( "func_car/landing.wav" );
	if( HasSpawnFlags( SF_CAR_EXHAUSTPOPS ) )
	{
		for( int i = 0; i < SIZEOFARRAY( pExhaustPopSounds ); i++ )
			PRECACHE_SOUND( (char *)pExhaustPopSounds[i] );
	}
	PRECACHE_SOUND( "func_car/gear1.wav" );
	PRECACHE_SOUND( "func_car/gear2.wav" );
	PRECACHE_SOUND( "func_car/gear3.wav" );
	if( HasSpawnFlags(SF_CAR_GEARWHINE ))
		PRECACHE_SOUND( "func_car/gear_whine.wav" );
	if( HasSpawnFlags( SF_CAR_TURBO ) )
		PRECACHE_SOUND( "func_car/turbo.wav" );
	if( HasSpawnFlags( SF_CAR_SQUEAKYBRAKES ))
		PRECACHE_SOUND( "func_car/brakes.wav" );

	for( int i = 0; i < SIZEOFARRAY( pTireSounds ); i++ )
		PRECACHE_SOUND( (char *)pTireSounds[i] );

	PRECACHE_SOUND( "drone/drone_hit1.wav" );
	PRECACHE_SOUND( "drone/drone_hit2.wav" );
	PRECACHE_SOUND( "drone/drone_hit3.wav" );
	PRECACHE_SOUND( "drone/drone_hit4.wav" );
	PRECACHE_SOUND( "drone/drone_hit5.wav" );

	m_iSpriteTexture = PRECACHE_MODEL( "sprites/white.spr" );
	m_iExplode = PRECACHE_MODEL( "sprites/fexplo.spr" );
	PRECACHE_SOUND( "weapons/mortarhit.wav" );

	UTIL_PrecacheOther( "apc_projectile" );

	pev->spawnflags |= SF_CAR_DOIDLEUNSTICK;
}

void CCar::Spawn( void )
{
	Precache();

	if( !FrontSuspWidth )
		FrontSuspWidth = 64;
	if( !RearSuspWidth )
		RearSuspWidth = 64;
	if( !FrontSuspDist )
		FrontSuspDist = 60;
	if( !RearSuspDist )
		RearSuspDist = 60;
	if( !MaxCarSpeed )
		MaxCarSpeed = 180; // km/h
	if( !MaxCarSpeedBackwards )
		MaxCarSpeedBackwards = 20;
	if( !AccelRate )
		AccelRate = 200;
	if( !BrakeRate )
		BrakeRate = 800;
	if( !BackAccelRate )
		BackAccelRate = 150;
	if( !TurningRate )
		TurningRate = 500;
	if( !MaxTurn )
		MaxTurn = 45;
	if( !FrontWheelRadius )
		FrontWheelRadius = 16;
	if( !RearWheelRadius )
		RearWheelRadius = 16;
	if( !SuspHardness || SuspHardness < 1.0f )
		SuspHardness = 10.0f;
	if( !FrontBumperLength )
		FrontBumperLength = 45;
	if( !RearBumperLength )
		RearBumperLength = 45;
	if( !surf_DirtMult )
		surf_DirtMult = 1.0f;
	if( !surf_GrassMult )
		surf_GrassMult = 1.0f;
	if( !surf_GravelMult )
		surf_GravelMult = 1.0f;
	if( !surf_SnowMult )
		surf_SnowMult = 1.0f;
	if( !surf_WoodMult )
		surf_WoodMult = 1.0f;
	if( !DamageMult )
		DamageMult = 1.0f;
	if( !MaxGears )
		MaxGears = 5;
	else if( MaxGears > 7 )
		MaxGears = 7;
	if( !ShiftingTime )
		ShiftingTime = 0.2f;
	if( !pev->frame ) // friction
		pev->frame = 1.0f;
	if( !TankTowerRotationOffset )
		TankTowerRotationOffset = 0;

	ShiftStartTime = 0;
	IsShifting = false;

	// convert km/h to units
	MaxCarSpeed = MaxCarSpeed * 15;
	MaxCarSpeedBackwards = MaxCarSpeedBackwards * 15;
	GearStep = MaxCarSpeed / MaxGears;
	TurboAccum = 0.0f;
	CameraBrakeOffsetX = 0.0f;
	BrakeSqueak = 0.0f;

	if( HasSpawnFlags( SF_CAR_STARTSILENT ) )
		StartSilent = true;

	pev->movetype = MOVETYPE_PUSHSTEP;
	pev->solid = SOLID_SLIDEBOX;
	SET_MODEL( edict(), GetModel() );
	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16,16,32 ) );

	SetLocalAngles( m_vecTempAngles );
	m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity( this );

	AllowCamera = true;

	PrevOrigin = GetAbsOrigin(); // for achievement purposes, to measure distance

	if( HasSpawnFlags(SF_CAR_DRIFTMODE) ) // experimental and unfinished...
	{
		DriftMode = true;
		DriftAngles = GetAbsAngles();
	}

	SafeSpawnPos = g_vecZero;
	SafeCarPos = g_vecZero;

	SetThink( &CCar::Setup );
	SetNextThink( RANDOM_FLOAT( 0.1f, 0.2f ) );
}

void CCar::ActivateSelfdrive( void )
{
	if( StartSilent )
		StartSilent = false;

	if( pCarHurt )
		pCarHurt->pev->frags = 1;

	if( hDriver == NULL ) // the car could be already on by previous script
	{
		if( HasSpawnFlags( SF_CAR_ELECTRIC ) )
		{
			EMIT_SOUND( edict(), CHAN_STATIC, "func_car/eng_start_elec.wav", VOL_NORM, ATTN_NORM );
			MESSAGE_BEGIN( MSG_ONE, gmsgTempEnt, NULL, hDriver->pev );
			WRITE_BYTE( TE_CARPARAMS );
			WRITE_BYTE( 0 );
			MESSAGE_END();
		}
		else
			EMIT_SOUND( edict(), CHAN_STATIC, "func_car/eng_start.wav", VOL_NORM, ATTN_NORM );

		if( m_iszIdleSnd )
			STOP_SOUND( edict(), CHAN_WEAPON, STRING( m_iszIdleSnd ) );
	}

	time = gpGlobals->time;
	DriverMdlSequence = -1;
	CameraBrakeOffsetX = 0;
	TurboAccum = 0;
	if( HasSpawnFlags( SF_CAR_ELECTRIC ) )
		IsShifting = false;
	else
		IsShifting = true;
	Gear = 1;
	LastGear = -1;
	ShiftStartTime = gpGlobals->time - ShiftingTime;
	CameraAngles = GetAbsAngles(); // make sure camera is angled properly when we enter the vehicle
	NewCameraAngleY = CameraAngles.y;
	NewCameraAngleX = 0;
	AccelAddX = BrakeAddX = 0;
	if( pExhaust1 )
	{
		pExhaust1->pev->iuser3 = -665;
		pExhaust1->pev->fuser1 = 0.1f;
		pExhaust1->pev->renderamt = 0;
	}
	if( pExhaust2 )
	{
		pExhaust2->pev->iuser3 = -665;
		pExhaust2->pev->fuser1 = 0.1f;
		pExhaust2->pev->renderamt = 0;
	}
	num_pops = 0.0f;
	poptime = 0.0f;

	TurningOverride = true;

	SetThink( &CCar::Drive );
	SetNextThink( 0 );
}

void CCar::DeactivateSelfdrive( void )
{
	if( pTankTower )
	{
		Vector TowerAngles = pChassis->GetAbsAngles();

		if( pChassisMdl )
		{
			TowerAngles = pChassisMdl->GetLocalAngles();
			TowerAngles.y += TankTowerRotationOffset;
			pTankTower->SetLocalAngles( TowerAngles );
		}
		else
			pTankTower->SetAbsAngles( TowerAngles );
	}

	if( pExhaust1 )
	{
		pExhaust1->pev->iuser3 = 0;
		pExhaust1->pev->fuser2 = 0.0f; // alpha multiplier
		pExhaust1->pev->renderamt = 0;
	}
	if( pExhaust2 )
	{
		pExhaust2->pev->iuser3 = 0;
		pExhaust2->pev->fuser2 = 0.0f; // alpha multiplier
		pExhaust2->pev->renderamt = 0;
	}
	num_pops = 0.0f;

	if( m_iszEngineSnd ) // stop engine sounds
	{
		STOP_SOUND( edict(), CHAN_WEAPON, STRING( m_iszEngineSnd ) );
		if( HasSpawnFlags( SF_CAR_ELECTRIC ) )
			EMIT_SOUND( edict(), CHAN_STATIC, "func_car/eng_stop_elec.wav", VOL_NORM, ATTN_NORM );
		else
			EMIT_SOUND( edict(), CHAN_STATIC, "func_car/eng_stop.wav", VOL_NORM, ATTN_NORM );
		if( HasSpawnFlags( SF_CAR_GEARWHINE ) )
			STOP_SOUND( edict(), CHAN_VOICE, "func_car/gear_whine.wav" );
	}

	if( pCarHurt )
		pCarHurt->pev->frags = 0;

	pev->owner = NULL;
	time = gpGlobals->time;
	hDriver = NULL;
	CameraBrakeOffsetX = 0;
	TurboAccum = 0;

	// clear tires' sound
//	EMIT_SOUND( edict(), CHAN_ITEM, "common/null.wav", 1, 3.0 );
	EMIT_SOUND( pWheel1->edict(), CHAN_ITEM, "common/null.wav", 0, 3.0 );
	EMIT_SOUND( pWheel2->edict(), CHAN_ITEM, "common/null.wav", 0, 3.0 );
	EMIT_SOUND( pWheel3->edict(), CHAN_ITEM, "common/null.wav", 0, 3.0 );
	EMIT_SOUND( pWheel4->edict(), CHAN_ITEM, "common/null.wav", 0, 3.0 );
	// clear brake squeak
	if( HasSpawnFlags( SF_CAR_SQUEAKYBRAKES ) )
		EMIT_SOUND( pWheel3->edict(), CHAN_BODY, "common/null.wav", 0, 3.0 );
	// clear particles (speed is stored here)
	pWheel1->pev->fuser1 = 0;
	pWheel2->pev->fuser1 = 0;
	pWheel3->pev->fuser1 = 0;
	pWheel4->pev->fuser1 = 0;
	// clear sound desync
	pWheel1->pev->fuser3 = 0;
	pWheel2->pev->fuser3 = 0;
	pWheel3->pev->fuser3 = 0;
	pWheel4->pev->fuser3 = 0;

	IsShifting = false;
	ShiftStartTime = 0;
	LastGear = -1;

	TurningOverride = false;

	SetThink( &CCar::Idle );
	SetNextThink( 0 );
}

void CCar::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( !pActivator || !pActivator->IsPlayer() )
		pActivator = UTIL_PlayerByIndex( 1 );

	CBasePlayer *pPlayer = (CBasePlayer *)pActivator;

	if( IsLockedByMaster() && (useType != USE_OFF) )
	{
		if( pev->message )
			UTIL_ShowMessage( STRING(pev->message), pPlayer );
		if( m_iszAltTarget )
			UTIL_FireTargets( m_iszAltTarget, pPlayer, this, USE_TOGGLE, 0 );
		return;
	}

	if( value == 2.0f ) // only allowing camera
	{
		AllowCamera = !AllowCamera;
		return;
	}

	if( value == 3.0f ) // special mode for chapter 1 intro
	{
		BrokenCar = !BrokenCar;
		return;
	}

	if( hDriver != NULL )
	{
		if( hDriver != pPlayer )
		{
			UTIL_ShowMessage( "The car is occupied!", pPlayer );
			return;
		}
	}

	if( pPlayer->pCar == this || useType == USE_OFF ) // player in this car, go out
	{
		if( !ExitCar( pPlayer ) )
			return;
		
		if( AllowCamera ) // only set view if camera was allowed, otherwise maybe we were using some external camera, don't interrupt it
		{
			SET_VIEW( pPlayer->edict(), pPlayer->edict() );
			pPlayer->m_flFOV = 0;
		}

		// reset player's angles, look in the vehicle direction
		if( !(pPlayer->pev->effects & EF_UPSIDEDOWN) )
		{
			pPlayer->SetAbsAngles( GetAbsAngles() );
			pPlayer->pev->fixangle = TRUE;
		}
		CamUnlocked = false;
		if( pTankTower )
		{
			Vector TowerAngles = pChassis->GetAbsAngles();

			if( pChassisMdl )
			{
				TowerAngles = pChassisMdl->GetLocalAngles();
				TowerAngles.y += TankTowerRotationOffset;
				pTankTower->SetLocalAngles( TowerAngles );
			}
			else
				pTankTower->SetAbsAngles( TowerAngles );
		}

		if( pExhaust1 )
		{
			pExhaust1->pev->iuser3 = 0;
			pExhaust1->pev->fuser2 = 0.0f; // alpha multiplier
			pExhaust1->pev->renderamt = 0;
		}
		if( pExhaust2 )
		{
			pExhaust2->pev->iuser3 = 0;
			pExhaust2->pev->fuser2 = 0.0f; // alpha multiplier
			pExhaust2->pev->renderamt = 0;
		}
		num_pops = 0.0f;

		if( pPlayer == hDriver )
		{
			if( m_iszEngineSnd ) // stop engine sounds
			{
				STOP_SOUND( edict(), CHAN_WEAPON, STRING( m_iszEngineSnd ) );
				if( HasSpawnFlags( SF_CAR_ELECTRIC ) )
					EMIT_SOUND( edict(), CHAN_STATIC, "func_car/eng_stop_elec.wav", VOL_NORM, ATTN_NORM );
				else
					EMIT_SOUND( edict(), CHAN_STATIC, "func_car/eng_stop.wav", VOL_NORM, ATTN_NORM );
				if( HasSpawnFlags( SF_CAR_GEARWHINE ) )
					STOP_SOUND( edict(), CHAN_VOICE, "func_car/gear_whine.wav" );
			}

			// FIXME - car stops immediately because otherwise there's a chance that player can still stuck in studiomodel collision...
			// example - when player is spawned in the front and car still goes forward and stops. and you are softlocked...
			CarSpeed = 0;

			if( pCarHurt )
				pCarHurt->pev->frags = 0;

			pev->owner = NULL;
			UTIL_FireTargets( pev->target, pPlayer, this, USE_TOGGLE, 0 );
			time = gpGlobals->time;
			pPlayer->pCar = NULL;
			hDriver = NULL;
			CameraBrakeOffsetX = 0;
			TurboAccum = 0;
			if( pDriverMdl )
				pDriverMdl->pev->effects |= EF_NODRAW;
			SetThink( &CCar::Idle );

			// clear tires' sound
		//	EMIT_SOUND( edict(), CHAN_ITEM, "common/null.wav", 1, 3.0 );
			EMIT_SOUND( pWheel1->edict(), CHAN_ITEM, "common/null.wav", 0, 3.0 );
			EMIT_SOUND( pWheel2->edict(), CHAN_ITEM, "common/null.wav", 0, 3.0 );
			EMIT_SOUND( pWheel3->edict(), CHAN_ITEM, "common/null.wav", 0, 3.0 );
			EMIT_SOUND( pWheel4->edict(), CHAN_ITEM, "common/null.wav", 0, 3.0 );
			// clear brake squeak
			if( HasSpawnFlags( SF_CAR_SQUEAKYBRAKES ))
				EMIT_SOUND( pWheel3->edict(), CHAN_BODY, "common/null.wav", 0, 3.0 );
			// clear particles (speed is stored here)
			pWheel1->pev->fuser1 = 0;
			pWheel2->pev->fuser1 = 0;
			pWheel3->pev->fuser1 = 0;
			pWheel4->pev->fuser1 = 0;
			// clear sound desync
			pWheel1->pev->fuser3 = 0;
			pWheel2->pev->fuser3 = 0;
			pWheel3->pev->fuser3 = 0;
			pWheel4->pev->fuser3 = 0;

			IsShifting = false;
			ShiftStartTime = 0;
			LastGear = -1;

			SetNextThink( 0 );
		}
	}
	else if( pPlayer->pCar == NULL || useType == USE_ON ) // player not in car, enter this car
	{
		if( pPlayer->m_pHoldableItem != NULL )
			return;

		if( fabs( CarSpeed ) > 300 )
			return;

		if( pev->deadflag == DEAD_DEAD )
			return;
		
		pPlayer->pCar = this;
		pPlayer->m_flFOV = CAR_FOV;

		if( StartSilent )
			StartSilent = false;

		if( hDriver == NULL )
		{
			hDriver = pPlayer;
			pev->owner = pPlayer->edict();
			if( pCarHurt )
				pCarHurt->pev->frags = 1;
			if( pDriverMdl )
			{
				// this works, but I don't need it
				/*
				const char *model = g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model" );
				
				if( FStrEq( model, "player" ) )
					SET_MODEL( pDriverMdl->edict(), "models/player.mdl" );
				else if( FStrEq( model, "robo" ) )
					SET_MODEL( pDriverMdl->edict(), "models/driver.mdl" );
				else
					SET_MODEL( pDriverMdl->edict(), "models/driver.mdl" );
				*/
				
				pDriverMdl->pev->effects &= ~EF_NODRAW;
			}

			if( HasSpawnFlags( SF_CAR_ELECTRIC ) )
			{
				EMIT_SOUND( edict(), CHAN_STATIC, "func_car/eng_start_elec.wav", VOL_NORM, ATTN_NORM );
				MESSAGE_BEGIN( MSG_ONE, gmsgTempEnt, NULL, hDriver->pev );
					WRITE_BYTE( TE_CARPARAMS );
					WRITE_BYTE( 0 );
				MESSAGE_END();
			}
			else
				EMIT_SOUND( edict(), CHAN_STATIC, "func_car/eng_start.wav", VOL_NORM, ATTN_NORM );

			if( m_iszIdleSnd )
				STOP_SOUND( edict(), CHAN_WEAPON, STRING( m_iszIdleSnd ) );

			SetThink( &CCar::Drive );
			UTIL_FireTargets( pev->target, pPlayer, this, USE_TOGGLE, 0 );
			time = gpGlobals->time;
			DriverMdlSequence = -1;
			CameraBrakeOffsetX = 0;
			TurboAccum = 0;
			if( HasSpawnFlags(SF_CAR_ELECTRIC))
				IsShifting = false;
			else
				IsShifting = true;
			Gear = 1;
			LastGear = -1;
			ShiftStartTime = gpGlobals->time - ShiftingTime;
			CameraAngles = GetAbsAngles(); // make sure camera is angled properly when we enter the vehicle
			NewCameraAngleY = CameraAngles.y;
			NewCameraAngleX = 0;
			Camera2RotationX = 0;
			Camera2RotationY = 0;
			LastPlayerAngles = hDriver->pev->v_angle;
			AccelAddX = 0;
			BrakeAddX = 0;
			EnteringShake = 2.0f;
			CameraModeAddDist_Free = 100.0f;
			CameraModeAddDist_Main = 50.0f;
			if( pExhaust1 )
			{
				pExhaust1->pev->iuser3 = -665;
				pExhaust1->pev->fuser1 = 0.1f;
				pExhaust1->pev->renderamt = 0;
			}
			if( pExhaust2 )
			{
				pExhaust2->pev->iuser3 = -665;
				pExhaust2->pev->fuser1 = 0.1f;
				pExhaust2->pev->renderamt = 0;
			}
			num_pops = 0.0f;
			poptime = 0.0f;

			SetNextThink( 0 );
		}
	}
	else
		ALERT( at_aiconsole, "CCar: Player is already in another car!\n" );
}

//================================================================
// sometimes car can stick to the floor and won't move...
//================================================================
void CCar::TryUnstick(void)
{
	Vector hackz = GetAbsOrigin();
	hackz.z += 1.0f;
	SetAbsOrigin( hackz );

	TraceResult tr;
	// trace 4 points...
	Vector a = hackz + Vector( -16, -16, 32 );
	UTIL_TraceLine( a, a - Vector(0,0,32), dont_ignore_monsters, dont_ignore_glass, edict(), &tr );
	if( tr.flFraction < 1.0f )
	{
		hackz.z += 1.0f;
		SetAbsOrigin( hackz );
	}
	else
	{
		Vector b = hackz + Vector( -16, 16, 32 );
		UTIL_TraceLine( b, b - Vector( 0, 0, 32 ), dont_ignore_monsters, dont_ignore_glass, edict(), &tr );
		if( tr.flFraction < 1.0f )
		{
			hackz.z += 1.0f;
			SetAbsOrigin( hackz );
		}
		else
		{
			Vector c = hackz + Vector( 16, -16, 32 );
			UTIL_TraceLine( c, c - Vector( 0, 0, 32 ), dont_ignore_monsters, dont_ignore_glass, edict(), &tr );
			if( tr.flFraction < 1.0f )
			{
				hackz.z += 1.0f;
				SetAbsOrigin( hackz );
			}
			else
			{
				Vector d = hackz + Vector( 16, 16, 32 );
				UTIL_TraceLine( d, d - Vector( 0, 0, 32 ), dont_ignore_monsters, dont_ignore_glass, edict(), &tr );
				if( tr.flFraction < 1.0f )
				{
					hackz.z += 1.0f;
					SetAbsOrigin( hackz );
				}
				else
				{
					// if we got all the way here, all checks are passed and we are safe
					pev->spawnflags &= ~SF_CAR_DOIDLEUNSTICK;
					pev->flags &= ~FL_ONGROUND;
				}
			}
		}
	}

	// reset chassis...
	if( 0 )//pChassis )
	{
		Vector chass = pChassis->GetAbsAngles();
		chass.x = chass.z = 0;
		pChassis->SetAbsAngles( chass );
	}
}

//==========================================
// collect wheels, headlights, etc.
//==========================================
void CCar::Setup( void )
{
	pChassis = UTIL_FindEntityByTargetname( NULL, STRING( chassis ) );
	if( pChassis )
		pChassis->SetParent( this );
	else
	{
		ALERT( at_error, "CAR WITH NO CHASSIS!\n" );
		UTIL_Remove( this );
		return;
	}
	
	pWheel1 = UTIL_FindEntityByTargetname( NULL, STRING( wheel1 ) );
	pWheel2 = UTIL_FindEntityByTargetname( NULL, STRING( wheel2 ) );
	pWheel3 = UTIL_FindEntityByTargetname( NULL, STRING( wheel3 ) );
	pWheel4 = UTIL_FindEntityByTargetname( NULL, STRING( wheel4 ) );
	pCarHurt = UTIL_FindEntityByTargetname( NULL, STRING( carhurt ) );
	pDriverMdl = (CBaseAnimating*)UTIL_FindEntityByTargetname( NULL, STRING( drivermdl ) );
	pChassisMdl = (CBaseAnimating *)UTIL_FindEntityByTargetname( NULL, STRING( chassismdl ) );
	pCamera1 = CBaseEntity::Create( "info_target", GetAbsOrigin(), GetAbsAngles(), edict() );
	pCamera2 = UTIL_FindEntityByTargetname( NULL, STRING( camera2 ) );
	pTankTower = UTIL_FindEntityByTargetname( NULL, STRING( tank_tower ) );
	pDoorHandle1 = UTIL_FindEntityByTargetname( NULL, STRING( door_handle ) );
	pDoorHandle2 = UTIL_FindEntityByTargetname( NULL, STRING( door_handle2 ) );
	pExhaust1 = UTIL_FindEntityByTargetname( NULL, STRING( exhaust1 ) );
	pExhaust2 = UTIL_FindEntityByTargetname( NULL, STRING( exhaust2 ) );

	if( !pWheel1 || !pWheel2 || !pWheel3 || !pWheel4 )
	{
		ALERT( at_error, "CAR WITH NO WHEELS! Must be 4 wheels present.\n" );
		UTIL_Remove( this );
		return;
	}

	if( pChassisMdl )
	{
		pChassisMdl->pev->owner = edict();
		UTIL_SetSize( pChassisMdl->pev, Vector( -16, -16, 0 ), Vector( 16, 16, 16 ) );
		pChassisMdl->pev->takedamage = DAMAGE_YES;
		pChassisMdl->pev->spawnflags |= BIT( 31 ); // SF_ENVMODEL_OWNERDAMAGE
		pChassisMdl->RelinkEntity( FALSE );
	}
	else
		ALERT( at_warning, "func_car \"%s\" doesn't have body model entity specified, collision won't work properly.\n", GetTargetname() );

	if( pWheel1 )
	{
		pWheel1->SetParent( (CBaseEntity*)pChassis );
		pWheel1->pev->iuser2 = FrontWheelRadius;
		pWheel1->pev->owner = edict();
	}
	if( pWheel2 )
	{
		pWheel2->SetParent( (CBaseEntity *)pChassis );
		pWheel2->pev->iuser2 = FrontWheelRadius;
		pWheel2->pev->owner = edict();
	}
	if( pWheel3 )
	{
		pWheel3->SetParent( (CBaseEntity *)pChassis );
		pWheel3->pev->iuser2 = RearWheelRadius;
		pWheel3->pev->owner = edict();
	}
	if( pWheel4 )
	{
		pWheel4->SetParent( (CBaseEntity *)pChassis );
		pWheel4->pev->iuser2 = RearWheelRadius;
		pWheel4->pev->owner = edict();
	}

	if( CameraDistance <= 0 )
	{
		if( pChassisMdl )
		{
			// get camera distance according to model bounds
			Vector mins = g_vecZero;
			Vector maxs = g_vecZero;
			UTIL_GetModelBounds( pChassisMdl->pev->modelindex, mins, maxs );
			CameraDistance = (int)((mins - maxs).Length() * pChassisMdl->pev->scale);
		}
		else
			CameraDistance = 230;
	}

	if( pCamera1 )
	{
		pCamera1->SetNullModel();
		pCamera1->pev->effects |= EF_SKIPPVS;
		if( !CameraHeight )
		{
			if( pChassisMdl )
			{
				// get camera distance according to model bounds
				Vector mins = g_vecZero;
				Vector maxs = g_vecZero;
				UTIL_GetModelBounds( pChassisMdl->pev->modelindex, mins, maxs );
				CameraHeight = (int)(fabs( mins.z - maxs.z ) * pChassisMdl->pev->scale);
			}
			else
				CameraHeight = 80;
		}
		pCamera1->pev->iuser3 = -673;
	}

	if( pCamera2 )
	{
		if( !pCamera2->m_iParent )
			pCamera2->SetParent( this );
		pCamera2->SetNullModel();
		Camera2LocalOrigin = pCamera2->GetLocalOrigin();
		Camera2LocalAngles = pCamera2->GetLocalAngles();
	}

	if( pCarHurt )
	{
		pCarHurt->SetParent( this );
		pCarHurt->pev->owner = edict();
		pCarHurt->pev->frags = 0;
	}

	if( pDriverMdl )
	{
		if( pChassis )
			pDriverMdl->SetParent( (CBaseEntity *)pChassis );
		else
			pDriverMdl->SetParent( this );
		pDriverMdl->pev->effects |= EF_NODRAW;
	}

	if( pTankTower )
	{
		if( pChassisMdl )
			pTankTower->SetParent( (CBaseEntity *)pChassisMdl );
		else if( pChassis )
			pTankTower->SetParent( (CBaseEntity *)pChassis );
		else
			pTankTower->SetParent( this );
	}

	pFreeCam = CBaseEntity::Create( "info_target", GetAbsOrigin(), GetAbsAngles(), edict() );
	if( pFreeCam )
	{
		pFreeCam->SetNullModel();
		pFreeCam->SetParent( this );
		if( FreeCameraDistance <= 0 )
		{
			if( pChassisMdl )
			{
				// get camera distance according to model bounds
				Vector mins = g_vecZero;
				Vector maxs = g_vecZero;
				UTIL_GetModelBounds( pChassisMdl->pev->modelindex, mins, maxs );
				FreeCameraDistance = (int)((mins - maxs).Length() * 0.75f * pChassisMdl->pev->scale);
			}
			else
				FreeCameraDistance = CameraDistance;
		}
		pFreeCam->pev->effects |= EF_SKIPPVS;
		pFreeCam->pev->iuser3 = -673;
	}

	if( pDoorHandle1 )
	{
		// reset USE flag, car can be USE-pressed through handle only
		ObjectCaps();
		if( pChassisMdl )
			pDoorHandle1->SetParent( (CBaseEntity *)pChassisMdl );
		else
			pDoorHandle1->SetParent( this );

		pDoorHandle1->pev->owner = edict();

		if( pDoorHandle2 )
		{
			if( pChassisMdl )
				pDoorHandle2->SetParent( (CBaseEntity *)pChassisMdl );
			else
				pDoorHandle2->SetParent( this );

			pDoorHandle2->pev->owner = edict();
		}
	}

	num_exhausts = 0;
	num_pops = 0.0f;
	if( pExhaust1 )
	{
		SET_MODEL( pExhaust1->edict(), "sprites/muzzleflash2.spr" );
		pExhaust1->pev->rendermode = kRenderTransAdd;
		pExhaust1->pev->scale = 0.1f;
		pExhaust1->pev->framerate = 10;
		pExhaust1->pev->iuser1 = 5; // parallel oriented sprite
		if( pChassisMdl )
			pExhaust1->SetParent( (CBaseEntity *)pChassisMdl );
		else
			pExhaust1->SetParent( this );
		num_exhausts++;

		if( pExhaust2 )
		{
			SET_MODEL( pExhaust2->edict(), "sprites/muzzleflash2.spr" );
			pExhaust2->pev->rendermode = kRenderTransAdd;
			pExhaust2->pev->scale = 0.1f;
			pExhaust2->pev->framerate = 10;
			pExhaust2->pev->iuser1 = 5; // parallel oriented sprite
			if( pChassisMdl )
				pExhaust2->SetParent( (CBaseEntity *)pChassisMdl );
			else
				pExhaust2->SetParent( this );
			num_exhausts++;
		}
	}

	if( pev->iuser1 ) // rotate car
	{
		Vector ang = GetAbsAngles();
		ang.y += pev->iuser1;
		SetAbsAngles( ang );
	}

	if( pTankTower )
	{
		Vector TowerAngles = pChassis->GetAbsAngles();

		if( pChassisMdl )
		{
			TowerAngles = pChassisMdl->GetLocalAngles();
			TowerAngles.y += TankTowerRotationOffset;
			pTankTower->SetLocalAngles( TowerAngles );
		}
		else
			pTankTower->SetAbsAngles( TowerAngles );
	}

	pev->flags &= ~FL_ONGROUND; // !!! this resets position so the vehicle won't hang in air...

	// clear sound desync
	pWheel1->pev->fuser3 = 0;
	pWheel2->pev->fuser3 = 0;
	pWheel3->pev->fuser3 = 0;
	pWheel4->pev->fuser3 = 0;

	SetThink( &CCar::Idle );
	SetNextThink( RANDOM_FLOAT( 0.2f, 0.5f ) );
}

void CCar::GetCollision( const float AbsCarSpeed, const int Forward, Vector *Collision, float *ColDotProduct, Vector *ColPoint )
{
	TraceResult trCol;

	*Collision = g_vecZero;
	*ColPoint = g_vecZero; // place for sparks
	*ColDotProduct = 1.0f;

	Vector ColPointLeft = g_vecZero;
	Vector ColPointRight = g_vecZero;

	// make chassis vectors
	UTIL_MakeVectors( pChassis->GetAbsAngles() );
	const Vector ChassisForw = gpGlobals->v_forward;
	const Vector ChassisUp = gpGlobals->v_up;
	const Vector ChassisRight = gpGlobals->v_right;

	// bring back the car vectors
	UTIL_MakeVectors( GetAbsAngles() );

	bool hit_carblocker = false;

	if( Forward == 1 )
	{
		// front right wheel
		UTIL_TraceLine( GetAbsOrigin() + ChassisUp * FrontWheelRadius * 1.5 + ChassisRight * FrontSuspWidth * 0.5, GetAbsOrigin() + ChassisUp * FrontWheelRadius * 1.5 + ChassisRight * FrontSuspWidth * 0.5 + ChassisForw * (FrontSuspDist + FrontBumperLength), ignore_monsters, edict(), &trCol );
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_CARBLOCKER )
			hit_carblocker = true;
		if( trCol.flFraction < 1.0f )
		{
			*ColDotProduct = -DotProduct( ChassisForw, trCol.vecPlaneNormal ) * Forward;
			*Collision += trCol.vecPlaneNormal * AbsCarSpeed * (*ColDotProduct); // slide along the plane
			*ColPoint = trCol.vecEndPos;
			if( *ColDotProduct < 0.6 )
				*ColDotProduct *= -1;
		}
		else if( hit_carblocker )
		{
			*Collision += -gpGlobals->v_forward * AbsCarSpeed * Forward;
			*ColPoint = trCol.vecEndPos;
		}
		hit_carblocker = false;

		// front left wheel
		UTIL_TraceLine( GetAbsOrigin() + ChassisUp * FrontWheelRadius * 1.5 - ChassisRight * FrontSuspWidth * 0.5, GetAbsOrigin() + ChassisUp * FrontWheelRadius * 1.5 - ChassisRight * FrontSuspWidth * 0.5 + ChassisForw * (FrontSuspDist + FrontBumperLength), ignore_monsters, edict(), &trCol );
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_CARBLOCKER )
			hit_carblocker = true;
		if( trCol.flFraction < 1.0f )
		{
			*ColDotProduct = -DotProduct( ChassisForw, trCol.vecPlaneNormal ) * Forward;
			*Collision += trCol.vecPlaneNormal * AbsCarSpeed * (*ColDotProduct);
			*ColPoint = trCol.vecEndPos;
			if( *ColDotProduct < 0.6 )
				*ColDotProduct *= -1;
		}
		else if( hit_carblocker )
		{
			*Collision += -gpGlobals->v_forward * AbsCarSpeed * Forward;
			*ColPoint = trCol.vecEndPos;
		}
		hit_carblocker = false;

		// front center
		UTIL_TraceLine( GetAbsOrigin() + ChassisUp * FrontWheelRadius * 1.5, GetAbsOrigin() + ChassisUp * FrontWheelRadius * 1.5 + ChassisForw * (FrontSuspDist + FrontBumperLength), ignore_monsters, edict(), &trCol );
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_CARBLOCKER )
			hit_carblocker = true;
		if( trCol.flFraction < 1.0f )
		{
			*ColDotProduct = -DotProduct( ChassisForw, trCol.vecPlaneNormal ) * Forward;
			*Collision += trCol.vecPlaneNormal * AbsCarSpeed * (*ColDotProduct);
			*ColPoint = trCol.vecEndPos;
			if( *ColDotProduct < 0.6 )
				*ColDotProduct *= -1;
		}
		else if( hit_carblocker )
		{
			*Collision += -gpGlobals->v_forward * AbsCarSpeed * Forward;
			*ColPoint = trCol.vecEndPos;
		}
		hit_carblocker = false;
	}

	if( Forward == -1 )
	{
		// rear right wheel
		UTIL_TraceLine( GetAbsOrigin() + ChassisUp * RearWheelRadius * 1.5 + ChassisRight * RearSuspWidth * 0.5, GetAbsOrigin() + ChassisUp * RearWheelRadius * 1.5 + ChassisRight * RearSuspWidth * 0.5 - ChassisForw * (RearSuspDist + RearBumperLength), ignore_monsters, edict(), &trCol );
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_CARBLOCKER )
			hit_carblocker = true;
		if( trCol.flFraction < 1.0f )
		{
			*ColDotProduct = -DotProduct( ChassisForw, trCol.vecPlaneNormal ) * Forward;
			*Collision += trCol.vecPlaneNormal * AbsCarSpeed * (*ColDotProduct);
			*ColPoint = trCol.vecEndPos;
		}
		else if( hit_carblocker )
		{
			*Collision += -gpGlobals->v_forward * AbsCarSpeed * Forward;
			*ColPoint = trCol.vecEndPos;
		}
		hit_carblocker = false;

		// rear left wheel
		UTIL_TraceLine( GetAbsOrigin() + ChassisUp * RearWheelRadius * 1.5 - ChassisRight * RearSuspWidth * 0.5, GetAbsOrigin() + ChassisUp * RearWheelRadius * 1.5 - ChassisRight * RearSuspWidth * 0.5 - ChassisForw * (RearSuspDist + RearBumperLength), ignore_monsters, edict(), &trCol );
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_CARBLOCKER )
			hit_carblocker = true;
		if( trCol.flFraction < 1.0f )
		{
			*ColDotProduct = -DotProduct( ChassisForw, trCol.vecPlaneNormal ) * Forward;
			*Collision += trCol.vecPlaneNormal * AbsCarSpeed * (*ColDotProduct);
			*ColPoint = trCol.vecEndPos;
		}
		else if( hit_carblocker )
		{
			*Collision += -gpGlobals->v_forward * AbsCarSpeed * Forward;
			*ColPoint = trCol.vecEndPos;
		}
		hit_carblocker = false;

		// rear center
		UTIL_TraceLine( GetAbsOrigin() + ChassisUp * RearWheelRadius * 1.5, GetAbsOrigin() + ChassisUp * RearWheelRadius * 1.5 - ChassisForw * (RearSuspDist + RearBumperLength), ignore_monsters, edict(), &trCol );
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_CARBLOCKER )
			hit_carblocker = true;
		if( trCol.flFraction < 1.0f )
		{
			*ColDotProduct = -DotProduct( ChassisForw, trCol.vecPlaneNormal ) * Forward;
			*Collision += trCol.vecPlaneNormal * AbsCarSpeed * (*ColDotProduct);
			*ColPoint = trCol.vecEndPos;
		}
		else if( hit_carblocker )
		{
			*Collision += -gpGlobals->v_forward * AbsCarSpeed * Forward;
			*ColPoint = trCol.vecEndPos;
		}
		hit_carblocker = false;
	}

	// -------- sides! --------
	UTIL_MakeVectors( pChassis->GetAbsAngles() );

	// for additional checks for tail and front
	Vector CheckAdd = g_vecZero;
	const float width = (FrontSuspWidth + RearSuspWidth) * 0.3f;
	const Vector vWidth = gpGlobals->v_right * width;
	const Vector vSideStart = GetAbsOrigin() + gpGlobals->v_up * ((FrontWheelRadius + RearWheelRadius) * 0.5f) * 2.0f;

	// left
	UTIL_TraceLine( vSideStart, vSideStart - vWidth, ignore_monsters, edict(), &trCol );
	if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_CARBLOCKER )
		hit_carblocker = true;
	if( trCol.flFraction < 1.0f || hit_carblocker )
		ColPointLeft = gpGlobals->v_right * 100;
	hit_carblocker = false;

	// left tail and front
	if( Forward != 0 )
	{
		// check tail
		CheckAdd = -gpGlobals->v_forward * RearSuspDist;
		UTIL_TraceLine( vSideStart + CheckAdd, vSideStart + CheckAdd - vWidth, ignore_monsters, edict(), &trCol );
	//	UTIL_Sparks( trCol.vecEndPos );
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_CARBLOCKER )
			hit_carblocker = true;
		if( trCol.flFraction < 1.0f || hit_carblocker )
			ColPointLeft = gpGlobals->v_right * 100;
		hit_carblocker = false;
		// check front
		CheckAdd = gpGlobals->v_forward * FrontSuspDist;
		UTIL_TraceLine( vSideStart + CheckAdd, vSideStart + CheckAdd - vWidth, ignore_monsters, edict(), &trCol );
	//	UTIL_Sparks( trCol.vecEndPos );
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_CARBLOCKER )
			hit_carblocker = true;
		if( trCol.flFraction < 1.0f || hit_carblocker )
			ColPointLeft = gpGlobals->v_right * 100;
		hit_carblocker = false;
	}

	// right
	UTIL_TraceLine( vSideStart, vSideStart + vWidth, ignore_monsters, edict(), &trCol );
	if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_CARBLOCKER )
		hit_carblocker = true;
	if( trCol.flFraction < 1.0f || hit_carblocker )
		ColPointRight = -gpGlobals->v_right * 100;
	hit_carblocker = false;

	// right tail and front
	if( Forward != 0 )
	{
		// check tail
		CheckAdd = -gpGlobals->v_forward * RearSuspDist;
		UTIL_TraceLine( vSideStart + CheckAdd, vSideStart + CheckAdd + vWidth, ignore_monsters, edict(), &trCol );
	//	UTIL_Sparks( trCol.vecEndPos );
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_CARBLOCKER )
			hit_carblocker = true;
		if( trCol.flFraction < 1.0f || hit_carblocker )
			ColPointRight = -gpGlobals->v_right * 100;
		hit_carblocker = false;
		// check front
		CheckAdd = gpGlobals->v_forward * FrontSuspDist;
		UTIL_TraceLine( vSideStart + CheckAdd, vSideStart + CheckAdd + vWidth, ignore_monsters, edict(), &trCol );
	//	UTIL_Sparks( trCol.vecEndPos );
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_CARBLOCKER )
			hit_carblocker = true;
		if( trCol.flFraction < 1.0f || hit_carblocker )
			ColPointRight = -gpGlobals->v_right * 100;
		hit_carblocker = false;
	}

	Vector SideCollision = ColPointLeft + ColPointRight;

	*Collision += SideCollision;

	// bring back the car vectors
	UTIL_MakeVectors( GetAbsAngles() );
}

void CCar::SetupTireSoundAtSurface( int wheelnum, float AbsCarSpeed, float *ChassisShake, float *surf_Mult )
{
	TraceResult TexTr;
	edict_t *pWorld = INDEXENT( 0 );
	char szBuffer[64];
	char chTextureType = 0;
	int soundtype = 0;
	int handbrakesoundtype = 8; // default asphalt brake sound
	bool Particle = false;
	bool AddDirt = false;
	int Type = 2; // 0 asphalt, 1 sand, 2 dirt, 3 watersplash
	CBaseEntity *pWheelX = pWheel1;
	switch( wheelnum )
	{
	case 2: pWheelX = pWheel2; break;
	case 3: pWheelX = pWheel3; break;
	case 4: pWheelX = pWheel4; break;
	}
	
	// trace down from the wheel origin
	Vector Tex_vecStart = pWheelX->GetAbsOrigin();
	Vector Tex_vecEnd = pWheelX->GetAbsOrigin() - Vector( 0, 0, pWheelX->pev->iuser2 + 5 );
	UTIL_TraceLine( Tex_vecStart, Tex_vecEnd, ignore_monsters, dont_ignore_glass, pWheelX->edict(), &TexTr );
	if( TexTr.pHit )
		pWorld = TexTr.pHit;
	if( pWorld->v.solid == SOLID_BSP )
	{
		const char *pTextureName = TRACE_TEXTURE( pWorld, Tex_vecStart, Tex_vecEnd );
		if( pTextureName )
		{
			Q_strncpy( szBuffer, pTextureName, sizeof( szBuffer ) );
			chTextureType = TEXTURETYPE_Find( szBuffer );
			switch( chTextureType )
			{
			case CHAR_TEX_GRASS:
				*ChassisShake = 2.5f;
				soundtype = 2;
				handbrakesoundtype = 7;
				*surf_Mult = surf_GrassMult;
				Particle = true;
				Type = 2;
				AddDirt = true;
				break;
			case CHAR_TEX_GRAVEL:
				*ChassisShake = 5.0f;
				soundtype = 3;
				handbrakesoundtype = 7;
				*surf_Mult = surf_GravelMult;
				Particle = true;
				AddDirt = true;
				Type = 1;
				break;
			case CHAR_TEX_SNOW:
				*ChassisShake = 2.0f;
				soundtype = 4;
				handbrakesoundtype = 7;
				*surf_Mult = surf_SnowMult;
				// UNDONE PARTICLES
				// UNDONE snow "dirt"?
				break;
			case CHAR_TEX_DIRT:
				*ChassisShake = 4.0f;
				soundtype = 1;
				handbrakesoundtype = 7;
				*surf_Mult = surf_DirtMult;
				Particle = true;
				Type = 1;
				AddDirt = true;
				break;
			case CHAR_TEX_WOOD:
			case CHAR_TEX_WOODSTEP:
				*ChassisShake = 1.0f;
				soundtype = 5;
				handbrakesoundtype = 7;
				*surf_Mult = surf_WoodMult;
				break;
			}
		}
	}
	else
	{
		soundtype = 6;
	}

	// override the particle if in water
//	if( pev->waterlevel > 0 )
//	{
//		Particle = true;
//		Type = 3;
//	}

	float tirevolume = AbsCarSpeed * 0.001f * 0.5f;
	int tirepitch = 60 + ((AbsCarSpeed - ((2 - wheelnum) * 1000.0f)) * 0.03f); // move the pitch slightly, using wheelnum :)

	if( (HeatingTires || HeatingMult > 1.0f ) && wheelnum > 2 )
	{
		tirepitch = 60 + 10 * HeatingMult;
		tirevolume = HeatingMult * 0.05f;
		soundtype = handbrakesoundtype;
		*ChassisShake = 0.5f * HeatingMult;
		if( (pev->flags & FL_ONGROUND) && !Particle ) // assume all wheels on ground, and no particle was set, so it's asphalt...
		{
			Particle = true;
			Type = 0;
		}
	}
	else if( bUp() ) // handbrake
	{
		tirepitch += 130 + (2 - wheelnum);
		soundtype = handbrakesoundtype;
		if( (pev->flags & FL_ONGROUND) && !Particle ) // assume all wheels on ground, and no particle was set, so it's asphalt...
		{
			Particle = true;
			Type = 0;
		}
	}

	if( tirepitch > 250 )
		tirepitch = 250;
	if( tirevolume > 0.5f )
		tirevolume = 0.5f;
	
	if( tirevolume > 0.01f && (TexTr.flFraction < 1.0f) )
	{
		// desync using time
		if( gpGlobals->time >= pWheelX->pev->fuser3 )
		{
			EMIT_SOUND_DYN( pWheelX->edict(), CHAN_ITEM, pTireSounds[soundtype], tirevolume, ATTN_NORM, SND_CHANGE_VOL | SND_CHANGE_PITCH, tirepitch );
			pWheelX->pev->fuser3 = gpGlobals->time + RANDOM_FLOAT( 0.025f, 0.1f );
		}
	}
	else
	{
		EMIT_SOUND( pWheelX->edict(), CHAN_ITEM, "common/null.wav", 0, 0 ); // this works...

	//	STOP_SOUND( pWheelX->edict(), CHAN_ITEM, pTireSounds[soundtype] );
	
	//	EMIT_SOUND( pWheelX->edict(), CHAN_ITEM, "common/null.wav", 0.2, 3.0 );
	//	STOP_SOUND( pWheelX->edict(), CHAN_ITEM, "common/null.wav" );
	}

	if( Particle && pev->waterlevel == 0 ) // TODO allow water particles
		pWheelX->pev->iuser3 = -668;
	else
		pWheelX->pev->iuser3 = 0;

	if( HeatingTires && wheelnum > 2 )
		pWheelX->pev->fuser1 = 100 * HeatingMult;
	else
		pWheelX->pev->fuser1 = AbsCarSpeed;

	pWheelX->pev->iuser1 = Type;

	// add some dirt on the wheels
	if( !HasSpawnFlags( SF_CAR_CANTBEDIRTY ) )
	{
		if( pev->waterlevel != 0 )
			AddDirt = false; // wash the car!

		if( pev->waterlevel != 0 || HeatingTires || AbsCarSpeed > ADDDIRT_SPEED_THRESHOLD )
		{
			float targetdirt = (AddDirt ? 1.0f : 0.0f);
			float adddirt_speed = 0.0f;

			if( pev->waterlevel != 0 )
				adddirt_speed = gpGlobals->frametime * 0.5f;
			else if( HeatingTires && wheelnum > 2 )
				adddirt_speed = gpGlobals->frametime * HeatingMult * 0.05f;
			else
				adddirt_speed = 0.05f * gpGlobals->frametime * AbsCarSpeed * 0.001f;

			pWheelX->pev->fuser2 = UTIL_Approach( targetdirt, pWheelX->pev->fuser2, adddirt_speed );
			if( pWheelX->m_hChild ) // the actual wheel can be an invisible brush, and a wheel model was attached instead
				pWheelX->m_hChild->pev->fuser2 = pWheelX->pev->fuser2;
		}
	}
}

void CCar::Drive( void )
{
	if( hDriver == NULL )
	{
		SetThink( &CCar::Idle );
		SetNextThink( 0 );
		return;
	}

	// reset all velocity from previous frame
	pev->velocity = g_vecZero;

	pev->friction = 0.0f; // always set this, otherwise issues on different framerates

	pev->spawnflags |= SF_CAR_DOIDLEUNSTICK; // do unsticking procedure in Idle

	const float AbsCarSpeed = fabs( CarSpeed );

	// CAR STUCK???
	if( StuckTime > gpGlobals->time )
		StuckTime = gpGlobals->time;
	if( (hDriver->pev->button & IN_RELOAD) && (gpGlobals->time > StuckTime + 5) )
	{
		ALERT( at_aiconsole, "CCar::TryUnstick by user\n" );
		TryUnstick();

		if( !SafeCarPos.IsNull() )
		{
			SetAbsOrigin( SafeCarPos );
			CarSpeed = 0;
		}

		StuckTime = gpGlobals->time;
	}

	// will need those later!
	UTIL_MakeVectors( pChassis->GetAbsAngles() );
	const Vector ChassisForw = gpGlobals->v_forward;
	const Vector ChassisUp = gpGlobals->v_up;
	const Vector ChassisRight = gpGlobals->v_right;
	Vector DriftForw = g_vecZero;

	if( DriftMode )
	{
		const float ChassisAbsAngY = pChassis->pev->angles.y;
		const float anglediff = AngleDiff( ChassisAbsAngY, DriftAngles.y );

		// should depend on a surface/tires used etc.
		float drift_recovery_speed = 5000.0f / (1.0f + AbsCarSpeed); // stability
		UTIL_MakeVectors( DriftAngles );
		DriftForw = gpGlobals->v_forward;
		float dot = DotProduct( DriftForw, ChassisForw );
		DriftAmount = bound( 0.05, dot, 1 );
		DriftAmount *= DriftAmount * DriftAmount;
		drift_recovery_speed = bound( 30.0f, drift_recovery_speed, 5000.0f );
		drift_recovery_speed /= DriftAmount;
		if( !bLeft() && !bRight() )
			drift_recovery_speed *= 2.0f; // should depend on a surface/tires used etc.
		if( !bForward() )
			drift_recovery_speed *= 2.0f;
		if( AbsCarSpeed <= 0.1f )
			DriftAngles.y = ChassisAbsAngY;
		else
			DriftAngles.y = UTIL_ApproachAngle( ChassisAbsAngY, DriftAngles.y, drift_recovery_speed * gpGlobals->frametime );

		ALERT( at_console, "recovery %f, amt %f, dot %f\n", drift_recovery_speed, DriftAmount, dot );
	}
	else
		DriftAmount = 1.0f;
	
	// only NOW make the car's vectors
	UTIL_MakeVectors( GetAbsAngles() );

	// TEMP !!! (or not.)
	const Vector PlayerPos = GetAbsOrigin() + gpGlobals->v_up * 40;
//	if( pDriverMdl )
//		PlayerPos = pDriverMdl->GetAbsOrigin();
	hDriver->SetAbsOrigin( PlayerPos );
	hDriver->SetAbsVelocity( g_vecZero );

	// motion blur
	hDriver->pev->vuser1.y = AbsCarSpeed;
	
	// achievement
	float Distance = 0.0f;
	Distance = (GetAbsOrigin() - PrevOrigin).Length();
	Distance = fUnitsToMeters( Distance ); // convert units to meters
	PrevOrigin = GetAbsOrigin();
	if( hDriver->pev->flags & FL_CLIENT )
	{
		CBasePlayer *plr = (CBasePlayer*)UTIL_PlayerByIndex( hDriver->entindex() );
		plr->AchievementStats[ACH_CARDISTANCE] += Distance;
	}
	
	//----------------------------
	// wheels' origin (aka "suspension"...)
	// 1-2 front wheels, 3-4 rear wheels
	//----------------------------
	int FRW_InAir;
	int FLW_InAir;
	int RRW_InAir;
	int RLW_InAir;
	Wheels( &FRW_InAir, &FLW_InAir, &RRW_InAir, &RLW_InAir );
	const int FrontWheelsInAir = FRW_InAir + FLW_InAir; // can't turn if 2
	const int RearWheelsInAir = RRW_InAir + RLW_InAir; // can't accelerate if 2 // TODO: 4x4? Front drive?

	//----------------------------
	// turn
	//----------------------------
	// Forward turning controls if going backwards
	int Forward = 1;
	if( HeatingTires )
	{
		// still 1
	}
	else if( ((bBack()) && (CarSpeed < 0.0f)) || (CarSpeed < 0.0f) )
		Forward = -1;
	else if( CarSpeed == 0.0f )
		Forward = 0;

	float TurnRate = TurningRate / (1 + AbsCarSpeed);
	TurnRate = (Forward == 0 ? 1 : Forward) * bound( 0, TurnRate, 250 );
	int CameraSwayRate = 0;
	if( SecondaryCamera )
		CameraSwayRate = MaxCamera2Sway;

	if( !TurningOverride )
	{
		if( bRight() )
		{
			Turning -= TurnRate * gpGlobals->frametime;
			if( CameraSwayRate > 0 )
				CameraMoving -= (CameraMoving > 0 ? 2 : 1) * CameraSwayRate * gpGlobals->frametime;
		}
		else if( bLeft() )
		{
			Turning += TurnRate * gpGlobals->frametime;
			if( CameraSwayRate > 0 )
				CameraMoving += (CameraMoving < 0 ? 2 : 1) * CameraSwayRate * gpGlobals->frametime;
		}

		// no turning buttons pressed, go to zero slowly
		if( (!bLeft() && !bRight()) )
		{
			Turning = UTIL_Approach( 0, Turning, fabs( TurnRate * 0.75 ) * gpGlobals->frametime );
			if( CameraSwayRate > 0 )
				CameraMoving = UTIL_Approach( 0, CameraMoving, CameraSwayRate * 0.5 * gpGlobals->frametime );

			if( fabs( Turning ) <= 0.001f )
				Turning = 0;
		}
	}

	const float empirical = AbsCarSpeed * 0.00625f;
	float max_turn_val = 1.0f / (0.1f + empirical); // magic
	if( max_turn_val > 1.0f )
		max_turn_val = 1.0f;
	Turning = bound( -max_turn_val, Turning, max_turn_val );
	const float AbsTurning = fabs( Turning );

	int CameraMovingBound = 0;
	if( SecondaryCamera )
		CameraMovingBound = MaxCamera2Sway;

	CameraMoving = bound( -CameraMovingBound, CameraMoving, CameraMovingBound );

	float CarTurnRate = (empirical * 0.1) + (0.005f * HeatingMult);
//	if( 0 )//DriftMode && AbsCarSpeed > 10 )
//	{
//		CarTurnRate *= DriftAmount;
//		if( bForward() )
//			CarTurnRate *= 0.75f * DriftAmount;
//	}
	CarTurnRate = bound( 0, CarTurnRate, 1 );
	
	// handbrake pressed
	if( bUp() )
	{
		if( DriftMode )
		{
			const float HandbrakeTurnRateMult = bound( 1.0f, AbsCarSpeed * 0.002, 1.5f ) + AbsTurning;
			DriftAngles.y -= (10 * MaxTurn) * (Turning * CarTurnRate * HandbrakeTurnRateMult) * gpGlobals->frametime;
		}
		else
			CarTurnRate *= bound( 1.0f, AbsCarSpeed * 0.002, 1.5f ) + AbsTurning;
	}

	Vector CarAng = GetLocalAngles();
	
	// YAW - turn left and right
	float trn = (10 * MaxTurn) * (Turning * CarTurnRate) * gpGlobals->frametime;
	if( FrontWheelsInAir == 2 )
		trn = 0;
	CarAng.y += trn;

	//----------------------------
	// wheels' turning (visual effect)
	//----------------------------
	// get one front wheel which rotates and turn, the second will repeat
	Vector FrontWheelAng = pWheel1->GetLocalAngles();
	// get one rear wheel which only rotates, the second will repeat
	Vector RearWheelAng = pWheel3->GetLocalAngles();

	const float ApproachSpeed = bound( TurningOverride ? 0 : 200, AbsCarSpeed, 400 );
	if( Forward == 0 )
	{
		if( bLeft() )
			FrontWheelAng.y = UTIL_Approach( MaxTurn, FrontWheelAng.y, ApproachSpeed * gpGlobals->frametime );
		else if( bRight() )
			FrontWheelAng.y = UTIL_Approach( -MaxTurn, FrontWheelAng.y, ApproachSpeed * gpGlobals->frametime );
		else
			FrontWheelAng.y = UTIL_Approach( 0, FrontWheelAng.y, ApproachSpeed * gpGlobals->frametime );
	}
	else
		FrontWheelAng.y = UTIL_Approach( Forward * Turning * MaxTurn, FrontWheelAng.y, ApproachSpeed * gpGlobals->frametime );

	FrontWheelAng.y = bound( -MaxTurn, FrontWheelAng.y, MaxTurn );

	// rear wheels can turn too if specified
	if( HasSpawnFlags(SF_CAR_TURNREARWHEELS) )
		RearWheelAng.y = -FrontWheelAng.y * 0.25f;

	//----------------------------
	// wheels' rotation (visual effect)
	//----------------------------
	if( HeatingTires )
	{
		HeatingMult = UTIL_Approach( 7.0f, HeatingMult, 2.5f * gpGlobals->frametime );
		RearWheelAng.x += 100.0f * HeatingMult * (M_PI * 0.5) * gpGlobals->frametime; // 0.5 ??? so...pi/2?
	}
	else if( !(bUp()) )
	{
		// I have no idea how and why this works. It just does.
		FrontWheelAng.x += Forward * AbsCarSpeed * (M_PI * 0.5) * gpGlobals->frametime / (FrontWheelRadius * 0.05f);
		RearWheelAng.x += Forward * AbsCarSpeed * (M_PI * 0.5) * gpGlobals->frametime * (1 + HeatingMult) * (1.0f / (bForward() ? DriftAmount : 1.0f)) / (RearWheelRadius * 0.05f);
	}

	if( FrontWheelAng.x > 359.9f || FrontWheelAng.x < -359.9f )
		FrontWheelAng.x = 0.0f;

	pWheel1->SetLocalAngles( FrontWheelAng );
	pWheel2->SetLocalAngles( FrontWheelAng );
	pWheel3->SetLocalAngles( RearWheelAng );
	pWheel4->SetLocalAngles( RearWheelAng );

	//----------------------------
	// texture sound (tires) and chassis shaking (from dirt etc.)
	//----------------------------
	float ChassisShake = 0.0f;
	float ChassisShake1 = 0.0f;
	float ChassisShake2 = 0.0f;
	float ChassisShake3 = 0.0f;
	float ChassisShake4 = 0.0f;
	float surf_CurrentMult = 1.0f;
	float surf_Mult1 = 1.0f;
	float surf_Mult2 = 1.0f;
	float surf_Mult3 = 1.0f;
	float surf_Mult4 = 1.0f;

	for( int setupwheel = 1; setupwheel <= 4; setupwheel++ )
		SetupTireSoundAtSurface( setupwheel, AbsCarSpeed, &ChassisShake1, &surf_Mult1 );

	if( !HasSpawnFlags(SF_CAR_CANTBEDIRTY) )
	{
		if( pChassisMdl && AbsCarSpeed > ADDDIRT_SPEED_THRESHOLD )
		{
			// add some dirt on the car based on the amount of dirt on wheels
			// ! only if we are driving with significant speed
			pChassisMdl->pev->fuser2 = (pWheel1->pev->fuser2 + pWheel2->pev->fuser2 + pWheel3->pev->fuser2 + pWheel4->pev->fuser2) * 0.25f;
		}
	}

	ChassisShake = (ChassisShake1 + ChassisShake2 + ChassisShake3 + ChassisShake4) * 0.25f;
	surf_CurrentMult = (surf_Mult1 + surf_Mult2 + surf_Mult3 + surf_Mult4) * 0.25f;

	// add turnwheel wobbling :)
	const float NewTurnWheelShake = RANDOM_FLOAT( -ChassisShake, ChassisShake ) * AbsCarSpeed * 0.01f;
	AddTurnWheelShake = UTIL_Approach( NewTurnWheelShake, AddTurnWheelShake, 50 * gpGlobals->frametime );

	//----------------------------
	// engine sound and gear
	//----------------------------
	
	if( HasSpawnFlags( SF_CAR_TURBO ) )
	{
		if( (CarSpeed > 0 && bForward()) /*|| (CarSpeed < 0 && bBack())*/ )
		{
			TurboAccum += 0.75f * gpGlobals->frametime;
			CanPlayTurboSound = false;
		}
		else
		{
			CanPlayTurboSound = true;
		}
		TurboAccum = bound( 0.0f, TurboAccum, 1.0f );
	//	ALERT( at_console, "turbo %f\n", TurboAccum );
	}

	bool GearUpdate = false;
	bool Upshifting = false;

	if( CarSpeed <= 0 || (CarSpeed > 0 && !bBack() && !bUp() && bForward()) ) // do not mess with gears during braking, only shift when gas is pressed
	{
		if( !HasSpawnFlags( SF_CAR_ELECTRIC ) )
		{
			if( !IsShifting )
			{
				if( CarSpeed < 0 )
					Gear = 8; // R
				else if( CarSpeed > 0 )
					Gear = 1 + (float)(AbsCarSpeed / GearStep);

				if( Gear == 0 || AbsCarSpeed > 5.0f ) // make HUD update only during valuable speed, to prevent 'back-n-forth' spam
				{
					if( (Gear == 8 && LastGear != Gear) || (Gear == 1 && LastGear == 8) || (Gear > LastGear && Gear == 1) ) // upshifting to first gear from zero speed, or from reverse?
					{
						// skip shifting sequence, just update the HUD
						LastGear = Gear;
						GearUpdate = true;
					}
				}
			}
		}
		else // electric cars only have first gear and reverse
		{
			if( Gear == 0 || AbsCarSpeed > 5.0f )
				Gear = (CarSpeed > 0) ? 1 : 8;
			if( Gear != LastGear )
			{
				LastGear = Gear;
				GearUpdate = true;
			}
		}
	}

	if( Gear > MaxGears && Gear != 8 )
		Gear = MaxGears;

	if( LastGear != Gear )
	{
		// start the shifting
		if( !IsShifting )
		{
			ShiftStartTime = gpGlobals->time;
			IsShifting = true;
			CanPlayTurboSound = true;
		}
		else
		{
			// shifting finished, update the gear and HUD
			if( gpGlobals->time > ShiftStartTime + ShiftingTime )
			{
				IsShifting = false;
				if( Gear > LastGear )
				{
					Upshifting = true;
				}

				LastGear = Gear;
				GearUpdate = true;
			}
		}
	}

	if( GearUpdate ) // send to HUD
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgTempEnt, NULL, hDriver->pev );
			WRITE_BYTE( TE_CARPARAMS );
			WRITE_BYTE( Gear );
		MESSAGE_END();

		if( !HasSpawnFlags( SF_CAR_ELECTRIC ) )
		{
			switch( RANDOM_LONG( 1, 3 ) )
			{
			case 1: EMIT_SOUND( edict(), CHAN_BODY, "func_car/gear1.wav", 0.35, ATTN_NORM ); break;
			case 2: EMIT_SOUND( edict(), CHAN_BODY, "func_car/gear2.wav", 0.35, ATTN_NORM ); break;
			case 3: EMIT_SOUND( edict(), CHAN_BODY, "func_car/gear3.wav", 0.35, ATTN_NORM ); break;
			}
		}

		if( Upshifting && bForward() ) // shake body only when upshifting and gas pressed
			AccelAddX_ShiftAdd = 1.25f;
	}
	
	if( AbsCarSpeed < 15.0f && bBack() && bForward() ) // heating up the tires
	{
		HeatingTires = true;
		if( EngPitch < 200 )
			EngPitch = UTIL_Approach( 200, EngPitch, 45 * gpGlobals->frametime );
		else
			EngPitch = 220 + sin( gpGlobals->time * 50 ) * 10.0f;
	}
	else
	{
		if( HeatingTires )
		{
			if( HeatingMult > 5.0f )
				UTIL_ScreenShakeLocal( hDriver, GetAbsOrigin(), 2, 100, 0.5, 300, true );

			HeatingTires = false;
		}

		HeatingMult = UTIL_Approach( 0, HeatingMult, 10 * gpGlobals->frametime );
	}

	if( !HeatingTires )
	{
		if( AbsCarSpeed == 0.0f && bUp() )
		{
			// same stuff as heating tires, but we are just revving the engine
			if( bForward() || bBack() )
			{
				if( EngPitch < 200 )
					EngPitch = UTIL_Approach( 200, EngPitch, 125 * gpGlobals->frametime );
				else
					EngPitch = 220 + sin( gpGlobals->time * 50 ) * 10.0f;
			}
			else
				EngPitch -= 150 * gpGlobals->frametime;
		}
		else if( ((CarSpeed < 0) || bBack() || bUp()) && !HasSpawnFlags( SF_CAR_ELECTRIC ) ) // going backwards or braking
		{
			if( CarSpeed < 0 )
			{
				EngPitch = 80 + 150 * (AbsCarSpeed / MaxCarSpeedBackwards) + (5 * HeatingMult);
				// close to max speed?
				if( AbsCarSpeed > ( MaxCarSpeedBackwards - 10) )
					EngPitch = 220 + sin( gpGlobals->time * 50 ) * 15.0f;
			}
			else
				EngPitch = 80 + 150 * (AbsCarSpeed / MaxCarSpeed) + (5 * HeatingMult);
		}
		else if( !(bForward()) && !HasSpawnFlags( SF_CAR_ELECTRIC ) ) // no gas pressed
		{
			EngPitch -= (MaxCarSpeed / (1.0f + AbsCarSpeed * 0.1f)) * gpGlobals->frametime;
		}
		else if( (bForward()) || HasSpawnFlags( SF_CAR_ELECTRIC ) ) // stepping on the gas
		{
			if( IsShifting )
			{
				// during the shift, drop the engine pitch down
				EngPitch = UTIL_Approach( 80, EngPitch, 200 * gpGlobals->frametime );
			}
			else
			{
				EngPitch = UTIL_Approach( 80 + 150.0f * ((AbsCarSpeed / (float)(GearStep * (Gear < 1 ? 1 : Gear)))) - (Gear * 2) + (5 * HeatingMult), EngPitch, 1000 * gpGlobals->frametime );
			}
		}
	}

	// add "sound shake" on some surfaces
	const float SndShake = ChassisShake * SuspHardness * AbsCarSpeed * 0.0001f;
	EngPitch += RANDOM_FLOAT( -SndShake, SndShake );
	EngPitch = bound( 80, EngPitch, 250 );
	if( BrokenCar )
		EngPitch *= 0.65;

	int EngFlags = SND_CHANGE_PITCH;
	float EngVol = 1.0f;

	if( HasSpawnFlags( SF_CAR_ELECTRIC ) )
	{
		EngFlags |= SND_CHANGE_VOL;
		EngVol = AbsCarSpeed / MaxCarSpeed;
	}

	if( m_iszEngineSnd )
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, STRING( m_iszEngineSnd ), EngVol, ATTN_NORM, EngFlags, (int)EngPitch );

	// add gear whine
	if( HasSpawnFlags( SF_CAR_GEARWHINE ) )
	{
		float WhinePitch = 90 + (10 * AbsCarSpeed) / (MaxCarSpeed * 0.35f) + Gear;
		WhinePitch = bound( 90, WhinePitch, 120 );
		float WhineVolume = AbsCarSpeed / (MaxCarSpeed * 0.35f);
		WhineVolume = bound( 0, WhineVolume, 1 );
		if( (WhineVolume > 0.01f) && bForward() && !IsShifting )
			EMIT_SOUND_DYN( edict(), CHAN_VOICE, "func_car/gear_whine.wav", WhineVolume, ATTN_NORM, SND_CHANGE_VOL | SND_CHANGE_PITCH, (int)WhinePitch );
		else
			STOP_SOUND( edict(), CHAN_VOICE, "func_car/gear_whine.wav" );
	}
	
	//----------------------------
	// collision
	//----------------------------

	Vector Collision;
	float ColDotProduct;
	Vector ColPoint;
	GetCollision( AbsCarSpeed, Forward, &Collision, &ColDotProduct, &ColPoint );
	if( Collision.IsNull() && (FrontWheelsInAir + RearWheelsInAir == 0) )
		SafeCarPos = GetAbsOrigin();

	//----------------------------
	// chassis inclination
	//----------------------------
	const int SuspDiff = fabs( RearWheelRadius - FrontWheelRadius );
	float SuspDiff2 = 1.0f;

	// RECHECK THIS IN GAME!!! Set bigger front or rear wheels
//	if( FrontWheelRadius < RearWheelRadius )
//		SuspDiff2 = (float)FrontWheelRadius / (float)RearWheelRadius;
//	else if( FrontWheelRadius > RearWheelRadius )
//		SuspDiff2 = (float)RearWheelRadius / (float)FrontWheelRadius;

	Vector ChassisAng = pChassis->GetLocalAngles();
	float CrossChange = ((pWheel1->GetAbsOrigin().z + pWheel2->GetAbsOrigin().z) * 0.5) - ((pWheel3->GetAbsOrigin().z + pWheel4->GetAbsOrigin().z - SuspDiff * 2) * 0.5);
	// FIXME - magic numbers! we have to account the distance between wheels for correct body angle on slopes
	// SuspDiff2 is not needed anymore because of this, so it's always 1.0
	// something similar must be done for LateralChange...
	CrossChange *= 115.0f / (float)(FrontSuspDist + RearSuspDist);
	
	const float LateralChange = ((pWheel1->GetAbsOrigin().z + pWheel3->GetAbsOrigin().z - SuspDiff) * 0.5) - ((pWheel2->GetAbsOrigin().z + pWheel4->GetAbsOrigin().z - SuspDiff) * 0.5);

	// add shaking from going on dirt/snow/grass...
	float AddChassisShake = 0.0f;
	if( ChassisShake > 0 && (AbsCarSpeed > 0 || HeatingTires) )
	{
		AddChassisShake = RANDOM_FLOAT( -ChassisShake, ChassisShake );
		AddChassisShake *= (AbsCarSpeed * 0.001 + HeatingMult * 0.1);
		AddChassisShake = bound( -ChassisShake, AddChassisShake, ChassisShake );
	}

	// X - transversal rotation
	// lean car when braking
	int HandBrakeLean = 1;
	if( bUp() ) // hit handbrake
		HandBrakeLean = 3;

	if( CarSpeed > 10 && ( ((bBack()) && !(bForward())) || (bUp())))
		BrakeAddX += HandBrakeLean * BrakeRate * 0.01 * gpGlobals->frametime;
	else if( CarSpeed < -10 && ((bForward()) || (bUp())) ) // don't need bBack check because of "else if" in accelerate below
		BrakeAddX -= HandBrakeLean * BrakeRate * 0.01 * gpGlobals->frametime;
	else
		BrakeAddX = UTIL_Approach( 0, BrakeAddX, 5 * gpGlobals->frametime );

	BrakeAddX = bound( -MaxLean * 0.2, BrakeAddX, MaxLean * 0.2 );

	// lean car when accelerating
	if( HeatingTires )
		AccelAddX = UTIL_Approach( 0, AccelAddX, 3 * gpGlobals->frametime );
	else if( CarSpeed > 0 && bForward() && !IsShifting )
		//	AccelAddX -= AccelRate * 0.02 * gpGlobals->frametime * (1 + HeatingMult);
		AccelAddX = UTIL_Approach( NewAccelAddX * (10.0f / SuspHardness), AccelAddX, 10 * gpGlobals->frametime );
	else if( CarSpeed < 0 && CarSpeed > -150 && !(bForward()) && (bBack()) )
		AccelAddX += BackAccelRate * 0.01 * gpGlobals->frametime * (1 + HeatingMult);
	else
		AccelAddX = UTIL_Approach( 0, AccelAddX, 3 * gpGlobals->frametime );

	AccelAddX = bound( -MaxLean * 0.2, AccelAddX, MaxLean * 0.2 );
	
	const float NewChassisAngX = -CrossChange * 0.5 * SuspDiff2 + BrakeAddX + AccelAddX + AddChassisShake - AccelAddX_ShiftAdd;
	NewCameraAngleX = NewChassisAngX - BrakeAddX - AccelAddX + AccelAddX_ShiftAdd; // yeah...compensate it back
	const float OldChassisAngX = ChassisAng.x;
	AccelAddX_ShiftAdd = UTIL_Approach( 0.0f, AccelAddX_ShiftAdd, 10 * gpGlobals->frametime );
	if( FrontWheelsInAir + RearWheelsInAir < 4 )
		ChassisAng.x = lerp( ChassisAng.x, NewChassisAngX, SuspHardness * gpGlobals->frametime );

	// Z - lateral rotation
	float NewChassisAngZ = -LateralChange * SuspDiff2 + CarSpeed * Turning * (MaxLean * 0.001) + AddChassisShake;
	if( EnteringShake > 0.0f )
	{
		NewChassisAngZ += EnteringShake * sin( gpGlobals->time * 5.0f );
		EnteringShake = UTIL_Approach( 0.0f, EnteringShake, 3.0f * gpGlobals->frametime );
	}

	if( FrontWheelsInAir + RearWheelsInAir < 4 )
		ChassisAng.z = lerp( ChassisAng.z, NewChassisAngZ, SuspHardness * gpGlobals->frametime );

	if( FrontWheelsInAir + RearWheelsInAir == 4 )
	{
	//	ChassisAng.x = UTIL_Approach( 0, ChassisAng.x, 50 * gpGlobals->frametime );
		ChassisAng.z = 0;
	}

	ChassisAng.x = bound( -30, ChassisAng.x, 30 );
	ChassisAng.z = bound( -30, ChassisAng.z, 30 );
	pChassis->SetLocalAngles( ChassisAng );

//	float MiddlePoint = (pWheel1->GetAbsOrigin().z + pWheel2->GetAbsOrigin().z + pWheel3->GetAbsOrigin().z + pWheel4->GetAbsOrigin().z) * 0.25;
//	Vector ChassisOrg = pChassis->GetAbsOrigin();
//	ChassisOrg.z = UTIL_Approach( MiddlePoint, ChassisOrg.z, SuspHardness * fabs( MiddlePoint - ChassisOrg.z ) * gpGlobals->frametime );
//	pChassis->SetAbsOrigin( ChassisOrg );
	Vector ChassisOrg = pChassis->GetAbsOrigin();
	const Vector NewChassisOrg = (pWheel1->GetAbsOrigin() + pWheel2->GetAbsOrigin() + pWheel3->GetAbsOrigin() + pWheel4->GetAbsOrigin()) * 0.25;
	ChassisOrg.x = UTIL_Approach( NewChassisOrg.x, ChassisOrg.x, SuspHardness * fabs( NewChassisOrg.x - ChassisOrg.x ) * gpGlobals->frametime );
	ChassisOrg.y = UTIL_Approach( NewChassisOrg.y, ChassisOrg.y, SuspHardness * fabs( NewChassisOrg.y - ChassisOrg.y ) * gpGlobals->frametime );
	ChassisOrg.z = UTIL_Approach( NewChassisOrg.z, ChassisOrg.z, SuspHardness * fabs( NewChassisOrg.z - ChassisOrg.z ) * gpGlobals->frametime );
	pChassis->SetAbsOrigin( ChassisOrg );

	//----------------------------
	// accelerate/brake
	//----------------------------
	// water
	float WaterVelocityMult = 1.0;
	switch( hDriver->pev->waterlevel )
	{
	case 1: WaterVelocityMult = 0.75; break;
	case 2: WaterVelocityMult = 0.5; break;
	case 3: WaterVelocityMult = 0.25; break;
	}

	const float ActualMaxCarSpeed = MaxCarSpeed * WaterVelocityMult * surf_CurrentMult * DriftAmount;
	const float ActualMaxCarSpeedBackwards = MaxCarSpeedBackwards * WaterVelocityMult * surf_CurrentMult * DriftAmount;
	const float ActualAccelRate = (DriftAmount * (AccelRate + (200 * HeatingMult) + (TurboAccum * 100))) / (1 + (AbsCarSpeed / MaxCarSpeed));
	const float ActualBackAccelRate = (BackAccelRate + (200 * HeatingMult)) / (1 + (AbsCarSpeed / MaxCarSpeed));

	if( HasSpawnFlags( SF_CAR_TURBO ) )
	{
		if( CanPlayTurboSound && (TurboAccum > 0.25f) )
		{
			EMIT_SOUND_DYN( edict(), CHAN_STATIC, "func_car/turbo.wav", TurboAccum, ATTN_NORM, 0, RANDOM_LONG( 90, 110 ) );
			CanPlayTurboSound = false;
			TurboAccum = 0.0f;
		}
	}

	bool DoBrakeSqueak = false;

	int RampDir = 1; // 1 up, -1 down
	if( Forward == 1 && ChassisAng.x > 10.0f )
		RampDir = -1;
	else if( Forward == -1 && ChassisAng.x < -10.0f ) // going backwards, nose up
		RampDir = -1;

	if( !Collision.IsNull() )
	{
		// can't accelerate if we are colliding
		float AbsDot = fabs( ColDotProduct );
		if( CarSpeed > 0 )
		{
			CarSpeed -= 5000 * AbsDot * gpGlobals->frametime;
			if( CarSpeed < 25 && bForward() ) // in case if stuck...
				CarSpeed = 25;
			else if( CarSpeed < 0 )
				CarSpeed = 0;
		}
		else if( CarSpeed < 0 )
		{
			CarSpeed += 5000 * AbsDot * gpGlobals->frametime;
			if( CarSpeed > -25 && bBack() ) // in case if stuck...
				CarSpeed = -25;
			else if( CarSpeed > 0 )
				CarSpeed = 0;
		}
	}
	else if( (FrontWheelsInAir + RearWheelsInAir == 4) )
	{
		// don't add speed
	}
	else if( FrontWheelsInAir == 2 )
	{
		CarSpeed += 300 * gpGlobals->frametime; // push the car off cliff
	}
	else if( RearWheelsInAir == 2 )
	{
		CarSpeed -= 300 * gpGlobals->frametime; // push the car off cliff
	}
	else if( (!(bForward()) && !(bBack()) && !(bUp())) || BrokenCar || IsLockedByMaster() )
	{
		// no accelerate/brake buttons pressed or all wheels in air
		// car slows down (aka friction...)
		float SlowDownRamp = 150.0f * pev->frame;

		if( fabs( ChassisAng.x ) > 10 ) // start to go down by itself, if on a slope
			SlowDownRamp = 100 + 5 * fabs( ChassisAng.x ); // must put weight there I guess

		if( CarSpeed >= 20.0f )
			CarSpeed -= RampDir * SlowDownRamp * WaterVelocityMult * (1/surf_CurrentMult) * gpGlobals->frametime;
		else if( CarSpeed <= -20.0f )
			CarSpeed += RampDir * SlowDownRamp * WaterVelocityMult * (1/surf_CurrentMult) * gpGlobals->frametime;

		if( AbsCarSpeed < 20 )
			CarSpeed = 0.0f;
	}
	else if( bForward() || bBack() || bUp() )
	{
		if( bUp() || HeatingTires ) // handbrake
		{
			CarSpeed = UTIL_Approach( 0, CarSpeed, BrakeRate * 1.25f * gpGlobals->frametime );
		}
		else if( bForward() )
		{
			if( CarSpeed > 0 && CarSpeed > ActualMaxCarSpeed && ActualMaxCarSpeed != MaxCarSpeed )
				CarSpeed -= 300 * gpGlobals->frametime;
			else if( !IsShifting )
			{
				float tx = TurningOverride ? 1.0f : (1.0f - AbsTurning * 0.75f);
				tx = bound( 0.5f, tx, 1.0f );
				CarSpeed += ActualAccelRate * tx * WaterVelocityMult * surf_CurrentMult * gpGlobals->frametime;
			}

			if( CarSpeed < 0 )
				DoBrakeSqueak = true;
		}
		else if( bBack() )
		{
			if( CarSpeed > 0 )
			{
				CarSpeed -= BrakeRate * gpGlobals->frametime;
				DoBrakeSqueak = true;
			}
			else
			{
				if( CarSpeed < 0 && CarSpeed < -ActualMaxCarSpeedBackwards && ActualMaxCarSpeedBackwards != MaxCarSpeedBackwards )
					CarSpeed += 300 * gpGlobals->frametime;
				else
					CarSpeed -= ActualBackAccelRate * (1.0f - AbsTurning * 0.5f) * WaterVelocityMult * surf_CurrentMult * gpGlobals->frametime;
			}
		}
	}

	if( HasSpawnFlags( SF_CAR_SQUEAKYBRAKES ) )
	{
		if( DoBrakeSqueak )
		{
			BrakeSqueak = UTIL_Approach( 1.0f, BrakeSqueak, gpGlobals->frametime );
			if( AbsCarSpeed < 200 )
				BrakeSqueak *= (AbsCarSpeed / 200);
			if( BrakeSqueak > 0.1f )
			{
				EMIT_SOUND_DYN( pWheel3->edict(), CHAN_BODY, "func_car/brakes.wav", BrakeSqueak, 1.0f, SND_CHANGE_PITCH | SND_CHANGE_VOL, PITCH_NORM );
			}
		}
		else if( BrakeSqueak > 0.0f ) // this way we shouldn't overspam the sounds...ugh
		{
			BrakeSqueak = UTIL_Approach( 0.0f, BrakeSqueak, gpGlobals->frametime * 2.0f );
			if( AbsCarSpeed < 200 )
				BrakeSqueak *= (AbsCarSpeed / 200);
			if( BrakeSqueak <= 0.0f )
				EMIT_SOUND( pWheel3->edict(), CHAN_BODY, "common/null.wav", 0, 3.0 );
			else
				EMIT_SOUND_DYN( pWheel3->edict(), CHAN_BODY, "func_car/brakes.wav", BrakeSqueak, 1.0f, SND_CHANGE_PITCH | SND_CHANGE_VOL, PITCH_NORM );
		}
	}

	// chassis has changed angles too fast (more than 5 deg this frame). slow down
	if( fabs( NewChassisAngX - OldChassisAngX ) > 5.0f && (FrontWheelsInAir + RearWheelsInAir == 0) )
	{	
		CarSpeed = UTIL_Approach( 0.0f, CarSpeed, 150 * gpGlobals->frametime * RampDir * fabs( NewChassisAngX - OldChassisAngX ) );
	//	ALERT( at_console, "slow %.1f\n", fabs( NewChassisAngX - OldChassisAngX ) );
	//	CarSpeed -= 50 * gpGlobals->frametime * Forward * fabs( NewChassisAngX - ChassisAng.x );
	}

	if( DriftMode )
	{
		CarSpeed = bound( (-ActualMaxCarSpeedBackwards + (ActualMaxCarSpeedBackwards * AbsTurning * 0.5f)), CarSpeed, (ActualMaxCarSpeed - (ActualMaxCarSpeed * AbsTurning * 0.75f)) );
	}
	else
	{
		// also slow down the car if turning too much
		float turn_slowdown_back = 1.0f;
		float turn_slowdown_forw = 1.0f;
		if( !TurningOverride )
		{
			turn_slowdown_back /= 1.0f + (AbsTurning * 0.25f);
			turn_slowdown_forw /= 1.0f + (AbsTurning * 0.5f);
		}
		// main speed bound
		CarSpeed = bound( -ActualMaxCarSpeedBackwards * turn_slowdown_back, CarSpeed, ActualMaxCarSpeed * turn_slowdown_forw );
	}

	// -------- apply main movement  --------
	const float AngleDiff = pWheel1->GetLocalAngles().y - pChassis->GetLocalAngles().y;
	const float Percentage = Forward * (AngleDiff / (90 - MaxTurn));
	if( !Collision.IsNull() )
	{
		pev->velocity = Collision;
	}
	else if( bUp() ) // handbrake
	{
		pev->velocity += gpGlobals->v_forward * CarSpeed * (1 - fabs(Percentage)) + gpGlobals->v_right * CarSpeed * Percentage;
	}
	else
	{
		if( DriftMode )
			pev->velocity += DriftForw * CarSpeed;
		else
			pev->velocity += gpGlobals->v_forward * CarSpeed;

		// check wheels - if one side is fully in air, push car in that direction
		if( FrontWheelsInAir + RearWheelsInAir < 4 )
		{
			if( FLW_InAir + RLW_InAir == 2 )
				pev->velocity += -gpGlobals->v_right * 200;
			else if( FRW_InAir + RRW_InAir == 2 )
				pev->velocity += gpGlobals->v_right * 200;

			// add small drift
		//	if( (AbsCarSpeed > 500) && !(bUp) )
		//		pev->velocity += gpGlobals->v_right * Turning * CarSpeed * 0.25;
		}
	}

	//----------------------------
	// weight...?
	//----------------------------
	if( FrontWheelsInAir + RearWheelsInAir == 4 )
	{
		if( !CarInAir )
		{
			CarInAir = true;
			DownForce = 100;
		}
	}

	if( FrontWheelsInAir + RearWheelsInAir == 0 && CarInAir )
	{
		CarInAir = false;
	//	if( DownForce < -100 )
	//	{
			UTIL_ScreenShakeLocal( hDriver, GetAbsOrigin(), 4, 100, 1, 300, true );
			EMIT_SOUND_DYN( edict(), CHAN_STATIC, "func_car/landing.wav", VOL_NORM, ATTN_NORM, 0, RANDOM_LONG(90,110) );
			CarSpeed *= 0.75;
	//	}
	}

	if( hDriver->pev->waterlevel >= 2 )
	{
		DownForce = -50;
	}
	else if( FrontWheelsInAir + RearWheelsInAir < 4 && FrontWheelsInAir + RearWheelsInAir > 0 )
	{
		DownForce = -100; // car weight * 0.1 maybe? (no such thing as car weight yet, just thoughts)
	}
	else if( FrontWheelsInAir + RearWheelsInAir == 4 )
	{
		DownForce -= 200 * gpGlobals->frametime;
		if( DownForce < 0 )
			DownForce *= 1 + (2 * gpGlobals->frametime);
		else
			DownForce -= 400 * gpGlobals->frametime;

		if( DownForce < -2000 )
			DownForce = -2000;
	}
	else
	{
		float mult = fabs( ChassisAng.x );
		if( mult < 1.0f ) mult = 1.0f; // -____-
		DownForce = -AbsCarSpeed * (0.035f * mult);
	}

	pev->velocity.z = DownForce;

	//----------------------------
	// timed effects
	//----------------------------

	bool TookDamage = false;
	float DriverDamage = 0.0f;

	if( CarTouch )
		CarSpeed *= 0.5;
	
	// do timing stuff each 0.5 seconds
	if( !Collision.IsNull() && (gpGlobals->time > time) || CarTouch )
	{
		if( ColDotProduct < 0 && ColDotProduct > -0.18 )
			ColDotProduct = -0.18;
		else if( ColDotProduct > 0 && ColDotProduct < 0.18 )
			ColDotProduct = 0.18;
		// 0.18 so the multiplier would be less than 1 always

		if( AbsCarSpeed > 500 )
		{
			TookDamage = true;
			DriverDamage = AbsCarSpeed * 0.025f * fabs(ColDotProduct);
		}

		if( AbsCarSpeed > 100 )
		{
			// apply collision effects
			if( !ColPoint.IsNull() )
				UTIL_Sparks( ColPoint );

			switch( RANDOM_LONG( 0, 3 ) )
			{
			case 0: EMIT_SOUND( edict(), CHAN_STATIC, "func_car/car_impact1.wav", 1, ATTN_NORM ); break;
			case 1: EMIT_SOUND( edict(), CHAN_STATIC, "func_car/car_impact2.wav", 1, ATTN_NORM ); break;
			case 2: EMIT_SOUND( edict(), CHAN_STATIC, "func_car/car_impact3.wav", 1, ATTN_NORM ); break;
			case 3: EMIT_SOUND( edict(), CHAN_STATIC, "func_car/car_impact4.wav", 1, ATTN_NORM ); break;
			}
		}

		if( CarTouch )
			CarTouch = false;
			
		time = gpGlobals->time + 0.5;
		StuckTime = gpGlobals->time;
	}

	Camera();

	//----------------------------
	// driver model
	//----------------------------
	if( pDriverMdl )
	{
		int f = 1;
		if( CarSpeed < -10 ) f = -1; // ¯\_(ツ)_/¯
		CBaseAnimating *DriverMdl = (CBaseAnimating *)(CBaseEntity *)pDriverMdl;
		DriverMdl->SetBlending( 0, (f * -FrontWheelAng.y * 2) + AddTurnWheelShake);
		
		bool SpecialAnim = false;

		if( CarSpeed >= 0 || HeatingTires )
			DriverMdlSequence = DriverMdl->LookupSequence( "idle" );
		else if( CarSpeed < 25 && bBack() )
			DriverMdlSequence = DriverMdl->LookupSequence( "lookback" );

		// special anims, reserve a second
		if( (ColPoint.Length() > 0) && (AbsCarSpeed > 100) )
		{
			SpecialAnim = true;

			if( CarSpeed > 0 )
				DriverMdlSequence = DriverMdl->LookupSequence( "damage_small" );
			else if( CarSpeed < 0 )
				DriverMdlSequence = DriverMdl->LookupSequence( "damage_small_back" );
		}

		if( TookDamage )
		{
			DriverMdlSequence = DriverMdl->LookupSequence( "damage" );
			SpecialAnim = true;
		}

		if( DriverMdl->pev->sequence != DriverMdlSequence && driveranimtime < gpGlobals->time )
		{
			DriverMdl->pev->sequence = DriverMdlSequence;
			DriverMdl->pev->frame = 0;
			DriverMdl->ResetSequenceInfo();
			if( SpecialAnim )
				driveranimtime = gpGlobals->time + 0.5;
		}
	}

	// measure speed
	const float kph = (Distance * 0.001f) / (gpGlobals->frametime / 3600.0f);

	// unstick the car if this happens...
	// stupid hack just to keep things playable because it can truly stick in a brush and it looks terrible
	const float ExpectedDist = fUnitsToMeters( (pev->velocity * gpGlobals->frametime).Length() ); // predicted velocity next frame in meters
	// we didn't make even a half of expected distance, likely stuck
	if( (AbsCarSpeed > 100.0f) && (ExpectedDist > 0.01f) && (Distance < ExpectedDist * 0.5f) && (gpGlobals->time > StuckTime + 1) && Collision.IsNull() )
	{
	//	ALERT( at_console, "this frame %f, next frame %f\n", Distance, ExpectedDist );
		ALERT( at_aiconsole, "CCar::TryUnstick\n" );
		TryUnstick();
		StuckTime = gpGlobals->time;
	}

	//----------------------------
	// chassis (body) model
	//----------------------------
	if( pChassisMdl )
	{
		// turn the wheel
		if( pChassisMdl->pev->sequence != 0 )
			pChassisMdl->pev->sequence = 0;

		CBaseAnimating *ChassisMdl = (CBaseAnimating *)(CBaseEntity *)pChassisMdl;
		ChassisMdl->SetBlending( 0, (-FrontWheelAng.y * 2) + AddTurnWheelShake );
	}
	
	SetLocalAngles( CarAng ); // car direction is now set

	//----------------------------
	// shoot!
	//----------------------------
	if( pTankTower )
	{
		const bool bAimingView = (hDriver->pev->button & IN_ATTACK2);
		// rotate tower
		Vector TowerAngles = ChassisAng;
		if( pChassisMdl )
			TowerAngles = pChassisMdl->GetLocalAngles();

		if( bAimingView || CamUnlocked )
		{
			// turret moves almost freely because we use it as a camera
			TowerAngles = hDriver->pev->v_angle;
			TowerAngles.x *= 0.35f; // let's not get carried away...
			Camera2RotationX = 0;
			Camera2RotationY = 0;
		}
		else
		{
			TowerAngles.y += TankTowerRotationOffset;
			TowerAngles.y += Camera2RotationY;
			TowerAngles.x += Camera2RotationX;
		}
		
		if( bAimingView || CamUnlocked )
			pTankTower->SetAbsAngles( TowerAngles );
		else
			pTankTower->SetLocalAngles( TowerAngles );

		if( (hDriver->pev->button & IN_ATTACK) && (gpGlobals->time > LastShootTime + 1) )
		{
			// use forward vector of the tower
			Vector TowerAbsAngles = pTankTower->GetAbsAngles();
			UTIL_MakeVectors( TowerAbsAngles );
			Vector RocketOrg = pTankTower->GetAbsOrigin() + gpGlobals->v_forward * 50;
			Vector RocketAng = TowerAbsAngles;

			if( !CamUnlocked )
				RocketAng = TowerAbsAngles; // just forward
			
			if( !CamUnlocked )
			{
				RocketAng.z = 0;
				if( !SecondaryCamera )
					RocketAng.x = 0;
			}

			RocketAng.x -= 5.0f; // aim higher
			CBaseEntity *pRocket = CBaseEntity::Create( "apc_projectile", RocketOrg, RocketAng, hDriver->edict() );

			if( pRocket )
			{
				pRocket->SetAbsVelocity( gpGlobals->v_forward * 3500 );
				PlayClientSound( pTankTower, 248 );
			}

			LastShootTime = gpGlobals->time;

			// bring back the car vectors
			UTIL_MakeVectors( GetAbsAngles() );
		}
	}

	SafeSpawnPosition();

	if( !LastCarSpeed )
		LastCarSpeed = CarSpeed;
	float spd_diff = CarSpeed - LastCarSpeed;
	// make sure we are accelerating
	if( spd_diff > 0 && (CarSpeed < ActualMaxCarSpeed * 0.8f) )
		NewAccelAddX = (-spd_diff / gpGlobals->frametime) * 0.01f;
	else
		NewAccelAddX = UTIL_Approach( 0.0f, NewAccelAddX, gpGlobals->frametime );
	LastCarSpeed = CarSpeed;

	//-----------------------------------------------------
	// !!! hDriver can become NULL after this, by taking damage and dying, game will crash.
	//-----------------------------------------------------
	if( TookDamage && DriverDamage > 10.0f )
	{
		UTIL_ScreenShakeLocal( hDriver, GetAbsOrigin(), AbsCarSpeed * 0.01, 100, 1, 300, true );
		TakeDamage( pev, pev, DriverDamage, DMG_CRUSH );
	}

	if( hDriver == NULL )
		return;

	// trail effects when driving on water
	if( (gpGlobals->time > LastTrailTime + 0.1) && (AbsCarSpeed > 15) && (pev->waterlevel > 0) )
	{
		Vector TrailOrg = GetAbsOrigin();
		float TrailAngle = GetAbsAngles().y;
		UTIL_MakeVectors( GetAbsAngles() );
		int CarLength = FrontBumperLength + RearBumperLength + FrontSuspDist + RearSuspDist;
		if( Forward == 1 )
		{
			TrailAngle += 180;
			TrailOrg -= gpGlobals->v_forward * CarLength * 0.35;
		}
		else if( Forward == -1 )
			TrailOrg += gpGlobals->v_forward * CarLength * 0.35;

		int Scale = bound( 100, AbsCarSpeed * 0.25, 255 );
		int Spd = bound( 0, AbsCarSpeed, 255 );
		MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, TrailOrg );
			WRITE_BYTE( TE_BOAT_TRAIL );
			WRITE_COORD( TrailOrg.x );
			WRITE_COORD( TrailOrg.y );
			WRITE_COORD( TrailOrg.z );
			WRITE_COORD( TrailAngle );
			WRITE_BYTE( Scale );
			WRITE_BYTE( Spd );
		MESSAGE_END();

		LastTrailTime = gpGlobals->time;
	}

	//----------------------------
	// exhaust fx
	//----------------------------
	// this is not supposed to be on electic cars but if the creator wants that...be my guest!
	if( num_exhausts > 0 )
	{
		// write the smoke velocity and exhaust rate - the client will deal with it using "iuser3" tag
		UTIL_MakeVectors( GetAbsAngles() );
		float smoke_vel = -35.0f;
		if( Forward == -1 )
			smoke_vel = 35.0f;
		const Vector v_smoke_vel = pev->velocity + gpGlobals->v_forward * smoke_vel;
		if( pExhaust1 )
		{
			pExhaust1->pev->vuser1 = v_smoke_vel;
			if( bForward() || bBack() )
				pExhaust1->pev->fuser1 -= gpGlobals->frametime * AccelRate * 0.01f;
			else
				pExhaust1->pev->fuser1 += gpGlobals->frametime * 0.35f;
			pExhaust1->pev->fuser1 = bound( 0.025f, pExhaust1->pev->fuser1, 0.1f );
			pExhaust1->pev->renderamt = 255;
		}

		if( pExhaust2 )
		{
			pExhaust2->pev->vuser1 = v_smoke_vel;
			if( bForward() || bBack() )
				pExhaust2->pev->fuser1 -= gpGlobals->frametime * 0.5f;
			else
				pExhaust2->pev->fuser1 += gpGlobals->frametime * 0.5f;
			pExhaust2->pev->fuser1 = bound( 0.025f, pExhaust2->pev->fuser1, 0.1f );
		}

		// accumulate the pops
		if( HasSpawnFlags( SF_CAR_EXHAUSTPOPS ) )
		{
			if( !IsShifting &&
					(
					HeatingTires 
					|| (bForward() && CarSpeed > 0) 
					|| (bBack() && CarSpeed < 0) 
					|| (AbsCarSpeed == 0.0f && bUp() && (bForward() || bBack())) 
					)
				)
			{
				if( num_pops < 7.0f )
					num_pops += gpGlobals->frametime * AccelRate * 0.01f;
				poptime = gpGlobals->time + 0.1f;
			}
		}

		// do pop-pop-pop!
		bool candopop = true;
		if( IsShifting )
		{
			// yes
		}
		else if( bForward() && CarSpeed > 0 )
			candopop = false;
		else if( bBack() && CarSpeed < 0 )
			candopop = false;

		if( num_pops > 0.0f && candopop )
		{
			if( gpGlobals->time > poptime )
			{
				if( num_exhausts == 2 ) // alternate
					currentExhaust = (currentExhaust == pExhaust1) ? pExhaust2 : pExhaust1;
				else
					currentExhaust = pExhaust1;
				currentExhaust->pev->fuser2 = 1.0f;
				if( currentExhaust->pev->fuser2 == 1.0f )
				{
					EMIT_SOUND_DYN( currentExhaust->edict(), CHAN_BODY, pExhaustPopSounds[RANDOM_LONG( 0, SIZEOFARRAY( pExhaustPopSounds ) - 1 )], 1.0, ATTN_NORM, 0, RANDOM_LONG( 80, 120 ) );
					Vector x = currentExhaust->GetAbsAngles();
					x[ROLL] = RANDOM_LONG( 0, 359 );
					currentExhaust->SetAbsAngles( x );
				}
				poptime = gpGlobals->time + (RANDOM_FLOAT( 0.1f, 0.5f ) / num_pops);
				num_pops -= 1.0f;
			}
		}

		// immediately start fading the sprites out because the sound plays at exactly 1.0f
		if( pExhaust1 )
		{
			if( pExhaust1->pev->fuser2 > 0.0f )
				pExhaust1->pev->fuser2 = UTIL_Approach( 0.0f, pExhaust1->pev->fuser2, gpGlobals->frametime * 7.0f );

			pExhaust1->pev->renderamt = 255 * pExhaust1->pev->fuser2;
		}

		if( pExhaust2 )
		{
			if( pExhaust2->pev->fuser2 > 0.0f )
				pExhaust2->pev->fuser2 = UTIL_Approach( 0.0f, pExhaust2->pev->fuser2, gpGlobals->frametime * 7.0f );

			pExhaust2->pev->renderamt = 255 * pExhaust2->pev->fuser2;
		}
	}
	
	SetNextThink( 0 );
}

void CCar::Wheels( int *FRW_InAir, int *FLW_InAir, int *RRW_InAir, int *RLW_InAir )
{
	TraceResult tr;

	*FRW_InAir = 0;
	*FLW_InAir = 0;
	*RRW_InAir = 0;
	*RLW_InAir = 0;

	// make chassis vectors
	UTIL_MakeVectors( pChassis->GetAbsAngles() );
	Vector ChassisForw = gpGlobals->v_forward;
	Vector ChassisUp = gpGlobals->v_up;
	Vector ChassisRight = gpGlobals->v_right;

	// bring back the car vectors
	UTIL_MakeVectors( GetAbsAngles() );

	pWheel1Org = GetAbsOrigin() + ChassisForw * FrontSuspDist + gpGlobals->v_right * (FrontSuspWidth * 0.5);
	pWheel2Org = GetAbsOrigin() + ChassisForw * FrontSuspDist - gpGlobals->v_right * (FrontSuspWidth * 0.5);
	pWheel3Org = GetAbsOrigin() - ChassisForw * RearSuspDist + gpGlobals->v_right * (RearSuspWidth * 0.5);
	pWheel4Org = GetAbsOrigin() - ChassisForw * RearSuspDist - gpGlobals->v_right * (RearSuspWidth * 0.5);
	Vector pWheel1NewOrg = pWheel1Org;
	Vector pWheel2NewOrg = pWheel2Org;
	Vector pWheel3NewOrg = pWheel3Org;
	Vector pWheel4NewOrg = pWheel4Org;

	// front right wheel
	// ChassisUp * 40: need to make tracing start high to work properly on slopes !!!
	const float SlopeAdjust = 40.0f;
	UTIL_TraceLine( pWheel1Org + ChassisUp * SlopeAdjust, pWheel1Org - ChassisUp * FrontWheelRadius * 2, ignore_monsters, dont_ignore_glass, pWheel1->edict(), &tr );
	pWheel1NewOrg = tr.vecEndPos + ChassisUp * FrontWheelRadius;

	if( tr.flFraction == 1.0f && (int)(pWheel1NewOrg - pWheel1Org).Length() >= FrontWheelRadius - 1 )
	{
		pWheel1NewOrg = pWheel1Org - ChassisUp * FrontWheelRadius;
		*FRW_InAir = 1;
	}

	// front left wheel
	UTIL_TraceLine( pWheel2Org + ChassisUp * SlopeAdjust, pWheel2Org - ChassisUp * FrontWheelRadius * 2, ignore_monsters, dont_ignore_glass, pWheel2->edict(), &tr );
	pWheel2NewOrg = tr.vecEndPos + ChassisUp * FrontWheelRadius;
	if( tr.flFraction == 1.0f && (int)(pWheel2NewOrg - pWheel2Org).Length() >= FrontWheelRadius - 1 )
	{
		pWheel2NewOrg = pWheel2Org - ChassisUp * FrontWheelRadius;
		*FLW_InAir = 1;
	}

	// rear right wheel
	UTIL_TraceLine( pWheel3Org + ChassisUp * SlopeAdjust, pWheel3Org - ChassisUp * RearWheelRadius * 2, ignore_monsters, dont_ignore_glass, pWheel3->edict(), &tr );
	pWheel3NewOrg = tr.vecEndPos + ChassisUp * RearWheelRadius;
	if( tr.flFraction == 1.0f && (int)(pWheel3NewOrg - pWheel3Org).Length() >= RearWheelRadius - 1 )
	{
		pWheel3NewOrg = pWheel3Org - ChassisUp * RearWheelRadius;
		*RRW_InAir = 1;
	}

	// rear left wheel
	UTIL_TraceLine( pWheel4Org + ChassisUp * SlopeAdjust, pWheel4Org - ChassisUp * RearWheelRadius * 2, ignore_monsters, dont_ignore_glass, pWheel4->edict(), &tr );
	pWheel4NewOrg = tr.vecEndPos + ChassisUp * RearWheelRadius;
	if( tr.flFraction == 1.0f && (int)(pWheel4NewOrg - pWheel4Org).Length() >= RearWheelRadius - 1 )
	{
		pWheel4NewOrg = pWheel4Org - ChassisUp * RearWheelRadius;
		*RLW_InAir = 1;
	}

	pWheel1->SetAbsOrigin( pWheel1NewOrg );
	pWheel2->SetAbsOrigin( pWheel2NewOrg );
	pWheel3->SetAbsOrigin( pWheel3NewOrg );
	pWheel4->SetAbsOrigin( pWheel4NewOrg );
}

//===============================================================================
// camera view
//===============================================================================
void CCar::Camera(void)
{
	if( !AllowCamera )
		return;

	if( !hDriver || !(hDriver->pev->flags & FL_CLIENT) )
		return;

	if( !LastPlayerAngles )
		LastPlayerAngles = hDriver->pev->v_angle;

	if( !pCamera2 )
		SecondaryCamera = false;

	float MouseWaitON = CAR_CAMERA_MOUSE_WAIT_ON;
	float MouseWaitOFF = CAR_CAMERA_MOUSE_WAIT_OFF;
	if( SecondaryCamera )
	{
		MouseWaitON = 0.0f;
	//	MouseWaitOFF = 3.0f;
	}

	const float AbsCarSpeed = fabs( CarSpeed );
	const float ChassisAngleX = pChassis->GetAbsAngles().x;
	const float ChassisAngleY = pChassis->GetAbsAngles().y;

	// this is also used to stop free camera from snapping back on vehicles with no turret
	const bool bAimingView = (hDriver->pev->button & IN_ATTACK2);

	if( hDriver->pev->v_angle != LastPlayerAngles )
	{
		if( fMouseTouchedON < MouseWaitON )
			fMouseTouchedON += gpGlobals->frametime;

		if( fMouseTouchedON >= MouseWaitON )
		{
			fMouseTouchedOFF = 0.0f;
			if( !SecondaryCamera )
				CamUnlocked = true;
		}
	}
	else
	{
		if( fMouseTouchedOFF < MouseWaitOFF )
			fMouseTouchedOFF += gpGlobals->frametime;

		if( fMouseTouchedOFF >= MouseWaitOFF )
		{
			if( AbsCarSpeed > 200.0f )
			{
				fMouseTouchedON = 0.0f;
				CamUnlocked = false;
				Camera2RotationY = lerp( Camera2RotationY, 0.0f, 3.0f * gpGlobals->frametime );
				Camera2RotationX = lerp( Camera2RotationX, 0.0f, 3.0f * gpGlobals->frametime );
			}
		}
	}

	if( SecondaryCamera ) // first person or else
	{
		if( hDriver->pev->v_angle != LastPlayerAngles )
		{
			// get player's turn angle, see if it's changed
			float CurY = hDriver->pev->v_angle.y;
			float LastY = LastPlayerAngles.y;
			float CurX = hDriver->pev->v_angle.x;
			float LastX = LastPlayerAngles.x;
			// check the direction of the camera movement
			bool MoveRight = (CurY > LastY);
			bool MoveUp = (CurX > LastX);
			float add = 0.0f;
			if( MoveRight )
			{
				add = fabs( CurY - LastY ); // sometimes it equals to 359 so the check is needed
				if( add < 180 )
					Camera2RotationY += add;
			}
			else
			{
				add = fabs( CurY - LastY );
				if( add < 180 )
					Camera2RotationY -= add;
			}
			if( MoveUp )
			{
				add = fabs( CurX - LastX );
				if( add < 180 )
					Camera2RotationX += add;
			}
			else
			{
				add = fabs( CurX - LastX );
				if( add < 180 )
					Camera2RotationX -= add;
			}
		}
		Camera2RotationY = bound( -100, Camera2RotationY, 100 );
		Camera2RotationX = bound( -60, Camera2RotationX, 25 );
	}

	if( CamUnlocked )
	{
		CameraModeAddDist_Free = bound( 0.0f, CameraModeAddDist_Free, 200.0f );
		// lerp to the new position
		TmpCameraModeAddDist_Free = lerp( TmpCameraModeAddDist_Free, CameraModeAddDist_Free, 10.0f * gpGlobals->frametime );
	}
	else if( !SecondaryCamera )
	{
		CameraModeAddDist_Main = bound( 0.0f, CameraModeAddDist_Main, 100.0f );
		// lerp to the new position
		TmpCameraModeAddDist_Main = lerp( TmpCameraModeAddDist_Main, CameraModeAddDist_Main, 10.0f * gpGlobals->frametime );
	}

	if( !NewCameraAngleY )
		NewCameraAngleY = ChassisAngleY;

	if( !CamDistAdjust )
		CamDistAdjust = 1.0f;

	if( CarSpeed < -75 ) // going backwards
	{
		NewCameraAngleY = UTIL_ApproachAngle( ChassisAngleY - 180, NewCameraAngleY, 200 * gpGlobals->frametime ); // smooth it out
	}
	else if( CarSpeed > 25 || IsBoat || IsHeli )
	{
		if( AbsCarSpeed < MaxCarSpeed * 0.2f )
			NewCameraAngleY = UTIL_ApproachAngle( ChassisAngleY, NewCameraAngleY, 200 * gpGlobals->frametime );
		else
			NewCameraAngleY = ChassisAngleY;
	}

	const float anglediff = AngleDiff( NewCameraAngleY, CameraAngles.y );
	const float approach_speed = bound( 1.0f, AbsCarSpeed * 0.005f, 3.0f );

	Vector vForward, vRight;
	g_engfuncs.pfnAngleVectors( CameraAngles, vForward, vRight, NULL );
	CameraAngles.y = UTIL_ApproachAngle( NewCameraAngleY, CameraAngles.y, approach_speed * anglediff * gpGlobals->frametime );

	if( CarSpeed < -10 ) // going backwards
		vRight = -vRight;

	const float max_camera_lean = bound( 0.f, CarSpeed * 0.025f, 10.f );

	if( CarSpeed > 0.01f && (bBack() || bUp()) ) // braking
	{	
		if( bUp() && (IsHeli || IsBoat) ) // boats don't have handbrake
			CameraBrakeOffsetX = lerp( CameraBrakeOffsetX, 0, gpGlobals->frametime * 1.25f );
		else
			CameraBrakeOffsetX = lerp( CameraBrakeOffsetX, max_camera_lean, gpGlobals->frametime * 1.25f );
	}
	else
		CameraBrakeOffsetX = lerp( CameraBrakeOffsetX, 0, gpGlobals->frametime * 1.25f );

	// lean the camera view according to the chassis angle (except boats), make sure to compensate the added offsets too
	if( !IsBoat )
	{
		// NewCameraAngleX is being set in Drive()
		if( CarSpeed >= 0.0f )
			CameraAngles.x = lerp( CameraAngles.x, NewCameraAngleX, approach_speed * gpGlobals->frametime );
		else
			CameraAngles.x = lerp( CameraAngles.x, -NewCameraAngleX, approach_speed * gpGlobals->frametime );
	}

	TraceResult CamTr;

	if( pTankTower && bAimingView )
	{
		SET_VIEW( hDriver->edict(), pTankTower->edict() );
	}
	else if( (CamUnlocked || bAimingView) && pFreeCam )
	{
		SET_VIEW( hDriver->edict(), pFreeCam->edict() );
		TraceResult CamTr;
		Vector DriverAngles = hDriver->pev->v_angle;
		Vector CamOrg;

		UTIL_MakeVectors( DriverAngles );
		UTIL_TraceLine( hDriver->GetAbsOrigin(), hDriver->GetAbsOrigin() - gpGlobals->v_forward * (FreeCameraDistance + TmpCameraModeAddDist_Free), dont_ignore_monsters, dont_ignore_glass, edict(), &CamTr );
		CamOrg = CamTr.vecEndPos + CamTr.vecPlaneNormal * 10;
		pFreeCam->SetAbsOrigin( CamOrg );
		pFreeCam->SetAbsAngles( DriverAngles );
	}
	else
	{
		Vector CamOrg;
		float BackSway = CarSpeed * 0.01;

		if( SecondaryCamera )
		{
			SET_VIEW( hDriver->edict(), pCamera2->edict() );
			CamOrg = Camera2LocalOrigin;
			BackSway = bound( 0, BackSway, MaxCamera2Sway );
			CamOrg.x = Camera2LocalOrigin.x - BackSway;
			CamOrg.y = Camera2LocalOrigin.y + CameraMoving;
			pCamera2->SetLocalOrigin( CamOrg );
			pCamera2->SetLocalAngles( Camera2LocalAngles + Vector( CameraBrakeOffsetX + Camera2RotationX, Camera2RotationY, 0 ) );
		}
		else if( pCamera1 )
		{
			// just like in NFS MW, move camera closer if we don't press accelerate (car is slowing down)
			if( AbsCarSpeed > 50 )
			{
				if( CarSpeed > 50 && !bForward() )
				{
					if( bBack() ) // different lerp for brakes
						CamDistAdjust = lerp( CamDistAdjust, 0.65f, gpGlobals->frametime );
					else
						CamDistAdjust = UTIL_Approach( 0.65f, CamDistAdjust, 0.1f * gpGlobals->frametime );
				}
				else
				//	CamDistAdjust = UTIL_Approach( 1.0f, CamDistAdjust, 0.2f * gpGlobals->frametime );
					CamDistAdjust = lerp( CamDistAdjust, 1.0f, gpGlobals->frametime );
			}
			SET_VIEW( hDriver->edict(), pCamera1->edict() );
			UTIL_TraceLine( GetAbsOrigin() + gpGlobals->v_up * CameraHeight, GetAbsOrigin() + gpGlobals->v_up * CameraHeight - vForward * (CameraDistance + TmpCameraModeAddDist_Main) * CamDistAdjust, dont_ignore_monsters, dont_ignore_glass, edict(), &CamTr );
			CamOrg = CamTr.vecEndPos + CamTr.vecPlaneNormal * 10;
			pCamera1->SetAbsOrigin( CamOrg );
			pCamera1->SetAbsAngles( CameraAngles + Vector( CameraBrakeOffsetX, 0, 0 ) );
		}
	}

	// bring back the car vectors
	UTIL_MakeVectors( GetAbsAngles() );

	LastPlayerAngles = hDriver->pev->v_angle;
}

//===============================================================================
// stripped down copy-paste of "Drive()"
//===============================================================================
void CCar::Idle( void )
{
	pev->friction = 0.0f; // always set this, otherwise issues on different framerates

	// will need those later!
	UTIL_MakeVectors( pChassis->GetAbsAngles() );
	Vector ChassisForw = gpGlobals->v_forward;
	Vector ChassisUp = gpGlobals->v_up;
	Vector ChassisRight = gpGlobals->v_right;

	// now make the car's vectors
	UTIL_MakeVectors( GetAbsAngles() );

	const float AbsCarSpeed = fabs( CarSpeed );

	//----------------------------
	// wheels' origin (aka "suspension"...)
	// 1-2 front wheels, 3-4 rear wheels
	//----------------------------
	//----------------------------
	// wheels' origin (aka "suspension"...)
	// 1-2 front wheels, 3-4 rear wheels
	//----------------------------
	int FRW_InAir;
	int FLW_InAir;
	int RRW_InAir;
	int RLW_InAir;
	Wheels( &FRW_InAir, &FLW_InAir, &RRW_InAir, &RLW_InAir );
	int FrontWheelsInAir = FRW_InAir + FLW_InAir; // can't turn if 2
	int RearWheelsInAir = RRW_InAir + RLW_InAir; // can't accelerate if 2 // TODO: 4x4? Front drive?

	//----------------------------
	// turn
	//----------------------------
	
	// Forward turning controls if going backwards
	int Forward = 0;
	const Vector CarAng = GetLocalAngles();

	//----------------------------
	// chassis inclination
	//----------------------------
	int SuspDiff = fabs( RearWheelRadius - FrontWheelRadius );
	float SuspDiff2 = 1.0f;

	// RECHECK THIS IN GAME!!! Set bigger front or rear wheels
//	if( FrontWheelRadius < RearWheelRadius )
//		SuspDiff2 = (float)FrontWheelRadius / (float)RearWheelRadius;
//	else if( FrontWheelRadius > RearWheelRadius )
//		SuspDiff2 = (float)RearWheelRadius / (float)FrontWheelRadius;

	Vector ChassisAng = pChassis->GetLocalAngles();
	float CrossChange = ((pWheel1->GetAbsOrigin().z + pWheel2->GetAbsOrigin().z) * 0.5) - ((pWheel3->GetAbsOrigin().z + pWheel4->GetAbsOrigin().z - SuspDiff * 2) * 0.5);
	CrossChange *= 115.0f / (float)(FrontSuspDist + RearSuspDist); // !!! FIXME magic
	float LateralChange = ((pWheel1->GetAbsOrigin().z + pWheel3->GetAbsOrigin().z - SuspDiff) * 0.5) - ((pWheel2->GetAbsOrigin().z + pWheel4->GetAbsOrigin().z - SuspDiff) * 0.5);

	// X - transversal rotation
	float NewChassisAngX = -CrossChange * 0.5 * SuspDiff2;
	ChassisAng.x = lerp( ChassisAng.x, NewChassisAngX, SuspHardness * gpGlobals->frametime );
	ChassisAng.x = bound( -30, ChassisAng.x, 30 );

	// Z - lateral rotation
	float NewChassisAngZ = -LateralChange * SuspDiff2 + CarSpeed * Turning * (MaxLean * 0.001);
	ChassisAng.z = lerp( ChassisAng.z, NewChassisAngZ, SuspHardness * gpGlobals->frametime );
	ChassisAng.z = bound( -30, ChassisAng.z, 30 );

	if( FrontWheelsInAir + RearWheelsInAir == 4 )
	{
		ChassisAng.x = 0;
		ChassisAng.z = 0;
	}

	pChassis->SetLocalAngles( ChassisAng );

//	float MiddlePoint = (pWheel1->GetAbsOrigin().z + pWheel2->GetAbsOrigin().z + pWheel3->GetAbsOrigin().z + pWheel4->GetAbsOrigin().z) * 0.25;
//	Vector ChassisOrg = pChassis->GetAbsOrigin();
//	ChassisOrg.z = UTIL_Approach( MiddlePoint, ChassisOrg.z, SuspHardness * fabs( MiddlePoint - ChassisOrg.z ) * gpGlobals->frametime );
//	pChassis->SetAbsOrigin( ChassisOrg );
	Vector ChassisOrg = pChassis->GetAbsOrigin();
	Vector NewChassisOrg = (pWheel1->GetAbsOrigin() + pWheel2->GetAbsOrigin() + pWheel3->GetAbsOrigin() + pWheel4->GetAbsOrigin()) * 0.25;
	if( pev->deadflag == DEAD_DEAD )
		NewChassisOrg.z -= (FrontWheelRadius + RearWheelRadius) * 0.5f;
	ChassisOrg.x = UTIL_Approach( NewChassisOrg.x, ChassisOrg.x, SuspHardness * fabs( NewChassisOrg.x - ChassisOrg.x ) * gpGlobals->frametime );
	ChassisOrg.y = UTIL_Approach( NewChassisOrg.y, ChassisOrg.y, SuspHardness * fabs( NewChassisOrg.y - ChassisOrg.y ) * gpGlobals->frametime );
	ChassisOrg.z = UTIL_Approach( NewChassisOrg.z, ChassisOrg.z, SuspHardness * fabs( NewChassisOrg.z - ChassisOrg.z ) * gpGlobals->frametime );
	pChassis->SetAbsOrigin( ChassisOrg );

	//----------------------------
	// collision
	//----------------------------
	Vector Collision;
	Vector ColPoint;
	float ColDotProduct;

	GetCollision( AbsCarSpeed, Forward, &Collision, &ColDotProduct, &ColPoint );

	//----------------------------
	// accelerate/brake
	//----------------------------
	if( !Collision.IsNull() )
	{
		// can't accelerate if we are colliding
		float AbsDot = fabs( ColDotProduct );
		if( CarSpeed > 0 )
		{
			CarSpeed -= 5000 * AbsDot * gpGlobals->frametime;
			if( CarSpeed < 0 )
				CarSpeed = 0;
		}
		else if( CarSpeed < 0 )
		{
			CarSpeed += 5000 * AbsDot * gpGlobals->frametime;
			if( CarSpeed > 0 )
				CarSpeed = 0;
		}
	}
	else if( (FrontWheelsInAir + RearWheelsInAir == 4) || (FrontWheelsInAir == 2) )
	{
	//	CarSpeed += Forward * 200 * gpGlobals->frametime;
		CarSpeed += 200 * gpGlobals->frametime;
	}
	else if( RearWheelsInAir == 2 )
	{
		CarSpeed -= 200 * gpGlobals->frametime;
	}
	else
	{
		// no accelerate/break buttons pressed or all wheels in air
		// car slows down (aka friction...)
		CarSpeed = UTIL_Approach( 0, CarSpeed, 1000 * gpGlobals->frametime );
	}

	CarSpeed = bound( -MaxCarSpeedBackwards, CarSpeed, MaxCarSpeed );

	// apply main movement
	pev->velocity = gpGlobals->v_forward * CarSpeed;
	// check wheels - if one side is fully in air, push car in that direction
	if( FrontWheelsInAir + RearWheelsInAir < 4 )
	{
		if( FLW_InAir + RLW_InAir == 2 )
			pev->velocity += -gpGlobals->v_right * 200;
		else if( FRW_InAir + RRW_InAir == 2 )
			pev->velocity += gpGlobals->v_right * 200;
	}

	//----------------------------
	// weight...?
	//----------------------------

	if( FrontWheelsInAir + RearWheelsInAir == 4 )
	{
		if( !CarInAir )
		{
			CarInAir = true;
			DownForce = 0;
		}
	}

	if( FrontWheelsInAir + RearWheelsInAir == 0 && CarInAir )
	{
		CarInAir = false;
		if( DownForce < -150 )
		{
			EMIT_SOUND_DYN( edict(), CHAN_STATIC, "func_car/landing.wav", VOL_NORM, ATTN_NORM, 0, RANDOM_LONG( 90, 110 ) );
			CarSpeed *= 0.8;
		}
	}

	if( FrontWheelsInAir + RearWheelsInAir < 4 && FrontWheelsInAir + RearWheelsInAir > 0 )
	{
		DownForce = -100; // car weight * 0.1 maybe? (no such thing as car weight yet, just thoughts)
	}
	else if( FrontWheelsInAir + RearWheelsInAir == 4 )
	{
		DownForce -= 200 * 0.02;
		DownForce *= 1.05;
		if( DownForce < -2000 )
			DownForce = -2000;
	}
	else
	{
		float mult = fabs( ChassisAng.x );
		if( mult < 1 ) mult = 1; // -____-
		DownForce = -AbsCarSpeed * (0.02 * mult);
	}

	pev->velocity.z = DownForce;

	//----------------------------
	// timed effects
	//----------------------------

	// do timing stuff each 0.5 seconds
	if( (gpGlobals->time > time) && !Collision.IsNull() )
	{
		// apply collision effects
		if( !ColPoint.IsNull() )
		{
			UTIL_Sparks( ColPoint );
		}

		switch( RANDOM_LONG( 0, 3 ) )
		{
		case 0: EMIT_SOUND( edict(), CHAN_ITEM, "func_car/car_impact1.wav", 1, ATTN_NORM ); break;
		case 1: EMIT_SOUND( edict(), CHAN_ITEM, "func_car/car_impact2.wav", 1, ATTN_NORM ); break;
		case 2: EMIT_SOUND( edict(), CHAN_ITEM, "func_car/car_impact3.wav", 1, ATTN_NORM ); break;
		case 3: EMIT_SOUND( edict(), CHAN_ITEM, "func_car/car_impact4.wav", 1, ATTN_NORM ); break;
		}

		time = gpGlobals->time + 0.5;
	}

	//----------------------------
	// idle sound
	//----------------------------
	if( m_iszIdleSnd && !StartSilent )
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, STRING( m_iszIdleSnd ), 1, 2.2, SND_CHANGE_PITCH, PITCH_NORM );

	SetLocalAngles( CarAng ); // car direction is now set

	float thinktime = 0.0f;
	if( !g_pGameRules->IsMultiplayer() && (CarSpeed == 0.0f) && (pev->velocity.IsNull()) && (pev->flags & FL_ONGROUND) )
	{
		// don't think too often if player is far from the car, and it is idle
		CBaseEntity *pPlayer = CBaseEntity::Instance( INDEXENT( 1 ) );
		if( (GetAbsOrigin() - pPlayer->GetAbsOrigin()).Length() > 2000 )
			thinktime = 0.5;
	}

	if( HasSpawnFlags( SF_CAR_DOIDLEUNSTICK ) )
		TryUnstick();

	SetNextThink( thinktime );
}

void CCar::ClearEffects( void )
{
	DontThink();
	
	if( pWheel1 )
		UTIL_Remove( pWheel1, true );
	if( pWheel2 )
		UTIL_Remove( pWheel2, true );
	if( pWheel3 )
		UTIL_Remove( pWheel3, true );
	if( pWheel4 )
		UTIL_Remove( pWheel4, true );
	if( pChassis )
		UTIL_Remove( pChassis, true );
	if( pChassisMdl )
		UTIL_Remove( pChassisMdl, true );
	if( pCamera1 )
		UTIL_Remove( pCamera1, true );
	if( pCamera2 )
		UTIL_Remove( pCamera2, true );
	if( pFreeCam )
		UTIL_Remove( pFreeCam, true );
	if( pCarHurt )
		UTIL_Remove( pCarHurt, true );
	if( pDriverMdl )
		UTIL_Remove( pDriverMdl, true );
	if( pExhaust1 )
		UTIL_Remove( pExhaust1 );
	if( pExhaust2 )
		UTIL_Remove( pExhaust2 );
	if( pDoorHandle1 )
		UTIL_Remove( pDoorHandle1 );
	if( pDoorHandle2 )
		UTIL_Remove( pDoorHandle2 );

	pWheel1 = NULL;
	pWheel2 = NULL;
	pWheel3 = NULL;
	pWheel4 = NULL;
	pChassis = NULL;
	pChassisMdl = NULL;
	pCamera1 = NULL;
	pCamera2 = NULL;
	pFreeCam = NULL;
	pCarHurt = NULL;
	pDriverMdl = NULL;
	pExhaust1 = NULL;
	pExhaust2 = NULL;
	pDoorHandle1 = NULL;
	pDoorHandle2 = NULL;
}

void CCar::OnRemove(void)
{
	// if car is somehow deleted during drive, we need to remove the driver to prevent game crash
	if( hDriver != NULL && hDriver->IsPlayer() )
		Use( hDriver, hDriver, USE_OFF, 0 );
}

bool CCar::ExitCar( CBaseEntity *pPlayer )
{
	// set player position
	TraceResult TrExit;
	TraceResult VisCheck;
	UTIL_MakeVectors( pChassis->GetAbsAngles() );
	const Vector CarCenter = GetAbsOrigin() + gpGlobals->v_up * 46; // (human hull height / 2) + 10
	Vector vecSrc, vecEnd;

	const float iHeightCheck = 160.0f;
	Vector vPotentialPoint = g_vecZero;

	// check left side of the car first
	vecSrc = pWheel2->GetAbsOrigin() + gpGlobals->v_up * (38 - FrontWheelRadius) - gpGlobals->v_right * 50 - gpGlobals->v_forward * (FrontSuspDist + RearSuspDist) * 0.5;
	vecEnd = vecSrc;
	UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, human_hull, edict(), &TrExit );
	UTIL_TraceLine( CarCenter, vecSrc, dont_ignore_monsters, dont_ignore_glass, edict(), &VisCheck );
	if( (TrExit.flFraction == 1.0f) && !TrExit.fAllSolid && (VisCheck.flFraction == 1.0f) )
	{
		// mark this as a potential spot
		// now check if we fall down? 200 units
		vPotentialPoint = vecSrc;
		UTIL_TraceLine( vecSrc, vecSrc - Vector( 0, 0, iHeightCheck ), dont_ignore_monsters, dont_ignore_glass, edict(), &VisCheck );
		if( VisCheck.flFraction != 1.0f )
		{
			// we won't fall far down.
			pPlayer->SetAbsOrigin( vecSrc );
			return true;
		}
	}

	// can't spawn there, now check right side
	vecSrc = pWheel1->GetAbsOrigin() + gpGlobals->v_up * (38 - FrontWheelRadius) + gpGlobals->v_right * 50 - gpGlobals->v_forward * (FrontSuspDist + RearSuspDist) * 0.5;
	vecEnd = vecSrc;
	UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, human_hull, edict(), &TrExit );
	UTIL_TraceLine( CarCenter, vecSrc, dont_ignore_monsters, dont_ignore_glass, edict(), &VisCheck );
	if( (TrExit.flFraction == 1.0f) && !TrExit.fAllSolid && (VisCheck.flFraction == 1.0f) )
	{
		// mark this as a potential spot
		// now check if we fall down?
		vPotentialPoint = vecSrc;
		UTIL_TraceLine( vecSrc, vecSrc - Vector( 0, 0, iHeightCheck ), dont_ignore_monsters, dont_ignore_glass, edict(), &VisCheck );
		if( VisCheck.flFraction != 1.0f )
		{
			// we won't fall far down.
			pPlayer->SetAbsOrigin( vecSrc );
			return true;
		}
	}

	// can't spawn both, try front of the car
	vecSrc = CarCenter + gpGlobals->v_forward * (FrontSuspDist + FrontBumperLength + 50);
	vecEnd = vecSrc;
	UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, human_hull, edict(), &TrExit );
	UTIL_TraceLine( CarCenter, vecSrc, dont_ignore_monsters, dont_ignore_glass, edict(), &VisCheck );
	if( (TrExit.flFraction == 1.0f) && !TrExit.fAllSolid && (VisCheck.flFraction == 1.0f) )
	{
		// mark this as a potential spot
		// now check if we fall down?
		vPotentialPoint = vecSrc;
		UTIL_TraceLine( vecSrc, vecSrc - Vector( 0, 0, iHeightCheck ), dont_ignore_monsters, dont_ignore_glass, edict(), &VisCheck );
		if( VisCheck.flFraction != 1.0f )
		{
			// we won't fall far down.
			pPlayer->SetAbsOrigin( vecSrc );
			return true;
		}
	}

	// can't spawn, try back of the car
	vecSrc = CarCenter - gpGlobals->v_forward * (RearSuspDist + RearBumperLength + 50);
	vecEnd = vecSrc;
	UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, human_hull, edict(), &TrExit );
	UTIL_TraceLine( CarCenter, vecSrc, dont_ignore_monsters, dont_ignore_glass, edict(), &VisCheck );
	if( (TrExit.flFraction == 1.0f) && !TrExit.fAllSolid && (VisCheck.flFraction == 1.0f) )
	{
		// mark this as a potential spot
		// now check if we fall down?
		vPotentialPoint = vecSrc;
		UTIL_TraceLine( vecSrc, vecSrc - Vector( 0, 0, iHeightCheck ), dont_ignore_monsters, dont_ignore_glass, edict(), &VisCheck );
		if( VisCheck.flFraction != 1.0f )
		{
			// we won't fall far down.
			pPlayer->SetAbsOrigin( vecSrc );
			return true;
		}
	}

	if( vPotentialPoint != g_vecZero )
	{
		pPlayer->SetAbsOrigin( vPotentialPoint );
		return true;
	}

	// can't spawn anywhere.
	ALERT( at_error, "Can't exit the vehicle here!\n" );

	// last try...
	pPlayer->SetAbsOrigin( SafeSpawnPos );
	return true;
}

//========================================================================
// SafeSpawnPosition: collect a vector at the back of the car
// where you can exit for sure, if blocked
//========================================================================
Vector CCar::SafeSpawnPosition(void)
{
	if( gpGlobals->time < LastSafeSpawnCollectTime + 5 )
		return SafeSpawnPos;

	LastSafeSpawnCollectTime = gpGlobals->time;

	UTIL_MakeVectors( pChassis->GetAbsAngles() );
	const Vector CarCenter = GetAbsOrigin() + gpGlobals->v_up * 46; // (human hull height / 2) + 10
	Vector vecSrc, vecEnd;
	TraceResult TrExit, VisCheck;

	vecSrc = CarCenter - gpGlobals->v_forward * (RearSuspDist + RearBumperLength + 50);
	vecEnd = vecSrc;
	UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, human_hull, edict(), &TrExit );
	UTIL_TraceLine( CarCenter, vecSrc, dont_ignore_monsters, dont_ignore_glass, edict(), &VisCheck );
	if( (TrExit.flFraction == 1.0f) && !TrExit.fAllSolid && (VisCheck.flFraction == 1.0f) )
	{
		SafeSpawnPos = vecSrc;
		return true;
	}

	return SafeSpawnPos;
}

int CCar::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if( pev->deadflag == DEAD_DEAD )
		return 0;
	
	if( hDriver != NULL )
	{
		hDriver->PlayerCarDmgOverride = true;
		hDriver->TakeDamage( pevInflictor, pevAttacker, flDamage * DamageMult, bitsDamageType );
	}

	if( pev->health > 0 )
		pev->health -= flDamage;

	if( pev->health < 0 )
		CarExplode();

	return 0;
}

void CCar::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	UTIL_Sparks( ptr->vecEndPos );
	AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );
	switch( RANDOM_LONG( 0, 4 ) )
	{
	case 0: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit1.wav", 1, ATTN_NORM ); break;
	case 1: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit2.wav", 1, ATTN_NORM ); break;
	case 2: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit3.wav", 1, ATTN_NORM ); break;
	case 3: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit4.wav", 1, ATTN_NORM ); break;
	case 4: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit5.wav", 1, ATTN_NORM ); break;
	}
}

void CCar::CarExplode( void )
{
	pev->deadflag = DEAD_DEAD;

	if( pChassisMdl )
	{
		pChassisMdl->pev->rendermode = kRenderTransColor;
		pChassisMdl->pev->renderamt = 255;
		pChassisMdl->pev->rendercolor = Vector( 40, 40, 40 );
	}
	if( pWheel1 )
	{
		// diffusion - we can't remove the actual wheels, because they are used in the Idle code
		// so just remove the wheel models, which are attached to wheels
		if( pWheel1->m_hChild != NULL )
			UTIL_Remove( pWheel1->m_hChild );
	}
	if( pWheel2 )
	{
		if( pWheel2->m_hChild != NULL )
			UTIL_Remove( pWheel2->m_hChild );
	}
	if( pWheel3 )
	{
		if( pWheel3->m_hChild != NULL )
			UTIL_Remove( pWheel3->m_hChild );
	}
	if( pWheel4 )
	{
		if( pWheel4->m_hChild != NULL )
			UTIL_Remove( pWheel4->m_hChild );
	}
	
	const Vector vecOrigin = GetAbsOrigin();
	const Vector vecSpot = vecOrigin + (pev->mins + pev->maxs) * 0.5;

	// fireball
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSpot );
	WRITE_BYTE( TE_SPRITE );
	WRITE_COORD( vecSpot.x );
	WRITE_COORD( vecSpot.y );
	WRITE_COORD( vecSpot.z + 256 );
	WRITE_SHORT( m_iExplode );
	WRITE_BYTE( 60 ); // scale * 10
	WRITE_BYTE( 255 ); // brightness
	MESSAGE_END();

	// big smoke
	MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, vecSpot );
	WRITE_BYTE( TE_SMOKE );
	WRITE_COORD( vecSpot.x );
	WRITE_COORD( vecSpot.y );
	WRITE_COORD( vecSpot.z + 512 );
	WRITE_SHORT( g_sModelIndexSmoke );
	WRITE_BYTE( 250 ); // scale * 10
	WRITE_BYTE( 5 ); // framerate
	WRITE_BYTE( 10 ); // pos randomize
	MESSAGE_END();

	// blast circle
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecOrigin );
	WRITE_BYTE( TE_BEAMCYLINDER );
	WRITE_COORD( vecOrigin.x );
	WRITE_COORD( vecOrigin.y );
	WRITE_COORD( vecOrigin.z );
	WRITE_COORD( vecOrigin.x );
	WRITE_COORD( vecOrigin.y );
	WRITE_COORD( vecOrigin.z + 2000 ); // reach damage radius over .2 seconds
	WRITE_SHORT( m_iSpriteTexture );
	WRITE_BYTE( 0 ); // startframe
	WRITE_BYTE( 0 ); // framerate
	WRITE_BYTE( 4 ); // life
	WRITE_BYTE( 32 );  // width
	WRITE_BYTE( 0 );   // noise
	WRITE_BYTE( 255 );   // r, g, b
	WRITE_BYTE( 255 );   // r, g, b
	WRITE_BYTE( 192 );   // r, g, b
	WRITE_BYTE( 128 ); // brightness
	WRITE_BYTE( 0 );		// speed
	MESSAGE_END();

	EMIT_SOUND( ENT( pev ), CHAN_STATIC, "weapons/mortarhit.wav", 1.0, 0.3 );

	RadiusDamage( vecOrigin, pev, pev, 150, 300, CLASS_NONE, DMG_BLAST );

	if( hDriver != NULL ) // driver survived?!
		Use( hDriver, hDriver, USE_OFF, 0 );

	UTIL_ScreenShake( GetAbsOrigin(), 10.0, 150.0, 1.0, 1200, true );
}






//===============================================================================
// trigger zone which must wrap the car, it will be attached
// if monster or player touches it while car at speed, he will be hurt
//===============================================================================

class CCarHurt : public CBaseTrigger
{
	DECLARE_CLASS( CCarHurt, CBaseTrigger );
public:
	void Spawn( void );
	void CarHurtTouch( CBaseEntity *pOther );
	float dmgtime;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( trigger_carhurt, CCarHurt );

BEGIN_DATADESC( CCarHurt )
	DEFINE_FUNCTION( CarHurtTouch ),
END_DATADESC()

void CCarHurt::Spawn( void )
{
	InitTrigger();
	SetTouch( &CCarHurt::CarHurtTouch );
	dmgtime = 0;
	if( !pev->dmg )
		pev->dmg = 500;
}

void CCarHurt::CarHurtTouch( CBaseEntity *pOther )
{
	if( pev->frags == 0 )
		return; // car turned off

	if( pev->owner == NULL )
		return; // not ready (car must set the owner after spawn)

	if( !pOther )
		return;

	// I had to implement this, because monster changes his origin while checking local move
	// and this causes false touches of the trigger
	// the touch happens where the monster wants to go
	if( pOther->CheckingLocalMove )
		return;

	if( pOther->IsMonster() || (pOther->IsPlayer()) )
	{
		CCar *pCar = (CCar *)CBaseEntity::Instance( pev->owner );

		if( !pCar )
			return; // this shouldn't happen

		if( pOther->IsPlayer() )
		{
			CBasePlayer *pPlayer = (CBasePlayer *)pOther;
			if( pPlayer == pCar->hDriver )
				return;
		}

		Vector vel = pCar->GetAbsVelocity();

		if( (gpGlobals->time > dmgtime) && (vel.Length() > 50) )
		{
			pCar->CarSpeed *= 1 / (1 + (pCar->CarSpeed / pCar->MaxCarSpeed));
			pOther->TakeDamage( pCar->pev, pCar->hDriver->pev, fabs(pev->dmg * (pCar->CarSpeed / pCar->MaxCarSpeed)), DMG_CRUSH );
			UTIL_ScreenShakeLocal( pCar->hDriver, GetAbsOrigin(), vel.Length() * 0.01, 100, 1, 300, true );
			dmgtime = gpGlobals->time + 0.25;
			pOther->SetAbsVelocity( vel );
		}
	}
}

//===============================================================================
// a door handle - invisible point entity that can be pressed
// it's specified by name through car entity
// the car will automatically parent it to itself and set itself as the owner
//===============================================================================
class CCarDoorHandle : public CPointEntity
{
	DECLARE_CLASS( CCarDoorHandle, CPointEntity );
public:
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ){ return FCAP_IMPULSE_USE; }
};

LINK_ENTITY_TO_CLASS( car_door_handle, CCarDoorHandle );

void CCarDoorHandle::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( !pev->owner )
		return;
	
	CCar *pCar = (CCar *)CBaseEntity::Instance( pev->owner );

	if( !pCar )
		return;

	pCar->Use( pActivator, pCaller, useType, value );
}


//===============================================================================
// trigger_car_selfdrive
//===============================================================================
#define SELFDRIVE_DEBUG 0
class CFuncCarSelfdrive : public CBaseDelay
{
	DECLARE_CLASS( CFuncCarSelfdrive, CBaseDelay );
public:
	void Precache( void );
	void Spawn( void );
	void DriveThink( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	void FindCar( void );

	EHANDLE hCar;
	EHANDLE hRouteTarget;
	EHANDLE hRouteNextTarget;
	int speed_limit;

	// cache for saverestore
	bool b_set_car_params;
	bool b_set_initial_speed;
	float car_speed;

#if SELFDRIVE_DEBUG
	int m_iBeam;
#endif
	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( trigger_car_selfdrive, CFuncCarSelfdrive );

BEGIN_DATADESC( CFuncCarSelfdrive )
	DEFINE_FIELD( hCar, FIELD_EHANDLE ),
	DEFINE_FIELD( hRouteTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( hRouteNextTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( car_speed, FIELD_FLOAT ),
	DEFINE_FIELD( speed_limit, FIELD_INTEGER ),
	DEFINE_FUNCTION( FindCar ),
	DEFINE_FUNCTION( DriveThink ),
END_DATADESC()

void CFuncCarSelfdrive::Precache( void )
{
#if SELFDRIVE_DEBUG
	m_iBeam = PRECACHE_MODEL( "sprites/smoke.spr" );
#endif

	b_set_car_params = true;
}

void CFuncCarSelfdrive::Spawn( void )
{
#if SELFDRIVE_DEBUG
	Precache();
#endif
	m_iState = STATE_OFF;
	hRouteTarget = NULL;
	speed_limit = 0;
	if( pev->frags > 0 )
		speed_limit = pev->frags * 15; // km/h to units

	if( FStringNull( pev->netname ) )
	{
		ALERT( at_error, "trigger_car_selfdrive \"%s\" doesn't have route target. Removed.\n", STRING( pev->targetname ) );
		UTIL_Remove( this );
		return;
	}

	SetThink( &CFuncCarSelfdrive::FindCar );
	SetNextThink( 0.1f );
}

void CFuncCarSelfdrive::FindCar( void )
{
	hCar = (CCar *)UTIL_FindEntityByTargetname( NULL, STRING( pev->target ) );

	if( !hCar )
	{
		ALERT( at_error, "trigger_car_selfdrive \"%s\" can't find specified vehicle! Removed.\n", STRING( pev->targetname ) );
		UTIL_Remove( this );
		return;
	}

	if( !FClassnameIs( hCar, "func_car" ) )
	{
		ALERT( at_error, "trigger_car_selfdrive \"%s\": target entity is not a func_car. Removed.\n", STRING( pev->targetname ) );
		UTIL_Remove( this );
		return;
	}

	if( HasSpawnFlags( BIT( 0 ) ) )
	{
		SetThink( &CBaseEntity::SUB_CallUseToggle );
		SetNextThink( 1.0 ); // let the car spawn properly
	}
}

void CFuncCarSelfdrive::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( hCar == NULL )
		return;

	if( IsLockedByMaster() )
		return;

	CCar *pCar = (CCar*)(CBaseEntity*)pCar;

	if( m_iState == STATE_OFF )
	{
		hRouteTarget = UTIL_FindEntityByTargetname( NULL, STRING( pev->netname ) );
		if( !hRouteTarget )
		{
			ALERT( at_error, "trigger_car_selfdrive \"%s\" can't find route target \"%s\".\n", STRING( pev->targetname ), STRING( pev->netname ) );
			return;
		}

		// find the target afterwards, to be used later
		hRouteNextTarget = NULL;
		if( !FStringNull( hRouteTarget->pev->netname ) )
			hRouteNextTarget = UTIL_FindEntityByTargetname( NULL, STRING( hRouteTarget->pev->netname ) );

		if( pCar->hDriver != NULL && pCar->hDriver->IsPlayer() )
		{
			ALERT( at_warning, "trigger_car_selfdrive \"%s\" can't possess the car for it is occupied by player!\n", STRING( pev->targetname ) );
			return;
		}
		// possess the car
		m_iState = STATE_ON;
		pCar->ActivateSelfdrive();
		pCar->hDriver = this;

		speed_limit = 0;
		if( pev->frags > 0 )
			speed_limit = pev->frags * 15; // km/h to units

		if( pev->fuser1 > 0.0f )
			b_set_initial_speed = true;

		SetThink( &CFuncCarSelfdrive::DriveThink );
		SetNextThink( 0 );
	}
	else
	{
		m_iState = STATE_OFF;
		pev->button = 0;
		if( pCar )
		{
			pCar->DeactivateSelfdrive();
		}
		SetThink( NULL );
		DontThink();
	}
}

void CFuncCarSelfdrive::DriveThink( void )
{
	pev->button = 0;

	if( !hCar )
	{
		m_iState = STATE_OFF;
		SetThink( NULL );
		DontThink();
		return;
	}

	CCar *pCar = (CCar*)(CBaseEntity*)pCar;

	if( b_set_car_params )
	{
		pCar->CarSpeed = car_speed;
		b_set_car_params = false;
	}

	if( b_set_initial_speed )
	{
		pCar->CarSpeed = pev->fuser1 * 15.0f; // initial car speed
		b_set_initial_speed = false;
	}

	// assume always going forward
	float speed = fabs( pCar->CarSpeed );
	
	car_speed = speed;

	// find first target (in 2D space)
	Vector vCarOrg = pCar->GetAbsOrigin();
	vCarOrg.z = 0;
	Vector vTargetOrg = hRouteTarget->GetAbsOrigin();
	vTargetOrg.z = 0;

	const float cur_dist_to_t = (vTargetOrg - vCarOrg).Length2D();

	// turning
	Vector vec_to_target = vTargetOrg - vCarOrg;
	Vector ang_to_target = UTIL_VecToAngles( vec_to_target );
	UTIL_MakeVectors( ang_to_target );
	const Vector forward_desired = gpGlobals->v_forward;
	Vector ang_car = pCar->GetAbsAngles();
	ang_car.z = 0;
	UTIL_MakeVectors( ang_car );
	const Vector forward_current = gpGlobals->v_forward;
	const Vector right_current = gpGlobals->v_right;

#if SELFDRIVE_DEBUG
	// diffusion - for testing all this shit...
	Vector dbg_org = pCar->GetAbsOrigin();
	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, dbg_org );
	WRITE_BYTE( TE_BEAMPOINTS );
	WRITE_COORD( dbg_org.x );
	WRITE_COORD( dbg_org.y );
	WRITE_COORD( dbg_org.z );
	WRITE_COORD( (dbg_org + forward_desired * 250).x );
	WRITE_COORD( (dbg_org + forward_desired * 250).y );
	WRITE_COORD( (dbg_org + forward_desired * 250).z );
	WRITE_SHORT( m_iBeam );
	WRITE_BYTE( 0 ); // startframe
	WRITE_BYTE( 0 ); // framerate
	WRITE_BYTE( 1 ); // life

	WRITE_BYTE( 10 );  // width

	WRITE_BYTE( 0 );   // noise

	WRITE_BYTE( 0 );   // r
	WRITE_BYTE( 0 );   // g
	WRITE_BYTE( 255 );   // b

	WRITE_BYTE( 255 );	// brightness

	WRITE_BYTE( 0 );		// speed
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, dbg_org );
	WRITE_BYTE( TE_BEAMPOINTS );
	WRITE_COORD( dbg_org.x );
	WRITE_COORD( dbg_org.y );
	WRITE_COORD( dbg_org.z );
	WRITE_COORD( (dbg_org + forward_current * 250).x );
	WRITE_COORD( (dbg_org + forward_current * 250).y );
	WRITE_COORD( (dbg_org + forward_current * 250).z );
	WRITE_SHORT( m_iBeam );
	WRITE_BYTE( 0 ); // startframe
	WRITE_BYTE( 0 ); // framerate
	WRITE_BYTE( 1 ); // life

	WRITE_BYTE( 10 );  // width

	WRITE_BYTE( 0 );   // noise

	WRITE_BYTE( 0 );   // r
	WRITE_BYTE( 255 );   // g
	WRITE_BYTE( 0 );   // b

	WRITE_BYTE( 255 );	// brightness

	WRITE_BYTE( 0 );		// speed
	MESSAGE_END();
#endif

	float fTurnDir = 1.0f;
	const float dot_right = DotProduct( right_current, forward_desired );
	if( dot_right > 0.0f )
		fTurnDir = -1.0f;

	fTurnDir *= fabs( dot_right ) * 100.0f; // turn strength

	const float dot_forward = DotProduct( forward_current, forward_desired );
	const float abs_dot_forward = fabs( dot_forward );
	pCar->Turning = fTurnDir * (1.0f - dot_forward);

	// hit the gas unless told otherwise
	pev->button |= IN_FORWARD;

	if( speed_limit > 0 )
	{
		if( pCar->CarSpeed > speed_limit )
		{
			pev->button &= ~IN_FORWARD;
			pev->button |= IN_BACK;
		}
	}

	if( cur_dist_to_t < speed )
	{
		// get vector to next target ahead of schedule
		if( hRouteNextTarget )
		{
			Vector vNextTargetOrg = hRouteNextTarget->GetAbsOrigin();
			vNextTargetOrg.z = 0;
			Vector vec_to_next_target = vNextTargetOrg - vCarOrg;
			Vector ang_to_next_target = UTIL_VecToAngles( vec_to_next_target );
			UTIL_MakeVectors( ang_to_next_target );
			const Vector forward_to_next = gpGlobals->v_forward;
			const float dot_to_next = DotProduct( forward_current, forward_to_next );
			const float abs_dot_to_next = fabs( dot_to_next );

			if( abs_dot_to_next < 0.8f )
			{
				// bad angle, drop gas
				pev->button &= ~IN_FORWARD;

				// very bad, hit brakes
				if( abs_dot_to_next < 0.666f )
					pev->button |= IN_BACK;
				else
					pev->button &= ~IN_BACK;
			}
		}
		else // no next target, stop
		{
			pev->button &= ~IN_FORWARD;
			pev->button |= IN_BACK;
		}
	}

	int precision = 250;
	if( hRouteTarget->pev->sequence > 0 )
		precision = hRouteTarget->pev->sequence;

	if( cur_dist_to_t < precision ) // find next target
	{
		if( !FStringNull( hRouteTarget->pev->target ) )
			UTIL_FireTargets( hRouteTarget->pev->target, this, this, USE_TOGGLE, 0 );

		if( hRouteTarget->pev->frags > 0 )
			speed_limit = hRouteTarget->pev->frags * 15; // km/h to units
		else
			speed_limit = 0;

		if( FStringNull( hRouteTarget->pev->netname ) )
		{
			// don't turn off the car - only using trigger
			// (if you need to turn it off on last path, just specify it in the target field of the path)
			pev->button = 0;
			SetThink( NULL );
			DontThink();
			return;
		}
		else
		{
			hRouteTarget = UTIL_FindEntityByTargetname( NULL, STRING( hRouteTarget->pev->netname ) );
			if( !hRouteTarget )
			{
				pev->button = 0;
				SetThink( NULL );
				DontThink();
				return;
			}
			else
			{
				// find the target afterwards, to be used later
				hRouteNextTarget = NULL;
				if( !FStringNull( hRouteTarget->pev->netname ) )
					hRouteNextTarget = UTIL_FindEntityByTargetname( NULL, STRING( hRouteTarget->pev->netname ) );
			}
		}
	}

	SetNextThink( 0 );
}

//===============================================================================
// info_selfdrive_target - used for path
// target: activate target on pass
// netname: next path
// frags: set car max speed
//===============================================================================
class CInfoSelfdriveTarget : public CPointEntity
{
	DECLARE_CLASS( CInfoSelfdriveTarget, CPointEntity );
public:
	void Spawn( void );
};

LINK_ENTITY_TO_CLASS( info_selfdrive_target, CInfoSelfdriveTarget );

void CInfoSelfdriveTarget::Spawn( void )
{
	pev->solid = SOLID_NOT;
	SetBits( m_iFlags, MF_POINTENTITY );
}