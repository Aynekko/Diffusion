#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "game/gamerules.h"
#include "game/game.h"

class CHudSprite : public CBaseEntity
{
	DECLARE_CLASS(CHudSprite, CBaseEntity);
public:
	void Spawn(void);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	STATE GetState(void) { return FBitSet(pev->spawnflags, SF_HUDSPR_ACTIVE) ? STATE_ON : STATE_OFF; }
	void StartMessage(CBasePlayer* pPlayer);
	void KeyValue(KeyValueData* pkvd);
	int IsAdditive;

	DECLARE_DATADESC();
};

BEGIN_DATADESC(CHudSprite)
	DEFINE_KEYFIELD(IsAdditive, FIELD_INTEGER, "isadditive"),
END_DATADESC();


// HUD_SPRITE
// diffusion: + additive/not additive setting

LINK_ENTITY_TO_CLASS(hud_sprite, CHudSprite);

void CHudSprite::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "isadditive"))
	{
		IsAdditive = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue(pkvd);
}

void CHudSprite::Spawn(void)
{
	if (g_pGameRules->IsMultiplayer())
	{
		UTIL_Remove( this ); // only in single
		return;
	}

	if (FStringNull(pev->targetname))
		SetBits(pev->spawnflags, SF_HUDSPR_ACTIVE);
}

void CHudSprite::StartMessage(CBasePlayer* pPlayer)
{
	if (GetState() == STATE_OFF) return; // inactive

	MESSAGE_BEGIN(MSG_ONE, gmsgStatusIcon, NULL, pPlayer->pev);
		WRITE_BYTE(GetState());
		WRITE_STRING(STRING(pev->model));
		WRITE_BYTE(pev->rendercolor.x);
		WRITE_BYTE(pev->rendercolor.y);
		WRITE_BYTE(pev->rendercolor.z);
		WRITE_BYTE(IsAdditive);
	MESSAGE_END();
}

void CHudSprite::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!pActivator || !pActivator->IsPlayer())
	{
		pActivator = UTIL_PlayerByIndex(1);
	}

	if (ShouldToggle(useType))
	{
		if (FBitSet(pev->spawnflags, SF_HUDSPR_ACTIVE))
			ClearBits(pev->spawnflags, SF_HUDSPR_ACTIVE);
		else SetBits(pev->spawnflags, SF_HUDSPR_ACTIVE);
	}

	MESSAGE_BEGIN(MSG_ONE, gmsgStatusIcon, NULL, pActivator->pev);
		WRITE_BYTE(GetState());
		WRITE_STRING(STRING(pev->model));
		WRITE_BYTE(pev->rendercolor.x);
		WRITE_BYTE(pev->rendercolor.y);
		WRITE_BYTE(pev->rendercolor.z);
		WRITE_BYTE(IsAdditive);
	MESSAGE_END();
}