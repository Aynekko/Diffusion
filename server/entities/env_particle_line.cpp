#include "extdll.h"
#include "util.h"
#include "cbase.h"

//========================================================================================
// creates particles between two points in random positions. Currently I use it for water droplets only.
// renderamt - intensity (0 - 255)
// iuser1 - particle type:
// 0: rain drips
//========================================================================================

#define SF_ENVPARTLINE_STARTOFF BIT(0)

class CEnvParticleLine : public CBaseDelay
{
	DECLARE_CLASS( CEnvParticleLine, CBaseDelay );
public:
	void Spawn( void );
	void FindSecondPoint( void );

	DECLARE_DATADESC();
};

BEGIN_DATADESC( CEnvParticleLine )
	DEFINE_FUNCTION( FindSecondPoint ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( env_particle_line, CEnvParticleLine );

void CEnvParticleLine::Spawn(void)
{
	SetNullModel();
	pev->rendermode = kRenderTransTexture; // need to set this so the engine wouldn't mess with renderamt

	SetThink( &CEnvParticleLine::FindSecondPoint );
	SetNextThink( 0.25f ); // let all entities spawn
}

void CEnvParticleLine::FindSecondPoint(void)
{
	CBaseEntity *SecondPoint = UTIL_FindEntityByTargetname( NULL, GetTarget() );

	if( !SecondPoint )
	{
		ALERT( at_error, "env_particle_line at position (%f %f %f) without second point! Removed.\n", pev->origin.x, pev->origin.y, pev->origin.z );
		UTIL_Remove( this );
		return;
	}

	pev->vuser1 = SecondPoint->GetAbsOrigin();

	DontThink();

	if( !HasSpawnFlags( SF_ENVPARTLINE_STARTOFF ) )
		pev->renderfx = kRenderFxParticleLine;
}