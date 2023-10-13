#include "hud.h"
#include "r_local.h"
#include "utils.h"
#include "ammohistory.h"
#include "event_api.h"

//===============================================================================
// diffusion - test: local animations for weapons (right now shooting only...XD)
// This must be always re-checked if you do any changes in weapons, otherwise don't use it
// ...or just write proper weapon prediction :>
//===============================================================================
float localanim_NextPAttackTime = 0;
float localanim_NextSAttackTime = 0;
int localanim_WeaponID = 0;
bool localanim_AllowRpgShoot = false;

bool LocalWeaponAnims( void )
{
	//	if( CL_IsThirdPerson() || CL_IsDead() )
	//		return;

	if( cl_localweaponanims->value == 2 )
		return true; // allow in singleplayer too (for testing)

	if( cl_localweaponanims->value == 1 && tr.viewparams.maxclients > 1 )
		return true; // only in multiplayer

	return false;
}

//============================================================
// a lot of work but maybe it is worth it until Real Prediction
// would be implemented (press x to doubt) by a guy with bigger brain!
// This function checks if the animation can be played right away, when player presses SHOOT button.
// If it's a 'shoot' animation, then it just plays it here and immediately,
// and skips the delayed (or packet-lost) animation which came (or didn't) from server,
// along with sounds and screenshake effects
//============================================================
bool CheckForLocalWeaponShootAnimation( int seq )
{
	if( !LocalWeaponAnims() )
		return false;

	// check if it's an attack animation!
	switch( localanim_WeaponID )
	{
	case WEAPON_HKMP5:
		if( seq == 1 ) // caught reload anim!
			localanim_NextPAttackTime = tr.time + 2.5;
		else if( seq == 2 ) // caught reload_noshot anim!
			localanim_NextPAttackTime = tr.time + 2.0;
		else if( seq == 3 ) // caught deploy anim!
			localanim_NextPAttackTime = tr.time + 1.0;

		if( seq < 4 ) // only interested in FIRE animations
			return false;
		else
			return true;
		break;
	case WEAPON_MRC:
		if( seq == 3 ) // caught reload anim!
			localanim_NextPAttackTime = tr.time + 2.0;
		else if( seq == 4 ) // caught deploy anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 1.0;

		if( seq < 5 && seq != 2 ) // only interested in FIRE animations
			return false;
		else
			return true;
		break;
	case WEAPON_GLOCK:
		if( seq == 5 || seq == 6 ) // caught reload anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 1.5;
		else if( seq == 7 ) // caught deploy anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 1.0;

		if( seq != 3 ) // only interested in FIRE animations
			return false;
		else
			return true;
		break;
	case WEAPON_AR2:
		if( seq == 2 ) // caught deploy anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 0.5;

		if( seq != 1 && seq != 3 ) // only interested in FIRE animations
			return false;
		else
			return true;
		break;
	case WEAPON_CROSSBOW:
		if( seq == 7 ) // caught reload anim!
			localanim_NextPAttackTime = tr.time + 4.5;
		else if( seq == 8 || seq == 9 ) // caught deploy anim!
			localanim_NextPAttackTime = tr.time + 0.5;

		if( seq != 4 && seq != 6 ) // only interested in FIRE animations
			return false;
		else
			return true;
		break;
	case WEAPON_DEAGLE:
		if( seq == 3 ) // caught reload_empty anim!
			localanim_NextPAttackTime = tr.time + 3.5;
		else if( seq == 4 ) // caught reload anim!
			localanim_NextPAttackTime = tr.time + 2.6;
		else if( seq == 6 ) // caught deploy anim!
			localanim_NextPAttackTime = tr.time + 1.0;

		if( seq != 2 && seq != 7 ) // only interested in FIRE animations
			return false;
		else
			return true;
		break;
	case WEAPON_FIVESEVEN:
		if( seq == 4 ) // caught reload anim!
			localanim_NextPAttackTime = tr.time + 3.0;
		else if( seq == 5 ) // caught deploy anim!
			localanim_NextPAttackTime = tr.time + 1.0;

		if( seq != 1 && seq != 3 ) // only interested in FIRE animations
			return false;
		else
			return true;
		break;
	case WEAPON_G36C:
		if( seq == 1 ) // caught reload anim!
			localanim_NextPAttackTime = tr.time + 3.0;
		else if( seq == 2 ) // caught deploy anim!
			localanim_NextPAttackTime = tr.time + 1.3;
		else if( seq == 4 ) // caught reload empty anim!
			localanim_NextPAttackTime = tr.time + 4.8;

		if( seq != 3 ) // only interested in FIRE animations
			return false;
		else
			return true;
		break;
	case WEAPON_KNIFE:
		if( seq == 1 ) // caught deploy anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 1.1;

		if( seq != 3 && seq != 6 ) // only interested in FIRE animations
			return false;
		else
			return true;
		break;
	case WEAPON_RPG:
		if( seq == 2 ) // caught reload anim!
		{
			localanim_NextPAttackTime = tr.time + 2.0;
			localanim_AllowRpgShoot = true;
		}
		else if( seq == 7 || seq == 5 ) // caught deploy anim!
		{
			localanim_NextPAttackTime = tr.time + 0.5;
			localanim_AllowRpgShoot = true;
		}

		if( seq != 3 ) // only interested in FIRE animations
			return false;
		else
			return true;
		break;
	case WEAPON_SHOTGUN:
		if( seq == 5 || seq == 3 ) // caught reload anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 1.5; // !!! slightly pushed forward...
		else if( seq == 6 ) // caught deploy anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 0.5;
		else if( seq == 4 ) // caught finish reload anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 0.7;

		if( seq != 1 && seq != 2 ) // only interested in FIRE animations
			return false;
		else
			return true;
		break;
	case WEAPON_SHOTGUN_XM:
		if( seq == 5 || seq == 3 ) // caught reload anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 1.5; // !!! slightly pushed forward...
		else if( seq == 6 ) // caught deploy anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 0.5;
		else if( seq == 4 ) // caught finish reload anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 0.8;

		if( seq != 1 ) // only interested in FIRE animations
			return false;
		else
			return true;
		break;
	case WEAPON_SNIPER:
		if( seq == 3 ) // caught reload anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 5.5;
		else if( seq == 4 ) // caught reload_noshot anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 4.5;
		else if( seq == 0 ) // caught deploy anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 1;

		if( seq != 2 ) // only interested in FIRE animations
			return false;
		else
			return true;
		break;
	}

	return false;
}

//=========================================================
// V_LocalWeaponAnimations: this function keeps track of
// button presses, timings and shake/sound effects
//=========================================================
void V_LocalWeaponAnimations( struct ref_params_s *pparams )
{
	if( !LocalWeaponAnims() )
		return;

	if( !RP_NORMALPASS() )
		return;

	if( CL_IsThirdPerson() )
		return;

	// weapons are hidden
	if( gHUD.m_iHideHUDDisplay & (HIDEHUD_WPNS | HIDEHUD_ALL | HIDEHUD_WPNS_HOLDABLEITEM | HIDEHUD_WPNS_CUSTOM) )
		return;

	cl_entity_t *player = gEngfuncs.GetLocalPlayer();

	bool Attack1 = (gHUD.m_iKeyBits & IN_ATTACK) ? true : false;
	bool Attack2 = (gHUD.m_iKeyBits & IN_ATTACK2) ? true : false;
	int CurrentPAmmoClip = 0;
	int CurrentSAmmoClip = 0;
	int MaxSAmmoCapacity = 0;
	bool Underwater = (pparams->waterlevel == 3);
	if( gHUD.m_Ammo.m_pWeapon )
	{
		CurrentPAmmoClip = gHUD.m_Ammo.m_pWeapon->iClip;
		CurrentSAmmoClip = gWR.CountAmmo( gHUD.m_Ammo.m_pWeapon->iAmmo2Type );
		MaxSAmmoCapacity = gHUD.m_Ammo.m_pWeapon->iMax2; // check if weapon has secondary ammo at all
	}

	if( !gHUD.CanShoot )
	{
		Attack1 = false;
		Attack2 = false;
	}

	if( Attack1 && (tr.time < localanim_NextPAttackTime) )
		return;

	if( Attack1 && CurrentPAmmoClip == 0 )
		return;

	if( Attack2 && (tr.time < localanim_NextSAttackTime) )
		return;

	if( Attack2 && CurrentSAmmoClip == 0 && MaxSAmmoCapacity != -1 )
		return;

	switch( localanim_WeaponID )
	{
	case WEAPON_HKMP5:
		if( Attack1 && !Underwater )
		{
			gEngfuncs.pfnWeaponAnim( RANDOM_LONG( 4, 6 ), 0 );
			switch( RANDOM_LONG( 0, 2 ) )
			{
			case 0: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/mp5-1.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			case 1: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/mp5-2.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			case 2: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/mp5-3.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			}
			localanim_NextPAttackTime = tr.time + 0.1;
			R_MakeWeaponShake( WEAPON_HKMP5, 0, true );
			PlayLowAmmoSound( player->index, player->origin, (CurrentPAmmoClip <= 10 ? CurrentPAmmoClip : 0) );
		}
		break;
	case WEAPON_MRC:
		if( Attack1 && !Underwater )
		{
			gEngfuncs.pfnWeaponAnim( RANDOM_LONG( 5, 7 ), 0 );
			switch( RANDOM_LONG( 0, 2 ) )
			{
			case 0: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/hks1.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			case 1: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/hks2.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			case 2: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/hks3.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			}
			localanim_NextPAttackTime = tr.time + 0.1;
			R_MakeWeaponShake( WEAPON_MRC, 0, true );
			PlayLowAmmoSound( player->index, player->origin, (CurrentPAmmoClip <= 15 ? CurrentPAmmoClip : 0) );
		}
		if( Attack2 && !Underwater )
		{
			gEngfuncs.pfnWeaponAnim( 2, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/glauncher.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 1.0;
			R_MakeWeaponShake( WEAPON_MRC, 1, true );
		}
		break;
	case WEAPON_AR2:
		if( Attack1 && !Underwater )
		{
			gEngfuncs.pfnWeaponAnim( 3, 0 );
			switch( RANDOM_LONG( 0, 3 ) )
			{
			case 0: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "drone/aliendrone_shoot1.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			case 1: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "drone/aliendrone_shoot2.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			case 2: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "drone/aliendrone_shoot3.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			case 3: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "drone/aliendrone_shoot4.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			}
			localanim_NextPAttackTime = tr.time + 0.1;
			R_MakeWeaponShake( WEAPON_AR2, 0, true );
		}
		if( Attack2 && !Underwater )
		{
			gEngfuncs.pfnWeaponAnim( 1, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/ar2_secondary.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 1.0;
			R_MakeWeaponShake( WEAPON_AR2, 1, true );
		}
		break;
	case WEAPON_GLOCK:
		if( Attack1 || Attack2 )
		{
			gEngfuncs.pfnWeaponAnim( 3, 0 );
			switch( RANDOM_LONG( 0, 2 ) )
			{
			case 0: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/pistol1.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			case 1: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/pistol2.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			case 2: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/pistol3.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			}
			if( Attack1 )
				localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 0.3;
			else
				localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 0.2;
		}
		break;
	case WEAPON_CROSSBOW:
		if( Attack1 )
		{
			if( CurrentPAmmoClip == 1 ) // last bolt
				gEngfuncs.pfnWeaponAnim( 6, 0 );
			else
				gEngfuncs.pfnWeaponAnim( 4, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/xbow_fire1.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_NextPAttackTime = tr.time + 0.75;
		}
		break;
	case WEAPON_DEAGLE:
		if( Attack1 && !Underwater )
		{
			if( CurrentPAmmoClip == 1 ) // last bullet
				gEngfuncs.pfnWeaponAnim( 7, 0 );
			else
				gEngfuncs.pfnWeaponAnim( 2, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/deagle_shot1.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_NextPAttackTime = tr.time + 0.75;
			R_MakeWeaponShake( WEAPON_DEAGLE, 0, true );
		}
		break;
	case WEAPON_FIVESEVEN:
		if( Attack1 && !Underwater )
		{
			if( CurrentPAmmoClip == 1 ) // last bullet
				gEngfuncs.pfnWeaponAnim( 3, 0 );
			else
				gEngfuncs.pfnWeaponAnim( 1, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/fiveseven_shoot.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_NextPAttackTime = tr.time + 0.2;
		}
		break;
	case WEAPON_G36C:
		if( Attack1 && !Underwater )
		{
			gEngfuncs.pfnWeaponAnim( 3, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/g36c_shoot.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_NextPAttackTime = tr.time + 0.1;
			R_MakeWeaponShake( WEAPON_G36C, 0, true );
			PlayLowAmmoSound( player->index, player->origin, (CurrentPAmmoClip <= 10 ? CurrentPAmmoClip : 0) );
		}
		break;
	case WEAPON_KNIFE:
		if( Attack1 )
		{
			gEngfuncs.pfnWeaponAnim( 3, 0 );
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 0.5;
		}
		if( Attack2 )
		{
			gEngfuncs.pfnWeaponAnim( 6, 0 );
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 1.25;
		}
		break;
	case WEAPON_RPG:
		if( Attack1 && localanim_AllowRpgShoot )
		{
			gEngfuncs.pfnWeaponAnim( 3, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/rocketfire1.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_AllowRpgShoot = false; // do not allow any more animations, until we catch reload or deploy
		}
		break;
	case WEAPON_SHOTGUN:
		if( Attack1 && !Underwater )
		{
			gEngfuncs.pfnWeaponAnim( 1, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/shotgun_single.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 1.2;
			R_MakeWeaponShake( WEAPON_SHOTGUN, 0, true );
		}
		if( Attack2 && !Underwater )
		{
			if( CurrentPAmmoClip > 1 )
			{
				gEngfuncs.pfnWeaponAnim( 2, 0 );
				gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/shotgun_dbl.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
				R_MakeWeaponShake( WEAPON_SHOTGUN, 1, true );
			}
			else
			{
				gEngfuncs.pfnWeaponAnim( 1, 0 );
				gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/shotgun_single.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
				R_MakeWeaponShake( WEAPON_SHOTGUN, 0, true );
			}
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 1.6;
		}
		break;
	case WEAPON_SHOTGUN_XM:
		if( Attack1 && !Underwater )
		{
			gEngfuncs.pfnWeaponAnim( 1, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/xm_fire.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_NextPAttackTime = tr.time + 0.5;
			R_MakeWeaponShake( WEAPON_SHOTGUN_XM, 0, true );
		}
		break;
	case WEAPON_SNIPER:
		if( Attack1 && !Underwater )
		{
			gEngfuncs.pfnWeaponAnim( 2, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/sniper_wpn_fire.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_NextPAttackTime = tr.time + 1.0;
			R_MakeWeaponShake( WEAPON_SNIPER, 0, true );
		}
		break;
	}
}