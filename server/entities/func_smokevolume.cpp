#include "extdll.h"
#include "util.h"
#include "cbase.h"

// =================== FUNC_SMOKEVOLUME ==============================================

/*
iuser1 0 = smoke
iuser1 1 = dustmotes
iuser2 = particle visible distance
fuser1 = particle size
fuser2 = density
fuser3 = delay between emissions
renderamt = transparency
vuser1 = add velocity on spawn
vuser2 = randomize velocity direction (+/-)
*/

class CFuncSmokeVolume : public CBaseEntity
{
	DECLARE_CLASS( CFuncSmokeVolume, CBaseEntity );
public:
	virtual int ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void Spawn( void );
};

LINK_ENTITY_TO_CLASS( func_smokevolume, CFuncSmokeVolume );

void CFuncSmokeVolume::Spawn( void )
{
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_TRIGGER;
	pev->effects |= EF_DIMLIGHT;
	pev->iuser3 = -660; // this entity is distinguished on client by "dimlight + iuser3"
	pev->rendermode = kRenderTransTexture; // otherwise renderamt is always 255

	if( !pev->fuser3 || pev->fuser3 <= 0.0f )
		pev->fuser3 = 2.0f;

	// keep them above zero
	if( !pev->vuser2.IsNull() )
	{
		if( pev->vuser2.x < 0.0f )
			pev->vuser2.x = -pev->vuser2.x;
		if( pev->vuser2.y < 0.0f )
			pev->vuser2.y = -pev->vuser2.x;
		if( pev->vuser2.z < 0.0f )
			pev->vuser2.z = -pev->vuser2.x;
	}

	SET_MODEL( edict(), GetModel() );
}