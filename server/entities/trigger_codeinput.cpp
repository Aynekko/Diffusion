#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"

//=======================================================================================
// diffusion - upon using with USE_TOGGLE, the entity will show "enter code" screen;
// using with USE_ON and the correct code (stored in iuser1) will activate the target
//=======================================================================================

#define SF_CODE_REMOVEONFIRE	BIT( 0 )

class CTriggerCodeInput : public CBaseDelay
{
	DECLARE_CLASS( CTriggerCodeInput, CBaseDelay );
public:
	void Spawn(void);
	void Precache( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS(trigger_codeinput, CTriggerCodeInput );

void CTriggerCodeInput::Precache(void)
{
	if( !FStringNull( pev->noise ) )
		PRECACHE_SOUND( (char *)STRING( pev->noise ) );
}

void CTriggerCodeInput::Spawn(void)
{
	Precache();
	
	pev->solid = SOLID_NOT;
}

void CTriggerCodeInput::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( !pActivator || !pActivator->IsPlayer() )
		pActivator = CBaseEntity::Instance( INDEXENT( 1 ) );

	CBasePlayer *pPlayer = (CBasePlayer *)pActivator;

	if( IsLockedByMaster( pActivator ) )
		return;
	
	// success
	if( useType == USE_ON && (int)value == pev->iuser1 )
	{
		UTIL_FireTargets( GetTarget(), pActivator, pCaller, USE_TOGGLE, 0 );

		if( !FStringNull( pev->noise ) )
			EMIT_SOUND( ENT( pev ), CHAN_BODY, (char *)STRING( pev->noise ), 1, ATTN_NORM );

		if( HasSpawnFlags( SF_CODE_REMOVEONFIRE ) )
			UTIL_Remove( this );

		return;
	}

	// show "enter code" screen
	MESSAGE_BEGIN( MSG_ONE, gmsgCodeInput, NULL, pPlayer->pev );
		WRITE_SHORT( entindex() );
		WRITE_BYTE( pev->iuser1 / 1000 % 10 );
		WRITE_BYTE( pev->iuser1 / 100 % 10 );
		WRITE_BYTE( pev->iuser1 / 10 % 10 );
		WRITE_BYTE( pev->iuser1 % 10 );
	MESSAGE_END();
}