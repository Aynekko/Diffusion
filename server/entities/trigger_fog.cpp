#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "game/gamerules.h"
#include "triggers.h"

// trigger_fog
// func_fog

//==========================================================================
//diffusion - change fog on the map (point entity) - dynamic
//==========================================================================

class CTriggerFog : public CBaseDelay
{
	DECLARE_CLASS(CTriggerFog, CBaseDelay);
public:
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void KeyValue(KeyValueData* pkvd);
	void Spawn(void);

	string_t NewFogString;
	int NewFog[4];
	int OldFog[4];
	void SetNewFog(void);
	int SpeedState;

	DECLARE_DATADESC();
};
LINK_ENTITY_TO_CLASS(trigger_fog, CTriggerFog);

BEGIN_DATADESC(CTriggerFog)
	DEFINE_KEYFIELD(NewFogString, FIELD_STRING, "newfog"),
	DEFINE_ARRAY(NewFog, FIELD_INTEGER, 4),
	DEFINE_ARRAY(OldFog, FIELD_INTEGER, 4),
	DEFINE_KEYFIELD(SpeedState, FIELD_INTEGER, "speedstate"),
	DEFINE_FUNCTION(SetNewFog),
END_DATADESC();

void CTriggerFog::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "newfog"))
	{
		NewFogString = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "speedstate"))
	{
		SpeedState = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue(pkvd);
}

void CTriggerFog::Spawn(void)
{
	if (!NewFogString) // mapper forgot to set new fog, delete entity
	{
		ALERT(at_error, "trigger_fog \"%s\" doesn't have New Fog setting!\n", STRING(pev->targetname));
		UTIL_Remove(this);
		return;
	}

	if( !SpeedState || (SpeedState <= 0) )
		m_flWait = 0;
	else if( SpeedState == 1 )
		m_flWait = 0.01;
	else if( SpeedState == 2 )
		m_flWait = 0.02;
	else if( SpeedState == 3 )
		m_flWait = 0.05;
	else if( SpeedState == 4 )
		m_flWait = 0.1;
	else if( SpeedState != 5 )
		m_flWait = SpeedState * 0.01f;

	UTIL_StringToIntArray(NewFog, 4, STRING(NewFogString));
}

void CTriggerFog::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!pActivator || !pActivator->IsPlayer())
		pActivator = CBaseEntity::Instance(INDEXENT(1));

	CBasePlayer* pPlayer = (CBasePlayer*)pActivator;

	if (IsLockedByMaster(pActivator))
		return;

	// first, get the values of the current fog
	OldFog[0] = (g_pWorld->pev->impulse & 0xFF000000) >> 24;
	OldFog[1] = (g_pWorld->pev->impulse & 0xFF0000) >> 16;
	OldFog[2] = (g_pWorld->pev->impulse & 0xFF00) >> 8;
	OldFog[3] = (g_pWorld->pev->impulse & 0xFF) >> 0;

	if ((OldFog[0] == NewFog[0]) && (OldFog[1] == NewFog[1]) && (OldFog[2] == NewFog[2]) && (OldFog[3] == NewFog[3]))
	{
		//	ALERT(at_console, "The same fog is already set!\n");
		return;
	}

	// is there another entity messing with the fog right now? we need to stop them before proceeding
	CBaseEntity* pOther = NULL;
	while ((pOther = UTIL_FindEntityByClassname(pOther, "trigger_fog")) != NULL)
		pOther->DontThink();

	if (SpeedState == 5)
	{
		// instantly update fog and save new values
		UPDATE_PACKED_FOG((NewFog[0] << 24) | (NewFog[1] << 16) | (NewFog[2] << 8) | NewFog[3]);
		g_pWorld->pev->impulse = (NewFog[0] << 24) | (NewFog[1] << 16) | (NewFog[2] << 8) | NewFog[3];
	}
	else
	{
		SetThink(&CTriggerFog::SetNewFog);
		SetNextThink(m_flWait);
	}
}

void CTriggerFog::SetNewFog(void)
{
	for (int i = 0; i < 4; i++)
	{
		OldFog[i] = UTIL_Approach( NewFog[i], OldFog[i], 1.0f );
	//	ALERT(at_console, "setting new fog %3d %3d %3d %3d\n", OldFog[0], OldFog[1], OldFog[2], OldFog[3]);
	}

	// update the fog on the map
	UPDATE_PACKED_FOG((OldFog[0] << 24) | (OldFog[1] << 16) | (OldFog[2] << 8) | OldFog[3]);
	g_pWorld->pev->impulse = (OldFog[0] << 24) | (OldFog[1] << 16) | (OldFog[2] << 8) | OldFog[3];

	if ((OldFog[0] == NewFog[0]) && (OldFog[1] == NewFog[1]) && (OldFog[2] == NewFog[2]) && (OldFog[3] == NewFog[3]))
	{
	//	ALERT(at_console, "FINISHED NewFog\n");
		return;
	}

	SetThink(&CTriggerFog::SetNewFog);
	SetNextThink(m_flWait);
}


//==========================================================================
//diffusion - change fog on the map (brush entity) - instanteneous only
//==========================================================================
class CTriggerFogToggle : public CTriggerInOut
{
	DECLARE_CLASS(CTriggerFogToggle, CTriggerInOut);
public:
	void KeyValue(KeyValueData* pkvd);
	virtual void FireOnEntry(CBaseEntity* pOther);
	virtual void FireOnLeaving(CBaseEntity* pOther);
	int EntryFog[4];
	int ExitFog[4];
	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(func_fog, CTriggerFogToggle);

BEGIN_DATADESC(CTriggerFogToggle)
DEFINE_ARRAY(EntryFog, FIELD_INTEGER, 4),
DEFINE_ARRAY(ExitFog, FIELD_INTEGER, 4),
END_DATADESC();

void CTriggerFogToggle::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "entryfog"))
	{
		UTIL_StringToIntArray(EntryFog, 4, pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "exitfog"))
	{
		UTIL_StringToIntArray(ExitFog, 4, pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue(pkvd);
}

void CTriggerFogToggle::FireOnEntry(CBaseEntity* pOther)
{
	if (!pOther->IsPlayer()) return;
	UPDATE_PACKED_FOG((EntryFog[0] << 24) | (EntryFog[1] << 16) | (EntryFog[2] << 8) | EntryFog[3]);
	g_pWorld->pev->impulse = (EntryFog[0] << 24) | (EntryFog[1] << 16) | (EntryFog[2] << 8) | EntryFog[3];
}

void CTriggerFogToggle::FireOnLeaving(CBaseEntity* pOther)
{
	if (!pOther->IsPlayer()) return;
	UPDATE_PACKED_FOG((ExitFog[0] << 24) | (ExitFog[1] << 16) | (ExitFog[2] << 8) | ExitFog[3]);
	g_pWorld->pev->impulse = (ExitFog[0] << 24) | (ExitFog[1] << 16) | (ExitFog[2] << 8) | ExitFog[3];
}