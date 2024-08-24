#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "triggers.h"
#include "game/gamerules.h"

#define SF_BOAT_ONLYTRIGGER			BIT(0) // only trigger or external button

BEGIN_DATADESC( CBoat )
	DEFINE_KEYFIELD( BoatLength, FIELD_INTEGER, "boatlength" ),
	DEFINE_KEYFIELD( BoatWidth, FIELD_INTEGER, "boatwidth" ),
	DEFINE_KEYFIELD( BoatDepth, FIELD_INTEGER, "boatdepth" ),
	DEFINE_FIELD( AddZ, FIELD_FLOAT ),
	DEFINE_KEYFIELD( wheel1, FIELD_STRING, "wheel1" ),
	DEFINE_KEYFIELD( wheel2, FIELD_STRING, "wheel2" ),
	DEFINE_KEYFIELD( wheel3, FIELD_STRING, "wheel3" ),
	DEFINE_KEYFIELD( wheel4, FIELD_STRING, "wheel4" ),
	DEFINE_KEYFIELD( chassis, FIELD_STRING, "chassis" ),
	DEFINE_KEYFIELD( carhurt, FIELD_STRING, "carhurt" ),
	DEFINE_KEYFIELD( camera2, FIELD_STRING, "camera2" ),
	DEFINE_KEYFIELD( tank_tower, FIELD_STRING, "tank_tower" ),
	DEFINE_FIELD( Camera2LocalOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( Camera2LocalAngles, FIELD_VECTOR ),
	DEFINE_KEYFIELD( MaxCamera2Sway, FIELD_INTEGER, "maxcam2sway" ),
	DEFINE_KEYFIELD( drivermdl, FIELD_STRING, "drivermdl" ),
	DEFINE_KEYFIELD( chassismdl, FIELD_STRING, "chassismdl" ),
	DEFINE_KEYFIELD( MaxLean, FIELD_INTEGER, "maxlean" ),
	DEFINE_KEYFIELD( MaxCarSpeed, FIELD_INTEGER, "maxspeed" ),
	DEFINE_KEYFIELD( MaxCarSpeedBackwards, FIELD_INTEGER, "maxspeedback" ),
	DEFINE_KEYFIELD( AccelRate, FIELD_INTEGER, "accelrate" ),
	DEFINE_KEYFIELD( BackAccelRate, FIELD_INTEGER, "backaccelrate" ),
	DEFINE_KEYFIELD( BrakeRate, FIELD_INTEGER, "brakerate" ),
	DEFINE_KEYFIELD( TurningRate, FIELD_INTEGER, "turningrate" ),
	DEFINE_KEYFIELD( MaxTurn, FIELD_INTEGER, "maxturn" ),
	DEFINE_KEYFIELD( DamageMult, FIELD_FLOAT, "damagemult" ),
	DEFINE_KEYFIELD( CameraHeight, FIELD_INTEGER, "camheight" ),
	DEFINE_KEYFIELD( CameraDistance, FIELD_INTEGER, "camdistance" ),
	DEFINE_FIELD( hDriver, FIELD_EHANDLE ),
	DEFINE_FUNCTION( Setup ),
	DEFINE_FUNCTION( Drive ),
	DEFINE_FUNCTION( Idle ),
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
	DEFINE_FIELD( IsBoat, FIELD_BOOLEAN ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( func_boat, CBoat );

int CBoat::ObjectCaps( void )
{
	if( HasSpawnFlags( SF_BOAT_ONLYTRIGGER ) )
		return 0;

	return FCAP_IMPULSE_USE;
}

void CBoat::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "boatlength" ) )
	{
		BoatLength = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "boatwidth" ) )
	{
		BoatWidth = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "boatdepth" ) )
	{
		BoatDepth = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CCar::KeyValue( pkvd );
}

void CBoat::Precache( void )
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
	PRECACHE_SOUND( "func_car/eng_stop.wav" );
	PRECACHE_SOUND( "func_car/splash1.wav" );
	PRECACHE_SOUND( "func_car/splash2.wav" );
	PRECACHE_SOUND( "func_car/splash3.wav" );

	PRECACHE_SOUND( "func_car/tires/water.wav" );
	PRECACHE_SOUND( "func_car/metal_drag.wav" );

	PRECACHE_SOUND( "drone/drone_hit1.wav" );
	PRECACHE_SOUND( "drone/drone_hit2.wav" );
	PRECACHE_SOUND( "drone/drone_hit3.wav" );
	PRECACHE_SOUND( "drone/drone_hit4.wav" );
	PRECACHE_SOUND( "drone/drone_hit5.wav" );

	m_iSpriteTexture = PRECACHE_MODEL( "sprites/white.spr" );
	m_iExplode = PRECACHE_MODEL( "sprites/fexplo.spr" );
	PRECACHE_SOUND( "weapons/mortarhit.wav" );
}

void CBoat::Spawn( void )
{
	Precache();

	IsBoat = true;

	if( !MaxCarSpeed )
		MaxCarSpeed = 100;
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
		MaxTurn = 30;
	if( !BoatWidth )
		BoatWidth = 40;
	if( !BoatLength )
		BoatLength = 200;
	if( !BoatDepth )
		BoatDepth = 40;
	if( !MaxLean )
		MaxLean = 30;
	if( !DamageMult )
		DamageMult = 1.0f;
	if( !MaxGears )
		MaxGears = 1;

	// convert km/h to units
	MaxCarSpeed = MaxCarSpeed * 15;
	MaxCarSpeedBackwards = MaxCarSpeedBackwards * 15;
	GearStep = MaxCarSpeed / MaxGears;

	pev->movetype = MOVETYPE_PUSHSTEP;
	pev->solid = SOLID_SLIDEBOX;
	pev->gravity = 0.00001;
	SET_MODEL( edict(), GetModel() );
	UTIL_SetSize( pev, Vector( -8, -8, 0 ), Vector( 8, 8, 24 ) );

	SetLocalAngles( m_vecTempAngles );
	m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity( this );

	AllowCamera = true;

	if( pev->health > 0 )
		pev->takedamage = DAMAGE_YES;

	PrevOrigin = GetAbsOrigin();

	SafeSpawnPos = g_vecZero;

	SetThink( &CBoat::Setup );
	SetNextThink( RANDOM_FLOAT( 0.1f, 0.2f ) );
}

void CBoat::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
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

	if( (hDriver == NULL && ( pev->flags & FL_ONGROUND )) || (PushVelocity.Length() > 0) )
	{
		UTIL_MakeVectors( pPlayer->pev->v_angle );
		PushVelocity = gpGlobals->v_forward * 200;
		PushVelocity.z = 35;
		return;
	}

	if( hDriver != NULL )
	{
		if( hDriver != pPlayer )
		{
			UTIL_ShowMessage( "The vehicle is full!", pPlayer );
			return;
		}
	}

	if( pPlayer->pCar == this || useType == USE_OFF ) // player in this car, go out
	{
		if( !ExitCar( pPlayer ) )
			return;

		if( AllowCamera ) // only set view if camera was allowed, otherwise maybe we were using some external camera, don't interrupt it
			SET_VIEW( pPlayer->edict(), pPlayer->edict() );

		// reset player's angles, look in the vehicle direction
		pPlayer->SetAbsAngles( GetAbsAngles() );
		pPlayer->pev->fixangle = TRUE;

		if( pPlayer == hDriver )
		{
			if( m_iszEngineSnd ) // stop engine sounds
			{
				STOP_SOUND( edict(), CHAN_WEAPON, STRING( m_iszEngineSnd ) );
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
			SetThink( &CBoat::Idle );

			// clear tires' sound
			EMIT_SOUND( edict(), CHAN_ITEM, "common/null.wav", 0, 3.0 );

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

			EMIT_SOUND( edict(), CHAN_STATIC, "func_car/eng_start.wav", VOL_NORM, ATTN_NORM );
			if( m_iszIdleSnd )
				STOP_SOUND( edict(), CHAN_WEAPON, STRING( m_iszIdleSnd ) );
			SetThink( &CBoat::Drive );
			UTIL_FireTargets( pev->target, pPlayer, this, USE_TOGGLE, 0 );
			time = gpGlobals->time;
			DriverMdlSequence = -1;
			CameraAngles = GetAbsAngles(); // make sure camera is angled properly when we enter the vehicle
			NewCameraAngle = CameraAngles.y;

			MESSAGE_BEGIN( MSG_ONE, gmsgTempEnt, NULL, hDriver->pev );
				WRITE_BYTE( TE_CARPARAMS );
				WRITE_BYTE( 0 ); // set no gear to show on HUD, boats don't have gears here
			MESSAGE_END();

			SetNextThink( 0 );
		}
	}
	else
		ALERT( at_aiconsole, "CBoat: Player is already in another vehicle!\n" );
}

void CBoat::Setup( void )
{
	pChassis = UTIL_FindEntityByTargetname( NULL, STRING( chassis ) );
	if( pChassis )
	{
		pChassis->SetAbsOrigin( GetAbsOrigin() );
		pChassis->SetParent( this );
	}
	else
	{
		ALERT( at_error, "BOAT WITH NO CHASSIS!\n" );
		UTIL_Remove( this );
		return;
	}
	
	pCarHurt = UTIL_FindEntityByTargetname( NULL, STRING( carhurt ) );
	pDriverMdl = (CBaseAnimating *)UTIL_FindEntityByTargetname( NULL, STRING( drivermdl ) );
	pChassisMdl = (CBaseAnimating *)UTIL_FindEntityByTargetname( NULL, STRING( chassismdl ) );
	pCamera1 = CBaseEntity::Create( "info_target", GetAbsOrigin(), GetAbsAngles(), edict() );
	pCamera2 = UTIL_FindEntityByTargetname( NULL, STRING( camera2 ) );

	if( pChassisMdl )
	{
		pChassisMdl->pev->owner = edict();
		UTIL_SetSize( pChassisMdl->pev, Vector( -16, -16, 0 ), Vector( 16, 16, 16 ) );
		pChassisMdl->pev->takedamage = DAMAGE_YES;
		pChassisMdl->pev->spawnflags |= BIT( 31 ); // SF_ENVMODEL_OWNERDAMAGE
		pChassisMdl->RelinkEntity( FALSE );
	}
	else
		ALERT( at_warning, "func_boat \"%s\" doesn't have body model entity specified, collision won't work properly.\n", GetTargetname() );

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
				CameraHeight = 80;
		}
	}

	if( pCamera2 )
	{
		if( !pCamera2->m_iParent )
			pCamera2->SetParent( this );
		pCamera2->SetNullModel();
		Camera2LocalOrigin = pCamera2->GetLocalOrigin();
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
		pFreeCam->pev->iuser1 = 0;
		if( pChassisMdl )
		{
			// get camera distance according to model bounds
			Vector mins = g_vecZero;
			Vector maxs = g_vecZero;
			UTIL_GetModelBounds( pChassisMdl->pev->modelindex, mins, maxs );
			pFreeCam->pev->iuser1 = (int)((mins - maxs).Length() * 0.75f * pChassisMdl->pev->scale);
		}
		pFreeCam->pev->effects |= EF_SKIPPVS;
		if( !FreeCameraDistance )
			FreeCameraDistance = CameraDistance;
	}

	if( pev->iuser1 ) // rotate boat
	{
		Vector ang = GetAbsAngles();
		ang.y += pev->iuser1;
		SetAbsAngles( ang );
	}

	SetThink( &CBoat::Idle );
	SetNextThink( RANDOM_FLOAT( 0.2f, 0.5f ) );
}

void CBoat::GetCollision( const float AbsCarSpeed, const int Forward, Vector *Collision, float *ColDotProduct, Vector *ColPoint )
{
	TraceResult trCol;

	*Collision = g_vecZero;
	*ColPoint = g_vecZero; // place for sparks
	*ColDotProduct = 1.0f;

	Vector ColPointLeft = g_vecZero;
	Vector ColPointRight = g_vecZero;

	// 4 collision points from corners.
	Vector ColPointWFR = g_vecZero;
	Vector ColPointWFL = g_vecZero;
	Vector ColPointWRR = g_vecZero;
	Vector ColPointWRL = g_vecZero;

	// make chassis vectors
	UTIL_MakeVectors( pChassis->GetAbsAngles() );
	Vector ChassisForw = gpGlobals->v_forward;
	Vector ChassisUp = gpGlobals->v_up;
	Vector ChassisRight = gpGlobals->v_right;

	// bring back the boat vectors
	UTIL_MakeVectors( GetAbsAngles() );

	bool hit_carblocker = false;
	
	int magic = 10;
	if( Forward == 1 )
	{
		// front right corner
		UTIL_TraceLine( GetAbsOrigin() + ChassisUp * magic + ChassisRight * BoatWidth * 0.5, GetAbsOrigin() + ChassisUp * magic + ChassisRight * BoatWidth * 0.5 + ChassisForw * BoatLength * 0.5, ignore_monsters, edict(), &trCol );
		ColPointWFR = trCol.vecEndPos;
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_BOATBLOCKER )
			hit_carblocker = true;
		if( trCol.flFraction < 1.0f )
		{
			*ColDotProduct = -DotProduct( ChassisForw, trCol.vecPlaneNormal ) * Forward;
			*Collision += trCol.vecPlaneNormal * AbsCarSpeed * (*ColDotProduct);
			*ColPoint = ColPointWFR;
			if( *ColDotProduct < 0.6 )
				*ColDotProduct *= -1;
		}
		else if( hit_carblocker )
		{
			*Collision += -gpGlobals->v_forward * AbsCarSpeed * Forward;
			*ColPoint = ColPointWFR;
		}
		hit_carblocker = false;

		// front left corner
		UTIL_TraceLine( GetAbsOrigin() + ChassisUp * magic - ChassisRight * BoatWidth * 0.5, GetAbsOrigin() + ChassisUp * magic - ChassisRight * BoatWidth * 0.5 + ChassisForw * BoatLength * 0.5, ignore_monsters, edict(), &trCol );
		ColPointWFL = trCol.vecEndPos;
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_BOATBLOCKER )
			hit_carblocker = true;
		if( trCol.flFraction < 1.0f )
		{
			*ColDotProduct = -DotProduct( ChassisForw, trCol.vecPlaneNormal ) * Forward;
			*Collision += trCol.vecPlaneNormal * AbsCarSpeed * (*ColDotProduct);
			*ColPoint = ColPointWFL;
			if( *ColDotProduct < 0.6 )
				*ColDotProduct *= -1;
		}
		else if( hit_carblocker )
		{
			*Collision += -gpGlobals->v_forward * AbsCarSpeed * Forward;
			*ColPoint = ColPointWFL;
		}
		hit_carblocker = false;
	}

	if( Forward == -1 )
	{
		// rear right corner
		UTIL_TraceLine( GetAbsOrigin() + ChassisUp * magic + ChassisRight * BoatWidth * 0.5, GetAbsOrigin() + ChassisUp * magic + ChassisRight * BoatWidth * 0.5 - ChassisForw * BoatLength * 0.5, ignore_monsters, edict(), &trCol );
		ColPointWRR = trCol.vecEndPos;
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_BOATBLOCKER )
			hit_carblocker = true;
		if( trCol.flFraction < 1.0f )
		{
			*ColDotProduct = -DotProduct( ChassisForw, trCol.vecPlaneNormal ) * Forward;
			*Collision += trCol.vecPlaneNormal * AbsCarSpeed * (*ColDotProduct);
			*ColPoint = ColPointWRR;
		}
		else if( hit_carblocker )
		{
			*Collision += gpGlobals->v_forward * AbsCarSpeed * Forward;
			*ColPoint = ColPointWRR;
		}
		hit_carblocker = false;

		// rear left corner
		UTIL_TraceLine( GetAbsOrigin() + ChassisUp * magic - ChassisRight * BoatWidth * 0.5, GetAbsOrigin() + ChassisUp * magic - ChassisRight * BoatWidth * 0.5 - ChassisForw * BoatLength * 0.5, ignore_monsters, edict(), &trCol );
		ColPointWRL = trCol.vecEndPos;
		if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_BOATBLOCKER )
			hit_carblocker = true;
		if( trCol.flFraction < 1.0f )
		{
			*ColDotProduct = -DotProduct( ChassisForw, trCol.vecPlaneNormal ) * Forward;
			*Collision += trCol.vecPlaneNormal * AbsCarSpeed * (*ColDotProduct);
			*ColPoint = ColPointWRL;
		}
		else if( hit_carblocker )
		{
			*Collision += gpGlobals->v_forward * AbsCarSpeed * Forward;
			*ColPoint = ColPointWRL;
		}
		hit_carblocker = false;
	}

	// sides!
	// left
	UTIL_TraceLine( GetAbsOrigin() + gpGlobals->v_up * magic, GetAbsOrigin() + gpGlobals->v_up * magic - gpGlobals->v_right * BoatWidth * 0.5, ignore_monsters, edict(), &trCol );
	if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_BOATBLOCKER )
		hit_carblocker = true;
	if( trCol.flFraction < 1.0f || hit_carblocker )
		ColPointLeft = gpGlobals->v_right * 100;
	hit_carblocker = false;
	// right
	UTIL_TraceLine( GetAbsOrigin() + gpGlobals->v_up * magic, GetAbsOrigin() + gpGlobals->v_up * magic + gpGlobals->v_right * BoatWidth * 0.5, ignore_monsters, edict(), &trCol );
	if( UTIL_PointContents( trCol.vecEndPos ) == CONTENT_BOATBLOCKER )
		hit_carblocker = true;
	if( trCol.flFraction < 1.0f || hit_carblocker )
		ColPointRight = -gpGlobals->v_right * 100;

	Vector SideCollision = ColPointLeft + ColPointRight;

	*Collision += SideCollision;
}

void CBoat::Drive( void )
{
	if( hDriver == NULL )
	{
		SetThink( &CBoat::Idle );
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

	// now make the car's vectors
	UTIL_MakeVectors( GetAbsAngles() );

	// TEMP !!! (or not.)
	Vector PlayerPos;
	if( pDriverMdl )
	{
		PlayerPos = pDriverMdl->GetAbsOrigin();
		// don't touch the water, it can be dangerous
		PlayerPos.z += 40 - fabs(pDriverMdl->GetAbsOrigin().z - GetAbsOrigin().z);
	}
	else
		PlayerPos = GetAbsOrigin() + gpGlobals->v_up * 40;
	hDriver->SetAbsOrigin( PlayerPos );
	hDriver->SetAbsVelocity( g_vecZero );

	float AbsCarSpeed = fabs( CarSpeed );
	
	// motion blur
	hDriver->pev->vuser1.y = AbsCarSpeed;

	// achievement
	float Distance = (GetAbsOrigin() - PrevOrigin).Length();
	Distance *= 0.01905f; // convert units to meters
	if( hDriver->pev->flags & FL_CLIENT )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( hDriver->entindex() );
		plr->AchievementStats[ACH_WATERJETDISTANCE] += Distance;
	}
	PrevOrigin = GetAbsOrigin();

	int chktop = 0;
	int chkbtm = 0;
	if( UTIL_PointContents( GetAbsOrigin() ) == CONTENTS_WATER )
		chktop = 1;
//	if( UTIL_PointContents( GetAbsOrigin() - Vector( 0, 0, BoatDepth ) ) == CONTENTS_WATER )
	if( UTIL_PointContents( GetAbsOrigin() - Vector( 0, 0, 1 ) ) == CONTENTS_WATER )
		chkbtm = 1;

	bool InAir = (chktop + chkbtm == 0); // !!! this includes being on the ground!
	bool UnderWater = (chktop == 1) || (chktop + chkbtm == 2);
	bool Afloat = (chkbtm == 1 && chktop == 0);

	//----------------------------
	// turn
	//----------------------------
	// Forward turning controls if going backwards
	int Forward = 1;
	if( ((bBack()) && (CarSpeed < 0.0f)) || (CarSpeed < 0.0f) )
		Forward = -1;
	else if( CarSpeed == 0.0f )
		Forward = 0;

	float TurnRate = TurningRate / (1 + AbsCarSpeed);
	TurnRate = Forward * bound( 0, TurnRate, 20 );
	if( CarSpeed == 0.0f )
		TurnRate = 1;
	float OpposeTurn = 1.0f;
	int CameraSwayRate = 0;
	if( SecondaryCamera )
		CameraSwayRate = MaxCamera2Sway;

	if( !InAir ) // boat can always turn
	{
		if( bRight() )
		{
			if( Turning > 0 ) OpposeTurn = (3 * TurningRate) / (1 + AbsCarSpeed);
			OpposeTurn = bound( 1, OpposeTurn, 3 );
			Turning -= OpposeTurn * TurnRate * gpGlobals->frametime;
			if( CameraSwayRate > 0 )
				CameraMoving -= CameraSwayRate * gpGlobals->frametime;
		}
		else if( bLeft() )
		{
			if( Turning < 0 ) OpposeTurn = (3 * TurningRate) / (1 + AbsCarSpeed);
			OpposeTurn = bound( 1, OpposeTurn, 3 );
			Turning += OpposeTurn * TurnRate * gpGlobals->frametime;
			if( CameraSwayRate > 0 )
				CameraMoving += CameraSwayRate * gpGlobals->frametime;
		}
	}

	// no turning buttons pressed, go to zero slowly
	if( !(bLeft()) && !(bRight()) )
	{
		Turning = UTIL_Approach( 0, Turning, fabs( TurnRate * 0.75 ) * gpGlobals->frametime );
		if( CameraSwayRate > 0 )
			CameraMoving = UTIL_Approach( 0, CameraMoving, CameraSwayRate * 0.5 * gpGlobals->frametime );

		if( fabs( Turning ) <= 0.001f )
			Turning = 0;
	}

	if( InAir )
	{
		Turning = UTIL_Approach( 0, Turning, gpGlobals->frametime );
	}

	float max_turn_val = 1 / (1 + AbsCarSpeed * 0.0025); // magic
	Turning = bound( -max_turn_val, Turning, max_turn_val );

	int CameraMovingBound = 0;
	if( SecondaryCamera )
		CameraMovingBound = MaxCamera2Sway;

	CameraMoving = bound( -CameraMovingBound, CameraMoving, CameraMovingBound );

	float BoatTurnRate = bound( 0.05, AbsCarSpeed * 0.00075, 1 );

	Vector BoatAng = GetLocalAngles();

	//	Y - turn left and right
	float trn = (10 * MaxTurn) * (Turning * BoatTurnRate) * gpGlobals->frametime;
	BoatAng.y += trn;

	//----------------------------
	// engine sound
	//----------------------------
	EngPitch = 90 + 165 * (AbsCarSpeed / MaxCarSpeed);
	EngPitch = bound( 90, EngPitch, 250 );
	if( BrokenCar )
		EngPitch *= 0.65;
	if( m_iszEngineSnd )
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, STRING( m_iszEngineSnd ), 1, ATTN_NORM, SND_CHANGE_PITCH, (int)EngPitch );

	//----------------------------
	// texture sound (tires) and chassis shaking (from dirt etc.)
	//----------------------------
	float tiresvolume = AbsCarSpeed * 0.001;
	int tirespitch = 60 + (AbsCarSpeed * 0.03);

	if( tirespitch > 250 )
		tirespitch = 250;
	if( tiresvolume > 1.0f )
		tiresvolume = 1.0f;
	if( (tiresvolume > 0.01f) && ((pev->flags & FL_ONGROUND) || !InAir) )
	{
		if( pev->flags & FL_ONGROUND )
			EMIT_SOUND_DYN( edict(), CHAN_ITEM, "func_car/metal_drag.wav", tiresvolume, ATTN_NORM, SND_CHANGE_VOL | SND_CHANGE_PITCH, tirespitch );
		else if( !InAir )
			EMIT_SOUND_DYN( edict(), CHAN_ITEM, "func_car/tires/water.wav", tiresvolume, ATTN_NORM, SND_CHANGE_VOL | SND_CHANGE_PITCH, tirespitch );
	}
	else
	{
		// stop sound...
		STOP_SOUND( edict(), CHAN_ITEM, "func_car/metal_drag.wav" ); 
		STOP_SOUND( edict(), CHAN_ITEM, "func_car/tires/water.wav" );
	}

	//----------------------------
	// collision
	//----------------------------
	
	Vector Collision;
	float ColDotProduct;
	Vector ColPoint;
	GetCollision( AbsCarSpeed, Forward, &Collision, &ColDotProduct, &ColPoint );

	//----------------------------
	// chassis inclination
	//----------------------------
	Vector ChassisAng = pChassis->GetLocalAngles();

	// X - transversal rotation
	// lean boat when accelerating
	if( CarSpeed > 0 && (bForward()) )
	{
		AccelAddX -= AccelRate * 0.01 * gpGlobals->frametime;
		AccelAddX = bound( -MaxLean * 0.5, AccelAddX, MaxLean * 0.5 );
	}
	else if( CarSpeed < 0 && !(bForward()) && (bBack()) )
	{
		AccelAddX += BackAccelRate * 0.02 * gpGlobals->frametime;
		AccelAddX = bound( -MaxLean * 0.1, AccelAddX, MaxLean * 0.1 );
	}
	else
		AccelAddX = UTIL_Approach( 0, AccelAddX, AccelRate * ( 1 / (10 + AbsCarSpeed * 0.1) ) * gpGlobals->frametime );

	TraceResult TrCross;
	float FrontPoint = pChassis->GetAbsOrigin().z;
	float RearPoint = FrontPoint;
	Vector FrontCheck = pChassis->GetAbsOrigin() + ChassisForw * ((BoatLength * 0.5) + 1);
	Vector RearCheck = pChassis->GetAbsOrigin() - ChassisForw * ((BoatLength * 0.5) + 1);
	UTIL_TraceLine( FrontCheck + ChassisUp * 50, FrontCheck - ChassisUp * (BoatDepth * 4), ignore_monsters, dont_ignore_glass, edict(), &TrCross );
	if( TrCross.flFraction < 1.0f && !Afloat )
		FrontPoint = TrCross.vecEndPos.z;
	UTIL_TraceLine( RearCheck + ChassisUp * 50, RearCheck - ChassisUp * (BoatDepth * 4), ignore_monsters, dont_ignore_glass, edict(), &TrCross );
	if( TrCross.flFraction < 1.0f && !Afloat )
		RearPoint = TrCross.vecEndPos.z;
	const float CrossPoint = (RearPoint - FrontPoint) * 0.5f;
	if( !Collision.IsNull() || CrossPoint != 0.0f || !Afloat )
	{
		AccelAddX = 0;
		BrakeAddX = 0;
	}

	float NewChassisAngX = BrakeAddX + AccelAddX + CrossPoint;
	if( !Collision.IsNull() )
		NewChassisAngX = 0;
	ChassisAng.x = UTIL_Approach( NewChassisAngX, ChassisAng.x, 100 * gpGlobals->frametime );

	// Z - lateral rotation
	float NewChassisAngZ = CarSpeed * -Turning * (MaxLean * 0.001);
	ChassisAng.z = NewChassisAngZ;

	if( InAir )
	{
		ChassisAng.x = UTIL_Approach( 0, ChassisAng.x, 50 * gpGlobals->frametime );
		ChassisAng.z = UTIL_Approach( 0, ChassisAng.z, 50 * gpGlobals->frametime );
	}

	ChassisAng.x = bound( -MaxLean, ChassisAng.x, MaxLean );
	ChassisAng.z = bound( -MaxLean, ChassisAng.z, MaxLean );
	pChassis->SetLocalAngles( ChassisAng );

	Vector ChOrg = pChassis->GetLocalOrigin();
	if( InAir )
	{
		AddZ = UTIL_Approach( BoatDepth, AddZ, 50 * gpGlobals->frametime );
		ChOrg.z = AddZ;
	}
	else
	{
		AddZ = UTIL_Approach( 0, AddZ, 20 * gpGlobals->frametime );
		float sinamp = 0.01 * bound( 150, AbsCarSpeed, 300 );
		ChOrg.z = AddZ + sin( gpGlobals->time * 2 ) * sinamp;
	}

	pChassis->SetLocalOrigin( ChOrg );

	//----------------------------
	// accelerate/brake
	//----------------------------
	float ActualAccelRate = AccelRate / (1 + (AbsCarSpeed / MaxCarSpeed));
	float ActualBackAccelRate = BackAccelRate / (1 + (AbsCarSpeed / MaxCarSpeed));

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
	else if( (!(bForward()) && !(bBack())) || BrokenCar || IsLockedByMaster() || ( pev->flags & FL_ONGROUND ) )
	{
		// no accelerate/brake buttons pressed or all wheels in air
		// boat slows down (aka friction...)

		int AddGroundFriction = 1;
		if( pev->flags & FL_ONGROUND )
			AddGroundFriction = 2;
		if( CarSpeed >= 20.0f )
			CarSpeed -= AddGroundFriction * 150 * gpGlobals->frametime;
		else if( CarSpeed <= -20.0f )
			CarSpeed += AddGroundFriction * 150 * gpGlobals->frametime;

		if( CarSpeed > -20 && CarSpeed < 20 )
			CarSpeed = 0.0f;
	}
	else if( !InAir && (bForward() || bBack()) )
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
				CarSpeed -= ActualBackAccelRate * (1 - fabs( Turning ) * 0.5) * gpGlobals->frametime;
		}
	}

	CarSpeed = bound( -MaxCarSpeedBackwards + (MaxCarSpeedBackwards * fabs( Turning ) * 0.25), CarSpeed, MaxCarSpeed - (MaxCarSpeed * fabs( Turning ) * 0.25) );

	// apply main movement
	if( !Collision.IsNull() )
		pev->velocity = Collision;
	else
		pev->velocity += gpGlobals->v_forward * CarSpeed;

	//----------------------------
	// weight...?
	//----------------------------
	if( InAir )
	{
		if( !CarInAir )
		{
			CarInAir = true;
			DownForce = 100;
		}
	}

	if( !InAir && CarInAir ) // lol I know :)
	{
		CarInAir = false;
		if( landing_effect_time < gpGlobals->time )
		{
		//	UTIL_ScreenShakeLocal( hDriver, GetAbsOrigin(), 4, 100, 1, 300, true );
			switch( RANDOM_LONG( 0, 2 ) )
			{
			case 0: EMIT_SOUND_DYN( edict(), CHAN_STATIC, "func_car/splash1.wav", VOL_NORM, ATTN_NORM, 0, RANDOM_LONG( 85, 110 ) ); break;
			case 1: EMIT_SOUND_DYN( edict(), CHAN_STATIC, "func_car/splash2.wav", VOL_NORM, ATTN_NORM, 0, RANDOM_LONG( 85, 110 ) ); break;
			case 2: EMIT_SOUND_DYN( edict(), CHAN_STATIC, "func_car/splash3.wav", VOL_NORM, ATTN_NORM, 0, RANDOM_LONG( 85, 110 ) ); break;
			}

			CarSpeed *= 0.65;
			landing_effect_time = gpGlobals->time + 1;
			MakeWaterSplash( GetAbsOrigin() + Vector( 0, 0, 64 ), GetAbsOrigin() - Vector( 0, 0, 64 ), 3 );
		}
	}

	if( InAir )
	{
		DownForce -= 200 * gpGlobals->frametime;
		if( DownForce < 0 )
			DownForce *= 1 + (2 * gpGlobals->frametime);
		else
			DownForce -= 400 * gpGlobals->frametime;
	}

	if( UnderWater )
	{
		if( DownForce < -200 )
			DownForce = -200;

		DownForce += 100 * gpGlobals->frametime;
		if( DownForce > 0 )
			DownForce *= 1 + (2 * gpGlobals->frametime);
		else
			DownForce += 800 * gpGlobals->frametime;
	}
	else
	{
		if( pev->flags & FL_ONGROUND )
			DownForce = 0;
	}


	if( Afloat )
	{
		DownForce = 0;
	}

	DownForce = bound( -2000, DownForce, 250 );
	pev->velocity.z = DownForce;

	//----------------------------
	// timed effects
	//----------------------------

	bool TookDamage = false;
	float DriverDamage = 0.0f;

	// do timing stuff each 0.5 seconds
	if( !Collision.IsNull() && (gpGlobals->time > time) )
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

		time = gpGlobals->time + 0.5;
	}

	Camera();

	//----------------------------
	// driver model
	//----------------------------
	if( pDriverMdl )
	{
		int f = Forward;
		if( f == 0 ) f = 1;  // ¯\_(ツ)_/¯
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

	SetLocalAngles( BoatAng ); // car direction is now set

	SafeSpawnPosition();

	if( TookDamage && DriverDamage > 7.5f )
	{
		TakeDamage( pev, pev, DriverDamage, DMG_CRUSH );
		UTIL_ScreenShakeLocal( hDriver, GetAbsOrigin(), AbsCarSpeed * 0.01, 100, 1, 300, true );
		if( DriverDamage > 15.0f ) // fall off scooter :)
			Use( this, this, USE_OFF, 0 );
	}
	// !!! hDriver can become NULL after this, by taking damage and dying, game will crash.

	if( hDriver == NULL )
		return;

	//----------------------------
	// water trail sprite
	//----------------------------
	if( (gpGlobals->time > LastTrailTime + 0.1) && Afloat && (AbsCarSpeed > 15) )
	{
		Vector TrailOrg = GetAbsOrigin();
		float TrailAngle = GetAbsAngles().y;
		UTIL_MakeVectors( GetAbsAngles() );
		if( Forward == 1 )
		{
			TrailAngle += 180;
			TrailOrg -= gpGlobals->v_forward * BoatLength * 0.35;
		}
		else if( Forward == -1 )
			TrailOrg += gpGlobals->v_forward * BoatLength * 0.35;

		int Scale = bound( 0, AbsCarSpeed * 0.25, 150);
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

//========================================================================
// SafeSpawnPosition: collect a vector at the back of the car
// where you can exit for sure, if blocked
//========================================================================
Vector CBoat::SafeSpawnPosition( void )
{
	if( gpGlobals->time < LastSafeSpawnCollectTime + 5 )
		return SafeSpawnPos;

	LastSafeSpawnCollectTime = gpGlobals->time;

	UTIL_MakeVectors( pChassis->GetAbsAngles() );
	Vector CarCenter = GetAbsOrigin() + gpGlobals->v_up * 46; // (human hull height / 2) + 10
	Vector vecSrc, vecEnd;
	TraceResult TrExit, VisCheck;

	vecSrc = CarCenter - gpGlobals->v_forward * (BoatLength + 50);
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

void CBoat::Idle( void )
{
	// reset all velocity from previous frame
	pev->velocity = g_vecZero;

	// will need those later!
	UTIL_MakeVectors( pChassis->GetAbsAngles() );
	Vector ChassisForw = gpGlobals->v_forward;
	Vector ChassisUp = gpGlobals->v_up;
	Vector ChassisRight = gpGlobals->v_right;

	// now make the car's vectors
	UTIL_MakeVectors( GetAbsAngles() );

	float AbsCarSpeed = fabs( CarSpeed );

	int chktop = 0;
	int chkbtm = 0;
	if( UTIL_PointContents( GetAbsOrigin() ) == CONTENTS_WATER )
		chktop = 1;
//	if( UTIL_PointContents( GetAbsOrigin() - Vector(0,0,BoatDepth) ) == CONTENTS_WATER )
	if( UTIL_PointContents( GetAbsOrigin() - Vector( 0, 0, 1 ) ) == CONTENTS_WATER )
		chkbtm = 1;

	bool InAir = (chktop + chkbtm == 0); // !!! this includes being on the ground!
	bool UnderWater = (chktop == 1) || (chktop + chkbtm == 2);
	bool Afloat = (chkbtm == 1 && chktop == 0);

	//----------------------------
	// turn
	//----------------------------
	// Forward turning controls if going backwards
	int Forward = 1;
	if( CarSpeed < 0.0f )
		Forward = -1;
	else if( CarSpeed == 0.0f )
		Forward = 0;

	Turning = UTIL_Approach( 0, Turning, gpGlobals->frametime );

	float max_turn_val = 1 / (1 + AbsCarSpeed * 0.0025); // magic
	Turning = bound( -max_turn_val, Turning, max_turn_val );

	float BoatTurnRate = bound( 0.05, AbsCarSpeed * 0.00075, 1 );

	Vector BoatAng = GetLocalAngles();

	//	Y - turn left and right
	float trn = (10 * MaxTurn) * (Turning * BoatTurnRate) * gpGlobals->frametime;
	BoatAng.y += trn;

	//----------------------------
	// texture sound (tires) and chassis shaking (from dirt etc.)
	//----------------------------
	float tiresvolume = AbsCarSpeed * 0.001;
	int tirespitch = 60 + (AbsCarSpeed * 0.03);

	if( tirespitch > 250 )
		tirespitch = 250;
	if( tiresvolume > 1.0f )
		tiresvolume = 1.0f;
	if( (tiresvolume > 0.01f) && ((pev->flags & FL_ONGROUND) || !InAir) )
	{
		if( pev->flags & FL_ONGROUND )
			EMIT_SOUND_DYN( edict(), CHAN_ITEM, "func_car/metal_drag.wav", tiresvolume, ATTN_NORM, SND_CHANGE_VOL | SND_CHANGE_PITCH, tirespitch );
		else if( !InAir )
			EMIT_SOUND_DYN( edict(), CHAN_ITEM, "func_car/tires/water.wav", tiresvolume, ATTN_NORM, SND_CHANGE_VOL | SND_CHANGE_PITCH, tirespitch );
	}
	else
	{
		// stop sound...
		STOP_SOUND( edict(), CHAN_ITEM, "func_car/metal_drag.wav" );
		STOP_SOUND( edict(), CHAN_ITEM, "func_car/tires/water.wav" );
	}

	//----------------------------
	// collision
	//----------------------------

	Vector Collision;
	float ColDotProduct;
	Vector ColPoint;
	GetCollision( AbsCarSpeed, Forward, &Collision, &ColDotProduct, &ColPoint );

	//----------------------------
	// chassis inclination
	//----------------------------
	Vector ChassisAng = pChassis->GetLocalAngles();

	// X - transversal rotation
	AccelAddX = 0;
	BrakeAddX = 0;

	TraceResult TrCross;
	float FrontPoint = pChassis->GetAbsOrigin().z;
	float RearPoint = FrontPoint;
	Vector FrontCheck = pChassis->GetAbsOrigin() + ChassisForw * ((BoatLength * 0.5) + 1);
	Vector RearCheck = pChassis->GetAbsOrigin() - ChassisForw * ((BoatLength * 0.5) + 1);
	UTIL_TraceLine( FrontCheck + ChassisUp * 50, FrontCheck - ChassisUp * (BoatDepth * 4), ignore_monsters, dont_ignore_glass, edict(), &TrCross );
	if( TrCross.flFraction < 1.0f && !Afloat )
		FrontPoint = TrCross.vecEndPos.z;
	UTIL_TraceLine( RearCheck + ChassisUp * 50, RearCheck - ChassisUp * (BoatDepth * 4), ignore_monsters, dont_ignore_glass, edict(), &TrCross );
	if( TrCross.flFraction < 1.0f && !Afloat )
		RearPoint = TrCross.vecEndPos.z;
	const float CrossPoint = (RearPoint - FrontPoint) * 0.5f;

	float NewChassisAngX = CrossPoint;
	ChassisAng.x = UTIL_Approach( NewChassisAngX, ChassisAng.x, 100 * gpGlobals->frametime );

	// Z - lateral rotation
	float NewChassisAngZ = CarSpeed * -Turning * (MaxLean * 0.001);
	ChassisAng.z = NewChassisAngZ;

	if( InAir )
	{
		ChassisAng.x = UTIL_Approach( 0, ChassisAng.x, 50 * gpGlobals->frametime );
		ChassisAng.z = UTIL_Approach( 0, ChassisAng.z, 50 * gpGlobals->frametime );
	}

	ChassisAng.x = bound( -MaxLean, ChassisAng.x, MaxLean );
	ChassisAng.z = bound( -MaxLean, ChassisAng.z, MaxLean );
	pChassis->SetLocalAngles( ChassisAng );

	Vector ChOrg = pChassis->GetLocalOrigin();
	if( InAir )
	{
		AddZ = BoatDepth;
		ChOrg.z = AddZ;
	}
	else
	{
		AddZ = 0;
		ChOrg.z = AddZ + sin( gpGlobals->time ) * 1.25;
	}
	pChassis->SetLocalOrigin( ChOrg );

	//----------------------------
	// accelerate/brake
	//----------------------------
	float ActualAccelRate = AccelRate / (1 + (AbsCarSpeed / MaxCarSpeed));
	float ActualBackAccelRate = BackAccelRate / (1 + (AbsCarSpeed / MaxCarSpeed));

	// no accelerate/brake buttons pressed or all wheels in air
	// boat slows down (aka friction...)

	int AddGroundFriction = 1;
	if( pev->flags & FL_ONGROUND )
		AddGroundFriction = 3;
	if( CarSpeed >= 20.0f )
		CarSpeed -= AddGroundFriction * 150 * gpGlobals->frametime;
	else if( CarSpeed <= -20.0f )
		CarSpeed += AddGroundFriction * 150 * gpGlobals->frametime;

	if( CarSpeed > -20 && CarSpeed < 20 )
		CarSpeed = 0.0f;

	CarSpeed = bound( -MaxCarSpeedBackwards + (MaxCarSpeedBackwards * fabs( Turning ) * 0.25), CarSpeed, MaxCarSpeed - (MaxCarSpeed * fabs( Turning ) * 0.25) );

	// apply main movement
	pev->velocity += gpGlobals->v_forward * CarSpeed;

	//----------------------------
	// weight...?
	//----------------------------
	if( InAir )
	{
		if( !CarInAir )
		{
			CarInAir = true;
			DownForce = 100;
		}
	}

	if( !InAir && CarInAir ) // lol I know :)
	{
		CarInAir = false;
		switch( RANDOM_LONG( 0, 2 ) )
		{
		case 0: EMIT_SOUND_DYN( edict(), CHAN_STATIC, "func_car/splash1.wav", VOL_NORM, ATTN_NORM, 0, RANDOM_LONG( 85, 110 ) ); break;
		case 1: EMIT_SOUND_DYN( edict(), CHAN_STATIC, "func_car/splash2.wav", VOL_NORM, ATTN_NORM, 0, RANDOM_LONG( 85, 110 ) ); break;
		case 2: EMIT_SOUND_DYN( edict(), CHAN_STATIC, "func_car/splash3.wav", VOL_NORM, ATTN_NORM, 0, RANDOM_LONG( 85, 110 ) ); break;
		}

		CarSpeed *= 0.75;
	}

	if( InAir )
	{
		DownForce -= 200 * gpGlobals->frametime;
		if( DownForce < 0 )
			DownForce *= 1 + (2 * gpGlobals->frametime);
		else
			DownForce -= 400 * gpGlobals->frametime;
	}

	if( UnderWater )
	{
		if( DownForce < -200 )
			DownForce = -200;

		DownForce += 100 * gpGlobals->frametime;
		if( DownForce > 0 )
			DownForce *= 1 + (2 * gpGlobals->frametime);
		else
			DownForce += 800 * gpGlobals->frametime;
	}
	else
	{
		if( pev->flags & FL_ONGROUND )
			DownForce = 0;
	}

	if( Afloat )
	{
		DownForce = 0;
	}

	DownForce = bound( -2000, DownForce, 250 );
	pev->velocity.z = DownForce;

	//----------------------------
	// timed effects
	//----------------------------

	bool TookDamage = false;
	float DriverDamage = 0.0f;
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

		if( AbsCarSpeed > 500 )
		{
			TookDamage = true;
			DriverDamage = AbsCarSpeed * 0.05 * ColDotProduct;
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

		pChassisMdl->SetBlending( 0, Turning );
	}

	SetLocalAngles( BoatAng ); // car direction is now set

	if( Collided )
		CarSpeed *= -0.15 / ColDotProduct;

	// push the boat
	if( !PushVelocity.IsNull() )
	{
		pev->velocity += PushVelocity;
		PushVelocity.x = UTIL_Approach( 0, PushVelocity.x, 100 * gpGlobals->frametime );
		PushVelocity.y = UTIL_Approach( 0, PushVelocity.y, 100 * gpGlobals->frametime );
		PushVelocity.z = UTIL_Approach( 0, PushVelocity.z, 100 * gpGlobals->frametime );
		if( PushVelocity.Length() <= 0.1 )
			PushVelocity = g_vecZero;
	}

	//----------------------------
	// idle sound
	//----------------------------
	if( m_iszIdleSnd )
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, STRING( m_iszIdleSnd ), 1, 2.2, SND_CHANGE_PITCH, PITCH_NORM );

	float thinktime = 0.0f;
	if( !g_pGameRules->IsMultiplayer() && (CarSpeed == 0.0f) && (pev->velocity == g_vecZero) )
	{
		// don't think too often if player is far from the car, and it is idle
		CBaseEntity *pPlayer = CBaseEntity::Instance( INDEXENT( 1 ) );
		if( (GetAbsOrigin() - pPlayer->GetAbsOrigin()).Length() > 2000 )
			thinktime = 0.5;
	}

	SetNextThink( thinktime );
}

void CBoat::OnRemove( void )
{
	// if car is somehow deleted during drive, we need to remove the driver to prevent game crash
	if( hDriver != NULL )
		Use( hDriver, hDriver, USE_OFF, 0 );
}

void CBoat::ClearEffects( void )
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
}

bool CBoat::ExitCar( CBaseEntity *pPlayer )
{
	// set player position
	TraceResult TrExit;
	TraceResult VisCheck;
	UTIL_MakeVectors( pChassis->GetAbsAngles() );
	Vector BoatCenter = GetAbsOrigin() + gpGlobals->v_up * 38; // (human hull height / 2) + 2
	Vector vecSrc, vecEnd;

	// check left side of the car first
	vecSrc = BoatCenter - gpGlobals->v_right * (BoatWidth + 18);
	vecEnd = vecSrc;
	UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, human_hull, edict(), &TrExit );
	UTIL_TraceLine( BoatCenter, vecSrc, dont_ignore_monsters, dont_ignore_glass, edict(), &VisCheck );
	if( (TrExit.flFraction == 1.0f) && !TrExit.fAllSolid && (VisCheck.flFraction == 1.0f) )
	{
		pPlayer->SetAbsOrigin( vecSrc );
		return true;
	}

	// can't spawn there, now check right side
	vecSrc = BoatCenter + gpGlobals->v_right * (BoatWidth + 18);
	vecEnd = vecSrc;
	UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, human_hull, edict(), &TrExit );
	UTIL_TraceLine( BoatCenter, vecSrc, dont_ignore_monsters, dont_ignore_glass, edict(), &VisCheck );
	if( (TrExit.flFraction == 1.0f) && !TrExit.fAllSolid && (VisCheck.flFraction == 1.0f) )
	{
		pPlayer->SetAbsOrigin( vecSrc );
		return true;
	}

	// can't spawn both, try front
	vecSrc = BoatCenter + gpGlobals->v_forward * (BoatLength + 20);
	vecEnd = vecSrc;
	UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, human_hull, edict(), &TrExit );
	UTIL_TraceLine( BoatCenter, vecSrc, dont_ignore_monsters, dont_ignore_glass, edict(), &VisCheck );
	if( (TrExit.flFraction == 1.0f) && !TrExit.fAllSolid && (VisCheck.flFraction == 1.0f) )
	{
		pPlayer->SetAbsOrigin( vecSrc );
		return true;
	}

	// can't spawn, try back
	vecSrc = BoatCenter - gpGlobals->v_forward * (BoatLength + 20);
	vecEnd = vecSrc;
	UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, human_hull, edict(), &TrExit );
	UTIL_TraceLine( BoatCenter, vecSrc, dont_ignore_monsters, dont_ignore_glass, edict(), &VisCheck );
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