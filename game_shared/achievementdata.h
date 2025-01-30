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
	ACH_TANKBALL,			// 19 kill the alien military ship with balls
	ACH_DASH,				// 20 dash # times
	ACH_NOTES,				// 21 find # notes
	ACH_SECRETS,			// 22 find # secrets
	ACH_KILLENEMIESBALLS,	// 23 kill # enemies with balls (weapon_ar2 or func_tankball)
	ACH_REDDWELLER,			// 24 help the red dweller escape (chapter 1)
	ACH_ASSEMBLEBLASTLEVEL,	// 25 get the first blast level by assembling the module on ch2map2
	ACH_BROKENCAR,			// 26 break the car completely in chapter 1 intro
	ACH_CARDISTANCE,		// 27 travelled distance by car
	ACH_WATERJETDISTANCE,	// 28 travelled distance by water jet
	ACH_KILLBOTS,			// 29 kill # bots in multiplayer
	ACH_CH3_NOKILLDW,		// 30 don't kill any dwellers in chapter 3
	ACH_CH3_3MINS,			// 31 destroy the computer within 3 minutes
};

#define TOTAL_ACHIEVEMENTS 32

const int AchievementGoals[TOTAL_ACHIEVEMENTS]
{
	100000, // ACH_BULLETSFIRED = 0,	// 0 fire # bullets
	1000,	// ACH_JUMPS,				// 1 jump # times
	100,	// ACH_AMMOCRATES,			// 2 find # ammo crates
	10,		// ACH_DISARMEDMINES,		// 3 disarm # enemy mines (disarming your own doesn't count)
	100,	// ACH_KILLENEMIES,			// 4 kill # enemies
	10000,	// ACH_INFLICTDAMAGE,		// 5 inflict a total of # damage
	20,		// ACH_KILLENEMIESSNIPER,	// 6 kill # enemies with a stationary sniper rifle (func_tank)
	1,		// ACH_CH1,					// 7 complete chapter
	1,		// ACH_CH2,					// 8
	1,		// ACH_CH3,					// 9
	1,		// ACH_CH4,					// 10
	1,		// ACH_CH5,					// 11
	1,		// ACH_GENERAL30SEC,		// 12 kill security general under 30 sec
	9000,	// ACH_HPREGENERATE,		// 13 regenerate a total of # health
	10000,	// ACH_RECEIVEDAMAGE,		// 14 receive a total of # damage
	1,		// ACH_OVERCOOK,			// 15 overcook the grenade
	20,		// ACH_DRONESEC,			// 16 kill # security drones
	20,		// ACH_DRONEALIEN,			// 17 kill # alien drones
	20,		// ACH_CROSSBOW,			// 18 kill # enemies on a certain distance with a crossbow
	1,		// ACH_TANKBALL,			// 19 kill the alien military ship with balls
	100,	// ACH_DASH,				// 20 dash # times
	50,		// ACH_NOTES,				// 21 find # notes
	10,		// ACH_SECRETS,				// 22 find # secrets
	10,		// ACH_KILLENEMIESBALLS,	// 23 kill # enemies with balls (weapon_ar2 or func_tankball)
	1,		// ACH_REDDWELLER,			// 24 help the red dweller escape (chapter 1)
	1,		// ACH_ASSEMBLEBLASTLEVEL,	// 25 get the first blast level by assembling the module on ch2map2
	1,		// ACH_BROKENCAR,			// 26 break the car completely in chapter 1 intro
	10000,	// ACH_CARDISTANCE,			// 27 travelled distance by car
	5000,	// ACH_WATERJETDISTANCE,	// 28 travelled distance by water jet
	500,	// ACH_KILLBOTS,			// 29 kill # bots in multiplayer
	1,		// ACH_CH3_NOKILLDW,		// 30 don't kill any dwellers in chapter 3
	1,		// ACH_CH3_3MINS,			// 31 destroy the computer within 3 minutes
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

#define ACHIEVEMENT_CHECK_TIME 5 // period between checks