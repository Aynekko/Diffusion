#include "extdll.h"
#include "util.h"
#include "cbase.h"

//==========================================================================
// diffusion - cables
// Thanks to Bacontsu for the cable rendering code (original code by Overfloater)!
//==========================================================================
// env_cable + env_cable_manager

// pev->message = cable group
// vuser1 = origin of the 2nd point
// vuser2.x = water drip intensity (0 - 255)
// vuser2.y = number of cables (<=0 means 1 cable)
// vuser2.z = added slack for the next cable

class CEnvCable : public CPointEntity
{
	DECLARE_CLASS( CEnvCable, CPointEntity );
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void CalcBox( void );
	void CalcBoxThink( void );
	void CollectTarget( void );
	EHANDLE m_hTarget; // 2nd point (optional)
	void KeyValue( KeyValueData *pkvd );
	int WaterDropIntensity;
	int NumberOfCables;
	int AddSlack; // added slack for the next cable

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( env_cable, CEnvCable );

BEGIN_DATADESC( CEnvCable )
	DEFINE_FUNCTION( CalcBoxThink ),
	DEFINE_FUNCTION( CollectTarget ),
	DEFINE_FIELD( m_hTarget, FIELD_EHANDLE ),
	DEFINE_KEYFIELD( WaterDropIntensity, FIELD_INTEGER, "waterdrops" ),
	DEFINE_KEYFIELD( NumberOfCables, FIELD_INTEGER, "cables" ),
	DEFINE_KEYFIELD( AddSlack, FIELD_INTEGER, "addslack" ),
END_DATADESC()

void CEnvCable::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "waterdrops" ) )
	{
		WaterDropIntensity = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "cables" ) )
	{
		NumberOfCables = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "addslack" ) )
	{
		AddSlack = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CEnvCable::Spawn( void )
{
	SetNullModel(); // needed to pass to client

	// bound particle rate
	pev->vuser2.x = bound( 0, WaterDropIntensity, 255 );
	// number of cables
	pev->vuser2.y = bound( 1, NumberOfCables, 999 );
	// added slack
	pev->vuser2.z = bound( 5, AddSlack, 999 );

	if( !pev->vuser1 || pev->vuser1 == g_vecZero ) // user didn't specify 2nd point manually, so we need to find target
	{
		// get second point as self at first
		pev->vuser1 = GetAbsOrigin();
		SetThink( &CEnvCable::CollectTarget );
		SetNextThink( RANDOM_FLOAT( 0.1f, 0.2f ) ); // let all entities spawn
	}
	else
	{
		CalcBox();
		// this cable is attached to some moving object so we always need to recalculate its box
		if( m_hParent )
		{
			SetThink( &CEnvCable::CalcBoxThink );
			SetNextThink( RANDOM_FLOAT( 0.1f, 0.2f ) ); // let all entities spawn
		}
	}
	
	pev->iuser3 = -669; // indicator for client that it's a cable

	// segments
	if( !pev->iuser2 )
		pev->iuser2 = 20;

	pev->iuser2 = bound( 3, pev->iuser2, 49 );

	// fall depth
	if( !pev->fuser1 || pev->fuser1 <= 0.0f )
		pev->fuser1 = 0.0001f;

	// width
	if( !pev->fuser2 )
		pev->fuser2 = 0.5f;

	// sway intensity
	if( !pev->fuser3 )
		pev->fuser3 = 0.0f;

	// remember the value if fuser3 changes (I'll just use gaityaw here...)
	m_flGaitYaw = pev->fuser3;

	if( !pev->rendercolor || (pev->rendercolor.x == 0.0f && pev->rendercolor.y == 0.0f && pev->rendercolor.z == 0.0f) )
		pev->rendercolor = Vector( 1,1,1 ); // it becomes white if 0, Xash does this
}

void CEnvCable::CalcBox( void )
{
	if( m_hTarget != NULL )
		pev->vuser1 = m_hTarget->GetAbsOrigin();

	// find points
	Vector vmidpoint{ 0,0,0 };
	Vector mins{ 0,0,0 };
	Vector maxs = pev->vuser1 - pev->origin;

	// for slightly expanding bbox due to cable slacking
	float accounted_difference = fabs( pev->origin.z - pev->vuser1.z ) * 0.2f;

	// normalize
	for( int i = 0; i < 3; i++ )
		if( mins[i] > maxs[i] )
		{
			float swap = mins[i];
			mins[i] = maxs[i];
			maxs[i] = swap;
		}

	// after normalizing, then find drop point
	Vector diff = pev->vuser1 - pev->origin;
	vmidpoint = pev->origin + (diff * 0.5f);
	float total_falldepth = pev->fuser1;
	if( pev->vuser2.y > 1 ) // if we have more than 1 cable, increase falldepth accordingly
		total_falldepth += pev->vuser2.y * pev->vuser2.z; // number of cables * added slack per cable
	float lowest = (vmidpoint.z - (total_falldepth * 0.5f)) - pev->origin.z - accounted_difference;

	// check if mins is already lower than this
	if( mins.z > lowest )
		mins.z = lowest;

	pev->mins = mins;
	pev->maxs = maxs;

	UTIL_SetSize( pev, pev->mins, pev->maxs );

	SetAbsOrigin( pev->origin );
}

void CEnvCable::CalcBoxThink( void )
{
	SetNextThink( 0.2f );

	if( m_hTarget != NULL )
	{
		if( pev->origin == pev->oldorigin && m_hTarget->GetAbsOrigin() == pev->vuser1 )
			return;
	}
	else
	{
		if( pev->origin == pev->oldorigin )
			return;
	}

	CalcBox();

	SetNextThink( 0 ); // think fast!
}

void CEnvCable::CollectTarget(void)
{
	m_hTarget = UTIL_FindEntityByTargetname( NULL, GetTarget() );

	if( m_hTarget == NULL )
	{
		ALERT( at_error, "env_cable at position (%f %f %f) without ending position! Removed.\n", pev->origin.x, pev->origin.y, pev->origin.z );
		UTIL_Remove( this );
		return;
	}

	CalcBox();
	
	// this cable is attached to some moving object so we always need to recalculate its box
	if( m_hParent || (m_hTarget && m_hTarget->m_hParent) )
	{
		SetThink( &CEnvCable::CalcBoxThink );
		SetNextThink( 0.1f );
	}
}

void CEnvCable::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{	
	if( IsLockedByMaster() )
		return;
	
	// enable/disable
	if( pev->iuser3 == -669 )
	{
		pev->iuser3 = 0;
		pev->effects |= EF_NODRAW;
	}
	else
	{
		pev->iuser3 = -669;
		pev->effects = 0;
	}
}

//======================================================
// affects cable group with new sway values
// fuser1 - off value
// fuser2 - on value
// overrides fuser3 value in the cables
//======================================================

#define SF_CABLEMANAGER_USECABLEVALUE_ON	BIT(0)
#define SF_CABLEMANAGER_USECABLEVALUE_OFF	BIT(1)

class CEnvCableManager : public CBaseDelay
{
	DECLARE_CLASS( CEnvCableManager, CBaseDelay );
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( env_cable_manager, CEnvCableManager );

void CEnvCableManager::Spawn( void )
{
	m_iState = STATE_OFF;

	if( FStringNull( pev->message ) )
	{
		ALERT( at_warning, "env_cable_manager doesn't have cable group set. Removed.\n" );
		UTIL_Remove( this );
	}
}

void CEnvCableManager::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster() )
		return;

	// find all the cables
	CBaseEntity *pCable = NULL;

	float NewValue;
	if( useType == USE_ON )
	{
		NewValue = pev->fuser2;
		m_iState = STATE_ON;
	}
	else if( useType == USE_OFF )
	{
		NewValue = pev->fuser1;
		m_iState = STATE_OFF;
	}
	else // toggle
	{
		if( m_iState == STATE_OFF )
		{
			NewValue = pev->fuser2;
			m_iState = STATE_ON;
		}
		else
		{
			NewValue = pev->fuser1;
			m_iState = STATE_OFF;
		}
	}

	while( (pCable = UTIL_FindEntityByClassname( pCable, "env_cable" )) != NULL )
	{
		// this cable doesn't belong to the cable group we need
		if( pCable->pev->message != pev->message )
			continue;
			
		if( HasSpawnFlags( SF_CABLEMANAGER_USECABLEVALUE_ON ) && (m_iState == STATE_ON) )
			pCable->pev->fuser3 = pCable->m_flGaitYaw;
		else if( HasSpawnFlags( SF_CABLEMANAGER_USECABLEVALUE_OFF ) && (m_iState == STATE_OFF) )
			pCable->pev->fuser3 = pCable->m_flGaitYaw;
		else
			pCable->pev->fuser3 = NewValue;
	}

	UTIL_FireTargets( GetTarget(), pActivator, pCaller, useType, value );
}