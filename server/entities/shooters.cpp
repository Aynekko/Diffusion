#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "entities/func_break.h"
#include "customentity.h"
#include "effects.h"
#include "studio.h"

#define SF_GIBSHOOTER_REPEATABLE	1 // allows a gibshooter to be refired

// Shooter particle
class CShot : public CSprite
{
	DECLARE_CLASS(CShot, CSprite);
public:
	void Touch(CBaseEntity* pOther)
	{
		if (pev->teleport_time > gpGlobals->time)
			return;

		// don't fire too often in collisions!
		// teleport_time is the soonest this can be touched again.
		pev->teleport_time = gpGlobals->time + 0.1;

		if (pev->netname)
			UTIL_FireTargets(pev->netname, this, this, USE_TOGGLE, 0);
		if (pev->message && pOther != g_pWorld)
			UTIL_FireTargets(pev->message, pOther, this, USE_TOGGLE, 0);
	};
};

LINK_ENTITY_TO_CLASS(shot, CShot); // enable save\restore

class CGibShooter : public CBaseDelay
{
	DECLARE_CLASS(CGibShooter, CBaseDelay);
public:
	void Spawn(void);
	void Precache(void);
	void KeyValue(KeyValueData* pkvd);
	void ShootThink(void);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	virtual int ObjectCaps(void) { return (BaseClass::ObjectCaps() & (~FCAP_ACROSS_TRANSITION)) | FCAP_SET_MOVEDIR; };

	virtual CBaseEntity* CreateGib(void);

	DECLARE_DATADESC();

	int	m_iGibs;
	int	m_iGibCapacity;
	int	m_iGibMaterial;
	int	m_iGibModelIndex;
	float	m_flGibVelocity;
	float	m_flVariance;
	float	m_flGibLife;
	string_t	m_iszTargetname;
	string_t	m_iszSpawnTarget;
	int	m_iBloodColor;
};

BEGIN_DATADESC(CGibShooter)
DEFINE_KEYFIELD(m_iGibs, FIELD_INTEGER, "m_iGibs"),
DEFINE_KEYFIELD(m_iGibCapacity, FIELD_INTEGER, "m_iGibs"),
DEFINE_FIELD(m_iGibMaterial, FIELD_INTEGER),
DEFINE_FIELD(m_iGibModelIndex, FIELD_INTEGER),
DEFINE_KEYFIELD(m_flGibVelocity, FIELD_FLOAT, "m_flVelocity"),
DEFINE_KEYFIELD(m_flVariance, FIELD_FLOAT, "m_flVariance"),
DEFINE_KEYFIELD(m_flGibLife, FIELD_FLOAT, "m_flGibLife"),
DEFINE_KEYFIELD(m_iszTargetname, FIELD_STRING, "m_iszTargetName"),
DEFINE_KEYFIELD(m_iszSpawnTarget, FIELD_STRING, "m_iszSpawnTarget"),
DEFINE_KEYFIELD(m_iBloodColor, FIELD_INTEGER, "m_iBloodColor"),
DEFINE_FUNCTION(ShootThink),
END_DATADESC()

LINK_ENTITY_TO_CLASS(gibshooter, CGibShooter);

void CGibShooter::Precache(void)
{
	if (g_Language == LANGUAGE_GERMAN)
	{
		m_iGibModelIndex = PRECACHE_MODEL("models/germanygibs.mdl");
	}
	else if (m_iBloodColor == BLOOD_COLOR_YELLOW)
	{
		m_iGibModelIndex = PRECACHE_MODEL("models/agibs.mdl");
	}
	else
	{
		m_iGibModelIndex = PRECACHE_MODEL("models/hgibs.mdl");
	}
}

void CGibShooter::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "m_iGibs"))
	{
		m_iGibs = m_iGibCapacity = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_flVelocity"))
	{
		m_flGibVelocity = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_flVariance"))
	{
		m_flVariance = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_flGibLife"))
	{
		m_flGibLife = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszTargetName"))
	{
		m_iszTargetname = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszSpawnTarget"))
	{
		m_iszSpawnTarget = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iBloodColor"))
	{
		m_iBloodColor = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
	{
		CBaseDelay::KeyValue(pkvd);
	}
}

void CGibShooter::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	m_hActivator = pActivator;
	SetThink(&CGibShooter::ShootThink);
	SetNextThink(0);
}

void CGibShooter::Spawn(void)
{
	Precache();

	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;

	// is mapper forgot set angles?
	if (pev->movedir == g_vecZero)
		pev->movedir = Vector(1, 0, 0);

	if (m_flDelay == 0)
	{
		m_flDelay = 0.1;
	}

	if (m_flGibLife == 0)
	{
		m_flGibLife = 25;
	}

	pev->body = MODEL_FRAMES(m_iGibModelIndex);
}


CBaseEntity* CGibShooter::CreateGib(void)
{
	if (CVAR_GET_FLOAT("violence_hgibs") == 0)
		return NULL;

	CGib* pGib = GetClassPtr((CGib*)NULL);

	if (m_iBloodColor == BLOOD_COLOR_YELLOW)
	{
		pGib->Spawn("models/agibs.mdl");
		pGib->m_bloodColor = BLOOD_COLOR_YELLOW;
	}
	else if (m_iBloodColor)
	{
		pGib->Spawn("models/hgibs.mdl");
		pGib->m_bloodColor = m_iBloodColor;
	}
	else
	{
		pGib->Spawn("models/hgibs.mdl");
		pGib->m_bloodColor = BLOOD_COLOR_RED;
	}

	if (pev->body <= 1)
	{
		ALERT(at_aiconsole, "GibShooter Body is <= 1!\n");
	}

	pGib->pev->body = RANDOM_LONG(1, pev->body - 1);// avoid throwing random amounts of the 0th gib. (skull).

	return pGib;
}


void CGibShooter::ShootThink(void)
{
	SetNextThink(m_flDelay);

	Vector vecShootDir = EntityToWorldTransform().VectorRotate(pev->movedir);
	UTIL_MakeVectors(GetAbsAngles()); // g-cont. was missed into original game

	vecShootDir = vecShootDir + gpGlobals->v_right * RANDOM_FLOAT(-1, 1) * m_flVariance;
	vecShootDir = vecShootDir + gpGlobals->v_forward * RANDOM_FLOAT(-1, 1) * m_flVariance;
	vecShootDir = vecShootDir + gpGlobals->v_up * RANDOM_FLOAT(-1, 1) * m_flVariance;

	vecShootDir = vecShootDir.Normalize();
	CBaseEntity* pShot = CreateGib();

	if (pShot)
	{
		pShot->SetAbsOrigin(GetAbsOrigin());
		pShot->SetAbsVelocity(vecShootDir * m_flGibVelocity);

		if (pShot->m_iActorType == ACTOR_DYNAMIC)
			WorldPhysic->AddForce(pShot, vecShootDir * m_flGibVelocity * (1.0f / gpGlobals->frametime) * 0.1f);

		// custom particles already set this
		if (FClassnameIs(pShot->pev, "gib"))
		{
			CGib* pGib = (CGib*)pShot;

			Vector vecAvelocity = pGib->GetLocalAvelocity();
			vecAvelocity.x = RANDOM_FLOAT(100, 200);
			vecAvelocity.y = RANDOM_FLOAT(100, 300);
			pGib->SetLocalAvelocity(vecAvelocity);

			float thinkTime = pGib->pev->nextthink - gpGlobals->time;
			pGib->m_lifeTime = (m_flGibLife * RANDOM_FLOAT(0.95, 1.05)); // +/- 5%

			if (pGib->m_lifeTime < thinkTime)
			{
				pGib->SetNextThink(pGib->m_lifeTime);
				pGib->m_lifeTime = 0;
			}
		}

		pShot->pev->targetname = m_iszTargetname;

		if (m_iszSpawnTarget)
			UTIL_FireTargets(m_iszSpawnTarget, pShot, this, USE_TOGGLE, 0);
	}

	if (--m_iGibs <= 0)
	{
		if (FBitSet(pev->spawnflags, SF_GIBSHOOTER_REPEATABLE))
		{
			m_iGibs = m_iGibCapacity;
			SetThink(NULL);
			SetNextThink(0);
		}
		else
		{
			SetThink(&CBaseEntity::SUB_Remove);
			SetNextThink(0);
		}
	}
}


class CEnvShooter : public CGibShooter
{
	DECLARE_CLASS(CEnvShooter, CGibShooter);

	void Spawn(void);
	void Precache(void);
	void KeyValue(KeyValueData* pkvd);

	DECLARE_DATADESC();

	virtual CBaseEntity* CreateGib(void);

	string_t m_iszTouch;
	string_t m_iszTouchOther;
	int m_iPhysics;
	float m_fFriction;
	Vector m_vecSize;
};

BEGIN_DATADESC(CEnvShooter)
DEFINE_KEYFIELD(m_iszTouch, FIELD_STRING, "m_iszTouch"),
DEFINE_KEYFIELD(m_iszTouchOther, FIELD_STRING, "m_iszTouchOther"),
DEFINE_KEYFIELD(m_iPhysics, FIELD_INTEGER, "m_iPhysics"),
DEFINE_KEYFIELD(m_fFriction, FIELD_FLOAT, "m_fFriction"),
DEFINE_KEYFIELD(m_vecSize, FIELD_VECTOR, "m_vecSize"),
END_DATADESC()

LINK_ENTITY_TO_CLASS(env_shooter, CEnvShooter);

void CEnvShooter::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "shootmodel"))
	{
		pev->model = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "shootsounds"))
	{
		int iNoise = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
		switch (iNoise)
		{
		case 0:
			m_iGibMaterial = matGlass;
			break;
		case 1:
			m_iGibMaterial = matWood;
			break;
		case 2:
			m_iGibMaterial = matMetal;
			break;
		case 3:
			m_iGibMaterial = matFlesh;
			break;
		case 4:
			m_iGibMaterial = matRocks;
			break;
		default:
		case -1:
			m_iGibMaterial = matNone;
			break;
		}
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszTouch"))
	{
		m_iszTouch = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszTouchOther"))
	{
		m_iszTouchOther = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iPhysics"))
	{
		m_iPhysics = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_fFriction"))
	{
		m_fFriction = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_vecSize"))
	{
		UTIL_StringToVector(m_vecSize, pkvd->szValue);
		m_vecSize = m_vecSize / 2;
		pkvd->fHandled = TRUE;
	}
	else
	{
		CGibShooter::KeyValue(pkvd);
	}
}

void CEnvShooter::Precache(void)
{
	m_iGibModelIndex = PRECACHE_MODEL(GetModel());

	CBreakable::MaterialSoundPrecache((Materials)m_iGibMaterial);
}

void CEnvShooter::Spawn(void)
{
	int iBody = pev->body;
	BaseClass::Spawn();
	pev->body = iBody;
}

CBaseEntity* CEnvShooter::CreateGib(void)
{
	if (m_iPhysics <= 1)
	{
		// normal gib or sticky gib
		CGib* pGib = GetClassPtr((CGib*)NULL);

		pGib->Spawn(GetModel());

		if (m_iPhysics == 1)
		{
			// sticky gib
			pGib->pev->movetype = MOVETYPE_TOSS;
			pGib->pev->solid = SOLID_BBOX;
			pGib->SetTouch(&CGib::StickyGibTouch);
		}

		if (m_iBloodColor)
			pGib->m_bloodColor = m_iBloodColor;
		else pGib->m_bloodColor = DONT_BLEED;
		pGib->m_material = m_iGibMaterial;

		if (m_iBloodColor == DONT_BLEED)  // diffusion - for garg
			pGib->m_material = matMetal;

		pGib->pev->rendermode = pev->rendermode;
		pGib->pev->renderamt = pev->renderamt;
		pGib->pev->rendercolor = pev->rendercolor;
		pGib->pev->renderfx = pev->renderfx;
		pGib->pev->effects = pev->effects & ~EF_NODRAW;
		pGib->pev->scale = pev->scale;
		pGib->pev->skin = pev->skin;

		// g-cont. random body select
		if (pGib->pev->body == -1)
		{
			int numBodies = MODEL_FRAMES(pGib->pev->modelindex);
			pGib->pev->body = RANDOM_LONG(0, numBodies - 1);
		}
		else if (pev->body > 0)
			pGib->pev->body = RANDOM_LONG(0, pev->body - 1);

		// g-cont. random skin select
		if (pGib->pev->skin == -1)
		{
			studiohdr_t* pstudiohdr = (studiohdr_t*)(GET_MODEL_PTR(ENT(pGib->pev)));

			// NOTE: this code works fine only under XashXT because GoldSource doesn't merge texture and model files
			// into one model_t slot. So this will not working with models which keep textures seperate
			if (pstudiohdr && pstudiohdr->numskinfamilies > 0)
			{
				pGib->pev->skin = RANDOM_LONG(0, pstudiohdr->numskinfamilies - 1);
			}
			else pGib->pev->skin = 0; // reset to default
		}

		return pGib;
	}

	// special shot
	CShot* pShot = GetClassPtr((CShot*)NULL);
	pShot->pev->classname = MAKE_STRING("shot");
	pShot->pev->solid = SOLID_SLIDEBOX;
	pShot->SetLocalAvelocity(GetLocalAvelocity());
	SET_MODEL(ENT(pShot->pev), STRING(pev->model));
	UTIL_SetSize(pShot->pev, -m_vecSize, m_vecSize);
	pShot->pev->renderamt = pev->renderamt;
	pShot->pev->rendermode = pev->rendermode;
	pShot->pev->rendercolor = pev->rendercolor;
	pShot->pev->renderfx = pev->renderfx;
	pShot->pev->netname = m_iszTouch;
	pShot->pev->message = m_iszTouchOther;
	pShot->pev->effects = pev->effects & ~EF_NODRAW;
	pShot->pev->skin = pev->skin;
	pShot->pev->body = pev->body;
	pShot->pev->scale = pev->scale;
	pShot->pev->frame = pev->frame;
	pShot->pev->framerate = pev->framerate;
	pShot->pev->friction = m_fFriction;

	// g-cont. random body select
	if (pShot->pev->body == -1)
	{
		int numBodies = MODEL_FRAMES(pShot->pev->modelindex);
		pShot->pev->body = RANDOM_LONG(0, numBodies - 1);
	}
	else if (pev->body > 0)
		pShot->pev->body = RANDOM_LONG(0, pev->body - 1);

	// g-cont. random skin select
	if (pShot->pev->skin == -1)
	{
		studiohdr_t* pstudiohdr = (studiohdr_t*)(GET_MODEL_PTR(ENT(pShot->pev)));

		// NOTE: this code works fine only under XashXT because GoldSource doesn't merge texture and model files
		// into one model_t slot. So this will not working with models which keep textures seperate
		if (pstudiohdr && pstudiohdr->numskinfamilies > 0)
		{
			pShot->pev->skin = RANDOM_LONG(0, pstudiohdr->numskinfamilies - 1);
		}
		else pShot->pev->skin = 0; // reset to default
	}

	switch (m_iPhysics)
	{
	case 2:
		pShot->pev->movetype = MOVETYPE_NOCLIP;
		pShot->pev->solid = SOLID_NOT;
		break;
	case 3:
		pShot->pev->movetype = MOVETYPE_FLYMISSILE;
		break;
	case 4:
		pShot->pev->movetype = MOVETYPE_BOUNCEMISSILE;
		break;
	case 5:
		pShot->pev->movetype = MOVETYPE_TOSS;
		break;
	case 6:
		// FIXME: tune NxGroupMask for each body to avoid collision with this particles
		// because so small bodies can be pushed through floor
		pShot->pev->solid = SOLID_NOT;
		if (WorldPhysic->Initialized())
			pShot->pev->movetype = MOVETYPE_PHYSIC;
		pShot->SetAbsOrigin(GetAbsOrigin());
		pShot->m_pUserData = WorldPhysic->CreateBodyFromEntity(pShot);
		break;
	default:
		pShot->pev->movetype = MOVETYPE_BOUNCE;
		break;
	}

	if (pShot->pev->framerate && UTIL_GetModelType(pShot->pev->modelindex) == mod_sprite)
	{
		pShot->m_maxFrame = (float)MODEL_FRAMES(pShot->pev->modelindex) - 1;
		if (pShot->m_maxFrame > 1.0f)
		{
			if (m_flGibLife)
			{
				pShot->pev->dmgtime = gpGlobals->time + m_flGibLife;
				pShot->SetThink(&CSprite::AnimateUntilDead);
			}
			else
			{
				pShot->SetThink(&CSprite::AnimateThink);
			}

			pShot->SetNextThink(0);
			pShot->m_lastTime = gpGlobals->time;
			return pShot;
		}
	}

	// if it's not animating
	if (m_flGibLife)
	{
		pShot->SetThink(&CBaseEntity::SUB_Remove);
		pShot->SetNextThink(m_flGibLife);
	}

	return pShot;
}