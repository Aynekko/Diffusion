/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#ifndef FUNC_BREAK_H
#define FUNC_BREAK_H

// func breakable
#define SF_BREAK_TRIGGER_ONLY	BIT(0)	// may only be broken by trigger
#define SF_BREAK_TOUCH			BIT(1)	// can be 'crashed through' by running player (plate glass)
#define SF_BREAK_PRESSURE		BIT(2)	// can be broken by a player standing on it
#define SF_BREAK_NOELECTROBLAST BIT(3)	// no damage from player's electroblast
#define SF_BREAK_NOBULLET		BIT(4)	// no bullet damage
#define SF_BREAK_NOEXPLOSIONS	BIT(5)	// no explosion damage
#define SF_BREAK_NOGIBS			BIT(6)	// no gibs spawning
#define SF_BREAK_CROWBAR		BIT(8)	// instant break if hit with crowbar

// func_pushable (it's also func_breakable, so don't collide with those flags)
#define SF_PUSH_BREAKABLE		BIT(7)
#define SF_PUSH_HOLDABLE		BIT(9)	// item can be picked up by player
#define SF_PUSH_BSPCOLLISION	BIT(10)	// use BSP tree instead of bbox
#define SF_PUSH_NOTSOLID		BIT(11) // not solid
#define SF_PUSH_USEPUSH			BIT(12) // diffusion - only push and pull with USE button, no touch

typedef enum { expRandom, expDirected } ExplType;
typedef enum { matGlass = 0, matWood, matMetal, matFlesh, matCinderBlock, matCeilingTile, matComputer, matUnbreakableGlass, matRocks, matNone, matLastMaterial } Materials;

#define	NUM_SHARDS 6 // this many shards spawned when breakable objects break;

class CBreakable : public CBaseDelay
{
	DECLARE_CLASS( CBreakable, CBaseDelay );
public:
	// basic functions
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData* pkvd);
	void BreakTouch( CBaseEntity *pOther );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void DamageSound( void );
	void BreakRespawn( void ); // diffusion
	float RespawnTime; // time in future when to respawn. if 0, no respawn

	// breakables use an overridden takedamage
	virtual int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
	// To spark when hit
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );

	BOOL IsBreakable( void );
	BOOL SparkWhenHit( void );

	int DamageDecal( int bitsDamageType );

	void EXPORT	Die( void );
	void Die( CBaseEntity *pActivator );
	virtual int ObjectCaps( void ) { return (CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION); }

	inline BOOL Explodable( void ) { return ExplosionMagnitude() > 0; }
	inline int ExplosionMagnitude( void ) { return pev->impulse; }
	inline void ExplosionSetMagnitude( int magnitude ) { pev->impulse = magnitude; }

	static void MaterialSoundPrecache( Materials precacheMaterial );
	static void MaterialSoundRandom( edict_t *pEdict, Materials soundMaterial, float volume );
	static const char **MaterialSoundList( Materials precacheMaterial, int &soundCount );

	static const char *pSoundsWood[];
	static const char *pSoundsFlesh[];
	static const char *pSoundsGlass[];
	static const char *pSoundsMetal[];
	static const char *pSoundsConcrete[];
	static const char *pSpawnObjects[];

	DECLARE_DATADESC();

	Materials	m_Material;
	ExplType	m_Explosion;
	int	m_idShard;
	float	m_angle;
	int	m_iszGibModel;
	int	m_iszSpawnObject;
	string_t m_iszFireOnRespawn;
};

class CPushable : public CBreakable
{
	DECLARE_CLASS(CPushable, CBreakable);
public:
	void	Spawn(void);
	void	Precache(void);
	void	Touch(CBaseEntity* pOther);
	void	Move(CBaseEntity* pMover, int push);
	void	KeyValue(KeyValueData* pkvd);
	void	Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void	OnClearParent(void) { ClearGroundEntity(); }

	virtual int ObjectCaps(void)
	{
		int flags = (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION);

		if (FBitSet(pev->spawnflags, SF_PUSH_HOLDABLE))
			flags |= FCAP_HOLDABLE_ITEM;
		else
			flags |= FCAP_CONTINUOUS_USE;

		return flags;
	}

	inline float MaxSpeed(void) { return m_maxSpeed; }

	// breakables use an overridden takedamage
	virtual int TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);
	virtual BOOL IsPushable(void) { return TRUE; }

	DECLARE_DATADESC();

	static const char* m_soundNames[3];
	int	m_lastSound;	// no need to save/restore, just keeps the same sound from playing twice in a row
	float	m_maxSpeed;
	float	m_soundTime;
};

#endif	// FUNC_BREAK_H
