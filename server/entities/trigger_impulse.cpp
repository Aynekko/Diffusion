#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "game/gamerules.h"
#include "triggers.h"

//=====================================================
// trigger_impulse: activate a player command
//=====================================================
class CTriggerImpulse : public CBaseDelay
{
	DECLARE_CLASS(CTriggerImpulse, CBaseDelay);
public:
	void Spawn(void);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	virtual int ObjectCaps(void) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};
LINK_ENTITY_TO_CLASS(trigger_impulse, CTriggerImpulse);

void CTriggerImpulse::Spawn(void)
{
	// apply default name
	if (FStringNull(pev->targetname))
		pev->targetname = MAKE_STRING("game_firetarget");
}

void CTriggerImpulse::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	int iImpulse = (int)value;

	if (IsLockedByMaster(pActivator))
		return;

	// run custom filter for entity
	if (!pev->impulse || (pev->impulse == iImpulse))
	{
		UTIL_FireTargets(STRING(pev->target), pActivator, this, USE_TOGGLE, value);
	}
}