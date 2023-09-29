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

#define SF_CAR_ONLYTRIGGER		BIT(0) // only trigger or external button
#define SF_CAR_TURBO			BIT(1) // turbo sound (*pfsshhh...*)
#define SF_CAR_STARTSILENT		BIT(2) // StartSilent = true
#define SF_CAR_DRIFTMODE		BIT(3) // DriftMode = true
#define SF_CAR_GEARWHINE		BIT(4) // whining transmission sound, like a race car (hello NFS MW)
#define SF_CAR_ELECTRIC			BIT(5) // for now this only makes different sounds

BEGIN_DATADESC( CCar )
	DEFINE_KEYFIELD( wheel1, FIELD_STRING, "wheel1" ),
	DEFINE_KEYFIELD( wheel2, FIELD_STRING, "wheel2" ),
	DEFINE_KEYFIELD( wheel3, FIELD_STRING, "wheel3" ),
	DEFINE_KEYFIELD( wheel4, FIELD_STRING, "wheel4" ),
	DEFINE_KEYFIELD( chassis, FIELD_STRING, "chassis" ),
	DEFINE_KEYFIELD( carhurt, FIELD_STRING, "carhurt" ),
	DEFINE_KEYFIELD( camera1, FIELD_STRING, "camera1" ),
	DEFINE_KEYFIELD( camera2, FIELD_STRING, "camera2" ),
	DEFINE_KEYFIELD( tank_tower, FIELD_STRING, "tank_tower" ),
	DEFINE_FIELD( Camera1LocalOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( Camera2LocalOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( Camera1LocalAngles, FIELD_VECTOR ),
	DEFINE_FIELD( Camera2LocalAngles, FIELD_VECTOR ),
	DEFINE_KEYFIELD( MaxCamera1Sway, FIELD_INTEGER, "maxcam1sway"),
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
	DEFINE_FIELD( hDriver, FIELD_EHANDLE ),
	DEFINE_FUNCTION( Setup ),
	DEFINE_FUNCTION( Drive ),
	DEFINE_FUNCTION( Idle ),
	DEFINE_FIELD( pWheel1, FIELD_CLASSPTR ),
	DEFINE_FIELD( pWheel2, FIELD_CLASSPTR ),
	DEFINE_FIELD( pWheel3, FIELD_CLASSPTR ),
	DEFINE_FIELD( pWheel4, FIELD_CLASSPTR ),
	DEFINE_FIELD( pChassis, FIELD_CLASSPTR ),
	DEFINE_FIELD( pCamera1, FIELD_CLASSPTR ),
	DEFINE_FIELD( pCamera2, FIELD_CLASSPTR ),
	DEFINE_FIELD( pFreeCam, FIELD_CLASSPTR ),
	DEFINE_FIELD( pCarHurt, FIELD_CLASSPTR ),
	DEFINE_FIELD( pDriverMdl, FIELD_CLASSPTR ),
	DEFINE_FIELD( pChassisMdl, FIELD_CLASSPTR ),
	DEFINE_FIELD( pTankTower, FIELD_CLASSPTR ),
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

int CCar::ObjectCaps( void )
{
	if( HasSpawnFlags( SF_CAR_ONLYTRIGGER ) )
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
	else if( FStrEq( pkvd->szKeyName, "camera1" ) )
	{
		camera1 = ALLOC_STRING( pkvd->szValue );
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
	else if( FStrEq( pkvd->szKeyName, "maxcam1sway" ) )
	{
		MaxCamera1Sway = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "maxcam2sway" ) )
	{
		MaxCamera2Sway = Q_atoi( pkvd->szValue );
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
	else if( FStrEq( pkvd->szKeyName, "shiftingtime" ) )
	{
		ShiftingTime = Q_atof( pkvd->szValue );
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
	PRECACHE_SOUND( "func_car/gear1.wav" );
	PRECACHE_SOUND( "func_car/gear2.wav" );
	PRECACHE_SOUND( "func_car/gear3.wav" );
	if( HasSpawnFlags(SF_CAR_GEARWHINE ))
		PRECACHE_SOUND( "func_car/gear_whine.wav" );
	if( HasSpawnFlags( SF_CAR_TURBO ) )
		PRECACHE_SOUND( "func_car/turbo.wav" );

	for( int i = 0; i < SIZEOFARRAY( pTireSounds ); i++ )
		PRECACHE_SOUND( (char *)pTireSounds[i] );;

	PRECACHE_SOUND( "drone/drone_hit1.wav" );
	PRECACHE_SOUND( "drone/drone_hit2.wav" );
	PRECACHE_SOUND( "drone/drone_hit3.wav" );
	PRECACHE_SOUND( "drone/drone_hit4.wav" );
	PRECACHE_SOUND( "drone/drone_hit5.wav" );

	m_iSpriteTexture = PRECACHE_MODEL( "sprites/white.spr" );
	m_iExplode = PRECACHE_MODEL( "sprites/fexplo.spr" );
	PRECACHE_SOUND( "weapons/mortarhit.wav" );

	UTIL_PrecacheOther( "apc_projectile" );
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
	if( !SuspHardness )
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

	ShiftStartTime = 0;
	IsShifting = false;

	// convert km/h to units
	MaxCarSpeed = MaxCarSpeed * 15;
	MaxCarSpeedBackwards = MaxCarSpeedBackwards * 15;
	GearStep = MaxCarSpeed / MaxGears;
	TurboAccum = 0.0f;
	CameraBrakeOffsetX = 0.0f;

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
		DriftMode = true;

	SafeSpawnPos = g_vecZero;

	SetThink( &CCar::Setup );
	SetNextThink( RANDOM_FLOAT( 0.1f, 0.2f ) );
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
			SET_VIEW( pPlayer->edict(), pPlayer->edict() );

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
			// clear particles (speed is stored here)
			pWheel1->pev->fuser1 = 0;
			pWheel2->pev->fuser1 = 0;
			pWheel3->pev->fuser1 = 0;
			pWheel4->pev->fuser1 = 0;

			IsShifting = false;
			ShiftStartTime = 0;

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
			IsShifting = false;
			ShiftStartTime = 0;

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
	hackz.z += 1;
	SetAbsOrigin( hackz );

	// reset chassis...
	if( pChassis )
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
	pCamera1 = UTIL_FindEntityByTargetname( NULL, STRING( camera1 ) );
	pCamera2 = UTIL_FindEntityByTargetname( NULL, STRING( camera2 ) );
	pTankTower = UTIL_FindEntityByTargetname( NULL, STRING( tank_tower ) );

	if( !pWheel1 || !pWheel2 || !pWheel3 || !pWheel4 )
	{
		ALERT( at_error, "CAR WITH NO WHEELS! Must be 4 wheels present.\n" );
		UTIL_Remove( this );
		return;
	}

	if( pChassisMdl )
	{
		pChassisMdl->pev->owner = edict();
		UTIL_SetSize( pChassisMdl->pev, Vector( -16, -16, 0 ), Vector( 16,16,16 ) );
		pChassisMdl->pev->takedamage = DAMAGE_YES;
		pChassisMdl->pev->spawnflags |= BIT( 31 ); // SF_ENVMODEL_OWNERDAMAGE
		pChassisMdl->RelinkEntity( FALSE );
	}

	if( pWheel1 )
	{
		pWheel1->SetParent( pChassis );
		pWheel1->pev->iuser2 = FrontWheelRadius;
	}
	if( pWheel2 )
	{
		pWheel2->SetParent( pChassis );
		pWheel2->pev->iuser2 = FrontWheelRadius;
	}
	if( pWheel3 )
	{
		pWheel3->SetParent( pChassis );
		pWheel3->pev->iuser2 = RearWheelRadius;
	}
	if( pWheel4 )
	{
		pWheel4->SetParent( pChassis );
		pWheel4->pev->iuser2 = RearWheelRadius;
	}

	if( pCamera1 )
	{
		if( !pCamera1->m_iParent )
			pCamera1->SetParent( this );
		pCamera1->SetNullModel();
		Camera1LocalOrigin = pCamera1->GetLocalOrigin();
		Camera1LocalAngles = pCamera1->GetLocalAngles();
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
			pDriverMdl->SetParent( pChassis );
		else
			pDriverMdl->SetParent( this );
		pDriverMdl->pev->effects |= EF_NODRAW;
	}

	if( pTankTower )
	{
		if( pChassisMdl )
			pTankTower->SetParent( pChassisMdl );
		else if( pChassis )
			pTankTower->SetParent( pChassis );
		else
			pTankTower->SetParent( this );
	}

	pFreeCam = CBaseEntity::Create( "info_target", GetAbsOrigin(), GetAbsAngles(), edict() );
	if( pFreeCam )
	{
		pFreeCam->SetNullModel();
		pFreeCam->SetParent( this );
		pFreeCam->pev->iuser1 = 0;
		if( pChassisMdl )
		{
			// get camera distance according to model bounds
			Vector mins = g_vecZero;
			Vector maxs = g_vecZero;
			UTIL_GetModelBounds( pChassisMdl->pev->modelindex, mins, maxs );
			pFreeCam->pev->iuser1 = (int)( (mins - maxs).Length() * 0.5f * pChassisMdl->pev->scale );
		}
		if( !pCamera1 )
			Camera1LocalAngles = pFreeCam->GetLocalAngles();
	}

	if( pev->iuser1 ) // rotate car
	{
		Vector ang = GetAbsAngles();
		ang.y += pev->iuser1;
		SetAbsAngles( ang );
	}

	pev->flags &= ~FL_ONGROUND; // !!! this resets position so the vehicle won't hang in air...

	SetThink( &CCar::Idle );
	SetNextThink( RANDOM_FLOAT( 0.2f, 0.5f ) );
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
			strcpy( szBuffer, pTextureName );
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
				break;
			case CHAR_TEX_GRAVEL:
				*ChassisShake = 5.0f;
				soundtype = 3;
				handbrakesoundtype = 7;
				*surf_Mult = surf_GravelMult;
				Particle = true;
				Type = 1;
				break;
			case CHAR_TEX_SNOW:
				*ChassisShake = 2.0f;
				soundtype = 4;
				handbrakesoundtype = 7;
				*surf_Mult = surf_SnowMult;
				// UNDONE PARTICLES
				break;
			case CHAR_TEX_DIRT:
				*ChassisShake = 4.0f;
				soundtype = 1;
				handbrakesoundtype = 7;
				*surf_Mult = surf_DirtMult;
				Particle = true;
				Type = 1;
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

	float tirevolume = AbsCarSpeed * 0.001 * 0.5f;
	int tirepitch = 60 + ((AbsCarSpeed - wheelnum) * 0.03) + (2 - wheelnum); // move the pitch slightly, using wheelnum :)

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
		tirepitch += 120;
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
		EMIT_SOUND_DYN( pWheelX->edict(), CHAN_ITEM, pTireSounds[soundtype], tirevolume, ATTN_NORM, SND_CHANGE_VOL | SND_CHANGE_PITCH, tirepitch );
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
	bool IsDrifting = false;

	float AbsCarSpeed = fabs( CarSpeed );

	// CAR STUCK???
	if( StuckTime > gpGlobals->time )
		StuckTime = gpGlobals->time;
	if( (hDriver->pev->button & IN_RELOAD) && (gpGlobals->time > StuckTime + 5) )
	{
		TryUnstick();
		StuckTime = gpGlobals->time;
	}

	// will need those later!
	UTIL_MakeVectors( pChassis->GetAbsAngles() );
	Vector ChassisForw = gpGlobals->v_forward;
	Vector ChassisUp = gpGlobals->v_up;
	Vector ChassisRight = gpGlobals->v_right;
	Vector DriftForw = g_vecZero;

	if( DriftMode )
	{
		float ChassisAbsAngY = pChassis->pev->angles.y;
		float anglediff = fabs( ChassisAbsAngY - DriftAngles.y );
		if( (ChassisAbsAngY > 0 && DriftAngles.y < 0) )
		{
			DriftAngles.y = DriftAngles.y + 360;
			if( anglediff > 180 )
				DriftAngles.y += 360;
			if( DriftAngles.y > 180 )
				DriftAngles.y -= 360;
		}
		else if( (ChassisAbsAngY < 0 && DriftAngles.y > 0) )
		{
			DriftAngles.y = DriftAngles.y - 360;
			if( anglediff > 180 )
				DriftAngles.y -= 360;
			if( DriftAngles.y < -180 )
				DriftAngles.y += 360;
		}

		float drift_recovery_speed = (MaxCarSpeed * 3) / (1 + AbsCarSpeed); // stability
		drift_recovery_speed = bound( 10.0f, drift_recovery_speed, 500.0f );
		if( bForward() )
			drift_recovery_speed *= 0.25f;
		if( AbsCarSpeed <= 0.1f )
			DriftAngles.y = ChassisAbsAngY;
		else
			DriftAngles.y = UTIL_Approach( ChassisAbsAngY, DriftAngles.y, drift_recovery_speed * gpGlobals->frametime );

		UTIL_MakeVectors( DriftAngles );
		DriftForw = gpGlobals->v_forward;
		float dot = DotProduct( DriftForw, ChassisForw );
		DriftAmount = 1 / bound( 0.05, dot, 1 ); // 1 to 20
		ALERT( at_console, "recovery %f, amt %f, dot %f\n", drift_recovery_speed, DriftAmount, dot );
	}
	else
		DriftAmount = 1.0f;
	
	// only NOW make the car's vectors
	UTIL_MakeVectors( GetAbsAngles() );

	// TEMP !!! (or not.)
	Vector PlayerPos = GetAbsOrigin() + gpGlobals->v_up * 40;
//	if( pDriverMdl )
//		PlayerPos = pDriverMdl->GetAbsOrigin();
	hDriver->SetAbsOrigin( PlayerPos );
	hDriver->SetAbsVelocity( g_vecZero );

	// motion blur
	hDriver->pev->vuser1.y = AbsCarSpeed;
	
	// achievement
	float Distance = 0.0f;
	Distance = (GetAbsOrigin() - PrevOrigin).Length();
	Distance *= 0.01905f; // convert units to meters
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
	TraceResult tr;

	pWheel1Org = GetAbsOrigin() + ChassisForw * FrontSuspDist + gpGlobals->v_right * (FrontSuspWidth * 0.5);
	pWheel2Org = GetAbsOrigin() + ChassisForw * FrontSuspDist- gpGlobals->v_right * (FrontSuspWidth * 0.5);
	pWheel3Org = GetAbsOrigin() - ChassisForw * RearSuspDist + gpGlobals->v_right * (RearSuspWidth * 0.5);
	pWheel4Org = GetAbsOrigin() - ChassisForw * RearSuspDist - gpGlobals->v_right * (RearSuspWidth * 0.5);
	Vector pWheel1NewOrg = g_vecZero;
	Vector pWheel2NewOrg = g_vecZero;
	Vector pWheel3NewOrg = g_vecZero;
	Vector pWheel4NewOrg = g_vecZero;
	Vector pWheelOrgSet = g_vecZero;

	int FRW_InAir = 0;
	int FLW_InAir = 0;
	int RRW_InAir = 0;
	int RLW_InAir = 0;

	// front right wheel
	// ChassisUp * 40: need to make tracing start high to work properly on slopes !!!
	float SlopeAdjust = 40.0f;
	UTIL_TraceLine( pWheel1Org + ChassisUp * SlopeAdjust, pWheel1Org - ChassisUp * FrontWheelRadius * 2, ignore_monsters, dont_ignore_glass, pWheel1->edict(), &tr );
	pWheel1NewOrg = tr.vecEndPos + ChassisUp * FrontWheelRadius;

	if( tr.flFraction == 1.0f && (int)(pWheel1NewOrg - pWheel1Org).Length() >= FrontWheelRadius - 1 )
	{
		pWheel1NewOrg = pWheel1Org - ChassisUp * FrontWheelRadius;
		FRW_InAir = 1;
	}

	// front left wheel
	UTIL_TraceLine( pWheel2Org + ChassisUp * SlopeAdjust, pWheel2Org - ChassisUp * FrontWheelRadius * 2, ignore_monsters, dont_ignore_glass, pWheel2->edict(), &tr );
	pWheel2NewOrg = tr.vecEndPos + ChassisUp * FrontWheelRadius;
	if( tr.flFraction == 1.0f && (int)(pWheel2NewOrg - pWheel2Org).Length() >= FrontWheelRadius - 1 )
	{
		pWheel2NewOrg = pWheel2Org - ChassisUp * FrontWheelRadius;
		FLW_InAir = 1;
	}

	// rear right wheel
	UTIL_TraceLine( pWheel3Org + ChassisUp * SlopeAdjust, pWheel3Org - ChassisUp * RearWheelRadius * 2, ignore_monsters, dont_ignore_glass, pWheel3->edict(), &tr );
	pWheel3NewOrg = tr.vecEndPos + ChassisUp * RearWheelRadius;
	if( tr.flFraction == 1.0f && (int)(pWheel3NewOrg - pWheel3Org).Length() >= RearWheelRadius - 1 )
	{
		pWheel3NewOrg = pWheel3Org - ChassisUp * RearWheelRadius;
		RRW_InAir = 1;
	}

	// rear left wheel
	UTIL_TraceLine( pWheel4Org + ChassisUp * SlopeAdjust, pWheel4Org - ChassisUp * RearWheelRadius * 2, ignore_monsters, dont_ignore_glass, pWheel4->edict(), &tr );
	pWheel4NewOrg = tr.vecEndPos + ChassisUp * RearWheelRadius;
	if( tr.flFraction == 1.0f && (int)(pWheel4NewOrg - pWheel4Org).Length() >= RearWheelRadius - 1 )
	{
		pWheel4NewOrg = pWheel4Org - ChassisUp * RearWheelRadius;
		RLW_InAir = 1;
	}

	int FrontWheelsInAir = FRW_InAir + FLW_InAir; // can't turn if 2
	int RearWheelsInAir = RRW_InAir + RLW_InAir; // can't accelerate if 2 // TODO: 4x4? Front drive?

	pWheel1->SetAbsOrigin( pWheel1NewOrg );
	pWheel2->SetAbsOrigin( pWheel2NewOrg );
	pWheel3->SetAbsOrigin( pWheel3NewOrg );
	pWheel4->SetAbsOrigin( pWheel4NewOrg );

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
	int CameraSwayRate = MaxCamera1Sway;
	if( SecondaryCamera )
		CameraSwayRate = MaxCamera2Sway;

	if( bRight() )
	{
		Turning -= TurnRate * gpGlobals->frametime;
		CameraMoving -= CameraSwayRate * gpGlobals->frametime;
	}
	else if( bLeft() )
	{
		Turning += TurnRate * gpGlobals->frametime;
		CameraMoving += CameraSwayRate * gpGlobals->frametime;
	}

	// no turning buttons pressed, go to zero slowly
	if( (!bLeft() && !bRight()) )
	{
		Turning = UTIL_Approach( 0, Turning, fabs( TurnRate * 0.75 ) * gpGlobals->frametime );
		CameraMoving = UTIL_Approach( 0, CameraMoving, CameraSwayRate * 0.5 * gpGlobals->frametime );

		if( fabs( Turning ) <= 0.001f )
			Turning = 0;
	}

	float max_turn_val = 1.0f / (0.1f + AbsCarSpeed * 0.0075f); // magic
	if( max_turn_val > 1.0f )
		max_turn_val = 1.0f;
	Turning = bound( -max_turn_val, Turning, max_turn_val );

	int CameraMovingBound = MaxCamera1Sway * DriftAmount * DriftAmount;
	if( SecondaryCamera )
		CameraMovingBound = MaxCamera2Sway;

	CameraMoving = bound( -CameraMovingBound, CameraMoving, CameraMovingBound );

	float CarTurnRate = (AbsCarSpeed * 0.00075) + (0.005f * HeatingMult);
	if( 0 )//DriftMode && AbsCarSpeed > 10 )
	{
		CarTurnRate *= DriftAmount;
		if( bForward() )
			CarTurnRate *= 0.75f * DriftAmount;
	}
	CarTurnRate = bound( 0, CarTurnRate, 1 );
	
	// handbrake pressed
	if( bUp() )
	{
		CarTurnRate *= bound(1.0f, AbsCarSpeed * 0.002, 1.5f);
	}

	Vector CarAng = GetLocalAngles();
	
	//	Y - turn left and right
	float trn = (10 * MaxTurn) * (Turning * CarTurnRate) * gpGlobals->frametime;
	if( FrontWheelsInAir == 2 )
		trn = 0;
	CarAng.y += trn;

	CollisionAddTurn = bound( -5.0f, CollisionAddTurn, 5.0f );
	if( CollisionAddTurn != 0.0f )
		CollisionAddTurn = UTIL_Approach( 0.0f, CollisionAddTurn, 20 * gpGlobals->frametime );
	CarAng.y += CollisionAddTurn;

	//----------------------------
	// wheels' turning (visual effect)
	//----------------------------
	// get one front wheel which rotates and turn, the second will repeat
	Vector FrontWheelAng = pWheel1->GetLocalAngles();
	// get one rear wheel which only rotates, the second will repeat
	Vector RearWheelAng = pWheel3->GetLocalAngles();

	float ApproachSpeed = bound( 200, AbsCarSpeed, 400 );
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

	/* old code
	OpposeTurn = 1.0f;
	if( bRight() )
	{
		if( Forward == -1 )
		{
			if( Turning > 0 ) OpposeTurn = (TurningRate * 3) / (0.1 + AbsCarSpeed);
		}
		else if( Forward == 1 )
		{
			if( Turning < 0 ) OpposeTurn = (TurningRate * 3) / (0.1 + AbsCarSpeed);
		}
		OpposeTurn = bound( 1, OpposeTurn, 10 );
		FrontWheelAng.y -= 50 * OpposeTurn * gpGlobals->frametime;
	}
	else if( bLeft() )
	{
		if( Forward == -1 )
		{
			if( Turning < 0 ) OpposeTurn = (TurningRate * 3) / (0.1 + AbsCarSpeed);
		}
		else if( Forward == 1 )
		{
			if( Turning > 0 ) OpposeTurn = (TurningRate * 3) / (0.1 + AbsCarSpeed);
		}
		OpposeTurn = bound( 1, OpposeTurn, 10 );
		FrontWheelAng.y += 50 * OpposeTurn * gpGlobals->frametime;
	}

	// no turning buttons pressed, go to zero
	if( !(bLeft()) && !(bRight()) )
	{
		FrontWheelAng.y = UTIL_Approach( 0, FrontWheelAng.y, 100 * gpGlobals->frametime );
		if( fabs( FrontWheelAng.y ) <= 0.01f )
			FrontWheelAng.y = 0;
	}

	float MaxWheelTurn = MaxTurn * (150 / (1 + AbsCarSpeed));
	if( MaxWheelTurn > MaxTurn ) MaxWheelTurn = MaxTurn;
	FrontWheelAng.y = bound( -MaxWheelTurn, FrontWheelAng.y, MaxWheelTurn );
	*/

	//----------------------------
	// wheels' rotation (visual effect)
	//----------------------------
	// ??? should I include wheel radius here somehow?
	if( HeatingTires )
	{
		HeatingMult = UTIL_Approach( 7.0f, HeatingMult, 2.5f * gpGlobals->frametime );
		RearWheelAng.x += 100.0f * HeatingMult * (M_PI * 0.5) * gpGlobals->frametime; // 0.5 ??? so...pi/2?
	}
	else if( !(bUp()) )
	{
		FrontWheelAng.x += Forward * AbsCarSpeed * (M_PI * 0.5) * gpGlobals->frametime;
		RearWheelAng.x += Forward * AbsCarSpeed * (M_PI * 0.5) * gpGlobals->frametime * (1 + HeatingMult);
	}

	if( FrontWheelAng.x > 359 || FrontWheelAng.x < -359 )
		FrontWheelAng.x = 0;

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

	ChassisShake = (ChassisShake1 + ChassisShake2 + ChassisShake3 + ChassisShake4) * 0.25f;
	surf_CurrentMult = (surf_Mult1 + surf_Mult2 + surf_Mult3 + surf_Mult4) * 0.25f;

	// add turnwheel wobbling :)
	float NewTurnWheelShake = RANDOM_FLOAT( -ChassisShake, ChassisShake ) * AbsCarSpeed * 0.01f;
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

	if( CarSpeed <= 0 || (CarSpeed > 0 && !bBack() && !bUp()) ) // do not mess with gears during braking
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
					CanPlayTurboSound = true;
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

		switch( RANDOM_LONG( 1, 3 ) )
		{
		case 1: EMIT_SOUND( edict(), CHAN_BODY, "func_car/gear1.wav", 0.35, ATTN_NORM ); break;
		case 2: EMIT_SOUND( edict(), CHAN_BODY, "func_car/gear2.wav", 0.35, ATTN_NORM ); break;
		case 3: EMIT_SOUND( edict(), CHAN_BODY, "func_car/gear3.wav", 0.35, ATTN_NORM ); break;
		}

		if( Upshifting && bForward() ) // shake body only when upshifting and gas pressed
			AccelAddX -= 1;
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
			EngPitch -= (MaxCarSpeed / (1 + AbsCarSpeed * 0.1)) * gpGlobals->frametime;
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
				EngPitch = 80 + 150.0f * ((AbsCarSpeed / (float)(GearStep * (Gear < 1 ? 1 : Gear)))) - (Gear * 2) + (5 * HeatingMult);
			}
		}
	}

	// add "sound shake" on some surfaces
	EngPitch += RANDOM_FLOAT( -ChassisShake * SuspHardness * AbsCarSpeed * 0.0002, ChassisShake * SuspHardness * AbsCarSpeed * 0.0002 );
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
	TraceResult trCol;
	Vector Collision = g_vecZero;
	Vector ColPoint = g_vecZero; // place for sparks
	Vector ColPointLeft = g_vecZero;
	Vector ColPointRight = g_vecZero;

	// 4 collision points from wheels.
	Vector ColPointWFR = g_vecZero;
	Vector ColPointWFL = g_vecZero;
	Vector ColPointWRR = g_vecZero;
	Vector ColPointWRL = g_vecZero;

	float ColDotProduct = 1.0f;

	bool hit_carblocker = false;

	if( Forward == 1 || (FrontWheelsInAir + RearWheelsInAir < 3 && Forward == -1) )
	{
		// front right wheel
		UTIL_TraceLine( GetAbsOrigin() + ChassisUp * FrontWheelRadius * 1.5 + ChassisRight * FrontSuspWidth * 0.5, GetAbsOrigin() + ChassisUp * FrontWheelRadius * 1.5 + ChassisRight * FrontSuspWidth * 0.5 + ChassisForw * (FrontSuspDist + FrontBumperLength), ignore_monsters, edict(), &trCol );
		ColPointWFR = trCol.vecEndPos;
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_CARBLOCKER )
			hit_carblocker = true;
		if( trCol.flFraction < 1.0f )
		{
			ColDotProduct = -DotProduct( ChassisForw, trCol.vecPlaneNormal ) * Forward;
			Collision += -gpGlobals->v_forward * CarSpeed * Forward + trCol.vecPlaneNormal * CarSpeed * ColDotProduct;
			ColPoint = ColPointWFR;
			if( ColDotProduct < 0.6 )
				ColDotProduct *= -1;
		}
		else if( hit_carblocker )
		{
			Collision += -gpGlobals->v_forward * CarSpeed * Forward;
			ColPoint = ColPointWFR;
		}
		hit_carblocker = false;

		// front left wheel
		UTIL_TraceLine( GetAbsOrigin() + ChassisUp * FrontWheelRadius * 1.5 - ChassisRight * FrontSuspWidth * 0.5, GetAbsOrigin() + ChassisUp * FrontWheelRadius * 1.5 - ChassisRight * FrontSuspWidth * 0.5 + ChassisForw * (FrontSuspDist + FrontBumperLength), ignore_monsters, edict(), &trCol );
		ColPointWFL = trCol.vecEndPos;
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_CARBLOCKER )
			hit_carblocker = true;
		if( trCol.flFraction < 1.0f )
		{
			ColDotProduct = -DotProduct( ChassisForw, trCol.vecPlaneNormal ) * Forward;
			Collision += -gpGlobals->v_forward * CarSpeed * Forward + trCol.vecPlaneNormal * CarSpeed * ColDotProduct;
			ColPoint = ColPointWFL;
			if( ColDotProduct < 0.6 )
				ColDotProduct *= -1;
		}
		else if( hit_carblocker )
		{
			Collision += -gpGlobals->v_forward * CarSpeed * Forward;
			ColPoint = ColPointWFL;
		}
		hit_carblocker = false;
	}

	if( Forward == -1 || (FrontWheelsInAir + RearWheelsInAir < 3 && Forward == 1) )
	{
		// rear right wheel
		UTIL_TraceLine( GetAbsOrigin() + ChassisUp * RearWheelRadius * 1.5 + ChassisRight * RearSuspWidth * 0.5, GetAbsOrigin() + ChassisUp * RearWheelRadius * 1.5 + ChassisRight * RearSuspWidth * 0.5 - ChassisForw * (RearSuspDist + RearBumperLength), ignore_monsters, edict(), &trCol );
		ColPointWRR = trCol.vecEndPos;
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_CARBLOCKER )
			hit_carblocker = true;
		if( trCol.flFraction < 1.0f )
		{
			ColDotProduct = -DotProduct( ChassisForw, trCol.vecPlaneNormal ) * Forward;
			Collision += gpGlobals->v_forward * CarSpeed * Forward + trCol.vecPlaneNormal * CarSpeed * Forward;
			ColPoint = ColPointWRR;
			if( Forward == -1 )
			{
				if( ColDotProduct < 0.6 )
					ColDotProduct *= -1;
			}
			else if( Forward == 1 )
			{
				if( ColDotProduct > 0.4 )
					ColDotProduct *= -1;
			}
		}
		else if( hit_carblocker )
		{
			Collision += gpGlobals->v_forward * CarSpeed * Forward;
			ColPoint = ColPointWRR;
		}
		hit_carblocker = false;

		// rear left wheel
		UTIL_TraceLine( GetAbsOrigin() + ChassisUp * RearWheelRadius * 1.5 - ChassisRight * RearSuspWidth * 0.5, GetAbsOrigin() + ChassisUp * RearWheelRadius * 1.5 - ChassisRight * RearSuspWidth * 0.5 - ChassisForw * (RearSuspDist + RearBumperLength), ignore_monsters, edict(), &trCol );
		ColPointWRL = trCol.vecEndPos;
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_CARBLOCKER )
			hit_carblocker = true;
		if( trCol.flFraction < 1.0f )
		{
			ColDotProduct = -DotProduct( ChassisForw, trCol.vecPlaneNormal ) * Forward;
			Collision += gpGlobals->v_forward * CarSpeed * Forward + trCol.vecPlaneNormal * CarSpeed * Forward;
			ColPoint = ColPointWRL;
			if( Forward == -1 )
			{
				if( ColDotProduct < 0.6 )
					ColDotProduct *= -1;
			}
		}
		else if( hit_carblocker )
		{
			Collision += gpGlobals->v_forward * CarSpeed * Forward;
			ColPoint = ColPointWRL;
		}
		hit_carblocker = false;
	}

	// sides!
	// left
	UTIL_TraceLine( GetAbsOrigin() + gpGlobals->v_up * 30, GetAbsOrigin() + gpGlobals->v_up * 30 - gpGlobals->v_right * (FrontSuspWidth + RearSuspWidth) * 0.3, ignore_monsters, edict(), &trCol );
	if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_CARBLOCKER )
		hit_carblocker = true;
	if( trCol.flFraction < 1.0f || hit_carblocker )
		ColPointLeft = gpGlobals->v_right * 100;
	hit_carblocker = false;
	// right
	UTIL_TraceLine( GetAbsOrigin() + gpGlobals->v_up * 30, GetAbsOrigin() + gpGlobals->v_up * 30 + gpGlobals->v_right * (FrontSuspWidth + RearSuspWidth) * 0.3, ignore_monsters, edict(), &trCol );
	if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_CARBLOCKER )
		hit_carblocker = true;
	if( trCol.flFraction < 1.0f || hit_carblocker )
		ColPointRight = -gpGlobals->v_right * 100;

	Vector SideCollision = ColPointLeft + ColPointRight;
	
	// adjust velocity according to collision...
	pev->velocity += Collision * 2 + SideCollision; // * 2 - bump into opposite direction
	
	hit_carblocker = false;

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
	// FIXME - magic numbers! we have to account the distance between wheels for correct body angle on slopes
	// SuspDiff2 is not needed anymore because of this, so it's always 1.0
	// something similar must be done for LateralChange...
	CrossChange *= 115.0f / (float)(FrontSuspDist + RearSuspDist);
	
	float LateralChange = ((pWheel1->GetAbsOrigin().z + pWheel3->GetAbsOrigin().z - SuspDiff) * 0.5) - ((pWheel2->GetAbsOrigin().z + pWheel4->GetAbsOrigin().z - SuspDiff) * 0.5);

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
		AccelAddX = UTIL_Approach( 0, AccelAddX, 2 * gpGlobals->frametime );
	else if( CarSpeed > 0 && CarSpeed < 250 && (bForward()) && (CarSpeed < (MaxCarSpeed * 0.5)) && !IsShifting )
		AccelAddX -= AccelRate * 0.01 * gpGlobals->frametime * (1 + HeatingMult);
	else if( CarSpeed < 0 && CarSpeed > -150 && !(bForward()) && (bBack()) )
		AccelAddX += BackAccelRate * 0.01 * gpGlobals->frametime * (1 + HeatingMult);
	else
		AccelAddX = UTIL_Approach( 0, AccelAddX, 2 * gpGlobals->frametime );

	AccelAddX = bound( -MaxLean * 0.2, AccelAddX, MaxLean * 0.2 );
	
	float NewChassisAngX = -CrossChange * 0.5 * SuspDiff2 + BrakeAddX + AccelAddX + AddChassisShake;
	if( FrontWheelsInAir + RearWheelsInAir < 4 )
		ChassisAng.x = UTIL_Approach( NewChassisAngX, ChassisAng.x, SuspHardness * fabs( NewChassisAngX - ChassisAng.x) * gpGlobals->frametime);

	// Z - lateral rotation
	float NewChassisAngZ = -LateralChange * SuspDiff2 + CarSpeed * Turning * (DriftAmount * MaxLean * 0.001) + AddChassisShake;
	if( FrontWheelsInAir + RearWheelsInAir < 4 )
		ChassisAng.z = UTIL_Approach( NewChassisAngZ, ChassisAng.z, SuspHardness * fabs( NewChassisAngZ - ChassisAng.z ) * gpGlobals->frametime );

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
	Vector NewChassisOrg = (pWheel1->GetAbsOrigin() + pWheel2->GetAbsOrigin() + pWheel3->GetAbsOrigin() + pWheel4->GetAbsOrigin()) * 0.25;
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

	float ActualMaxCarSpeed = MaxCarSpeed * WaterVelocityMult * surf_CurrentMult;
	float ActualMaxCarSpeedBackwards = MaxCarSpeedBackwards * WaterVelocityMult * surf_CurrentMult;
	float ActualAccelRate = (AccelRate + (200 * HeatingMult) + (TurboAccum * 100)) / (1 + (AbsCarSpeed / MaxCarSpeed));
	float ActualBackAccelRate = (BackAccelRate + (200 * HeatingMult)) / (1 + (AbsCarSpeed / MaxCarSpeed));

	if( HasSpawnFlags( SF_CAR_TURBO ) )
	{
		if( CanPlayTurboSound && (TurboAccum > 0.25f) )
		{
			EMIT_SOUND_DYN( edict(), CHAN_STATIC, "func_car/turbo.wav", TurboAccum, ATTN_NORM, 0, RANDOM_LONG( 90, 110 ) );
			CanPlayTurboSound = false;
			TurboAccum = 0.0f;
		}
	}

	if( (FrontWheelsInAir + RearWheelsInAir == 4) )
	{
		// don't add speed
	}
	else if( FrontWheelsInAir == 2 )
	{
		CarSpeed += 300 * gpGlobals->frametime;
	}
	else if( RearWheelsInAir == 2 )
	{
		CarSpeed -= 300 * gpGlobals->frametime;
	}
	else if( (!(bForward()) && !(bBack()) && !(bUp())) || BrokenCar || IsLockedByMaster() )
	{
		// no accelerate/brake buttons pressed or all wheels in air
		// car slows down (aka friction...)
		float SlowDownRamp = 150.0f;
		int RampDir = 1; // 1 up, -1 down

		if( fabs( ChassisAng.x ) > 10 ) // start to go down by itself, if on a slope
			SlowDownRamp = 100 + 5 * fabs( ChassisAng.x ); // must put weight there I guess

		if( Forward == 1 && ChassisAng.x > 10.0f )
			RampDir = -1;
		else if( Forward == -1 && ChassisAng.x < -10.0f ) // going backwards, nose up
			RampDir = -1;

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
			CarSpeed = UTIL_Approach( 0, CarSpeed, BrakeRate * 1.35 * gpGlobals->frametime );
		}
		else if( bForward() )
		{
			if( CarSpeed > 0 && CarSpeed > ActualMaxCarSpeed && ActualMaxCarSpeed != MaxCarSpeed )
				CarSpeed -= 300 * gpGlobals->frametime;
			else if( !IsShifting )
				CarSpeed += ActualAccelRate * (1 - fabs( Turning ) * 0.5) * WaterVelocityMult * surf_CurrentMult * gpGlobals->frametime;
		}
		else if( bBack() )
		{
			if( CarSpeed > 0 )
				CarSpeed -= BrakeRate * gpGlobals->frametime;
			else
			{
				if( CarSpeed < 0 && CarSpeed < -ActualMaxCarSpeedBackwards && ActualMaxCarSpeedBackwards != MaxCarSpeedBackwards )
					CarSpeed += 300 * gpGlobals->frametime;
				else
					CarSpeed -= ActualBackAccelRate * (1 - fabs( Turning ) * 0.5) * WaterVelocityMult * surf_CurrentMult * gpGlobals->frametime;
			}
		}
	}

	// chassis has changed angles too fast (more than 5 deg this frame). slow down
	if( fabs( NewChassisAngX - ChassisAng.x ) > 5.0f )
	{
		CarSpeed = UTIL_Approach( 0, CarSpeed, 150 * gpGlobals->frametime * Forward * fabs( NewChassisAngX - ChassisAng.x ) );
	//	ALERT( at_console, "slow %f\n", fabs( NewChassisAngX - ChassisAng.x ) );
	//	CarSpeed -= 50 * gpGlobals->frametime * Forward * fabs( NewChassisAngX - ChassisAng.x );
	}

	if( DriftMode )
		CarSpeed = bound( (-MaxCarSpeedBackwards + (MaxCarSpeedBackwards * fabs( Turning ) * 0.5) ), CarSpeed, (MaxCarSpeed - (MaxCarSpeed * fabs( Turning ) * 0.75)) );
	else
		// also slow down the car if turning too much
		CarSpeed = bound( -MaxCarSpeedBackwards + (MaxCarSpeedBackwards * fabs(Turning) * 0.5) + RANDOM_LONG(-1,1), CarSpeed, MaxCarSpeed - (MaxCarSpeed * fabs( Turning ) * 0.75) + RANDOM_LONG( -1, 1 ) );

	// -------- apply main movement  --------
	float AngleDiff = pWheel1->GetLocalAngles().y - pChassis->GetLocalAngles().y;
	float Percentage = Forward * (AngleDiff / (90 - MaxTurn));
	if( bUp() ) // handbrake
	{
		pev->velocity += gpGlobals->v_forward * CarSpeed * (1 - fabs(Percentage)) + gpGlobals->v_right * CarSpeed * Percentage;
	}
	else
	{
		if( DriftMode )
			pev->velocity += DriftForw * CarSpeed;
		else
			pev->velocity += gpGlobals->v_forward * CarSpeed;
	}

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
		if( mult < 1 ) mult = 1; // -____-
		DownForce = -AbsCarSpeed * (0.02 * mult);
	}

	pev->velocity.z = DownForce;

	//----------------------------
	// timed effects
	//----------------------------

	bool TookDamage = false;
	float DriverDamage = 0.0f;
	bool Collided = false;

	if( CarTouch )
		CarSpeed *= 0.5;
	
	// do timing stuff each 0.5 seconds
	if( (Collision.Length() > 0.001f) && (gpGlobals->time > time) || CarTouch )
	{
		if( ColDotProduct < 0 && ColDotProduct > -0.18 )
			ColDotProduct = -0.18;
		else if( ColDotProduct > 0 && ColDotProduct < 0.18 )
			ColDotProduct = 0.18;
		// 0.18 so the multiplier would be less than 1 always
	//	CarSpeed *= -0.15 / ColDotProduct; // moved to bottom
		Collided = true;

		if( AbsCarSpeed > 500 )
		{
			TookDamage = true;
			DriverDamage = AbsCarSpeed * 0.025f * fabs(ColDotProduct);
		}

		if( AbsCarSpeed > 100 )
		{
			// apply collision effects
			if( ColPoint.Length() > 0 )
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
		pDriverMdl->SetBlending( 0, (f * -FrontWheelAng.y * 2) + AddTurnWheelShake);
		
		bool SpecialAnim = false;

		if( CarSpeed >= 0 || HeatingTires )
			DriverMdlSequence = pDriverMdl->LookupSequence( "idle" );
		else if( CarSpeed < 25 && bBack() )
			DriverMdlSequence = pDriverMdl->LookupSequence( "lookback" );

		// special anims, reserve a second
		if( (ColPoint.Length() > 0) && (AbsCarSpeed > 100) )
		{
			SpecialAnim = true;

			if( CarSpeed > 0 )
				DriverMdlSequence = pDriverMdl->LookupSequence( "damage_small" );
			else if( CarSpeed < 0 )
				DriverMdlSequence = pDriverMdl->LookupSequence( "damage_small_back" );
		}

		if( TookDamage )
		{
			DriverMdlSequence = pDriverMdl->LookupSequence( "damage" );
			SpecialAnim = true;
		}

		if( pDriverMdl->pev->sequence != DriverMdlSequence && driveranimtime < gpGlobals->time )
		{
			pDriverMdl->pev->sequence = DriverMdlSequence;
			pDriverMdl->pev->frame = 0;
			pDriverMdl->ResetSequenceInfo();
			if( SpecialAnim )
				driveranimtime = gpGlobals->time + 0.5;
		}
	}

	// measure speed
	float kph = (Distance * 0.001f) / (gpGlobals->frametime / 3600.0f);

	// unstick the car if this happens...
	// stupid hack just to keep things playable because it can truly stick in a brush and it looks terrible
	float ExpectedDist = (pev->velocity * gpGlobals->frametime).Length() * 0.01905f; // predicted velocity next frame in meters
	// we didn't make even a half of expected distance, likely stuck
	if( (AbsCarSpeed > 100.0f) && (ExpectedDist > 0.01f) && (Distance < ExpectedDist * 0.5f) && (gpGlobals->time > StuckTime + 1) && !Collided )
	{
	//	ALERT( at_console, "this frame %f, next frame %f\n", Distance, ExpectedDist );
		ALERT( at_aiconsole, "Car unstick\n" );
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

		pChassisMdl->SetBlending( 0, (-FrontWheelAng.y * 2) + AddTurnWheelShake );
	}
	
	SetLocalAngles( CarAng ); // car direction is now set

	if( Collided )
		CarSpeed *= -0.1 / ColDotProduct;

	//----------------------------
	// shoot!
	//----------------------------
	if( pTankTower )
	{
		// rotate tower
		Vector TowerAngles = ChassisAng;
		if( pChassisMdl )
			TowerAngles = pChassisMdl->GetLocalAngles();

		if( CamUnlocked )
			TowerAngles.y = hDriver->pev->v_angle.y;
		else
			TowerAngles.y = pev->angles.y;
		pTankTower->SetAbsAngles( TowerAngles );

		if( (hDriver->pev->button & IN_ATTACK) && (gpGlobals->time > LastShootTime + 1) )
		{
			// gpGlobals->v_forward comes from the driver's v_angle vector! don't mess up util_makevectors
			Vector RocketOrg = pTankTower->GetAbsOrigin() + gpGlobals->v_forward * 50;
			Vector RocketAng = hDriver->pev->v_angle;
			RocketAng.x -= 10; // aim higher

			if( !CamUnlocked )
				RocketAng = TowerAngles; // just forward
			
			if( !CamUnlocked )
				RocketAng.x = RocketAng.z = 0;
			CBaseEntity *pRocket = CBaseEntity::Create( "apc_projectile", RocketOrg, RocketAng, hDriver->edict() );

			if( pRocket )
			{
				pRocket->SetAbsVelocity( gpGlobals->v_forward * 3500 );
				PlayClientSound( pTankTower, 248 );
			}

			LastShootTime = gpGlobals->time;
		}
	}

	SafeSpawnPosition();

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
	
	SetNextThink( 0 );
}

//===============================================================================
// camera view
//===============================================================================
void CCar::Camera(void)
{
	if( !AllowCamera )
		return;

	if( !(hDriver->pev->flags & FL_CLIENT) )
		return;

	if( CarSpeed > 0.01f && (bBack() || bUp()) ) // braking
		CameraBrakeOffsetX = UTIL_Approach( 25, CameraBrakeOffsetX, 5 * gpGlobals->frametime );
	else
		CameraBrakeOffsetX = UTIL_Approach( 0, CameraBrakeOffsetX, 9 * gpGlobals->frametime );

	TraceResult CamTr; // !!! needs to be done better

	if( (CamUnlocked && pFreeCam) || (!pCamera1 && !SecondaryCamera) )
	{
		SET_VIEW( hDriver->edict(), pFreeCam->edict() );
		TraceResult CamTr;
		Vector DriverAngles = hDriver->pev->v_angle;
		Vector CamOrg;

		if( !CamUnlocked )
		{
			Vector ChAng = pChassis->GetAbsAngles();
			ChAng.x *= 0.5;
			UTIL_MakeVectors( ChAng );
			if( CarSpeed >= -100 )
			{
				// pev->iuser2 - hacked default camera height offset
				// iuser1 for freecam is hardcoded, just tune it if needed - it's the distance
				UTIL_TraceLine( GetAbsOrigin() + gpGlobals->v_up * 80, GetAbsOrigin() + gpGlobals->v_up * (80 + pev->iuser2) - gpGlobals->v_forward * (250 + pFreeCam->pev->iuser1) - gpGlobals->v_right * CameraMoving, dont_ignore_monsters, dont_ignore_glass, edict(), &CamTr );
				CamOrg = CamTr.vecEndPos + CamTr.vecPlaneNormal * 10;
				pFreeCam->SetAbsOrigin( CamOrg );
			//	pFreeCam->SetAbsAngles( GetAbsAngles() );
				if( !pCamera1 )
					pFreeCam->SetLocalAngles( Camera1LocalAngles + Vector( CameraBrakeOffsetX, 0, 0 ) );
			}
			else if( CarSpeed < -100 )
			{
				UTIL_TraceLine( GetAbsOrigin() + gpGlobals->v_up * 80, GetAbsOrigin() + gpGlobals->v_up * 80 + gpGlobals->v_forward * (250 + pFreeCam->pev->iuser1) - gpGlobals->v_right * CameraMoving, dont_ignore_monsters, dont_ignore_glass, edict(), &CamTr );
				CamOrg = CamTr.vecEndPos + CamTr.vecPlaneNormal * 10;
				pFreeCam->SetAbsOrigin( CamOrg );
				pFreeCam->SetAbsAngles( GetAbsAngles() + Vector( 0, 180, 0 ) );
			}
		}
		else
		{
			UTIL_MakeVectors( DriverAngles );
			UTIL_TraceLine( hDriver->GetAbsOrigin(), hDriver->GetAbsOrigin() - gpGlobals->v_forward * (250 + pFreeCam->pev->iuser1) + gpGlobals->v_up * pev->iuser2, dont_ignore_monsters, dont_ignore_glass, edict(), &CamTr );
			CamOrg = CamTr.vecEndPos + CamTr.vecPlaneNormal * 10;
			pFreeCam->SetAbsOrigin( CamOrg );
			pFreeCam->SetAbsAngles( DriverAngles );
		}
	}
	else
	{
		if( !pCamera2 )
			SecondaryCamera = false;

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
			pCamera2->SetLocalAngles( Camera2LocalAngles + Vector( CameraBrakeOffsetX, 0, 0 ) );
		}
		else if( pCamera1 )
		{
			SET_VIEW( hDriver->edict(), pCamera1->edict() );
			CamOrg = Camera1LocalOrigin;
			BackSway = bound( 0, BackSway, MaxCamera1Sway );
			CamOrg.x = Camera1LocalOrigin.x - BackSway;
			CamOrg.y = Camera1LocalOrigin.y + CameraMoving;
			pCamera1->SetLocalOrigin( CamOrg );
			pCamera1->SetLocalAngles( Camera1LocalAngles + Vector( CameraBrakeOffsetX, 0, 0 ) );
		}
	}
}

//===============================================================================
// stripped down copy-paste of "Drive()"
//===============================================================================
void CCar::Idle( void )
{
	// will need those later!
	UTIL_MakeVectors( pChassis->GetAbsAngles() );
	Vector ChassisForw = gpGlobals->v_forward;
	Vector ChassisUp = gpGlobals->v_up;
	Vector ChassisRight = gpGlobals->v_right;

	// now make the car's vectors
	UTIL_MakeVectors( GetAbsAngles() );

	float AbsCarSpeed = fabs( CarSpeed );

	//----------------------------
	// wheels' origin (aka "suspension"...)
	// 1-2 front wheels, 3-4 rear wheels
	//----------------------------
	TraceResult tr;

	pWheel1Org = GetAbsOrigin() + ChassisForw * FrontSuspDist + gpGlobals->v_right * (FrontSuspWidth * 0.5);
	pWheel2Org = GetAbsOrigin() + ChassisForw * FrontSuspDist - gpGlobals->v_right * (FrontSuspWidth * 0.5);
	pWheel3Org = GetAbsOrigin() - ChassisForw * RearSuspDist + gpGlobals->v_right * (RearSuspWidth * 0.5);
	pWheel4Org = GetAbsOrigin() - ChassisForw * RearSuspDist - gpGlobals->v_right * (RearSuspWidth * 0.5);
	Vector pWheel1NewOrg = g_vecZero;
	Vector pWheel2NewOrg = g_vecZero;
	Vector pWheel3NewOrg = g_vecZero;
	Vector pWheel4NewOrg = g_vecZero;
	Vector pWheelOrgSet = g_vecZero;

	int FRW_InAir = 0;
	int FLW_InAir = 0;
	int RRW_InAir = 0;
	int RLW_InAir = 0;

	// front right wheel
	// ChassisUp * 40: need to make tracing start high to work properly on slopes !!!
	float SlopeAdjust = 40.0f;
	UTIL_TraceLine( pWheel1Org + ChassisUp * SlopeAdjust, pWheel1Org - ChassisUp * FrontWheelRadius * 2, ignore_monsters, dont_ignore_glass, pWheel1->edict(), &tr );
	pWheel1NewOrg = tr.vecEndPos + ChassisUp * FrontWheelRadius;

	if( tr.flFraction == 1.0f && (int)(pWheel1NewOrg - pWheel1Org).Length() >= FrontWheelRadius - 1 )
	{
		pWheel1NewOrg = pWheel1Org - ChassisUp * FrontWheelRadius;
		FRW_InAir = 1;
	}

	// front left wheel
	UTIL_TraceLine( pWheel2Org + ChassisUp * SlopeAdjust, pWheel2Org - ChassisUp * FrontWheelRadius * 2, ignore_monsters, dont_ignore_glass, pWheel2->edict(), &tr );
	pWheel2NewOrg = tr.vecEndPos + ChassisUp * FrontWheelRadius;
	if( tr.flFraction == 1.0f && (int)(pWheel2NewOrg - pWheel2Org).Length() >= FrontWheelRadius - 1 )
	{
		pWheel2NewOrg = pWheel2Org - ChassisUp * FrontWheelRadius;
		FLW_InAir = 1;
	}

	// rear right wheel
	UTIL_TraceLine( pWheel3Org + ChassisUp * SlopeAdjust, pWheel3Org - ChassisUp * RearWheelRadius * 2, ignore_monsters, dont_ignore_glass, pWheel3->edict(), &tr );
	pWheel3NewOrg = tr.vecEndPos + ChassisUp * RearWheelRadius;
	if( tr.flFraction == 1.0f && (int)(pWheel3NewOrg - pWheel3Org).Length() >= RearWheelRadius - 1 )
	{
		pWheel3NewOrg = pWheel3Org - ChassisUp * RearWheelRadius;
		RRW_InAir = 1;
	}

	// rear left wheel
	UTIL_TraceLine( pWheel4Org + ChassisUp * SlopeAdjust, pWheel4Org - ChassisUp * RearWheelRadius * 2, ignore_monsters, dont_ignore_glass, pWheel4->edict(), &tr );
	pWheel4NewOrg = tr.vecEndPos + ChassisUp * RearWheelRadius;
	if( tr.flFraction == 1.0f && (int)(pWheel4NewOrg - pWheel4Org).Length() >= RearWheelRadius - 1 )
	{
		pWheel4NewOrg = pWheel4Org - ChassisUp * RearWheelRadius;
		RLW_InAir = 1;
	}

	int FrontWheelsInAir = FRW_InAir + FLW_InAir; // can't turn if 2
	int RearWheelsInAir = RRW_InAir + RLW_InAir; // can't accelerate if 2 // TODO: 4x4? Front drive?

	pWheel1->SetAbsOrigin( pWheel1NewOrg );
	pWheel2->SetAbsOrigin( pWheel2NewOrg );
	pWheel3->SetAbsOrigin( pWheel3NewOrg );
	pWheel4->SetAbsOrigin( pWheel4NewOrg );

	//----------------------------
	// turn
	//----------------------------
	
	// Forward turning controls if going backwards
	int Forward = 1;
	Vector CarAng = GetLocalAngles();

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
	ChassisAng.x = UTIL_Approach( NewChassisAngX, ChassisAng.x, SuspHardness * fabs( NewChassisAngX - ChassisAng.x ) * gpGlobals->frametime );
	ChassisAng.x = bound( -30, ChassisAng.x, 30 );

	// Z - lateral rotation
	float NewChassisAngZ = -LateralChange * SuspDiff2 + CarSpeed * Turning * (MaxLean * 0.001);
	ChassisAng.z = UTIL_Approach( NewChassisAngZ, ChassisAng.z, SuspHardness * fabs( NewChassisAngZ - ChassisAng.z ) * gpGlobals->frametime );
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
	// accelerate/brake
	//----------------------------
	if( (FrontWheelsInAir + RearWheelsInAir == 4) || (FrontWheelsInAir == 2) )
	{
		CarSpeed += Forward * 200 * gpGlobals->frametime;
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
	// collision
	//----------------------------
	TraceResult trCol;
	Vector Collision = g_vecZero;
	Vector ColPoint = g_vecZero; // place for sparks
	Vector ColPointLeft = g_vecZero;
	Vector ColPointRight = g_vecZero;

	// 4 collision points from wheels.
	Vector ColPointWFR = g_vecZero;
	Vector ColPointWFL = g_vecZero;
	Vector ColPointWRR = g_vecZero;
	Vector ColPointWRL = g_vecZero;

	float ColDotProduct = 1.0f;

	bool hit_carblocker = false;

	if( Forward == 1 || (FrontWheelsInAir + RearWheelsInAir < 3 && Forward == -1) )
	{
		// front right wheel
		UTIL_TraceLine( GetAbsOrigin() + ChassisUp * FrontWheelRadius * 1.5 + ChassisRight * FrontSuspWidth * 0.5, GetAbsOrigin() + ChassisUp * FrontWheelRadius * 1.5 + ChassisRight * FrontSuspWidth * 0.5 + ChassisForw * (FrontSuspDist + FrontBumperLength), ignore_monsters, edict(), &trCol );
		ColPointWFR = trCol.vecEndPos;
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_CARBLOCKER )
			hit_carblocker = true;
		if( trCol.flFraction < 1.0f )
		{
			ColDotProduct = -DotProduct( ChassisForw, trCol.vecPlaneNormal ) * Forward;
			Collision += -gpGlobals->v_forward * CarSpeed * Forward + trCol.vecPlaneNormal * CarSpeed * ColDotProduct;
			ColPoint = ColPointWFR;
			if( ColDotProduct < 0.6 )
				ColDotProduct *= -1;
		}
		else if( hit_carblocker )
		{
			Collision += -gpGlobals->v_forward * CarSpeed * Forward;
			ColPoint = ColPointWFR;
		}
		hit_carblocker = false;

		// front left wheel
		UTIL_TraceLine( GetAbsOrigin() + ChassisUp * FrontWheelRadius * 1.5 - ChassisRight * FrontSuspWidth * 0.5, GetAbsOrigin() + ChassisUp * FrontWheelRadius * 1.5 - ChassisRight * FrontSuspWidth * 0.5 + ChassisForw * (FrontSuspDist + FrontBumperLength), ignore_monsters, edict(), &trCol );
		ColPointWFL = trCol.vecEndPos;
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_CARBLOCKER )
			hit_carblocker = true;
		if( trCol.flFraction < 1.0f )
		{
			ColDotProduct = -DotProduct( ChassisForw, trCol.vecPlaneNormal ) * Forward;
			Collision += -gpGlobals->v_forward * CarSpeed * Forward + trCol.vecPlaneNormal * CarSpeed * ColDotProduct;
			ColPoint = ColPointWFL;
			if( ColDotProduct < 0.6 )
				ColDotProduct *= -1;
		}
		else if( hit_carblocker )
		{
			Collision += -gpGlobals->v_forward * CarSpeed * Forward;
			ColPoint = ColPointWFL;
		}
		hit_carblocker = false;
	}

	if( Forward == -1 || (FrontWheelsInAir + RearWheelsInAir < 3 && Forward == 1) )
	{
		// rear right wheel
		UTIL_TraceLine( GetAbsOrigin() + ChassisUp * RearWheelRadius * 1.5 + ChassisRight * RearSuspWidth * 0.5, GetAbsOrigin() + ChassisUp * RearWheelRadius * 1.5 + ChassisRight * RearSuspWidth * 0.5 - ChassisForw * (RearSuspDist + RearBumperLength), ignore_monsters, edict(), &trCol );
		ColPointWRR = trCol.vecEndPos;
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_CARBLOCKER )
			hit_carblocker = true;
		if( trCol.flFraction < 1.0f )
		{
			ColDotProduct = -DotProduct( ChassisForw, trCol.vecPlaneNormal ) * Forward;
			Collision += gpGlobals->v_forward * CarSpeed * Forward + trCol.vecPlaneNormal * CarSpeed * Forward;
			ColPoint = ColPointWRR;
			if( Forward == -1 )
			{
				if( ColDotProduct < 0.6 )
					ColDotProduct *= -1;
			}
			else if( Forward == 1 )
			{
				if( ColDotProduct > 0.4 )
					ColDotProduct *= -1;
			}
		}
		else if( hit_carblocker )
		{
			Collision += gpGlobals->v_forward * CarSpeed * Forward;
			ColPoint = ColPointWRR;
		}
		hit_carblocker = false;

		// rear left wheel
		UTIL_TraceLine( GetAbsOrigin() + ChassisUp * RearWheelRadius * 1.5 - ChassisRight * RearSuspWidth * 0.5, GetAbsOrigin() + ChassisUp * RearWheelRadius * 1.5 - ChassisRight * RearSuspWidth * 0.5 - ChassisForw * (RearSuspDist + RearBumperLength), ignore_monsters, edict(), &trCol );
		ColPointWRL = trCol.vecEndPos;
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_CARBLOCKER )
			hit_carblocker = true;
		if( trCol.flFraction < 1.0f )
		{
			ColDotProduct = -DotProduct( ChassisForw, trCol.vecPlaneNormal ) * Forward;
			Collision += gpGlobals->v_forward * CarSpeed * Forward + trCol.vecPlaneNormal * CarSpeed * Forward;
			ColPoint = ColPointWRL;
			if( Forward == -1 )
			{
				if( ColDotProduct < 0.6 )
					ColDotProduct *= -1;
			}
		}
		else if( hit_carblocker )
		{
			Collision += gpGlobals->v_forward * CarSpeed * Forward;
			ColPoint = ColPointWRL;
		}
		hit_carblocker = false;
	}

	// sides!
	// left
	UTIL_TraceLine( GetAbsOrigin() + gpGlobals->v_up * 30, GetAbsOrigin() + gpGlobals->v_up * 30 - gpGlobals->v_right * (FrontSuspWidth + RearSuspWidth) * 0.3, ignore_monsters, edict(), &trCol );
	if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_CARBLOCKER )
		hit_carblocker = true;
	if( trCol.flFraction < 1.0f || hit_carblocker )
		ColPointLeft = gpGlobals->v_right * 100;
	hit_carblocker = false;
	// right
	UTIL_TraceLine( GetAbsOrigin() + gpGlobals->v_up * 30, GetAbsOrigin() + gpGlobals->v_up * 30 + gpGlobals->v_right * (FrontSuspWidth + RearSuspWidth) * 0.3, ignore_monsters, edict(), &trCol );
	if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_CARBLOCKER )
		hit_carblocker = true;
	if( trCol.flFraction < 1.0f || hit_carblocker )
		ColPointRight = -gpGlobals->v_right * 100;

	Vector SideCollision = ColPointLeft + ColPointRight;

	// adjust velocity according to collision...
	pev->velocity += Collision * 2 + SideCollision; // * 2 - bump into opposite direction

	hit_carblocker = false;

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
	if( (gpGlobals->time > time) && (Collision.Length() > 0.01f) )
	{
		// apply collision effects
		if( ColPoint.Length() > 0 )
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

		CarSpeed *= -0.2;

		time = gpGlobals->time + 0.5;
	}

	//----------------------------
	// idle sound
	//----------------------------
	if( m_iszIdleSnd && !StartSilent )
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, STRING( m_iszIdleSnd ), 1, 2.2, SND_CHANGE_PITCH, PITCH_NORM );

	SetLocalAngles( CarAng ); // car direction is now set

	float thinktime = 0.0f;
	if( !g_pGameRules->IsMultiplayer() && (CarSpeed == 0.0f) && (pev->velocity == g_vecZero) && (pev->flags & FL_ONGROUND) )
	{
		// don't think too often if player is far from the car, and it is idle
		CBaseEntity *pPlayer = CBaseEntity::Instance( INDEXENT( 1 ) );
		if( (GetAbsOrigin() - pPlayer->GetAbsOrigin()).Length() > 2000 )
			thinktime = 0.5;
	}

	SetNextThink( thinktime );
}

void CCar::ClearEffects( void )
{
	DontThink();
	
	if( pWheel1 )
		UTIL_Remove( pWheel1 );
	if( pWheel2 )
		UTIL_Remove( pWheel2 );
	if( pWheel3 )
		UTIL_Remove( pWheel3 );
	if( pWheel4 )
		UTIL_Remove( pWheel4 );
	if( pChassis )
		UTIL_Remove( pChassis );
	if( pChassisMdl )
		UTIL_Remove( pChassisMdl );
	if( pCamera1 )
		UTIL_Remove( pCamera1 );
	if( pCamera2 )
		UTIL_Remove( pCamera2 );
	if( pFreeCam )
		UTIL_Remove( pFreeCam );
	if( pCarHurt )
		UTIL_Remove( pCarHurt );
	if( pDriverMdl )
		UTIL_Remove( pDriverMdl );

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
}

bool CCar::ExitCar( CBaseEntity *pPlayer )
{
	// set player position
	TraceResult TrExit;
	TraceResult VisCheck;
	UTIL_MakeVectors( pChassis->GetAbsAngles() );
	Vector CarCenter = GetAbsOrigin() + gpGlobals->v_up * 46; // (human hull height / 2) + 10
	Vector vecSrc, vecEnd;

	// check left side of the car first
	vecSrc = pWheel2->GetAbsOrigin() + gpGlobals->v_up * (38 - FrontWheelRadius) - gpGlobals->v_right * 50 - gpGlobals->v_forward * (FrontSuspDist + RearSuspDist) * 0.5;
	vecEnd = vecSrc;
	UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, human_hull, edict(), &TrExit );
	UTIL_TraceLine( CarCenter, vecSrc, dont_ignore_monsters, dont_ignore_glass, edict(), &VisCheck );
	if( (TrExit.flFraction == 1.0f) && !TrExit.fAllSolid && (VisCheck.flFraction == 1.0f) )
	{
		pPlayer->SetAbsOrigin( vecSrc );
		return true;
	}

	// can't spawn there, now check right side
	vecSrc = pWheel1->GetAbsOrigin() + gpGlobals->v_up * (38 - FrontWheelRadius) + gpGlobals->v_right * 50 - gpGlobals->v_forward * (FrontSuspDist + RearSuspDist) * 0.5;
	vecEnd = vecSrc;
	UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, human_hull, edict(), &TrExit );
	UTIL_TraceLine( CarCenter, vecSrc, dont_ignore_monsters, dont_ignore_glass, edict(), &VisCheck );
	if( (TrExit.flFraction == 1.0f) && !TrExit.fAllSolid && (VisCheck.flFraction == 1.0f) )
	{
		pPlayer->SetAbsOrigin( vecSrc );
		return true;
	}

	// can't spawn both, try front of the car
	vecSrc = CarCenter + gpGlobals->v_forward * (FrontSuspDist + FrontBumperLength + 50);
	vecEnd = vecSrc;
	UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, human_hull, edict(), &TrExit );
	UTIL_TraceLine( CarCenter, vecSrc, dont_ignore_monsters, dont_ignore_glass, edict(), &VisCheck );
	if( (TrExit.flFraction == 1.0f) && !TrExit.fAllSolid && (VisCheck.flFraction == 1.0f) )
	{
		pPlayer->SetAbsOrigin( vecSrc );
		return true;
	}

	// can't spawn, try back of the car
	vecSrc = CarCenter - gpGlobals->v_forward * (RearSuspDist + RearBumperLength + 50);
	vecEnd = vecSrc;
	UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, human_hull, edict(), &TrExit );
	UTIL_TraceLine( CarCenter, vecSrc, dont_ignore_monsters, dont_ignore_glass, edict(), &VisCheck );
	if( (TrExit.flFraction == 1.0f) && !TrExit.fAllSolid && (VisCheck.flFraction == 1.0f) )
	{
		pPlayer->SetAbsOrigin( vecSrc );
		return true;
	}

	// can't spawn anywhere.
//	UTIL_ShowMessage( "Can't exit the vehicle here!", pPlayer );
	ALERT( at_error, "Can't exit the vehicle here!\n" );

	// last try...
	pPlayer->SetAbsOrigin( SafeSpawnPos );
	return true;

	return false;
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
	Vector CarCenter = GetAbsOrigin() + gpGlobals->v_up * 46; // (human hull height / 2) + 10
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
	
	Vector vecOrigin = GetAbsOrigin();
	Vector vecSpot = vecOrigin + (pev->mins + pev->maxs) * 0.5;

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