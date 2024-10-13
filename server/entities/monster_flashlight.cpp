#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "basemonster.h"

class CFlashlightMonster : public CBaseMonster
{
	DECLARE_CLASS( CFlashlightMonster, CBaseMonster );
public:
	void Spawn( void );
	int ObjectCaps( void ) { return FCAP_DONT_SAVE; }
};

LINK_ENTITY_TO_CLASS( _flashlight, CFlashlightMonster );

void CFlashlightMonster::Spawn(void)
{
#if 0 // for testing purposes
	PRECACHE_MODEL( "sprites/ballsmoke.spr" );
	SET_MODEL( ENT(pev), "sprites/ballsmoke.spr" );
	pev->scale = 0.2;
#endif
}