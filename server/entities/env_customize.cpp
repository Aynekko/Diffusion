#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "game/gamerules.h"
#include "talkmonster.h"

//***********************************************************
//
// EnvCustomize
//
// Changes various properties of an entity (some properties only apply to monsters.)
//

#define SF_CUSTOM_AFFECTDEAD	1
#define SF_CUSTOM_ONCE			2
#define SF_CUSTOM_DEBUG			4

#define CUSTOM_FLAG_NOCHANGE	0
#define CUSTOM_FLAG_ON			1
#define CUSTOM_FLAG_OFF			2
#define CUSTOM_FLAG_TOGGLE		3
#define CUSTOM_FLAG_USETYPE		4
#define CUSTOM_FLAG_INVUSETYPE	5

class CEnvCustomize : public CBaseEntity
{
	DECLARE_CLASS(CEnvCustomize, CBaseEntity);
public:
	void Spawn(void);
	void Precache(void);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);

	void Affect(CBaseEntity* pTarget, USE_TYPE useType);
	int GetActionFor(int iField, int iActive, USE_TYPE useType, char* szDebug);
	void SetBoneController(float fController, int cnum, CBaseEntity* pTarget);

	virtual int ObjectCaps(void) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void KeyValue(KeyValueData* pkvd);

	DECLARE_DATADESC();

	int	m_iszModel;
	int	m_iClass;
	int	m_iPlayerReact;
	int	m_iPrisoner;
	int	m_iMonsterClip;
	int m_iGag; // diffusion
	int	m_iVisible;
	int	m_iSolid;
	int	m_iProvoked;
	int	m_voicePitch;
	int	m_iBloodColor;
	float	m_fFramerate;
	float	m_fController0;
	float	m_fController1;
	float	m_fController2;
	float	m_fController3;
	int	m_iReflection;	// reflection style
	int newhealth; // diffusion - change monster health
	bool m_iEnemy; // diffusion - make them aware about the player and hunt him
	int m_HealthBar;
	int m_HealthBarType;
	bool m_bWakeUp; // start monster AI
	int m_iLightLerp; // enable or disable light lerp on model
};

LINK_ENTITY_TO_CLASS(env_customize, CEnvCustomize);

BEGIN_DATADESC(CEnvCustomize)
	DEFINE_KEYFIELD(m_iszModel, FIELD_STRING, "m_iszModel"),
	DEFINE_KEYFIELD(m_iClass, FIELD_INTEGER, "m_iClass"),
	DEFINE_KEYFIELD(m_iPlayerReact, FIELD_INTEGER, "m_iPlayerReact"),
	DEFINE_KEYFIELD(m_iPrisoner, FIELD_INTEGER, "m_iPrisoner"),
	DEFINE_KEYFIELD(m_iMonsterClip, FIELD_INTEGER, "m_iMonsterClip"),
	DEFINE_KEYFIELD(m_iGag, FIELD_INTEGER, "m_iGag"),
	DEFINE_KEYFIELD(m_iVisible, FIELD_INTEGER, "m_iVisible"),
	DEFINE_KEYFIELD(m_iSolid, FIELD_INTEGER, "m_iSolid"),
	DEFINE_KEYFIELD(m_iProvoked, FIELD_INTEGER, "m_iProvoked"),
	DEFINE_KEYFIELD(m_voicePitch, FIELD_INTEGER, "m_voicePitch"),
	DEFINE_KEYFIELD(m_iBloodColor, FIELD_INTEGER, "m_iBloodColor"),
	DEFINE_KEYFIELD(m_fFramerate, FIELD_FLOAT, "m_fFramerate"),
	DEFINE_KEYFIELD(m_fController0, FIELD_FLOAT, "m_fController0"),
	DEFINE_KEYFIELD(m_fController1, FIELD_FLOAT, "m_fController1"),
	DEFINE_KEYFIELD(m_fController2, FIELD_FLOAT, "m_fController2"),
	DEFINE_KEYFIELD(m_fController3, FIELD_FLOAT, "m_fController3"),
	DEFINE_KEYFIELD(m_iReflection, FIELD_INTEGER, "m_iReflection"),
	DEFINE_KEYFIELD(newhealth, FIELD_INTEGER, "newhealth"),
	DEFINE_KEYFIELD(m_iEnemy, FIELD_BOOLEAN, "m_iEnemy"),
	DEFINE_KEYFIELD(m_HealthBar, FIELD_INTEGER, "m_HealthBar" ),
	DEFINE_KEYFIELD( m_HealthBarType, FIELD_INTEGER, "m_HealthBarType" ),
	DEFINE_KEYFIELD( m_bWakeUp, FIELD_BOOLEAN, "m_bWakeUp" ),
	DEFINE_KEYFIELD( m_iLightLerp, FIELD_INTEGER, "m_iLightLerp" ),
END_DATADESC()

void CEnvCustomize::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "m_iVisible"))
	{
		m_iVisible = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iSolid"))
	{
		m_iSolid = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszModel"))
	{
		m_iszModel = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_voicePitch"))
	{
		m_voicePitch = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iPrisoner"))
	{
		m_iPrisoner = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iMonsterClip"))
	{
		m_iMonsterClip = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iGag"))
	{
		m_iGag = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iClass"))
	{
		m_iClass = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iPlayerReact"))
	{
		m_iPlayerReact = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iProvoked"))
	{
		m_iProvoked = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_bloodColor") || FStrEq(pkvd->szKeyName, "m_iBloodColor"))
	{
		m_iBloodColor = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_fFramerate"))
	{
		m_fFramerate = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_fController0"))
	{
		m_fController0 = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_fController1"))
	{
		m_fController1 = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_fController2"))
	{
		m_fController2 = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_fController3"))
	{
		m_fController3 = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iReflection"))
	{
		m_iReflection = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "newhealth"))
	{
		newhealth = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iEnemy"))
	{
		m_iEnemy = (Q_atoi( pkvd->szValue ) > 0);
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "newhealth" ) )
	{
		newhealth = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_HealthBar" ) )
	{
		m_HealthBar = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_HealthBarType" ) )
	{
		m_HealthBarType = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_bWakeUp" ) )
	{
		m_bWakeUp = (Q_atoi( pkvd->szValue ) > 0);
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_iLightLerp" ) )
	{
		m_iLightLerp = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue(pkvd);
}

void CEnvCustomize::Spawn(void)
{
	Precache();

	if (!pev->targetname)
	{
		SetThink(&CBaseEntity::SUB_CallUseToggle);
		SetNextThink(0.1);
	}
}

void CEnvCustomize::Precache(void)
{
	if (m_iszModel)
		PRECACHE_MODEL((char*)STRING(m_iszModel));
}

void CEnvCustomize::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (FStringNull(pev->target))
	{
		if (pActivator)
			Affect(pActivator, useType);
		else if (pev->spawnflags & SF_CUSTOM_DEBUG)
			ALERT(at_console, "DEBUG: env_customize \"%s\" was fired without a locus!\n", STRING(pev->targetname));
	}
	else
	{
		BOOL fail = TRUE;
		CBaseEntity* pTarget = UTIL_FindEntityByTargetname(NULL, STRING(pev->target), pActivator);
		while (pTarget)
		{
			Affect(pTarget, useType);
			fail = FALSE;
			pTarget = UTIL_FindEntityByTargetname(pTarget, STRING(pev->target), pActivator);
		}
		pTarget = UTIL_FindEntityByClassname(NULL, STRING(pev->target));
		while (pTarget)
		{
			Affect(pTarget, useType);
			fail = FALSE;
			pTarget = UTIL_FindEntityByClassname(pTarget, STRING(pev->target));
		}
		if (fail && pev->spawnflags & SF_CUSTOM_DEBUG)
			ALERT(at_console, "DEBUG: env_customize \"%s\" does nothing; can't find any entity with name or class \"%s\".\n", STRING(pev->target));
	}

	if (pev->spawnflags & SF_CUSTOM_ONCE)
	{
		if (pev->spawnflags & SF_CUSTOM_DEBUG)
			ALERT(at_console, "DEBUG: env_customize \"%s\" removes itself.\n", STRING(pev->targetname));
		UTIL_Remove(this);
	}
}

void CEnvCustomize::Affect(CBaseEntity* pTarget, USE_TYPE useType)
{
	CBaseMonster* pMonster = pTarget->MyMonsterPointer();
	if (!FBitSet(pev->spawnflags, SF_CUSTOM_AFFECTDEAD) && pMonster && pMonster->m_MonsterState == MONSTERSTATE_DEAD)
	{
		if (pev->spawnflags & SF_CUSTOM_DEBUG)
			ALERT(at_console, "DEBUG: env_customize %s does nothing; can't apply to a corpse.\n", STRING(pev->targetname));
		return;
	}

	if (pev->spawnflags & SF_CUSTOM_DEBUG)
		ALERT(at_console, "DEBUG: env_customize \"%s\" affects %s \"%s\": [", STRING(pev->targetname), STRING(pTarget->pev->classname), STRING(pTarget->pev->targetname));

	if (m_iszModel)
	{
		Vector vecMins, vecMaxs;
		vecMins = pTarget->pev->mins;
		vecMaxs = pTarget->pev->maxs;
		SET_MODEL(pTarget->edict(), STRING(m_iszModel));
		UTIL_SetSize(pTarget->pev, vecMins, vecMaxs);
		if (pev->spawnflags & SF_CUSTOM_DEBUG)
			ALERT(at_console, " model=%s", STRING(m_iszModel));
	}

	SetBoneController(m_fController0, 0, pTarget);
	SetBoneController(m_fController1, 1, pTarget);
	SetBoneController(m_fController2, 2, pTarget);
	SetBoneController(m_fController3, 3, pTarget);

	if (m_fFramerate != -1)
	{
		//FIXME: check for env_model, stop it from changing its own framerate
		pTarget->pev->framerate = m_fFramerate;
		if (pev->spawnflags & SF_CUSTOM_DEBUG)
			ALERT(at_console, " framerate=%f", m_fFramerate);
	}
	if (pev->body != -1)
	{
		pTarget->pev->body = pev->body;
		if (pev->spawnflags & SF_CUSTOM_DEBUG)
			ALERT(at_console, " body = %d", pev->body);
	}
	if (pev->skin != -1)
	{
		if (pev->skin == -2)
		{
			if (pTarget->pev->skin)
			{
				pTarget->pev->skin = 0;
				if (pev->spawnflags & SF_CUSTOM_DEBUG)
					ALERT(at_console, " skin=0");
			}
			else
			{
				pTarget->pev->skin = 1;
				if (pev->spawnflags & SF_CUSTOM_DEBUG)
					ALERT(at_console, " skin=1");
			}
		}
		else if (pev->skin == -99) // special option to set CONTENTS_EMPTY
		{
			pTarget->pev->skin = -1;
			if (pev->spawnflags & SF_CUSTOM_DEBUG)
				ALERT(at_console, " skin=-1");
		}
		else
		{
			pTarget->pev->skin = pev->skin;
			if (pev->spawnflags & SF_CUSTOM_DEBUG)
				ALERT(at_console, " skin=%d", pTarget->pev->skin);
		}
	}

	if (m_iReflection != -1)
	{
		switch (m_iReflection)
		{
		case 0:
			pTarget->pev->effects &= ~EF_NOREFLECT;
			pTarget->pev->effects &= ~EF_REFLECTONLY;
			break;
		case 1:
			pTarget->pev->effects |= EF_NOREFLECT;
			pTarget->pev->effects &= ~EF_REFLECTONLY;
			break;
		case 2:
			pTarget->pev->effects |= EF_REFLECTONLY;
			pTarget->pev->effects &= ~EF_NOREFLECT;
			break;
		}
	}

	switch (GetActionFor(m_iVisible, !(pTarget->pev->effects & EF_NODRAW), useType, "visible"))
	{
	case CUSTOM_FLAG_ON: pTarget->pev->effects &= ~EF_NODRAW; break;
	case CUSTOM_FLAG_OFF: pTarget->pev->effects |= EF_NODRAW; break;
	}

	switch (GetActionFor(m_iSolid, pTarget->pev->solid != SOLID_NOT, useType, "solid"))
	{
	case CUSTOM_FLAG_ON: pTarget->RestoreSolid(); break;
	case CUSTOM_FLAG_OFF: pTarget->MakeNonSolid(); break;
	}

	if (!pMonster)
	{
		if (pev->spawnflags & SF_CUSTOM_DEBUG)
			ALERT(at_console, " ]\n");
		return;
	}

	if (m_iBloodColor != 0)
	{
		pMonster->m_bloodColor = m_iBloodColor;
		if (pev->spawnflags & SF_CUSTOM_DEBUG)
			ALERT(at_console, " bloodcolor=%d", m_iBloodColor);
	}
	if( m_HealthBar != 0 )
	{
		if( m_HealthBar == 1 )
			pMonster->EnableHealthBar = false;
		else if( m_HealthBar == 2 )
			pMonster->EnableHealthBar = true;

		if( pev->spawnflags & SF_CUSTOM_DEBUG )
			ALERT( at_console, " healthbar=%d", m_HealthBar );
	}
	if( m_HealthBarType != 0 )
	{
		pMonster->HealthBarType = m_HealthBarType - 1;

		if( pev->spawnflags & SF_CUSTOM_DEBUG )
			ALERT( at_console, " HealthBarType=%d", m_HealthBarType );
	}
	if (m_voicePitch > 0)
	{
		if (FClassnameIs(pTarget->pev, "monster_barney") || FClassnameIs(pTarget->pev, "monster_scientist") || FClassnameIs(pTarget->pev, "monster_sitting_scientist"))
		{
			((CTalkMonster*)pTarget)->m_voicePitch = m_voicePitch;
			if (pev->spawnflags & SF_CUSTOM_DEBUG)
				ALERT(at_console, " voicePitch(talk)=%d", m_voicePitch);
		}
		else if (pev->spawnflags & SF_CUSTOM_DEBUG)
			ALERT(at_console, " voicePitch=unchanged");
	}

	if (newhealth > 0)
	{
		pMonster->pev->health = newhealth;
		if (pev->spawnflags & SF_CUSTOM_DEBUG)
			ALERT(at_console, "new health = %d", newhealth);
	}

	if( m_iEnemy )
	{
		CBaseEntity* pPlayer = CBaseEntity::Instance(INDEXENT(1));
		if (pPlayer)
		{
			// the player is now the known enemy.
			pMonster->SetEnemy( pPlayer );
			pMonster->SetConditions( bits_COND_NEW_ENEMY );
			pMonster->m_IdealMonsterState = MONSTERSTATE_COMBAT;
		}
	}

	if( m_bWakeUp )
	{
		if( pMonster->HasSpawnFlags( SF_MONSTER_ASLEEP ) )
			pMonster->pev->spawnflags &= ~SF_MONSTER_ASLEEP;
	}

	if (m_iClass != 0)
	{
		pMonster->m_iClass = m_iClass;
		if (pev->spawnflags & SF_CUSTOM_DEBUG)
			ALERT(at_console, " class=%d", m_iClass);
		if (pMonster->m_hEnemy)
		{
			pMonster->m_hEnemy = NULL;
			// make 'em stop attacking... might be better to use a different signal?
			pMonster->SetConditions(bits_COND_NEW_ENEMY);
		}
	}
	if (m_iPlayerReact != -1)
	{
		pMonster->m_iPlayerReact = m_iPlayerReact;
		if (pev->spawnflags & SF_CUSTOM_DEBUG)
			ALERT(at_console, " playerreact=%d", m_iPlayerReact);
	}

	if( m_iLightLerp > 0 )
	{
		if( m_iLightLerp == 1 )
			pMonster->pev->effects |= EF_NOLIGHTLERP; // disable lerp
		else if( m_iLightLerp == 2 )
			pMonster->pev->effects &= ~EF_NOLIGHTLERP; // enable lerp
	}

	switch (GetActionFor(m_iPrisoner, pMonster->pev->spawnflags & SF_MONSTER_PRISONER, useType, "prisoner"))
	{
	case CUSTOM_FLAG_ON:
		pMonster->pev->spawnflags |= SF_MONSTER_PRISONER;
		if (pMonster->m_hEnemy)
		{
			pMonster->m_hEnemy = NULL;
			// make 'em stop attacking... might be better to use a different signal?
			pMonster->SetConditions(bits_COND_NEW_ENEMY);
		}
		break;
	case CUSTOM_FLAG_OFF:
		pMonster->pev->spawnflags &= ~SF_MONSTER_PRISONER;
		break;
	}

	switch (GetActionFor(m_iMonsterClip, pMonster->pev->flags & FL_MONSTERCLIP, useType, "monsterclip"))
	{
	case CUSTOM_FLAG_ON: pMonster->pev->flags |= FL_MONSTERCLIP; break;
	case CUSTOM_FLAG_OFF: pMonster->pev->flags &= ~FL_MONSTERCLIP; break;
	}

	switch (GetActionFor(m_iGag, pMonster->pev->spawnflags & SF_MONSTER_GAG, useType, "gag"))
	{
	case CUSTOM_FLAG_ON: pMonster->pev->spawnflags |= SF_MONSTER_GAG; break;
	case CUSTOM_FLAG_OFF: pMonster->pev->spawnflags &= ~SF_MONSTER_GAG; break;
	}

	switch (GetActionFor(m_iProvoked, pMonster->m_afMemory & bits_MEMORY_PROVOKED, useType, "provoked"))
	{
	case CUSTOM_FLAG_ON: pMonster->Remember(bits_MEMORY_PROVOKED); break;
	case CUSTOM_FLAG_OFF: pMonster->Forget(bits_MEMORY_PROVOKED); break;
	}

	if (pev->spawnflags & SF_CUSTOM_DEBUG)
		ALERT(at_console, " ]\n");
}

int CEnvCustomize::GetActionFor(int iField, int iActive, USE_TYPE useType, char* szDebug)
{
	int iAction = iField;

	if (iAction == CUSTOM_FLAG_USETYPE)
	{
		if (useType == USE_ON)
			iAction = CUSTOM_FLAG_ON;
		else if (useType == USE_OFF)
			iAction = CUSTOM_FLAG_OFF;
		else
			iAction = CUSTOM_FLAG_TOGGLE;
	}
	else if (iAction == CUSTOM_FLAG_INVUSETYPE)
	{
		if (useType == USE_ON)
			iAction = CUSTOM_FLAG_OFF;
		else if (useType == USE_OFF)
			iAction = CUSTOM_FLAG_ON;
		else
			iAction = CUSTOM_FLAG_TOGGLE;
	}

	if (iAction == CUSTOM_FLAG_TOGGLE)
	{
		if (iActive)
			iAction = CUSTOM_FLAG_OFF;
		else
			iAction = CUSTOM_FLAG_ON;
	}

	if (pev->spawnflags & SF_CUSTOM_DEBUG)
	{
		if (iAction == CUSTOM_FLAG_ON)
			ALERT(at_console, " %s=YES", szDebug);
		else if (iAction == CUSTOM_FLAG_OFF)
			ALERT(at_console, " %s=NO", szDebug);
	}
	return iAction;
}

void CEnvCustomize::SetBoneController(float fController, int cnum, CBaseEntity* pTarget)
{
	if (fController) //FIXME: the pTarget isn't necessarily a CBaseAnimating.
	{
		if (fController == 1024)
		{
			((CBaseAnimating*)pTarget)->SetBoneController(cnum, 0);
			if (pev->spawnflags & SF_CUSTOM_DEBUG)
				ALERT(at_console, " bone%d=0", cnum);
		}
		else
		{
			((CBaseAnimating*)pTarget)->SetBoneController(cnum, fController);
			if (pev->spawnflags & SF_CUSTOM_DEBUG)
				ALERT(at_console, " bone%d=%f", cnum, fController);
		}
	}
}