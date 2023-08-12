#include "extdll.h"
#include "util.h"
#include "cbase.h"

//=================================================================
// func_carblocker: blocks car from moving (acts as a wall)
// doesn't block anything else
// iuser1 temporary holds the value of skin (to choose if we are blocking a car or a boat)
//=================================================================

#define SF_CARBLOCKER_STARTOFF BIT(0) // spawn invisible

class CFuncCarBlocker : public CBaseEntity
{
	DECLARE_CLASS( CFuncCarBlocker, CBaseEntity );
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( func_carblocker, CFuncCarBlocker );

void CFuncCarBlocker::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	if( pev->iuser1 <= 0 )
		pev->iuser1 = CONTENT_CARBLOCKER;

	if( HasSpawnFlags( SF_CARBLOCKER_STARTOFF ) )
	{
		pev->skin = 0;
		SET_MODEL( edict(), GetModel() );
	}
	else
	{
		pev->skin = pev->iuser1; // skin must be set before set_model!!! (the more you know...)
		SET_MODEL( edict(), GetModel() );
	}
}

void CFuncCarBlocker::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster() )
		return;
	
	if( pev->skin == 0 )
		pev->skin = pev->iuser1;
	else
		pev->skin = 0;

	SET_MODEL( edict(), GetModel() );
}