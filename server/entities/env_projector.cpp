#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "effects.h"

// =================== ENV_PROJECTOR ==============================================

class CEnvProjector : public CBaseDelay
{
	DECLARE_CLASS(CEnvProjector, CBaseDelay);
public:
	void Spawn(void);
	void Precache(void);
	void KeyValue(KeyValueData* pkvd);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	virtual STATE GetState(void) { return FBitSet(pev->effects, EF_NODRAW) ? STATE_OFF : STATE_ON; };
	virtual int ObjectCaps(void) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void CineThink(void);
	void SpriteThink(void);
	void PVSThink(void);
	void UpdatePVSPoint(void);
	float Frames(void) { return pev->frags; }

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(env_projector, CEnvProjector);

BEGIN_DATADESC(CEnvProjector)
	DEFINE_FUNCTION(CineThink),
	DEFINE_FUNCTION(SpriteThink),
	DEFINE_FUNCTION(PVSThink),
END_DATADESC()

void CEnvProjector::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "fov"))
	{
		pev->iuser2 = Q_atoi(pkvd->szValue);
		if( !pev->iuser2 ) pev->iuser2 = 90;
		pev->iuser2 = bound(1, pev->iuser2, 120);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "radius"))
	{
		pev->iuser3 = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "texture"))
	{
		pev->message = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "lightstyle"))
	{
		pev->renderfx = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "brightness" ) )
	{
		pev->fuser1 = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else BaseClass::KeyValue(pkvd);
}

void CEnvProjector::Precache(void)
{
	if (!FStringNull(pev->message))
	{
		if (FBitSet(pev->iuser1, (CF_TEXTURE | CF_SPRITE)))
		{
			pev->sequence = g_engfuncs.pfnPrecacheGeneric(STRING(pev->message));

			if (FBitSet(pev->iuser1, CF_SPRITE))
				pev->frags = MODEL_FRAMES(PRECACHE_MODEL((char*)STRING(pev->message)));	// BUGBUG: this loaded sprite twice
		}
		else if (FBitSet(pev->iuser1, CF_MOVIE))
			pev->sequence = UTIL_PrecacheMovie(pev->message);
	}
}

void CEnvProjector::Spawn(void)
{	
	pev->effects |= (EF_PROJECTED_LIGHT | EF_NODRAW );

	// diffusion - these two needed for correct fading on client
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 255;

	pev->movetype = MOVETYPE_NOCLIP;
	SetFlag(F_NOBACKCULL);

	if (!pev->framerate)
		pev->framerate = 10; // for sprites

	if( !pev->iuser3 )
		pev->iuser3 = 300;

	pev->frags = pev->iuser3; // member radius

	SetNullModel();
	SetBits(m_iFlags, MF_POINTENTITY);
	//	SetLocalAvelocity( Vector( 30, 30, 30 ));
	RelinkEntity(FALSE);

	if (HasSpawnFlags(SF_PROJECTOR_ASPECT4X3))
		pev->iuser1 |= CF_ASPECT4X3;

	if (HasSpawnFlags(SF_PROJECTOR_ASPECT3X4))
		pev->iuser1 |= CF_ASPECT3X4;

	if (HasSpawnFlags(SF_PROJECTOR_NOWORLDLIGHT))
		pev->iuser1 |= CF_NOWORLD_PROJECTION;

	if (HasSpawnFlags(SF_PROJECTOR_NOSHADOWS))
		pev->iuser1 |= CF_NOSHADOWS;

	if (HasSpawnFlags(SF_PROJECTOR_FLIPTEXTURE))
		pev->iuser1 |= CF_FLIPTEXTURE;

	if( HasSpawnFlags( SF_PROJECTOR_ONLYBRUSHSHADOWS ) )
		pev->iuser1 |= CF_ONLYBRUSHSHADOWS;

	// determine texture type by extension
	if (!FStringNull(pev->message))
	{
		const char* ext = UTIL_FileExtension(STRING(pev->message));

		if( !Q_stricmp( ext, "tga" ) || !Q_stricmp( ext, "mip" ) || !Q_stricmp( ext, "dds" ) )
			pev->iuser1 |= CF_TEXTURE;
		else if( !Q_stricmp( ext, "spr" ) )
			pev->iuser1 |= CF_SPRITE;
		else if( !Q_stricmp( ext, "avi" ) )
			pev->iuser1 |= CF_MOVIE;
		else
			ALERT( at_error, "env_projector \"%s\": unsupported texture extension!\n", GetTargetname() );
	}

	Precache();

	// create second PVS point
	pev->enemy = Create("info_target", GetAbsOrigin(), g_vecZero, edict())->edict();
	SET_MODEL(pev->enemy, "sprites/null.spr"); // allow to collect visibility info
	UTIL_SetSize(VARS(pev->enemy), Vector(-8, -8, -8), Vector(8, 8, 8));

	// enable light
	if (HasSpawnFlags(SF_PROJECTOR_START_ON) || (FStringNull( pev->targetname ) && FStringNull( m_iParent )) )
	{
		SetThink(&CBaseEntity::SUB_CallUseToggle);
		SetNextThink(0.1);
	}
}

void CEnvProjector::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (IsLockedByMaster(pActivator))
		return;

	if (useType == USE_SET)
	{
		// set radius
		value = bound(0.0f, value, 1.0f);
		pev->iuser3 = pev->frags * value;
		return;
	}

	if (FBitSet(pev->effects, EF_NODRAW))
		pev->effects &= ~EF_NODRAW;
	else pev->effects |= EF_NODRAW;

	if (pev->effects & EF_NODRAW)
	{
		DontThink();
		return;
	}
	else
	{
		pev->dmgtime = gpGlobals->time + 0.1f;

		// run properly method
		if (FBitSet(pev->iuser1, CF_MOVIE))
			SetThink(&CEnvProjector::CineThink);
		else if (FBitSet(pev->iuser1, CF_SPRITE) && Frames() > 1)
			SetThink(&CEnvProjector::SpriteThink);
		else SetThink(&CEnvProjector::PVSThink);

		SetNextThink(0.1f);
	}
}

void CEnvProjector::CineThink(void)
{
	UpdatePVSPoint();

	// update as 30 frames per second
	pev->fuser2 += CIN_FRAMETIME;
	SetNextThink(CIN_FRAMETIME);
}

void CEnvProjector::SpriteThink(void)
{
	// animate the sprite
	pev->frame += pev->framerate * (gpGlobals->time - pev->dmgtime);
	if (pev->frame > Frames())
		pev->frame = fmod(pev->frame, Frames());
	pev->dmgtime = gpGlobals->time;

	UpdatePVSPoint();

	// keep think at 0.1 so interpolation will working properly
	SetNextThink(0.1f);
}

void CEnvProjector::PVSThink(void)
{
	UpdatePVSPoint();
	SetNextThink(0.1f);
}

void CEnvProjector::UpdatePVSPoint(void)
{
	TraceResult tr;

	UTIL_MakeVectors(GetAbsAngles());
	Vector vecSrc = GetAbsOrigin() + gpGlobals->v_forward * 8.0f;
	Vector vecEnd = vecSrc + gpGlobals->v_forward * pev->iuser3;
	UTIL_TraceLine(vecSrc, vecEnd, ignore_monsters, edict(), &tr);

	// this is our second PVS point
	CBaseEntity* pVisHelper = CBaseEntity::Instance(pev->enemy);

	if (pVisHelper)
		UTIL_SetOrigin(pVisHelper, tr.vecEndPos + tr.vecPlaneNormal * 8.0f);
}