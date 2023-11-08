#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "studio.h"
#include "matrix.h"

// =================== ENV_STATIC ==============================================
#define SF_STATIC_SOLID			BIT( 0 )
#define SF_STATIC_DROPTOFLOOR	BIT( 1 )
#define SF_STATIC_NOSHADOW		BIT( 2 ) // hlrad
#define SF_STATIC_NOVLIGHT		BIT( 3 ) // hlrad
#define SF_STATIC_IGNOREPVS		BIT( 4 ) // ignore PVS - always visible (fadedistance still works)

class CEnvStatic : public CBaseEntity
{
	DECLARE_CLASS(CEnvStatic, CBaseEntity);
public:
	void Spawn(void);
	void Precache(void);
	void AutoSetSize(void);
	virtual int ObjectCaps(void) { return FCAP_IGNORE_PARENT; }
	void SetObjectCollisionBox(void);
	void KeyValue(KeyValueData* pkvd);
	void SoundThink(void);
	void StopSound(void);

	DECLARE_DATADESC();

	int m_iszLoopSound; // sound to play when passing near/inside the model
	float SoundRadius;
	float RadiusMultiplier; // the radius should be set according the model bounds. so in case it didn't work...

	// probably no need to save those
	bool SoundStopped;
	float SoundVolume;
};

LINK_ENTITY_TO_CLASS(env_static, CEnvStatic);

BEGIN_DATADESC(CEnvStatic)
	DEFINE_KEYFIELD(m_iszLoopSound, FIELD_STRING, "sndloop"),
	DEFINE_FIELD( SoundRadius, FIELD_FLOAT ),
	DEFINE_KEYFIELD( RadiusMultiplier, FIELD_FLOAT, "sndradius" ),
	DEFINE_FUNCTION( SoundThink ),
	DEFINE_FUNCTION( StopSound ),
END_DATADESC()

void CEnvStatic::Precache(void)
{
	PRECACHE_MODEL(GetModel());

	if (m_iszLoopSound)
		PRECACHE_SOUND((char*)STRING(m_iszLoopSound));
}

void CEnvStatic::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "xform"))
	{
		UTIL_StringToVector((float*)pev->startpos, pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "sndradius"))
	{
		RadiusMultiplier = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "sndloop" ) )
	{
		m_iszLoopSound = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue(pkvd);
}

void CEnvStatic::Spawn(void)
{
	Precache();
	
	if( pev->scale == 0 )
		pev->scale = 1.0f;

	if (pev->startpos == g_vecZero)
		pev->startpos = Vector(pev->scale, pev->scale, pev->scale);

	// check xform values
	if (pev->startpos.x < 0.01f) pev->startpos.x = 1.0f;
	if (pev->startpos.y < 0.01f) pev->startpos.y = 1.0f;
	if (pev->startpos.z < 0.01f) pev->startpos.z = 1.0f;
	if (pev->startpos.x > 16.0f) pev->startpos.x = 16.0f;
	if (pev->startpos.y > 16.0f) pev->startpos.y = 16.0f;
	if (pev->startpos.z > 16.0f) pev->startpos.z = 16.0f;

	SET_MODEL(edict(), GetModel());

	// tell the client about static entity
	SetBits(pev->iuser1, CF_STATIC_ENTITY);
	
	if (HasSpawnFlags(SF_STATIC_SOLID))
	{
		if (WorldPhysic->Initialized())
			pev->solid = SOLID_CUSTOM;
		pev->movetype = MOVETYPE_NONE;
	//	AutoSetSize();
	}
	AutoSetSize();

	if (HasSpawnFlags(SF_STATIC_DROPTOFLOOR))
	{
		Vector origin = GetLocalOrigin();
		origin.z += 1;
		SetLocalOrigin(origin);
		UTIL_DropToFloor(this);
	}
	else
		UTIL_SetOrigin(this, GetLocalOrigin());

	if( HasSpawnFlags( SF_STATIC_SOLID ) )
	{
		m_pUserData = WorldPhysic->CreateStaticBodyFromEntity( this );
		RelinkEntity( TRUE );
	}
#if 0
	// diffusion - EXPERIMENTAL check if the origin of the model in solid world brush.
	// if it is, set the model to skip PVS to be always visible.
	// I decided not to set this to all props, because the slight fps drop is still there, despite culling on client.
	TraceResult trace;
	Vector vecStart = GetAbsOrigin();
	Vector vecEnd = GetAbsOrigin() + Vector( 0, 0, 1 );
	TRACE_LINE( vecStart, vecEnd, TRUE, edict(), &trace );
	if( trace.fStartSolid && FStrEq(STRING(trace.pHit->v.classname), "worldspawn")  )
		pev->effects |= EF_SKIPPVS;
#endif

	if( HasSpawnFlags( SF_STATIC_IGNOREPVS ))
		pev->effects |= EF_SKIPPVS;

	if (m_iszLoopSound) // we are in the sound mode
	{
		if( RadiusMultiplier <= 0.0f )
			RadiusMultiplier = 1.0f;
		
		Vector modmins, modmaxs;
		UTIL_GetModelBounds( pev->modelindex, modmins, modmaxs );
		SoundRadius = RadiusMultiplier * 0.5 * pev->scale * ((modmaxs - modmins).Length() / 2.f); // FIXME magic numbers!!!
//		ALERT( at_console, "radius %f\n", SoundRadius );
		SetThink(&CEnvStatic::SoundThink);
		SetNextThink(RANDOM_FLOAT(0.2f, 0.7f));
	}
	else
	{
//		if( !HasSpawnFlags( SF_STATIC_SOLID ) )
//			MAKE_STATIC( edict() );
	}
}

void CEnvStatic::SoundThink(void)
{
	// why if I use only GetAbsOrigin, it crashes the game?
	Vector Org = GetAbsOrigin() + ((pev->mins + pev->maxs) / 2.f);

	CBaseEntity *pSomeone = NULL;
	if( (pSomeone = UTIL_FindEntityInSphere( pSomeone, Org, SoundRadius )) != NULL )
	{
		if (pSomeone->IsMonster() || pSomeone->IsPlayer())
		{
			SoundVolume = pSomeone->GetAbsVelocity().Length() * 0.003;
			SoundVolume = bound(0, SoundVolume, 1);
			if (SoundVolume >= 0)
			{
				EMIT_SOUND_DYN(edict(), CHAN_STATIC, (char*)STRING(m_iszLoopSound), SoundVolume, ATTN_NORM, SND_CHANGE_PITCH | SND_CHANGE_VOL, 100);
				SoundStopped = false;
			}
			SetThink(&CEnvStatic::SoundThink);
			SetNextThink(0.1);
		}
		else
			StopSound();
	}
	else
		StopSound();
}

void CEnvStatic::StopSound(void)
{
	float ThinkTime = 0.5;

	if (SoundVolume > 0)
	{
		EMIT_SOUND_DYN(edict(), CHAN_STATIC, (char*)STRING(m_iszLoopSound), SoundVolume, ATTN_NORM, SND_CHANGE_PITCH | SND_CHANGE_VOL, 100);
		SoundVolume -= 0.1;
		SoundStopped = false;
		ThinkTime = 0.05;
	}

	if (SoundStopped == false && SoundVolume <= 0)
	{
		STOP_SOUND(edict(), CHAN_STATIC, (char*)STRING(m_iszLoopSound));
		SoundStopped = true;
	}

	SetThink(&CEnvStatic::SoundThink);
	SetNextThink(ThinkTime);
}

void CEnvStatic::SetObjectCollisionBox(void)
{
	// expand for rotation
	TransformAABB(EntityToWorldTransform(), pev->mins, pev->maxs, pev->absmin, pev->absmax);

	pev->absmin.x -= 1;
	pev->absmin.y -= 1;
	pev->absmin.z -= 1;
	pev->absmax.x += 1;
	pev->absmax.y += 1;
	pev->absmax.z += 1;
}

// automatically set collision box
void CEnvStatic::AutoSetSize(void)
{
	studiohdr_t* pstudiohdr;
	pstudiohdr = (studiohdr_t*)GET_MODEL_PTR(edict());

	if (pstudiohdr == NULL)
	{
		UTIL_SetSize(pev, Vector(-10, -10, -10), Vector(10, 10, 10));
		ALERT(at_error, "env_static: unable to fetch model pointer!\n");
		return;
	}

	mstudioseqdesc_t* pseqdesc = (mstudioseqdesc_t*)((byte*)pstudiohdr + pstudiohdr->seqindex);
	UTIL_SetSize(pev, pseqdesc[pev->sequence].bbmin * pev->startpos, pseqdesc[pev->sequence].bbmax * pev->startpos);
}