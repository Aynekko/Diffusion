#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "game/gamerules.h"
#include "triggers.h"

class CTriggerGravity : public CBaseTrigger
{
	DECLARE_CLASS(CTriggerGravity, CBaseTrigger);
public:
	void Spawn(void);
	void GravityTouch(CBaseEntity* pOther);

	DECLARE_DATADESC();
};
LINK_ENTITY_TO_CLASS(trigger_gravity, CTriggerGravity);

BEGIN_DATADESC(CTriggerGravity)
	DEFINE_FUNCTION(GravityTouch),
END_DATADESC()

void CTriggerGravity::Spawn(void)
{
	InitTrigger();
	SetTouch(&CTriggerGravity::GravityTouch);
}

void CTriggerGravity::GravityTouch(CBaseEntity* pOther)
{
	// Only save on clients
	if (!pOther->IsPlayer())
		return;

	pOther->pev->gravity = pev->gravity;
}