#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "game/gamerules.h"
#include "triggers.h"

//=====================================================
// trigger_command: activate a console command
//=====================================================
class CTriggerCommand : public CBaseEntity
{
	DECLARE_CLASS(CTriggerCommand, CBaseEntity);
public:
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	virtual int ObjectCaps(void) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};
LINK_ENTITY_TO_CLASS(trigger_command, CTriggerCommand);

void CTriggerCommand::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	char szCommand[256];

	if (pev->netname)
	{
		Q_snprintf(szCommand, sizeof(szCommand), "%s\n", STRING(pev->netname));
		SERVER_COMMAND(szCommand);
	}
}