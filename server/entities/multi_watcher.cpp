#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "game/saverestore.h"
#include "entities/trains.h"			// trigger_camera has train functionality
#include "game/gamerules.h"
#include "talkmonster.h"
#include "weapons/weapons.h"
#include "triggers.h"

//=======================================================================
// 		   multi_watcher
//=======================================================================
#define SF_WATCHER_START_OFF	1

#define LOGIC_AND  		0 // fire if all objects active
#define LOGIC_OR   		1 // fire if any object active
#define LOGIC_NAND 		2 // fire if not all objects active
#define LOGIC_NOR  		3 // fire if all objects disable
#define LOGIC_XOR		4 // fire if only one (any) object active
#define LOGIC_XNOR		5 // fire if active any number objects, but < then all

class CMultiMaster : public CBaseDelay
{
	DECLARE_CLASS(CMultiMaster, CBaseDelay);
public:
	void Spawn(void);
	void Think(void);
	void KeyValue(KeyValueData* pkvd);
	int ObjectCaps(void) { return (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION); }
	virtual void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	virtual STATE GetState(CBaseEntity* pActivator) { return EvalLogic(pActivator) ? STATE_ON : STATE_OFF; };
	virtual STATE GetState(void) { return m_iState; };
	int GetLogicModeForString(const char* string);
	bool CheckState(STATE state, int targetnum);

	DECLARE_DATADESC();

	int	m_cTargets; // the total number of targets in this manager's fire list.
	int	m_iTargetName[MAX_MULTI_TARGETS]; // list of indexes into global string array
	STATE	m_iTargetState[MAX_MULTI_TARGETS]; // list of wishstate targets
	STATE	m_iSharedState;
	int	m_iLogicMode;

	BOOL	EvalLogic(CBaseEntity* pEntity);
	bool	globalstate;
};

LINK_ENTITY_TO_CLASS(multi_watcher, CMultiMaster);

BEGIN_DATADESC(CMultiMaster)
DEFINE_FIELD(m_cTargets, FIELD_INTEGER),
DEFINE_KEYFIELD(m_iLogicMode, FIELD_INTEGER, "logic"),
DEFINE_FIELD(m_iSharedState, FIELD_INTEGER),
DEFINE_AUTO_ARRAY(m_iTargetName, FIELD_STRING),
DEFINE_AUTO_ARRAY(m_iTargetState, FIELD_INTEGER),
END_DATADESC()

void CMultiMaster::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "logic"))
	{
		m_iLogicMode = GetLogicModeForString(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "state"))
	{
		m_iSharedState = GetStateForString(pkvd->szValue);
		pkvd->fHandled = TRUE;
		globalstate = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "offtarget"))
	{
		pev->netname = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "parent") || FStrEq(pkvd->szKeyName, "movewith"))
	{
		m_iParent = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else // add this field to the target list
	{
		// this assumes that additional fields are targetnames and their values are delay values.
		if (m_cTargets < MAX_MULTI_TARGETS)
		{
			char tmp[128];

			UTIL_StripToken(pkvd->szKeyName, tmp);
			m_iTargetName[m_cTargets] = ALLOC_STRING(tmp);
			m_iTargetState[m_cTargets] = GetStateForString(pkvd->szValue);
			m_cTargets++;
			pkvd->fHandled = TRUE;
		}
	}
}

int CMultiMaster::GetLogicModeForString(const char* string)
{
	if (!Q_stricmp(string, "AND"))
		return LOGIC_AND;
	else if (!Q_stricmp(string, "OR"))
		return LOGIC_OR;
	else if (!Q_stricmp(string, "NAND") || !Q_stricmp(string, "!AND"))
		return LOGIC_NAND;
	else if (!Q_stricmp(string, "NOR") || !Q_stricmp(string, "!OR"))
		return LOGIC_NOR;
	else if (!Q_stricmp(string, "XOR") || !Q_stricmp(string, "^OR"))
		return LOGIC_XOR;
	else if (!Q_stricmp(string, "XNOR") || !Q_stricmp(string, "^!OR"))
		return LOGIC_XNOR;
	else if (Q_isdigit(string))
		return Q_atoi(string);

	// assume error
	ALERT(at_error, "Unknown logic mode '%s' specified\n", string);
	return -1;
}

void CMultiMaster::Spawn(void)
{
	// use local states instead
	if (!globalstate)
	{
		m_iSharedState = (STATE)-1;
	}

	if (!FBitSet(pev->spawnflags, SF_WATCHER_START_OFF))
		SetNextThink(0.1);
}

bool CMultiMaster::CheckState(STATE state, int targetnum)
{
	// global state for all targets
	if (m_iSharedState != -1)
	{
		if (m_iSharedState == state)
			return TRUE;
		return FALSE;
	}

	if ((STATE)m_iTargetState[targetnum] == state)
		return TRUE;
	return FALSE;
}

void CMultiMaster::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!ShouldToggle(useType, (pev->nextthink != 0)))
		return;

	if (pev->nextthink == 0)
	{
		SetNextThink(0.01);
	}
	else
	{
		// disabled watcher is always off
		m_iState = STATE_OFF;
		DontThink();
	}
}

void CMultiMaster::Think(void)
{
	if (EvalLogic(NULL))
	{
		if (m_iState == STATE_OFF)
		{
			m_iState = STATE_ON;
			UTIL_FireTargets(pev->target, this, this, USE_ON);
		}
	}
	else
	{
		if (m_iState == STATE_ON)
		{
			m_iState = STATE_OFF;
			UTIL_FireTargets(pev->netname, this, this, USE_OFF);
		}
	}

	SetNextThink(0.01);
}

BOOL CMultiMaster::EvalLogic(CBaseEntity* pActivator)
{
	BOOL xorgot = FALSE;
	CBaseEntity* pEntity;

	for (int i = 0; i < m_cTargets; i++)
	{
		pEntity = UTIL_FindEntityByTargetname(NULL, STRING(m_iTargetName[i]), pActivator);
		if (!pEntity) continue;

		// handle the states for this logic mode
		if (CheckState(pEntity->GetState(), i))
		{
			switch (m_iLogicMode)
			{
			case LOGIC_OR:
				return TRUE;
			case LOGIC_NOR:
				return FALSE;
			case LOGIC_XOR:
				if (xorgot)
					return FALSE;
				xorgot = TRUE;
				break;
			case LOGIC_XNOR:
				if (xorgot)
					return TRUE;
				xorgot = TRUE;
				break;
			}
		}
		else // state is false
		{
			switch (m_iLogicMode)
			{
			case LOGIC_AND:
				return FALSE;
			case LOGIC_NAND:
				return TRUE;
			}
		}
	}

	// handle the default cases for each logic mode
	switch (m_iLogicMode)
	{
	case LOGIC_AND:
	case LOGIC_NOR:
		return TRUE;
	case LOGIC_XOR:
		return xorgot;
	case LOGIC_XNOR:
		return !xorgot;
	default:
		return FALSE;
	}
}