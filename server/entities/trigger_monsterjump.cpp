#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "game/gamerules.h"
#include "triggers.h"

//
// trigger_monsterjump
//
class CTriggerMonsterJump : public CBaseTrigger
{
	DECLARE_CLASS(CTriggerMonsterJump, CBaseTrigger);
public:
	void Spawn(void);
	void Touch(CBaseEntity* pOther);
	void Think(void);
};

LINK_ENTITY_TO_CLASS(trigger_monsterjump, CTriggerMonsterJump);

void CTriggerMonsterJump::Spawn(void)
{
	InitTrigger();

	pev->nextthink = 0;
	pev->speed = 200;
	m_flHeight = 150;

	if (!FStringNull(pev->targetname))
	{
		// if targetted, spawn turned off
		pev->solid = SOLID_NOT;
		RelinkEntity(FALSE); // Unlink from trigger list
		SetUse(&CBaseTrigger::ToggleUse);
	}
}


void CTriggerMonsterJump::Think(void)
{
	pev->solid = SOLID_NOT;// kill the trigger for now !!!UNDONE
	RelinkEntity(FALSE); // Unlink from trigger list
	SetThink(NULL);
}

void CTriggerMonsterJump::Touch(CBaseEntity* pOther)
{
	entvars_t* pevOther = pOther->pev;

	if (!FBitSet(pevOther->flags, FL_MONSTER))
		return; // touched by a non-monster.

	pevOther->origin.z += 1;

	if (FBitSet(pevOther->flags, FL_ONGROUND))
		pevOther->flags &= ~FL_ONGROUND; // clear the onground so physics don't bitch

	// toss the monster!
	Vector vecVelocity = pev->movedir * pev->speed;
	vecVelocity.z += m_flHeight;
	pOther->SetLocalVelocity(vecVelocity);
	SetNextThink(0);
}