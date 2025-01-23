#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"

//=======================================================================================
// diffusion - pass a hint to the client. It will pop up from the bottom of the screen
// pev->message contains a header text from titles.txt
//=======================================================================================

#define SF_HINT_REMOVEONFIRE	BIT( 0 )

class CHintEntity : public CBaseDelay
{
	DECLARE_CLASS( CHintEntity, CBaseDelay );
public:
	void Spawn(void);
	void Precache( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( hint, CHintEntity );

void CHintEntity::Precache(void)
{
	PRECACHE_SOUND( "player/hint.wav" );
}

void CHintEntity::Spawn(void)
{
	Precache();
	
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;

	if( !pev->message )
		ALERT( at_error, "Hint entity without a hint message!\n" );
}

void CHintEntity::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( !pActivator || !pActivator->IsPlayer() )
		pActivator = CBaseEntity::Instance( INDEXENT( 1 ) );

	CBasePlayer *pPlayer = (CBasePlayer *)pActivator;

	if( IsLockedByMaster( pActivator ) )
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsgHint, NULL, pPlayer->edict() );
		WRITE_BYTE( 0 );
		WRITE_STRING( STRING( pev->message ) );
	MESSAGE_END();
	
	if( pev->target )
		UTIL_FireTargets( pev->target, pActivator, this, USE_TOGGLE );

	if( HasSpawnFlags( SF_HINT_REMOVEONFIRE ) )
		UTIL_Remove( this );
}