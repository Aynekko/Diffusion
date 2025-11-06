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
//=========================================================
// skill.h - skill level concerns
//=========================================================
 
// values are picked using g_iSkillLevel, so the first value is always zero
 
// monster_gargantua --------------------------------------
const int g_RoboHealth[] =
{
	0,
	1600,
	1800,
	2200
};

const int g_RoboDmgSlash[] =
{
	0,
	25,
	30,
	35
};

const float g_RoboBulletDmg[] =
{
	0,
	4.0f,
	4.5f,
	5.0f
};

const int g_RoboRocketDamage[] =
{
	0,
	30,
	32,
	35,
};

// monster_security_heavydrone ----------------------------
const int g_DroneRocketDamage[] =
{
	0,
	20,
	23,
	26
};

// monster_apache -----------------------------------------
const int g_ApacheHealth[] =
{
	0,
	350,
	400,
	500
};

// turrets ------------------------------------------------
const int g_turretHealth[] =
{
	0,
	50,
	55,
	60
};

const int g_miniturretHealth[] =
{
	0,
	40,
	45,
	50
};

const int g_sentryHealth[] =
{
	0,
	60,
	65,
	70
};

// scientist (male/female) --------------------------------
const int g_scientistHealth = 25;
const int g_scientistHeal = 20;

// dweller ------------------------------------------------
const int g_dwellerZapDamage = 25;
const int g_dwellerClawDamage = 10;
const int g_dwellerClawRakeDamage = 25;

// leech (unused really) ----------------------------------
const int g_leechHealth = 2;
const int g_leechDmgBite = 2;

// HL controller monster ----------------------------------
const int g_controllerHealth = 75;
const int g_controllerDmgZap = 20;
const int g_controllerSpeedBall = 666;
const int g_controllerDmgBall = 5;

// soldiers / health---------------------------------------
const int g_ArmyGuyHealth[] =
{
	0,
	85,
	90,
	95
};

const int g_AlienRoboHealth[] =
{
	0,
	92,
	97,
	105
};

const int g_SecurityGuyHealth[] =
{
	0,
	95,
	100,
	110
};

const int g_SecurityGeneralHealth[] =
{
	0,
	500,
	666,
	800
};

const int g_AndrewHealth[] =
{
	0,
	2000,
	2500,
	3000
};

// soldiers / damage --------------------------------------
const int g_GruntShotgunDamage[] =
{
	0,
	6,
	8,
	10
};

const int g_GruntKickDamage = 10;
const int g_GruntGrenadeSpeed = 500;

// soldiers / reaction times ------------------------------
const float g_HGruntCombatWaitTime[] =
{
	0.0f,
	0.8f,
	0.5f,
	0.3f
};

const float g_AlienCombatWaitTime[] =
{
	0.0f,
	0.6f,
	0.4f,
	0.2f
};

const float g_SecurityGruntCombatWaitTime[] =
{
	0.0f,
	0.7f,
	0.5f,
	0.2f
};

const float g_SecurityGeneralCombatWaitTime[] =
{
	0.0f,
	0.4f,
	0.25f,
	0.1f
};

const float g_AndrewCombatWaitTime[] =
{
	0.0f,
	0.5f,
	0.25f,
	0.1f
};

float GetSkillCvar( char *pName );

extern DLL_GLOBAL int g_iSkillLevel;

#define SKILL_EASY		1
#define SKILL_MEDIUM	2
#define SKILL_HARD		3
