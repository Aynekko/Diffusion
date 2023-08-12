#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons/weapons.h"
#include "triggers.h"

//==========================================================================
// diffusion - simplified version of a flare, as much as I could do it.
//==========================================================================

// class is in the effects.h

LINK_ENTITY_TO_CLASS(item_flare, CFlare);

BEGIN_DATADESC(CFlare)
	DEFINE_KEYFIELD(m_flTimeBurnOut, FIELD_TIME, "burnouttime"),
	DEFINE_FIELD(m_flTimeStartBurn, FIELD_TIME),
	DEFINE_FIELD(m_flPitchChangeTime, FIELD_TIME),
	DEFINE_FIELD(CurPitch, FIELD_INTEGER),
//	DEFINE_FIELD(LightRadius, FIELD_INTEGER), // pev->iuser2
	DEFINE_FIELD(EffectsCreated, FIELD_BOOLEAN),
	DEFINE_FIELD(StaticFlareOn, FIELD_BOOLEAN),
	DEFINE_KEYFIELD(ThrowVelocity, FIELD_FLOAT, "throwvelocity"),
	DEFINE_FUNCTION(Ignite),
	DEFINE_FUNCTION(BounceTouch),
	DEFINE_FUNCTION(ToggleUse),
END_DATADESC()

int CFlare::ObjectCaps(void)
{
	int flags = 0;

	if (pev->deadflag == DEAD_DEAD)
		flags = 0;
	else
		flags = FCAP_HOLDABLE_ITEM;

	// CUSTOMFLAG2 in this entity indicates that it's invisible and can't be picked up (frozen until triggered)
	if (HasFlag(F_CUSTOMFLAG2))
		flags = 0;

	return (BaseClass::ObjectCaps()) | flags;
}

void CFlare::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "burnouttime"))
	{
		m_flTimeBurnOut = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "throwvelocity"))
	{
		ThrowVelocity = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseMonster::KeyValue(pkvd);
}

void CFlare::Precache( void )
{
	PRECACHE_MODEL( "models/flare.mdl" );
	PRECACHE_SOUND( "gear/flare_burn.wav" );
	PRECACHE_SOUND( "gear/flare_off.wav" );
}

void CFlare::Spawn(void)
{
	Precache();

	if (HasSpawnFlags(SF_FLARE_DONTHURT))
		SetFlag(F_CUSTOMFLAG1);

	SetFlag(F_ENTITY_ONCEUSED);

	if (!HasSpawnFlags(SF_FLARE_THROW))
	{
		pev->movetype = MOVETYPE_BOUNCE;
		pev->solid = SOLID_BBOX;
		pev->friction = 0.6;
		pev->gravity = 0.5;
	}
	else
	{
		SetUse(&CFlare::ToggleUse);
		SetFlag(F_CUSTOMFLAG2);
		pev->rendermode = kRenderTransTexture;
		pev->renderamt = 0;
		ObjectCaps();
	}

	CurPitch = 110;
	pev->iuser2 = 125; // light radius
	if (m_flTimeBurnOut <= 0)
		m_flTimeBurnOut = 45;

	SET_MODEL(ENT(pev), "models/flare.mdl");

	// BUGBUG when I set this, player can stand on it.
	// bad behaviour if player stands on it and goes up in the func_door elevator, causing it to change direction
	// ... BUT I will leave it for now...because it's impossible to pick it up otherwise :(
	// NOTENOTE - the issue seems to be resolved, because I set the owner below.
	// coop players can still stand on it, I guess.
	UTIL_SetSize(pev, Vector(-4, -4, -4), Vector(4, 4, 4));

	SetTouch(NULL);
	m_hOwner = Instance(pev->owner);

	pev->renderfx = kRenderFxNoShadows;

	if( pev->iuser4 == 0 )
		SetFadeDistance( 2500 );
}

void CFlare::BounceTouch(CBaseEntity* pOther)
{
	pev->velocity *= 0.9;

	if( pev->deadflag == DEAD_DEAD ) // burned out, can't hurt anyone
		return;

	if (!HasFlag(F_CUSTOMFLAG1))
	{
		if (pOther->IsMonster() && !(pOther->HasFlag(F_ENTITY_ONFIRE | F_FIRE_IMMUNE)))
		{
			pOther->SetFlag(F_ENTITY_ONFIRE);
			//	ALERT(at_console, "set on fire\n");
		}
	}
}

void CFlare::ToggleUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (HasSpawnFlags(SF_FLARE_THROW))
	{
		SetUse(NULL);
		pev->targetname = NULL;
		pev->rendermode = kRenderNormal;
		pev->renderamt = 255;
		UTIL_MakeVectors(GetAbsAngles());
		Vector Throw = gpGlobals->v_forward * ThrowVelocity;
		pev->movetype = MOVETYPE_BOUNCE;
		pev->solid = SOLID_BBOX;
		pev->friction = 0.6;
		pev->gravity = 0.5;
		SetAbsVelocity(Throw);
		SetTouch(&CFlare::BounceTouch);
		RemoveFlag(F_CUSTOMFLAG2);
		ObjectCaps();
		SetThink(&CFlare::Ignite);
		SetNextThink(0.1);
	}
}

void CFlare::Ignite(void)
{
	if (EffectsCreated == false)
	{
		m_flTimeStartBurn = gpGlobals->time + m_flTimeBurnOut;
		m_flPitchChangeTime = gpGlobals->time + (m_flTimeBurnOut / 15);
		pev->effects |= EF_DIMLIGHT;
		pev->renderfx = kRenderFxFullbrightNoShadows;
		pev->iuser3 = -666; // to catch on client
		EffectsCreated = true;
	}

	TraceResult tr = UTIL_GetGlobalTrace();
	Vector vecOrg = GetAbsOrigin() + tr.vecPlaneNormal * 10;

	if (pev->waterlevel == 0)
	{
		UTIL_Sparks(vecOrg);

		MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, vecOrg );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( vecOrg.x );
			WRITE_COORD( vecOrg.y );
			WRITE_COORD( vecOrg.z + 3 );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( RANDOM_LONG( 3, 8 ) ); // scale x1.0
			WRITE_BYTE( RANDOM_LONG( 30, 50 ) ); // framerate
		MESSAGE_END();
	}

	if (gpGlobals->time > m_flPitchChangeTime)
	{
		pev->iuser2 -= 4;
		CurPitch -= 2;
		m_flPitchChangeTime = gpGlobals->time + (m_flTimeBurnOut / 15);
	}

	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "gear/flare_burn.wav", 0.25, 1.5, SND_CHANGE_PITCH, CurPitch);

	if (gpGlobals->time > m_flTimeStartBurn)
	{
		ClearEffects();
		EMIT_SOUND(ENT(pev), CHAN_STATIC, "gear/flare_off.wav", 0.25, 1.5);
		pev->deadflag = DEAD_DEAD;
		ObjectCaps();
		SetThink( &CBaseEntity::SUB_FadeOut );
		SetNextThink( 20 ); // remove after 20 sec.
		return;
	}
	else
	{
		SetThink(&CFlare::Ignite);
		SetNextThink(0.1);
	}
}

void CFlare::ClearEffects(void)
{
	STOP_SOUND(ENT(pev), CHAN_STATIC, "gear/flare_burn.wav");
	pev->effects &= ~EF_DIMLIGHT;
	pev->iuser3 = 0;
	pev->iuser2 = 0;
}