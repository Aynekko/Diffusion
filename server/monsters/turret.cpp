/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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
/*

===== turret.cpp ========================================================

*/

//
// TODO: 
//		Take advantage of new monster fields like m_hEnemy and get rid of that OFFSET() stuff
//		Revisit enemy validation stuff, maybe it's not necessary with the newest monster code
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons/weapons.h"
#include "effects.h"
#include "player.h"
#include "game/gamerules.h"

extern Vector VecBModelOrigin( entvars_t* pevBModel );

#define TURRET_SHOTS	2
#define TURRET_RANGE	1200
#define TURRET_SPREAD	Vector( 0, 0, 0 )
#define TURRET_TURNRATE	30		//angles per 0.1 second
#define TURRET_MAXWAIT	15		// seconds turret will stay active w/o a target
#define TURRET_MAXSPIN	5		// seconds turret barrel will spin w/o a target
#define TURRET_MACHINE_VOLUME	0.5

typedef enum
{
	TURRET_ANIM_NONE = 0,
	TURRET_ANIM_FIRE,
	TURRET_ANIM_SPIN,
	TURRET_ANIM_DEPLOY,
	TURRET_ANIM_RETIRE,
	TURRET_ANIM_DIE,
} TURRET_ANIM;

class CBaseTurret : public CBaseMonster
{
	DECLARE_CLASS( CBaseTurret, CBaseMonster );
public:
	void Spawn(void);
	virtual void Precache(void);
	void KeyValue( KeyValueData *pkvd );
	void TurretUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	
	virtual void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	virtual int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	virtual int Classify(void);

	int BloodColor( void ) { return DONT_BLEED; }
	void GibMonster( void );	// Throw turret gibs? // diffusion - yes :)

	// Think functions

	void ActiveThink(void);
	void SearchThink(void);
	void AutoSearchThink(void);
	void TurretDeath(void);
	void PostDeathThink( void ) {  };

	virtual void SpinDownCall(void) { m_iSpin = 0; }
	virtual void SpinUpCall(void) { m_iSpin = 1; }

	void Deploy(void);
	void Retire(void);
	
	void Initialize(void);

	virtual void Ping(void);
	virtual void EyeOn(void);
	virtual void EyeOff(void);

	DECLARE_DATADESC();

	// other functions
	void SetTurretAnim(TURRET_ANIM anim);
	int MoveTurret(void);
	virtual void Shoot(Vector &vecSrc, Vector &vecDirToEnemy) { };

	void SetEnemy( CBaseEntity *enemy );
	CBaseEntity *AcquireTarget(void);

	float m_flMaxSpin;		// Max time to spin the barrel w/o a target
	int m_iSpin;

	CSprite *m_pEyeGlow;
	int m_eyeBrightness;

	int m_iDeployHeight;
	int m_iRetractHeight;
	int m_iMinPitch;

	int m_iBaseTurnRate;	// angles per second
	float m_fTurnRate;		// actual turn rate
	int m_iOrientation;		// 0 = floor, 1 = Ceiling
	int m_iOn;
	int m_fBeserk;			// Sometimes this bitch will just freak out
	int m_iAutoStart;		// true if the turret auto deploys when a target
							// enters its range

	Vector m_vecLastSight;
	float m_flLastSight;	// Last time we saw a target
	float m_flMaxWait;		// Max time to seach w/o a target
	int m_iSearchSpeed;		// Not Used!

	// movement
	float	m_flStartYaw;
	Vector	m_vecCurAngles;
	Vector	m_vecGoalAngles;

	float	m_flPingTime;	// Time until the next ping, used when searching
	float	m_flSpinUpTime;	// Amount of time until the barrel should spin down when searching

	// diffusion stuff for monster_sentry only
	int SentryAmmoClip; // maximum ammo
};


BEGIN_DATADESC( CBaseTurret )
	DEFINE_FIELD( m_flMaxSpin, FIELD_FLOAT ),
	DEFINE_FIELD( m_iSpin, FIELD_INTEGER ),
	DEFINE_FIELD( m_pEyeGlow, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_eyeBrightness, FIELD_INTEGER ),
	DEFINE_FIELD( m_iDeployHeight, FIELD_INTEGER ),
	DEFINE_FIELD( m_iRetractHeight, FIELD_INTEGER ),
	DEFINE_FIELD( m_iMinPitch, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_iBaseTurnRate, FIELD_INTEGER, "turnrate" ),
	DEFINE_FIELD( m_fTurnRate, FIELD_FLOAT ),
	DEFINE_KEYFIELD( m_iOrientation, FIELD_INTEGER, "orientation" ),
	DEFINE_FIELD( m_iOn, FIELD_INTEGER ),
	DEFINE_FIELD( m_fBeserk, FIELD_INTEGER ),
	DEFINE_FIELD( m_iAutoStart, FIELD_INTEGER ),
	DEFINE_FIELD( m_vecLastSight, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_flLastSight, FIELD_TIME ),
	DEFINE_KEYFIELD( m_flMaxWait, FIELD_FLOAT, "maxsleep" ),
	DEFINE_KEYFIELD( m_iSearchSpeed, FIELD_INTEGER, "searchspeed" ),
	DEFINE_FIELD( m_flStartYaw, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecCurAngles, FIELD_VECTOR ),
	DEFINE_FIELD( m_vecGoalAngles, FIELD_VECTOR ),
	DEFINE_FIELD( m_flPingTime, FIELD_TIME ),
	DEFINE_FIELD( m_flSpinUpTime, FIELD_TIME ),
	DEFINE_KEYFIELD( SentryAmmoClip, FIELD_INTEGER, "ammo"),
	DEFINE_FUNCTION( TurretUse ),
	DEFINE_FUNCTION( ActiveThink ),
	DEFINE_FUNCTION( SearchThink ),
	DEFINE_FUNCTION( AutoSearchThink ),
	DEFINE_FUNCTION( TurretDeath ),
	DEFINE_FUNCTION( SpinDownCall ),
	DEFINE_FUNCTION( SpinUpCall ),
	DEFINE_FUNCTION( Deploy ),
	DEFINE_FUNCTION( Retire ),
	DEFINE_FUNCTION( Initialize ),
	DEFINE_FUNCTION( PostDeathThink ),
END_DATADESC()


class CTurret : public CBaseTurret
{
	DECLARE_CLASS( CTurret, CBaseTurret );
public:
	void Spawn(void);
	void Precache(void);
	// Think functions
	void SpinUpCall(void);
	void SpinDownCall(void);

	DECLARE_DATADESC();

	// other functions
	void Shoot(Vector &vecSrc, Vector &vecDirToEnemy);

private:
	int m_iStartSpin;

};

BEGIN_DATADESC( CTurret )
	DEFINE_FIELD( m_iStartSpin, FIELD_INTEGER ),
END_DATADESC()

class CMiniTurret : public CBaseTurret
{
	DECLARE_CLASS( CMiniTurret, CBaseMonster );
public:
	void Spawn( );
	void Precache(void);
	// other functions
	void Shoot(Vector &vecSrc, Vector &vecDirToEnemy);
};

LINK_ENTITY_TO_CLASS( monster_turret, CTurret );
LINK_ENTITY_TO_CLASS( monster_miniturret, CMiniTurret );

void CBaseTurret::GibMonster( void )
{
	Vector vecOrigin = GetAbsOrigin();

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecOrigin );
	WRITE_BYTE( TE_BREAKMODEL );

	// position
	WRITE_COORD( vecOrigin.x );
	WRITE_COORD( vecOrigin.y );
	WRITE_COORD( vecOrigin.z + 10 );
	// size
	WRITE_COORD( 0.01 );
	WRITE_COORD( 0.01 );
	WRITE_COORD( 0.01 );
	// velocity
	WRITE_COORD( 0 );
	WRITE_COORD( 0 );
	WRITE_COORD( 0 );
	// randomization of the velocity
	WRITE_BYTE( 30 );
	// Model
	WRITE_SHORT( g_sModelIndexMetalGibs );	//model id#
		// # of shards
	WRITE_BYTE( RANDOM_LONG( 30, 40 ) );
	// duration
	WRITE_BYTE( 20 );// 3.0 seconds
		// flags
	WRITE_BYTE( BREAK_METAL );
	MESSAGE_END();

	DontThink();
	UTIL_Remove( this );
}

void CBaseTurret::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "maxsleep"))
	{
		m_flMaxWait = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "orientation"))
	{
		m_iOrientation = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "ammo"))
	{
		SentryAmmoClip = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "searchspeed"))
	{
		m_iSearchSpeed = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "turnrate"))
	{
		m_iBaseTurnRate = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "style") ||
			 FStrEq(pkvd->szKeyName, "height") ||
			 FStrEq(pkvd->szKeyName, "value1") ||
			 FStrEq(pkvd->szKeyName, "value2") ||
			 FStrEq(pkvd->szKeyName, "value3"))
		pkvd->fHandled = TRUE;
	else
		BaseClass::KeyValue( pkvd );
}


void CBaseTurret::Spawn()
{ 
	Precache( );
	pev->nextthink		= gpGlobals->time + 1;
	pev->movetype		= MOVETYPE_FLY;
	pev->sequence		= 0;
	pev->frame		= 0;
	pev->solid		= SOLID_SLIDEBOX;
	pev->takedamage		= DAMAGE_AIM;

	SetBits (pev->flags, FL_MONSTER);
	SetUse( &CBaseTurret::TurretUse );

	if(( pev->spawnflags & SF_MONSTER_TURRET_AUTOACTIVATE ) && !( pev->spawnflags & SF_MONSTER_TURRET_STARTINACTIVE ))
		m_iAutoStart = TRUE;

	if( m_iOrientation == 1 )
	{
		Vector angles = GetLocalAngles();
		pev->idealpitch = 180;
		angles.x = 180;
		SetLocalAngles( angles );
	}

	ResetSequenceInfo( );
	SetBoneController( 0, 0 );
	SetBoneController( 1, 0 );
	m_flFieldOfView = VIEW_FIELD_FULL;
	if( !m_flDistLook ) m_flDistLook = TURRET_RANGE;
	SetFlag( F_METAL_MONSTER );
}


void CBaseTurret::Precache( )
{
	PRECACHE_SOUND ("turret/tu_fire1.wav");
	PRECACHE_SOUND ("turret/tu_ping.wav");
	PRECACHE_SOUND ("turret/tu_active2.wav");
	PRECACHE_SOUND ("turret/tu_die.wav");
	PRECACHE_SOUND ("turret/tu_die2.wav");
	PRECACHE_SOUND ("turret/tu_die3.wav");
	PRECACHE_SOUND ("turret/tu_retract.wav");
	PRECACHE_SOUND ("turret/tu_deploy.wav");
	PRECACHE_SOUND ("turret/tu_spinup.wav");
	PRECACHE_SOUND ("turret/tu_spindown.wav");
	PRECACHE_SOUND ("turret/tu_search.wav");
	PRECACHE_SOUND ("turret/tu_alert.wav");
	PRECACHE_SOUND( "drone/drone_hit1.wav" );
	PRECACHE_SOUND( "drone/drone_hit2.wav" );
	PRECACHE_SOUND( "drone/drone_hit3.wav" );
	PRECACHE_SOUND( "drone/drone_hit4.wav" );
	PRECACHE_SOUND( "drone/drone_hit5.wav" );
}

#define TURRET_GLOW_SPRITE "sprites/flare3.spr"

void CTurret::Spawn()
{ 
	Precache( );
	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
	else
		SET_MODEL(ENT(pev), "models/turret.mdl");
	if (!pev->health) pev->health = g_turretHealth[g_iSkillLevel];
	pev->max_health = pev->health;
	m_HackedGunPos		= Vector( 0, 0, 12.75 );
	m_flMaxSpin =		TURRET_MAXSPIN;
	pev->view_ofs.z = 12.75;

	CBaseTurret::Spawn( );

	m_iRetractHeight = 16;
	m_iDeployHeight = 32;
	m_iMinPitch	= -15;
	UTIL_SetSize(pev, Vector(-32, -32, -m_iRetractHeight), Vector(32, 32, m_iRetractHeight));
	
	SetThink(&CBaseTurret::Initialize);

	m_pEyeGlow = CSprite::SpriteCreate( TURRET_GLOW_SPRITE, GetAbsOrigin(), FALSE );
	if( m_pEyeGlow )
	{
		m_pEyeGlow->SetTransparency( kRenderGlow, 255, 0, 0, 0, kRenderFxNoDissipation );
		m_pEyeGlow->SetAttachment( edict(), 2 );
		m_pEyeGlow->SetFadeDistance( pev->iuser4 );
	}
	m_eyeBrightness = 0;

	pev->nextthink = gpGlobals->time + 0.3; 
}

void CTurret::Precache()
{
	CBaseTurret::Precache( );
	if (pev->model)
		PRECACHE_MODEL((char*)STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL ("models/turret.mdl");
	PRECACHE_MODEL (TURRET_GLOW_SPRITE);
}

void CMiniTurret::Spawn()
{ 
	Precache( );
	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
	else
		SET_MODEL(ENT(pev), "models/miniturret.mdl");
	if( !pev->health ) pev->health = g_miniturretHealth[g_iSkillLevel];
	pev->max_health = pev->health;
	m_HackedGunPos		= Vector( 0, 0, 12.75 );
	m_flMaxSpin = 0;
	pev->view_ofs.z = 12.75;

	CBaseTurret::Spawn( );
	m_iRetractHeight = 16;
	m_iDeployHeight = 32;
	m_iMinPitch	= -15;
	UTIL_SetSize(pev, Vector(-16, -16, -m_iRetractHeight), Vector(16, 16, m_iRetractHeight));

	SetThink(&CBaseTurret::Initialize);
	pev->nextthink = gpGlobals->time + 0.3; 
}


void CMiniTurret::Precache()
{
	CBaseTurret::Precache( );
	if (pev->model)
		PRECACHE_MODEL((char*)STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL ("models/miniturret.mdl");	
	PRECACHE_SOUND("weapons/hks1.wav");
	PRECACHE_SOUND("weapons/hks2.wav");
	PRECACHE_SOUND("weapons/hks3.wav");
}

void CBaseTurret::Initialize(void)
{
	m_iOn = 0;
	m_fBeserk = 0;
	m_iSpin = 0;

	SetBoneController( 0, 0 );
	SetBoneController( 1, 0 );

	if( m_iBaseTurnRate == 0 )
		m_iBaseTurnRate = TURRET_TURNRATE;

	if( m_flMaxWait == 0 )
		m_flMaxWait = TURRET_MAXWAIT;

	m_flStartYaw = GetLocalAngles().y;

	if( m_iOrientation == 1 )
	{
		Vector angles = GetLocalAngles();
		pev->view_ofs.z = -pev->view_ofs.z;
		pev->effects |= EF_INVLIGHT;
		angles.y = angles.y + 180;
		if( angles.y > 360 )
			angles.y = angles.y - 360;
		SetLocalAngles( angles );
	}

	m_vecGoalAngles.x = 0;

	if( m_iAutoStart )
	{
		m_flLastSight = gpGlobals->time + m_flMaxWait;
		SetThink(&CBaseTurret::AutoSearchThink );
		pev->nextthink = gpGlobals->time + .1;
	}
	else
	{
		SetThink( &CBaseEntity::SUB_DoNothing );
	}
}

void CBaseTurret::TurretUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster( pActivator ))
		return;

	if ( !ShouldToggle( useType, m_iOn ) )
	{
		if( useType == USE_OFF && FBitSet( pev->spawnflags, SF_MONSTER_TURRET_AUTOACTIVATE ))
			m_iAutoStart = FALSE; // disable autostart on inactive turret (old Half-Life bug)
		return;
	}

	if (m_iOn)
	{
		m_hEnemy = NULL;
		pev->nextthink = gpGlobals->time + 0.1;
		m_iAutoStart = FALSE;// switching off a turret disables autostart
		//!!!! this should spin down first!!BUGBUG
		SetThink(&CBaseTurret::Retire);
	}
	else 
	{
		pev->nextthink = gpGlobals->time + 0.1; // turn on delay

		// if the turret is flagged as an autoactivate turret, re-enable it's ability open self.
		if ( pev->spawnflags & SF_MONSTER_TURRET_AUTOACTIVATE )
			m_iAutoStart = TRUE;
		
		SetThink(&CBaseTurret::Deploy);
	}
}


void CBaseTurret::Ping( void )
{
	// make the pinging noise every second while searching
	if (m_flPingTime == 0)
		m_flPingTime = gpGlobals->time + 1;
	else if (m_flPingTime <= gpGlobals->time)
	{
		m_flPingTime = gpGlobals->time + 1;
		EMIT_SOUND(ENT(pev), CHAN_ITEM, "turret/tu_ping.wav", 1, ATTN_NORM);
		EyeOn( );
	}
	else if (m_eyeBrightness > 0)
	{
		EyeOff( );
	}
}


void CBaseTurret::EyeOn( )
{
	if (m_pEyeGlow)
	{
		if (m_eyeBrightness != 255)
		{
			m_eyeBrightness = 255;
		}
		m_pEyeGlow->SetBrightness( m_eyeBrightness );
	}
}


void CBaseTurret::EyeOff( )
{
	if (m_pEyeGlow)
	{
		if (m_eyeBrightness > 0)
		{
			m_eyeBrightness = Q_max( 0, m_eyeBrightness - 30 );
			m_pEyeGlow->SetBrightness( m_eyeBrightness );
		}
	}
}

CBaseEntity *CBaseTurret::AcquireTarget(void)
{
	Look( m_flDistLook );
	CBaseEntity *pEnemy = BestVisibleEnemy();
	if( !pEnemy )
	{
		// special case for player's turret and deathmatch mode
		if( g_pGameRules->IsMultiplayer() && FClassnameIs( this, "_playersentry" ) )
		{
			int iDist = 0;
			int iNearest = 8192;
			CBaseEntity *pOwner = NULL;
			if( pev->owner != NULL )
				pOwner = CBaseEntity::Instance( pev->owner );
			if( pOwner )
			{
				for( int i = 1; i <= gpGlobals->maxClients; i++ )
				{
					CBaseEntity *pEnt = UTIL_PlayerByIndex( i );
					if( !pEnt || !pEnt->IsPlayer() )
						continue;

					if( pOwner == pEnt )
						continue;

					if( !pEnt->IsAlive() )
						continue;

					if( g_pGameRules->IsTeamplay() )
					{
						// don't target your teammates
						if( UTIL_TeamsMatch( g_pGameRules->GetTeamID( pOwner ), g_pGameRules->GetTeamID( pEnt ) ) )
							continue;
					}

					if( !FVisible( pEnt->GetAbsOrigin() ) )
						continue;

					iDist = (pEnt->GetAbsOrigin() - GetAbsOrigin()).Length();
					if( iDist <= iNearest )
					{
						iNearest = iDist;
						pEnemy = pEnt;
					}
				}
			}

			// still no enemy? now perform search for turrets.
			if( !pEnemy )
			{
				CBaseEntity *pTurret = NULL;
				while( (pTurret = UTIL_FindEntityByClassname( pTurret, "_playersentry" )) != NULL )
				{
					if( pTurret == this )
						continue;

					// get the owner - player
					CBaseEntity *pTurretOwner = CBaseEntity::Instance( pTurret->pev->owner );
					if( pTurretOwner )
					{
						if( !pTurret->IsAlive() )
							continue;

						// my turret
						if( pOwner && pTurretOwner == pOwner )
							continue;

						if( g_pGameRules->IsTeamplay() )
						{
							// don't target your teammates if team names match...
							if( UTIL_TeamsMatch( g_pGameRules->GetTeamID( this ), g_pGameRules->GetTeamID( pTurretOwner ) ) )
								continue;
						}

						// it's an enemy turret
						auto eye_position = pTurret->EyePosition();

						if( FInViewCone( &eye_position ) && FVisible( eye_position ) )
						{
							float turret_distance = (pTurret->GetAbsOrigin() - GetAbsOrigin()).Length();
							// this turret is farther then current enemy player
							if( turret_distance > iNearest )
								continue;

							iNearest = turret_distance;
							pEnemy = pTurret;
						}
					}
					else
						continue;
				}
			}
		}
	}

	return pEnemy;
}

void CBaseTurret::ActiveThink(void)
{
	int fAttack = 0;
	Vector vecDirToEnemy;

	SetNextThink(0.1);
	StudioFrameAdvance();

	if ((!m_iOn) || (m_hEnemy == NULL))
	{
		m_hEnemy = NULL;
		m_flLastSight = gpGlobals->time + m_flMaxWait;
		SetThink(&CBaseTurret::SearchThink);
		return;
	}
	
	// if it's dead, look for something new
	if ( !m_hEnemy->IsAlive() )
	{
		if (!m_flLastSight)
			m_flLastSight = gpGlobals->time + 0.5; // continue-shooting timeout
		else
		{
			if (gpGlobals->time > m_flLastSight)
			{	
				m_hEnemy = NULL;
				m_flLastSight = gpGlobals->time + m_flMaxWait;
				SetThink(&CBaseTurret::SearchThink);
				return;
			}
		}
	}

	Vector vecMid = GetAbsOrigin() + pev->view_ofs;
	Vector vecMidEnemy = m_hEnemy->BodyTarget( vecMid );

	// Look for our current enemy
	int fEnemyVisible = FBoxVisible(pev, m_hEnemy->pev, vecMidEnemy );	

	vecDirToEnemy = vecMidEnemy - vecMid;	// calculate dir and dist to enemy
	float flDistToEnemy = vecDirToEnemy.Length();

	// Current enemy is not visible.
	if( !fEnemyVisible || ( flDistToEnemy > m_flDistLook ))
	{
		if( !m_flLastSight )
			m_flLastSight = gpGlobals->time + 0.5;
		else
		{
			// Should we look for a new target?
			if( gpGlobals->time > m_flLastSight )
			{
				m_hEnemy = NULL;
				m_flLastSight = gpGlobals->time + m_flMaxWait;
				SetThink(&CBaseTurret::SearchThink );
				return;
			}
		}
		fEnemyVisible = 0;
	}
	else
		m_vecLastSight = vecMidEnemy;

	UTIL_MakeAimVectors( m_vecCurAngles );	

	/*
	ALERT( at_console, "%.0f %.0f : %.2f %.2f %.2f\n", 
		m_vecCurAngles.x, m_vecCurAngles.y,
		gpGlobals->v_forward.x, gpGlobals->v_forward.y, gpGlobals->v_forward.z );
	*/
	
	Vector vecLOS = vecDirToEnemy; //vecMid - m_vecLastSight;
	vecLOS = vecLOS.Normalize();

	// Is the Gun looking at the target
	if (DotProduct(vecLOS, gpGlobals->v_forward) <= 0.866) // 30 degree slop
		fAttack = FALSE;
	else
		fAttack = TRUE;

	// fire the gun
	if (m_iSpin && ((fAttack) || (m_fBeserk)))
	{
		Vector vecSrc, vecAng;
		GetAttachment( 0, vecSrc, vecAng );
		SetTurretAnim(TURRET_ANIM_FIRE);
		Shoot(vecSrc, gpGlobals->v_forward );
	} 
	else
		SetTurretAnim(TURRET_ANIM_SPIN);

	// move the gun
	if( m_fBeserk )
	{
		if( RANDOM_LONG( 0, 9 ) == 0 )
		{
			m_vecGoalAngles.y = RANDOM_FLOAT( 0, 360 );
			m_vecGoalAngles.x = RANDOM_FLOAT( 0, 90 ) - 90 * m_iOrientation;
			TakeDamage( pev, pev, 1, DMG_GENERIC ); // don't beserk forever
			return;
		}
	} 
	else if( fEnemyVisible )
	{
		Vector vec = UTIL_VecToAngles( vecDirToEnemy );
		vec.x = -vec.x;

		if( vec.y > 360 )
			vec.y -= 360;

		if (vec.y < 0)
			vec.y += 360;

		//ALERT(at_console, "[%.2f]", vec.x);
		
		if (vec.x < -180)
			vec.x += 360;

		if (vec.x > 180)
			vec.x -= 360;

		// now all numbers should be in [1...360]
		// pin to turret limitations to [-90...15]

		if( m_iOrientation == 0 )
		{
			if( vec.x > 90 )
				vec.x = 90;
			else if( vec.x < m_iMinPitch )
				vec.x = m_iMinPitch;
		}
		else
		{
			if( vec.x < -90 )
				vec.x = -90;
			else if( vec.x > -m_iMinPitch )
				vec.x = -m_iMinPitch;
		}

		// ALERT(at_console, "->[%.2f]\n", vec.x);

		m_vecGoalAngles.y = vec.y;
		m_vecGoalAngles.x = vec.x;

	}

	SpinUpCall();
	MoveTurret();
}


void CTurret::Shoot(Vector &vecSrc, Vector &vecDirToEnemy)
{
	FireBullets( 1, vecSrc, vecDirToEnemy, TURRET_SPREAD, m_flDistLook, BULLET_MONSTER_9MM, 1, 0, VARS(pev->owner) );
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "turret/tu_fire1.wav", 1, 0.6, 0, RANDOM_LONG( 95, 105 ) );
	pev->effects |= EF_MUZZLEFLASH;
}


void CMiniTurret::Shoot(Vector &vecSrc, Vector &vecDirToEnemy)
{
	FireBullets( 1, vecSrc, vecDirToEnemy, TURRET_SPREAD, m_flDistLook, BULLET_MONSTER_9MM, 1, 0, VARS( pev->owner ) );

	switch(RANDOM_LONG(0,2))
	{
	case 0: EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/hks1.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) ); break;
	case 1: EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/hks2.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) ); break;
	case 2: EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/hks3.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) ); break;
	}
	pev->effects |= EF_MUZZLEFLASH;
}


void CBaseTurret::Deploy(void)
{
	SetNextThink( 0.1 );
	StudioFrameAdvance( );

	if (pev->sequence != TURRET_ANIM_DEPLOY)
	{
		m_iOn = 1;
		SetTurretAnim(TURRET_ANIM_DEPLOY);
		EMIT_SOUND(ENT(pev), CHAN_BODY, "turret/tu_deploy.wav", TURRET_MACHINE_VOLUME, ATTN_NORM);
		SUB_UseTargets( this, USE_ON, 0 );
	}

	if( m_fSequenceFinished )
	{
//		pev->maxs.z = m_iDeployHeight;  // diffusion - removed this
//		pev->mins.z = -m_iDeployHeight;
//		UTIL_SetSize( pev, pev->mins, pev->maxs );

		m_vecCurAngles.x = 0;

		if( m_iOrientation == 1 )
		{
			m_vecCurAngles.y = UTIL_AngleMod( GetLocalAngles().y + 180 );
		}
		else
		{
			m_vecCurAngles.y = UTIL_AngleMod( GetLocalAngles().y );
		}

		SetTurretAnim( TURRET_ANIM_SPIN );
		pev->framerate = 0;
		SetThink(&CBaseTurret::SearchThink );
	}

	m_flLastSight = gpGlobals->time + m_flMaxWait;
}

void CBaseTurret::Retire(void)
{
	// make the turret level
	m_vecGoalAngles.x = 0;
	m_vecGoalAngles.y = m_flStartYaw;

	SetNextThink( 0.1 );

	StudioFrameAdvance( );

	EyeOff( );

	if (!MoveTurret())
	{
		if (m_iSpin)
		{
			SpinDownCall();
		}
		else if (pev->sequence != TURRET_ANIM_RETIRE)
		{
			SetTurretAnim(TURRET_ANIM_RETIRE);
			EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "turret/tu_retract.wav", TURRET_MACHINE_VOLUME, ATTN_NORM, 0, 120);
			SUB_UseTargets( this, USE_OFF, 0 );
		}
		else if (m_fSequenceFinished) 
		{	
			m_iOn = 0;
			m_flLastSight = 0;
			SetTurretAnim(TURRET_ANIM_NONE);
//			pev->maxs.z = m_iRetractHeight;  // diffusion - removed this
//			pev->mins.z = -m_iRetractHeight;
//			UTIL_SetSize(pev, pev->mins, pev->maxs);
			if (m_iAutoStart)
			{
				SetThink(&CBaseTurret::AutoSearchThink);
				SetNextThink(0.1);
			}
			else
				SetThink(&CBaseEntity::SUB_DoNothing);
		}
	}
	else
	{
		SetTurretAnim(TURRET_ANIM_SPIN);
	}
}


void CTurret::SpinUpCall(void)
{
	StudioFrameAdvance( );
	pev->nextthink = gpGlobals->time + 0.1;

	// Are we already spun up? If not start the two stage process.
	if (!m_iSpin)
	{
		SetTurretAnim( TURRET_ANIM_SPIN );
		// for the first pass, spin up the the barrel
		if (!m_iStartSpin)
		{
			SetNextThink( 1.0 ); // spinup delay
			EMIT_SOUND(ENT(pev), CHAN_BODY, "turret/tu_spinup.wav", TURRET_MACHINE_VOLUME, ATTN_NORM);
			m_iStartSpin = 1;
			pev->framerate = 0.1;
		}
		// after the barrel is spun up, turn on the hum
		else if (pev->framerate >= 1.0)
		{
			SetNextThink( 0.1 ); // retarget delay
			EMIT_SOUND(ENT(pev), CHAN_STATIC, "turret/tu_active2.wav", TURRET_MACHINE_VOLUME, ATTN_NORM);
			SetThink(&CBaseTurret::ActiveThink);
			m_iStartSpin = 0;
			m_iSpin = 1;
		} 
		else
		{
			pev->framerate += 0.075;
		}
	}

	if (m_iSpin)
	{
		SetThink(&CBaseTurret::ActiveThink);
	}
}


void CTurret::SpinDownCall(void)
{
	if (m_iSpin)
	{
		SetTurretAnim( TURRET_ANIM_SPIN );
		if (pev->framerate == 1.0)
		{
			EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "turret/tu_active2.wav", 0, 0, SND_STOP, 100);
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "turret/tu_spindown.wav", TURRET_MACHINE_VOLUME, ATTN_NORM);
		}
		pev->framerate -= 0.02;
		if (pev->framerate <= 0)
		{
			pev->framerate = 0;
			m_iSpin = 0;
		}
	}
}


void CBaseTurret::SetTurretAnim(TURRET_ANIM anim)
{
	if (pev->sequence != anim)
	{
		switch(anim)
		{
		case TURRET_ANIM_FIRE:
		case TURRET_ANIM_SPIN:
			if (pev->sequence != TURRET_ANIM_FIRE && pev->sequence != TURRET_ANIM_SPIN)
			{
				pev->frame = 0;
			}
			break;
		default:
			pev->frame = 0;
			break;
		}

		pev->sequence = anim;
		ResetSequenceInfo( );

		switch(anim)
		{
		case TURRET_ANIM_RETIRE:
			pev->frame			= 255;
			pev->framerate		= -1.0;
			break;
		case TURRET_ANIM_DIE:
			pev->framerate		= 1.0;
			break;
		}
		//ALERT(at_console, "Turret anim #%d\n", anim);
	}
}


//
// This search function will sit with the turret deployed and look for a new target. 
// After a set amount of time, the barrel will spin down. After m_flMaxWait, the turret will
// retact.
//
void CBaseTurret::SearchThink(void)
{
	// ensure rethink
	SetTurretAnim(TURRET_ANIM_SPIN);
	StudioFrameAdvance( );
	SetNextThink(0.1);

	if (m_flSpinUpTime == 0 && m_flMaxSpin)
		m_flSpinUpTime = gpGlobals->time + m_flMaxSpin;

	Ping();

	// If we have a target and we're still healthy
	if (m_hEnemy != NULL)
	{
		if (!m_hEnemy->IsAlive() )
			m_hEnemy = NULL;// Dead enemy forces a search for new one
	}


	// Acquire Target
	if (m_hEnemy == NULL)
	{	
		SetEnemy( AcquireTarget() );
	}

	// If we've found a target, spin up the barrel and start to attack
	if (m_hEnemy != NULL)
	{
		m_flLastSight = 0;
		m_flSpinUpTime = 0;
		SetThink(&CBaseTurret::ActiveThink);
	}
	else
	{
		// Are we out of time, do we need to retract?
 		if (gpGlobals->time > m_flLastSight)
		{
			//Before we retrace, make sure that we are spun down.
			m_flLastSight = 0;
			m_flSpinUpTime = 0;
			SetThink(&CBaseTurret::Retire);
		}
		// should we stop the spin?
		else if ((m_flSpinUpTime) && (gpGlobals->time > m_flSpinUpTime))
		{
			SpinDownCall();
		}
		
		// generic hunt for new victims
		m_vecGoalAngles.y = (m_vecGoalAngles.y + 0.1 * m_fTurnRate);
		if (m_vecGoalAngles.y >= 360)
			m_vecGoalAngles.y -= 360;
		MoveTurret();
	}
}

// 
// This think function will deploy the turret when something comes into range. This is for
// automatically activated turrets.
//
void CBaseTurret::AutoSearchThink(void)
{
	// ensure rethink
	StudioFrameAdvance( );
	SetNextThink(0.3);

	// If we have a target and we're still healthy

	if (m_hEnemy != NULL)
	{
		if (!m_hEnemy->IsAlive() )
			m_hEnemy = NULL;// Dead enemy forces a search for new one
	}

	// Acquire Target
	if( m_hEnemy == NULL )
	{
		SetEnemy( AcquireTarget() );
	}

	if (m_hEnemy != NULL)
	{
		SetThink(&CBaseTurret::Deploy);
		EMIT_SOUND(ENT(pev), CHAN_BODY, "turret/tu_alert.wav", TURRET_MACHINE_VOLUME, ATTN_NORM);
	}
}


void CBaseTurret ::	TurretDeath( void )
{
	BOOL iActive = FALSE;

	StudioFrameAdvance( );
	SetNextThink( 0.1 );

	if (pev->deadflag != DEAD_DEAD)
	{
		pev->deadflag = DEAD_DEAD;

		EMIT_SOUND(ENT(pev), CHAN_BODY, "turret/tu_die.wav", 1.0, ATTN_NORM);

		EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "turret/tu_active2.wav", 0, 0, SND_STOP, 100);

		if (m_iOrientation == 0)
			m_vecGoalAngles.x = -15;
		else
			m_vecGoalAngles.x = -90;

		SetTurretAnim(TURRET_ANIM_DIE); 
		
		EyeOn( );	
	}

	EyeOff( );

	if (pev->dmgtime + RANDOM_FLOAT( 0, 2 ) > gpGlobals->time)
	{
		// lots of smoke
		MESSAGE_BEGIN( MSG_BROADCAST, gmsgTempEnt );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( RANDOM_FLOAT( pev->absmin.x, pev->absmax.x ) );
			WRITE_COORD( RANDOM_FLOAT( pev->absmin.y, pev->absmax.y ) );
			WRITE_COORD( GetAbsOrigin().z - m_iOrientation * 64 );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( 25 ); // scale * 10
			WRITE_BYTE( 10 - m_iOrientation * 5); // framerate
			WRITE_BYTE( 2 ); // pos randomize
		MESSAGE_END();
	}
	
	if (pev->dmgtime + RANDOM_FLOAT( 0, 5 ) > gpGlobals->time)
	{
		Vector vecSrc = Vector( RANDOM_FLOAT( pev->absmin.x, pev->absmax.x ), RANDOM_FLOAT( pev->absmin.y, pev->absmax.y ), 0 );
		if (m_iOrientation == 0)
			vecSrc = vecSrc + Vector( 0, 0, RANDOM_FLOAT( GetAbsOrigin().z, pev->absmax.z ) );
		else
			vecSrc = vecSrc + Vector( 0, 0, RANDOM_FLOAT( pev->absmin.z, GetAbsOrigin().z ) );

		UTIL_Sparks( vecSrc );
	}

	if (m_fSequenceFinished && !MoveTurret( ) && pev->dmgtime + 5 < gpGlobals->time)
	{
		pev->framerate = 0;
		SetThink( NULL );
	}
}



void CBaseTurret::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	if( ptr->iHitgroup == 10 )
	{
		// hit armor
		if( pev->dmgtime != gpGlobals->time || (RANDOM_LONG( 0, 10 ) < 1) )
		{
			UTIL_Ricochet( ptr->vecEndPos, RANDOM_FLOAT( 1, 2 ) );
			pev->dmgtime = gpGlobals->time;
		}

		flDamage = 0.1;// don't hurt the monster much, but allow bits_COND_LIGHT_DAMAGE to be generated
	}

	if( !pev->takedamage )
		return;

	if( pev->owner )
	{
		if( (pevAttacker->flags & FL_CLIENT) && (VARS(pev->owner) == pevAttacker) )
			return;
	}

	if( ptr->iHitgroup != 10 ) // not armor
	{
		switch( RANDOM_LONG( 0, 4 ) )
		{
		case 0: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit1.wav", 1, ATTN_NORM ); break;
		case 1: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit2.wav", 1, ATTN_NORM ); break;
		case 2: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit3.wav", 1, ATTN_NORM ); break;
		case 3: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit4.wav", 1, ATTN_NORM ); break;
		case 4: EMIT_SOUND( edict(), CHAN_STATIC, "drone/drone_hit5.wav", 1, ATTN_NORM ); break;
		}

		UTIL_Sparks( ptr->vecEndPos );
	}

	AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );
}

// take damage. bitsDamageType indicates type of damage sustained, ie: DMG_BULLET

int CBaseTurret::TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType)
{
	if ( !pev->takedamage )
		return 0;

	if (!m_iOn)
		flDamage /= 10.0;

	pev->health -= flDamage;
	if (pev->health <= 0)
	{
		pev->health = 0;
		pev->takedamage = DAMAGE_NO;
		pev->dmgtime = gpGlobals->time;

		ClearBits (pev->flags, FL_MONSTER); // why are they set in the first place???

		SetUse(NULL);
		
		SUB_UseTargets( this, USE_ON, 0 ); // wake up others

		if( g_pGameRules->IsMultiplayer() )
		{
			// always gib - don't leave the models
			GibMonster();
		}
		else
		{
			if( pev->health < -40 )
				GibMonster();
			else
			{
				SetThink( &CBaseTurret::TurretDeath );
				SetNextThink( 0.1 );
			}
		}

		return 0;
	}

	if (pev->health <= 10)
	{
		if (m_iOn && (1 || RANDOM_LONG(0, 0x7FFF) > 800))
		{
			m_fBeserk = 1;
			SetThink(&CBaseTurret::SearchThink);
		}
	}

	return 1;
}

int CBaseTurret::MoveTurret(void)
{
	int state = 0;
	// any x movement?
	
	if (m_vecCurAngles.x != m_vecGoalAngles.x)
	{
		float flDir = m_vecGoalAngles.x > m_vecCurAngles.x ? 1 : -1 ;

		m_vecCurAngles.x += 0.1 * m_fTurnRate * flDir;

		// if we started below the goal, and now we're past, peg to goal
		if (flDir == 1)
		{
			if (m_vecCurAngles.x > m_vecGoalAngles.x)
				m_vecCurAngles.x = m_vecGoalAngles.x;
		} 
		else
		{
			if (m_vecCurAngles.x < m_vecGoalAngles.x)
				m_vecCurAngles.x = m_vecGoalAngles.x;
		}

		if (m_iOrientation == 0)
			SetBoneController(1, -m_vecCurAngles.x);
		else
			SetBoneController(1, m_vecCurAngles.x);
		state = 1;
	}

	if (m_vecCurAngles.y != m_vecGoalAngles.y)
	{
		float flDir = m_vecGoalAngles.y > m_vecCurAngles.y ? 1 : -1 ;
		float flDist = fabs(m_vecGoalAngles.y - m_vecCurAngles.y);
		
		if (flDist > 180)
		{
			flDist = 360 - flDist;
			flDir = -flDir;
		}
		if (flDist > 30)
		{
			if (m_fTurnRate < m_iBaseTurnRate * 10)
			{
				m_fTurnRate += m_iBaseTurnRate;
			}
		}
		else if (m_fTurnRate > 45)
		{
			m_fTurnRate -= m_iBaseTurnRate;
		}
		else
		{
			m_fTurnRate += m_iBaseTurnRate;
		}

		m_vecCurAngles.y += 0.1 * m_fTurnRate * flDir;

		if (m_vecCurAngles.y < 0)
			m_vecCurAngles.y += 360;
		else if (m_vecCurAngles.y >= 360)
			m_vecCurAngles.y -= 360;

		if (flDist < (0.05 * m_iBaseTurnRate))
			m_vecCurAngles.y = m_vecGoalAngles.y;

		//ALERT(at_console, "%.2f -> %.2f\n", m_vecCurAngles.y, y);
		if (m_iOrientation == 0)
			SetBoneController(0, m_vecCurAngles.y - GetLocalAngles().y );
		else 
			SetBoneController(0, GetLocalAngles().y - 180 - m_vecCurAngles.y );
		state = 1;
	}

	if (!state)
		m_fTurnRate = m_iBaseTurnRate;

	//ALERT(at_console, "(%.2f, %.2f)->(%.2f, %.2f)\n", m_vecCurAngles.x, 
	//	m_vecCurAngles.y, m_vecGoalAngles.x, m_vecGoalAngles.y);
	return state;
}

//
// ID as a machine
//
int CBaseTurret::Classify ( void )
{
	if (m_iOn || m_iAutoStart)
	{
		if (m_iClass)
			return m_iClass;
		return CLASS_MACHINE;
	}
	return CLASS_NONE;
}

void CBaseTurret::SetEnemy( CBaseEntity *enemy )
{
	m_hEnemy = enemy;
	if( m_hEnemy )
	{
		SetConditions( bits_COND_SEE_ENEMY );
		FCheckAITrigger();
		ClearConditions( bits_COND_SEE_ENEMY );
	}
}










#include	"entities/soundent.h"

//=========================================================
// Sentry gun - smallest turret, placed near grunt entrenchments
//=========================================================
class CSentry : public CBaseTurret
{
	DECLARE_CLASS( CSentry, CBaseTurret );
public:
	void Spawn( );
	void Precache(void);

	// other functions
	void Shoot(Vector &vecSrc, Vector &vecDirToEnemy);
	int TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType);
	void SentryTouch( CBaseEntity *pOther );
	void SentryDeath( void );
	void PostDeathThink( void );
	void Retire(void);
	int ObjectCaps( void );

	float SpawnTime; // diffusion - hackhack: invincible for 1.5 seconds, to initialize all the needed stuff after spawn

//	Vector EyePosition() { return GetAbsOrigin() + Vector( 0, 0, 15 ); };

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( monster_sentry, CSentry );
LINK_ENTITY_TO_CLASS( _playersentry, CSentry );

BEGIN_DATADESC( CSentry )
	DEFINE_FUNCTION( SentryTouch ),
	DEFINE_FUNCTION( SentryDeath ),
END_DATADESC()

int CSentry::ObjectCaps( void )
{ 
	// only player turret is holdable (TODO: make a flag?)
	if( !FClassnameIs( pev, "_playersentry" ) )
		return BaseClass::ObjectCaps();
	
	int flags = 0;

	if( pev->deadflag == DEAD_DEAD )
		flags = 0;
	else
		flags = FCAP_HOLDABLE_ITEM;
	return (BaseClass :: ObjectCaps()) | flags;
}

void CSentry::Precache()
{
	BaseClass::Precache( );

	if (pev->model)
		PRECACHE_MODEL((char*)STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL ("models/sentry.mdl");

	PRECACHE_SOUND("turret/shoot1.wav");
	PRECACHE_SOUND("turret/shoot2.wav");
	PRECACHE_SOUND("turret/shoot3.wav");
}

void CSentry::Spawn()
{ 
	Precache( );

	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
	else
		SET_MODEL(ENT(pev), "models/sentry.mdl");

	if( !pev->health ) pev->health = g_sentryHealth[g_iSkillLevel];
	pev->max_health = pev->health;
	m_HackedGunPos		= Vector( 0, 0, 48 );
	pev->view_ofs.z		= 48;
	if (m_flMaxWait == 0)
		m_flMaxWait = TURRET_MAXWAIT;//G-Cont. now sentry can be deactivated
	m_flMaxSpin	= 1E6;

	BaseClass::Spawn();
	pev->movetype = MOVETYPE_STEP; // diffusion - baseclass sets movetype_fly. set the correct one here

	m_iRetractHeight = 64;
	m_iDeployHeight = 64;
	m_iMinPitch	= -60;
//	UTIL_SetSize(pev, Vector(-16, -16, -m_iRetractHeight), Vector(16, 16, m_iRetractHeight));
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 48)); // diffusion

	if( FStringNull(pev->targetname) ) // diffusion - I have no name, so I start automatically
		m_iAutoStart = TRUE;

	if( FClassnameIs( pev, "_playersentry" ) )
	{
		SentryAmmoClip = 500;
		if( g_pGameRules->IsMultiplayer() )
			SentryAmmoClip = 9999;
	}

	// hack to not create a bool
	if( !SentryAmmoClip || (SentryAmmoClip == -1) )
		SentryAmmoClip = 9999; // this is actually infinite (bullets won't be subtracted).

	SpawnTime = gpGlobals->time;

	SetTouch(&CSentry::SentryTouch);
	SetThink(&CBaseTurret::Initialize);	
	SetNextThink( 0.3 );
	
	pev->iuser3 = 0; // will be set to -672 when hp is low, to enable smoke effect on client
	pev->vuser1.z = 30; // offset for smoke effect
}

void CSentry::Shoot(Vector &vecSrc, Vector &vecDirToEnemy)
{
	const bool bPlayerSentry = FClassnameIs( pev, "_playersentry" );

	FireBullets( 1, vecSrc, vecDirToEnemy, TURRET_SPREAD, m_flDistLook, BULLET_MONSTER_MP5, 1, bPlayerSentry ? 5.0f : 2.5f, VARS( pev->owner ) );
	CSoundEnt::InsertSound ( bits_SOUND_COMBAT, GetAbsOrigin(), 768, 0.3, ENTINDEX(edict()) );

//	ALERT(at_console, "SentryAmmoClip %3d\n", SentryAmmoClip);
	
	switch(RANDOM_LONG(0,2))
	{
	case 0: EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "turret/shoot1.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) ); break;
	case 1: EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "turret/shoot2.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) ); break;
	case 2: EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "turret/shoot3.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ) ); break;
	}

	pev->effects |= EF_MUZZLEFLASH;

	// it's a loud weapon
	if( bPlayerSentry )
	{
		if( pev->owner )
		{
			CBasePlayer *pPlayerOwner = (CBasePlayer *)CBaseEntity::Instance( pev->owner );
			if( pPlayerOwner && pPlayerOwner->LoudWeaponsRestricted )
				pPlayerOwner->FireLoudWeaponRestrictionEntity();
		}
	}

	if( SentryAmmoClip > 0 )
	{
		if( SentryAmmoClip < 9999 )
			SentryAmmoClip--;
	}
	else
	{
		SetThink( &CSentry::Retire );
		SetNextThink( 0.1 );
	}
}

int CSentry::TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType)
{
	if( gpGlobals->time < SpawnTime + 1.5 )
		return 0;
	
	if ( !pev->takedamage )
		return 0;

	if( pev->owner )
	{
		if( (pevAttacker->flags & FL_CLIENT) && (VARS( pev->owner ) == pevAttacker) ) // diffusion - weapon_sentry
			return 0;
	}

	if (!m_iOn && (SentryAmmoClip > 0) )
	{
		SetThink( &CBaseTurret::Deploy );
		SetUse( NULL );
		SetNextThink( 0.1 );
	}

	pev->health -= flDamage;

	if( pev->health < pev->max_health * 0.35f )
		pev->iuser3 = -672; // enable smoke effect

	if (pev->health <= 0)
	{
		// drop the turret if it's holdable
		CBasePlayer *pPlayer = NULL;
		for( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if( !pPlayer )
				continue;

			if( pPlayer->m_pHoldableItem == this )
				pPlayer->DropHoldableItem( 50 );
		}

		pev->iuser3 = 0; // clear smoke effect
		pev->health = 0;
		pev->takedamage = DAMAGE_NO;
		pev->dmgtime = gpGlobals->time;

		ClearBits (pev->flags, FL_MONSTER); // why are they set in the first place???

		SetUse(NULL);
		
		SUB_UseTargets( this, USE_ON, 0 ); // wake up others
		
		if( g_pGameRules->IsMultiplayer() )
		{
			// always gib - don't leave the models
			GibMonster();
		}
		else
		{
			if( pev->health < -40 )
				GibMonster();
			else
			{
				SetThink( &CSentry::SentryDeath );
				SetNextThink( 0.1 );
			}
		}

		return 0;
	}

	return 1;
}


void CSentry::SentryTouch( CBaseEntity *pOther )
{
	// diffusion - don't stack holdable sentries on top of each other
	if( FClassnameIs(pOther->pev, "monster_sentry") || FClassnameIs(pOther->pev, "_playersentry") )
	{
		SetAbsVelocity( Vector( RANDOM_LONG(-100,100), RANDOM_LONG(-100,100), RANDOM_LONG(50,125) ) );
		return;
	}

	if( pev->owner )
	{
		if( pOther && (pOther->IsPlayer()) && (pev->owner == pOther->edict()) )
			return;
	}
	
	if ( pOther && (pOther->IsPlayer() || (pOther->pev->flags & FL_MONSTER)) )
	{
		if( pOther->Classify() != Classify() ) // diffusion - do not wake up if a friend touched me
			TakeDamage(pOther->pev, pOther->pev, 0, 0 );
	}
}


void CSentry :: SentryDeath( void )
{
	BOOL iActive = FALSE;

	pev->flags &= ~FL_ONGROUND;

	StudioFrameAdvance( );
	SetNextThink( 0.1 );

	if (pev->deadflag != DEAD_DEAD)
	{
		pev->deadflag = DEAD_DEAD;

		ObjectCaps(); // can't pick up the turret anymore

		STOP_SOUND( ENT( pev ), CHAN_STATIC, "turret/tu_active2.wav" );
		EMIT_SOUND(ENT(pev), CHAN_BODY, "turret/tu_die.wav", 0.5, ATTN_NORM);

		SetBoneController( 0, 0 );
		SetBoneController( 1, 0 );

		SetTurretAnim(TURRET_ANIM_DIE); 

		Vector angles = GetLocalAngles();
		pev->solid = SOLID_NOT;
		angles.y = UTIL_AngleMod( angles.y + RANDOM_LONG( 0, 2 ) * 120 );
		SetLocalAngles( angles );

		EyeOn( );
	}

	EyeOff( );

	Vector vecSrc, vecAng;
	GetAttachment( 1, vecSrc, vecAng );

	if (pev->dmgtime + RANDOM_FLOAT( 0, 2 ) > gpGlobals->time)
	{
		// lots of smoke
		MESSAGE_BEGIN( MSG_BROADCAST, gmsgTempEnt );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( vecSrc.x + RANDOM_FLOAT( -16, 16 ) );
			WRITE_COORD( vecSrc.y + RANDOM_FLOAT( -16, 16 ) );
			WRITE_COORD( vecSrc.z - 32 );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( 15 ); // scale * 10
			WRITE_BYTE( 8 ); // framerate
			WRITE_BYTE( 2 ); // pos randomize
		MESSAGE_END();
	}
	
	if (pev->dmgtime + RANDOM_FLOAT( 0, 8 ) > gpGlobals->time)
		UTIL_Sparks( vecSrc );

	if (m_fSequenceFinished && pev->dmgtime + 5 < gpGlobals->time)
	{
		pev->framerate = 0;
		if( FClassnameIs( this, "_playersentry" ) )
		{
			SetThink( &CSentry::PostDeathThink );
			SetNextThink( 0 );
			m_iCounter = 0;
		}
		else
			SetThink( NULL );
	}
}

void CSentry::PostDeathThink( void )
{
	pev->flags &= ~FL_ONGROUND;
	m_iCounter++;

	if( m_iCounter == 60 )
		SetThink( &CBaseEntity::SUB_FadeOut );

	SetNextThink( 1.0 );
}

void CSentry::Retire(void)
{	
	// make the turret level
	m_vecGoalAngles.x = 0;
	m_vecGoalAngles.y = m_flStartYaw;

	pev->nextthink = gpGlobals->time + 0.1;

	StudioFrameAdvance( );

	EyeOff( );

	if (!MoveTurret())
	{
		if (m_iSpin)
			SpinDownCall();
		else if (pev->sequence != TURRET_ANIM_RETIRE)
		{
			SetTurretAnim(TURRET_ANIM_RETIRE);
			EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "turret/tu_retract.wav", TURRET_MACHINE_VOLUME, ATTN_NORM, 0, 120);
			SUB_UseTargets( this, USE_OFF, 0 );
		}
		else if (m_fSequenceFinished) 
		{	
			m_iOn = 0;
			m_flLastSight = 0;
			SetTurretAnim(TURRET_ANIM_NONE);
//			pev->maxs.z = m_iRetractHeight;  // diffusion - removed this
//			pev->mins.z = -m_iRetractHeight;
//			UTIL_SetSize(pev, pev->mins, pev->maxs);

			if( SentryAmmoClip <= 0 )
			{
				m_iClass = CLASS_INSECT; // yep. Can't use CLASS_NONE because of (!m_iClass) checks.
				DontThink();
				SetTouch(NULL);
			}
			else
			{
				if (m_iAutoStart)
				{
					SetThink(&CBaseTurret::AutoSearchThink);		
					SetNextThink( 0.1 );
				}
				else
					SetThink(&CBaseEntity::SUB_DoNothing);
			}
		}
	}
	else
		SetTurretAnim(TURRET_ANIM_SPIN);
}
