#include "extdll.h"
#include "util.h"
#include "cbase.h"
//#include "player.h"

//==========================================================================
// diffusion - cables
// Thanks to Bacontsu for the cable rendering code!
//==========================================================================
//#define SF_CABLE_SWAY		BIT(0)

// env_cable + env_cable_manager

class CEnvCable : public CPointEntity
{
	DECLARE_CLASS( CEnvCable, CPointEntity );
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void ClearEffects( void );
};

LINK_ENTITY_TO_CLASS( env_cable, CEnvCable );

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
		pev->fuser1 = 10.0f;

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

	// FIXME this is not completely correct yet
	// find points
	Vector vmidpoint{ 0,0,0 };
	Vector mins{ 0,0,0 };
	Vector maxs = pev->vuser1 - pev->origin;

	// normalize
	for( int i = 0; i < 3; i++ )
		if( mins[i] > maxs[i] )
		{
			float swap = mins[i];
			mins[i] = maxs[i];
			maxs[i] = swap;
		}

	// after normalizing, then find drop point
	Vector vdirection;
	VectorSubtract( pev->vuser1, pev->origin, vdirection );
	VectorMA( pev->origin, 0.5, vdirection, vmidpoint );
	mins.z = (vmidpoint.z - pev->fuser1) - pev->origin.z;
		
	UTIL_SetSize( pev, mins, maxs );
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

void CEnvCable::ClearEffects( void )
{
	// delete ending point
	CBaseEntity *pEnd = CBaseEntity::Instance( pev->enemy );
	if( pEnd )
	{
		UTIL_Remove( pEnd );
		pev->enemy = NULL;
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

		if( HasSpawnFlags( SF_CABLEMANAGER_USECABLEVALUE_ON ) && (m_iState == STATE_OFF) )
			pCable->pev->fuser3 = pCable->m_flGaitYaw;
		else if( HasSpawnFlags( SF_CABLEMANAGER_USECABLEVALUE_OFF ) && (m_iState == STATE_ON) )
			pCable->pev->fuser3 = pCable->m_flGaitYaw;
		else
			pCable->pev->fuser3 = NewValue;
	}
}