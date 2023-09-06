/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/

#ifndef HGRUNT_H
#define HGRUNT_H

#define HGRUNT_FLASHLIGHT_SPR "sprites/blueflare1.spr"

// monster_andrew_grunt
#define MAX_ANDREW_SPAWNS 10
#define ACCUMULATED_DMG_THRESHOLD 500
#define ANDREW_GRUNT_MODEL "models/npc/andrewgrunt.mdl"

//=========================================================
// hgrunt
//=========================================================
class CHGrunt : public CSquadMonster
{
	DECLARE_CLASS( CHGrunt, CSquadMonster );
public:
	DECLARE_DATADESC();
	virtual void Spawn(void);
	virtual void Precache(void);
	void SetYawSpeed(void);
	virtual int  Classify(void);
	int ISoundMask(void);
	virtual void HandleAnimEvent(MonsterEvent_t *pEvent);
	void KeyValue( KeyValueData *pkvd );
	BOOL FCanCheckAttacks(void);
	BOOL CheckMeleeAttack1(float flDot, float flDist);
	BOOL CheckRangeAttack1(float flDot, float flDist);
	BOOL CheckRangeAttack2(float flDot, float flDist);
	BOOL CheckRangeAttack2Impl( float grenadeSpeed, float flDot, float flDist );
	void CheckAmmo(void);
	void SetActivity(Activity NewActivity);
	void StartTask(Task_t *pTask);
	void RunTask(Task_t *pTask);

	bool BodyTurn( const Vector &vecTarget );
	bool RunningShooting; // if true, we are firing bullets while running

	virtual void DeathSound(void);
	virtual void PainSound(void);
	virtual void IdleSound(void);
	Vector GetGunPosition(void);
	void Shoot(void);
	void Shotgun(void);
	void RunAI( void );
	void PrescheduleThink(void);
	virtual void GibMonster(void);
	virtual void SpeakSentence(void);
	void ClearEffects(void);
	int CanInvestigate;

	float AttackStartTime; // once the enemy is in sight, grunt began range attack, but he needs to wait, so...
	float CombatWaitTime; // ...grunt waits for # seconds before shooting.

	int GruntShotgunDamage;

	string_t	wpns;
	bool CanSpawnDrone;
	int DroneSpawned;
	int m_iFlashlightCap;
	CSprite *FlashlightSpr;
	//monster_alien_soldier:
	int AlternateShoot;
	CSprite *AlienEye;

//	int	Save(CSave &save);
//	int Restore(CRestore &restore);

	CBaseEntity	*Kick(void);
	Schedule_t	*GetSchedule(void);
	Schedule_t  *GetScheduleOfType(int Type);
	void TraceAttack(entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	int TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType);

	int IRelationship(CBaseEntity *pTarget);

	virtual BOOL FOkToSpeak(void);
	void JustSpoke(void);

	CUSTOM_SCHEDULES;
//	static TYPEDESCRIPTION m_SaveData[];

	virtual int SizeForGrapple() { return GRAPPLE_MEDIUM; }

	// checking the feasibility of a grenade toss is kind of costly, so we do it every couple of seconds,
	// not every server frame.
	float m_flNextGrenadeCheck;
	float m_flNextPainTime;
	float m_flLastEnemySightTime;

	Vector	m_vecTossVelocity;

	BOOL	m_fThrowGrenade;
	BOOL	m_fStanding;
	BOOL	m_fFirstEncounter;// only put on the handsign show in the squad's first encounter.
	int		m_cClipSize;

	int		m_voicePitch;

	int		m_iBrassShell;
	int		m_iShotgunShell;

	int		m_iSentence;

	static const char *pGruntSentences[];

	// monster_andrew_grunt
	float AndrewLastHurt;
	bool AndrewDash;
	float AndrewDashTime;
	Vector AndrewRespawnPoint[MAX_ANDREW_SPAWNS]; // an array of origins of spawn points (collected on spawn)
	Vector AndrewEscapePoint; // the origin of an escape point
	int RespawnPoints; // total number of escape points found
	bool AndrewHidden; // escaped to remote location
	float AndrewEscapeTime; // next time in the future when he will be able to escape again
	float AndrewHidingTime; // sets when Andrew is escaped
	int AccumulatedDamage; // he will try to escape after total damage exceeds this amount, then start over
};

//=========================================================
// CHGruntRepel - when triggered, spawns a monster_human_grunt
// repelling down a line.
//=========================================================
/*
class CHGruntRepel : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void EXPORT RepelUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int m_iSpriteTexture;	// Don't save, precache
	virtual const char* TrooperName();
	virtual void PrepareBeforeSpawn(CBaseEntity* pEntity);
};
*/
#endif // HGRUNT_H