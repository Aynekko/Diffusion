#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "entities/trains.h"			// trigger_camera has train functionality
#include "game/gamerules.h"

//
// Render parameters trigger
//
// This entity will copy its render parameters (renderfx, rendermode, rendercolor, renderamt)
// to its targets when triggered.
//


// Flags to indicate masking off various render parameters that are normally copied to the targets
#define SF_RENDER_MASKFX		BIT(0)
#define SF_RENDER_MASKAMT		BIT(1)
#define SF_RENDER_MASKMODE		BIT(2)
#define SF_RENDER_MASKCOLOR		BIT(3)
//LRC
#define SF_RENDER_KILLTARGET	BIT(5)
#define SF_RENDER_ONLYONCE		BIT(6)
// diffusion
#define SF_RENDER_RESETSKIN BIT(7)
#define SF_RENDER_RESETBODY BIT(8)

//LRC-  RenderFxFader, a subsidiary entity for RenderFxManager
class CRenderFxFader : public CBaseEntity
{
	DECLARE_CLASS(CRenderFxFader, CBaseEntity);
public:
	void Spawn(void);
	void FadeThink(void);
	virtual int ObjectCaps(void) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_DATADESC();

	float m_flStartTime;
	float m_flDuration;
	float m_flCoarseness;
	int m_iStartAmt;
	int m_iOffsetAmt;
	Vector m_vecStartColor;
	Vector m_vecOffsetColor;
	float m_fStartScale;
	float m_fOffsetScale;
	EHANDLE m_hTarget;

	int m_iszAmtFactor;
};

LINK_ENTITY_TO_CLASS(fader, CRenderFxFader);

BEGIN_DATADESC(CRenderFxFader)
	DEFINE_FIELD(m_flStartTime, FIELD_FLOAT),
	DEFINE_FIELD(m_flDuration, FIELD_FLOAT),
	DEFINE_FIELD(m_flCoarseness, FIELD_FLOAT),
	DEFINE_FIELD(m_iStartAmt, FIELD_INTEGER),
	DEFINE_FIELD(m_iOffsetAmt, FIELD_INTEGER),
	DEFINE_FIELD(m_vecStartColor, FIELD_VECTOR),
	DEFINE_FIELD(m_vecOffsetColor, FIELD_VECTOR),
	DEFINE_FIELD(m_fStartScale, FIELD_FLOAT),
	DEFINE_FIELD(m_fOffsetScale, FIELD_FLOAT),
	DEFINE_FIELD(m_hTarget, FIELD_EHANDLE),
	DEFINE_FUNCTION(FadeThink),
END_DATADESC()

void CRenderFxFader::Spawn(void)
{
	SetThink(&CRenderFxFader::FadeThink);
}

void CRenderFxFader::FadeThink(void)
{
	if (m_hTarget == NULL)
	{
		SUB_Remove();
		return;
	}

	float flDegree = (gpGlobals->time - m_flStartTime) / m_flDuration;

	if (flDegree >= 1)
	{
		m_hTarget->pev->renderamt = m_iStartAmt + m_iOffsetAmt;
		m_hTarget->pev->rendercolor = m_vecStartColor + m_vecOffsetColor;
		m_hTarget->pev->scale = m_fStartScale + m_fOffsetScale;
		// diffusion - added these two
		// can't set back to 0 - I added spawnflags...
		if( pev->skin > 0 )
			m_hTarget->pev->skin = pev->skin;
		if( pev->body > 0 )
			m_hTarget->pev->body = pev->body;
		if( HasSpawnFlags( SF_RENDER_RESETSKIN ) )
			m_hTarget->pev->skin = 0;
		if( HasSpawnFlags( SF_RENDER_RESETBODY ) )
			m_hTarget->pev->body = 0;
		if( pev->iuser1 == 2 ) // set_nodraw
			m_hTarget->pev->effects |= EF_NODRAW;
		else if( pev->iuser1 == 1 )
			m_hTarget->pev->effects &= ~EF_NODRAW;

		SUB_UseTargets(m_hTarget, USE_TOGGLE, 0);

		if (pev->spawnflags & SF_RENDER_KILLTARGET)
		{
			m_hTarget->SetThink(&CBaseEntity::SUB_Remove);
			m_hTarget->SetNextThink(0.1);
		}

		m_hTarget = NULL;

		SetNextThink(0.1);
		SetThink(&CBaseEntity::SUB_Remove);
	}
	else
	{
		m_hTarget->pev->renderamt = m_iStartAmt + m_iOffsetAmt * flDegree;

		m_hTarget->pev->rendercolor.x = m_vecStartColor.x + m_vecOffsetColor.x * flDegree;
		m_hTarget->pev->rendercolor.y = m_vecStartColor.y + m_vecOffsetColor.y * flDegree;
		m_hTarget->pev->rendercolor.z = m_vecStartColor.z + m_vecOffsetColor.z * flDegree;

		m_hTarget->pev->scale = m_fStartScale + m_fOffsetScale * flDegree;

		SetNextThink(m_flCoarseness);
	}
}

// RenderFxManager itself
class CRenderFxManager : public CPointEntity
{
	DECLARE_CLASS(CRenderFxManager, CPointEntity);
public:
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void Affect(CBaseEntity* pEntity, BOOL bIsLocus, CBaseEntity* pActivator);

	void KeyValue(KeyValueData* pkvd);
};

LINK_ENTITY_TO_CLASS(env_render, CRenderFxManager);

void CRenderFxManager::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "m_fScale"))
	{
		pev->scale = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "set_nodraw" ) )
	{
		pev->iuser1 = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue(pkvd);
}

void CRenderFxManager::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!FStringNull(pev->target))
	{
		CBaseEntity *pTarget = NULL;
		if( !Q_strcmp( STRING( pev->target ), "player" ) )
			pTarget = UTIL_PlayerByIndex( 1 );
		else
			pTarget = UTIL_FindEntityByTargetname(NULL, STRING(pev->target), pActivator);
		BOOL first = TRUE;
		while (pTarget != NULL)
		{
			Affect(pTarget, first, pActivator);
			first = FALSE;
			pTarget = UTIL_FindEntityByTargetname(pTarget, STRING(pev->target), pActivator);
		}
	}

	if (pev->spawnflags & SF_RENDER_ONLYONCE)
	{
		SetThink(&CBaseEntity::SUB_Remove);
		SetNextThink(0.1);
	}
}

void CRenderFxManager::Affect(CBaseEntity* pTarget, BOOL bIsFirst, CBaseEntity* pActivator)
{
	entvars_t* pevTarget = pTarget->pev;

	if (!HasSpawnFlags(SF_RENDER_MASKFX))
		pevTarget->renderfx = pev->renderfx;

	if (!HasSpawnFlags(SF_RENDER_MASKMODE))
	{
		//LRC - amt is often 0 when mode is normal. Set it to be fully visible, for fade purposes.
		if (pev->frags && pevTarget->renderamt == 0 && pevTarget->rendermode == kRenderNormal)
			pevTarget->renderamt = 255;
		pevTarget->rendermode = pev->rendermode;
	}

	if (pev->frags == 0) // not fading?
	{
		if (!HasSpawnFlags(SF_RENDER_MASKAMT))
			pevTarget->renderamt = pev->renderamt;
		if (!HasSpawnFlags(SF_RENDER_MASKCOLOR))
			pevTarget->rendercolor = pev->rendercolor;
		if (pev->scale)
			pevTarget->scale = pev->scale;
		// diffusion - added these two
		// can't set back to 0 - I added spawnflags...
		if( pev->skin > 0 )
			pevTarget->skin = pev->skin;
		if( pev->body > 0 )
			pevTarget->body = pev->body;
		if( HasSpawnFlags( SF_RENDER_RESETSKIN ) )
			pevTarget->skin = 0;
		if( HasSpawnFlags( SF_RENDER_RESETBODY ) )
			pevTarget->body = 0;
		if( pev->iuser1 == 2 ) // set_nodraw
			pevTarget->effects |= EF_NODRAW;
		else if( pev->iuser1 == 1 )
			pevTarget->effects &= ~EF_NODRAW;

		if (bIsFirst)
			UTIL_FireTargets(STRING(pev->netname), pTarget, this, USE_TOGGLE, 0);
	}
	else
	{
		//LRC - fade the entity in/out!
		// (We create seperate fader entities to do this, one for each entity that needs fading.)
		CRenderFxFader* pFader = GetClassPtr((CRenderFxFader*)NULL);
		pFader->pev->classname = MAKE_STRING("fader");
		pFader->m_hTarget = pTarget;
		pFader->m_iStartAmt = pevTarget->renderamt;
		pFader->m_vecStartColor = pevTarget->rendercolor;
		pFader->m_fStartScale = pevTarget->scale;
		if (pFader->m_fStartScale == 0)
			pFader->m_fStartScale = 1; // When we're scaling, 0 is treated as 1. Use 1 as the number to fade from.
		pFader->pev->spawnflags = pev->spawnflags;
		pFader->pev->skin = pev->skin;
		pFader->pev->body = pev->body;
		pFader->pev->iuser1 = pev->iuser1; // set_nodraw

		if (bIsFirst)
			pFader->pev->target = pev->netname;

		if (!FBitSet(pev->spawnflags, SF_RENDER_MASKAMT))
			pFader->m_iOffsetAmt = pev->renderamt - pevTarget->renderamt;
		else
			pFader->m_iOffsetAmt = 0;

		if (!FBitSet(pev->spawnflags, SF_RENDER_MASKCOLOR))
		{
			pFader->m_vecOffsetColor.x = pev->rendercolor.x - pevTarget->rendercolor.x;
			pFader->m_vecOffsetColor.y = pev->rendercolor.y - pevTarget->rendercolor.y;
			pFader->m_vecOffsetColor.z = pev->rendercolor.z - pevTarget->rendercolor.z;
		}
		else
		{
			pFader->m_vecOffsetColor = g_vecZero;
		}

		if (pev->scale)
			pFader->m_fOffsetScale = pev->scale - pevTarget->scale;
		else
			pFader->m_fOffsetScale = 0;

		pFader->m_flStartTime = gpGlobals->time;
		pFader->m_flDuration = pev->frags;
		pFader->m_flCoarseness = pev->armorvalue;
		pFader->SetNextThink(0);
		pFader->Spawn();
	}
}