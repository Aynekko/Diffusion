#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "triggers.h"
#include "game/game.h"
#include "game/gamerules.h"
#include "weapons/weapons.h"

// #define USE_LOCAL_ROTATION okay it's not working :(

#define MAX_VERTICAL_VELOCITY 500
#define MAX_BLADE_SPEED 1000

#define SF_HELI_ONLYTRIGGER			BIT(0) // only trigger or external button

BEGIN_DATADESC( CHelicopter )
	DEFINE_KEYFIELD( HeliLength, FIELD_INTEGER, "helilength" ),
	DEFINE_KEYFIELD( HeliWidth, FIELD_INTEGER, "heliwidth" ),
	DEFINE_KEYFIELD( chassis, FIELD_STRING, "chassis" ),
	DEFINE_KEYFIELD( blade, FIELD_STRING, "blade" ),
	DEFINE_KEYFIELD( blade2, FIELD_STRING, "blade2" ),
	DEFINE_KEYFIELD( carhurt, FIELD_STRING, "carhurt" ),
	DEFINE_KEYFIELD( camera2, FIELD_STRING, "camera2" ),
	DEFINE_FIELD( Camera2LocalOrigin, FIELD_VECTOR ),
	DEFINE_KEYFIELD( MaxCamera2Sway, FIELD_INTEGER, "maxcam2sway" ),
	DEFINE_KEYFIELD( drivermdl, FIELD_STRING, "drivermdl" ),
	DEFINE_KEYFIELD( chassismdl, FIELD_STRING, "chassismdl" ),
	DEFINE_FIELD( MaxLean, FIELD_INTEGER ),
	DEFINE_KEYFIELD( MaxCarSpeed, FIELD_INTEGER, "maxspeed" ),
	DEFINE_KEYFIELD( AccelRate, FIELD_INTEGER, "accelrate" ),
	DEFINE_KEYFIELD( BrakeRate, FIELD_INTEGER, "brakerate" ),
	DEFINE_KEYFIELD( TurningRate, FIELD_INTEGER, "turningrate" ),
	DEFINE_FIELD( MaxTurn, FIELD_INTEGER ),
	DEFINE_FIELD( hDriver, FIELD_EHANDLE ),
	DEFINE_FUNCTION( Setup ),
	DEFINE_FUNCTION( Drive ),
	DEFINE_FUNCTION( Idle ),
	DEFINE_FIELD( pChassis, FIELD_CLASSPTR ),
	DEFINE_FIELD( pBlade, FIELD_CLASSPTR ),
	DEFINE_FIELD( pBlade2, FIELD_CLASSPTR ),
	DEFINE_FIELD( pCamera1, FIELD_CLASSPTR ),
	DEFINE_FIELD( pCamera2, FIELD_CLASSPTR ),
	DEFINE_FIELD( pFreeCam, FIELD_CLASSPTR ),
	DEFINE_FIELD( pCarHurt, FIELD_CLASSPTR ),
	DEFINE_FIELD( pDriverMdl, FIELD_CLASSPTR ),
	DEFINE_FIELD( pChassisMdl, FIELD_CLASSPTR ),
	DEFINE_KEYFIELD( m_iszEngineSnd, FIELD_STRING, "enginesnd" ),
	DEFINE_KEYFIELD( m_iszIdleSnd, FIELD_STRING, "idlesnd" ),
	DEFINE_FIELD( AllowCamera, FIELD_BOOLEAN ),
	DEFINE_FIELD( BrokenCar, FIELD_BOOLEAN ),
	DEFINE_KEYFIELD( m_iszAltTarget, FIELD_STRING, "m_iszAltTarget" ),
	DEFINE_FIELD( VerticalVelocity, FIELD_FLOAT ),
	DEFINE_FIELD( BladeSpeed, FIELD_FLOAT ),
	DEFINE_KEYFIELD( CameraHeight, FIELD_INTEGER, "camheight" ),
	DEFINE_KEYFIELD( CameraDistance, FIELD_INTEGER, "camdistance" ),
	DEFINE_FIELD( IsHeli, FIELD_BOOLEAN ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( func_helicopter, CHelicopter );

int CHelicopter::ObjectCaps( void )
{
	if( HasSpawnFlags( SF_HELI_ONLYTRIGGER ) )
		return 0;

	return FCAP_IMPULSE_USE;
}

void CHelicopter::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "helilength" ) )
	{
		HeliLength = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "heliwidth" ) )
	{
		HeliWidth = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "blade" ) )
	{
		blade = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "blade2" ) )
	{
		blade2 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CCar::KeyValue( pkvd );
}

void CHelicopter::Precache( void )
{
	if( m_iszEngineSnd ) // blade sound basically
		PRECACHE_SOUND( (char *)STRING( m_iszEngineSnd ) );

	if( m_iszIdleSnd )
		PRECACHE_SOUND( (char *)STRING( m_iszIdleSnd ) );

	PRECACHE_SOUND( "func_car/car_impact1.wav" );
	PRECACHE_SOUND( "func_car/car_impact2.wav" );
	PRECACHE_SOUND( "func_car/car_impact3.wav" );
	PRECACHE_SOUND( "func_car/car_impact4.wav" );
	PRECACHE_SOUND( "func_car/eng_start.wav" );
	PRECACHE_SOUND( "func_car/eng_stop.wav" );
	PRECACHE_SOUND( "func_car/splash1.wav" );
	PRECACHE_SOUND( "func_car/splash2.wav" );
	PRECACHE_SOUND( "func_car/splash3.wav" );

//	PRECACHE_SOUND( "func_car/tires/water.wav" );
	PRECACHE_SOUND( "func_car/metal_drag.wav" );

	UTIL_PrecacheOther( "hvr_rocket" );

	m_iSpriteTexture = PRECACHE_MODEL( "sprites/white.spr" );
	m_iExplode = PRECACHE_MODEL( "sprites/fexplo.spr" );
	PRECACHE_SOUND( "weapons/mortarhit.wav" );

	PRECACHE_SOUND( "drone/drone_hit1.wav" );
	PRECACHE_SOUND( "drone/drone_hit2.wav" );
	PRECACHE_SOUND( "drone/drone_hit3.wav" );
	PRECACHE_SOUND( "drone/drone_hit4.wav" );
	PRECACHE_SOUND( "drone/drone_hit5.wav" );
}

void CHelicopter::Spawn( void )
{
	Precache();

	IsHeli = true;

	if( !MaxCarSpeed )
		MaxCarSpeed = 100;
	if( !AccelRate )
		AccelRate = 200;
	if( !BrakeRate )
		BrakeRate = 800;
	if( !TurningRate )
		TurningRate = 500;
	
	MaxTurn = 50;

	if( !HeliWidth )
		HeliWidth = 40;
	if( !HeliLength )
		HeliLength = 200;

	MaxLean = 30;

	if( !DamageMult )
		DamageMult = 1.0f;

	MaxGears = 1;

	// convert km/h to units
	MaxCarSpeed = MaxCarSpeed * 15;
	GearStep = MaxCarSpeed / MaxGears;

	pev->movetype = MOVETYPE_PUSHSTEP;
	pev->solid = SOLID_SLIDEBOX;
	pev->gravity = 0.00001;
	SET_MODEL( edict(), GetModel() );
	UTIL_SetSize( pev, Vector( -32, -32, 0 ), Vector( 32, 32, 32 ) );

	SetLocalAngles( m_vecTempAngles );
	m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity( this );

	AllowCamera = true;

	PrevOrigin = GetAbsOrigin();

	SafeSpawnPos = g_vecZero;

	SetThink( &CHelicopter::Setup );
	SetNextThink( RANDOM_FLOAT( 0.1f, 0.2f ) );
}

void CHelicopter::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( !pActivator || !pActivator->IsPlayer() )
		pActivator = UTIL_PlayerByIndex( 1 );

	CBasePlayer *pPlayer = (CBasePlayer *)pActivator;

	if( IsLockedByMaster() && (useType != USE_OFF) )
	{
		if( pev->message )
			UTIL_ShowMessage( STRING( pev->message ), pPlayer );
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
			UTIL_ShowMessage( "The vehicle is occupied!", pPlayer );
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
			if( m_iszEngineSnd )
			{
				EMIT_SOUND( edict(), CHAN_STATIC, "func_car/eng_stop.wav", VOL_NORM, ATTN_NORM );
			}

			if( pCarHurt )
				pCarHurt->pev->frags = 0;

			pev->owner = NULL;
			UTIL_FireTargets( pev->target, pPlayer, this, USE_TOGGLE, 0 );
			time = gpGlobals->time;
			pPlayer->pCar = NULL;
			hDriver = NULL;
			if( pDriverMdl )
				pDriverMdl->pev->effects |= EF_NODRAW;
			SetThink( &CHelicopter::Idle );

			// clear tires' sound
			EMIT_SOUND( edict(), CHAN_ITEM, "common/null.wav", 0, 3.0 );

			AccelAddX = 0;

			SetNextThink( 0 );
		}
	}
	else if( pPlayer->pCar == NULL || useType == USE_ON ) // player not in car, enter this car
	{
		if( pPlayer->m_pHoldableItem != NULL )
			return;

		if( fabs( CarSpeed ) > 300 )
			return;

		if( pev->deadflag == DEAD_DEAD ) // exploded
			return;

		pPlayer->pCar = this;

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

			// no gears to show
			MESSAGE_BEGIN( MSG_ONE, gmsgTempEnt, NULL, hDriver->pev );
				WRITE_BYTE( TE_CARPARAMS );
				WRITE_BYTE( 0 );
			MESSAGE_END();

			EMIT_SOUND( edict(), CHAN_STATIC, "func_car/eng_start.wav", VOL_NORM, ATTN_NORM );
			if( m_iszIdleSnd )
				STOP_SOUND( edict(), CHAN_WEAPON, STRING( m_iszIdleSnd ) );
			SetThink( &CHelicopter::Drive );
			UTIL_FireTargets( pev->target, pPlayer, this, USE_TOGGLE, 0 );
			time = gpGlobals->time;
			DriverMdlSequence = -1;
			CameraAngles = GetAbsAngles(); // make sure camera is angled properly when we enter the vehicle
			NewCameraAngle = CameraAngles.y;

			SetNextThink( 0 );
		}
	}
	else
		ALERT( at_aiconsole, "CHelicopter: Player is already in another vehicle!\n" );
}

void CHelicopter::Setup( void )
{
	pChassis = UTIL_FindEntityByTargetname( NULL, STRING( chassis ) );
	if( pChassis )
		pChassis->SetParent( this );
	else
	{
		ALERT( at_error, "HELICOPTER WITH NO CHASSIS!\n" );
		UTIL_Remove( this );
		return;
	}

	pCarHurt = UTIL_FindEntityByTargetname( NULL, STRING( carhurt ) );
	pDriverMdl = (CBaseAnimating *)UTIL_FindEntityByTargetname( NULL, STRING( drivermdl ) );
	pChassisMdl = (CBaseAnimating *)UTIL_FindEntityByTargetname( NULL, STRING( chassismdl ) );
	pCamera1 = CBaseEntity::Create( "info_target", GetAbsOrigin(), GetAbsAngles(), edict() );
	pCamera2 = UTIL_FindEntityByTargetname( NULL, STRING( camera2 ) );
	pBlade = UTIL_FindEntityByTargetname( NULL, STRING( blade ) );
	pBlade2 = UTIL_FindEntityByTargetname( NULL, STRING( blade2 ) );

	if( pChassisMdl )
	{
		pChassisMdl->pev->owner = edict();
		UTIL_SetSize( pChassisMdl->pev, Vector( -16, -16, 0 ), Vector( 16, 16, 16 ) );
		pChassisMdl->pev->takedamage = DAMAGE_YES;
		pChassisMdl->pev->spawnflags |= BIT( 31 ); // SF_ENVMODEL_OWNERDAMAGE
		pChassisMdl->RelinkEntity( FALSE );
	}
	else
		ALERT( at_warning, "func_helicopter \"%s\" doesn't have body model entity specified, collision won't work properly.\n", GetTargetname() );

	if( pCamera1 )
	{
		pCamera1->SetNullModel();
		pCamera1->pev->effects |= EF_SKIPPVS;
		if( !CameraDistance )
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
				CameraHeight = 150;
		}
	}

	if( pCamera2 )
	{
		if( !pCamera2->m_iParent )
			pCamera2->SetParent( this );
		pCamera2->SetNullModel();
		Camera2LocalOrigin = pCamera2->GetLocalOrigin();
	}

	if( pBlade )
	{
		if( pChassis )
			pBlade->SetParent( pChassis );
		else
			pBlade->SetParent( this );
		pBlade->pev->movetype = MOVETYPE_NOCLIP;
		#ifdef USE_LOCAL_ROTATION
		pBlade->pev->effects |= EF_ROTATING;
		pBlade->pev->iuser1 = 1;
		#endif
	}

	if( pBlade2 )
	{
		if( pChassis )
			pBlade2->SetParent( pChassis );
		else
			pBlade2->SetParent( this );
		pBlade2->pev->movetype = MOVETYPE_NOCLIP;
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

	pFreeCam = CBaseEntity::Create( "info_target", GetAbsOrigin(), GetAbsAngles(), edict() );
	if( pFreeCam )
	{
		pFreeCam->SetNullModel();
		pFreeCam->SetParent( this );
		if( pChassisMdl )
		{
			// get camera distance according to model bounds
			Vector mins = g_vecZero;
			Vector maxs = g_vecZero;
			UTIL_GetModelBounds( pChassisMdl->pev->modelindex, mins, maxs );
			pFreeCam->pev->iuser1 = (int)((mins - maxs).Length() * 0.75f * pChassisMdl->pev->scale);
		}
		else
			pFreeCam->pev->iuser1 = 100; // distance from heli, because it's bigger
		pFreeCam->pev->effects |= EF_SKIPPVS;
		if( !FreeCameraDistance )
			FreeCameraDistance = CameraDistance;
	}

	if( pev->iuser1 ) // rotate vehicle
	{
		Vector ang = GetAbsAngles();
		ang.y += pev->iuser1;
		SetAbsAngles( ang );
	}

	pev->flags &= ~FL_ONGROUND; // !!! this resets position so the vehicle won't hang in air...

	SetThink( &CHelicopter::Idle );
	SetNextThink( RANDOM_FLOAT(0.2f, 0.5f) );
}

void CHelicopter::Drive( void )
{
	if( hDriver == NULL )
	{
		SetThink( &CHelicopter::Idle );
		SetNextThink( 0 );
		return;
	}
	
	// reset all velocity from previous frame
	pev->velocity = g_vecZero;

	// will need those later!
	UTIL_MakeVectors( pChassis->GetAbsAngles() );
	Vector ChassisForw = gpGlobals->v_forward;
	Vector ChassisUp = gpGlobals->v_up;
	Vector ChassisRight = gpGlobals->v_right;

	// now make the heli's vectors
	UTIL_MakeVectors( GetAbsAngles() );

	// TEMP !!! (or not.)
	Vector PlayerPos;
	if( pDriverMdl )
	{
		PlayerPos = pDriverMdl->GetAbsOrigin();
	}
	else
		PlayerPos = GetAbsOrigin() + gpGlobals->v_up * 40;
	hDriver->SetAbsOrigin( PlayerPos );
	hDriver->SetAbsVelocity( g_vecZero );

	float AbsCarSpeed = fabs( CarSpeed );

	// motion blur
	hDriver->pev->vuser1.y = AbsCarSpeed;
	
	// achievement UNDONE
	/*
	if( !g_pGameRules->IsMultiplayer() )
	{
		float Distance = (GetAbsOrigin() - PrevOrigin).Length();
		Distance *= 0.01905f; // convert units to meters
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( hDriver->entindex() );
		plr->AchievementStats[ACH_WATERJETDISTANCE] += Distance;
		PrevOrigin = GetAbsOrigin();
	}*/

	//----------------------------
	// turn
	//----------------------------
	// Forward turning controls if going backwards
	int Forward = 1;
	if( ((bBack()) && (CarSpeed < 0.0f)) || (CarSpeed < 0.0f) )
		Forward = -1;
	else if( CarSpeed == 0.0f )
		Forward = 0;

	bool InAir = !(pev->flags & FL_ONGROUND);

	float TurnRate = (0.1 * TurningRate) / (1 + AbsCarSpeed * 0.2);
	TurnRate = Forward * bound( 0, TurnRate, 20 );
	if( CarSpeed == 0.0f )
		TurnRate = 1;
	int CameraSwayRate = 0;
	if( SecondaryCamera )
		CameraSwayRate = MaxCamera2Sway;

	if( InAir ) // heli can always turn
	{
		if( bRight() )
		{
			Turning -= TurnRate * gpGlobals->frametime;
			if( CameraSwayRate > 0 )
				CameraMoving -= CameraSwayRate * gpGlobals->frametime;
		}
		else if( bLeft() )
		{
			Turning += TurnRate * gpGlobals->frametime;
			if( CameraSwayRate > 0 )
				CameraMoving += CameraSwayRate * gpGlobals->frametime;
		}

		// no turning buttons pressed, go to zero slowly
		if( !(bLeft()) && !(bRight()) )
		{
			Turning = UTIL_Approach( 0, Turning, fabs(TurnRate) * gpGlobals->frametime );
			if( CameraSwayRate > 0 )
				CameraMoving = UTIL_Approach( 0, CameraMoving, CameraSwayRate * 0.5 * gpGlobals->frametime );

			if( fabs( Turning ) <= 0.001f )
				Turning = 0;
		}
	}
	else
		Turning = 0;
	
	float max_turn_val = 1 / (1 + AbsCarSpeed * 0.01); // magic
	Turning = bound( -max_turn_val, Turning, max_turn_val );

	int CameraMovingBound = 0;
	if( SecondaryCamera )
		CameraMovingBound = MaxCamera2Sway;

	CameraMoving = bound( -CameraMovingBound, CameraMoving, CameraMovingBound );

	float HeliTurnRate = bound( 0.05, AbsCarSpeed * 0.00075, 1 );

	Vector HeliAng = GetLocalAngles();

	//	Y - turn left and right
	float trn = (10 * MaxTurn) * (Turning * HeliTurnRate) * gpGlobals->frametime;
	HeliAng.y += trn;

	//----------------------------
	// the blade
	//----------------------------
	// add blade speed if player presses gas
	if( ((bForward()) || (bBack()) || (bUp())) && InAir )
	{
		BladeAddVel += 100 * gpGlobals->frametime;
		// special case when going up, add more boost
		if( bUp() )
			BladeAddVel += 100 * gpGlobals->frametime;
	}
	else
		BladeAddVel = UTIL_Approach( 0, BladeAddVel, 100 * gpGlobals->frametime );

	BladeAddVel = bound( 0, BladeAddVel, 200 );

	if( BladeSpeed < MAX_BLADE_SPEED + BladeAddVel )
		BladeSpeed += 500 * gpGlobals->frametime;
	else if( BladeSpeed > MAX_BLADE_SPEED + BladeAddVel )
		BladeSpeed = MAX_BLADE_SPEED + BladeAddVel;

	if( pBlade )
	{
		pBlade->SetLocalAvelocity( Vector(0,BladeSpeed,0) );
	}
	if( pBlade2 )
		pBlade2->SetLocalAvelocity( Vector( BladeSpeed, 0, 0 ) );

	//----------------------------
	// engine sound
	//----------------------------
	EngPitch = BladeSpeed * 0.1;
	int PitchLimit = (MAX_BLADE_SPEED + BladeAddVel) * 0.1;
	EngPitch = bound( 0, EngPitch, PitchLimit );
	if( BrokenCar )
		EngPitch *= 0.65;
	if( m_iszEngineSnd )
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, STRING( m_iszEngineSnd ), 1, 0.25, SND_CHANGE_PITCH, (int)EngPitch );

	//----------------------------
	// collision
	//----------------------------
	TraceResult trCol;
	Vector Collision = g_vecZero;
	Vector ColPoint = g_vecZero; // place for sparks
	Vector ColPointLeft = g_vecZero;
	Vector ColPointRight = g_vecZero;

	// 4 collision points from corners.
	Vector ColPointWFR = g_vecZero;
	Vector ColPointWFL = g_vecZero;
	Vector ColPointWRR = g_vecZero;
	Vector ColPointWRL = g_vecZero;

	float ColDotProduct = 1.0f;

	bool hit_carblocker = false;

	int magic = 10;
	if( Forward == 1 || (!InAir && Forward == -1) )
	{
		// front right corner
		UTIL_TraceLine( GetAbsOrigin() + ChassisUp * magic + ChassisRight * HeliWidth * 0.5, GetAbsOrigin() + ChassisUp * magic + ChassisRight * HeliWidth * 0.5 + ChassisForw * HeliLength * 0.5, ignore_monsters, edict(), &trCol );
		ColPointWFR = trCol.vecEndPos;
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_BOATBLOCKER )
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

		// front left corner
		UTIL_TraceLine( GetAbsOrigin() + ChassisUp * magic - ChassisRight * HeliWidth * 0.5, GetAbsOrigin() + ChassisUp * magic - ChassisRight * HeliWidth * 0.5 + ChassisForw * HeliLength * 0.5, ignore_monsters, edict(), &trCol );
		ColPointWFL = trCol.vecEndPos;
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_BOATBLOCKER )
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

	if( Forward == -1 || (!InAir && Forward == 1) )
	{
		// rear right corner
		UTIL_TraceLine( GetAbsOrigin() + ChassisUp * magic + ChassisRight * HeliWidth * 0.5, GetAbsOrigin() + ChassisUp * magic + ChassisRight * HeliWidth * 0.5 - ChassisForw * HeliLength * 0.5, ignore_monsters, edict(), &trCol );
		ColPointWRR = trCol.vecEndPos;
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_BOATBLOCKER )
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

		// rear left corner
		UTIL_TraceLine( GetAbsOrigin() + ChassisUp * magic - ChassisRight * HeliWidth * 0.5, GetAbsOrigin() + ChassisUp * magic - ChassisRight * HeliWidth * 0.5 - ChassisForw * HeliLength * 0.5, ignore_monsters, edict(), &trCol );
		ColPointWRL = trCol.vecEndPos;
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_BOATBLOCKER )
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
	UTIL_TraceLine( GetAbsOrigin() + gpGlobals->v_up * magic, GetAbsOrigin() + gpGlobals->v_up * magic - gpGlobals->v_right * HeliWidth * 0.5, ignore_monsters, edict(), &trCol );
	if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_BOATBLOCKER )
		hit_carblocker = true;
	if( trCol.flFraction < 1.0f || hit_carblocker )
		ColPointLeft = gpGlobals->v_right * 100;
	hit_carblocker = false;
	// right
	UTIL_TraceLine( GetAbsOrigin() + gpGlobals->v_up * magic, GetAbsOrigin() + gpGlobals->v_up * magic + gpGlobals->v_right * HeliWidth * 0.5, ignore_monsters, edict(), &trCol );
	if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_BOATBLOCKER )
		hit_carblocker = true;
	if( trCol.flFraction < 1.0f || hit_carblocker )
		ColPointRight = -gpGlobals->v_right * 100;

	Vector SideCollision = ColPointLeft + ColPointRight;

	// adjust velocity according to collision...
	pev->velocity += Collision + SideCollision;

	hit_carblocker = false;

	//----------------------------
	// chassis inclination
	//----------------------------
	Vector ChassisAng = pChassis->GetLocalAngles();

	// X - transversal rotation
	// lean heli when accelerating
	if( CarSpeed > 0 && (bForward()) )
	{
		AccelAddX += AccelRate * 0.01 * gpGlobals->frametime;
		AccelAddX = bound( -MaxLean, AccelAddX, MaxLean );
	}
	else if( CarSpeed < 0 && !(bForward()) && (bBack()) )
	{
		AccelAddX -= AccelRate * 0.02 * gpGlobals->frametime;
		AccelAddX = bound( -MaxLean, AccelAddX, MaxLean );
	}
	else
		AccelAddX = UTIL_Approach( 0, AccelAddX, AccelRate * (1/(10 + (AbsCarSpeed * 0.1))) * gpGlobals->frametime );

	float NewChassisAngX = AccelAddX;
	ChassisAng.x = NewChassisAngX;

	// Z - lateral rotation
	ChassisAng.z = UTIL_Approach( CarSpeed * -Turning * (MaxLean * 0.01), ChassisAng.z, 100 * gpGlobals->frametime);

	if( !InAir )
	{
		ChassisAng.x = UTIL_Approach( 0, ChassisAng.x, 50 * gpGlobals->frametime );
		ChassisAng.z = UTIL_Approach( 0, ChassisAng.z, 50 * gpGlobals->frametime );
	}

	ChassisAng.x = bound( -MaxLean, ChassisAng.x, MaxLean );
	ChassisAng.z = bound( -MaxLean, ChassisAng.z, MaxLean );
	pChassis->SetLocalAngles( ChassisAng );

	//----------------------------
	// accelerate/brake
	//----------------------------
	float ActualAccelRate = AccelRate / (1 + (AbsCarSpeed / MaxCarSpeed));
	bool CanFlyVertical = (BladeSpeed >= MAX_BLADE_SPEED);
	bool CanFlyHorizontal = (BladeSpeed >= MAX_BLADE_SPEED && InAir);

	if( (!(bForward()) && !(bBack())) || BrokenCar || IsLockedByMaster() )
	{
		int FrictionMult = 1;
		if( !InAir )
			FrictionMult = 5;
		
		// no accelerate/brake buttons pressed
		if( CarSpeed >= 20.0f )
			CarSpeed -= FrictionMult * 150 * gpGlobals->frametime;
		else if( CarSpeed <= -20.0f )
			CarSpeed += FrictionMult * 150 * gpGlobals->frametime;

		if( CarSpeed > -20 && CarSpeed < 20 )
			CarSpeed = 0.0f;
	}
	else if( bForward() || bBack() )
	{
		if( CanFlyHorizontal )
		{
			if( bForward() )
			{
				CarSpeed += ActualAccelRate * (1 - fabs( Turning ) * 0.5) * gpGlobals->frametime;
			}
			else if( bBack() )
			{
				if( CarSpeed > 0 )
					CarSpeed -= BrakeRate * gpGlobals->frametime;
				else
				{
					CarSpeed -= ActualAccelRate * (1 - fabs( Turning ) * 0.5) * gpGlobals->frametime;
				}
			}
		}
		else
			CarSpeed = 0;
	}

	if( CanFlyVertical )
	{
		if( bUp() )
		{
			VerticalVelocity += 300 * (1 - fabs( Turning ) * 0.5) * gpGlobals->frametime;
		}
		else if( bDown() )
		{
			VerticalVelocity -= 500 * (1 - fabs( Turning ) * 0.5) * gpGlobals->frametime;
		}
		else
			VerticalVelocity = UTIL_Approach( 0, VerticalVelocity, 150 * gpGlobals->frametime );

		VerticalVelocity = bound( -MAX_VERTICAL_VELOCITY, VerticalVelocity, MAX_VERTICAL_VELOCITY );
	}
	else
		VerticalVelocity = 0;

	CarSpeed = bound( -MaxCarSpeed + (MaxCarSpeed * fabs( Turning ) * 0.25), CarSpeed, MaxCarSpeed - (MaxCarSpeed * fabs( Turning ) * 0.25) );

	// apply main movement
	pev->velocity += gpGlobals->v_forward * CarSpeed + gpGlobals->v_up * VerticalVelocity;

	//----------------------------
	// timed effects
	//----------------------------
	bool TookDamage = false;
	float DriverDamage = 0.0f;
	bool Collided = false;
	bool Explode = false;

	// explode if model took damage
	if( pChassisMdl )
	{
		if( pChassisMdl->pev->deadflag == DEAD_DEAD )
			Explode = true;
	}

	// do timing stuff each 0.5 seconds
	if( (Collision.Length() > 0.01f) && (gpGlobals->time > time) )
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
			DriverDamage = AbsCarSpeed * 0.05 * ColDotProduct;
		}

		if( AbsCarSpeed > 200 )
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

		time = gpGlobals->time + 0.5;
	}

	if( pev->flags & FL_ONGROUND )
		VerticalVelocity = 0.0f;

	Camera();

	//----------------------------
	// driver model
	//----------------------------
	if( pDriverMdl )
	{
		int f = Forward;
		if( f == 0 ) f = 1; // ¯\_(ツ)_/¯
		pDriverMdl->SetBlending( 0, -f * 100 * Turning );

		bool SpecialAnim = false;

		if( (CarSpeed < 25) && (bBack()) && !(pev->flags & FL_ONGROUND) )
			DriverMdlSequence = pDriverMdl->LookupSequence( "lookback" );
		else if( InAir && !(pev->flags & FL_ONGROUND) )
			DriverMdlSequence = pDriverMdl->LookupSequence( "Tpamplin" );
		else if( CarSpeed >= 0 )
			DriverMdlSequence = pDriverMdl->LookupSequence( "idle" );

		// special anims, reserve a second
		if( (ColPoint.Length() > 0) && (AbsCarSpeed > 200) )
		{
			SpecialAnim = true;

			if( CarSpeed > 0 )
				DriverMdlSequence = pDriverMdl->LookupSequence( "damag" );
			else if( CarSpeed < -0 )
				DriverMdlSequence = pDriverMdl->LookupSequence( "damagback" );
		}

		if( TookDamage )
		{
			DriverMdlSequence = pDriverMdl->LookupSequence( "damag" );
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

	if( (gpGlobals->time > LastShootTime + 0.5) && (hDriver->pev->button & IN_ATTACK) )
	{
		Vector RocketOrg = GetAbsOrigin() + gpGlobals->v_forward * 200 - gpGlobals->v_up * 30;
		Vector RocketAng = pChassis->GetAbsAngles();
		CBaseEntity *pRocket = CBaseEntity::Create( "hvr_rocket", RocketOrg, RocketAng, hDriver->edict() );

		if( pRocket )
		{
			MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, RocketOrg );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( RocketOrg.x );
			WRITE_COORD( RocketOrg.y );
			WRITE_COORD( RocketOrg.z );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( 20 ); // scale * 10
			WRITE_BYTE( 12 ); // framerate
			WRITE_BYTE( 10 ); // pos randomize
			MESSAGE_END();

			pRocket->SetAbsVelocity( ChassisForw * (500 + AbsCarSpeed) );
		}

		LastShootTime = gpGlobals->time;
	}

	//----------------------------
	// chassis (body) model
	//----------------------------
	if( pChassisMdl )
	{
		// turn the wheel
		if( pChassisMdl->pev->sequence != 0 )
			pChassisMdl->pev->sequence = 0;

		int k = Forward;
		if( k == 0 ) k = 1;
		pChassisMdl->SetBlending( 0, -k * Turning * 100 );
	}

	SetLocalAngles( HeliAng ); // car direction is now set

	if( Collided )
		CarSpeed *= -0.15 / ColDotProduct;

	if( TookDamage && DriverDamage > 10 )
	{
		hDriver->TakeDamage( pev, pev, DriverDamage, DMG_CRUSH );
		UTIL_ScreenShakeLocal( hDriver, GetAbsOrigin(), AbsCarSpeed * 0.01, 100, 1, 300, true );
	}
	// !!! hDriver can become NULL after this, by taking damage and dying, game will crash.

	SetNextThink( 0 );
}

void CHelicopter::Idle( void )
{
	// reset all velocity from previous frame
	pev->velocity = g_vecZero;

	// will need those later!
	UTIL_MakeVectors( pChassis->GetAbsAngles() );
	Vector ChassisForw = gpGlobals->v_forward;
	Vector ChassisUp = gpGlobals->v_up;
	Vector ChassisRight = gpGlobals->v_right;

	// now make the heli's vectors
	UTIL_MakeVectors( GetAbsAngles() );

	float AbsCarSpeed = fabs( CarSpeed );

	//----------------------------
	// turn
	//----------------------------
	// Forward turning controls if going backwards
	int Forward = 1;
	if( CarSpeed == 0.0f )
		Forward = 0;

	bool InAir = !(pev->flags & FL_ONGROUND);

//	float TurnRate = TurningRate / (1 + AbsCarSpeed);
//	TurnRate = Forward * bound( 0, TurnRate, 20 );
//	if( CarSpeed == 0.0f )
//		TurnRate = 1;

	Turning = 0;

	float HeliTurnRate = bound( 0.05, AbsCarSpeed * 0.00075, 1 );

	Vector HeliAng = GetLocalAngles();

	//	Y - turn left and right
	float trn = (10 * MaxTurn) * (Turning * HeliTurnRate) * gpGlobals->frametime;
	HeliAng.y += trn;

	//----------------------------
	// the blade
	//----------------------------
	if( BladeSpeed > 0 )
		BladeSpeed -= 200 * gpGlobals->frametime;
	else if( BladeSpeed < 0 )
		BladeSpeed = 0;

	if( pBlade )
	{
#ifdef USE_LOCAL_ROTATION
		pBlade->pev->iuser2 = BladeSpeed;
#else
		pBlade->SetLocalAvelocity( Vector( 0, BladeSpeed, 0 ) );
#endif
	}
	if( pBlade2 )
		pBlade2->SetLocalAvelocity( Vector( BladeSpeed, 0, 0 ) );

	//----------------------------
	// collision
	//----------------------------
	TraceResult trCol;
	Vector Collision = g_vecZero;
	Vector ColPoint = g_vecZero; // place for sparks
	Vector ColPointLeft = g_vecZero;
	Vector ColPointRight = g_vecZero;

	// 4 collision points from corners.
	Vector ColPointWFR = g_vecZero;
	Vector ColPointWFL = g_vecZero;
	Vector ColPointWRR = g_vecZero;
	Vector ColPointWRL = g_vecZero;

	float ColDotProduct = 1.0f;

	bool hit_carblocker = false;

	int magic = 10;
	if( Forward == 1 || (!InAir && Forward == -1) )
	{
		// front right corner
		UTIL_TraceLine( GetAbsOrigin() + ChassisUp * magic + ChassisRight * HeliWidth * 0.5, GetAbsOrigin() + ChassisUp * magic + ChassisRight * HeliWidth * 0.5 + ChassisForw * HeliLength * 0.5, ignore_monsters, edict(), &trCol );
		ColPointWFR = trCol.vecEndPos;
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_BOATBLOCKER )
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

		// front left corner
		UTIL_TraceLine( GetAbsOrigin() + ChassisUp * magic - ChassisRight * HeliWidth * 0.5, GetAbsOrigin() + ChassisUp * magic - ChassisRight * HeliWidth * 0.5 + ChassisForw * HeliLength * 0.5, ignore_monsters, edict(), &trCol );
		ColPointWFL = trCol.vecEndPos;
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_BOATBLOCKER )
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

	if( Forward == -1 || (!InAir && Forward == 1) )
	{
		// rear right corner
		UTIL_TraceLine( GetAbsOrigin() + ChassisUp * magic + ChassisRight * HeliWidth * 0.5, GetAbsOrigin() + ChassisUp * magic + ChassisRight * HeliWidth * 0.5 - ChassisForw * HeliLength * 0.5, ignore_monsters, edict(), &trCol );
		ColPointWRR = trCol.vecEndPos;
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_BOATBLOCKER )
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

		// rear left corner
		UTIL_TraceLine( GetAbsOrigin() + ChassisUp * magic - ChassisRight * HeliWidth * 0.5, GetAbsOrigin() + ChassisUp * magic - ChassisRight * HeliWidth * 0.5 - ChassisForw * HeliLength * 0.5, ignore_monsters, edict(), &trCol );
		ColPointWRL = trCol.vecEndPos;
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_BOATBLOCKER )
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
	UTIL_TraceLine( GetAbsOrigin() + gpGlobals->v_up * magic, GetAbsOrigin() + gpGlobals->v_up * magic - gpGlobals->v_right * HeliWidth * 0.5, ignore_monsters, edict(), &trCol );
	if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_BOATBLOCKER )
		hit_carblocker = true;
	if( trCol.flFraction < 1.0f || hit_carblocker )
		ColPointLeft = gpGlobals->v_right * 100;
	hit_carblocker = false;
	// right
	UTIL_TraceLine( GetAbsOrigin() + gpGlobals->v_up * magic, GetAbsOrigin() + gpGlobals->v_up * magic + gpGlobals->v_right * HeliWidth * 0.5, ignore_monsters, edict(), &trCol );
	if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_BOATBLOCKER )
		hit_carblocker = true;
	if( trCol.flFraction < 1.0f || hit_carblocker )
		ColPointRight = -gpGlobals->v_right * 100;

	Vector SideCollision = ColPointLeft + ColPointRight;

	// adjust velocity according to collision...
	pev->velocity += Collision * 2 + SideCollision;

	hit_carblocker = false;

	//----------------------------
	// chassis inclination
	//----------------------------
	Vector ChassisAng = pChassis->GetLocalAngles();

	// X - transversal rotation
	float NewChassisAngX = 0;
	ChassisAng.x = UTIL_Approach( NewChassisAngX, ChassisAng.x, 100 * gpGlobals->frametime );

	// Z - lateral rotation
	float NewChassisAngZ = 0;
	ChassisAng.z = UTIL_Approach( NewChassisAngZ, ChassisAng.z, 50 * gpGlobals->frametime );

	ChassisAng.x = bound( -MaxLean, ChassisAng.x, MaxLean );
	ChassisAng.z = bound( -MaxLean, ChassisAng.z, MaxLean );
	pChassis->SetLocalAngles( ChassisAng );

	//----------------------------
	// accelerate/brake
	//----------------------------

	// no accelerate/brake buttons pressed
	if( CarSpeed >= 20.0f )
		CarSpeed -= 150 * gpGlobals->frametime;
	else if( CarSpeed <= -20.0f )
		CarSpeed += 150 * gpGlobals->frametime;

	if( CarSpeed > -20 && CarSpeed < 20 )
		CarSpeed = 0.0f;

	if( InAir )
	{
		VerticalVelocity -= g_psv_gravity->value * gpGlobals->frametime;
		VerticalVelocity = bound( -MAX_VERTICAL_VELOCITY * 5, VerticalVelocity, MAX_VERTICAL_VELOCITY );
	}
	else
	{
		if( VerticalVelocity < -MAX_VERTICAL_VELOCITY * 3 ) // falling down with no pilot
			TakeDamage( pev, pev, -VerticalVelocity, DMG_FALL );

		VerticalVelocity = 0;
	}

	CarSpeed = bound( -MaxCarSpeed + (MaxCarSpeed * fabs( Turning ) * 0.25), CarSpeed, MaxCarSpeed - (MaxCarSpeed * fabs( Turning ) * 0.25) );

	// apply main movement
	pev->velocity += gpGlobals->v_forward * CarSpeed + gpGlobals->v_up * VerticalVelocity;

	//----------------------------
	// timed effects
	//----------------------------
	bool Collided = false;

	// do timing stuff each 0.5 seconds
	if( (Collision.Length() > 0.01f) && (gpGlobals->time > time) )
	{
		if( ColDotProduct < 0 && ColDotProduct > -0.18 )
			ColDotProduct = -0.18;
		else if( ColDotProduct > 0 && ColDotProduct < 0.18 )
			ColDotProduct = 0.18;
		// 0.18 so the multiplier would be less than 1 always
	//	CarSpeed *= -0.15 / ColDotProduct; // moved to bottom
		Collided = true;


		if( AbsCarSpeed > 200 )
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

		time = gpGlobals->time + 0.5;
	}

	//----------------------------
	// chassis (body) model
	//----------------------------
	if( pChassisMdl )
	{
		// turn the wheel
		if( pChassisMdl->pev->sequence != 0 )
			pChassisMdl->pev->sequence = 0;

		int k = Forward;
		if( k == 0 ) k = 1;
		pChassisMdl->SetBlending( 0, -k * Turning * 100 );
	}

	SetLocalAngles( HeliAng ); // car direction is now set

	if( Collided )
		CarSpeed *= -0.15 / ColDotProduct;

	//----------------------------
	// engine sound and idle sound
	//----------------------------
	EngPitch = BladeSpeed * 0.1;
	EngPitch = bound( 0, EngPitch, 100 );
	if( BrokenCar )
		EngPitch *= 0.65;
	if( m_iszEngineSnd && BladeSpeed > 0 && EngPitch >= 2.0f )
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, STRING( m_iszEngineSnd ), 1, ATTN_NORM, SND_CHANGE_PITCH, (int)EngPitch );
	else
	{
		if( m_iszIdleSnd )
			EMIT_SOUND_DYN( edict(), CHAN_WEAPON, STRING( m_iszIdleSnd ), 1, 2.2, SND_CHANGE_PITCH, PITCH_NORM );
		else
			STOP_SOUND( edict(), CHAN_WEAPON, STRING( m_iszEngineSnd ) );
	}

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

void CHelicopter::CarExplode(void)
{
	pev->deadflag = DEAD_DEAD;

	if( pChassisMdl )
	{
		pChassisMdl->pev->rendermode = kRenderTransColor;
		pChassisMdl->pev->renderamt = 255;
		pChassisMdl->pev->rendercolor = Vector( 40, 40, 40 );
	}
	if( pBlade )
	{
		if( pBlade->m_hChild != NULL )
			UTIL_Remove( pBlade->m_hChild );

		UTIL_Remove( pBlade );
	}
	if( pBlade2 )
	{
		if( pBlade2->m_hChild != NULL )
			UTIL_Remove( pBlade2->m_hChild );

		UTIL_Remove( pBlade2 );
	}
	
	Vector vecOrigin = GetAbsOrigin();
	Vector vecSpot = vecOrigin + (pev->mins + pev->maxs) * 0.5;

	/*
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_EXPLOSION);		// This just makes a dynamic light now
		WRITE_COORD( vecSpot.x );
		WRITE_COORD( vecSpot.y );
		WRITE_COORD( vecSpot.z + 300 );
		WRITE_SHORT( g_sModelIndexFireball );
		WRITE_BYTE( 250 ); // scale * 10
		WRITE_BYTE( 8  ); // framerate
	MESSAGE_END();
	*/

	// fireball
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSpot );
	WRITE_BYTE( TE_SPRITE );
	WRITE_COORD( vecSpot.x );
	WRITE_COORD( vecSpot.y );
	WRITE_COORD( vecSpot.z + 256 );
	WRITE_SHORT( m_iExplode );
	WRITE_BYTE( 120 ); // scale * 10
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

	RadiusDamage( vecOrigin, pev, pev, 300, 750, CLASS_NONE, DMG_BLAST );

	if(/*!(pev->spawnflags & SF_NOWRECKAGE) && */(pev->flags & FL_ONGROUND) )
	{
		CBaseEntity *pWreckage = Create( "cycler_wreckage", vecOrigin, GetAbsAngles() );
		// SET_MODEL( ENT(pWreckage->pev), STRING(pev->model) );
		UTIL_SetSize( pWreckage->pev, Vector( -200, -200, -128 ), Vector( 200, 200, -32 ) );
		pWreckage->pev->frame = pev->frame;
		pWreckage->pev->sequence = pev->sequence;
		pWreckage->pev->framerate = 0;
		pWreckage->pev->dmgtime = gpGlobals->time + 5;
	}

	UTIL_ScreenShake( GetAbsOrigin(), 10.0, 150.0, 1.0, 1200, true );

	if( hDriver != NULL ) // driver survived?!
		Use( hDriver, hDriver, USE_OFF, 0 );
}

bool CHelicopter::ExitCar( CBaseEntity *pPlayer )
{
	// set player position
	TraceResult TrExit;
	TraceResult VisCheck;
	UTIL_MakeVectors( pChassis->GetAbsAngles() );
	Vector HeliCenter = GetAbsOrigin() + gpGlobals->v_up * 38; // (human hull height / 2) + 2
	Vector vecSrc, vecEnd;

	// check left side of the car first
	vecSrc = HeliCenter - gpGlobals->v_right * (HeliWidth + 65);
	vecEnd = vecSrc;
	UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, human_hull, edict(), &TrExit );
	UTIL_TraceLine( HeliCenter, vecSrc, dont_ignore_monsters, dont_ignore_glass, edict(), &VisCheck );
	if( (TrExit.flFraction == 1.0f) && !TrExit.fAllSolid && (VisCheck.flFraction == 1.0f) )
	{
		pPlayer->SetAbsOrigin( vecSrc );
		return true;
	}

	// can't spawn there, now check right side
	vecSrc = HeliCenter + gpGlobals->v_right * (HeliWidth + 65);
	vecEnd = vecSrc;
	UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, human_hull, edict(), &TrExit );
	UTIL_TraceLine( HeliCenter, vecSrc, dont_ignore_monsters, dont_ignore_glass, edict(), &VisCheck );
	if( (TrExit.flFraction == 1.0f) && !TrExit.fAllSolid && (VisCheck.flFraction == 1.0f) )
	{
		pPlayer->SetAbsOrigin( vecSrc );
		return true;
	}

	// can't spawn both, try front
	vecSrc = HeliCenter + gpGlobals->v_forward * (HeliLength + 65);
	vecEnd = vecSrc;
	UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, human_hull, edict(), &TrExit );
	UTIL_TraceLine( HeliCenter, vecSrc, dont_ignore_monsters, dont_ignore_glass, edict(), &VisCheck );
	if( (TrExit.flFraction == 1.0f) && !TrExit.fAllSolid && (VisCheck.flFraction == 1.0f) )
	{
		pPlayer->SetAbsOrigin( vecSrc );
		return true;
	}

	// can't spawn, try back
	vecSrc = HeliCenter - gpGlobals->v_forward * (HeliLength + 65);
	vecEnd = vecSrc;
	UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, human_hull, edict(), &TrExit );
	UTIL_TraceLine( HeliCenter, vecSrc, dont_ignore_monsters, dont_ignore_glass, edict(), &VisCheck );
	if( (TrExit.flFraction == 1.0f) && !TrExit.fAllSolid && (VisCheck.flFraction == 1.0f) )
	{
		pPlayer->SetAbsOrigin( vecSrc );
		return true;
	}

	// can't spawn anywhere.
//	UTIL_ShowMessage( "Can't exit the vehicle here!", pPlayer );
	ALERT( at_error, "Can't exit the vehicle here!\n" );

	// FIXME just spawn on top, otherwise can be a softlock...
	pPlayer->SetAbsOrigin( HeliCenter + gpGlobals->v_up * 200 );
	return true;

	return false;
}

void CHelicopter::OnRemove( void )
{
	// if car is somehow deleted during drive, we need to remove the driver to prevent game crash
	if( hDriver != NULL )
		Use( hDriver, hDriver, USE_OFF, 0 );
}

void CHelicopter::ClearEffects( void )
{
	DontThink();

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
	if( pBlade )
		UTIL_Remove( pBlade, true );
}