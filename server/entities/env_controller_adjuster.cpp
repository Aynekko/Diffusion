#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "triggers.h"

//=======================================================================================================================
// diffusion - change entity's controller values
// mode: set new controller value, add value
//=======================================================================================================================

class CEnvControllerAdjuster : public CBaseDelay
{
	DECLARE_CLASS( CEnvControllerAdjuster, CBaseDelay );
public:
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void KeyValue( KeyValueData *pkvd );
	void AdjustControllers( void ); // think function

	string_t Entity;
	int ctrl0, ctrl1, ctrl2, ctrl3;
	int Mode;

	// only used in AdjustControllers
	int TargetCtrl[4];
	float CurCtrl[4];

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( env_controller_adjuster, CEnvControllerAdjuster );

BEGIN_DATADESC( CEnvControllerAdjuster )
	DEFINE_KEYFIELD( ctrl0, FIELD_INTEGER, "set0" ),
	DEFINE_KEYFIELD( ctrl1, FIELD_INTEGER, "set1" ),
	DEFINE_KEYFIELD( ctrl2, FIELD_INTEGER, "set2" ),
	DEFINE_KEYFIELD( ctrl3, FIELD_INTEGER, "set3" ),
	DEFINE_KEYFIELD( Mode, FIELD_INTEGER, "mode" ),
	DEFINE_KEYFIELD( Entity, FIELD_STRING, "entityname" ),
	DEFINE_ARRAY( TargetCtrl, FIELD_INTEGER, 4 ),
	DEFINE_ARRAY( CurCtrl, FIELD_FLOAT, 4 ),
	DEFINE_FUNCTION( AdjustControllers ),
END_DATADESC();

void CEnvControllerAdjuster::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "set0" ) )
	{
		ctrl0 = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "set1" ) )
	{
		ctrl1 = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "set2" ) )
	{
		ctrl2 = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "set3" ) )
	{
		ctrl3 = Q_atoi( pkvd->szValue );
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

void CEnvControllerAdjuster::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( !Entity )
	{
		ALERT( at_error, "env_controller_adjuster \"%s\" doesn't have an entity specified!\n", STRING( pev->targetname ) );
		return;
	}

	if( IsLockedByMaster( pActivator ) )
		return;

	/*
	Modes:
	0: set new controller values
	1: add new values to existing
	2: set new smoothly
	3: add new smoothly
	*/

	if( !Mode ) Mode = 0; // default mode: set new controller values.
	if( !ctrl0 ) ctrl0 = 127;
	if( !ctrl1 ) ctrl1 = 127;
	if( !ctrl2 ) ctrl2 = 127;
	if( !ctrl3 ) ctrl3 = 127;
	if( !pev->speed ) pev->speed = 100;
	ctrl0 = bound( 0, ctrl0, 255 );
	ctrl1 = bound( 0, ctrl1, 255 );
	ctrl2 = bound( 0, ctrl2, 255 );
	ctrl3 = bound( 0, ctrl3, 255 );

	CBaseEntity *pEntity = UTIL_FindEntityByTargetname( pEntity, STRING( Entity ) );

	if( !pEntity )
	{
		ALERT( at_error, "env_controller_adjuster \"%s\" can't find specified entity!\n", STRING( pev->targetname ) );
		return;
	}

	switch( Mode )
	{
		// set new controller values:
	case 0: 
		pEntity->pev->controller[0] = ctrl0;
		pEntity->pev->controller[1] = ctrl1;
		pEntity->pev->controller[2] = ctrl2;
		pEntity->pev->controller[3] = ctrl3;
		break;
		// add:
	case 1:
		pEntity->pev->controller[0] += ctrl0;
		pEntity->pev->controller[1] += ctrl1;
		pEntity->pev->controller[2] += ctrl2;
		pEntity->pev->controller[3] += ctrl3;
		break;
		// set new angles, smoothly
	case 2:
		TargetCtrl[0] = ctrl0;
		TargetCtrl[1] = ctrl1;
		TargetCtrl[2] = ctrl2;
		TargetCtrl[3] = ctrl3;
		CurCtrl[0] = pEntity->pev->controller[0];
		CurCtrl[1] = pEntity->pev->controller[1];
		CurCtrl[2] = pEntity->pev->controller[2];
		CurCtrl[3] = pEntity->pev->controller[3];
		SetThink( &CEnvControllerAdjuster::AdjustControllers );
		SetNextThink( 0 );
		break;
	case 3:
		TargetCtrl[0] = pEntity->pev->controller[0] + ctrl0;
		TargetCtrl[1] = pEntity->pev->controller[1] + ctrl1;
		TargetCtrl[2] = pEntity->pev->controller[2] + ctrl2;
		TargetCtrl[3] = pEntity->pev->controller[3] + ctrl3;
		CurCtrl[0] = pEntity->pev->controller[0];
		CurCtrl[1] = pEntity->pev->controller[1];
		CurCtrl[2] = pEntity->pev->controller[2];
		CurCtrl[3] = pEntity->pev->controller[3];
		SetThink( &CEnvControllerAdjuster::AdjustControllers );
		SetNextThink( 0 );
		break;
	}
}

void CEnvControllerAdjuster::AdjustControllers( void )
{
	// we have to perform a check again...
	CBaseEntity *pEntity = NULL;

	float SpeedActual = pev->speed * gpGlobals->frametime;

	if( (pEntity = UTIL_FindEntityByTargetname( pEntity, STRING( Entity ) )) != NULL )
	{
		for( int i = 0; i < 4; i++ )
		{
			CurCtrl[i] = UTIL_Approach( TargetCtrl[i], pEntity->pev->controller[i], SpeedActual );
			pEntity->pev->controller[i] = (int)CurCtrl[i];
		}

		// all done?
		if( (int)pEntity->pev->controller[0] == TargetCtrl[0] && (int)pEntity->pev->controller[1] == TargetCtrl[1] && (int)pEntity->pev->controller[2] == TargetCtrl[2] && (int)pEntity->pev->controller[3] == TargetCtrl[3] )
		{
			DontThink();
			return;
		}
	}
	else
	{
		ALERT( at_warning, "env_controller_adjuster \"%s\" lost its entity during transform, aborting.\n", STRING( pev->targetname ) );
		DontThink();
		return;
	}

	SetNextThink( 0 );
}