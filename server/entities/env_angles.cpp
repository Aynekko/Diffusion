#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "triggers.h"

//=======================================================================================================================
// diffusion - change entity's angles
// mode: set new angles, add angles
//=======================================================================================================================

class CEnvAngles : public CBaseDelay
{
	DECLARE_CLASS( CEnvAngles, CBaseDelay );
public:
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void KeyValue( KeyValueData *pkvd );
	void ChangeAngles( void ); // think function

	string_t Entity;
	int X, Y, Z;
	int Mode;
	Vector TargetAngles; // only used in ChangeAngles

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( env_angles, CEnvAngles );

BEGIN_DATADESC( CEnvAngles )
	DEFINE_KEYFIELD( X, FIELD_INTEGER, "setx" ),
	DEFINE_KEYFIELD( Y, FIELD_INTEGER, "sety" ),
	DEFINE_KEYFIELD( Z, FIELD_INTEGER, "setz" ),
	DEFINE_KEYFIELD( Mode, FIELD_INTEGER, "mode" ),
	DEFINE_KEYFIELD( Entity, FIELD_STRING, "entityname" ),
	DEFINE_FIELD( TargetAngles, FIELD_VECTOR ),
	DEFINE_FUNCTION( ChangeAngles ),
END_DATADESC();

void CEnvAngles::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "setx" ) )
	{
		X = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "sety" ) )
	{
		Y = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "setz" ) )
	{
		Z = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "mode" ) )
	{
		Mode = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "entityname" ) )
	{
		Entity = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue( pkvd );
}

void CEnvAngles::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( !Entity )
	{
		ALERT( at_error, "env_angles \"%s\" doesn't have an entity specified!\n", STRING( pev->targetname ) );
		return;
	}
	
	if( IsLockedByMaster( pActivator ) )
		return;

	/*
	Modes:
	0: set new angles
	1: add vector
	2: set new angles smoothly
	3: add new vector smoothly
	*/

	if( !Mode ) Mode = 0; // default mode: set new angles.
	if( !X ) X = 0;
	if( !Y ) Y = 0;
	if( !Z ) Z = 0;
	if( !pev->speed ) pev->speed = 100;

	CBaseEntity *pEntity = NULL;

	pEntity = UTIL_FindEntityByTargetname( pEntity, STRING( Entity ) );

	if( !pEntity )
	{
		ALERT( at_error, "env_angles \"%s\" can't find specified entity!\n", STRING( pev->targetname ) );
		return;
	}

	switch( Mode)
	{
	// set new angles:
	case 0: pEntity->SetAbsAngles( Vector( X, Y, Z ) ); break;
	// add a vector:
	case 1: pEntity->SetAbsAngles( pEntity->GetAbsAngles() + Vector( X, Y, Z ) ); break;
	// set new angles, smoothly
	case 2:
		TargetAngles = Vector( X, Y, Z );
		SetThink( &CEnvAngles::ChangeAngles );
		SetNextThink( 0 );
	break;
	case 3:
		Vector eAng = pEntity->GetAbsAngles();
		TargetAngles = Vector( (int)eAng.x, (int)eAng.y, (int)eAng.z ) + Vector( X, Y, Z );
		SetThink( &CEnvAngles::ChangeAngles );
		SetNextThink( 0 );
	break;
	}
}

void CEnvAngles::ChangeAngles( void )
{
	// we have to perform a check again...
	CBaseEntity *pEntity = NULL;

	float SpeedActual = pev->speed * gpGlobals->frametime;

	if( (pEntity = UTIL_FindEntityByTargetname( pEntity, STRING( Entity ) )) != NULL)
	{
		Vector eAng = pEntity->GetAbsAngles();

		if( Vector( (int)eAng.x, (int)eAng.y, (int)eAng.z) == TargetAngles )
		{
			ALERT( at_aiconsole, "env_angles \"%s\" successfully set new angles with speed %f.\n", STRING( pev->targetname ), pev->speed );
			pEntity->SetAbsAngles( TargetAngles ); // make sure!
			SetThink( NULL );
			return;
		}

		if( eAng.x != TargetAngles.x )
		{
			if( fabs( eAng.x - TargetAngles.x ) <= SpeedActual ) // have to do this, otherwise issues at low fps
				eAng.x = TargetAngles.x;
			else
			{
				if( eAng.x > TargetAngles.x ) eAng.x -= SpeedActual;
				else if( eAng.x < TargetAngles.x ) eAng.x += SpeedActual;
			}
		}

		if( eAng.y != TargetAngles.y )
		{
			if( fabs( eAng.y - TargetAngles.y ) <= SpeedActual )
				eAng.y = TargetAngles.y;
			else
			{
				if( eAng.y > TargetAngles.y ) eAng.y -= SpeedActual;
				else if( eAng.y < TargetAngles.y ) eAng.y += SpeedActual;
			}
		}

		if( eAng.z != TargetAngles.z )
		{
			if( fabs( eAng.z - TargetAngles.z ) <= SpeedActual )
				eAng.z = TargetAngles.z;
			else
			{
				if( eAng.z > TargetAngles.z ) eAng.z -= SpeedActual;
				else if( eAng.z < TargetAngles.z ) eAng.z += SpeedActual;
			}
		}

		pEntity->SetAbsAngles( eAng );
	}
	else
	{
		ALERT( at_error, "env_angles \"%s\" lost its entity during transform, aborting.\n", STRING( pev->targetname ) );
		DontThink();
		return;
	}

	SetThink( &CEnvAngles::ChangeAngles );
	SetNextThink( 0 );
}