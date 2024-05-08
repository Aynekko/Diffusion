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
		if( seq == HKMP5_RELOAD ) // caught reload anim!
			localanim_NextPAttackTime = tr.time + HKMP5_RELOAD_TIME;
		else if( seq == HKMP5_RELOAD_EMPTY ) // caught reload_empty anim!
			localanim_NextPAttackTime = tr.time + HKMP5_RELOADEMPTY_TIME;
		else if( seq == HKMP5_DRAW ) // caught deploy anim!
			localanim_NextPAttackTime = tr.time + HKMP5_DEPLOY_TIME;

		if( seq < HKMP5_FIRE1 ) // only interested in FIRE animations
			return false;
		else
			return true;
		break;
	case WEAPON_MRC:
		if( seq == MRC_RELOAD ) // caught reload anim!
			localanim_NextPAttackTime = tr.time + MRC_RELOAD_TIME;
		else if( seq == MRC_DEPLOY ) // caught deploy anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + MRC_DEPLOY_TIME;

		if( seq < MRC_FIRE1 && seq != MRC_LAUNCH ) // only interested in FIRE animations
			return false;
		else
			return true;
		break;
	case WEAPON_BERETTA:
		if( seq == BERETTA_RELOAD )
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + BERETTA_RELOADEMPTY_TIME;
		else if( seq == BERETTA_RELOAD_NOT_EMPTY ) // caught reload anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + BERETTA_RELOAD_TIME;
		else if( seq == BERETTA_DRAW ) // caught deploy anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + BERETTA_DEPLOY_TIME;

		if( seq != BERETTA_SHOOT && seq != BERETTA_SHOOT_EMPTY ) // only interested in FIRE animations
			return false;
		else
			return true;
		break;
	case WEAPON_AR2:
		if( seq == AR2_DEPLOY ) // caught deploy anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + AR2_DEPLOY_TIME;

		if( seq != AR2_LAUNCH && seq != AR2_FIRE ) // only interested in FIRE animations
			return false;
		else
			return true;
		break;
	case WEAPON_CROSSBOW:
		if( seq == CROSSBOW_RELOAD ) // caught reload anim!
			localanim_NextPAttackTime = tr.time + CROSSBOW_RELOAD_TIME;
		else if( seq == CROSSBOW_DRAW1 || seq == CROSSBOW_DRAW2 ) // caught deploy anim!
			localanim_NextPAttackTime = tr.time + DEFAULT_DEPLOY_TIME;

		if( seq != CROSSBOW_FIRE1 && seq != CROSSBOW_FIRE3 ) // only interested in FIRE animations
			return false;
		else
			return true;
		break;
	case WEAPON_DEAGLE:
		if( seq == DEAGLE_RELOAD_EMPTY ) // caught reload_empty anim!
			localanim_NextPAttackTime = tr.time + DEAGLE_RELOADEMPTY_TIME;
		else if( seq == DEAGLE_RELOAD ) // caught reload anim!
			localanim_NextPAttackTime = tr.time + DEAGLE_RELOAD_TIME;
		else if( seq == DEAGLE_DRAW ) // caught deploy anim!
			localanim_NextPAttackTime = tr.time + DEAGLE_DEPLOY_TIME;

		if( seq != DEAGLE_FIRE1 && seq != DEAGLE_FIRE_EMPTY ) // only interested in FIRE animations
			return false;
		else
			return true;
		break;
	case WEAPON_FIVESEVEN:
		if( seq == WPN57_RELOAD ) // caught reload anim!
			localanim_NextPAttackTime = tr.time + WPN57_RELOAD_TIME;
		else if( seq == WPN57_RELOAD_EMPTY ) // caught reload_empty anim!
			localanim_NextPAttackTime = tr.time + WPN57_RELOADEMPTY_TIME;
		else if( seq == WPN57_DEPLOY ) // caught deploy anim!
			localanim_NextPAttackTime = tr.time + WPN57_DEPLOY_TIME;

		if( seq != WPN57_SHOOT && seq != WPN57_SHOOT_EMPTY ) // only interested in FIRE animations
			return false;
		else
			return true;
		break;
	case WEAPON_G36C:
		if( seq == G36C_RELOAD ) // caught reload anim!
			localanim_NextPAttackTime = tr.time + G36C_RELOAD_TIME;
		else if( seq == G36C_DRAW ) // caught deploy anim!
			localanim_NextPAttackTime = tr.time + G36C_DEPLOY_TIME;
		else if( seq == G36C_RELOAD_EMPTY ) // caught reload empty anim!
			localanim_NextPAttackTime = tr.time + G36C_RELOADEMPTY_TIME;

		if( seq != G36C_SHOOT ) // only interested in FIRE animations
			return false;
		else
			return true;
		break;
	case WEAPON_KNIFE:
		if( seq == KNIFE_DRAW ) // caught deploy anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + KNIFE_DEPLOY_TIME;

		if( seq != KNIFE_ATTACK1HIT && seq != KNIFE_ATTACK2HIT ) // only interested in FIRE animations
			return false;
		else
			return true;
		break;
	case WEAPON_RPG:
		if( seq == RPG_RELOAD ) // caught reload anim!
		{
			localanim_NextPAttackTime = tr.time + RPG_RELOAD_TIME;
			localanim_AllowRpgShoot = true;
		}
		else if( seq == RPG_DRAW1 || seq == RPG_DRAW_UL ) // caught deploy anim!
		{
			localanim_NextPAttackTime = tr.time + DEFAULT_DEPLOY_TIME;
			localanim_AllowRpgShoot = true;
		}

		if( seq != RPG_FIRE ) // only interested in FIRE animations
			return false;
		else
			return true;
		break;
	case WEAPON_SHOTGUN:
		if( seq == SHOTGUN_RELOAD ) // caught reload anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + SHOTGUN_RELOAD_TIME + 1.0f; // !!! slightly pushed forward so we don't have an accidental shot between animations
		else if( seq == SHOTGUN_START_RELOAD ) // caught reload anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + SHOTGUN_STARTRELOAD_TIME + 1.0f; // !!! slightly pushed forward so we don't have an accidental shot between animations
		else if( seq == SHOTGUN_DRAW ) // caught deploy anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + SHOTGUN_DEPLOY_TIME;
		else if( seq == SHOTGUN_END_RELOAD ) // caught finish reload anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + SHOTGUN_RELOAD_FINISH_TIME;
		else if( seq == SHOTGUN_END_RELOAD_EMPTY ) // caught finish reload anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + SHOTGUN_RELOADEMPTY_FINISH_TIME;

		if( seq != SHOTGUN_FIRE && seq != SHOTGUN_FIRE2 ) // only interested in FIRE animations
			return false;
		else
			return true;
		break;
	case WEAPON_SHOTGUN_XM:
		if( seq == SHOTGUNXM_RELOAD ) // caught reload anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + SHOTGUNXM_RELOAD_TIME + 1.0f; // !!! slightly pushed forward so we don't have an accidental shot between animations
		else if( seq == SHOTGUN_START_RELOAD )
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + SHOTGUNXM_STARTRELOAD_TIME + 1.0f; // !!! slightly pushed forward so we don't have an accidental shot between animations
		else if( seq == SHOTGUNXM_DRAW ) // caught deploy anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + SHOTGUNXM_DEPLOY_TIME;
		else if( seq == SHOTGUNXM_END_RELOAD ) // caught finish reload anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + SHOTGUNXM_RELOAD_FINISH_TIME;
		else if( seq == SHOTGUNXM_END_RELOAD_EMPTY )  // caught finish reload anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + SHOTGUNXM_RELOADEMPTY_FINISH_TIME;
		else if( seq == SHOTGUNXM_DRAW_AFTER_RELOAD )  // caught deploy/finish reload anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + SHOTGUNXM_DRAWRELOADEMPTY_FINISH_TIME;

		if( seq != SHOTGUNXM_FIRE ) // only interested in FIRE animations
			return false;
		else
			return true;
		break;
	case WEAPON_SNIPER:
		if( seq == SNIPER_RELOAD_EMPTY ) // caught reload_empty anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + SNIPER_RELOADEMPTY_TIME;
		else if( seq == SNIPER_RELOAD ) // caught reload anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + SNIPER_RELOAD_TIME;
		else if( seq == SNIPER_DRAW ) // caught deploy anim!
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + SNIPER_DEPLOY_TIME;

		if( seq != SNIPER_SHOOT ) // only interested in FIRE animations
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
			WeaponAnim( RANDOM_LONG( HKMP5_FIRE1, HKMP5_FIRE3 ), 0 );
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
			WeaponAnim( RANDOM_LONG( MRC_FIRE1, MRC_FIRE3 ), 0 );
			switch( RANDOM_LONG( 0, 2 ) )
			{
			case 0: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/hks1.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			case 1: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/hks2.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			case 2: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/hks3.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			}
			localanim_NextPAttackTime = tr.time + MRC_NEXT_PA_TIME;
			R_MakeWeaponShake( WEAPON_MRC, 0, true );
			PlayLowAmmoSound( player->index, player->origin, (CurrentPAmmoClip <= 15 ? CurrentPAmmoClip : 0) );
		}
		if( Attack2 && !Underwater )
		{
			WeaponAnim( MRC_LAUNCH, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/glauncher.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + MRC_NEXT_SA_TIME;
			R_MakeWeaponShake( WEAPON_MRC, 1, true );
		}
		break;
	case WEAPON_AR2:
		if( Attack1 && !Underwater )
		{
			WeaponAnim( AR2_FIRE, 0 );
			switch( RANDOM_LONG( 0, 3 ) )
			{
			case 0: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "drone/aliendrone_shoot1.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			case 1: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "drone/aliendrone_shoot2.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			case 2: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "drone/aliendrone_shoot3.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			case 3: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "drone/aliendrone_shoot4.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			}
			localanim_NextPAttackTime = tr.time + AR2_NEXT_PA_TIME;
			R_MakeWeaponShake( WEAPON_AR2, 0, true );
		}
		if( Attack2 && !Underwater )
		{
			WeaponAnim( AR2_LAUNCH, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/ar2_secondary.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + AR2_NEXT_SA_TIME;
			R_MakeWeaponShake( WEAPON_AR2, 1, true );
		}
		break;
	case WEAPON_BERETTA:
		if( Attack1 || Attack2 )
		{
			if( CurrentPAmmoClip == 1 ) // last bullet
				WeaponAnim( BERETTA_SHOOT_EMPTY, 0 );
			else
				WeaponAnim( BERETTA_SHOOT, 0 );
			switch( RANDOM_LONG( 0, 2 ) )
			{
			case 0: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/pistol1.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			case 1: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/pistol2.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			case 2: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/pistol3.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			}
			if( Attack1 )
				localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + BERETTA_NEXT_PA_TIME;
			else
				localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + BERETTA_NEXT_SA_TIME;
		}
		break;
	case WEAPON_CROSSBOW:
		if( Attack1 )
		{
			if( CurrentPAmmoClip == 1 ) // last bolt
				WeaponAnim( CROSSBOW_FIRE3, 0 );
			else
				WeaponAnim( CROSSBOW_FIRE1, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/xbow_fire1.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_NextPAttackTime = tr.time + CROSSBOW_NEXT_PA_TIME;
		}
		break;
	case WEAPON_DEAGLE:
		if( Attack1 && !Underwater )
		{
			if( CurrentPAmmoClip == 1 ) // last bullet
				WeaponAnim( DEAGLE_FIRE_EMPTY, 0 );
			else
				WeaponAnim( DEAGLE_FIRE1, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/deagle_shot1.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_NextPAttackTime = tr.time + DEAGLE_NEXT_PA_TIME;
			R_MakeWeaponShake( WEAPON_DEAGLE, 0, true );
		}
		break;
	case WEAPON_FIVESEVEN:
		if( Attack1 && !Underwater )
		{
			if( CurrentPAmmoClip == 1 ) // last bullet
				WeaponAnim( WPN57_SHOOT_EMPTY, 0 );
			else
				WeaponAnim( WPN57_SHOOT, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/fiveseven_shoot.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_NextPAttackTime = tr.time + WPN57_NEXT_PA_TIME;
		}
		break;
	case WEAPON_G36C:
		if( Attack1 && !Underwater )
		{
			WeaponAnim( G36C_SHOOT, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/g36c_shoot.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_NextPAttackTime = tr.time + G36C_NEXT_PA_TIME;
			R_MakeWeaponShake( WEAPON_G36C, 0, true );
			PlayLowAmmoSound( player->index, player->origin, (CurrentPAmmoClip <= 10 ? CurrentPAmmoClip : 0) );
		}
		break;
	case WEAPON_KNIFE:
		if( Attack1 )
		{
			WeaponAnim( KNIFE_ATTACK1HIT, 0 );
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + KNIFE_NEXT_PA_TIME;
		}
		if( Attack2 )
		{
			WeaponAnim( KNIFE_ATTACK2HIT, 0 );
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + KNIFE_NEXT_SA_TIME;
		}
		break;
	case WEAPON_RPG:
		if( Attack1 && localanim_AllowRpgShoot )
		{
			WeaponAnim( RPG_FIRE, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/rocketfire1.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_AllowRpgShoot = false; // do not allow any more animations, until we catch reload or deploy
		}
		break;
	case WEAPON_SHOTGUN:
		if( Attack1 && !Underwater )
		{
			WeaponAnim( SHOTGUN_FIRE, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/shotgun_single.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + SHOTGUN_NEXT_PA_TIME;
			R_MakeWeaponShake( WEAPON_SHOTGUN, 0, true );
		}
		if( Attack2 && !Underwater )
		{
			if( CurrentPAmmoClip > 1 )
			{
				WeaponAnim( SHOTGUN_FIRE2, 0 );
				gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/shotgun_dbl.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
				R_MakeWeaponShake( WEAPON_SHOTGUN, 1, true );
			}
			else
			{
				WeaponAnim( SHOTGUN_FIRE, 0 );
				gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/shotgun_single.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
				R_MakeWeaponShake( WEAPON_SHOTGUN, 0, true );
			}
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + SHOTGUN_NEXT_SA_TIME;
		}
		break;
	case WEAPON_SHOTGUN_XM:
		if( Attack1 && !Underwater )
		{
			WeaponAnim( SHOTGUNXM_FIRE, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/xm_fire.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_NextPAttackTime = tr.time + SHOTGUNXM_NEXT_PA_TIME;
			R_MakeWeaponShake( WEAPON_SHOTGUN_XM, 0, true );
		}
		break;
	case WEAPON_SNIPER:
		if( Attack1 && !Underwater )
		{
			WeaponAnim( SNIPER_SHOOT, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/sniper_wpn_fire.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_NextPAttackTime = tr.time + SNIPER_NEXT_PA_TIME;
			R_MakeWeaponShake( WEAPON_SNIPER, 0, true );
		}
		break;
	}
}