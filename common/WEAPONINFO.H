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

//---------------------------weaponinfo.h--------------------------------

#ifndef WEAPONINFO_H
#define WEAPONINFO_H

#define MAX_WEAPONS				64
#define MAX_WEAPON_BYTES		(( MAX_WEAPONS + 7 ) / 8 )
#define WEAPON_SUIT				(MAX_WEAPONS - 1)

#define MAX_WEAPON_SLOTS		6	// hud item selection buckets
#define MAX_WEAPON_POSITIONS	7 // this is the max number of items in each bucket

#define MAX_ITEM_TYPES			MAX_WEAPON_POSITIONS	// hud item selection slots
#define MAX_ITEMS				5	// hard coded item types

#define MAX_AMMO_TYPES			32
#define MAX_AMMO_SLOTS  		32

#define TRACER_FREQ				0//4			// Tracers fire every fourth bullet

// Info about weapons player might have in his/her possession
typedef struct weapon_data_s
{
	int		m_iId;
	int		m_iClip;

	float		m_flNextPrimaryAttack;
	float		m_flNextSecondaryAttack;
	float		m_flTimeWeaponIdle;

	int		m_fInReload;
	int		m_fInSpecialReload;
	float		m_flNextReload;
	float		m_flPumpTime;
	float		m_fReloadTime;

	float		m_fAimedDamage;
	float		m_fNextAimBonus;
	int		m_fInZoom;
	int		m_iWeaponState;

	int		iuser1;
	int		iuser2;
	int		iuser3;
	int		iuser4;
	float		fuser1;
	float		fuser2;
	float		fuser3;
	float		fuser4;
} weapon_data_t;

// constant items
#define ITEM_HEALTHKIT		1
#define ITEM_ANTIDOTE		2
#define ITEM_SECURITY		3
#define ITEM_BATTERY		4

#define WEAPON_NONE				0
#define WEAPON_KNIFE			1
#define WEAPON_BERETTA			2
#define WEAPON_DEAGLE			3
#define WEAPON_MRC				4
#define WEAPON_CYCLER			5
#define WEAPON_CROSSBOW			6
#define WEAPON_SHOTGUN			7
#define WEAPON_RPG				8
#define WEAPON_GAUSS			9
#define WEAPON_EGON				10
#define WEAPON_HORNETGUN		11
#define WEAPON_HANDGRENADE		12
#define WEAPON_TRIPMINE			13
#define WEAPON_SATCHEL			14
#define WEAPON_SNARK			15
#define WEAPON_AR2				16
#define WEAPON_DRONE			17
#define WEAPON_SENTRY			18
#define WEAPON_HKMP5			19
#define WEAPON_FIVESEVEN		20
#define WEAPON_SNIPER			21
#define WEAPON_SHOTGUN_XM		22
#define WEAPON_G36C				23
#define WEAPON_SMOKEGRENADE		24

#define TOTAL_WEAPONS 25

#define WEAPON_ALLWEAPONS		(~(1<<WEAPON_SUIT))

//====================================================================
// weapon menu positions
//====================================================================
// slot 0 -----------------------------------
#define WPN_SLOT_KNIFE			0

#define WPN_POS_KNIFE			0
// slot 1 -----------------------------------
#define WPN_SLOT_BERETTA			1
#define WPN_SLOT_DEAGLE			1
#define WPN_SLOT_57				1

#define WPN_POS_BERETTA			0
#define WPN_POS_DEAGLE			1
#define WPN_POS_57				2
// slot 2 -----------------------------------
#define WPN_SLOT_AR2			2
#define WPN_SLOT_MRC			2
#define WPN_SLOT_G36C			2
#define WPN_SLOT_HKMP5			2
#define WPN_SLOT_SHOTGUN		2
#define WPN_SLOT_SHOTGUN_XM		2

#define WPN_POS_MRC				0
#define WPN_POS_HKMP5			1
#define WPN_POS_SHOTGUN_XM		2
#define WPN_POS_SHOTGUN			4
#define WPN_POS_AR2				5
#define WPN_POS_G36C			6
// slot 3 -----------------------------------
#define WPN_SLOT_GAUSS			3
#define WPN_SLOT_RPG			3
#define WPN_SLOT_SNIPER			3
#define WPN_SLOT_CROSSBOW		3

#define WPN_POS_RPG				0
#define WPN_POS_GAUSS			1
#define WPN_POS_SNIPER			2
#define WPN_POS_CROSSBOW		3
// slot 4 -----------------------------------
#define WPN_SLOT_TRIPMINE		4
#define WPN_SLOT_HANDGRENADE	4
#define WPN_SLOT_SATCHEL		4
#define WPN_SLOT_SMOKEGRENADE	4

#define WPN_POS_HANDGRENADE		0
#define WPN_POS_SMOKEGRENADE	1
#define WPN_POS_SATCHEL			2
#define WPN_POS_TRIPMINE		3
// slot 5 -----------------------------------
#define WPN_SLOT_DRONE			5
#define WPN_SLOT_SENTRY			5

#define WPN_POS_DRONE			0
#define WPN_POS_SENTRY			1

//====================================================================
// weapon animations
//====================================================================
enum tripmine_e
{
	TRIPMINE_IDLE1 = 0,
	TRIPMINE_IDLE2,
	TRIPMINE_ARM1,
	TRIPMINE_ARM2,
	TRIPMINE_FIDGET,
	TRIPMINE_HOLSTER,
	TRIPMINE_DRAW,
	TRIPMINE_WORLD,
	TRIPMINE_GROUND,
};

enum ar2_e
{
	AR2_IDLE,
	AR2_LAUNCH,
	AR2_DEPLOY,
	AR2_FIRE,
	AR2_HOLSTER,
};

enum mrc_e
{
	MRC_LONGIDLE = 0,
	MRC_IDLE1,
	MRC_LAUNCH,
	MRC_RELOAD,
	MRC_DEPLOY,
	MRC_FIRE1,
	MRC_FIRE2,
	MRC_FIRE3,
};

enum crossbow_e
{
	CROSSBOW_IDLE1 = 0,	// full
	CROSSBOW_IDLE2,	// empty
	CROSSBOW_FIDGET1,	// full
	CROSSBOW_FIDGET2,	// empty
	CROSSBOW_FIRE1,	// full
	CROSSBOW_FIRE2,	// reload
	CROSSBOW_FIRE3,	// empty
	CROSSBOW_RELOAD,	// from empty
	CROSSBOW_DRAW1,	// full
	CROSSBOW_DRAW2,	// empty
	CROSSBOW_HOLSTER1,	// full
	CROSSBOW_HOLSTER2,	// empty
};

enum wpndrone_e
{
	DRONE_IDLE = 0,
	DRONE_DEPLOY,
	DRONE_DEPLOYSLOW,
	DRONE_NODRONE,
};

enum wpndroneradio_e
{
	DRONERADIO_IDLE = 0,
	DRONERADIO_DEPLOY,
	DRONERADIO_CALL,
	DRONERADIO_HOLSTER,
};

enum deagle_e
{
	DEAGLE_IDLE1 = 0,
	DEAGLE_FIDGET,
	DEAGLE_FIRE1,
	DEAGLE_RELOAD_EMPTY,
	DEAGLE_RELOAD,
	DEAGLE_HOLSTER,
	DEAGLE_DRAW,
	DEAGLE_FIRE_EMPTY,
};

enum WpnFiveSeven_e
{
	WPN57_IDLE = 0,
	WPN57_SHOOT,
	WPN57_SHOOT2,
	WPN57_SHOOT_EMPTY,
	WPN57_RELOAD,
	WPN57_RELOAD_EMPTY,
	WPN57_DEPLOY,
};

enum WeaponG36C_e
{
	G36C_IDLE = 0,
	G36C_RELOAD,
	G36C_DRAW,
	G36C_SHOOT,
	G36C_RELOAD_EMPTY,
};

enum gauss_e
{
	GAUSS_IDLE = 0,
	GAUSS_IDLE2,
	GAUSS_FIDGET,
	GAUSS_SPINUP,
	GAUSS_SPIN,
	GAUSS_FIRE,
	GAUSS_FIRE2,
	GAUSS_HOLSTER,
	GAUSS_DRAW
};

enum handgrenade_e // smokegrenade uses this too
{
	HANDGRENADE_IDLE = 0,
	HANDGRENADE_FIDGET,
	HANDGRENADE_PINPULL,
	HANDGRENADE_THROW1,	// toss
	HANDGRENADE_THROW2,	// medium
	HANDGRENADE_THROW3,	// hard
	HANDGRENADE_HOLSTER,
	HANDGRENADE_DRAW,
};

enum WeaponHKMP5_e
{
	HKMP5_IDLE = 0,
	HKMP5_RELOAD_EMPTY,
	HKMP5_RELOAD,
	HKMP5_DRAW,
	HKMP5_FIRE1,
	HKMP5_FIRE2,
	HKMP5_FIRE3,
};


enum knife_e
{
	KNIFE_IDLE = 0,
	KNIFE_DRAW,
	KNIFE_HOLSTER,
	KNIFE_ATTACK1HIT,
	KNIFE_ATTACK1MISS,
	KNIFE_ATTACK2MISS,
	KNIFE_ATTACK2HIT,
	KNIFE_ATTACK3MISS,
	KNIFE_ATTACK3HIT
};

enum rpg_e
{
	RPG_IDLE = 0,
	RPG_FIDGET,
	RPG_RELOAD,	// to reload
	RPG_FIRE,	// to empty
	RPG_HOLSTER1,	// loaded
	RPG_DRAW1,	// loaded
	RPG_HOLSTER2,	// unloaded
	RPG_DRAW_UL,	// unloaded
	RPG_IDLE_UL,	// unloaded idle
	RPG_FIDGET_UL,	// unloaded fidget
};

enum satchel_e
{
	SATCHEL_IDLE1 = 0,
	SATCHEL_FIDGET1,
	SATCHEL_DRAW,
	SATCHEL_DROP
};

enum satchel_radio_e
{
	SATCHEL_RADIO_IDLE1 = 0,
	SATCHEL_RADIO_FIDGET1,
	SATCHEL_RADIO_DRAW,
	SATCHEL_RADIO_FIRE,
	SATCHEL_RADIO_HOLSTER
};

enum wpnsentry_e
{
	SENTRY_IDLE = 0,
	SENTRY_DEPLOY,
	SENTRY_DEPLOYSLOW,
	SENTRY_NOSENTRY,
};

enum shotgun_e
{
	SHOTGUN_IDLE = 0,
	SHOTGUN_FIRE,
	SHOTGUN_FIRE2,
	SHOTGUN_RELOAD,
	SHOTGUN_END_RELOAD,
	SHOTGUN_START_RELOAD,
	SHOTGUN_DRAW,
	SHOTGUN_END_RELOAD_EMPTY,
	SHOTGUN_IDLE4,
	SHOTGUN_IDLE_DEEP
};

enum shotgunxm_e
{
	SHOTGUNXM_IDLE = 0,
	SHOTGUNXM_FIRE,
	SHOTGUNXM_FIRE2,
	SHOTGUNXM_RELOAD,
	SHOTGUNXM_END_RELOAD,
	SHOTGUNXM_START_RELOAD,
	SHOTGUNXM_DRAW,
	SHOTGUNXM_END_RELOAD_EMPTY,
	SHOTGUNXM_DRAW_AFTER_RELOAD
};

enum sniper_e
{
	SNIPER_DRAW = 0,
	SNIPER_IDLE,
	SNIPER_SHOOT,
	SNIPER_RELOAD_EMPTY,
	SNIPER_RELOAD,
};

enum beretta_e
{
	BERETTA_IDLE1 = 0,
	BERETTA_IDLE2,
	BERETTA_IDLE3,
	BERETTA_SHOOT,
	BERETTA_SHOOT_EMPTY,
	BERETTA_RELOAD,
	BERETTA_RELOAD_NOT_EMPTY,
	BERETTA_DRAW,
	BERETTA_HOLSTER,
	BERETTA_ADD_SILENCER,
	BERETTA_SHOOT_SILENCER,
	BERETTA_SHOOT_EMPTY_SILENCER,
};

//====================================================================
// weapon timings
//====================================================================
#define DEFAULT_DEPLOY_TIME		0.5

#define AR2_DEPLOY_TIME			0.5
#define AR2_NEXT_PA_TIME		0.1
#define AR2_NEXT_SA_TIME		1.0

#define MRC_DEPLOY_TIME			1.0
#define MRC_NEXT_PA_TIME		0.1
#define MRC_NEXT_SA_TIME		1.0
#define MRC_RELOAD_TIME			2.0

#define CROSSBOW_NEXT_PA_TIME	0.75
#define CROSSBOW_RELOAD_TIME	4.5

#define DEAGLE_DEPLOY_TIME		1.0
#define DEAGLE_NEXT_PA_TIME		0.75
#define DEAGLE_RELOAD_TIME		2.6
#define DEAGLE_RELOADEMPTY_TIME	3.5

#define DRONE_DEPLOY_TIME		1.5

#define WPN57_DEPLOY_TIME		1.0
#define WPN57_NEXT_PA_TIME		0.2
#define WPN57_RELOAD_TIME		2.5
#define WPN57_RELOADEMPTY_TIME	2.8

#define G36C_DEPLOY_TIME		1.3
#define G36C_NEXT_PA_TIME		0.1
#define G36C_RELOAD_TIME		3.0
#define G36C_RELOADEMPTY_TIME	4.8

#define HKMP5_DEPLOY_TIME		1.0
#define HKMP5_NEXT_PA_TIME		0.1
#define HKMP5_RELOAD_TIME		2.6
#define HKMP5_RELOADEMPTY_TIME	3.5

#define KNIFE_DEPLOY_TIME		1.1
#define KNIFE_NEXT_PA_TIME		0.5
#define KNIFE_NEXT_SA_TIME		1.25

#define BERETTA_DEPLOY_TIME		1.0
#define BERETTA_NEXT_PA_TIME	0.3
#define BERETTA_NEXT_SA_TIME	0.2
#define BERETTA_RELOAD_TIME		1.5

#define RPG_RELOAD_TIME			2.1

#define SENTRY_DEPLOY_TIME		2.0

#define SHOTGUN_DEPLOY_TIME		0.6
#define SHOTGUN_NEXT_PA_TIME	1.2
#define SHOTGUN_NEXT_SA_TIME	1.6
#define SHOTGUN_STARTRELOAD_TIME 0.6
#define SHOTGUN_RELOAD_TIME		0.7
#define SHOTGUN_RELOAD_FINISH_TIME 0.7
#define SHOTGUN_RELOADEMPTY_FINISH_TIME 1.0

#define SHOTGUNXM_DEPLOY_TIME	0.6
#define SHOTGUNXM_NEXT_PA_TIME	0.5
#define SHOTGUNXM_STARTRELOAD_TIME 0.4
#define SHOTGUNXM_RELOAD_TIME	0.6
#define SHOTGUNXM_RELOAD_FINISH_TIME 0.8
#define SHOTGUNXM_RELOADEMPTY_FINISH_TIME 1.5
#define SHOTGUNXM_DRAWRELOADEMPTY_FINISH_TIME 1.8

#define SNIPER_DEPLOY_TIME		1.0
#define SNIPER_NEXT_PA_TIME		1.0
#define SNIPER_RELOAD_TIME		4.5
#define SNIPER_RELOADEMPTY_TIME 5.5

//====================================================================
// shell models (body)
//====================================================================
enum gun_shells
{
	SHELL_9MM = 0,
	SHELL_57,
	SHELL_50AE,
	SHELL_556,
	SHELL_50BMG,
	SHELL_12GAUGE,
};

#endif//WEAPONINFO_H