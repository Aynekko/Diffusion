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

#define BOLT_AIR_VELOCITY	3500
#define BOLT_WATER_VELOCITY	1500
#define CROSSBOW_MAX_ZOOM 20

class CCrossbowBolt : public CBaseEntity
{
	DECLARE_CLASS( CCrossbowBolt, CBaseEntity );

	void Spawn( void );
	void Precache( void );
	int Classify ( void );
	void BubbleThink( void );
	void BoltTouch( CBaseEntity *pOther );
	void ExplodeThink( void );
	virtual BOOL IsProjectile( void ) { return TRUE; }
	void TransferReset( void );
	void OnChangeLevel( void );
	void OnTeleport( void );
	void PlayTouchSound( CBaseEntity *pOther );
	int m_iTrail;

	DECLARE_DATADESC();
public:
	static CCrossbowBolt *BoltCreate( void );
};

LINK_ENTITY_TO_CLASS( crossbow_bolt, CCrossbowBolt );

BEGIN_DATADESC( CCrossbowBolt )
	DEFINE_FUNCTION( BubbleThink ),
	DEFINE_FUNCTION( ExplodeThink ),
	DEFINE_FUNCTION( BoltTouch ),
	DEFINE_FUNCTION( PlayTouchSound ),
END_DATADESC()

CCrossbowBolt *CCrossbowBolt::BoltCreate( void )
{
	// Create a new entity with CCrossbowBolt private data
	CCrossbowBolt *pBolt = GetClassPtr( (CCrossbowBolt *)NULL );
	pBolt->pev->classname = MAKE_STRING( "crossbow_bolt" );
	pBolt->Spawn();

	return pBolt;
}

void CCrossbowBolt::Spawn( void )
{
	Precache( );
	pev->vuser1 = GetAbsOrigin(); // diffusion - remember the spawn position to calculate distance for the achievement
	pev->movetype = MOVETYPE_TOSS;//MOVETYPE_FLY; // diffusion - realistic bolt!!!
	pev->solid = SOLID_BBOX;

	pev->gravity = 0.35; // was 0.5 when movetype_fly

	SET_MODEL( edict(), "models/weapons/crossbow_bolt.mdl");

	UTIL_SetSize( this, Vector( -2, -2, -2 ), Vector( 2, 2, 2 ) );
	RelinkEntity( TRUE );

	SetFadeDistance(1500);

	SetTouch( &CCrossbowBolt::BoltTouch );
	SetThink(&CCrossbowBolt::BubbleThink );
	SetNextThink(0);
}

void CCrossbowBolt::Precache( )
{
	PRECACHE_MODEL ("models/weapons/crossbow_bolt.mdl");
	PRECACHE_SOUND("weapons/xbow_hitbod1.wav");
	PRECACHE_SOUND("weapons/xbow_hitbod2.wav");
	PRECACHE_SOUND("weapons/xbow_hitmetal.wav");
	PRECACHE_SOUND("weapons/xbow_fly1.wav");
	PRECACHE_SOUND("weapons/xbow_hit1.wav");
	PRECACHE_SOUND("fvox/beep.wav");
	m_iTrail = PRECACHE_MODEL("sprites/streak.spr");
}

int CCrossbowBolt :: Classify ( void )
{
	return CLASS_NONE;
}

void CCrossbowBolt :: OnChangeLevel( void )
{
	// NOTE: clear parent. We can't moving it properly for non-global entities
	SetParent( 0 );
}

void CCrossbowBolt :: TransferReset( void )
{
	// NOTE: I need to do it here because my parent already has
	// landmark offset but I'm not
	Vector origin = GetAbsOrigin();
	origin += gpGlobals->vecLandmarkOffset;
	SetAbsOrigin( origin );

	// changelevel issues
	if ( m_hParent == NULL )
	{
		TraceResult tr;

		UTIL_MakeVectors( GetAbsAngles() );
		Vector vecSrc = GetAbsOrigin() + gpGlobals->v_forward * -32;
		Vector vecDst = GetAbsOrigin() + gpGlobals->v_forward * 32;

		UTIL_TraceHull( vecSrc, vecDst, ignore_monsters, head_hull, edict(), &tr );

		if( tr.pHit && ENTINDEX( tr.pHit ) != 0 )
		{
			CBaseEntity *pNewParent = CBaseEntity::Instance( tr.pHit );

			if( pNewParent && pNewParent->IsBSPModel( ))
			{
				ALERT( at_aiconsole, "SetNewParent: %s\n", pNewParent->GetClassname());
				SetParent( pNewParent );
			}
		}
	}
}

//============================================================
// PlayTouchSound: choose an appropriate hit sound to play
//============================================================
void CCrossbowBolt::PlayTouchSound( CBaseEntity *pOther )
{
	if( pOther->HasSpawnFlags(SF_MONSTER_NODAMAGE) )
		return;
	
	// FIXME this is not looking good
	if( !pOther->pev->takedamage || FClassnameIs(pOther->pev, "monster_alice") || FClassnameIs(pOther->pev, "_playerdrone") || FClassnameIs( pOther->pev, "_playersentry" ) )
		return;

	if ( pOther->HasFlag(F_NOT_A_MONSTER) || pOther->HasFlag(F_METAL_MONSTER) )
	{
		EMIT_SOUND(ENT(pev), CHAN_BODY, "weapons/xbow_hitmetal.wav", 1, ATTN_NORM);
		return;
	}

	if( UTIL_GetModelType(pOther->pev->modelindex) == mod_brush )
	{
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "weapons/xbow_hitmetal.wav", 1, ATTN_NORM );
		return;
	}

	switch( RANDOM_LONG(0,1) )
	{
	case 0: EMIT_SOUND(ENT(pev), CHAN_BODY, "weapons/xbow_hitbod1.wav", 1, ATTN_NORM); break;
	case 1: EMIT_SOUND(ENT(pev), CHAN_BODY, "weapons/xbow_hitbod2.wav", 1, ATTN_NORM); break;
	}
}

void CCrossbowBolt::BoltTouch( CBaseEntity *pOther )
{
	SetAbsAngles( UTIL_VecToAngles( GetAbsVelocity() ));
	
	SetTouch( NULL );
	SetThink( NULL );

	if( pOther->pev->takedamage )
	{
		TraceResult tr = UTIL_GetGlobalTrace( );
		entvars_t *pevOwner;

		pevOwner = VARS( pev->owner );

		// UNDONE: this needs to call TraceAttack instead
		ClearMultiDamage();

		if ( pOther->IsPlayer() )
			pOther->TraceAttack(pevOwner, DMG_WPN_CROSSBOW, GetAbsVelocity().Normalize(), &tr, DMG_NEVERGIB ); 
		else
			pOther->TraceAttack(pevOwner, DMG_WPN_CROSSBOW, GetAbsVelocity().Normalize(), &tr, DMG_BULLET | DMG_NEVERGIB );

		ApplyMultiDamage( pev, pevOwner );

		// play body "thwack" sound
		// diffusion - moved here
		PlayTouchSound( pOther );

		if( pOther->IsRigidBody( ))
		{
			Vector vecDir = GetAbsVelocity().Normalize( );
			UTIL_SetOrigin( this, GetAbsOrigin() - vecDir * 12 );
			SetLocalAngles( UTIL_VecToAngles( vecDir ));
			pev->solid = SOLID_NOT;
			pev->movetype = MOVETYPE_NONE;
			SetLocalVelocity( g_vecZero );
			SetLocalAvelocity( g_vecZero );
			Vector angles = GetLocalAngles();
			angles.z = RANDOM_LONG( 0, 360 );
			SetLocalAngles( angles );
			SetThink( &CBaseEntity::SUB_Remove );
			SetNextThink( 240.0 );

			// g-cont. Setup movewith feature
			SetParent( pOther );
		}
		else
		{
			SetLocalVelocity( g_vecZero );
			Killed( pev, GIB_NEVER );
		}
	}
	else
	{	
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "weapons/xbow_hit1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 98 + RANDOM_LONG(0,7));
		// diffusion - make monsters be aware of it, add some randomization
		int SoundBits = bits_SOUND_COMBAT;
		if( RANDOM_LONG( 0, 5 ) > 3 )
			SoundBits = bits_SOUND_DANGER;
		CSoundEnt::InsertSound( SoundBits, GetAbsOrigin(), 150, 0.15 );
		pev->nextthink = gpGlobals->time;// this will get changed below if the bolt is allowed to stick in what it hit.
		SetThink( &CBaseEntity::SUB_Remove );

		if( UTIL_GetModelType( pOther->pev->modelindex ) == mod_brush || pOther->pev->solid == SOLID_CUSTOM )
		{
			Vector vecDir = GetAbsVelocity().Normalize( );
			UTIL_SetOrigin( this, GetAbsOrigin() - vecDir * 12 );
			SetLocalAngles( UTIL_VecToAngles( vecDir ));
			pev->solid = SOLID_NOT;
			pev->movetype = MOVETYPE_NONE;
			SetLocalVelocity( g_vecZero );
			SetLocalAvelocity( g_vecZero );
			Vector angles = GetLocalAngles();
			angles.z = RANDOM_LONG( 0, 360 );
			SetLocalAngles( angles );
			SetNextThink( 240.0 );

			// g-cont. Setup movewith feature
			SetParent( pOther );
		}

		if( UTIL_PointContents( GetAbsOrigin() ) != CONTENTS_WATER )
			UTIL_Sparks( GetAbsOrigin() );
	}
}

void CCrossbowBolt :: OnTeleport( void )
{
	// re-aiming arrow
	SetLocalAngles( UTIL_VecToAngles( GetLocalVelocity( )));
}

void CCrossbowBolt::BubbleThink( void )
{
	SetNextThink(0.1);

	SetAbsAngles( UTIL_VecToAngles( GetAbsVelocity() ) );

	if( pev->waterlevel == 0 )
		return;

	UTIL_BubbleTrail( GetAbsOrigin() - GetAbsVelocity() * 0.1, GetAbsOrigin(), 1 );
}

void CCrossbowBolt::ExplodeThink( void )
{
	int iContents = UTIL_PointContents( GetAbsOrigin() );
	int iScale;
	
	pev->dmg = 40;
	iScale = 10;

	Vector vecOrigin = GetAbsOrigin();

	MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, vecOrigin );
		WRITE_BYTE( TE_EXPLOSION);		
		WRITE_COORD( vecOrigin.x );
		WRITE_COORD( vecOrigin.y );
		WRITE_COORD( vecOrigin.z );
		if (iContents != CONTENTS_WATER)
			WRITE_SHORT( g_sModelIndexFireball );
		else
			WRITE_SHORT( g_sModelIndexWExplosion );
		WRITE_BYTE( iScale  ); // scale * 10
		WRITE_BYTE( 15  ); // framerate
		WRITE_BYTE( TE_EXPLFLAG_NONE );
	MESSAGE_END();

	entvars_t *pevOwner;

	if ( pev->owner )
		pevOwner = VARS( pev->owner );
	else
		pevOwner = NULL;

	pev->owner = NULL; // can't traceline attack owner if this is set

	::RadiusDamage( GetAbsOrigin(), pev, pevOwner, pev->dmg, 128, CLASS_NONE, DMG_BLAST|DMG_ALWAYSGIB );

	UTIL_Remove(this);
}

class CCrossbow : public CBasePlayerWeapon
{
	DECLARE_CLASS( CCrossbow, CBasePlayerWeapon );
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( ) { return WPN_SLOT_CROSSBOW + 1; }
	int GetItemInfo(ItemInfo *p);

	void FireBolt( void );
//	void FireSniperBolt( void );
	void PrimaryAttack( void );
	void SecondaryAttack( void );
	int AddToPlayer( CBasePlayer *pPlayer );

	DECLARE_DATADESC();

	BOOL Deploy( void );
	void Holster( void );
	void Reload( void );
	void WeaponIdle( void );
	void ResetZoom(void);

	int m_fInZoom;
	int m_fZoomInUse;

	bool bNeedPump;
};

LINK_ENTITY_TO_CLASS( weapon_crossbow, CCrossbow );

BEGIN_DATADESC( CCrossbow )
	DEFINE_FIELD( m_fInZoom, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fZoomInUse, FIELD_BOOLEAN ),
END_DATADESC()

void CCrossbow::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_CROSSBOW;
	SET_MODEL(ENT(pev), "models/weapons/w_crossbow.mdl");

	m_iDefaultAmmo = CROSSBOW_DEFAULT_GIVE;

	FallInit();// get ready to fall down.

	bNeedPump = false;
}

int CCrossbow::AddToPlayer( CBasePlayer *pPlayer )
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

void CCrossbow::Precache( void )
{
	PRECACHE_MODEL( "models/weapons/w_crossbow.mdl" );
	PRECACHE_MODEL( "models/weapons/v_crossbow.mdl" );
	PRECACHE_MODEL( "models/weapons/p_crossbow.mdl" );

	PRECACHE_SOUND( "weapons/xbow_shoot.wav" );
	PRECACHE_SOUND( "weapons/xbow_clipin.wav" );
	PRECACHE_SOUND( "weapons/xbow_clipout.wav" );
	PRECACHE_SOUND( "weapons/xbow_drawback.wav" );
	PRECACHE_SOUND( "weapons/xbow_drawback_long.wav" );
	PRECACHE_SOUND( "weapons/xbow_cloth.wav" );
	PRECACHE_SOUND( "weapons/xbow_scope.wav" );

	UTIL_PrecacheOther( "crossbow_bolt" );
}

int CCrossbow::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "bolts";
	p->iMaxAmmo1 = BOLT_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = CROSSBOW_MAX_CLIP;
	p->iSlot = WPN_SLOT_CROSSBOW;
	p->iPosition = WPN_POS_CROSSBOW;
	p->iId = WEAPON_CROSSBOW;
	p->iFlags = 0;
	p->iWeight = CROSSBOW_WEIGHT;
	return 1;
}

BOOL CCrossbow::Deploy( )
{
	m_flTimeWeaponIdle = gpGlobals->time + 5.0;
	m_flNextPrimaryAttack = gpGlobals->time + CROSSBOW_DEPLOY_TIME;
	if( m_iClip > 0 )
	{
		if( bNeedPump )
		{
			m_flTimeWeaponIdle = gpGlobals->time + CROSSBOW_DEPLOY_TIME;
			return DefaultDeploy( "models/weapons/v_crossbow.mdl", "models/weapons/p_crossbow.mdl", CROSSBOW_DRAW2, "bow" );
		}
		else
			return DefaultDeploy( "models/weapons/v_crossbow.mdl", "models/weapons/p_crossbow.mdl", CROSSBOW_DRAW1, "bow" );
	}

	return DefaultDeploy( "models/weapons/v_crossbow.mdl", "models/weapons/p_crossbow.mdl", CROSSBOW_DRAW2, "bow" );
}

void CCrossbow::Holster( void )
{
	// cancel any zooming in progress
	ResetZoom();

	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;

	if (m_iClip)
		SendWeaponAnim( CROSSBOW_HOLSTER1 );
	else
		SendWeaponAnim( CROSSBOW_HOLSTER2 );

	BaseClass::Holster();
}

void CCrossbow::ResetZoom(void)
{
	if( m_pPlayer->ZoomState > 0 )
		UTIL_ScreenFade( m_pPlayer, g_vecZero, 0.5, 0, 255, 0x0000 );
	
	m_fZoomInUse = 0;
	m_fInZoom = 0;
	m_pPlayer->ZoomState = 0;
	m_pPlayer->m_flFOV = 0;

	MESSAGE_BEGIN(MSG_ONE, gmsgZoom, NULL, m_pPlayer->pev);
		WRITE_BYTE(m_pPlayer->ZoomState);
		WRITE_BYTE( WEAPON_CROSSBOW );
	MESSAGE_END();
}

void CCrossbow::PrimaryAttack( void )
{
/*	if (m_fInZoom && g_pGameRules->IsMultiplayer())
	{
		FireSniperBolt();
		return;
	}
*/
	CLIENT_COMMAND( m_pPlayer->edict(), "-attack\n" );

	if( bNeedPump )
		return;

	if( m_iClip <= 3)
		LowAmmoMsg(m_pPlayer);

	FireBolt();
}

// this function only gets called in multiplayer
// diffusion - not used
/*
void CCrossbow::FireSniperBolt()
{
	m_flNextPrimaryAttack = gpGlobals->time + 0.75;

	if (m_iClip == 0)
	{
		PlayEmptySound( );
		return;
	}

	TraceResult tr;

	m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
	m_iClip--;

	// make twang sound
	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/xbow_fire1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 93 + RANDOM_LONG(0,0xF));

	if (m_iClip)
	{
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/xbow_reload1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 93 + RANDOM_LONG(0,0xF));
		SendWeaponAnim( CROSSBOW_FIRE1 );
	}
	else if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] == 0)
		SendWeaponAnim( CROSSBOW_FIRE3 );

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	
	Vector anglesAim = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;
	UTIL_MakeVectors( anglesAim );
	Vector vecSrc = m_pPlayer->GetGunPosition( ) - gpGlobals->v_up * 2;
	Vector vecDir = gpGlobals->v_forward;

	UTIL_TraceLine(vecSrc, vecSrc + vecDir * 8192, dont_ignore_monsters, m_pPlayer->edict(), &tr);

	if ( tr.pHit->v.takedamage )
	{
		switch( RANDOM_LONG(0,1) )
		{
		case 0: EMIT_SOUND( tr.pHit, CHAN_BODY, "weapons/xbow_hitbod1.wav", 1, ATTN_NORM); break;
		case 1: EMIT_SOUND( tr.pHit, CHAN_BODY, "weapons/xbow_hitbod2.wav", 1, ATTN_NORM); break;
		}

		ClearMultiDamage( );
		CBaseEntity::Instance(tr.pHit)->TraceAttack(m_pPlayer->pev, 120, vecDir, &tr, DMG_BULLET | DMG_NEVERGIB ); 
		ApplyMultiDamage( pev, m_pPlayer->pev );
	}
	else
	{
		// create a bolt
		CCrossbowBolt *pBolt = CCrossbowBolt::BoltCreate();
		pBolt->SetAbsOrigin( tr.vecEndPos - vecDir * 10 );
		pBolt->SetAbsAngles( UTIL_VecToAngles( vecDir ));
		pBolt->pev->solid = SOLID_NOT;
		pBolt->SetTouch( NULL );
		pBolt->SetThink( &CBaseEntity::SUB_Remove );

		EMIT_SOUND( pBolt->edict(), CHAN_WEAPON, "weapons/xbow_hit1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM );

		if( UTIL_PointContents( tr.vecEndPos ) != CONTENTS_WATER )
			UTIL_Sparks( tr.vecEndPos );

		if( FClassnameIs( tr.pHit, "worldspawn" ))
			pBolt->SetNextThink(5.0); // let the bolt sit around for a while if it hit static architecture
		else
			pBolt->SetNextThink(0);
	}
}*/

void CCrossbow::FireBolt( void )
{
	TraceResult tr;

	if (m_iClip <= 0)
	{
		PlayEmptySound( );
		return;
	}

	m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;

	m_iClip--;

	// make twang sound
//	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/xbow_fire1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 93 + RANDOM_LONG(0,0xF));
	PlayClientSound( m_pPlayer, WEAPON_CROSSBOW );

	SendWeaponAnim( CROSSBOW_FIRE1 );

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	Vector anglesAim = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;
	UTIL_MakeVectors( anglesAim );

	// Vector vecSrc = GetAbsOrigin() + gpGlobals->v_up * 16 + gpGlobals->v_forward * 20 + gpGlobals->v_right * 4;
	Vector vecSrc	= m_pPlayer->GetGunPosition( ) - gpGlobals->v_up * 2;
	Vector vecDir	= gpGlobals->v_forward;

	CBaseEntity *pBolt = CBaseEntity::Create( "crossbow_bolt", vecSrc, anglesAim, m_pPlayer->edict() );
//	CCrossbowBolt *pBolt = CCrossbowBolt::BoltCreate();
//	pBolt->SetAbsOrigin( vecSrc );
//	pBolt->SetAbsAngles( anglesAim );
//	pBolt->pev->owner = m_pPlayer->edict();

	Vector vecGround;
	float flGroundSpeed;

	if( m_pPlayer->GetGroundEntity( ))
		vecGround = m_pPlayer->GetGroundEntity( )->GetAbsVelocity();
	else vecGround = g_vecZero;

	flGroundSpeed = vecGround.Length();

	// diffusion - changed local vel to abs
	if (m_pPlayer->pev->waterlevel == 3)
	{
		pBolt->SetAbsVelocity( vecDir * BOLT_WATER_VELOCITY + vecGround );
		pBolt->pev->speed = BOLT_WATER_VELOCITY + flGroundSpeed;
	}
	else
	{
		pBolt->SetAbsVelocity( vecDir * BOLT_AIR_VELOCITY + vecGround );
		pBolt->pev->speed = BOLT_AIR_VELOCITY + flGroundSpeed;
	}

	Vector avelocity( 0, 0, 10 );

	pBolt->SetLocalAvelocity( avelocity );

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flTimeWeaponIdle = gpGlobals->time + CROSSBOW_NEXT_PA_TIME;
	bNeedPump = true;

	if( !m_iClip )
		m_flNextPrimaryAttack = gpGlobals->time + CROSSBOW_NEXT_PA_TIME; // this will delay reloading to let shooting animation play

	if( m_pPlayer->ZoomState > 0 )
		m_pPlayer->pev->punchangle.x -= 2;
	else
		m_pPlayer->pev->punchangle.x -= 5;
}

void CCrossbow::SecondaryAttack( void )
{
	CLIENT_COMMAND(m_pPlayer->edict(), "-attack2\n");
	
	if( !IS_DEDICATED_SERVER() )
	{
		if( CVAR_GET_FLOAT( "default_fov" ) <= CROSSBOW_MAX_ZOOM )
		{
			ALERT( at_error, "weapon_crossbow can't zoom, default_fov is too low!\n" );
			return;
		}
	}
	
	// do not switch zoom when player stay button is pressed
	if (m_fZoomInUse)
		return;

	m_fZoomInUse = 1;

	if( m_fInZoom )
	{
		EMIT_SOUND( ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/xbow_scope.wav", 1, 1.5 );
		m_fInZoom = 0;
		m_pPlayer->ZoomState = 2; // zooming out
		MESSAGE_BEGIN(MSG_ONE, gmsgZoom, NULL, m_pPlayer->pev);
			WRITE_BYTE( m_pPlayer->ZoomState );
			WRITE_BYTE( WEAPON_CROSSBOW );
		MESSAGE_END();
		m_pPlayer->m_flFOV = 0;
		m_pPlayer->ZoomState = 0; // zoomed out. reset the state
		UTIL_ScreenFade( m_pPlayer, g_vecZero, 0.5, 0, 255, 0x0000 );
	}
	else
	{
		UTIL_ScreenFade( m_pPlayer, g_vecZero, 0.25, 0, 255, 0x0000 );
		EMIT_SOUND( ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/xbow_scope.wav", 1, 1.5 );
		m_fInZoom = 1;
		m_pPlayer->ZoomState = 1; // zooming in
		MESSAGE_BEGIN(MSG_ONE, gmsgZoom, NULL, m_pPlayer->pev);
			WRITE_BYTE( m_pPlayer->ZoomState );
			WRITE_BYTE( WEAPON_CROSSBOW );
		MESSAGE_END();
		m_pPlayer->m_flFOV = CROSSBOW_MAX_ZOOM;
	}

	m_flNextSecondaryAttack = gpGlobals->time + 0.2;
}

void CCrossbow::Reload( void )
{
	if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 || m_iClip == CROSSBOW_MAX_CLIP )
		return;
	
	CLIENT_COMMAND(m_pPlayer->edict(), "-reload\n");

	bNeedPump = false;
	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT( 10, 15 );
	
	if( m_fInZoom )
		ResetZoom();

	DefaultReload( CROSSBOW_MAX_CLIP, CROSSBOW_RELOAD, CROSSBOW_RELOAD_TIME );
}

void CCrossbow::WeaponIdle( void )
{
	m_pPlayer->GetAutoaimVector( AUTOAIM_2DEGREES );  // get the autoaim vector but ignore it;  used for autoaim crosshair in DM

	ResetEmptySound( );
	m_fZoomInUse = 0;
	
	if( m_flTimeWeaponIdle < gpGlobals->time )
	{
		if( bNeedPump )
		{
			SendWeaponAnim( CROSSBOW_FIRE2 );
			bNeedPump = false;
			m_flNextPrimaryAttack = gpGlobals->time + CROSSBOW_PUMP_TIME;
			m_flTimeWeaponIdle = gpGlobals->time + 8.0f;
			return;
		}
		
		if( RANDOM_FLOAT( 0.0f, 1.0f ) <= 0.75f )
		{
			if (m_iClip)
				SendWeaponAnim( CROSSBOW_IDLE1 );
			else
				SendWeaponAnim( CROSSBOW_IDLE2 );

			m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );
		}
		else
		{
			if (m_iClip)
			{
				SendWeaponAnim( CROSSBOW_FIDGET1 );
				m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT( 7, 15 );
			}
			else // no empty fidget anim...
			{
			//	SendWeaponAnim( CROSSBOW_FIDGET2 );
			//	m_flTimeWeaponIdle = gpGlobals->time + 80.0 / 30.0;
			}
		}
	}
}




class CCrossbowAmmo : public CBasePlayerAmmo
{
	DECLARE_CLASS( CCrossbowAmmo, CBasePlayerAmmo );

	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/weapons/w_crossbow_clip.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/weapons/w_crossbow_clip.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		if (pOther->GiveAmmo( AMMO_CROSSBOWCLIP_GIVE, "bolts", BOLT_MAX_CARRY ) != -1)
		{
		//	EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			PlayPickupSound( pOther );
			return TRUE;
		}
		return FALSE;
	}
};

LINK_ENTITY_TO_CLASS( ammo_crossbow, CCrossbowAmmo );