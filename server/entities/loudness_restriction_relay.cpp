#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"

//==========================================================================
// diffusion - loudness_restriction_relay
// When a player has a condition LoudWeaponsRestricted = true, and fires a "loud" weapon,
// this entity is activated and it fires a pev->target (mission fail, etc.)
//==========================================================================
#define SF_LRR_REMOVEONFIRE BIT(0)

class CLoudnessRestrictionRelay : public CBaseDelay
{
	DECLARE_CLASS( CLoudnessRestrictionRelay, CBaseDelay );
public:
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( loudness_restriction_relay, CLoudnessRestrictionRelay );


void CLoudnessRestrictionRelay::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster() )
		return;
	
	if( !pActivator || !pActivator->IsPlayer() )
		pActivator = CBaseEntity::Instance( INDEXENT( 1 ) );

	UTIL_FireTargets( GetTarget(), pActivator, pCaller, useType, value );

	if( HasSpawnFlags( SF_LRR_REMOVEONFIRE ) )
		UTIL_Remove( this );
}