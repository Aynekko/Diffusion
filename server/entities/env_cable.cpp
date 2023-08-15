#include "extdll.h"
#include "util.h"
#include "cbase.h"
//#include "player.h"

//==========================================================================
// diffusion - cables
// Thanks to Bacontsu for the cable rendering code (original code by Overfloater)!
//==========================================================================
// env_cable + env_cable_manager

class CEnvCable : public CPointEntity
{
	DECLARE_CLASS( CEnvCable, CPointEntity );
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void CalcBox( void );
	void CalcBoxThink( void );

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( env_cable, CEnvCable );

BEGIN_DATADESC( CEnvCable )
	DEFINE_FUNCTION( CalcBoxThink ),
END_DATADESC()

void CEnvCable::Spawn( void )
{
	if( !pev->vuser1 )
	{
		ALERT( at_error, "env_cable at position (%f %f %f) without ending position! Removed.\n", pev->origin.x, pev->origin.y, pev->origin.z );
		UTIL_Remove( this );
		return;
	}
	
	SetNullModel(); // needed to pass to client
	pev->iuser3 = -669; // indicator for client that it's a cable

	// segments
	if( !pev->iuser2 )
		pev->iuser2 = 20;

	pev->iuser2 = bound( 3, pev->iuser2, 49 );

	// fall depth
	if( !pev->fuser1 )
		pev->fuser1 = 50.0f;

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

	CalcBox();

	// this cable is attached to some moving object so we always need to recalculate its box
	if( m_hParent )
	{
		SetThink( &CEnvCable::CalcBoxThink );
		SetNextThink( RANDOM_FLOAT( 1.0f, 1.2f ) );
	}
}

void CEnvCable::CalcBox( void )
{
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
	float lowest = (vmidpoint.z - (pev->fuser1 / 2.0f)) - pev->origin.z - accounted_difference;

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
	SetNextThink( 1.0f );

	if( pev->origin == pev->oldorigin )
		return;

	CalcBox();

	SetNextThink( 0.1f ); // think fast!
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
// affects cable with new sway values
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

	while( (pCable = UTIL_FindEntityByTargetname( pCable, STRING( pev->target ), pActivator )) != NULL )
	{
		if( pCable->pev->iuser3 != -669 )
			continue;

		if( HasSpawnFlags( SF_CABLEMANAGER_USECABLEVALUE_ON ) && (m_iState == STATE_ON) )
			pCable->pev->fuser3 = pCable->m_flGaitYaw;
		else if( HasSpawnFlags( SF_CABLEMANAGER_USECABLEVALUE_OFF ) && (m_iState == STATE_OFF) )
			pCable->pev->fuser3 = pCable->m_flGaitYaw;
		else
			pCable->pev->fuser3 = NewValue;
	}
}