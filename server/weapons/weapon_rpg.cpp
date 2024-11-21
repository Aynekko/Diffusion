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

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons/weapons.h"
#include "nodes.h"
#include "player.h"
#include "game/gamerules.h"
#include "entities/soundent.h"
#include "decals.h"

class CRpg : public CBasePlayerWeapon
{
	DECLARE_CLASS( CRpg, CBasePlayerWeapon );
public:
	void Spawn( void );
	void Precache( void );
	void Reload( void );
	int iItemSlot( void ) { return WPN_SLOT_RPG + 1; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	DECLARE_DATADESC();

	BOOL Deploy( void );
	BOOL CanHolster( void );
	void Holster( void );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void WeaponIdle( void );

	void UpdateSpot( void );
	BOOL ShouldWeaponIdle( void ) { return TRUE; };	// laser spot give updates from WeaponIdle

	CLaserSpot *m_pSpot;
	int m_cActiveRockets;// how many missiles in flight from this launcher right now?
	int m_fSpotActive;
};

LINK_ENTITY_TO_CLASS( weapon_rpg, CRpg );

BEGIN_DATADESC( CRpg )
	DEFINE_FIELD( m_fSpotActive, FIELD_INTEGER ),
	DEFINE_FIELD( m_cActiveRockets, FIELD_INTEGER ),
END_DATADESC()





#define AUGER_YDEVIANCE			20.0f
#define AUGER_XDEVIANCEDOWN		1.0f
#define AUGER_XDEVIANCEUP		8.0f

class CRpgRocket : public CGrenade
{
	DECLARE_CLASS( CRpgRocket, CGrenade );
public:
	void Spawn( void );
	void Precache( void );
	void FollowThink( void );
	void IgniteThink( void );
	void RocketTouch( CBaseEntity *pOther );
	static CRpgRocket *CreateRpgRocket( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner, CRpg *pLauncher );
	void CreateTrail( void );
	virtual void OnTeleport( void );
	void ClearEffects( void );
	int Classify( void );
	
	DECLARE_DATADESC();

	int m_iTrail;
	float m_flIgniteTime;
	CRpg *m_pLauncher;// pointer back to the launcher that fired me. 

	// diffusion - some hl2 functionality
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void ShotDown( void );
	float m_flAugerTime;
	float m_flMarkDeadTime;
	void AugerThink( void );
};

LINK_ENTITY_TO_CLASS( rpg_rocket, CRpgRocket );

BEGIN_DATADESC( CRpgRocket )
	DEFINE_FIELD( m_flIgniteTime, FIELD_TIME ),
	DEFINE_FIELD( m_pLauncher, FIELD_CLASSPTR ),
	DEFINE_FUNCTION( FollowThink ),
	DEFINE_FUNCTION( IgniteThink ),
	DEFINE_FUNCTION( RocketTouch ),
	DEFINE_FIELD( m_flAugerTime, FIELD_TIME ),
	DEFINE_FIELD( m_flMarkDeadTime, FIELD_TIME ),
	DEFINE_FUNCTION( AugerThink ),
END_DATADESC()

//=========================================================
//=========================================================
CRpgRocket *CRpgRocket::CreateRpgRocket( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner, CRpg *pLauncher )
{
	CRpgRocket *pRocket = GetClassPtr( (CRpgRocket *)NULL );

	UTIL_SetOrigin( pRocket, vecOrigin );
	pRocket->SetLocalAngles( vecAngles );
	pRocket->pev->owner = pOwner->edict();
	if( pOwner->pev->effects & EF_UPSIDEDOWN )
		pRocket->pev->effects |= EF_UPSIDEDOWN;
	pRocket->Spawn();
	pRocket->SetTouch( &CRpgRocket::RocketTouch );
	pRocket->m_pLauncher = pLauncher;// remember what RPG fired me. 
	// diffusion - safety check...
	if( pRocket->m_pLauncher->m_cActiveRockets < 0 )
		pRocket->m_pLauncher->m_cActiveRockets = 0;
	pRocket->m_pLauncher->m_cActiveRockets++;// register this missile as active for the launcher

	return pRocket;
}

void CRpgRocket::Precache( void )
{
	PRECACHE_MODEL( "models/weapons/rpgrocket.mdl" );
	m_iTrail = PRECACHE_MODEL( "sprites/smoke.spr" );
	PRECACHE_SOUND( "weapons/rocket1.wav" );
	PRECACHE_SOUND( "weapons/rpg_shotdown.wav" );
}

void CRpgRocket :: Spawn( void )
{
	Precache( );
	// motor
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	// diffusion - helicopter can take down this rocket now
	pev->takedamage = DAMAGE_AIM;

	SET_MODEL(ENT(pev), "models/weapons/rpgrocket.mdl");
//	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));
	UTIL_SetSize( this, -Vector( 4, 4, 4 ), Vector( 4, 4, 4 ) );
	RelinkEntity( TRUE );

	pev->classname = MAKE_STRING("rpg_rocket");

	SetThink(&CRpgRocket::IgniteThink );
	SetTouch( &CGrenade::ExplodeTouch );

	Vector angles = GetLocalAngles();

	if( !(pev->effects & EF_UPSIDEDOWN) )
		angles.x -= 30;

	UTIL_MakeVectors( angles );

	SetLocalVelocity( gpGlobals->v_forward * 250 );
	pev->gravity = 0.5;

	SetFlag(F_NOBACKCULL);

	SetNextThink( 0.4 );

	pev->dmg = 135;
	
	if( pev->owner )
	{
		m_hOwner = Instance( pev->owner );
		m_iClass = m_hOwner->Classify();
		if( m_iClass == CLASS_PLAYER )
			m_iClass = CLASS_PLAYER_ALLY;
	}
}

int	CRpgRocket::Classify( void )
{
	return m_iClass;
}

int CRpgRocket::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if( pevAttacker && !FClassnameIs( pevAttacker, "monster_apache" ) )
		return 0;

	pev->takedamage = DAMAGE_NO;

	ShotDown();

	return 0;
}

//==================================================================================
// ShotDown: Causes the missile to spiral to the ground and explode, due to damage
//==================================================================================
void CRpgRocket::ShotDown( void )
{
	SetThink( &CRpgRocket::AugerThink );
	SetNextThink( 0 );
	m_flAugerTime = gpGlobals->time + 1.5f;
	m_flMarkDeadTime = gpGlobals->time + 0.75;

	// Let the RPG start reloading immediately
	// diffusion - FIXME sometimes m_cActiveRockets can become -1, likely because of this. So I put the check > 0 to m_cActiveRockets in needed places.
	if( m_pLauncher )
		m_pLauncher->m_cActiveRockets--;

	m_hOwner = NULL;

	EMIT_SOUND( edict(), CHAN_BODY, "weapons/rpg_shotdown.wav", 1, 0.25 );
}

void CRpgRocket::AugerThink( void )
{
	// If we've augered long enough, then just explode
	if( m_flAugerTime < gpGlobals->time )
	{
		Detonate();
		return;
	}

	if( m_flMarkDeadTime < gpGlobals->time )
		pev->deadflag = DEAD_DYING;

	Vector angles = GetLocalAngles();

	angles.y += RANDOM_FLOAT( -AUGER_YDEVIANCE, AUGER_YDEVIANCE );
	angles.x += RANDOM_FLOAT( -AUGER_XDEVIANCEDOWN, AUGER_XDEVIANCEUP );

	SetLocalAngles( angles );

	Vector vecForward;

	g_engfuncs.pfnAngleVectors( GetLocalAngles(), vecForward, NULL, NULL );

	SetAbsVelocity( vecForward * 1000.0f );

	Vector vecOrg = GetAbsOrigin();
	MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, vecOrg );
		WRITE_BYTE( TE_SMOKE );
		WRITE_COORD( vecOrg.x );
		WRITE_COORD( vecOrg.y );
		WRITE_COORD( vecOrg.z );
		WRITE_SHORT( g_sModelIndexSmoke );
		WRITE_BYTE( RANDOM_LONG( 30, 40 ) ); // scale x1.0
		WRITE_BYTE( 0 ); // framerate
		WRITE_BYTE( 2 ); // pos randomize
	MESSAGE_END();

	SetNextThink( 0.05f );
}

//=========================================================
//=========================================================
void CRpgRocket::RocketTouch ( CBaseEntity *pOther )
{
	if ( m_pLauncher )
	{
		// my launcher is still around, tell it I'm dead.
		m_pLauncher->m_cActiveRockets--;
	}

	STOP_SOUND( edict(), CHAN_VOICE, "weapons/rocket1.wav" );
	ExplodeTouch( pOther );
}

void CRpgRocket :: CreateTrail( void )
{
	// rocket trail
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );

		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT(entindex());	// entity
		WRITE_SHORT(m_iTrail );	// model
		WRITE_BYTE( 40 ); // life
		WRITE_BYTE( 5 );  // width
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );	// brightness

	MESSAGE_END();  // move PHS/PVS data sending into here (SEND_ALL, SEND_PVS, SEND_PHS)
}

void CRpgRocket :: IgniteThink( void  )
{
	// pev->movetype = MOVETYPE_TOSS;

	pev->movetype = MOVETYPE_FLY;
	pev->effects |= EF_LIGHT;

	// make rocket sound
	EMIT_SOUND( edict(), CHAN_VOICE, "weapons/rocket1.wav", 1, 0.5 );

	CreateTrail();

	m_flIgniteTime = gpGlobals->time;

	// set to follow laser spot
	SetThink(&CRpgRocket::FollowThink );
	SetNextThink( 0.1 );
}

void CRpgRocket :: OnTeleport( void )
{
	// re-aiming the projectile
	SetLocalAngles( UTIL_VecToAngles( GetLocalVelocity( ) ));

	MESSAGE_BEGIN( MSG_ALL, SVC_TEMPENTITY );
		WRITE_BYTE( TE_KILLBEAM );
		WRITE_ENTITY( entindex() );
	MESSAGE_END();

	CreateTrail();
}

void CRpgRocket :: FollowThink( void  )
{
	CBaseEntity *pOther = NULL;
	Vector vecTarget;
	Vector vecDir;
	float flDist, flMax, flDot;
	TraceResult tr;

	Vector angles = GetLocalAngles();
	UTIL_MakeVectors( angles );

	vecTarget = gpGlobals->v_forward;
	flMax = 4096;
	
	// Examine all entities within a reasonable radius
	while ((pOther = UTIL_FindEntityByClassname( pOther, "laser_spot" )) != NULL)
	{
		UTIL_TraceLine ( GetAbsOrigin(), pOther->GetAbsOrigin(), dont_ignore_monsters, ENT(pev), &tr );
		// ALERT( at_console, "%f\n", tr.flFraction );
		if( tr.flFraction >= 0.90 )
		{
			vecDir = pOther->GetAbsOrigin() - GetAbsOrigin();
			flDist = vecDir.Length( );
			vecDir = vecDir.Normalize( );
			flDot = DotProduct( gpGlobals->v_forward, vecDir );
			if ((flDot > 0) && (flDist * (1 - flDot) < flMax))
			{
				flMax = flDist * (1 - flDot);
				vecTarget = vecDir;
			}
		}
	}

	SetLocalAngles( UTIL_VecToAngles( vecTarget ));

	// this acceleration and turning math is totally wrong, but it seems to respond well so don't change it.
	float flSpeed = GetLocalVelocity().Length();

	if( gpGlobals->time - m_flIgniteTime < 1.0 )
	{
		SetLocalVelocity( GetLocalVelocity() * 0.2 + vecTarget * (flSpeed * 0.8 + 400));
		if (pev->waterlevel == 3)
		{
			// go slow underwater
			if( GetLocalVelocity().Length() > 300 )
				SetLocalVelocity( GetLocalVelocity().Normalize() * 300 );

			UTIL_BubbleTrail( GetAbsOrigin() - GetAbsVelocity() * 0.1, GetAbsOrigin(), 4 );
		} 
		else 
		{
			if( GetLocalVelocity().Length() > 2000 )
				SetLocalVelocity( GetLocalVelocity().Normalize() * 2000 );
		}
	}
	else
	{
		/* // diffusion - moved to ClearEffects
		if( pev->effects & EF_LIGHT )
		{
			pev->effects = 0;
			STOP_SOUND( ENT(pev), CHAN_VOICE, "weapons/rocket1.wav" );
		}
		*/
		SetLocalVelocity( GetLocalVelocity() * 0.2 + vecTarget * flSpeed * 0.798 );

		if( pev->waterlevel == 0 && GetLocalVelocity().Length() < 1500 )
		{
			if (m_pLauncher) // diffusion - fix by Lev
			{
				// my launcher is still around, tell it I'm dead.
				m_pLauncher->m_cActiveRockets--;
			}

			if( pev->effects & EF_LIGHT )
			{
				pev->effects = 0;
				STOP_SOUND( ENT( pev ), CHAN_VOICE, "weapons/rocket1.wav" );
			}

			Detonate( );
		}
	}
	// ALERT( at_console, "%.0f\n", flSpeed );

	SetNextThink( 0.1 );
}

void CRpgRocket::ClearEffects(void)
{
	if( pev->effects & EF_LIGHT )
	{
		pev->effects = 0;
		STOP_SOUND( ENT( pev ), CHAN_VOICE, "weapons/rocket1.wav" );
	}
}





















//----------------------------------------------------------------------------------------
class CRoboRocket2 : public CGrenade
{
	DECLARE_CLASS( CRoboRocket2, CGrenade );
public:
	void Spawn( void );
	void Precache( void );
	void FollowThink( void );
	void IgniteThink( void );
	void RocketTouch( CBaseEntity *pOther );
	void RocketExplode( TraceResult *pTrace, int bitsDamageType );
	void CreateTrail( void );
	virtual void OnTeleport( void );

	DECLARE_DATADESC();

	int m_iTrail;
	float m_flIgniteTime;
};

LINK_ENTITY_TO_CLASS( robo_rocket2, CRoboRocket2 );

BEGIN_DATADESC( CRoboRocket2 )
	DEFINE_FIELD( m_flIgniteTime, FIELD_TIME ),
	DEFINE_FUNCTION( FollowThink ),
	DEFINE_FUNCTION( IgniteThink ),
	DEFINE_FUNCTION( RocketTouch ),
	DEFINE_FUNCTION( RocketExplode ),
END_DATADESC()

const int RoboRocketDamage[] = 
{
	0,
	30,
	40,
	50,
};

void CRoboRocket2 :: Spawn( void )
{
	Precache( );
	// motor
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;
	SetFlag(F_NOBACKCULL);

	SET_MODEL(ENT(pev), "models/weapons/rpgrocket.mdl");
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));
	RelinkEntity( TRUE );

	SetThink( &CRoboRocket2::IgniteThink );
	SetTouch(&CRoboRocket2::RocketTouch );

	Vector angles = GetLocalAngles();

	angles.x -= 30;
	UTIL_MakeVectors( angles );

	SetLocalVelocity( gpGlobals->v_forward * 250 );
	pev->gravity = 0.5;

	SetNextThink( 0.4 );

	pev->dmg = RoboRocketDamage[g_iSkillLevel];

	if( pev->owner )
	{
		m_hOwner = Instance( pev->owner );
		m_iClass = m_hOwner->Classify();
	}
}

//=========================================================
//=========================================================
void CRoboRocket2 :: RocketTouch ( CBaseEntity *pOther )
{
	STOP_SOUND( edict(), CHAN_VOICE, "weapons/rocket1.wav" );
	TraceResult tr;
	Vector		vecSpot;// trace starts here!
	pev->enemy = pOther->edict();
	vecSpot = GetAbsOrigin() - GetAbsVelocity().Normalize() * 32;
	UTIL_TraceLine( vecSpot, vecSpot + GetAbsVelocity().Normalize() * 64, ignore_monsters, edict(), &tr );
	RocketExplode( &tr, DMG_BLAST );
}

// diffusion - all this work just to make custom explosion radius, lolz
void CRoboRocket2::RocketExplode( TraceResult *pTrace, int bitsDamageType )
{
	pev->model = iStringNull;//invisible
	pev->solid = SOLID_NOT;// intangible

	pev->takedamage = DAMAGE_NO;

	// Pull out of the wall a bit
	if ( pTrace->flFraction != 1.0 )
	{
		float vecPlaneScale = (pev->dmg - 24) * 0.6f;
		if( vecPlaneScale < 1 )
			vecPlaneScale = 1;
		SetAbsOrigin( pTrace->vecEndPos + (pTrace->vecPlaneNormal * vecPlaneScale ));
	}

	Vector absOrigin = GetAbsOrigin();

	int iContents = UTIL_PointContents ( absOrigin );
	
	MESSAGE_BEGIN( MSG_PAS, gmsgTempEnt, absOrigin );
		WRITE_BYTE( TE_EXPLOSION );		// This makes a dynamic light and the explosion sprites/sound
		WRITE_COORD( absOrigin.x );	// Send to PAS because of the sound
		WRITE_COORD( absOrigin.y );
		WRITE_COORD( absOrigin.z );

		if( iContents != CONTENTS_WATER )
		{
			WRITE_SHORT( g_sModelIndexFireball );
		}
		else
		{
			WRITE_SHORT( g_sModelIndexWExplosion );
		}
		int spritescale = (pev->dmg - 50) * .60;
		if( spritescale < 20 )
			spritescale = 20;
		WRITE_BYTE( spritescale ); // scale * 10
		WRITE_BYTE( 15  ); // framerate
		WRITE_BYTE( TE_EXPLFLAG_NONE );
	MESSAGE_END();

	int sndentindex = 0;
	if( pev->owner )
		sndentindex = ENTINDEX( pev->owner );
	CSoundEnt::InsertSound ( bits_SOUND_COMBAT, absOrigin, NORMAL_EXPLOSION_VOLUME, 3.0, sndentindex );
	entvars_t *pevOwner;
	if ( pev->owner )
		pevOwner = VARS( pev->owner );
	else
		pevOwner = NULL;

	pev->owner = NULL; // can't traceline attack owner if this is set

	RadiusDamage ( pev, pevOwner, pev->dmg, pev->dmg * 10, m_iClass, bitsDamageType );

	CBaseEntity *pEntity = CBaseEntity::Instance( pTrace->pHit );

	if ( RANDOM_FLOAT( 0 , 1 ) < 0.5 )
	{
		if( pEntity && UTIL_GetModelType( pEntity->pev->modelindex ) == mod_studio )
			UTIL_StudioDecalTrace( pTrace, DECAL_SCORCH1 );
		else UTIL_DecalTrace( pTrace, DECAL_SCORCH1 );
	}
	else
	{
		if( pEntity && UTIL_GetModelType( pEntity->pev->modelindex ) == mod_studio )
			UTIL_StudioDecalTrace( pTrace, DECAL_SCORCH2 );
		else UTIL_DecalTrace( pTrace, DECAL_SCORCH2 );
	}

	switch ( RANDOM_LONG( 0, 2 ) )
	{
		case 0:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/debris1.wav", 0.55, ATTN_NORM);	break;
		case 1:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/debris2.wav", 0.55, ATTN_NORM);	break;
		case 2:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/debris3.wav", 0.55, ATTN_NORM);	break;
	}

	pev->effects |= EF_NODRAW;
	SetThink( &CGrenade::Smoke );
	SetAbsVelocity( g_vecZero );
	pev->nextthink = gpGlobals->time + 0.3;

	if (iContents != CONTENTS_WATER)
	{
		int sparkCount = RANDOM_LONG(0,3);
		for ( int i = 0; i < sparkCount; i++ )
			Create( "spark_shower", absOrigin, pTrace->vecPlaneNormal, NULL );
	}
}

//=========================================================
//=========================================================
void CRoboRocket2 :: Precache( void )
{
	PRECACHE_MODEL("models/weapons/rpgrocket.mdl");
	m_iTrail = PRECACHE_MODEL("sprites/smoke.spr");
	PRECACHE_SOUND ("weapons/rocket1.wav");
}

void CRoboRocket2 :: CreateTrail( void )
{
	// rocket trail
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );

		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT(entindex());	// entity
		WRITE_SHORT(m_iTrail );	// model
		WRITE_BYTE( 40 ); // life
		WRITE_BYTE( 5 );  // width
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );	// brightness

	MESSAGE_END();  // move PHS/PVS data sending into here (SEND_ALL, SEND_PVS, SEND_PHS)
}

void CRoboRocket2 :: IgniteThink( void  )
{
	// pev->movetype = MOVETYPE_TOSS;

	pev->movetype = MOVETYPE_FLY;
	pev->effects |= EF_LIGHT;

	// make rocket sound
	EMIT_SOUND( edict(), CHAN_VOICE, "weapons/rocket1.wav", 1, 0.5 );

	CreateTrail();

	m_flIgniteTime = gpGlobals->time;

	// set to follow laser spot
	SetThink(&CRoboRocket2::FollowThink );
	SetNextThink( 0.1 );
}

void CRoboRocket2 :: OnTeleport( void )
{
	// re-aiming the projectile
	SetLocalAngles( UTIL_VecToAngles( GetLocalVelocity( ) ));

	MESSAGE_BEGIN( MSG_ALL, SVC_TEMPENTITY );
		WRITE_BYTE( TE_KILLBEAM );
		WRITE_ENTITY( entindex() );
	MESSAGE_END();

	CreateTrail();
}

void CRoboRocket2 :: FollowThink( void  )
{
//	CBaseEntity *pOther = NULL;

	Vector vecTarget;
	Vector vecDir;
	
	float flDist, flMax, flDot;
	TraceResult tr;

	Vector angles = GetLocalAngles();
	UTIL_MakeVectors( angles );

	vecTarget = gpGlobals->v_forward;
	flMax = 4096;
	
	/* disable auto-following
	if (m_hEnemy != NULL)
	{
		Vector vecEnemyOrigin = m_hEnemy->GetAbsOrigin();
		UTIL_TraceLine ( GetAbsOrigin(), m_hEnemy->GetAbsOrigin(), dont_ignore_monsters, ENT(pev), &tr );
		// ALERT( at_console, "%f\n", tr.flFraction );
		if( tr.flFraction >= 0.90 )
		{
			if( FBitSet( m_hEnemy->pev->flags, FL_ONGROUND ) )
				vecEnemyOrigin.z -= 10; // shoot under their feet

			if( m_hEnemy->IsPlayer() )
				vecEnemyOrigin.z -= 20; // -30 in total if player is on ground
			
			vecDir = vecEnemyOrigin - GetAbsOrigin();
			flDist = vecDir.Length( );
			vecDir = vecDir.Normalize( );
			flDot = DotProduct( gpGlobals->v_forward, vecDir );
			if ((flDot > 0) && (flDist * (1 - flDot) < flMax))
			{
				flMax = flDist * (1 - flDot);
				vecTarget = vecDir;
			}
		}
	}*/

	SetLocalAngles( UTIL_VecToAngles( vecTarget ));

	// this acceleration and turning math is totally wrong, but it seems to respond well so don't change it.
	float flSpeed = GetLocalVelocity().Length();

	if( gpGlobals->time - m_flIgniteTime < 1.0 )
	{
		SetLocalVelocity( GetLocalVelocity() * 0.2 + vecTarget * (flSpeed * 0.8 + 400));
		if (pev->waterlevel == 3)
		{
			// go slow underwater
			if( GetLocalVelocity().Length() > 300 )
			{
				SetLocalVelocity( GetLocalVelocity().Normalize() * 300 );
			}
			UTIL_BubbleTrail( GetAbsOrigin() - GetAbsVelocity() * 0.1, GetAbsOrigin(), 4 );
		} 
		else 
		{
			if( GetLocalVelocity().Length() > 2000 )
			{
				SetLocalVelocity( GetLocalVelocity().Normalize() * 2000 );
			}
		}
	}
	else
	{
		if( pev->effects & EF_LIGHT )
		{
			pev->effects = 0;
			STOP_SOUND( ENT(pev), CHAN_VOICE, "weapons/rocket1.wav" );
		}

		SetLocalVelocity( GetLocalVelocity() * 0.2 + vecTarget * flSpeed * 0.798 );

		if( pev->waterlevel == 0 && GetLocalVelocity().Length() < 1500 )
		{
			Detonate( );
		}
	}
	// ALERT( at_console, "%.0f\n", flSpeed );

	SetNextThink( 0.1 );
}

//----------------------------------------------------------------------------------------

// rocket for heavy drone

const int DroneRocketDamage[] =
{
	0,
	20,
	25,
	30
};

class CDroneRocket: public CGrenade
{
	DECLARE_CLASS( CDroneRocket, CGrenade );
public:
	void Spawn( void );
	void Precache( void );
	void FlyThink( void );
	void IgniteThink( void );
	void RocketTouch( CBaseEntity *pOther );
	void RocketExplode( TraceResult *pTrace, int bitsDamageType );
	void CreateTrail( void );
	virtual void OnTeleport( void );

	DECLARE_DATADESC();

	int m_iTrail;
	float m_flIgniteTime;
};

LINK_ENTITY_TO_CLASS( drone_rocket, CDroneRocket );

BEGIN_DATADESC( CDroneRocket )
	DEFINE_FIELD( m_flIgniteTime, FIELD_TIME ),
	DEFINE_FUNCTION( FlyThink ),
	DEFINE_FUNCTION( IgniteThink ),
	DEFINE_FUNCTION( RocketTouch ),
	DEFINE_FUNCTION( RocketExplode ),
END_DATADESC()

void CDroneRocket::Spawn( void )
{
	Precache();
	// motor
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;
	SetFlag( F_NOBACKCULL );

	SET_MODEL( ENT( pev ), "models/weapons/rpgrocket.mdl" );
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
	RelinkEntity( TRUE );

	pev->scale = 0.5;

	SetThink( &CDroneRocket::IgniteThink );
	SetTouch( &CDroneRocket::RocketTouch );

	Vector angles = GetLocalAngles();

	angles.x -= 30;
	UTIL_MakeVectors( angles );

	SetLocalVelocity( gpGlobals->v_forward * 250 );
	pev->gravity = 0.5;

	SetNextThink( 0.4 );

	pev->dmg = DroneRocketDamage[g_iSkillLevel];

	if( pev->owner )
	{
		m_hOwner = Instance( pev->owner );
		m_iClass = m_hOwner->Classify();
	}
}

void CDroneRocket::RocketTouch( CBaseEntity *pOther )
{
	STOP_SOUND( edict(), CHAN_VOICE, "weapons/rocket1.wav" );
	TraceResult tr;
	pev->enemy = pOther->edict();
	Vector vecSpot = GetAbsOrigin() - GetAbsVelocity().Normalize() * 32;
	UTIL_TraceLine( vecSpot, vecSpot + GetAbsVelocity().Normalize() * 64, ignore_monsters, edict(), &tr );
	RocketExplode( &tr, DMG_BLAST );
}

void CDroneRocket::RocketExplode( TraceResult *pTrace, int bitsDamageType )
{
	pev->model = iStringNull;//invisible
	pev->solid = SOLID_NOT;// intangible

	pev->takedamage = DAMAGE_NO;

	// Pull out of the wall a bit
	if( pTrace->flFraction != 1.0 )
	{
		float vecPlaneScale = (pev->dmg - 24) * 0.6f;
		if( vecPlaneScale < 1 )
			vecPlaneScale = 1;
		SetAbsOrigin( pTrace->vecEndPos + (pTrace->vecPlaneNormal * vecPlaneScale) );
	}

	Vector absOrigin = GetAbsOrigin();

	int iContents = UTIL_PointContents( absOrigin );

	MESSAGE_BEGIN( MSG_PAS, gmsgTempEnt, absOrigin );
	WRITE_BYTE( TE_EXPLOSION );		// This makes a dynamic light and the explosion sprites/sound
	WRITE_COORD( absOrigin.x );	// Send to PAS because of the sound
	WRITE_COORD( absOrigin.y );
	WRITE_COORD( absOrigin.z );

	if( iContents != CONTENTS_WATER )
	{
		WRITE_SHORT( g_sModelIndexFireball );
	}
	else
	{
		WRITE_SHORT( g_sModelIndexWExplosion );
	}
	int spritescale = (pev->dmg - 50) * .60;
	if( spritescale < 20 )
		spritescale = 20;
	WRITE_BYTE( spritescale ); // scale * 10
	WRITE_BYTE( 15 ); // framerate
	WRITE_BYTE( TE_EXPLFLAG_NONE );
	MESSAGE_END();

	int sndentindex = 0;
	if( pev->owner )
		sndentindex = ENTINDEX( pev->owner );
	CSoundEnt::InsertSound( bits_SOUND_COMBAT, absOrigin, NORMAL_EXPLOSION_VOLUME, 3.0, sndentindex );
	entvars_t *pevOwner;
	if( pev->owner )
		pevOwner = VARS( pev->owner );
	else
		pevOwner = NULL;

	pev->owner = NULL; // can't traceline attack owner if this is set

	RadiusDamage( pev, pevOwner, pev->dmg, 500, m_iClass, bitsDamageType );

	CBaseEntity *pEntity = CBaseEntity::Instance( pTrace->pHit );

	if( RANDOM_FLOAT( 0, 1 ) < 0.5 )
	{
		if( pEntity && UTIL_GetModelType( pEntity->pev->modelindex ) == mod_studio )
			UTIL_StudioDecalTrace( pTrace, DECAL_SCORCH1 );
		else UTIL_DecalTrace( pTrace, DECAL_SCORCH1 );
	}
	else
	{
		if( pEntity && UTIL_GetModelType( pEntity->pev->modelindex ) == mod_studio )
			UTIL_StudioDecalTrace( pTrace, DECAL_SCORCH2 );
		else UTIL_DecalTrace( pTrace, DECAL_SCORCH2 );
	}

	switch( RANDOM_LONG( 0, 2 ) )
	{
	case 0:	EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/debris1.wav", 0.55, ATTN_NORM );	break;
	case 1:	EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/debris2.wav", 0.55, ATTN_NORM );	break;
	case 2:	EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/debris3.wav", 0.55, ATTN_NORM );	break;
	}

	pev->effects |= EF_NODRAW;
	SetThink( &CGrenade::Smoke );
	SetAbsVelocity( g_vecZero );
	pev->nextthink = gpGlobals->time + 0.3;

	if( iContents != CONTENTS_WATER )
	{
		int sparkCount = RANDOM_LONG( 0, 3 );
		for( int i = 0; i < sparkCount; i++ )
			Create( "spark_shower", absOrigin, pTrace->vecPlaneNormal, NULL );
	}
}

void CDroneRocket::Precache( void )
{
	PRECACHE_MODEL( "models/weapons/rpgrocket.mdl" );
	m_iTrail = PRECACHE_MODEL( "sprites/smoke.spr" );
	PRECACHE_SOUND( "weapons/rocket1.wav" );
}

void CDroneRocket::CreateTrail( void )
{
	// rocket trail
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( entindex() );	// entity
		WRITE_SHORT( m_iTrail );	// model
		WRITE_BYTE( 25 ); // life
		WRITE_BYTE( 2 );  // width
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );	// brightness
	MESSAGE_END();  // move PHS/PVS data sending into here (SEND_ALL, SEND_PVS, SEND_PHS)
}

void CDroneRocket::IgniteThink( void )
{
	pev->movetype = MOVETYPE_FLY;
	pev->effects |= EF_LIGHT;

	// make rocket sound
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, "weapons/rocket1.wav", 0.6, 0.5, 0, 130 ); // higher pitch

	CreateTrail();

	m_flIgniteTime = gpGlobals->time;

	SetThink( &CDroneRocket::FlyThink );
	SetNextThink( 0 );
}

void CDroneRocket::OnTeleport( void )
{
	// re-aiming the projectile
	SetLocalAngles( UTIL_VecToAngles( GetLocalVelocity() ) );

	MESSAGE_BEGIN( MSG_ALL, SVC_TEMPENTITY );
		WRITE_BYTE( TE_KILLBEAM );
		WRITE_ENTITY( entindex() );
	MESSAGE_END();

	CreateTrail();
}

void CDroneRocket::FlyThink( void )
{
	Vector vecTarget;
	Vector vecDir;

	TraceResult tr;

	Vector angles = GetLocalAngles();
	UTIL_MakeVectors( angles );

	vecTarget = gpGlobals->v_forward;

	SetLocalAngles( UTIL_VecToAngles( vecTarget ) );

	// this acceleration and turning math is totally wrong, but it seems to respond well so don't change it.
	float flSpeed = GetLocalVelocity().Length();

	if( gpGlobals->time - m_flIgniteTime < 1.0 )
	{
		SetLocalVelocity( vecTarget * 3000 );
		if( pev->waterlevel == 3 )
		{
			// go slow underwater
			if( GetLocalVelocity().Length() > 300 )
			{
				SetLocalVelocity( GetLocalVelocity().Normalize() * 300 );
			}
			UTIL_BubbleTrail( GetAbsOrigin() - GetAbsVelocity() * 0.1, GetAbsOrigin(), 4 );
		}
		else
		{
			if( GetLocalVelocity().Length() > 2000 )
			{
				SetLocalVelocity( GetLocalVelocity().Normalize() * 2000 );
			}
		}
	}
	else
	{
		if( pev->effects & EF_LIGHT )
		{
			pev->effects = 0;
			STOP_SOUND( ENT( pev ), CHAN_VOICE, "weapons/rocket1.wav" );
		}

		SetLocalVelocity( GetLocalVelocity() * 0.2 + vecTarget * flSpeed * 0.798 );

		if( pev->waterlevel == 0 && GetLocalVelocity().Length() < 800 )
		{
			Detonate();
		}
	}
	// ALERT( at_console, "%.0f\n", flSpeed );

	SetNextThink( 0.1 );
}







void CRpg::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_RPG;

	SET_MODEL(ENT(pev), "models/weapons/w_rpg.mdl");
	m_fSpotActive = 1;

	if ( g_pGameRules->IsMultiplayer() )
	{
		// more default ammo in multiplay. 
		m_iDefaultAmmo = RPG_DEFAULT_GIVE * 2;
	}
	else
	{
		m_iDefaultAmmo = RPG_DEFAULT_GIVE;
	}

	FallInit();// get ready to fall down.
}

void CRpg::Precache( void )
{
	PRECACHE_MODEL("models/weapons/w_rpg.mdl");
	PRECACHE_MODEL("models/weapons/v_rpg.mdl");
	PRECACHE_MODEL("models/weapons/p_rpg.mdl");

	PRECACHE_SOUND("items/9mmclip1.wav");

	UTIL_PrecacheOther( "laser_spot" );
	UTIL_PrecacheOther( "rpg_rocket" );

	PRECACHE_SOUND("weapons/rocketfire1.wav");
	PRECACHE_SOUND("weapons/glauncher.wav"); // alternative fire sound
}

int CRpg::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "rockets";
	p->iMaxAmmo1 = ROCKET_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = RPG_MAX_CLIP;
	p->iSlot = WPN_SLOT_RPG;
	p->iPosition = WPN_POS_RPG;
	p->iId = m_iId = WEAPON_RPG;
	p->iFlags = 0;
	p->iWeight = RPG_WEIGHT;

	return 1;
}

int CRpg::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
			WRITE_BYTE( m_iId );
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}

BOOL CRpg::Deploy( )
{
	m_flTimeWeaponIdle = gpGlobals->time + 5.0;
	m_flNextPrimaryAttack = gpGlobals->time + DEFAULT_DEPLOY_TIME;

	if ( m_iClip == 0 )
		return DefaultDeploy( "models/weapons/v_rpg.mdl", "models/weapons/p_rpg.mdl", RPG_DRAW_UL, "rpg" );

	return DefaultDeploy( "models/weapons/v_rpg.mdl", "models/weapons/p_rpg.mdl", RPG_DRAW1, "rpg" );
}


BOOL CRpg::CanHolster( void )
{
	if ( m_fSpotActive && m_cActiveRockets > 0 )
	{
		// can't put away while guiding a missile.
		return FALSE;
	}

	return TRUE;
}

void CRpg::Holster( void )
{
	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
	// m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );
	SendWeaponAnim( RPG_HOLSTER1 );
	if (m_pSpot)
	{
		m_pSpot->Killed( NULL, GIB_NEVER );
		m_pSpot = NULL;
	}

	BaseClass::Holster();
}

void CRpg::PrimaryAttack()
{
	if (m_iClip)
	{
		m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

		SendWeaponAnim( RPG_FIRE );

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		UTIL_MakeVectors( m_pPlayer->pev->v_angle );
		Vector vecSrc = m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 16 + gpGlobals->v_right * 8 + gpGlobals->v_up * -8;
		
		CRpgRocket *pRocket = CRpgRocket::CreateRpgRocket( vecSrc, m_pPlayer->pev->v_angle, m_pPlayer, this );

		UTIL_MakeVectors( m_pPlayer->pev->v_angle );// RpgRocket::Create stomps on globals, so remake.
		if( pRocket ) // diffusion - safety check
		{
			pRocket->SetLocalVelocity( pRocket->GetLocalVelocity() + gpGlobals->v_forward * DotProduct( m_pPlayer->GetAbsVelocity(), gpGlobals->v_forward ) );

			// firing RPG no longer turns on the designator. ALT fire is a toggle switch for the LTD.
			// Ken signed up for this as a global change (sjb)
		//	EMIT_SOUND( m_pPlayer->edict(), CHAN_WEAPON, "weapons/rocketfire1.wav", 0.9, ATTN_NORM );
		//	EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/glauncher.wav", 0.7, ATTN_NORM );
			PlayClientSound( m_pPlayer, WEAPON_RPG );

			if( m_pPlayer->LoudWeaponsRestricted )
				m_pPlayer->FireLoudWeaponRestrictionEntity();

			m_iClip--;

			m_flNextPrimaryAttack = gpGlobals->time + 1.5;
			m_flTimeWeaponIdle = gpGlobals->time + 1.5;
			m_pPlayer->pev->punchangle.x -= 5;
		}
	}
	else
	{
		CLIENT_COMMAND(m_pPlayer->edict(), "-attack\n");
		PlayEmptySound( );
		m_flNextPrimaryAttack = gpGlobals->time + 0.2;
	}
	UpdateSpot( );
}

void CRpg::SecondaryAttack()
{
	m_fSpotActive = ! m_fSpotActive;

	if (!m_fSpotActive && m_pSpot)
	{
		m_pSpot->Killed( NULL, GIB_NORMAL );
		m_pSpot = NULL;
	}

	m_flNextSecondaryAttack = gpGlobals->time + 0.2;
}

void CRpg::Reload( void )
{
	int iResult;

	if ( m_iClip == 1 )
	{
		// don't bother with any of this if don't need to reload.
		return;
	}

	// because the RPG waits to autoreload when no missiles are active while  the LTD is on, the
	// weapons code is constantly calling into this function, but is often denied because 
	// a) missiles are in flight, but the LTD is on
	// or
	// b) player is totally out of ammo and has nothing to switch to, and should be allowed to
	//    shine the designator around
	//
	// Set the next attack time into the future so that WeaponIdle will get called more often
	// than reload, allowing the RPG LTD to be updated
	
	m_flNextPrimaryAttack = gpGlobals->time + RPG_RELOAD_TIME;

	if ( m_cActiveRockets > 0 && m_fSpotActive )
	{
		// no reloading when there are active missiles tracking the designator.
		// ward off future autoreload attempts by setting next attack time into the future for a bit. 
		return;
	}

	if (m_pSpot && m_fSpotActive)
	{
		m_pSpot->Suspend( RPG_RELOAD_TIME );
		m_flNextSecondaryAttack = gpGlobals->time + RPG_RELOAD_TIME;
	}

	if (m_iClip == 0)
	{
		iResult = DefaultReload( RPG_MAX_CLIP, RPG_RELOAD, RPG_RELOAD_TIME );
	}

	if (iResult)
	{
		m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );
	}
}

void CRpg::WeaponIdle( void )
{
	UpdateSpot( );

	ResetEmptySound( );

	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		int iAnim;
		float flRand = RANDOM_FLOAT(0, 1);
		if (flRand <= 0.75 || m_fSpotActive)
		{
			if ( m_iClip == 0 )
				iAnim = RPG_IDLE_UL;
			else
				iAnim = RPG_IDLE;

			m_flTimeWeaponIdle = gpGlobals->time + 90.0 / 15.0;
		}
		else
		{
			if ( m_iClip == 0 )
				iAnim = RPG_FIDGET_UL;
			else
				iAnim = RPG_FIDGET;

			m_flTimeWeaponIdle = gpGlobals->time + 3.0;
		}

		SendWeaponAnim( iAnim );
	}
	else
	{
		m_flTimeWeaponIdle = gpGlobals->time + 1;
	}
}

void CRpg::UpdateSpot( void )
{
	if (m_fSpotActive)
	{
		if (!m_pSpot)
		{
			m_pSpot = CLaserSpot::CreateSpot();

			if(m_pSpot) // diffusion - I need this to filter the spot in Addtofullpack
				m_pSpot->pev->owner = m_pPlayer->edict();
		}

		UTIL_MakeVectors( m_pPlayer->pev->v_angle );
		Vector vecSrc = m_pPlayer->GetGunPosition( );
		Vector vecAiming = gpGlobals->v_forward;

		TraceResult tr;
		UTIL_TraceLine ( vecSrc, vecSrc + vecAiming * 8192, dont_ignore_monsters, ENT(m_pPlayer->pev), &tr );
		UTIL_SetOrigin( m_pSpot, tr.vecEndPos );
	}
}

class CRpgAmmo : public CBasePlayerAmmo
{
	DECLARE_CLASS( CRpgAmmo, CBasePlayerAmmo );

	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/weapons/w_rpgammo.mdl");
		BaseClass::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/weapons/w_rpgammo.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		int iGive;

		if ( g_pGameRules->IsMultiplayer() )
		{
			// hand out more ammo per rocket in multiplayer.
			iGive = AMMO_RPGCLIP_GIVE * 2;
		}
		else
		{
			iGive = AMMO_RPGCLIP_GIVE;
		}

		if (pOther->GiveAmmo( iGive, "rockets", ROCKET_MAX_CARRY ) != -1)
		{
		//	EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			PlayPickupSound( pOther );
			return TRUE;
		}
		return FALSE;
	}
};

LINK_ENTITY_TO_CLASS( ammo_rpgclip, CRpgAmmo );