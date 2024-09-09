#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "triggers.h"

//=======================================================================================================================
// diffusion - change the brightness of sunlight
//=======================================================================================================================

// fuser1 - new scale when ON
// fuser2 - new scale when OFF
// frags - speed
// dmg - current target value

class CEnvSunlightScale : public CBaseDelay
{
	DECLARE_CLASS( CEnvSunlightScale, CBaseDelay );
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void UpdateSunlightScale( void ); // think function

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( env_sunlight_scale, CEnvSunlightScale );

BEGIN_DATADESC( CEnvSunlightScale )
	DEFINE_FUNCTION( UpdateSunlightScale ),
END_DATADESC();

void CEnvSunlightScale::Spawn( void )
{
	if( pev->fuser1 <= 0 )
		pev->fuser1 = 0.1f; // go dark

	if( pev->fuser2 <= 0 )
		pev->fuser2 = 1.0f; // bring back default sunlight

	if( pev->frags <= 0 )
		pev->frags = 25.0f;

	// because we will send byte through player's code
	pev->fuser1 *= 100.f;
	pev->fuser2 *= 100.f;

	m_iState = STATE_OFF;
}

void CEnvSunlightScale::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster( pActivator ) )
		return;

	if( m_iState == STATE_OFF || useType == USE_ON )
	{
		pev->dmg = pev->fuser1;
		m_iState = STATE_ON;
	}
	else if( m_iState == STATE_ON || useType == USE_OFF )
	{
		pev->dmg = pev->fuser2;
		m_iState = STATE_OFF;
	}

	SetThink( &CEnvSunlightScale::UpdateSunlightScale );
	SetNextThink( 0 );
}

void CEnvSunlightScale::UpdateSunlightScale( void )
{
	edict_t *pWorld = INDEXENT( 0 );

	pWorld->v.fuser1 = UTIL_Approach( pev->dmg, pWorld->v.fuser1, pev->frags * gpGlobals->frametime );

	if( pWorld->v.fuser1 == pev->dmg )
	{
		DontThink();
		return;
	}

	SetNextThink( 0 );
}