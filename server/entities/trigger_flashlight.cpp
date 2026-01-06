#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "game/gamerules.h"
#include "triggers.h"

//==========================================================================
// diffusion - enable flashlight for monster if he's in the trigger.
// disable, if not.
//==========================================================================

class CTriggerFlashlight : public CTriggerInOut
{
	DECLARE_CLASS(CTriggerFlashlight, CTriggerInOut);
public:
	void Spawn( void );
	void FireOnEntry(CBaseEntity* pOther);
	void FireOnLeaving(CBaseEntity* pOther);
};

LINK_ENTITY_TO_CLASS(trigger_flashlight, CTriggerFlashlight);

void CTriggerFlashlight::Spawn(void)
{
	BaseClass::Spawn();
	pev->spawnflags |= SF_TRIGGER_ALLOWMONSTERS | SF_TRIGGER_NOCLIENTS;
}

void CTriggerFlashlight::FireOnEntry(CBaseEntity* pOther)
{
	if (!pOther->IsMonster())
		return;
	if (IsLockedByMaster())
		return;
	
	CBaseMonster* pMonster = (CBaseMonster*)pOther;

	if( pMonster->bFlashlightCap && !(pMonster->pev->effects & EF_MONSTERFLASHLIGHT) )
		pMonster->pev->effects |= EF_MONSTERFLASHLIGHT;
}

void CTriggerFlashlight::FireOnLeaving(CBaseEntity* pOther)
{
	if (!pOther->IsMonster())
		return;
	if (IsLockedByMaster())
		return;

	CBaseMonster* pMonster = (CBaseMonster*)pOther;

	if( pMonster->bFlashlightCap && (pMonster->pev->effects & EF_MONSTERFLASHLIGHT) )
		pMonster->pev->effects &= ~EF_MONSTERFLASHLIGHT;
}