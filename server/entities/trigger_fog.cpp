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
	float NewFog[4];
	float OldFog[4];
	void SetNewFog(void);
	bool Instant;

	DECLARE_DATADESC();
};
LINK_ENTITY_TO_CLASS(trigger_fog, CTriggerFog);

BEGIN_DATADESC(CTriggerFog)
	DEFINE_KEYFIELD(NewFogString, FIELD_STRING, "newfog"),
	DEFINE_ARRAY(NewFog, FIELD_FLOAT, 4),
	DEFINE_ARRAY(OldFog, FIELD_FLOAT, 4),
	DEFINE_KEYFIELD( Instant, FIELD_BOOLEAN, "speedstate"),
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
		Instant = (Q_atoi( pkvd->szValue ) > 0);
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

	int tmp[4];
	UTIL_StringToIntArray(tmp, 4, STRING(NewFogString));
	NewFog[0] = tmp[0] / 255.f;
	NewFog[1] = tmp[1] / 255.f;
	NewFog[2] = tmp[2] / 255.f;
	NewFog[3] = tmp[3] / 255.f;
}

void CTriggerFog::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!pActivator || !pActivator->IsPlayer())
		pActivator = CBaseEntity::Instance(INDEXENT(1));

	CBasePlayer* pPlayer = (CBasePlayer*)pActivator;

	if (IsLockedByMaster(pActivator))
		return;

	// first, get the values of the current fog (stored in range 0 - 1)
	OldFog[0] = (float)((g_pWorld->pev->impulse & 0xFF000000) >> 24) / 255.f;
	OldFog[1] = (float)((g_pWorld->pev->impulse & 0xFF0000) >> 16) / 255.f;
	OldFog[2] = (float)((g_pWorld->pev->impulse & 0xFF00) >> 8) / 255.f;
	OldFog[3] = (float)((g_pWorld->pev->impulse & 0xFF) >> 0) / 255.f;

	if ((OldFog[0] == NewFog[0]) && (OldFog[1] == NewFog[1]) && (OldFog[2] == NewFog[2]) && (OldFog[3] == NewFog[3]))
	{
		//	ALERT(at_console, "The same fog is already set!\n");
		return;
	}

	// is there another entity messing with the fog right now? we need to stop them before proceeding
	CBaseEntity* pOther = NULL;
	while ((pOther = UTIL_FindEntityByClassname(pOther, "trigger_fog")) != NULL)
		pOther->DontThink();

	if( Instant )
	{
		// instantly update fog and save new values
		int fog[4];
		fog[0] = NewFog[0] * 255.f;
		fog[1] = NewFog[1] * 255.f;
		fog[2] = NewFog[2] * 255.f;
		fog[3] = NewFog[3] * 255.f;
		UPDATE_PACKED_FOG((fog[0] << 24) | (fog[1] << 16) | (fog[2] << 8) | fog[3]);
		g_pWorld->pev->impulse = (fog[0] << 24) | (fog[1] << 16) | (fog[2] << 8) | fog[3];
	}
	else
	{
		SetThink(&CTriggerFog::SetNewFog);
		SetNextThink(m_flWait);
	}
}

void CTriggerFog::SetNewFog(void)
{
	Vector TempFog = LerpRGB( Vector( OldFog[0], OldFog[1], OldFog[2] ), Vector( NewFog[0], NewFog[1], NewFog[2] ), gpGlobals->frametime );
	OldFog[0] = TempFog.x;
	OldFog[1] = TempFog.y;
	OldFog[2] = TempFog.z;
	OldFog[3] = UTIL_Approach( NewFog[3], OldFog[3], gpGlobals->frametime );
//	ALERT( at_console, "setting new fog %f %f %f %f\n", OldFog[0], OldFog[1], OldFog[2], OldFog[3] );

	bool finished = false;
	if( fabs( OldFog[0] - NewFog[0] ) < 0.01f && fabs( OldFog[1] - NewFog[1] ) < 0.01f && fabs( OldFog[2] - NewFog[2] ) < 0.01f && fabs( OldFog[3] - NewFog[3] ) < 0.01f )
	{
		OldFog[0] = NewFog[0];
		OldFog[1] = NewFog[1];
		OldFog[2] = NewFog[2];
		OldFog[3] = NewFog[3];
		finished = true;
	}

	// update the fog on the map
	int fog[4];
	fog[0] = OldFog[0] * 255.f;
	fog[1] = OldFog[1] * 255.f;
	fog[2] = OldFog[2] * 255.f;
	fog[3] = OldFog[3] * 255.f;
	UPDATE_PACKED_FOG((fog[0] << 24) | (fog[1] << 16) | (fog[2] << 8) | fog[3]);
	g_pWorld->pev->impulse = (fog[0] << 24) | (fog[1] << 16) | (fog[2] << 8) | fog[3];

	if( finished )
	{
	//	ALERT(at_console, "FINISHED NewFog\n");
		return;
	}

	SetThink( &CTriggerFog::SetNewFog );
	SetNextThink( 0 );
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