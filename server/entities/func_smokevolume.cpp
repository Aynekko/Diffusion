#include "extdll.h"
#include "util.h"
#include "cbase.h"

// =================== FUNC_SMOKEVOLUME ==============================================

#define SF_SMOKEVOL_STARTOFF BIT(0)

/*
iuser1 0 = smoke
iuser1 1 = dustmotes
iuser1 2 = smoke (fullbright)
iuser1 3 = dustmotes (fullbright)
iuser2 = particle visible distance
fuser1 = particle size
fuser2 = density
fuser3 = delay between emissions
fuser4 = number of particles at a time
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
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( func_smokevolume, CFuncSmokeVolume );

void CFuncSmokeVolume::Spawn( void )
{
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_TRIGGER;
	pev->rendermode = kRenderTransTexture; // otherwise renderamt is always 255

	if( !pev->fuser3 || pev->fuser3 <= 0.0f )
		pev->fuser3 = 2.0f;

	// keep them above zero
	if( !pev->vuser2.IsNull() )
	{
		pev->vuser2.x = fabs( pev->vuser2.x );
		pev->vuser2.y = fabs( pev->vuser2.y );
		pev->vuser2.z = fabs( pev->vuser2.z );
	}

	SET_MODEL( edict(), GetModel() );

	if( !HasSpawnFlags( SF_SMOKEVOL_STARTOFF ) )
	{
		pev->effects |= EF_DIMLIGHT;
		pev->iuser3 = -660; // this entity is distinguished on client by "dimlight + iuser3"
	}
	else
		pev->effects |= EF_NODRAW;
}

void CFuncSmokeVolume::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster() )
		return;

	if( pev->effects & EF_NODRAW ) // off, turn on
	{
		pev->effects |= EF_DIMLIGHT;
		pev->iuser3 = -660; // this entity is distinguished on client by "dimlight + iuser3"
		pev->effects &= ~EF_NODRAW;
	}
	else // turn off
	{
		pev->effects &= ~EF_DIMLIGHT;
		pev->effects |= EF_NODRAW;
		pev->iuser3 = 0;
	}
}