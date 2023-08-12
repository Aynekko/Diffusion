#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "game/gamerules.h"
#include "triggers.h"

class CTriggerPlayerFreeze : public CBaseDelay
{
	DECLARE_CLASS(CTriggerPlayerFreeze, CBaseDelay);
public:
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	int ObjectCaps(void) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};
LINK_ENTITY_TO_CLASS(trigger_playerfreeze, CTriggerPlayerFreeze);

void CTriggerPlayerFreeze::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!pActivator || !pActivator->IsPlayer())
		pActivator = CBaseEntity::Instance(INDEXENT(1));

	if (!pActivator || !ShouldToggle(useType, FBitSet(pActivator->pev->flags, FL_FROZEN)))
		return;

	if (pActivator->pev->flags & FL_FROZEN)
		((CBasePlayer*)((CBaseEntity*)pActivator))->EnableControl(TRUE);
	else ((CBasePlayer*)((CBaseEntity*)pActivator))->EnableControl(FALSE);

	SUB_UseTargets(pActivator, USE_TOGGLE, 0);
}