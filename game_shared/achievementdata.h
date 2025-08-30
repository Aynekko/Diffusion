#pragma once

// ========================================================================================
// diffusion - achievement data
// ========================================================================================
enum Achievements_e
{
	ACH_BULLETSFIRED = 0,	// 0 fire # bullets
	ACH_JUMPS,				// 1 jump # times
	ACH_AMMOCRATES,			// 2 find # ammo crates
	ACH_DISARMEDMINES,		// 3 disarm # enemy mines (disarming your own doesn't count)
	ACH_KILLENEMIES,		// 4 kill # enemies
	ACH_INFLICTDAMAGE,		// 5 inflict a total of # damage
	ACH_KILLENEMIESSNIPER,	// 6 kill # enemies with a stationary sniper rifle (func_tank)
	ACH_CH1,				// 7 complete chapter
	ACH_CH2,				// 8
	ACH_CH3,				// 9
	ACH_CH4,				// 10
	ACH_CH5,				// 11
	ACH_GENERAL30SEC,		// 12 kill security general under 30 sec
	ACH_HPREGENERATE,		// 13 regenerate a total of # health
	ACH_RECEIVEDAMAGE,		// 14 receive a total of # damage
	ACH_OVERCOOK,			// 15 overcook the grenade
	ACH_DRONESEC,			// 16 kill # security drones
	ACH_DRONEALIEN,			// 17 kill # alien drones
	ACH_CROSSBOW,			// 18 kill # enemies on a certain distance with a crossbow
	ACH_DASH,				// 19 dash # times
	ACH_NOTES,				// 20 find # notes
	ACH_KILLENEMIESBALLS,	// 21 kill # enemies with balls (weapon_ar2 or func_tankball)
	ACH_REDDWELLER,			// 22 help the red dweller escape (chapter 1)
	ACH_ASSEMBLEBLASTLEVEL,	// 23 get the first blast level by assembling the module on ch2map2
	ACH_BROKENCAR,			// 24 break the car completely in chapter 1 intro
	ACH_CARDISTANCE,		// 25 travelled distance by car
	ACH_WATERJETDISTANCE,	// 26 travelled distance by water jet
	ACH_KILLBOTS,			// 27 kill # bots in multiplayer
	ACH_CH3_NOKILLDW,		// 28 don't kill any dwellers in chapter 3
	ACH_CH3_3MINS,			// 29 destroy the computer within 3 minutes
	ACH_5DRONES,			// 30 kill 5 enemy drones while piloting a drone from 1st person
	ACH_DRUNK,				// 31 kill 5 enemies while drunk
	ACH_DIDNTLISTEN,		// 32 you didn't listen to Alice in ch5map2
	ACH_CARMAGEDDON,		// 33 run over 5 enemies with a car
	ACH_ELECTROBLAST,		// 34 kill 30 enemies with electroblast
};

#define TOTAL_ACHIEVEMENTS 35

const int AchievementGoals[TOTAL_ACHIEVEMENTS]
{
	10000,	// ACH_BULLETSFIRED = 0,	// 0 fire # bullets
	1000,	// ACH_JUMPS,				// 1 jump # times
	100,	// ACH_AMMOCRATES,			// 2 find # ammo crates
	5,		// ACH_DISARMEDMINES,		// 3 disarm # enemy mines (disarming your own doesn't count)
	666,	// ACH_KILLENEMIES,			// 4 kill # enemies
	100000,	// ACH_INFLICTDAMAGE,		// 5 inflict a total of # damage
	30,		// ACH_KILLENEMIESSNIPER,	// 6 kill # enemies with weapon_sniper
	1,		// ACH_CH1,					// 7 complete chapter
	1,		// ACH_CH2,					// 8
	1,		// ACH_CH3,					// 9
	1,		// ACH_CH4,					// 10
	1,		// ACH_CH5,					// 11
	1,		// ACH_GENERAL30SEC,		// 12 kill security general under 30 sec
	9000,	// ACH_HPREGENERATE,		// 13 regenerate a total of # health
	10000,	// ACH_RECEIVEDAMAGE,		// 14 receive a total of # damage
	1,		// ACH_OVERCOOK,			// 15 overcook the grenade
	30,		// ACH_DRONESEC,			// 16 kill # security drones
	30,		// ACH_DRONEALIEN,			// 17 kill # alien drones
	10,		// ACH_CROSSBOW,			// 18 kill # enemies on a certain distance with a crossbow
	100,	// ACH_DASH,				// 19 dash # times
	50,		// ACH_NOTES,				// 20 find 50 notes
	30,		// ACH_KILLENEMIESBALLS,	// 21 kill # enemies with balls (weapon_ar2 or func_tankball)
	1,		// ACH_REDDWELLER,			// 22 help the red dweller escape (chapter 1)
	1,		// ACH_ASSEMBLEBLASTLEVEL,	// 23 get the first blast level by assembling the module on ch2map2
	1,		// ACH_BROKENCAR,			// 24 break the car completely in chapter 1 intro
	10000,	// ACH_CARDISTANCE,			// 25 travelled distance by car
	5000,	// ACH_WATERJETDISTANCE,	// 26 travelled distance by water jet
	500,	// ACH_KILLBOTS,			// 27 kill # bots in multiplayer
	1,		// ACH_CH3_NOKILLDW,		// 28 don't kill any dwellers in chapter 3
	1,		// ACH_CH3_3MINS,			// 29 destroy the computer within 3 minutes
	5,		// ACH_5DRONES,				// 30 kill 5 enemy drones while piloting a drone from 1st person
	5,		// ACH_DRUNK,				// 31 kill 5 enemies while drunk
	1,		// ACH_DIDNTLISTEN,			// 32 you didn't listen to Alice in ch5map2
	5,		// ACH_CARMAGEDDON,			// 33 run over 5 enemies with a car
	30,		// ACH_ELECTROBLAST,		// 34 kill 30 enemies with electroblast
};

const char * const AchievementNames[TOTAL_ACHIEVEMENTS]
{
	"ach_firebullets",		// ACH_BULLETSFIRED = 0,	// 0 fire # bullets
	"ach_jump",				// ACH_JUMPS,				// 1 jump # times
	"ach_ammocrates",		// ACH_AMMOCRATES,			// 2 find # ammo crates
	"ach_disarm",			// ACH_DISARMEDMINES,		// 3 disarm # enemy mines (disarming your own doesn't count)
	"ach_killenemies",		// ACH_KILLENEMIES,			// 4 kill # enemies
	"ach_inflictdamage",	// ACH_INFLICTDAMAGE,		// 5 inflict a total of # damage
	"ach_killenemiessniper",// ACH_KILLENEMIESSNIPER,	// 6 kill # enemies with a stationary sniper rifle (func_tank)
	"ach_chapter1",			// ACH_CH1,					// 7 complete chapter
	"ach_chapter2",			// ACH_CH2,					// 8
	"ach_chapter3",			// ACH_CH3,					// 9
	"ach_chapter4",			// ACH_CH4,					// 10
	"ach_chapter5",			// ACH_CH5,					// 11
	"ach_general30sec",		// ACH_GENERAL30SEC,		// 12 kill security general under 30 sec
	"ach_hpregenerate",		// ACH_HPREGENERATE,		// 13 regenerate a total of # health
	"ach_receivedamage",	// ACH_RECEIVEDAMAGE,		// 14 receive a total of # damage
	"ach_overcook",			// ACH_OVERCOOK,			// 15 overcook the grenade
	"ach_dronesec",			// ACH_DRONESEC,			// 16 kill # security drones
	"ach_dronealien",		// ACH_DRONEALIEN,			// 17 kill # alien drones
	"ach_crossbow",			// ACH_CROSSBOW,			// 18 kill # enemies on a certain distance with a crossbow
	"ach_dash",				// ACH_DASH,				// 19 dash # times
	"ach_notes",			// ACH_NOTES,				// 20 find # notes
	"ach_killenemiesballs",	// ACH_KILLENEMIESBALLS,	// 21 kill # enemies with balls (weapon_ar2 or func_tankball)
	"ach_reddweller",		// ACH_REDDWELLER,			// 22 help the red dweller escape (chapter 1)
	"ach_assembleblastlevel",	// ACH_ASSEMBLEBLASTLEVEL,	// 23 get the first blast level by assembling the module on ch2map2
	"ach_brokencar",		// ACH_BROKENCAR,			// 24 break the car completely in chapter 1 intro
	"ach_cardistance",		// ACH_CARDISTANCE,			// 25 travelled distance by car
	"ach_waterjetdistance",	// ACH_WATERJETDISTANCE,	// 26 travelled distance by water jet
	"ach_killbots",			// ACH_KILLBOTS,			// 27 kill # bots in multiplayer
	"ach_dwellerch3",		// ACH_CH3_NOKILLDW,		// 28 don't kill any dwellers in chapter 3
	"ach_ch3_3mins",		// ACH_CH3_3MINS,			// 29 destroy the computer within 3 minutes
	"ach_5drones",			// ACH_5DRONES,				// 30 kill 5 enemy drones while piloting a drone from 1st person
	"ach_drunk",			// ACH_DRUNK,				// 31 kill 5 enemies while drunk
	"ach_didntlisten",		// ACH_DIDNTLISTEN,			// 32 you didn't listen to Alice in ch5map2
	"ach_carmageddon",		// ACH_CARMAGEDDON,			// 33 run over 5 enemies with a car
	"ach_electroblast",		// ACH_ELECTROBLAST,		// 34 kill 30 enemies with electroblast
};

enum Achievement_data_e
{
	AchD_GOAL = 0,
	AchD_VALUE,
	AchD_COMPLETION,
	AchD_NAME,
};

typedef struct
{
	int goal[TOTAL_ACHIEVEMENTS];
	int value[TOTAL_ACHIEVEMENTS];
	bool completion[TOTAL_ACHIEVEMENTS];
	char name[TOTAL_ACHIEVEMENTS][100];

} achievement_data_t;

#define ACHVAL_ADD 0
#define ACHVAL_SUBTRACT 1
#define ACHVAL_EQUAL 2

#define ACHIEVEMENT_CHECK_TIME 5 // period between checks