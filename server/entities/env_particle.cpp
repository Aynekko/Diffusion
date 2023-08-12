#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"

//==========================================================================
// diffusion - queakparticle
// iuser1 = particle number, hardcoded and taken from particles.txt
// fuser1 = delay between emissions
//==========================================================================
#define SF_ENVPARTICLE_STARTOFF BIT(0)

class CEnvParticle : public CBaseDelay
{
	DECLARE_CLASS( CEnvParticle, CBaseDelay );
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( env_particle, CEnvParticle );

void CEnvParticle::Spawn(void)
{
	SetNullModel();
	
	if( !HasSpawnFlags(SF_ENVPARTICLE_STARTOFF) )
		pev->renderfx = kRenderFxParticle;

	if( pev->fuser1 <= 0 )
		pev->fuser1 = 0.5f;
}

void CEnvParticle::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster() )
		return;
	
	if( pev->renderfx == 0 )
		pev->renderfx = kRenderFxParticle;
	else
		pev->renderfx = 0;
}