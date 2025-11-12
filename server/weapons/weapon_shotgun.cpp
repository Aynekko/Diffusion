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

// special deathmatch shotgun spreads
#define VECTOR_CONE_DM_SHOTGUN	Vector( 0.08716f, 0.04362f, 0.00f ) // 10 degrees by 5 degrees
#define VECTOR_CONE_DM_DOUBLESHOTGUN	Vector( 0.17365f, 0.04362f, 0.00f ) // 20 degrees by 5 degrees

class CShotgun : public CBasePlayerWeapon
{
	DECLARE_CLASS( CShotgun, CBasePlayerWeapon );
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( ) { return WPN_SLOT_SHOTGUN + 1; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	DECLARE_DATADESC();

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	BOOL Deploy( );
	void Reload( void );
	void WeaponIdle( void );
	int m_fInReload;
	float m_flNextReload;
	int PostShell; // diffusion - this value will decide whether to eject 1 or 2 shells
	bool bUseAfterReloadEmpty;
	bool AskToStopReload;
};

LINK_ENTITY_TO_CLASS( weapon_shotgun, CShotgun );

BEGIN_DATADESC( CShotgun )
	DEFINE_FIELD( m_flNextReload, FIELD_TIME ),
	DEFINE_FIELD( m_fInSpecialReload, FIELD_INTEGER ),
	DEFINE_FIELD( m_flPumpTime, FIELD_TIME ),
	DEFINE_FIELD( PostShell, FIELD_INTEGER),
END_DATADESC()

void CShotgun::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_SHOTGUN;
	SET_MODEL(ENT(pev), "models/weapons/w_shotgun.mdl");

	m_iDefaultAmmo = SHOTGUN_DEFAULT_GIVE;
	m_iDefaultAmmo2 = 0;

	FallInit();// get ready to fall
}

void CShotgun::Precache( void )
{
	PRECACHE_MODEL("models/weapons/v_shotgun.mdl");
	PRECACHE_MODEL("models/weapons/w_shotgun.mdl");
	PRECACHE_MODEL("models/weapons/p_shotgun.mdl");

	PRECACHE_SOUND("items/9mmclip1.wav");              

	PRECACHE_SOUND ("weapons/shotgun_single.wav");
	PRECACHE_SOUND ("weapons/shotgun_single_d.wav");
	PRECACHE_SOUND( "weapons/shotgun_dbl.wav" );
	PRECACHE_SOUND( "weapons/shotgun_dbl_d.wav" );

	// shotgun reload
	PRECACHE_SOUND ("weapons/reload1.wav");
	PRECACHE_SOUND( "weapons/reload2.wav" );
	PRECACHE_SOUND ("weapons/reload3.wav");

//	PRECACHE_SOUND ("weapons/sshell1.wav");	// shotgun reload - played on client
//	PRECACHE_SOUND ("weapons/sshell3.wav");	// shotgun reload - played on client
	
	PRECACHE_SOUND ("weapons/357_cock1.wav"); // gun empty sound
	PRECACHE_SOUND( "weapons/shotgun_pump.wav" );
}

int CShotgun::AddToPlayer( CBasePlayer *pPlayer )
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

int CShotgun::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = g_WpnAmmo[WEAPON_SHOTGUN];
	p->iMaxAmmo1 = BUCKSHOT_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = SHOTGUN_MAX_CLIP;
	p->iSlot = WPN_SLOT_SHOTGUN;
	p->iPosition = WPN_POS_SHOTGUN;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_SHOTGUN;
	p->iWeight = SHOTGUN_WEIGHT;

	return 1;
}

BOOL CShotgun::Deploy( )
{
	m_fInReload = 0; // reset any reloading
	AskToStopReload = false;
	if( m_flPumpTime && m_flPumpTime < gpGlobals->time )
	{
		m_flTimeWeaponIdle = m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + SHOTGUN_RELOADEMPTY_FINISH_TIME;
		if( m_iClip <= 0 )
			m_flNextReload = gpGlobals->time + 1.5;
		return DefaultDeploy( "models/weapons/v_shotgun.mdl", "models/weapons/p_shotgun.mdl", SHOTGUN_END_RELOAD_EMPTY, "shotgun" );
	}
	else
	{
		if( bUseAfterReloadEmpty && (m_iClip > 0) )
		{
			m_flTimeWeaponIdle = m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + SHOTGUN_RELOADEMPTY_FINISH_TIME;
			m_flPumpTime = -1; // need to do shell-less pump after reloading
			return DefaultDeploy( "models/weapons/v_shotgun.mdl", "models/weapons/p_shotgun.mdl", SHOTGUN_END_RELOAD_EMPTY, "shotgun" );
		}
		else
		{
			m_flPumpTime = 0;
			m_flTimeWeaponIdle = gpGlobals->time + 2.0;
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + SHOTGUN_DEPLOY_TIME;
			return DefaultDeploy( "models/weapons/v_shotgun.mdl", "models/weapons/p_shotgun.mdl", SHOTGUN_DRAW, "shotgun" );
		}
	}
}

void CShotgun::PrimaryAttack()
{
	// diffusion - Hack. Otherwise it will mess up the ejecting shell and pump sound.
	//             They won't work if you hold down the attack button. Same is done for attack2.
	CLIENT_COMMAND(m_pPlayer->edict(), "-attack\n");

	// stop reloading, move the gun
	if( m_fInReload > 0 )
	{
		// don't stop reload if didn't load anything yet
		if( m_iClip <= 0 )
		{
			CLIENT_COMMAND( m_pPlayer->edict(), "-attack\n" );
			return;
		}

		if( bUseAfterReloadEmpty )
		{
			SendWeaponAnim( SHOTGUN_END_RELOAD_EMPTY );
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + SHOTGUN_RELOADEMPTY_FINISH_TIME;
			m_flPumpTime = gpGlobals->time + 0.6;
		}
		else
		{
			SendWeaponAnim( SHOTGUN_END_RELOAD );
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + SHOTGUN_RELOAD_FINISH_TIME;
		}
		
		m_fInReload = 0;		
		m_flTimeWeaponIdle = gpGlobals->time + 1.5;
		return;
	}

	if (m_iClip <= 0)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = gpGlobals->time + 0.15;
		return;
	}

	AskToStopReload = false;
	
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = gpGlobals->time + 0.15;
		return;
	}

	if( m_iClip <= 4)
		LowAmmoMsg(m_pPlayer);

	if( m_iClip > 0 )
		m_flTimeWeaponIdle = gpGlobals->time + 5.0;
	else
		m_flTimeWeaponIdle = 0.75;

	if( m_iClip > 0 ) // just in case
	{
		m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

		m_iClip--;
		m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

		SendWeaponAnim( SHOTGUN_FIRE );

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	//	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/sbarrel1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 93 + RANDOM_LONG(0,0x1f));
		PlayClientSound( m_pPlayer, WEAPON_SHOTGUN );
	
		Vector vecSrc	 = m_pPlayer->GetGunPosition( );
		Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

		m_pPlayer->FireBullets( 6, vecSrc, vecAiming, VECTOR_CONE_10DEGREES, 8192, BULLET_PLAYER_BUCKSHOT, 0, DMG_WPN_SHOTGUN );

		m_pPlayer->SendAchievementStatToClient( ACH_BULLETSFIRED, 6, ACHVAL_ADD );

		m_pPlayer->pev->punchangle.x -= 4;

		PostShell = 1;

		m_flPumpTime = gpGlobals->time + 0.4;

		MakeWeaponShake( m_pPlayer, WEAPON_SHOTGUN, 0 );

		if( m_pPlayer->LoudWeaponsRestricted )
			m_pPlayer->FireLoudWeaponRestrictionEntity();
	}

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = gpGlobals->time + SHOTGUN_NEXT_PA_TIME; // diffusion - was 0.75
	m_flNextSecondaryAttack = gpGlobals->time + SHOTGUN_NEXT_PA_TIME;

	m_fInReload = 0;
}

void CShotgun::SecondaryAttack( void )
{
	CLIENT_COMMAND(m_pPlayer->edict(), "-attack2\n");

	// stop reloading, move the gun
	if( m_fInReload > 0 )
	{
		if( bUseAfterReloadEmpty )
		{
			SendWeaponAnim( SHOTGUN_END_RELOAD_EMPTY );
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + SHOTGUN_RELOADEMPTY_FINISH_TIME;
			m_flPumpTime = gpGlobals->time + 0.6;
		}
		else
		{
			SendWeaponAnim( SHOTGUN_END_RELOAD );
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + SHOTGUN_RELOAD_FINISH_TIME;
		}

		m_fInReload = 0;
		m_flTimeWeaponIdle = gpGlobals->time + 1.5;
		return;
	}

	if( m_iClip <= 0 )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = gpGlobals->time + 0.15;
		return;
	}

	AskToStopReload = false;
	
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = gpGlobals->time + 0.15;
		return;
	}

	if( m_iClip == 1)
	{
		PrimaryAttack();
		return;
	}
	
	if( m_iClip <= 4)
		LowAmmoMsg(m_pPlayer);

	if( m_iClip > 0 )
		m_flTimeWeaponIdle = gpGlobals->time + 6.0;
	else
		m_flTimeWeaponIdle = 1.5;

	if( m_iClip > 0 )
	{
		m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

		m_iClip -= 2;

		m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

		SendWeaponAnim( SHOTGUN_FIRE2 );

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

		PlayClientSound( m_pPlayer, WEAPON_SHOTGUN, 1 );
	
		Vector vecSrc = m_pPlayer->GetGunPosition( );
		Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
	
		m_pPlayer->FireBullets( 12, vecSrc, vecAiming, VECTOR_CONE_10DEGREES, 8192, BULLET_PLAYER_BUCKSHOT, 0, DMG_WPN_SHOTGUN );

		m_pPlayer->SendAchievementStatToClient( ACH_BULLETSFIRED, 12, ACHVAL_ADD );

		m_pPlayer->pev->punchangle.y -= 3.5;

		switch ( RANDOM_LONG(0,1) )
		{
		case 0:
			m_pPlayer->pev->punchangle.x -= RANDOM_FLOAT (7.5, 10);
			break;
		case 1:
			m_pPlayer->pev->punchangle.x += RANDOM_FLOAT (5, 7.5);
			break;
		}
	
		MakeWeaponShake( m_pPlayer, WEAPON_SHOTGUN, 1 );

		if( m_pPlayer->LoudWeaponsRestricted )
			m_pPlayer->FireLoudWeaponRestrictionEntity();

		PostShell = 2;

		m_flPumpTime = gpGlobals->time + 0.7;
	}

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = gpGlobals->time + SHOTGUN_NEXT_SA_TIME;
	m_flNextSecondaryAttack = gpGlobals->time + SHOTGUN_NEXT_SA_TIME;

	m_fInReload = 0;
}

void CShotgun::Reload( void )
{
//	CLIENT_COMMAND(m_pPlayer->edict(), "-reload\n"); // diffusion - no need to hold the button
	
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 || m_iClip == SHOTGUN_MAX_CLIP)
		return;

	if (m_flNextReload > gpGlobals->time)
		return;

	// don't reload until recoil is done
	if( ((m_flNextPrimaryAttack > gpGlobals->time) || (m_flNextSecondaryAttack > gpGlobals->time)) && (m_fInReload == 0) )
		return;

	// check to see if we're ready to reload
	if (m_fInReload == 0)
	{
		SendWeaponAnim( SHOTGUN_START_RELOAD );
		m_fInReload = 1;
		m_pPlayer->m_flNextAttack = gpGlobals->time + SHOTGUN_STARTRELOAD_TIME;
		m_flTimeWeaponIdle = gpGlobals->time + SHOTGUN_STARTRELOAD_TIME;
		m_flNextPrimaryAttack = gpGlobals->time + 1.5;
		m_flNextSecondaryAttack = gpGlobals->time + 1.5;
		bUseAfterReloadEmpty = (m_iClip <= 0);
		AskToStopReload = false;
		return;
	}
	else if (m_fInReload == 1)
	{
		// waiting for gun to move to side
		if (m_flTimeWeaponIdle > gpGlobals->time)
			return;
		
		m_fInReload = 2;

		SendWeaponAnim( SHOTGUN_RELOAD );

		// Add them to the clip
		m_iClip++;
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

		switch( RANDOM_LONG( 0, 2 ) )
		{
		case 0: EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/reload1.wav", 1, ATTN_NORM, 0, 85 + RANDOM_LONG( 0, 0x1f ) ); break;
		case 1: EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/reload2.wav", 1, ATTN_NORM, 0, 85 + RANDOM_LONG( 0, 0x1f ) ); break;
		case 2: EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/reload3.wav", 1, ATTN_NORM, 0, 85 + RANDOM_LONG( 0, 0x1f ) ); break;
		}

		m_flNextReload = gpGlobals->time + SHOTGUN_RELOAD_TIME;
		m_flTimeWeaponIdle = gpGlobals->time + SHOTGUN_RELOAD_TIME;
	}
	else
	{
		if( m_iClip > 0 )
		{
			if( AskToStopReload )
			{
				if( bUseAfterReloadEmpty )
				{
					SendWeaponAnim( SHOTGUN_END_RELOAD_EMPTY );
					m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + SHOTGUN_RELOADEMPTY_FINISH_TIME;
					m_flPumpTime = gpGlobals->time + 0.6;
				}
				else
				{
					SendWeaponAnim( SHOTGUN_END_RELOAD );
					m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + SHOTGUN_RELOAD_FINISH_TIME;
				}
				m_fInReload = 0;
				m_flTimeWeaponIdle = gpGlobals->time + 1.5;
				AskToStopReload = false;
				return;
			}
		}

		m_fInReload = 1;
	}
}

void CShotgun::WeaponIdle( void )
{
	ResetEmptySound();

	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if( !AskToStopReload && ((m_pPlayer->m_afButtonPressed & IN_ATTACK) || (m_pPlayer->m_afButtonPressed & IN_ATTACK2)) )
		AskToStopReload = true;

	if (m_flPumpTime && m_flPumpTime < gpGlobals->time)
	{
		// play pumping sound
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/shotgun_pump.wav", 1, ATTN_NORM, 0, 95 + RANDOM_LONG(0,0x1f));
		
		if( !bUseAfterReloadEmpty )
		{
			// diffusion - eject shell moved here
			Vector	vecShellVelocity = m_pPlayer->GetAbsVelocity()
				+ gpGlobals->v_right * RANDOM_LONG( 70, 90 )
				+ gpGlobals->v_up * RANDOM_LONG( 125, 175 )
				+ gpGlobals->v_forward * 25;
			if( PostShell == 1 )
			{
				EjectBrass( m_pPlayer->EyePosition() + gpGlobals->v_up * -10 + gpGlobals->v_forward * 25 + gpGlobals->v_right * 12,
					vecShellVelocity, m_pPlayer->GetAbsAngles().y, SHELL_12GAUGE, TE_BOUNCE_SHOTSHELL );
			}
			else if( PostShell == 2 )
			{
				EjectBrass( m_pPlayer->EyePosition() + gpGlobals->v_up * -10 + gpGlobals->v_forward * 25 + gpGlobals->v_right * 12,
					vecShellVelocity, m_pPlayer->GetAbsAngles().y, SHELL_12GAUGE, TE_BOUNCE_SHOTSHELL );
				EjectBrass( m_pPlayer->EyePosition() + gpGlobals->v_up * -12 + gpGlobals->v_forward * 30 + gpGlobals->v_right * 14,
					vecShellVelocity, m_pPlayer->GetAbsAngles().y, SHELL_12GAUGE, TE_BOUNCE_SHOTSHELL );
			}
		}

		bUseAfterReloadEmpty = false;
		m_flPumpTime = 0;
	}

	if (m_flTimeWeaponIdle < gpGlobals->time)
	{
		if (m_iClip == 0 && m_fInReload == 0 && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
		{
			Reload( );
		}
		else if (m_fInReload != 0)
		{
			if( m_iClip < SHOTGUN_MAX_CLIP && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
			{
				Reload();
			}
			else
			{
				// reload debounce has timed out
				if( bUseAfterReloadEmpty )
				{
					SendWeaponAnim( SHOTGUN_END_RELOAD_EMPTY );
					m_flPumpTime = gpGlobals->time + 0.6;
					m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + SHOTGUN_RELOADEMPTY_FINISH_TIME;
				}
				else
				{
					SendWeaponAnim( SHOTGUN_END_RELOAD );
					m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + SHOTGUN_RELOAD_FINISH_TIME;
				}
				
				m_fInReload = 0;
				m_flTimeWeaponIdle = gpGlobals->time + 1.5;
			}
		}
	}
}

class CShotgunAmmo : public CBasePlayerAmmo
{
	DECLARE_CLASS( CShotgunAmmo, CBasePlayerAmmo );

	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/weapons/w_shotbox.mdl");
		BaseClass :: Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/weapons/w_shotbox.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		if (pOther->GiveAmmo( AMMO_BUCKSHOTBOX_GIVE, g_WpnAmmo[WEAPON_SHOTGUN], BUCKSHOT_MAX_CARRY ) != -1)
		{
		//	EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			PlayPickupSound( pOther );
			return TRUE;
		}
		return FALSE;
	}
};

LINK_ENTITY_TO_CLASS( ammo_buckshot, CShotgunAmmo );