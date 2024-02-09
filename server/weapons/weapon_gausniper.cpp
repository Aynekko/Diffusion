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

// Gauss edited for Diffusion mod

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons/weapons.h"
#include "nodes.h"
#include "player.h"
#include "entities/soundent.h"
#include "shake.h"
#include "game/gamerules.h"

#define GAUSS_PRIMARY_CHARGE_VOLUME	256 // how loud gauss is while charging
#define GAUSS_PRIMARY_FIRE_VOLUME	450 // how loud gauss is when discharged

#define GAUSS_MID_ZOOM 30
#define GAUSS_MAX_ZOOM 10

class CGauss : public CBasePlayerWeapon
{
	DECLARE_CLASS( CGauss, CBasePlayerWeapon );
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return WPN_SLOT_GAUSS + 1; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );
	void ItemPostFrame( void );

	DECLARE_DATADESC();

	BOOL Deploy( void );
	void Holster( void );

	void PrimaryAttack( void );
	void Attack( void );
	void SecondaryAttack( void );
	void Reload( void );
	void WeaponIdle( void );
	void ResetZoom( void );
	void UpdateHUD( void );
	bool ReflectMirror( TraceResult &tr, CBaseEntity *e );
	void StartFire( void );
	void Fire( Vector vecOrigSrc, Vector vecDirShooting, float flDamage );

	int m_iBalls;
	int m_iGlow;
	int m_iBeam;
	int m_iSoundState; // don't save this
	float Charge; // 0.0 - 1.0
	int CL_Charge;

	float nReflect; // reflection degree - 0.5 default, 0.1 for mirrors
};

BEGIN_DATADESC( CGauss )
	DEFINE_FIELD( Charge, FIELD_FLOAT ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( weapon_gausniper, CGauss );

void CGauss::ItemPostFrame( void )
{
	// manage the charge in this constantly active function
	if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		Charge = 0.0f;
	else
	{
		// charge weapon
		if( Charge < 1.0f && (gpGlobals->time > m_pPlayer->m_flNextAttack) )
			Charge += gpGlobals->frametime;
	}

	Charge = bound( 0.0f, Charge, 1.0f );

	UpdateHUD();

	BaseClass::ItemPostFrame();
}

void CGauss::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_GAUSS;
	SET_MODEL(ENT(pev), "models/weapons/w_gausniper.mdl");

	m_iDefaultAmmo = GAUSS_DEFAULT_GIVE;

	Charge = 0.0f;

	FallInit();// get ready to fall down.
}

void CGauss::Precache( void )
{
	PRECACHE_MODEL("models/weapons/w_gausniper.mdl");
	PRECACHE_MODEL("models/weapons/v_gausniper.mdl");
	PRECACHE_MODEL("models/weapons/p_gausniper.mdl");

	PRECACHE_SOUND( "weapons/crystal_reload.wav" ); // Precaching reloading sound
	PRECACHE_SOUND("items/9mmclip1.wav");

	PRECACHE_SOUND("weapons/gausniper.wav");
	PRECACHE_SOUND("weapons/crystal_empty.wav"); // New dry sound
	PRECACHE_SOUND( "weapons/gauss_charge.wav" );
	
	m_iGlow = PRECACHE_MODEL( "sprites/hotglow.spr" );
	m_iBalls = PRECACHE_MODEL( "sprites/hotglow.spr" );
	m_iBeam = PRECACHE_MODEL( "sprites/smoke.spr" );

	CL_Charge = -1;
}

int CGauss::AddToPlayer( CBasePlayer *pPlayer )
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

int CGauss::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "uranium";
	p->iMaxAmmo1 = URANIUM_MAX_CARRY;
	p->iMaxClip = WEAPON_NOCLIP;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iSlot = WPN_SLOT_GAUSS;
	p->iPosition = WPN_POS_GAUSS;
	p->iId = m_iId = WEAPON_GAUSS;
	p->iFlags = 0;
	p->iWeight = GAUSS_WEIGHT;

	return 1;
}

void CGauss::ResetZoom( void )
{
	if( m_pPlayer->ZoomState > 0 )
		UTIL_ScreenFade( m_pPlayer, g_vecZero, 0.5, 0, 255, 0x0000 );

	m_pPlayer->ZoomState = 0;
	m_pPlayer->m_flFOV = 0;

	MESSAGE_BEGIN( MSG_ONE, gmsgZoom, NULL, m_pPlayer->pev );
		WRITE_BYTE( m_pPlayer->ZoomState );
		WRITE_BYTE( WEAPON_GAUSS );
	MESSAGE_END();
}

BOOL CGauss::Deploy( void )
{
	m_flTimeWeaponIdle = gpGlobals->time + 5.0;

	if( Charge < 1.0f )
		Charge = 0.0f;

	UpdateHUD();

	m_flNextPrimaryAttack = gpGlobals->time + DEFAULT_DEPLOY_TIME;

	return DefaultDeploy( "models/weapons/v_gausniper.mdl", "models/weapons/p_gausniper.mdl", GAUSS_DRAW, "gauss" );
}

void CGauss::Holster( void )
{
	// cancel any zooming in progress
	ResetZoom();

	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;

	SendWeaponAnim( GAUSS_HOLSTER );

	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 0, ATTN_NORM);

	// need to send "charge" to reset weapon skin on client
	UpdateHUD();

	BaseClass::Holster();
}

void CGauss::PrimaryAttack( void )
{
	if( Charge < 1.0f )
		return;
	
	Attack();
}

void CGauss::UpdateHUD( void )
{
	int NewCharge = (int)(Charge * 10);
	if( CL_Charge == NewCharge )
		return;
	
	MESSAGE_BEGIN( MSG_ONE, gmsgGaussHUD, NULL, m_pPlayer->pev );
		WRITE_BYTE( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] ); // current ammo count
		WRITE_BYTE( NewCharge );
	MESSAGE_END();

	CL_Charge = NewCharge;
}

void CGauss::Attack( void )
{
	// don't fire underwater
	if( m_pPlayer->pev->waterlevel == 3 )
	{
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/crystal_empty.wav", 1.0, ATTN_NORM);

		m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + 0.5;
		return;
	}

	if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
	{
		CLIENT_COMMAND(m_pPlayer->edict(), "-attack\n");
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/crystal_empty.wav", 1.0, ATTN_NORM);
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = gpGlobals->time + 0.5;
		UpdateHUD();
		return;
	}

	// do not show when zoomed, to not obstruct the hud
	if( (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 3) && (m_pPlayer->ZoomState == 0))
		LowAmmoMsg(m_pPlayer);
	
	StartFire();

	m_flTimeWeaponIdle = gpGlobals->time + 1.0;
}

void CGauss::SecondaryAttack(void)
{
	CLIENT_COMMAND( m_pPlayer->edict(), "-attack2\n" );

	if( !IS_DEDICATED_SERVER() )
	{
		if( CVAR_GET_FLOAT( "default_fov" ) <= GAUSS_MAX_ZOOM )
		{
			ALERT( at_error, "weapon_gausniper can't zoom, default_fov is too low!\n" );
			return;
		}
	}

	if( m_pPlayer->ZoomState == 0 ) // not zoomed, zoom to first level (fov 30)
	{
		UTIL_ScreenFade( m_pPlayer, g_vecZero, 0.25, 0, 255, 0x0000 );
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/xbow_scope.wav", 1, 1.5 );
		m_pPlayer->ZoomState = 1; // zooming in, first step
		MESSAGE_BEGIN( MSG_ONE, gmsgZoom, NULL, m_pPlayer->pev );
			WRITE_BYTE( m_pPlayer->ZoomState );
			WRITE_BYTE( WEAPON_GAUSS );
		MESSAGE_END();
		m_pPlayer->m_flFOV = GAUSS_MID_ZOOM;
	}
	else if( m_pPlayer->ZoomState == 1 ) // zoom to max level (fov 10)
	{
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/xbow_scope.wav", 1, 1.5 );
		m_pPlayer->ZoomState = 2; // zooming in, second step
		MESSAGE_BEGIN( MSG_ONE, gmsgZoom, NULL, m_pPlayer->pev );
			WRITE_BYTE( m_pPlayer->ZoomState );
			WRITE_BYTE( WEAPON_GAUSS );
		MESSAGE_END();
		m_pPlayer->m_flFOV = GAUSS_MAX_ZOOM;
	}
	else if( m_pPlayer->ZoomState == 2 ) // fully zoomed, unzoom fast
	{
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/xbow_scope.wav", 1, 1.5 );
		m_pPlayer->ZoomState = 3; // zooming out
		MESSAGE_BEGIN( MSG_ONE, gmsgZoom, NULL, m_pPlayer->pev );
			WRITE_BYTE( m_pPlayer->ZoomState );
			WRITE_BYTE( WEAPON_GAUSS );
		MESSAGE_END();
		m_pPlayer->ZoomState = 0; // hopefully we zoomed out by now. reset to default state
		m_pPlayer->m_flFOV = 0;
		UTIL_ScreenFade( m_pPlayer, g_vecZero, 0.5, 0, 255, 0x0000 );
	}

	UpdateHUD();

	m_flNextSecondaryAttack = gpGlobals->time + 0.2;
}

void CGauss::Reload( void )
{
	// diffusion - this was the first thing I ever did in coding...time flies
	/*
	// BUGBUG - without this, reloading still triggers, idk why
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 || m_iClip == GAUSS_MAX_CLIP)
		return;

	AmmoTaken = 0; // just in case
	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/crystal_reload.wav", 1.0, ATTN_NORM);
	DefaultReload( GAUSS_MAX_CLIP, GAUSS_RELOAD, 3 );
	*/
}

//=========================================================
// StartFire- since all of this code has to run and then 
// call Fire(), it was easier at this point to rip it out 
// of weaponidle() and make its own function then to try to
// merge this into Fire(), which has some identical variable names 
//=========================================================
void CGauss::StartFire( void )
{
	UTIL_ScreenShakeLocal( m_pPlayer, m_pPlayer->GetAbsOrigin(), 10, 100.0, 0.2, 0, true );
//	MakeWeaponShake( m_pPlayer, WEAPON_GAUSS, 0 ); // I didn't finish the "predicting" on this weapon

	float flDamage = DMG_WPN_GAUSS_FULL;
	
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecAiming = gpGlobals->v_forward;
	Vector vecSrc = m_pPlayer->GetGunPosition( ); // + gpGlobals->v_up * -8 + gpGlobals->v_right * 8;

	//ALERT ( at_console, "Time:%f Damage:%f\n", gpGlobals->time - m_flStartCharge, flDamage );

	float flZVel = m_pPlayer->GetAbsVelocity().z;

//	m_pPlayer->SetAbsVelocity( m_pPlayer->GetAbsVelocity() - gpGlobals->v_forward * flDamage * 1 );

	SendWeaponAnim( GAUSS_FIRE );

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	PlayClientSound( m_pPlayer, WEAPON_GAUSS );

	Fire( vecSrc, vecAiming, flDamage );

	if( m_pPlayer->LoudWeaponsRestricted )
		m_pPlayer->FireLoudWeaponRestrictionEntity();
}

bool CGauss::ReflectMirror( TraceResult &tr, CBaseEntity *e )
{
	if( UTIL_GetModelType( e->pev->modelindex ) != mod_brush )
		return false;

	// diffusion - I'm not sure about those positions...
	Vector vecStart = tr.vecEndPos + tr.vecPlaneNormal * -2;	// mirror thickness
	Vector vecEnd = tr.vecEndPos + tr.vecPlaneNormal * 2;
	const char *pTextureName = TRACE_TEXTURE( e->edict(), vecStart, vecEnd );

	if( pTextureName != NULL && !Q_strnicmp( pTextureName, "reflect", 7 ) )
		return true; // valid surface

	if( pTextureName != NULL && !Q_strnicmp( pTextureName, "!reflect", 8 ) )
		return true; // valid surface

	if( pTextureName != NULL && !Q_strnicmp( pTextureName, "lasrefl", 7 ) )
		return true; // diffusion - hack to allow reflection without rendering mirror

	return false;
}

void CGauss::Fire( Vector vecOrigSrc, Vector vecDir, float flDamage )
{
	m_pPlayer->m_iWeaponVolume = GAUSS_PRIMARY_FIRE_VOLUME;

	// take ammo
	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= 1;

	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;

	Charge = 0.0f; // set to recharge
	UpdateHUD(); // update weapon skin

	Vector vecDeviate = g_vecZero;
	if( m_pPlayer->ZoomState == 0 ) // non-zoomed player can't have a straight shot
		vecDeviate = Vector( RANDOM_LONG( -200, 200 ), RANDOM_LONG( -200, 200 ), RANDOM_LONG( -200, 200 ) );

	Vector vecSrc = vecOrigSrc;
	Vector vecDest = vecSrc + vecDir * 8192 + vecDeviate;
	edict_t	*pentIgnore;
	TraceResult tr, beam_tr;
	float flMaxFrac = 1.0;
	int	nTotal = 0;
	int fHasPunched = 0;
	int fFirstBeam = 1;
	int	nMaxHits = 10;

	pentIgnore = ENT( m_pPlayer->pev );

	// ALERT( at_console, "%f %f\n", tr.flFraction, flMaxFrac );

	while (flDamage > 3 && nMaxHits > 0)   // diffusion - was flDamage > 10
	{
		nMaxHits--;

		// ALERT( at_console, "." );
		UTIL_TraceLine(vecSrc, vecDest, dont_ignore_monsters, pentIgnore, &tr);

		if (tr.fAllSolid)
			break;

		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

		if (pEntity == NULL)
			break;

		if (fFirstBeam)
		{
			m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
			fFirstBeam = 0;

			Vector tmpSrc = vecSrc + gpGlobals->v_up * -4 + gpGlobals->v_right * 3 + gpGlobals->v_forward * 32;

			// don't draw beam until the damn thing looks like it's coming out of the barrel
			// draw beam
			MESSAGE_BEGIN( MSG_PAS, gmsgTempEnt, tr.vecEndPos );
				WRITE_BYTE( TE_BEAMENTPOINT );
				WRITE_SHORT( m_pPlayer->entindex() + 0x1000 );
				WRITE_COORD( tr.vecEndPos.x);
				WRITE_COORD( tr.vecEndPos.y);
				WRITE_COORD( tr.vecEndPos.z);
				WRITE_SHORT( m_iBeam );
				WRITE_BYTE( 0 ); // startframe
				WRITE_BYTE( 0 ); // framerate
				WRITE_BYTE( 3 ); // life

				WRITE_BYTE( 10 );  // width

				WRITE_BYTE( 0 );   // noise

				WRITE_BYTE( 70 );   // r, g, b
				WRITE_BYTE( 169 );   // r, g, b
				WRITE_BYTE( 255 );   // r, g, b
				
				WRITE_BYTE( 255 );	// brightness

				WRITE_BYTE( 0 );		// speed
			MESSAGE_END();
			
			nTotal += 26;

			MESSAGE_BEGIN( MSG_PAS, gmsgTempEnt, tr.vecEndPos );
				WRITE_BYTE( TE_BEAMPARTICLES );
				WRITE_SHORT( m_pPlayer->entindex() );
				WRITE_COORD( tr.vecEndPos.x );
				WRITE_COORD( tr.vecEndPos.y );
				WRITE_COORD( tr.vecEndPos.z );
			MESSAGE_END();
		}
		else
		{
			// draw beam
			
			MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, vecSrc );
				WRITE_BYTE( TE_BEAMPOINTS);
				WRITE_COORD( vecSrc.x);
				WRITE_COORD( vecSrc.y);
				WRITE_COORD( vecSrc.z);
				WRITE_COORD( tr.vecEndPos.x);
				WRITE_COORD( tr.vecEndPos.y);
				WRITE_COORD( tr.vecEndPos.z);
				WRITE_SHORT( m_iBeam );
				WRITE_BYTE( 0 ); // startframe
				WRITE_BYTE( 0 ); // framerate
				WRITE_BYTE( 1 ); // life

				WRITE_BYTE( 25 );  // width

				WRITE_BYTE( 0 );   // noise

				// secondary shot is always white, and intensity based on charge
				WRITE_BYTE( 255 );   // r, g, b
				WRITE_BYTE( 255 );   // r, g, b
				WRITE_BYTE( 255 );   // r, g, b
				
				WRITE_BYTE( 255 );	// brightness

				WRITE_BYTE( 0 );		// speed
			MESSAGE_END();
			
			nTotal += 26;
		}

		if (pEntity->pev->takedamage)
		{
			ClearMultiDamage();
			pEntity->TraceAttack( m_pPlayer->pev, flDamage, vecDir, &tr, DMG_SHOCK );
			ApplyMultiDamage(m_pPlayer->pev, m_pPlayer->pev);
		}

		// ALERT( at_console, "%s\n", STRING( pEntity->pev->classname ));

		if ( pEntity->ReflectGauss() )
		{
			nReflect = 0.6; // 60 degrees
			
			if( ReflectMirror( tr, pEntity ) )
				nReflect = 0.98;
			
			pentIgnore = NULL;

			float n = -DotProduct(tr.vecPlaneNormal, vecDir);
		//	ALERT( at_console, "reflect %f\n", n );
			if (n < nReflect) 
			{
				// reflect
				Vector r;
			
				r = 2.0 * tr.vecPlaneNormal * n + vecDir;
				flMaxFrac = flMaxFrac - tr.flFraction;
				vecDir = r;
				vecSrc = tr.vecEndPos + vecDir * 8;
				vecDest = vecSrc + vecDir * 8192;

				// explode a bit
				m_pPlayer->RadiusDamage( tr.vecEndPos, pev, m_pPlayer->pev, flDamage * n, flDamage * n * 2.5, CLASS_NONE, DMG_BLAST );

				// bounce wall glow
				MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, tr.vecEndPos );
					WRITE_BYTE( TE_GLOWSPRITE );
					WRITE_COORD( tr.vecEndPos.x);		// pos
					WRITE_COORD( tr.vecEndPos.y);
					WRITE_COORD( tr.vecEndPos.z);
					WRITE_SHORT( m_iGlow );				// model
					WRITE_BYTE( flDamage * n * 0.5 );	// life * 10
					WRITE_BYTE( 2 );					// size * 10
					WRITE_BYTE( flDamage * n );			// brightness
				MESSAGE_END();

				nTotal += 13;

				// lose energy
				if (n == 0) n = 0.1;
				flDamage = flDamage * (1 - n);
			}
			else
			{
				// tunnel
				DecalGunshot( &tr, BULLET_MONSTER_12MM );

				// entry wall glow
				MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, tr.vecEndPos );
					WRITE_BYTE( TE_GLOWSPRITE );
					WRITE_COORD( tr.vecEndPos.x);	// pos
					WRITE_COORD( tr.vecEndPos.y);
					WRITE_COORD( tr.vecEndPos.z);
					WRITE_SHORT( m_iGlow );		// model
					WRITE_BYTE( 60 );				// life * 10
					WRITE_BYTE( 10 );				// size * 10
					WRITE_BYTE( flDamage );			// brightness
				MESSAGE_END();
				nTotal += 13;

				// limit it to one hole punch
				if (fHasPunched)
					break;

				fHasPunched = 1;

				// slug doesn't punch through ever with primary 
				// fire, so leave a little glowy bit and make some balls
				MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, tr.vecEndPos );
					WRITE_BYTE( TE_GLOWSPRITE );
					WRITE_COORD( tr.vecEndPos.x);	// pos
					WRITE_COORD( tr.vecEndPos.y);
					WRITE_COORD( tr.vecEndPos.z);
					WRITE_SHORT( m_iGlow );		// model
					WRITE_BYTE( 20 );				// life * 10
					WRITE_BYTE( 3 );				// size * 10
					WRITE_BYTE( 200 );			// brightness
				MESSAGE_END();

				flDamage = 0;
			}
		}
		else
		{
			vecSrc = tr.vecEndPos + vecDir;
			pentIgnore = ENT( pEntity->pev );
		}
	}

	// ALERT( at_console, "%d bytes\n", nTotal );
}

void CGauss::WeaponIdle( void )
{
	ResetEmptySound();

	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;
}

class CGaussAmmo : public CBasePlayerAmmo
{
	DECLARE_CLASS( CGaussAmmo, CBasePlayerAmmo );

	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/weapons/w_gaussammo.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/weapons/w_gaussammo.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		if (pOther->GiveAmmo( AMMO_URANIUMBOX_GIVE, "uranium", URANIUM_MAX_CARRY ) != -1)
		{
		//	EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			PlayPickupSound( pOther );
			return TRUE;
		}
		return FALSE;
	}
};

LINK_ENTITY_TO_CLASS( ammo_gaussclip, CGaussAmmo );