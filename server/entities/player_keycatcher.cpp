#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "game/gamerules.h"
#include "game/game.h"

// =================== PLAYER_KEYCATCHER ==============================================
static catchtable_t gPlayerButtonTable[] =
{
{ "attack", IN_ATTACK },
{ "jump", IN_JUMP },
{ "duck", IN_DUCK },
{ "forward", IN_FORWARD },
{ "back", IN_BACK },
{ "use", IN_USE },
{ "left", IN_LEFT },
{ "right", IN_RIGHT },
{ "moveleft", IN_MOVELEFT },
{ "moveright", IN_MOVERIGHT },
{ "attack2", IN_ATTACK2 },
{ "run", IN_RUN },
{ "reload", IN_RELOAD },
{ "alt1", IN_ALT1 },
{ "score", IN_SCORE },
};

BEGIN_DATADESC(CPlayerKeyCatcher)
	DEFINE_KEYFIELD(m_iszKeyPressed, FIELD_STRING, "m_iszKeyPressed"),
	DEFINE_KEYFIELD(m_iszKeyReleased, FIELD_STRING, "m_iszKeyReleased"),
	DEFINE_KEYFIELD(m_iszKeyHoldDown, FIELD_STRING, "m_iszKeyHoldDown"),
END_DATADESC()

LINK_ENTITY_TO_CLASS(player_keycatcher, CPlayerKeyCatcher);

void CPlayerKeyCatcher::Spawn(void)
{
	m_iState = STATE_OFF;
}

int CPlayerKeyCatcher::Restore(CRestore& restore)
{
	int status = BaseClass::Restore(restore);

	m_iState = STATE_OFF;

	return status;
}

void CPlayerKeyCatcher::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "m_iszKeyPressed"))
	{
		m_iszKeyPressed = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszKeyReleased"))
	{
		m_iszKeyReleased = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszKeyHoldDown"))
	{
		m_iszKeyHoldDown = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszKeyToCatch"))
	{
		int i;
		for (i = 0; i < SIZEOFARRAY(gPlayerButtonTable); i++)
		{
			if (!Q_stricmp(pkvd->szValue, gPlayerButtonTable[i].buttonName))
			{
				pev->button = gPlayerButtonTable[i].buttonCode;
				break;
			}
		}

		if (i == SIZEOFARRAY(gPlayerButtonTable))
			ALERT(at_error, "%s has invalid keyname %s\n", GetClassname(), pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else CBaseDelay::KeyValue(pkvd);
}

void CPlayerKeyCatcher::CatchButton(CBaseEntity* pActivator, int buttons, int pressed, int released)
{
	if (!(pressed & pev->button) && !(released & pev->button) && !(buttons & pev->button))
		return;	// ignore other buttons

	if (IsLockedByMaster(pActivator))
		return;	// temporare blocked

	if (pressed & pev->button)
	{
		m_iState = STATE_ON;
		UTIL_FireTargets(m_iszKeyPressed, pActivator, this, USE_ON);
	}

	if (buttons & pev->button)
	{
		m_iState = STATE_ON;
		UTIL_FireTargets(m_iszKeyHoldDown, pActivator, this, USE_TOGGLE);
	}

	if (released & pev->button)
	{
		m_iState = STATE_OFF;
		UTIL_FireTargets(m_iszKeyReleased, pActivator, this, USE_OFF);
	}
}