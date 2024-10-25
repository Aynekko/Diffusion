#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"

//=======================================================================================
// diffusion - upon using, the entity will display a puzzle (or restart it)
//=======================================================================================

#define SF_PUZZLE_REMOVEONFIRE	BIT( 0 )

class CTriggerPuzzle : public CBaseDelay
{
	DECLARE_CLASS( CTriggerPuzzle, CBaseDelay );
public:
	void Spawn( void );
	void Precache( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( trigger_puzzle, CTriggerPuzzle );

void CTriggerPuzzle::Precache( void )
{
	if( !FStringNull( pev->noise ) )
		PRECACHE_SOUND( (char *)STRING( pev->noise ) );
}

void CTriggerPuzzle::Spawn( void )
{
	Precache();

	pev->iuser2 = bound( 4, pev->iuser2, 255 ); // puzzle field size
	pev->fuser1 = bound( 0.5f, pev->fuser1, 10.0f ); // time step when the correct block changes position

	// make a random code as a signature
	pev->iuser1 = RANDOM_LONG( 1, 255 );

	pev->solid = SOLID_NOT;
}

void CTriggerPuzzle::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( !pActivator || !pActivator->IsPlayer() )
		pActivator = CBaseEntity::Instance( INDEXENT( 1 ) );

	if( useType == USE_ON ) // solved!
	{
		// check signature
		if( value == pev->iuser1 )
		{
			UTIL_FireTargets( GetTarget(), pActivator, pCaller, USE_TOGGLE, 0 );

			if( !FStringNull( pev->noise ) )
				EMIT_SOUND( ENT( pev ), CHAN_BODY, (char *)STRING( pev->noise ), 1, ATTN_NORM );

			if( HasSpawnFlags( SF_PUZZLE_REMOVEONFIRE ) )
				UTIL_Remove( this );
		}

		return;
	}

	if( IsLockedByMaster( pActivator ) )
		return;

	// make a random code as a signature
	pev->iuser1 = RANDOM_LONG( 1, 255 );

	CBasePlayer *pPlayer = (CBasePlayer *)pActivator;

	MESSAGE_BEGIN( MSG_ONE, gmsgPuzzle, NULL, pPlayer->pev );
	WRITE_SHORT( entindex() );
	WRITE_BYTE( pev->iuser2 ); // field size
	WRITE_BYTE( (int)(pev->fuser1 * 10) ); // time step when the correct block changes position
	WRITE_BYTE( pev->iuser1 ); // signature to prevent solving remotely
	MESSAGE_END();
}