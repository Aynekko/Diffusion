#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons/weapons.h"
#include "nodes.h"
#include "player.h"
#include "game/gamerules.h"

class CShotgunXM : public CBasePlayerWeapon
{
	DECLARE_CLASS( CShotgunXM, CBasePlayerWeapon );
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot() { return WPN_SLOT_SHOTGUN_XM + 1; }
	int GetItemInfo( ItemInfo *p );
	int AddToPlayer( CBasePlayer *pPlayer );

	DECLARE_DATADESC();

	void PrimaryAttack( void );
	BOOL Deploy();
	void Reload( void );
	void WeaponIdle( void );
	int m_fInReload;
	float m_flNextReload;

	bool bUseAfterReloadEmpty;
	bool AskToStopReload;
};

LINK_ENTITY_TO_CLASS( weapon_shotgun_xm, CShotgunXM );

BEGIN_DATADESC( CShotgunXM )
	DEFINE_FIELD( m_flNextReload, FIELD_TIME ),
	DEFINE_FIELD( m_fInSpecialReload, FIELD_INTEGER ),
	DEFINE_FIELD( m_flPumpTime, FIELD_TIME ),
END_DATADESC()

void CShotgunXM::Spawn( void )
{
	Precache();
	m_iId = WEAPON_SHOTGUN_XM;
	SET_MODEL( ENT( pev ), "models/weapons/w_m1014.mdl" );

	m_iDefaultAmmo = SHOTGUN_DEFAULT_GIVE;

	FallInit();// get ready to fall
}

void CShotgunXM::Precache( void )
{
	PRECACHE_MODEL( "models/weapons/v_m1014.mdl" );
	PRECACHE_MODEL( "models/weapons/w_m1014.mdl" );
	PRECACHE_MODEL( "models/weapons/p_m1014.mdl" );

	PRECACHE_SOUND( "items/9mmclip1.wav" );

	PRECACHE_SOUND( "weapons/xm_fire.wav" );
	PRECACHE_SOUND( "weapons/xm_fire_d.wav" );

	// shotgun reload
	PRECACHE_SOUND( "weapons/reload1.wav" );
	PRECACHE_SOUND( "weapons/reload2.wav" );
	PRECACHE_SOUND( "weapons/reload3.wav" );

	PRECACHE_SOUND( "weapons/cloth1.wav" );
	PRECACHE_SOUND( "weapons/shotgun_bolt.wav" );

	//	PRECACHE_SOUND ("weapons/sshell1.wav");	// shotgun reload - played on client
	//	PRECACHE_SOUND ("weapons/sshell3.wav");	// shotgun reload - played on client

	PRECACHE_SOUND( "weapons/357_cock1.wav" ); // gun empty sound
	PRECACHE_SOUND( "weapons/scock1.wav" );	// cock gun
}

int CShotgunXM::AddToPlayer( CBasePlayer *pPlayer )
{
	if( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
		WRITE_BYTE( m_iId );
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}

int CShotgunXM::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "buckshot";
	p->iMaxAmmo1 = BUCKSHOT_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = SHOTGUNXM_MAX_CLIP;
	p->iSlot = WPN_SLOT_SHOTGUN_XM;
	p->iPosition = WPN_POS_SHOTGUN_XM;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_SHOTGUN_XM;
	p->iWeight = SHOTGUN_WEIGHT;

	return 1;
}

BOOL CShotgunXM::Deploy()
{
	m_flTimeWeaponIdle = gpGlobals->time + 2.0;
	m_fInReload = 0; // reset any reloading
	AskToStopReload = false;
	m_flNextPrimaryAttack = gpGlobals->time + SHOTGUNXM_DEPLOY_TIME;
	if( bUseAfterReloadEmpty )
	{
		bUseAfterReloadEmpty = false;
		if( m_iClip > 0 )
		{
			m_flNextPrimaryAttack = gpGlobals->time + SHOTGUNXM_DRAWRELOADEMPTY_FINISH_TIME;
			return DefaultDeploy( "models/weapons/v_m1014.mdl", "models/weapons/p_m1014.mdl", SHOTGUNXM_DRAW_AFTER_RELOAD, "shotgun" );
		}
		else
			return DefaultDeploy( "models/weapons/v_m1014.mdl", "models/weapons/p_m1014.mdl", SHOTGUNXM_DRAW, "shotgun" );
	}
	else
		return DefaultDeploy( "models/weapons/v_m1014.mdl", "models/weapons/p_m1014.mdl", SHOTGUNXM_DRAW, "shotgun" );
}

void CShotgunXM::PrimaryAttack()
{
	// stop reloading, move the gun
	if( m_fInReload > 0 )
	{	
		// don't stop reload if didn't load anything yet
		if( m_iClip <= 0 )
		{
			CLIENT_COMMAND( m_pPlayer->edict(), "-attack\n" );
			return;
		}

		CLIENT_COMMAND( m_pPlayer->edict(), "-attack\n" );
		if( bUseAfterReloadEmpty )
		{
			SendWeaponAnim( SHOTGUNXM_END_RELOAD_EMPTY );
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + SHOTGUNXM_RELOADEMPTY_FINISH_TIME;
		}
		else
		{
			SendWeaponAnim( SHOTGUNXM_END_RELOAD );
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + SHOTGUNXM_RELOAD_FINISH_TIME;
		}
		m_fInReload = 0;
		m_flTimeWeaponIdle = gpGlobals->time + 1.5;
		return;
	}
	
	if( m_iClip <= 0 )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = gpGlobals->time + 0.15;
		CLIENT_COMMAND( m_pPlayer->edict(), "-attack\n" );
		return;
	}

	AskToStopReload = false;

	// don't fire underwater
	if( m_pPlayer->pev->waterlevel == 3 )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = gpGlobals->time + 0.15;
		return;
	}

	if( m_iClip <= 4 )
		LowAmmoMsg( m_pPlayer );

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

		SendWeaponAnim( SHOTGUNXM_FIRE );

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

		PlayClientSound( m_pPlayer, WEAPON_SHOTGUN_XM );

		Vector vecSrc = m_pPlayer->GetGunPosition();
		Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

		m_pPlayer->FireBullets( 6, vecSrc, vecAiming, VECTOR_CONE_6DEGREES, 8192, BULLET_PLAYER_BUCKSHOT, 0, DMG_WEAPON_SHOTGUN_XM );

		MakeWeaponShake( m_pPlayer, WEAPON_SHOTGUN_XM, 0 );

		if( m_pPlayer->LoudWeaponsRestricted )
			m_pPlayer->FireLoudWeaponRestrictionEntity();

		m_pPlayer->SendAchievementStatToClient( ACH_BULLETSFIRED, 6, ACHVAL_ADD );

		m_pPlayer->pev->punchangle.x -= 7;

		Vector	vecShellVelocity = m_pPlayer->GetAbsVelocity()
			+ gpGlobals->v_right * RANDOM_LONG( 70, 90 )
			+ gpGlobals->v_up * RANDOM_LONG( 50, 75 )
			+ gpGlobals->v_forward * 25;

		EjectBrass( m_pPlayer->EyePosition() + gpGlobals->v_up * -10 + gpGlobals->v_forward * 28 + gpGlobals->v_right * 12,
			vecShellVelocity, m_pPlayer->GetAbsAngles().y, SHELL_12GAUGE, TE_BOUNCE_SHOTSHELL );
	}

	if( !m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );

	m_flNextPrimaryAttack = gpGlobals->time + SHOTGUNXM_NEXT_PA_TIME;

	m_fInReload = 0;
}

void CShotgunXM::Reload( void )
{
	//	CLIENT_COMMAND(m_pPlayer->edict(), "-reload\n"); // diffusion - no need to hold the button

	if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 || m_iClip == SHOTGUNXM_MAX_CLIP )
		return;

	if( m_flNextReload > gpGlobals->time )
		return;

	// don't reload until recoil is done
	if( ((m_flNextPrimaryAttack > gpGlobals->time) || (m_flNextSecondaryAttack > gpGlobals->time)) && (m_fInReload == 0) )
		return;

	// check to see if we're ready to reload
	if( m_fInReload == 0 )
	{
		SendWeaponAnim( SHOTGUNXM_START_RELOAD );
		m_fInReload = 1;
		m_pPlayer->m_flNextAttack = gpGlobals->time + SHOTGUNXM_STARTRELOAD_TIME;
		m_flTimeWeaponIdle = gpGlobals->time + SHOTGUNXM_STARTRELOAD_TIME;
		m_flNextPrimaryAttack = gpGlobals->time + 1;
		m_flNextSecondaryAttack = gpGlobals->time + 1;
		bUseAfterReloadEmpty = (m_iClip <= 0);
		AskToStopReload = false;
		return;
	}
	else if( m_fInReload == 1 )
	{
		// waiting for gun to move to side
		if( m_flTimeWeaponIdle > gpGlobals->time )
			return;

		m_fInReload = 2;

		SendWeaponAnim( SHOTGUNXM_RELOAD );

		// Add them to the clip
		m_iClip++;
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

		switch( RANDOM_LONG( 0, 2 ) )
		{
		case 0: EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/reload1.wav", 1, ATTN_NORM, 0, 85 + RANDOM_LONG( 0, 0x1f ) ); break;
		case 1: EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/reload2.wav", 1, ATTN_NORM, 0, 85 + RANDOM_LONG( 0, 0x1f ) ); break;
		case 2: EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/reload3.wav", 1, ATTN_NORM, 0, 85 + RANDOM_LONG( 0, 0x1f ) ); break;
		}

		m_flNextReload = gpGlobals->time + SHOTGUNXM_RELOAD_TIME;
		m_flTimeWeaponIdle = gpGlobals->time + SHOTGUNXM_RELOAD_TIME;
	}
	else
	{
		if( m_iClip > 0 )
		{
			if( AskToStopReload )
			{
				if( bUseAfterReloadEmpty )
				{
					SendWeaponAnim( SHOTGUNXM_END_RELOAD_EMPTY );
					m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + SHOTGUNXM_RELOADEMPTY_FINISH_TIME;
				}
				else
				{
					SendWeaponAnim( SHOTGUNXM_END_RELOAD );
					m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + SHOTGUNXM_RELOAD_FINISH_TIME;
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

void CShotgunXM::WeaponIdle( void )
{
	ResetEmptySound();

	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if( !AskToStopReload && ((m_pPlayer->m_afButtonPressed & IN_ATTACK) || (m_pPlayer->m_afButtonPressed & IN_ATTACK2)) )
		AskToStopReload = true;

	if( m_flTimeWeaponIdle < gpGlobals->time )
	{		
		if( m_iClip == 0 && m_fInReload == 0 && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
		{
			Reload();
		}
		else if( m_fInReload > 0 )
		{
			if( m_iClip < SHOTGUNXM_MAX_CLIP && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
			{
				Reload();
			}
			else
			{
				// reload debounce has timed out
				if( bUseAfterReloadEmpty )
				{
					SendWeaponAnim( SHOTGUNXM_END_RELOAD_EMPTY );
					m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + SHOTGUNXM_RELOADEMPTY_FINISH_TIME;
					bUseAfterReloadEmpty = false;
				}
				else
				{
					SendWeaponAnim( SHOTGUNXM_END_RELOAD );
					m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + SHOTGUNXM_RELOAD_FINISH_TIME;
				}

				m_fInReload = 0;
				m_flTimeWeaponIdle = gpGlobals->time + 2.0f;
			}
		}
		else
		{
			/*
			int iAnim = 0;
			float flRand = RANDOM_FLOAT( 0, 1 );
			if( flRand <= 0.8 )
			{
				iAnim = SHOTGUNXM_IDLE_DEEP;
				m_flTimeWeaponIdle = gpGlobals->time + (60.0 / 12.0);// * RANDOM_LONG(2, 5);
			}
			else if( flRand <= 0.95 )
			{
				iAnim = SHOTGUNXM_IDLE;
				m_flTimeWeaponIdle = gpGlobals->time + (20.0 / 9.0);
			}
			else
			{
				iAnim = SHOTGUNXM_IDLE4;
				m_flTimeWeaponIdle = gpGlobals->time + (20.0 / 9.0);
			}

			SendWeaponAnim( iAnim );*/
		}
	}
}