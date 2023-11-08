#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "game/gamerules.h"
#include "game/game.h"

// =====================TUTOR SPRITE=========================================

class CHudSpriteTutor : public CBaseEntity
{
	DECLARE_CLASS(CHudSpriteTutor, CBaseEntity);
public:
	void Spawn(void);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
};

LINK_ENTITY_TO_CLASS(hud_sprite_tutor, CHudSpriteTutor);

void CHudSpriteTutor::Spawn(void)
{
	if (g_pGameRules->IsMultiplayer())
	{
		ALERT(at_aiconsole, "Tutorials are not allowed in multiplayer. Removed.\n");
		UTIL_Remove(this);
		return;
	}
}

void CHudSpriteTutor::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!pActivator || !pActivator->IsPlayer())
		pActivator = UTIL_PlayerByIndex(1);

	MESSAGE_BEGIN(MSG_ONE, gmsgStatusIconTutor, NULL, pActivator->pev);
		WRITE_STRING(STRING(pev->model));
	MESSAGE_END();
}