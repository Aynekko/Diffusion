#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons/weapons.h"
#include "triggers.h"
#include "entities/soundent.h"

//==========================================================================
// diffusion - explosive barrel similar to HL2
// NOTE: monsters will always catch fire when damaged by barrel's explosion. See CBaseMonster::TakeDamage
//==========================================================================

#define SF_BARREL_CANHOLD		BIT(0)
#define SF_BARREL_RESPAWNABLE	BIT(1)
#define SF_BARREL_DONTDROP		BIT(2)

#define EXTENDED_FADEDISTANCE 2000 // add fadedistance if the barrel was picked up by player. He can move it around

class CExplosiveBarrel : public CBaseMonster
{
	DECLARE_CLASS(CExplosiveBarrel, CBaseMonster);
public:
	void Spawn(void);
	void Precache(void);
	void Ignite(void);
	void Explode(void);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	int ObjectCaps(void);
	void ClearEffects(void);
	//	void KeyValue( KeyValueData *pkvd );
	void TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType);
	int TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);
	Vector SpawnOrigin; // remember respawn origin, if the player takes barrel away
	void RespawnBarrel( void );
	int RespawnTime;
	void OnPickupObject(void);

	DECLARE_DATADESC();
};

BEGIN_DATADESC(CExplosiveBarrel)
	DEFINE_FUNCTION(Ignite),
	DEFINE_FUNCTION(Explode),
	DEFINE_FUNCTION( RespawnBarrel ),
	DEFINE_FIELD( SpawnOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( RespawnTime, FIELD_INTEGER ),
END_DATADESC();

LINK_ENTITY_TO_CLASS(prop_explosive_barrel, CExplosiveBarrel);

int CExplosiveBarrel::ObjectCaps(void)
{
	int flags;

	if (HasSpawnFlags(SF_BARREL_CANHOLD) && pev->health > 0)
		flags = FCAP_HOLDABLE_ITEM;
	else
		flags = 0;

	return (BaseClass::ObjectCaps()) | flags;
}

void CExplosiveBarrel::Precache(void)
{
	if (pev->model)
		PRECACHE_MODEL((char*)STRING(pev->model));
	else
		PRECACHE_MODEL("models/flammable.mdl");

	PRECACHE_SOUND("ambience/fire_burning.wav");
	PRECACHE_SOUND("drone/drone_hit1.wav");
	PRECACHE_SOUND("drone/drone_hit2.wav");
	PRECACHE_SOUND("drone/drone_hit3.wav");
	PRECACHE_SOUND("drone/drone_hit4.wav");
	PRECACHE_SOUND("drone/drone_hit5.wav");
	PRECACHE_SOUND( "debris/barrel_pickup.wav" );
	UTIL_PrecacheOther("env_explosion");
}

void CExplosiveBarrel::OnPickupObject( void )
{
	if( HasFlag( F_ENTITY_ONCEUSED ) )
	{
		if( pev->iuser4 > 0 )
			pev->iuser4 += EXTENDED_FADEDISTANCE;

		RemoveFlag( F_ENTITY_ONCEUSED );
	}

	EMIT_SOUND_DYN( edict(), CHAN_STATIC, "debris/barrel_pickup.wav", 0.5f, ATTN_NORM, 0, RANDOM_LONG(85,105) );
}

void CExplosiveBarrel::RespawnBarrel(void)
{
	SetAbsOrigin( SpawnOrigin );
	pev->health = 30;
	pev->max_health = pev->health;
	pev->solid = SOLID_BBOX;
	pev->effects &= ~EF_NODRAW;
	pev->takedamage = DAMAGE_YES;
	ObjectCaps();
	SetFlag( F_ENTITY_ONCEUSED );
	if( pev->iuser4 > EXTENDED_FADEDISTANCE )
		pev->iuser4 -= EXTENDED_FADEDISTANCE;
	RelinkEntity( TRUE );
}

void CExplosiveBarrel::Spawn(void)
{
	Precache();

	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model));
	else
		SET_MODEL(ENT(pev), "models/flammable.mdl");

	SetFlag(F_NOT_A_MONSTER);
	SetFlag( F_ENTITY_ONCEUSED );
	pev->health = 30;
	pev->max_health = pev->health;
#if 1
	pev->solid = SOLID_BBOX;
	pev->movetype = HasSpawnFlags( SF_BARREL_CANHOLD ) ? MOVETYPE_PUSHSTEP : MOVETYPE_NONE;
#else
	if( WorldPhysic->Initialized() )
	{
		pev->movetype = MOVETYPE_PHYSIC;
		pev->solid = SOLID_CUSTOM;
		m_pUserData = WorldPhysic->CreateBodyFromEntity( this );
	}
#endif
	pev->takedamage = DAMAGE_YES;
	m_bloodColor = DONT_BLEED;
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 44));
	IsOnFire = false;
	pev->dmg = 150;

	if( SpawnOrigin == g_vecZero )
		SpawnOrigin = GetAbsOrigin();

	RespawnTime = 30; // hardcoded for now

	pev->iuser3 = -661; // custom flag for dimlight

	if( !HasSpawnFlags( SF_BARREL_DONTDROP ) )
		UTIL_DropToFloor( this );
}

void CExplosiveBarrel::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( !IsOnFire )
	{
		IsOnFire = true;
		pev->effects |= EF_DIMLIGHT;
		CSoundEnt::InsertSound( bits_SOUND_DANGER, GetAbsOrigin(), 250, 0.3 );
		SetThink( &CExplosiveBarrel::Ignite );
		SetNextThink( 0.1 );
	}
}

void CExplosiveBarrel::TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType)
{
	UTIL_Ricochet(ptr->vecEndPos, RANDOM_FLOAT(0.5, 1.0));
	CBaseMonster::TraceAttack(pevAttacker, flDamage, vecDir, ptr, bitsDamageType);
}

int CExplosiveBarrel::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	// the attacker now owns us.
	if( pevAttacker )
		m_hOwner = CBaseEntity::Instance( pevAttacker );
	
	switch( RANDOM_LONG( 0, 4 ) )
	{
	case 0: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit1.wav", 1, ATTN_NORM ); break;
	case 1: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit2.wav", 1, ATTN_NORM ); break;
	case 2: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit3.wav", 1, ATTN_NORM ); break;
	case 3: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit4.wav", 1, ATTN_NORM ); break;
	case 4: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit5.wav", 1, ATTN_NORM ); break;
	}

	// immune to slash damage (i.e. player's knife)
	if( bitsDamageType & DMG_SLASH )
		return 0;

	if( FClassnameIs( pevInflictor, "crossbow_bolt" ) )
		return 0;

	if( pev->waterlevel > 1 )
		return 0;

	bool IgniteCondition = false;

	if( !IsOnFire )
	{
		// blast from afar and big damage. set hp to 10 and ignite
		if( ((pevInflictor->origin - GetAbsOrigin()).Length() > 100) && (flDamage > 25) )
			pev->health = 10;
		else
		{
			// clamp damage to have some burning (1-4 hp left)
			if( flDamage > 29 )
				flDamage = RANDOM_LONG( 26, 29 );

			pev->health -= flDamage;
		}
	}
	else
		pev->health -= flDamage / 2; // accept only halved external damage when on fire

	if( pev->health <= 18 )
		IgniteCondition = true;
	
	if (!IsOnFire && IgniteCondition)
	{
		IsOnFire = true;
		pev->effects |= EF_DIMLIGHT;
		CSoundEnt::InsertSound(bits_SOUND_DANGER, GetAbsOrigin(), 250, 0.3);
	//	pev->renderfx = kRenderFxFullbrightNoShadows; // don't cast dynamic shadows while on fire, and be fullbright // done on client
		SetThink(&CExplosiveBarrel::Ignite);
		SetNextThink(0.1);
	}

	return 0; // this way we won't use Killed()
}

void CExplosiveBarrel::Ignite(void)
{
	if( pev->waterlevel > 1 )
	{
		ClearEffects();
		DontThink();
		return;
	}

	if (pev->health <= 0)
	{
		Explode();
		return;
	}

	pev->health--;

	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "ambience/fire_burning.wav", 0.5, 1.5, SND_CHANGE_PITCH, 100);

	Vector vecOrg = GetAbsOrigin();
	MESSAGE_BEGIN(MSG_PVS, gmsgTempEnt, vecOrg);
	WRITE_BYTE(TE_FIRE);
		WRITE_COORD(vecOrg.x + RANDOM_LONG(-10, 10));
		WRITE_COORD(vecOrg.y + RANDOM_LONG(-10, 10));
		WRITE_COORD(vecOrg.z + 32);
		WRITE_SHORT(g_sModelIndexFire);
		WRITE_BYTE(RANDOM_LONG(2, 7)); // scale x1.0
		WRITE_BYTE(RANDOM_LONG(10, 20)); // framerate
	MESSAGE_END();

	SetThink(&CExplosiveBarrel::Ignite);
	SetNextThink(0.1);
}

void CExplosiveBarrel::Explode(void)
{
	Vector vecOrigin = GetAbsOrigin();

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecOrigin);
		WRITE_BYTE(TE_BREAKMODEL);

		// position
		WRITE_COORD(vecOrigin.x);
		WRITE_COORD(vecOrigin.y);
		WRITE_COORD(vecOrigin.z);
		// size
		WRITE_COORD(150); // this doesn't seem to do anything
		WRITE_COORD(150);
		WRITE_COORD(150);
		// velocity
		WRITE_COORD(0);
		WRITE_COORD(0);
		WRITE_COORD(0);
		// randomization of the velocity
		WRITE_BYTE(25);
		// Model
		WRITE_SHORT( g_sModelIndexMetalGibs );	//model id#
			// # of shards
		WRITE_BYTE(RANDOM_LONG(20, 30));
		// duration
		WRITE_BYTE(20);
		// flags
		WRITE_BYTE(BREAK_METAL);
	MESSAGE_END();

	vecOrigin.z += 16;
	KeyValueData kvd;
	CBaseEntity* pExplosion = CBaseEntity::Create("env_explosion", vecOrigin, g_vecZero, NULL);
	kvd.szKeyName = "iMagnitude";
	kvd.szValue = "40";
	pExplosion->KeyValue(&kvd);
	pExplosion->pev->spawnflags |= (1 << 0); // <-- SF_ENVEXPLOSION_NODAMAGE
	pExplosion->pev->scale = 2;
	pExplosion->Spawn();
	pExplosion->SetThink(&CBaseEntity::SUB_CallUseToggle);
	pExplosion->pev->nextthink = gpGlobals->time;

	if( m_hOwner )
		RadiusDamage( pev, m_hOwner->pev, pev->dmg, 300, CLASS_NONE, DMG_BLAST );
	else
		RadiusDamage(pev, pev, pev->dmg, 300, CLASS_NONE, DMG_BLAST);

	m_hOwner = NULL;

	UTIL_ScreenShake(pev->origin, 10.0, 150.0, 1.0, 1000, true);

	if( HasSpawnFlags(SF_BARREL_RESPAWNABLE) )
	{
		pev->effects |= EF_NODRAW;
		pev->solid = SOLID_NOT;
		pev->health = 0;
		pev->takedamage = DAMAGE_NO;
		ClearEffects();
		ObjectCaps();
		SetThink( &CExplosiveBarrel::RespawnBarrel );
		SetNextThink( RespawnTime );
	}
	else
		UTIL_Remove(this);
}

void CExplosiveBarrel::ClearEffects(void)
{
	pev->effects &= ~EF_DIMLIGHT;
	STOP_SOUND(ENT(pev), CHAN_STATIC, "ambience/fire_burning.wav");
	IsOnFire = false;
	pev->renderfx = 0;
}