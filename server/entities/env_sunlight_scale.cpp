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
// fuser3 - speed when going OFF
// frags - speed when going ON
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
	pev->fuser1 = bound( 0.1f, pev->fuser1, 25.0f ); // go dark

	if( pev->fuser2 <= 0 )
		pev->fuser2 = 1.0f; // bring back default sunlight

	pev->fuser2 = bound( 0.1f, pev->fuser2, 25.0f );

	if( pev->frags <= 0 )
		pev->frags = 25.0f;

	if( pev->fuser3 <= 0 )
		pev->fuser3 = 75.0f;

	// because we will send short through player's code
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

	// is there another entity messing with the sunlight right now? we need to stop them before proceeding
	CBaseEntity *pOther = NULL;
	while( (pOther = UTIL_FindEntityByClassname( pOther, "env_sunlight_scale" )) != NULL )
		pOther->DontThink();

	SetThink( &CEnvSunlightScale::UpdateSunlightScale );
	SetNextThink( 0 );
}

void CEnvSunlightScale::UpdateSunlightScale( void )
{
	edict_t *pWorld = INDEXENT( 0 );

	float speed = (m_iState ? pev->frags : pev->fuser3);

	if( speed >= 9999.0f ) // just do instant
		pWorld->v.fuser1 = pev->dmg;
	else
		pWorld->v.fuser1 = UTIL_Approach( pev->dmg, pWorld->v.fuser1, speed * gpGlobals->frametime );

	if( pWorld->v.fuser1 == pev->dmg )
	{
		DontThink();
		return;
	}

	SetNextThink( 0 );
}