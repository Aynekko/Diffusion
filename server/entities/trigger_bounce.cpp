#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "triggers.h"

//===========================================================
//LRC- trigger_bounce
//===========================================================
#define SF_BOUNCE_CUTOFF 	BIT( 4 )

class CTriggerBounce : public CBaseTrigger
{
	DECLARE_CLASS(CTriggerBounce, CBaseTrigger);
public:
	void Spawn(void);
	void Touch(CBaseEntity* pOther);
};

LINK_ENTITY_TO_CLASS(trigger_bounce, CTriggerBounce);


void CTriggerBounce::Spawn(void)
{
	InitTrigger();
}

void CTriggerBounce::Touch(CBaseEntity* pOther)
{
	if (!UTIL_IsMasterTriggered(m_sMaster, pOther))
		return;
	if (!CanTouch(pOther))
		return;

	float dot = DotProduct(pev->movedir, pOther->pev->velocity);
	if (dot < -pev->armorvalue)
	{
		if (pev->spawnflags & SF_BOUNCE_CUTOFF)
			pOther->pev->velocity = pOther->pev->velocity - (dot + pev->frags * (dot + pev->armorvalue)) * pev->movedir;
		else
			pOther->pev->velocity = pOther->pev->velocity - (dot + pev->frags * dot) * pev->movedir;
		SUB_UseTargets(pOther, USE_TOGGLE, 0);
	}
}