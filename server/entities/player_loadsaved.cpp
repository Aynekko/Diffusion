#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "shake.h"
#include "game/gamerules.h"
#include "game/game.h"

class CRevertSaved : public CPointEntity
{
	DECLARE_CLASS(CRevertSaved, CPointEntity);
public:
	void	Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void	MessageThink(void);
	void	LoadThink(void);
	void	KeyValue(KeyValueData* pkvd);

	DECLARE_DATADESC();

	inline	float	Duration(void) { return pev->dmg_take; }
	inline	float	HoldTime(void) { return pev->dmg_save; }
	inline	float	MessageTime(void) { return m_messageTime; }
	inline	float	LoadTime(void) { return m_loadTime; }

	inline	void	SetDuration(float duration) { pev->dmg_take = duration; }
	inline	void	SetHoldTime(float hold) { pev->dmg_save = hold; }
	inline	void	SetMessageTime(float time) { m_messageTime = time; }
	inline	void	SetLoadTime(float time) { m_loadTime = time; }

private:
	float	m_messageTime;
	float	m_loadTime;
};

LINK_ENTITY_TO_CLASS(player_loadsaved, CRevertSaved);

BEGIN_DATADESC(CRevertSaved)
	DEFINE_FIELD(m_messageTime, FIELD_FLOAT),	// These are not actual times, but durations, so save as floats
	DEFINE_FIELD(m_loadTime, FIELD_FLOAT),
	DEFINE_FUNCTION(MessageThink),
	DEFINE_FUNCTION(LoadThink),
END_DATADESC()

void CRevertSaved::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "duration"))
	{
		SetDuration( Q_atof(pkvd->szValue));
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "holdtime"))
	{
		SetHoldTime( Q_atof(pkvd->szValue));
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "messagetime"))
	{
		SetMessageTime( Q_atof(pkvd->szValue));
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "loadtime"))
	{
		SetLoadTime( Q_atof(pkvd->szValue));
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue(pkvd);
}

void CRevertSaved::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	UTIL_ScreenFadeAll(pev->rendercolor, Duration(), HoldTime(), pev->renderamt, FFADE_OUT);
	SetThink(&CRevertSaved::MessageThink);
	SetNextThink(MessageTime());
}


void CRevertSaved::MessageThink(void)
{
	if( !FStringNull(pev->message)) // diffusion
		UTIL_ShowMessageAll(STRING(pev->message));

	float nextThink = LoadTime() - MessageTime();
	if (nextThink > 0)
	{
		SetThink(&CRevertSaved::LoadThink);
		SetNextThink(nextThink);
	}
	else
		LoadThink();
}


void CRevertSaved::LoadThink(void)
{
	if (!gpGlobals->deathmatch)
		SERVER_COMMAND("reload\n");
}