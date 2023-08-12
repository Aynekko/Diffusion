#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "game/gamerules.h"
#include "game/game.h"

//=========================================================
// Multiplayer intermission spots.
//=========================================================
class CInfoIntermission : public CPointEntity
{
	DECLARE_CLASS(CInfoIntermission, CPointEntity);
public:
	void Spawn(void);
	void Think(void);
};

void CInfoIntermission::Spawn(void)
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;

	pev->nextthink = gpGlobals->time + 2; // let targets spawn!
}

void CInfoIntermission::Think(void)
{
	CBaseEntity* pTarget;

	// find my target
	pTarget = UTIL_FindEntityByTargetname(NULL, STRING(pev->target));

	if (!FNullEnt(pTarget))
		pev->v_angle = UTIL_VecToAngles((pTarget->GetAbsOrigin() - GetAbsOrigin()).Normalize());
}

LINK_ENTITY_TO_CLASS(info_intermission, CInfoIntermission);