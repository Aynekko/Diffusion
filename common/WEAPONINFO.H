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

#ifndef WEAPONINFO_H
#define WEAPONINFO_H

// must match weapons.h on server
#define WEAPON_NONE				0
#define WEAPON_KNIFE			1
#define WEAPON_GLOCK			2
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

#define TOTAL_WEAPONS 24

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

#endif//WEAPONINFO_H