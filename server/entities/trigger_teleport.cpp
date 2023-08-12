#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "triggers.h"

//======================================
// teleport trigger
//
//

void CBaseTrigger::TeleportTouch(CBaseEntity* pOther)
{
	CBaseEntity* pTarget = NULL;

	if (!CanTouch(pOther)) return;

	if (IsLockedByMaster(pOther))
		return;

	pTarget = UTIL_FindEntityByTargetname(pTarget, STRING(pev->target));
	if (!pTarget) return;

	// diffusion - check for validity
	CBaseEntity *ent = NULL;
	while( (ent = UTIL_FindEntityInSphere( ent, pTarget->GetAbsOrigin(), 128 )) != NULL )
	{
		if( ent->IsPlayer() || ent->IsMonster() )
		{
			if( ent->IsAlive() )
				return;
		}
	}

	Vector tmp = pTarget->GetAbsOrigin();
	Vector pAngles = pTarget->GetAbsAngles();

	if (pOther->IsPlayer())
		tmp.z -= pOther->pev->mins.z; // make origin adjustments in case the teleportee is a player. (origin in center, not at feet)
	tmp.z++;

	pOther->Teleport(&tmp, &pAngles, &g_vecZero);
	pOther->pev->flags &= ~FL_ONGROUND;

	UTIL_FireTargets(pev->message, pOther, this, USE_TOGGLE); // fire target on pass
}

class CTriggerTeleport : public CBaseTrigger
{
	DECLARE_CLASS(CTriggerTeleport, CBaseTrigger);
public:
	void Spawn(void);
};

LINK_ENTITY_TO_CLASS(trigger_teleport, CTriggerTeleport);

void CTriggerTeleport::Spawn(void)
{
	InitTrigger();
	SetTouch(&CBaseTrigger::TeleportTouch);
}

LINK_ENTITY_TO_CLASS(info_teleport_destination, CPointEntity);