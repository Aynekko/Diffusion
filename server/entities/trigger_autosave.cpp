#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "game/gamerules.h"
#include "game/game.h"
#include "triggers.h"

class CTriggerSave : public CBaseTrigger
{
	DECLARE_CLASS(CTriggerSave, CBaseTrigger);
public:
	void Spawn(void);
	void SaveTouch(CBaseEntity* pOther);

	DECLARE_DATADESC();
};
LINK_ENTITY_TO_CLASS(trigger_autosave, CTriggerSave);

BEGIN_DATADESC(CTriggerSave)
	DEFINE_FUNCTION(SaveTouch),
END_DATADESC()

void CTriggerSave::Spawn(void)
{
	if (g_pGameRules->IsDeathmatch())
	{
		REMOVE_ENTITY(ENT(pev));
		return;
	}

	InitTrigger();
	SetTouch(&CTriggerSave::SaveTouch);
}

void CTriggerSave::SaveTouch(CBaseEntity* pOther)
{
	if (!UTIL_IsMasterTriggered(m_sMaster, pOther))
		return;

	// Only save on clients
	if (!pOther->IsPlayer())
		return;

	if( sv_ignore_triggers.value > 0 )
		return;

	SetTouch(NULL);
	UTIL_Remove(this);
	SERVER_COMMAND("autosave\n");
	UTIL_ShowMessage( "GAMESAVED", pOther ); // diffusion - show save icon
}