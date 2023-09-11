#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "triggers.h"

//==========================================================================
//diffusion - energy ball, prop_combine_ball replica
//==========================================================================
#define ENVBALL_SILENT			BIT(0)  // no sounds (spawn, flying, hit, explode), no screen shakes
#define ENVBALL_NOLIGHTEFFECTS	BIT(1)  // do not emit light on spawn, bounce and explosion
#define ENVBALL_STARTON			BIT(2)  // fire on map spawn
#define ENVBALL_RANDOMDIR		BIT(3)  // fly in random direction, ignore angles
#define ENVBALL_REFIRE			BIT(4)  // automatically refire after delay
#define ENVBALL_DIEONMONSTER	BIT(5)  // explode if touched the monster or player
#define ENVBALL_ALLOWWATER		BIT(6)	// do not explode in the water
#define ENVBALL_TAIL			BIT(7)  // rocket trail effect

#define ENVBALL_ALIENSHIP		BIT(12) // not for mapping

class CEnvBall : public CBaseDelay
{
	DECLARE_CLASS(CEnvBall, CBaseDelay);
public:
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void KeyValue(KeyValueData* pkvd);
	int ObjectCaps(void) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void Precache(void);
	void Spawn(void);
	void Wait(void);

	CBaseEntity* pBall;

	int InitialVelocity; // initial velocity of the ball

	int MaxBalls; // total number of balls that can be fired
	int SpawnedBalls;
	int ActiveBalls; // limit of active balls at once

	int m_iTrail;

	DECLARE_DATADESC();
};
LINK_ENTITY_TO_CLASS(env_ball, CEnvBall);

BEGIN_DATADESC(CEnvBall)
	DEFINE_KEYFIELD(InitialVelocity, FIELD_INTEGER, "startvelocity"),
	DEFINE_KEYFIELD(MaxBalls, FIELD_INTEGER, "maxballs"),
	DEFINE_KEYFIELD(ActiveBalls, FIELD_INTEGER, "maxactiveballs" ),
	DEFINE_FIELD(pBall, FIELD_CLASSPTR),
	DEFINE_FIELD(SpawnedBalls, FIELD_INTEGER),
	DEFINE_FUNCTION(Wait),
END_DATADESC();

void CEnvBall::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "startvelocity"))
	{
		InitialVelocity = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "maxballs"))
	{
		MaxBalls = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "maxactiveballs" ) )
	{
		ActiveBalls = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue(pkvd);
}

void CEnvBall::Precache(void)
{
	UTIL_PrecacheOther("env_ballentity");

	if (!pev->spawnflags & ENVBALL_SILENT)
		PRECACHE_SOUND("comball/spawn.wav");

	m_iTrail = PRECACHE_MODEL( "sprites/smoke.spr" );
}

void CEnvBall::Spawn(void)
{
	if (pev->spawnflags & ENVBALL_STARTON)
	{
		SetThink(&CBaseEntity::SUB_CallUseToggle);
		SetNextThink(0.25);
	}

	if (m_flWait <= 0)
		m_flWait = 0.2;

	if (MaxBalls > 0)
		SpawnedBalls = 0;
	else
		SpawnedBalls = -1;

	pev->scale = 1;

	Precache();
}

void CEnvBall::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!pActivator || !pActivator->IsPlayer())
		pActivator = CBaseEntity::Instance(INDEXENT(1));

	CBasePlayer* pPlayer = (CBasePlayer*)pActivator;

	if (IsLockedByMaster(pActivator))
		return;

	if (pev->nextthink > gpGlobals->time)
		return;

	if( ActiveBalls > 0 ) // mapper has set to track active balls count
	{
		if( m_iCounter >= ActiveBalls )
		{
			if( HasSpawnFlags(ENVBALL_REFIRE) && (MaxBalls != SpawnedBalls) )
			{
				// try again later
				SetThink( &CBaseEntity::SUB_CallUseToggle );
				SetNextThink( m_flWait );
			}

			return;
		}
	}

	//--------------------------------------------------------------------------
	Vector SpawnAngles = GetAbsAngles();
	if (HasSpawnFlags(ENVBALL_RANDOMDIR))
	{
		SpawnAngles.x = RANDOM_LONG(0, 359);
		SpawnAngles.y = RANDOM_LONG(0, 359);
		SpawnAngles.z = RANDOM_LONG(0, 359);
	}

	CBaseEntity* pBall = Create("env_ballentity", GetAbsOrigin(), SpawnAngles, edict());

	if (!pBall)
		return;

	if (MaxBalls) // keep track of how many balls are spawned (if set)
		SpawnedBalls++;

	m_iCounter++; // increase counter of active balls. when ball dies/disappears, it will decrease this value.

	//--------------------------------------------------------------------------

	if (HasSpawnFlags(ENVBALL_SILENT))
		pBall->pev->spawnflags |= ENVBALL_SILENT;
	else
	{
		EMIT_SOUND( edict(), CHAN_STATIC, "comball/spawn.wav", 1, ATTN_NORM );
		EMIT_SOUND_DYN( pBall->edict(), CHAN_BODY, "comball/fly_loop.wav", 0.5, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ) );
	}

	if ( !HasSpawnFlags(ENVBALL_NOLIGHTEFFECTS) )
	{
		MESSAGE_BEGIN(MSG_PVS, gmsgTempEnt, pev->origin);
			WRITE_BYTE(TE_DLIGHT);
			WRITE_COORD(pev->origin.x);	// X
			WRITE_COORD(pev->origin.y);	// Y
			WRITE_COORD(pev->origin.z);	// Z
			WRITE_BYTE(30);		// radius * 0.1
			WRITE_BYTE(pev->rendercolor.x);		// r
			WRITE_BYTE(pev->rendercolor.y);		// g
			WRITE_BYTE(pev->rendercolor.z);		// b
			WRITE_BYTE(25);		// time * 10
			WRITE_BYTE(25);		// decay * 0.1
			WRITE_BYTE( 100 ); // brightness
			WRITE_BYTE( 1 ); // shadows
		MESSAGE_END();
	}
	else
		pBall->pev->spawnflags |= ENVBALL_NOLIGHTEFFECTS;

	if (pev->spawnflags & ENVBALL_DIEONMONSTER)
		pBall->pev->spawnflags |= ENVBALL_DIEONMONSTER;

	if (pev->spawnflags & ENVBALL_ALLOWWATER)
		pBall->pev->spawnflags |= ENVBALL_ALLOWWATER;

	//--------------------------------------------------------------------------

	// set initial velocity of the ball
	UTIL_MakeVectors( SpawnAngles );

	if (!InitialVelocity)
		pBall->SetAbsVelocity(gpGlobals->v_forward * RANDOM_LONG(100, 2500));
	else
	{
		if (InitialVelocity > 3000)
			InitialVelocity = 3000;
		if (InitialVelocity < 31) // it will explode if 30. less then 30 can result in ball being stuck
			InitialVelocity = 31;
		pBall->SetAbsVelocity(gpGlobals->v_forward * InitialVelocity);
	}

	// set the same name in case we want to track it or remove it
	pBall->pev->targetname = pev->targetname;

	// set properties
	if( pev->rendermode )
		pBall->pev->rendermode = pev->rendermode;
	if( pev->renderfx )
		pBall->pev->renderfx = pev->renderfx;
	if( pev->renderamt )
		pBall->pev->renderamt = pev->renderamt;

	if (!pev->rendercolor || (pev->rendercolor == g_vecZero))
	{
		pev->rendercolor = Vector(252, 255, 215);
		pBall->pev->rendercolor = pev->rendercolor;
	}
	else
		pBall->pev->rendercolor = pev->rendercolor;

	// set lifetime
	if (pev->iuser2) // bounce limit
		pBall->pev->iuser2 = pev->iuser2;
	if (pev->fuser1) // time limit
		pBall->pev->fuser1 = pev->fuser1;

	// set damage
	if (pev->fuser2)
		pBall->pev->fuser2 = pev->fuser2;

	if( HasSpawnFlags(ENVBALL_TAIL) )
	{
		// create trail
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY );
			WRITE_BYTE( TE_BEAMFOLLOW );
			WRITE_SHORT( pBall->entindex() );	// entity
			WRITE_SHORT(m_iTrail );	// model
			WRITE_BYTE( 6 ); // life
			WRITE_BYTE( 5 );  // width
			WRITE_BYTE( pev->rendercolor.x );   // r
			WRITE_BYTE( pev->rendercolor.y );   // g
			WRITE_BYTE( pev->rendercolor.z );   // b
			WRITE_BYTE( 15 );	// brightness
		MESSAGE_END();
	}

	//------------------------------------------------------------------

	// spawn another ball after delay
	if ((pev->spawnflags & ENVBALL_REFIRE) && (MaxBalls != SpawnedBalls))
	{
		SetThink(&CBaseEntity::SUB_CallUseToggle);
		SetNextThink(m_flWait);
	}
	else if (MaxBalls == SpawnedBalls)
	{
		SpawnedBalls = 0;
		SetThink(NULL);
		SetNextThink(m_flWait);
	}
	else if (m_flWait > 0)
	{
		SetThink(&CEnvBall::Wait);
		SetNextThink(m_flWait);
	}
}

void CEnvBall::Wait(void)
{
	DontThink();
}

//==========================================================================
// diffusion - energy ball itself, env_ball spawns this. not for mapping
//==========================================================================
class CEnvBallEntity : public CBaseMonster
{
	DECLARE_CLASS(CEnvBallEntity, CBaseMonster);
public:
	void Spawn(void);
	void Precache(void);
	void AnimateThink(void);
	void BounceTouch(CBaseEntity* pOther);
	void ClearEffects(void);
	void Explode(void);
	virtual BOOL IsProjectile( void ) { return TRUE; }

	Vector m_vecIdeal;
	int Bounced; // how many times did it bounce?
	int SpriteExplosion;
	float m_maxFrame;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(env_ballentity, CEnvBallEntity);

BEGIN_DATADESC(CEnvBallEntity)
	DEFINE_FUNCTION(AnimateThink),
	DEFINE_FUNCTION(BounceTouch),
	DEFINE_FUNCTION(Explode),
	DEFINE_FIELD(Bounced, FIELD_INTEGER),
END_DATADESC()

void CEnvBallEntity::Spawn(void)
{
	Precache();

	pev->movetype = MOVETYPE_BOUNCEMISSILE;
	pev->solid = SOLID_BBOX;

	pev->scale = 0.1;
	pev->framerate = 100;
	pev->frame = 0;

	if( !pev->iuser4) SetFadeDistance(4000); // fadedistance
	SetFlag(F_NOBACKCULL);

	SET_MODEL(ENT(pev), "sprites/comball.spr");
	m_maxFrame = (float)MODEL_FRAMES( pev->modelindex ) - 1;

	if (!pev->rendermode)
		pev->rendermode = kRenderTransAdd;
	if (!pev->renderamt)
		pev->renderamt = 150;
	if (!pev->rendercolor)
		pev->rendercolor = Vector(252, 255, 215);
	if (!pev->iuser2)
		pev->iuser2 = 0; // bounce limit: unlimited by default
	if (!pev->fuser1)
		pev->fuser1 = 0; // time limit: unlimited by default
	if (!pev->fuser2)
		pev->fuser2 = 0; // damage: no damage by default

//	UTIL_SetSize(pev, Vector(-4, -4, -4), Vector(4, 4, 4));
		UTIL_SetSize(pev, g_vecZero, g_vecZero);

	SetTouch(&CEnvBallEntity::BounceTouch);

	m_vecIdeal = Vector(0, 0, 0);

	if( pev->owner )
		m_hOwner = Instance(pev->owner);
	pev->dmgtime = gpGlobals->time; // keep track of when ball spawned

	Bounced = 0; // haven't bounced yet

	SetThink( &CEnvBallEntity::AnimateThink );
	SetNextThink( 0.1 );
}

void CEnvBallEntity::Precache(void)
{
	if (pev->model)
		PRECACHE_MODEL((char*)STRING(pev->model));
	else
		PRECACHE_MODEL("sprites/comball.spr");

	if (!(pev->spawnflags & ENVBALL_SILENT))
	{
		PRECACHE_SOUND("comball/fly_loop.wav");
		PRECACHE_SOUND("comball/bounce1.wav");
		PRECACHE_SOUND("comball/bounce2.wav");
		PRECACHE_SOUND("comball/explosion.wav");
		PRECACHE_SOUND( "comball/bounce_hit.wav" );
	}

	SpriteExplosion = PRECACHE_MODEL("sprites/white.spr");
}

void CEnvBallEntity::AnimateThink(void)
{
	if (m_hOwner == NULL)
		pev->owner = NULL;

	if ((pev->fuser1 > 0) && (gpGlobals->time - pev->dmgtime > pev->fuser1))
	{
		Explode();
		return;
	}

	if (GetAbsVelocity().Length() < 30)
	{
		Explode();
		return;
	}

	if (!HasSpawnFlags(ENVBALL_ALLOWWATER) && (pev->waterlevel != 0))
	{
		Explode();
		return;
	}

	// animate
	float frames = pev->framerate * gpGlobals->frametime;
	if( m_maxFrame > 0 )
		pev->frame = fmod( pev->frame + frames, m_maxFrame );

	SetNextThink(0);
}

void CEnvBallEntity::BounceTouch(CBaseEntity* pOther)
{
	// balls touched. this is gay. delete.
	if (FClassnameIs(pOther->pev, "env_ballentity") || FClassnameIs(pOther->pev, "shootball") || FClassnameIs(pOther->pev, "playerball"))
	{
		// HACKHACK when two balls collide, they execute the same code so should be both deleted
		// however one of them always stays alive. so we need to use this "hack"
		UTIL_Remove(pOther);
		Explode();
		return;
	}

	UTIL_DoSparkNoSound(pev, GetAbsOrigin());

	// keep track of bounces
	if (pev->iuser2)
	{
		Bounced++;
		if (Bounced > pev->iuser2)
		{
			Explode();
			return;
		}
	}
	else
		Bounced = -1;

	if (!(pev->spawnflags & ENVBALL_NOLIGHTEFFECTS) && (Bounced <= pev->iuser2))
	{
		TraceResult tr = UTIL_GetGlobalTrace();

		Vector vecOrg = GetAbsOrigin() + tr.vecPlaneNormal * 3; // pull out the templight a bit from the touched surface

		MESSAGE_BEGIN(MSG_PVS, gmsgTempEnt, vecOrg);
			WRITE_BYTE(TE_DLIGHT);
			WRITE_COORD(vecOrg.x);	// X
			WRITE_COORD(vecOrg.y);	// Y
			WRITE_COORD(vecOrg.z);	// Z
			WRITE_BYTE(10);		// radius * 0.1
			WRITE_BYTE(pev->rendercolor.x);		// r
			WRITE_BYTE(pev->rendercolor.y);		// g
			WRITE_BYTE(pev->rendercolor.z);		// b
			WRITE_BYTE(10);		// time * 10
			WRITE_BYTE(15);		// decay * 0.1
			WRITE_BYTE( 100 ); // brightness
			WRITE_BYTE( 0 ); // shadows
		MESSAGE_END();
	}

	if (pOther->IsPlayer() || pOther->IsMonster())
	{
		if (HasSpawnFlags(ENVBALL_DIEONMONSTER))
		{
			Explode();
			return;
		}
		else
		{
			// with this code you can push the ball with your body, but it can be buggy...
			// UPD: FIXME I feel some lines can be unnecessary. It works, but must be looked into
			Vector m_vecIdeal = pOther->GetAbsVelocity();
			m_vecIdeal = m_vecIdeal + (m_vecIdeal - GetAbsOrigin()).Normalize() * 100;
			SetAbsVelocity(m_vecIdeal);
			Vector vecDir = m_vecIdeal.Normalize();
			TraceResult tr = UTIL_GetGlobalTrace();
			float n = -DotProduct(tr.vecPlaneNormal, vecDir);
			vecDir = 2.0 * tr.vecPlaneNormal * n + vecDir;
			m_vecIdeal = vecDir * m_vecIdeal.Length();
			m_vecIdeal.z += 2; // so it won't stick to surface...

			if (pev->fuser2 > 0)
			{
				if (!HasSpawnFlags(ENVBALL_SILENT))
					EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "comball/bounce_hit.wav", 1, 0.4, 0, RANDOM_LONG( 90, 110 ) );
			}
		}
	}
	else
	{
		if( !HasSpawnFlags( ENVBALL_SILENT ) && (Bounced <= pev->iuser2) )
		{
			switch( RANDOM_LONG( 0, 1 ) )
			{
			case 0: EMIT_SOUND_DYN( ENT( pev ), CHAN_STATIC, "comball/bounce1.wav", 1, 0.4, 0, 100 + RANDOM_LONG( -10, 10 ) ); break; // ...channel, wav file, volume, radius, flags, pitch
			case 1: EMIT_SOUND_DYN( ENT( pev ), CHAN_STATIC, "comball/bounce2.wav", 1, 0.4, 0, 100 + RANDOM_LONG( -10, 10 ) ); break;
			}
		}
	}

	if (GetAbsVelocity().Length() > CVAR_GET_FLOAT("sv_maxvelocity"))
		pev->velocity *= 0.9; // ¯\_(ツ)_/¯

	if (pev->fuser2 > 0)
	{
		entvars_t* pevOwner;
		if (pev->owner)
			pevOwner = VARS(pev->owner);
		else
			pevOwner = NULL;

		pev->owner = NULL; // can't traceline attack owner if this is set
	//	RadiusDamage(GetAbsOrigin(), pev, pevOwner, pev->fuser2, pev->fuser2 * 2.5, CLASS_NONE, DMG_SHOCK);
		pOther->TakeDamage( pev, pevOwner, pev->fuser2, DMG_SHOCK );
	}

	if (!(pev->spawnflags & ENVBALL_SILENT))
		UTIL_ScreenShake(pev->origin, 5.0, 150.0, 0.5, 500, true);
}
/*
void CEnvBallEntity::Slowdown(void)
{
	Vector curVelocity = GetAbsVelocity();

	if (curVelocity.Length() < 300)
	{
		pev->nextthink = -1;
		return;
	}
	else
	{
		curVelocity *= 0.9f;
		SetAbsVelocity( curVelocity );
		SetThink(Slowdown);
		SetNextThink(0.1);
	}
}
*/
void CEnvBallEntity::ClearEffects(void)
{
	STOP_SOUND(ENT(pev), CHAN_BODY, "comball/fly_loop.wav");
	
	// notify our owner env_ball that we died
	if( m_hOwner != NULL )
		m_hOwner->m_iCounter--;
}

void CEnvBallEntity::Explode(void)
{
	if (!(pev->spawnflags & ENVBALL_NOLIGHTEFFECTS))
	{
		// light
		TraceResult tr = UTIL_GetGlobalTrace();
		Vector vecOrg = GetAbsOrigin() + tr.vecPlaneNormal * 3;

		MESSAGE_BEGIN(MSG_PVS, gmsgTempEnt, vecOrg);
		WRITE_BYTE(TE_DLIGHT);
			WRITE_COORD(vecOrg.x);	// X
			WRITE_COORD(vecOrg.y);	// Y
			WRITE_COORD(vecOrg.z);	// Z
			WRITE_BYTE(35);		// radius * 0.1
			WRITE_BYTE(255);		// r
			WRITE_BYTE(255);		// g
			WRITE_BYTE(255);		// b			
			WRITE_BYTE(30);		// time * 10
			WRITE_BYTE(30);		// decay * 0.1
			WRITE_BYTE( 150 ); // brightness
			WRITE_BYTE( 1 ); // shadows
		MESSAGE_END();
	}

	// blast effect
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
		WRITE_BYTE(TE_BEAMCYLINDER);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z + RANDOM_LONG(500, 1000)); // reach damage radius over .2 seconds
		WRITE_SHORT(SpriteExplosion);
		WRITE_BYTE(0); // startframe
		WRITE_BYTE(0); // framerate
		WRITE_BYTE(4); // life
		WRITE_BYTE(16);  // width
		WRITE_BYTE(0);   // noise
		WRITE_BYTE(255);   // r, g, b
		WRITE_BYTE(255);   // r, g, b
		WRITE_BYTE(255);   // r, g, b
		WRITE_BYTE(120); // brightness
		WRITE_BYTE(0);		// speed
	MESSAGE_END();

	// sparks
	Create("spark_shower", pev->origin, g_vecZero, NULL);

	// damage
	if (pev->fuser2 > 0)
	{
		entvars_t* pevOwner;
		if (pev->owner)
			pevOwner = VARS(pev->owner);
		else
			pevOwner = NULL;

		pev->owner = NULL; // can't traceline attack owner if this is set
		RadiusDamage(GetAbsOrigin(), pev, pevOwner, pev->fuser2, pev->fuser2 * 2.5, CLASS_NONE, DMG_SHOCK);
	}

	UTIL_DoSparkNoSound(pev, pev->origin);

	if (!(pev->spawnflags & ENVBALL_SILENT))
	{
		UTIL_ScreenShake(pev->origin, 10.0, 150.0, 1.0, 2000, true);
		EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "comball/explosion.wav", 1, 0.3, 0, 100 + RANDOM_LONG(-10, 10));
	}

	SetTouch(NULL);
	UTIL_Remove(this);
}


//==========================================================================
//diffusion - energy ball used by monster_alien_soldier, minor changes
//==========================================================================

class CEnvBallEntitySoldier : public CEnvBallEntity
{
	DECLARE_CLASS(CEnvBallEntitySoldier, CEnvBallEntity);
public:
	void Spawn(void);
	void BounceTouch(CBaseEntity* pOther);
	void Explode(void);
	void AnimateThink(void);
	virtual BOOL IsProjectile( void ) { return TRUE; }

	int GotOwnerClass; // we need to get the class of our owner, so we don't hurt our allies

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(shootball, CEnvBallEntitySoldier);

BEGIN_DATADESC(CEnvBallEntitySoldier)
	DEFINE_FUNCTION(AnimateThink),
	DEFINE_FUNCTION(BounceTouch),
	DEFINE_FUNCTION(Explode),
	DEFINE_FIELD(Bounced, FIELD_INTEGER),
END_DATADESC()

void CEnvBallEntitySoldier::AnimateThink(void)
{
	if (m_hOwner == NULL)
		pev->owner = NULL;
	else
	{
		if (GotOwnerClass == 0)
		{
			m_iClass = m_hOwner->Classify();
			GotOwnerClass = 1;
		}
	}

	if ((pev->fuser1 > 0) && (gpGlobals->time - pev->dmgtime > pev->fuser1))
	{
		Explode();
		return;
	}

	if (GetAbsVelocity().Length() < 30)
	{
		Explode();
		return;
	}

	if (pev->waterlevel != 0)
	{
		Explode();
		return;
	}

	// animate
	float frames = pev->framerate * gpGlobals->frametime;
	if( m_maxFrame > 0 )
		pev->frame = fmod( pev->frame + frames, m_maxFrame );

	SetNextThink(0);
}

void CEnvBallEntitySoldier::Spawn(void)
{
	Precache();

	pev->movetype = MOVETYPE_BOUNCEMISSILE;
	pev->solid = SOLID_BBOX;

	pev->scale = 0.1;

	//	if( !m_iClass )
	//		m_iClass = CLASS_ALIEN_MILITARY;

	SET_MODEL(ENT(pev), "sprites/comball.spr");
	m_maxFrame = (float)MODEL_FRAMES( pev->modelindex ) - 1;

	pev->rendermode = kRenderTransAdd;
	pev->renderamt = 150;
	pev->rendercolor = Vector(252, 255, 215);
	pev->iuser2 = 10; // bounce limit
	pev->fuser1 = 5; // time limit: 5 seconds
	pev->fuser2 = 60; // damage

	UTIL_SetSize(pev, Vector(-4, -4, -4), Vector(4, 4, 4));

	SetThink(&CEnvBallEntitySoldier::AnimateThink);
	SetTouch(&CEnvBallEntitySoldier::BounceTouch);

	m_vecIdeal = Vector(0, 0, 0);

	if (pev->owner)
	{
		m_hOwner = Instance(pev->owner);
		m_iClass = m_hOwner->Classify();
	}
	pev->dmgtime = gpGlobals->time; // keep track of when ball spawned
	pev->nextthink = gpGlobals->time + 0.1;

	Bounced = 0; // haven't bounced yet

	// light
	MESSAGE_BEGIN(MSG_PVS, gmsgTempEnt, pev->origin);
		WRITE_BYTE(TE_DLIGHT);
			WRITE_COORD(pev->origin.x);	// X
			WRITE_COORD(pev->origin.y);	// Y
			WRITE_COORD(pev->origin.z);	// Z
			WRITE_BYTE(30);		// radius * 0.1
			WRITE_BYTE(pev->rendercolor.x);		// r
			WRITE_BYTE(pev->rendercolor.y);		// g
			WRITE_BYTE(pev->rendercolor.z);		// b
			WRITE_BYTE(25);		// time * 10
			WRITE_BYTE(25);		// decay * 0.1
			WRITE_BYTE( 100 ); // brightness
			WRITE_BYTE( 1 ); // shadows
		MESSAGE_END();

	EMIT_SOUND_DYN(edict(), CHAN_BODY, "comball/fly_loop.wav", 0.5, ATTN_NORM, 0, 100 + RANDOM_LONG(-5, 5));

	SetFadeDistance( 4000 ); // fadedistance

	pev->framerate = 100;
	pev->frame = 0;
}

void CEnvBallEntitySoldier::BounceTouch(CBaseEntity* pOther)
{
	if (HasSpawnFlags(ENVBALL_ALIENSHIP) )
	{
		Explode();
		return;
	}

	if (FClassnameIs(pOther->pev, "env_ballentity") || FClassnameIs(pOther->pev, "shootball") || FClassnameIs(pOther->pev, "playerball"))
	{
		// when two balls collide, they execute the same code so should be both deleted
		// however one of them always stays alive. so we need to use this "hack"
		UTIL_Remove(pOther);
		Explode();
		return;
	}

	UTIL_DoSparkNoSound(pev, GetAbsOrigin());

	pev->velocity *= 1.1;

	Bounced++;
	if (Bounced > pev->iuser2)
	{
		Explode();
		return;
	}

	switch (RANDOM_LONG(0, 1))
	{
	case 0: EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "comball/bounce1.wav", 1, 0.4, 0, 100 + RANDOM_LONG(-10, 10)); break; // ...channel, wav file, volume, radius, flags, pitch
	case 1: EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "comball/bounce2.wav", 1, 0.4, 0, 100 + RANDOM_LONG(-10, 10)); break;
	}

	TraceResult tr = UTIL_GetGlobalTrace();
	Vector vecOrg = GetAbsOrigin() + tr.vecPlaneNormal * 3;

	MESSAGE_BEGIN(MSG_PVS, gmsgTempEnt, vecOrg);
	WRITE_BYTE(TE_DLIGHT);
		WRITE_COORD(vecOrg.x);	// X
		WRITE_COORD(vecOrg.y);	// Y
		WRITE_COORD(vecOrg.z);	// Z
		WRITE_BYTE(10);		// radius * 0.1
		WRITE_BYTE(pev->rendercolor.x);		// r
		WRITE_BYTE(pev->rendercolor.y);		// g
		WRITE_BYTE(pev->rendercolor.z);		// b
		WRITE_BYTE(10);		// time * 10
		WRITE_BYTE(15);		// decay * 0.1
		WRITE_BYTE( 100 ); // brightness
		WRITE_BYTE( 0 ); // shadows
	MESSAGE_END();

	if ((pOther->IsMonster() || (pOther->IsPlayer())))
	{
		if (pOther->Classify() == m_iClass)
		{
			// do nothing
		}
		else
		{
			Explode();
			return;
		}
	}

	UTIL_ScreenShake(pev->origin, 5.0, 150.0, 0.5, 500, true);
}

void CEnvBallEntitySoldier::Explode(void)
{
	// light
	TraceResult tr = UTIL_GetGlobalTrace();
	Vector vecOrg = GetAbsOrigin() + tr.vecPlaneNormal * 3;

	MESSAGE_BEGIN(MSG_PVS, gmsgTempEnt, vecOrg);
		WRITE_BYTE(TE_DLIGHT);
		WRITE_COORD(vecOrg.x);	// X
		WRITE_COORD(vecOrg.y);	// Y
		WRITE_COORD(vecOrg.z);	// Z
		WRITE_BYTE(35);		// radius * 0.1
		WRITE_BYTE(255);		// r			
		WRITE_BYTE(255);		// g
		WRITE_BYTE(255);		// b			
		WRITE_BYTE(30);		// time * 10
		WRITE_BYTE(30);		// decay * 0.1
		WRITE_BYTE( 150 ); // brightness
		WRITE_BYTE( 1 ); // shadows
	MESSAGE_END();

	// blast effect
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
		WRITE_BYTE(TE_BEAMCYLINDER);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z + RANDOM_LONG(500, 1000));
		WRITE_SHORT(SpriteExplosion);
		WRITE_BYTE(0); // startframe
		WRITE_BYTE(0); // framerate
		WRITE_BYTE(4); // life
		WRITE_BYTE(16);  // width
		WRITE_BYTE(0);   // noise
		WRITE_BYTE(255);   // r, g, b
		WRITE_BYTE(255);   // r, g, b
		WRITE_BYTE(255);   // r, g, b
		WRITE_BYTE(120); // brightness
		WRITE_BYTE(0);		// speed
	MESSAGE_END();

	// sparks
	Create("spark_shower", pev->origin, g_vecZero, NULL);
	// damage
	entvars_t* pevOwner;
	if (pev->owner)
		pevOwner = VARS(pev->owner);
	else
		pevOwner = NULL;

	pev->owner = NULL; // can't traceline attack owner if this is set
	RadiusDamage(GetAbsOrigin(), pev, pevOwner, pev->fuser2, pev->fuser2 * 2.5, m_iClass, DMG_SHOCK);

	UTIL_DoSparkNoSound(pev, pev->origin);

	UTIL_ScreenShake(GetAbsOrigin(), 10.0, 150.0, 1.0, 2000, true);
	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "comball/explosion.wav", 1, 0.3, 0, 100 + RANDOM_LONG(-10, 10));

	SetTouch(NULL);
	UTIL_Remove(this);
}

//==========================================================================
//diffusion - energy ball used by player (weapon_ar2), minor changes
//==========================================================================

class CEnvBallEntityPlayer : public CEnvBallEntity
{
	DECLARE_CLASS(CEnvBallEntityPlayer, CEnvBallEntity);
public:
	void Spawn(void);
	void BounceTouch(CBaseEntity* pOther);
	void Explode(void);
	void AnimateThink(void);
	virtual BOOL IsProjectile( void ) { return TRUE; }

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS(playerball, CEnvBallEntityPlayer);

BEGIN_DATADESC(CEnvBallEntityPlayer)
	DEFINE_FUNCTION(AnimateThink),
	DEFINE_FUNCTION(BounceTouch),
	DEFINE_FUNCTION(Explode),
	DEFINE_FIELD(Bounced, FIELD_INTEGER),
END_DATADESC()

void CEnvBallEntityPlayer::AnimateThink(void)
{
	if (m_hOwner == NULL)
		pev->owner = NULL;

	SetNextThink(0);

	if ((pev->fuser1 > 0) && (gpGlobals->time - pev->dmgtime > pev->fuser1))
	{
		Explode();
		return;
	}

	if (GetAbsVelocity().Length() < 30)
	{
		Explode();
		return;
	}

	if (pev->waterlevel != 0)
	{
		Explode();
		return;
	}

	// animate
	float frames = pev->framerate * gpGlobals->frametime;
	if( m_maxFrame > 0 )
		pev->frame = fmod( pev->frame + frames, m_maxFrame );
}

void CEnvBallEntityPlayer::Spawn(void)
{
	Precache();

	pev->movetype = MOVETYPE_BOUNCEMISSILE;
	pev->solid = SOLID_BBOX;

	pev->scale = 0.1;

	SET_MODEL(ENT(pev), "sprites/comball.spr");
	m_maxFrame = (float)MODEL_FRAMES( pev->modelindex ) - 1;

	pev->rendermode = kRenderTransAdd;
	pev->renderamt = 150;
	pev->rendercolor = Vector(252, 255, 215);
	pev->iuser2 = 10; // bounce limit
	pev->fuser1 = 5; // time limit: 5 seconds
	pev->fuser2 = 200; // damage

	float flSize = 4.0f;
	UTIL_SetSize( pev, Vector( -flSize, -flSize, -flSize ), Vector( flSize, flSize, flSize ));

	SetThink(&CEnvBallEntityPlayer::AnimateThink);
	SetTouch(&CEnvBallEntityPlayer::BounceTouch);

	m_vecIdeal = Vector(0, 0, 0);

	m_hOwner = Instance(pev->owner);
	pev->dmgtime = gpGlobals->time; // keep track of when ball spawned
	pev->nextthink = gpGlobals->time + 0.1;

	Bounced = 0; // haven't bounced yet

	EMIT_SOUND_DYN(edict(), CHAN_BODY, "comball/fly_loop.wav", 0.5, ATTN_NORM, 0, 100 + RANDOM_LONG(-5, 5));

	SetFadeDistance( 4000 ); // fadedistance
	SetFlag( F_NOBACKCULL );

	pev->framerate = 100;
	pev->frame = 0;
}

void CEnvBallEntityPlayer::BounceTouch(CBaseEntity* pOther)
{
	if (FClassnameIs(pOther->pev, "env_ballentity") || FClassnameIs(pOther->pev, "shootball") || FClassnameIs(pOther->pev, "playerball")) // balls touched. this is gay. delete.
	{
		// when two balls collide, they execute the same code so should be both deleted
		// however one of them always stays alive. so we need to use this "hack"
		UTIL_Remove(pOther);
		Explode();
		return;
	}

	UTIL_DoSparkNoSound(pev, GetAbsOrigin());

	pev->velocity *= 1.1;

	Bounced++;
	if (Bounced > pev->iuser2)
	{
		Explode();
		return;
	}

	switch (RANDOM_LONG(0, 1))
	{
	case 0: EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "comball/bounce1.wav", 1, 0.4, 0, 100 + RANDOM_LONG(-10, 10)); break; // ...channel, wav file, volume, radius, flags, pitch
	case 1: EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "comball/bounce2.wav", 1, 0.4, 0, 100 + RANDOM_LONG(-10, 10)); break;
	}

	TraceResult tr = UTIL_GetGlobalTrace();
	Vector vecOrg = GetAbsOrigin() + tr.vecPlaneNormal * 3;

	MESSAGE_BEGIN(MSG_PVS, gmsgTempEnt, vecOrg);
		WRITE_BYTE(TE_DLIGHT);
		WRITE_COORD(vecOrg.x);	// X
		WRITE_COORD(vecOrg.y);	// Y
		WRITE_COORD(vecOrg.z);	// Z
		WRITE_BYTE(10);		// radius * 0.1
		WRITE_BYTE(pev->rendercolor.x);		// r
		WRITE_BYTE(pev->rendercolor.y);		// g
		WRITE_BYTE(pev->rendercolor.z);		// b
		WRITE_BYTE(10);		// time * 10
		WRITE_BYTE(15);		// decay * 0.1
		WRITE_BYTE( 100 ); // brightness
		WRITE_BYTE( 0 ); // shadows
	MESSAGE_END();

	entvars_t* pevOwner = NULL;

	if( pev->owner )
		pevOwner = VARS( pev->owner );

	if (pOther->IsMonster())
	{
		if (FClassnameIs(pOther, "monster_alien_ship") || FClassnameIs(pOther, "monster_gargantua"))
		{
			Explode();
			return;
		}
		else
		{
			pOther->TakeDamage(pev, pevOwner, pev->fuser2, DMG_SHOCK);
			Bounced += 2;

			EMIT_SOUND_DYN( ENT( pev ), CHAN_STATIC, "comball/bounce_hit.wav", 1, 0.4, 0, RANDOM_LONG( 90, 110 ) );
		}
	}
	else if ((pOther->Classify() == CLASS_PLAYER_ALLY))
	{
		// do nothing
	}
	else if( FClassnameIs( pOther, "func_breakable" ) ) // maybe pev->takedamage?
	{
		pOther->TakeDamage( pev, pevOwner, pev->fuser2, DMG_SHOCK );
		Bounced += 2;
		EMIT_SOUND_DYN( ENT( pev ), CHAN_STATIC, "comball/bounce_hit.wav", 1, 0.4, 0, RANDOM_LONG( 90, 110 ) );
	}
	else if (pOther->IsPlayer()) // multiplayer stuff
	{
		if( pevOwner )
		{
			if( pOther->edict() != pev->owner )
			{
				pOther->TakeDamage(pev, pevOwner, pev->fuser2, DMG_SHOCK);
				EMIT_SOUND_DYN( ENT( pev ), CHAN_STATIC, "comball/bounce_hit.wav", 1, 0.4, 0, RANDOM_LONG( 90, 110 ) );
				return;
			}
		}
	}

	UTIL_ScreenShake(pev->origin, 5.0, 150.0, 0.5, 500, true);
}

void CEnvBallEntityPlayer::Explode(void)
{
	// light
	TraceResult tr = UTIL_GetGlobalTrace();
	Vector vecOrg = GetAbsOrigin() + tr.vecPlaneNormal * 3;

	MESSAGE_BEGIN(MSG_PVS, gmsgTempEnt, vecOrg);
		WRITE_BYTE(TE_DLIGHT);
		WRITE_COORD(vecOrg.x);	// X
		WRITE_COORD(vecOrg.y);	// Y
		WRITE_COORD(vecOrg.z);	// Z
		WRITE_BYTE(35);		// radius * 0.1
		WRITE_BYTE(255);		// r			
		WRITE_BYTE(255);		// g
		WRITE_BYTE(255);		// b			
		WRITE_BYTE(30);		// time * 10
		WRITE_BYTE(30);		// decay * 0.1
		WRITE_BYTE( 150 ); // brightness
		WRITE_BYTE( 1 ); // shadows
	MESSAGE_END();

	// blast effect
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
		WRITE_BYTE(TE_BEAMCYLINDER);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z + RANDOM_LONG(500, 1000)); // reach damage radius over .2 seconds
		WRITE_SHORT(SpriteExplosion);
		WRITE_BYTE(0); // startframe
		WRITE_BYTE(0); // framerate
		WRITE_BYTE(4); // life
		WRITE_BYTE(16);  // width
		WRITE_BYTE(0);   // noise
		WRITE_BYTE(255);   // r, g, b
		WRITE_BYTE(255);   // r, g, b
		WRITE_BYTE(255);   // r, g, b
		WRITE_BYTE(120); // brightness
		WRITE_BYTE(0);		// speed
	MESSAGE_END();

	// sparks
	Create("spark_shower", pev->origin, g_vecZero, NULL);

	// damage
	RadiusDamage(GetAbsOrigin(), pev, VARS(pev->owner), pev->fuser2, pev->fuser2 * 2.5, CLASS_PLAYER_ALLY, DMG_SHOCK);

	UTIL_DoSparkNoSound(pev, pev->origin);

	UTIL_ScreenShake(GetAbsOrigin(), 10.0, 150.0, 1.0, 2000, true);
	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "comball/explosion.wav", 1, 0.3, 0, 100 + RANDOM_LONG(-10, 10));

	SetTouch(NULL);
	UTIL_Remove(this);
}