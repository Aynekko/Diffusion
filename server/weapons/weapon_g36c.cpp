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
#include "player.h"

class CWeaponG36C : public CBasePlayerWeapon
{
	DECLARE_CLASS( CWeaponG36C, CBasePlayerWeapon );
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return WPN_SLOT_G36C + 1; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	BOOL Deploy( void );
	void Holster( void );
	void Reload( void );
	void WeaponIdle( void );

	void ResetZoom( void );
	void SetWeaponZoom( int iZoom );
	int m_fInZoom;
	int m_fZoomInUse;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( weapon_g36c, CWeaponG36C );

BEGIN_DATADESC( CWeaponG36C )
	DEFINE_FIELD( m_fInZoom, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fZoomInUse, FIELD_BOOLEAN ),
END_DATADESC()

void CWeaponG36C::Spawn( void )
{
	Precache( );
	SET_MODEL(ENT(pev), "models/weapons/w_g36c.mdl");
	m_iId = WEAPON_G36C;

	m_iDefaultAmmo = MP5_DEFAULT_GIVE; // two rounds
	m_iDefaultAmmo2 = 0;
	m_iSavedZoomState = 1;

	FallInit();// get ready to fall down.
}

void CWeaponG36C::Precache( void )
{
	PRECACHE_MODEL("models/weapons/v_g36c.mdl");
	PRECACHE_MODEL("models/weapons/w_g36c.mdl");
	PRECACHE_MODEL("models/weapons/p_g36c.mdl");

	PRECACHE_MODEL("models/weapons/w_g36c_clip.mdl");
	PRECACHE_SOUND("items/9mmclip1.wav");              

	PRECACHE_SOUND( "weapons/g36c_magout.wav" );
	PRECACHE_SOUND( "weapons/g36c_magin.wav" );
	PRECACHE_SOUND( "weapons/g36c_magtap.wav" );
	PRECACHE_SOUND( "weapons/g36c_deploy.wav" );
	PRECACHE_SOUND( "weapons/g36c_safety.wav" );
	PRECACHE_SOUND( "weapons/g36c_boltpull.wav" );

	PRECACHE_SOUND ("weapons/g36c_shoot.wav");

	PRECACHE_SOUND ("weapons/357_cock1.wav");
}

int CWeaponG36C::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = g_WpnAmmo[WEAPON_G36C];
	p->iMaxAmmo1 = G36C_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = G36C_MAX_CLIP;
	p->iSlot = WPN_SLOT_G36C;
	p->iPosition = WPN_POS_G36C;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_G36C;
	p->iWeight = MP5_WEIGHT;

	return 1;
}

int CWeaponG36C::AddToPlayer( CBasePlayer *pPlayer )
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

BOOL CWeaponG36C::Deploy( )
{
	m_flTimeWeaponIdle = gpGlobals->time + 5.0;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + G36C_DEPLOY_TIME;
	return DefaultDeploy( "models/weapons/v_g36c.mdl", "models/weapons/p_g36c.mdl", G36C_DRAW, "mp5" );
}

void CWeaponG36C::Holster( void )
{
	// cancel any zooming in progress
	ResetZoom();

	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;

	BaseClass::Holster();
}

void CWeaponG36C::ResetZoom( void )
{
	if( m_pPlayer->ZoomState > 0 )
		UTIL_ScreenFade( m_pPlayer, g_vecZero, 0.5, 0, 255, 0x0000 );

	m_fZoomInUse = 0;
	m_fInZoom = 0;
	m_pPlayer->ZoomState = 0;
	if( m_pPlayer->pCar == NULL || m_pPlayer->CameraEntity[0] != '\0' )
		m_pPlayer->m_flFOV = 0;

	MESSAGE_BEGIN( MSG_ONE, gmsgZoom, NULL, m_pPlayer->pev );
		WRITE_BYTE( m_pPlayer->ZoomState );
		WRITE_BYTE( WEAPON_G36C );
	MESSAGE_END();
}

void CWeaponG36C::PrimaryAttack()
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
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

	m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;

	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	SendWeaponAnim( G36C_SHOOT );

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

//	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/g36c_shoot.wav", 1, ATTN_NORM, 0, 95 + RANDOM_LONG(0,10));
	PlayClientSound( m_pPlayer, WEAPON_G36C, 0, (m_iClip <= 12 ? m_iClip : 0) );

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	
	Vector	vecShellVelocity = m_pPlayer->GetAbsVelocity() 
							 + gpGlobals->v_right * RANDOM_FLOAT(50,70) 
							 + gpGlobals->v_up * RANDOM_FLOAT(100,150) 
							 + gpGlobals->v_forward * 25;

	EjectBrass ( m_pPlayer->EyePosition()
					+ gpGlobals->v_up * -12 
					+ gpGlobals->v_forward * 20 
					+ gpGlobals->v_right * 6, vecShellVelocity,
					m_pPlayer->GetAbsAngles().y, SHELL_556, TE_BOUNCE_SHELL); 
	
	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	// diffusion - a bit of realism: precision changes at different walking speeds, + small shake :)
	MakeWeaponShake( m_pPlayer, WEAPON_G36C, 0 );
	
	float Cone = m_pPlayer->pev->velocity.Length() * 0.0002;
	
	Cone = bound( 0.01, Cone, 0.06 );

	if( Cone < 0.02 )
	{
		if( m_pPlayer->pev->flags & FL_DUCKING )
			Cone = 0.01;
		else
			Cone = 0.02;
	}

	if( m_fInZoom )
		Cone *= 0.75;

	m_pPlayer->FireBullets( 1, vecSrc, vecAiming, Vector(Cone,Cone,Cone), 8192, BULLET_PLAYER_MP5, 0, DMG_WPN_G36C );
	m_iClip--;

	if( m_iClip <= 10 )
		LowAmmoMsg( m_pPlayer );

	m_pPlayer->SendAchievementStatToClient( ACH_BULLETSFIRED, 1, ACHVAL_ADD );

	m_pPlayer->pev->punchangle.x += Cone * RANDOM_LONG(15,25);
	m_pPlayer->pev->punchangle.y += -Cone * RANDOM_LONG(-15,25);

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = gpGlobals->time + G36C_NEXT_PA_TIME;
	if (m_flNextPrimaryAttack < gpGlobals->time)
		m_flNextPrimaryAttack = gpGlobals->time + G36C_NEXT_PA_TIME;

	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );
}

void CWeaponG36C::SetWeaponZoom( int iZoom )
{
	if( iZoom == 0 ) // unzoom
	{
		ResetZoom();
	}
	else // iZoom == 1
	{
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/xbow_scope.wav", 1, 1.5 );
		m_fInZoom = 1;
		m_pPlayer->ZoomState = 1; // zooming in
		MESSAGE_BEGIN( MSG_ONE, gmsgZoom, NULL, m_pPlayer->pev );
		WRITE_BYTE( m_pPlayer->ZoomState );
		WRITE_BYTE( WEAPON_G36C );
		MESSAGE_END();
		m_pPlayer->m_flFOV = G36C_MAX_ZOOM;
	}
}

void CWeaponG36C::SecondaryAttack( void )
{
	CLIENT_COMMAND( m_pPlayer->edict(), "-attack2\n" );

	if( !IS_DEDICATED_SERVER() )
	{
		if( CVAR_GET_FLOAT( "default_fov" ) <= G36C_MAX_ZOOM )
		{
			ALERT( at_error, "weapon_g36c can't zoom, default_fov is too low!\n" );
			return;
		}
	}

	// do not switch zoom when player stay button is pressed
	if( m_fZoomInUse )
		return;

	m_fZoomInUse = 1;

	if( m_fInZoom )
	{
		ResetZoom();
	}
	else
	{
		SetWeaponZoom( 1 );
		UTIL_ScreenFade( m_pPlayer, g_vecZero, 0.25, 0, 255, 0x0000 );
	}

	m_flNextSecondaryAttack = gpGlobals->time + 0.2;
	m_flTimeWeaponIdle = gpGlobals->time + 5.0;
}

void CWeaponG36C::Reload( void )
{
	CLIENT_COMMAND(m_pPlayer->edict(), "-reload\n");

	if( m_fInZoom )
		ResetZoom();

	if( !m_iClip )
		DefaultReload( 30, G36C_RELOAD_EMPTY, G36C_RELOADEMPTY_TIME );
	else
		DefaultReload( 30, G36C_RELOAD, G36C_RELOAD_TIME );
}

void CWeaponG36C::WeaponIdle( void )
{
	ResetEmptySound( );

	m_fZoomInUse = 0;

	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
}

class CG36CAmmoClip : public CBasePlayerAmmo
{
	DECLARE_CLASS( CG36CAmmoClip, CBasePlayerAmmo );

	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/weapons/w_g36c_clip.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/weapons/w_g36c_clip.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		int bResult = (pOther->GiveAmmo( AMMO_G36C_GIVE, g_WpnAmmo[WEAPON_G36C], G36C_MAX_CARRY ) != -1);

		if (bResult)
			PlayPickupSound( pOther );

		return bResult;
	}
};

LINK_ENTITY_TO_CLASS( ammo_g36c, CG36CAmmoClip );
