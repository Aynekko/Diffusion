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
#include "entities/soundent.h"
#include "game/gamerules.h"

class CMRC : public CBasePlayerWeapon
{
	DECLARE_CLASS( CMRC, CBasePlayerWeapon );
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return WPN_SLOT_MRC + 1; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	int SecondaryAmmoIndex( void );
	BOOL Deploy( void );
	void Reload( void );
	void WeaponIdle( void );
};

LINK_ENTITY_TO_CLASS( weapon_mrc, CMRC );
LINK_ENTITY_TO_CLASS( weapon_9mmAR, CMRC );


//=========================================================
//=========================================================
int CMRC::SecondaryAmmoIndex( void )
{
	return m_iSecondaryAmmoType;
}

void CMRC::Spawn( void )
{
	pev->classname = MAKE_STRING("weapon_9mmAR"); // hack to allow for old names
	Precache( );
	SET_MODEL(ENT(pev), "models/weapons/w_mrc.mdl");
	m_iId = WEAPON_MRC;

	m_iDefaultAmmo = MRC_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}


void CMRC::Precache( void )
{
	PRECACHE_MODEL("models/weapons/v_mrc.mdl");
	PRECACHE_MODEL("models/weapons/w_mrc.mdl");
	PRECACHE_MODEL("models/weapons/p_mrc.mdl");

	PRECACHE_MODEL("models/weapons/grenade.mdl");	// grenade

	PRECACHE_MODEL("models/weapons/w_9mmARclip.mdl");
	PRECACHE_SOUND("items/9mmclip1.wav");              

	PRECACHE_SOUND("items/clipinsert1.wav");
	PRECACHE_SOUND("items/cliprelease1.wav");

	PRECACHE_SOUND ("weapons/hks1.wav");// H to the K
	PRECACHE_SOUND ("weapons/hks2.wav");
	PRECACHE_SOUND ("weapons/hks3.wav");
	PRECACHE_SOUND( "weapons/hks1_d.wav" );
	PRECACHE_SOUND( "weapons/hks2_d.wav" );
	PRECACHE_SOUND( "weapons/hks3_d.wav" );

	PRECACHE_SOUND( "weapons/glauncher.wav" );

	PRECACHE_SOUND ("weapons/357_cock1.wav");
}

int CMRC::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = g_WpnAmmo[WEAPON_MRC];
	p->iMaxAmmo1 = _9MM_MAX_CARRY;
	p->pszAmmo2 = "ARgrenades";
	p->iMaxAmmo2 = M203_GRENADE_MAX_CARRY;
	p->iMaxClip = MRC_MAX_CLIP;
	p->iSlot = WPN_SLOT_MRC;
	p->iPosition = WPN_POS_MRC;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_MRC;
	p->iWeight = MP5_WEIGHT;

	return 1;
}

int CMRC::AddToPlayer( CBasePlayer *pPlayer )
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

BOOL CMRC::Deploy( )
{
	m_flTimeWeaponIdle = gpGlobals->time + 5.0;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + MRC_DEPLOY_TIME;
	return DefaultDeploy( "models/weapons/v_mrc.mdl", "models/weapons/p_mrc.mdl", MRC_DEPLOY, "mp5" );
}

void CMRC::PrimaryAttack()
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		CLIENT_COMMAND( m_pPlayer->edict(), "-attack\n" );
		PlayEmptySound( );
		m_flNextPrimaryAttack = gpGlobals->time + 0.15;
		return;
	}

	if (m_iClip <= 0)
	{
		CLIENT_COMMAND(m_pPlayer->edict(), "-attack\n");
		PlayEmptySound();
		m_flNextPrimaryAttack = gpGlobals->time + 0.15;
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	if( m_pPlayer->LoudWeaponsRestricted )
		m_pPlayer->FireLoudWeaponRestrictionEntity();

	SendWeaponAnim( MRC_FIRE1 + RANDOM_LONG(0,2));

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

//	switch( RANDOM_LONG(0,2) )
//	{
//	case 0:	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/hks1.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xf));	break;
//	case 1:	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/hks2.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xf));	break;
//	case 2: EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/hks3.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xf));	break;
//	}
	PlayClientSound( m_pPlayer, WEAPON_MRC, 0, (m_iClip <= 15 ? m_iClip : 0) );

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	
	Vector	vecShellVelocity = m_pPlayer->GetAbsVelocity() 
							 + gpGlobals->v_right * RANDOM_FLOAT(50,70) 
							 + gpGlobals->v_up * RANDOM_FLOAT(100,150) 
							 + gpGlobals->v_forward * 25;

	// MRC doesn't have gun shells - no EjectBrass
	
	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	// diffusion - a bit of realism: precision changes at different walking speeds, + small shake :)
	MakeWeaponShake( m_pPlayer, WEAPON_MRC, 0 );
	
	float Cone = m_pPlayer->pev->velocity.Length() * 0.000185;
	
	Cone = bound( 0.01, Cone, 0.07 );

	if( Cone < 0.02 )
	{
		if( m_pPlayer->pev->flags & FL_DUCKING )
			Cone = 0.01;
		else
			Cone = 0.02;
	}

	m_pPlayer->FireBullets( 1, vecSrc, vecAiming, Vector(Cone,Cone,Cone), 8192, BULLET_PLAYER_MP5, 0, DMG_WPN_MRC ); // 12 dmg
	m_iClip--;

	if( m_iClip <= 15 )
		LowAmmoMsg( m_pPlayer );

	m_pPlayer->SendAchievementStatToClient( ACH_BULLETSFIRED, 1, ACHVAL_ADD );

	m_pPlayer->pev->punchangle.x += Cone * RANDOM_LONG(15,25);
	m_pPlayer->pev->punchangle.y += -Cone * RANDOM_LONG(-10,20);

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = gpGlobals->time + MRC_NEXT_PA_TIME;
	if (m_flNextPrimaryAttack < gpGlobals->time)
		m_flNextPrimaryAttack = gpGlobals->time + MRC_NEXT_PA_TIME;

	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );

}

void CMRC::SecondaryAttack( void )
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		CLIENT_COMMAND( m_pPlayer->edict(), "-attack2\n" );
		PlayEmptySound( );
		m_flNextSecondaryAttack = gpGlobals->time + 0.15;
		return;
	}

	if (m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] <= 0)
	{
		PlayEmptySound( );
		CLIENT_COMMAND(m_pPlayer->edict(), "-attack2\n");
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	if( m_pPlayer->LoudWeaponsRestricted )
		m_pPlayer->FireLoudWeaponRestrictionEntity();

	m_pPlayer->m_iExtraSoundTypes = bits_SOUND_DANGER;
	m_pPlayer->m_flStopExtraSoundTime = gpGlobals->time + 0.2;
			
	m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType]--;

	SendWeaponAnim( MRC_LAUNCH );

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

//	if ( RANDOM_LONG(0,1) )
//	{
//		// play this sound through BODY channel so we can hear it if player didn't stop firing MP3
//		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/glauncher.wav", 0.8, ATTN_NORM);
//	}
//	else
//	{
//		// play this sound through BODY channel so we can hear it if player didn't stop firing MP3
//		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/glauncher2.wav", 0.8, ATTN_NORM);
//	}
	PlayClientSound( m_pPlayer, WEAPON_MRC, 1 );
 
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	MakeWeaponShake( m_pPlayer, WEAPON_MRC, 1 ); // diffusion - small shake when launching a grenade

	// we don't add in player velocity anymore.
	CGrenade::ShootContact( m_pPlayer->pev, m_pPlayer->EyePosition() + gpGlobals->v_forward * 16, gpGlobals->v_forward * 800 );
	
	m_flNextPrimaryAttack = gpGlobals->time + MRC_NEXT_SA_TIME;
	m_flNextSecondaryAttack = gpGlobals->time + MRC_NEXT_SA_TIME;
	m_flTimeWeaponIdle = gpGlobals->time + 5;// idle pretty soon after shooting.

	if (!m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType])
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_pPlayer->pev->punchangle.x -= 10;
}

void CMRC::Reload( void )
{
	CLIENT_COMMAND(m_pPlayer->edict(), "-reload\n");
	DefaultReload( MRC_MAX_CLIP, MRC_RELOAD, MRC_RELOAD_TIME );
}

void CMRC::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
	/*
	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	int iAnim;
	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:	
		iAnim = MRC_LONGIDLE;
		break;
	
	default:
	case 1:
		iAnim = MRC_IDLE1;
		break;
	}

	SendWeaponAnim( iAnim );

	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );// how long till we do this again.
	*/
}

class CMRCAmmoClip : public CBasePlayerAmmo
{
	DECLARE_CLASS( CMRCAmmoClip, CBasePlayerAmmo );

	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/weapons/w_9mmARclip.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/weapons/w_9mmARclip.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		int bResult = (pOther->GiveAmmo( AMMO_MRCCLIP_GIVE, g_WpnAmmo[WEAPON_MRC], _9MM_MAX_CARRY) != -1);
		if (bResult)
		{
		//	EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			PlayPickupSound( pOther );
		}
		return bResult;
	}
};

LINK_ENTITY_TO_CLASS( ammo_mp5clip, CMRCAmmoClip );
LINK_ENTITY_TO_CLASS( ammo_9mmAR, CMRCAmmoClip );


class CMRCChainammo : public CBasePlayerAmmo
{
	DECLARE_CLASS( CMRCChainammo, CBasePlayerAmmo );

	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/weapons/w_chainammo.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/weapons/w_chainammo.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		int bResult = (pOther->GiveAmmo( AMMO_CHAINBOX_GIVE, g_WpnAmmo[WEAPON_MRC], _9MM_MAX_CARRY) != -1);

		if (bResult)
		//	EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			PlayPickupSound( pOther );

		return bResult;
	}
};

LINK_ENTITY_TO_CLASS( ammo_9mmbox, CMRCChainammo );

class CMRCAmmoGrenade : public CBasePlayerAmmo
{
	DECLARE_CLASS( CMRCAmmoGrenade, CBasePlayerAmmo );

	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/weapons/w_ARgrenade.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/weapons/w_ARgrenade.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		int bResult = (pOther->GiveAmmo( AMMO_M203BOX_GIVE, "ARgrenades", M203_GRENADE_MAX_CARRY ) != -1);

		if (bResult)
		//	EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			PlayPickupSound( pOther );

		return bResult;
	}
};

LINK_ENTITY_TO_CLASS( ammo_mp5grenades, CMRCAmmoGrenade );
LINK_ENTITY_TO_CLASS( ammo_ARgrenades, CMRCAmmoGrenade );