#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "triggers.h"

//=====================================
//
// trigger_cdaudio - starts/stops cd audio tracks
//
class CTriggerCDAudio : public CBaseTrigger
{
	DECLARE_CLASS(CTriggerCDAudio, CBaseTrigger);
public:
	void Spawn(void);

	virtual void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void PlayTrack(void);
	void Touch(CBaseEntity* pOther);
};

LINK_ENTITY_TO_CLASS(trigger_cdaudio, CTriggerCDAudio);

//
// Changes tracks or stops CD when player touches
//
// !!!HACK - overloaded HEALTH to avoid adding new field
void CTriggerCDAudio::Touch(CBaseEntity* pOther)
{
	if (!pOther->IsPlayer())
		return; // only clients may trigger these events

	PlayTrack();
}

void CTriggerCDAudio::Spawn(void)
{
	InitTrigger();
}

void CTriggerCDAudio::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	PlayTrack();
}

void PlayCDTrack(int iTrack)
{
	edict_t* pClient;

	// manually find the single player. 
	pClient = INDEXENT(1);

	// Can't play if the client is not connected!
	if (!pClient)
		return;

	if (iTrack < -1 || iTrack > 30)
	{
		ALERT(at_console, "TriggerCDAudio - Track %d out of range\n");
		return;
	}

	if (iTrack == -1)
		CLIENT_COMMAND(pClient, "cd pause\n");
	else
	{
		char string[64];

		sprintf(string, "cd play %3d\n", iTrack);
		CLIENT_COMMAND(pClient, string);
	}
}


// only plays for ONE client, so only use in single play!
void CTriggerCDAudio::PlayTrack(void)
{
	PlayCDTrack((int)pev->health);

	SetTouch(NULL);
	UTIL_Remove(this);
}


// This plays a CD track when fired or when the player enters it's radius
class CTargetCDAudio : public CPointEntity
{
	DECLARE_CLASS(CTargetCDAudio, CPointEntity);
public:
	void		Spawn(void);
	void		KeyValue(KeyValueData* pkvd);

	virtual void	Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void		Think(void);
	void		Play(void);
};

LINK_ENTITY_TO_CLASS(target_cdaudio, CTargetCDAudio);

void CTargetCDAudio::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "radius"))
	{
		pev->scale = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue(pkvd);
}

void CTargetCDAudio::Spawn(void)
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;

	if (pev->scale > 0)
		SetNextThink(1.0);
}

void CTargetCDAudio::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	Play();
}

// only plays for ONE client, so only use in single play!
void CTargetCDAudio::Think(void)
{
	CBaseEntity* pClient;

	// manually find the single player. 
	pClient = UTIL_PlayerByIndex(1);

	// Can't play if the client is not connected!
	if (!pClient)
		return;

	SetNextThink(0.5);

	if ((pClient->GetAbsOrigin() - GetAbsOrigin()).Length() <= pev->scale)
		Play();

}

void CTargetCDAudio::Play(void)
{
	PlayCDTrack((int)pev->health);
	UTIL_Remove(this);
}