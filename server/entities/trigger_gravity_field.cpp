#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "game/gamerules.h"
#include "triggers.h"

class CTriggerGravityField : public CTriggerInOut
{
	DECLARE_CLASS(CTriggerGravityField, CTriggerInOut);
public:
	virtual void FireOnEntry(CBaseEntity* pOther);
	virtual void FireOnLeaving(CBaseEntity* pOther);
};

void CTriggerGravityField::FireOnEntry(CBaseEntity* pOther)
{
	// Only save on clients
	if (!pOther->IsPlayer()) return;
	pOther->pev->gravity = pev->gravity;
}

void CTriggerGravityField::FireOnLeaving(CBaseEntity* pOther)
{
	if (!pOther->IsPlayer()) return;
	pOther->pev->gravity = 1.0f;
}

LINK_ENTITY_TO_CLASS(trigger_gravity_field, CTriggerGravityField);