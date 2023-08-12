#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "basemonster.h"

//==================================================================
// diffusion - reason for this entity is only one - hull issues
// it is better for me to just create a point entity and then parent
// the rest of the brushes to it (see the beginning of the map ch2map2)
//==================================================================
class CEnvHoldable : public CPointEntity
{
	DECLARE_CLASS( CEnvHoldable, CPointEntity );
public:
	void Spawn( void );
	int ObjectCaps(void);
	void Think( void );
};

LINK_ENTITY_TO_CLASS( env_holdable, CEnvHoldable );

int CEnvHoldable::ObjectCaps( void )
{
	int flags = (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION);

	flags |= FCAP_HOLDABLE_ITEM;

	return flags;
}

void CEnvHoldable::Spawn( void )
{
//	pev->movetype = MOVETYPE_PUSHSTEP;
	pev->movetype = MOVETYPE_TOSS;
	UTIL_SetSize( pev, Vector( -1, -1, -1 ), Vector( 1,1,1 ) );
	pev->solid = SOLID_BBOX;

	Vector origin = GetLocalOrigin();
	origin.z += 1; // Pick up off of the floor
	UTIL_SetOrigin( this, origin );

	SetNextThink( 0.5 );
}

// hack to not stuck
void CEnvHoldable::Think(void)
{
	SetNextThink( 1.0 );
	pev->flags &= ~FL_ONGROUND;
}