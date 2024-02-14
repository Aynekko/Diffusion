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

#ifndef GAME_H
#define GAME_H

extern void GameDLLInit( void );
extern void GameDLLShutdown( void );

extern cvar_t	displaysoundlist;

extern cvar_t sv_fadecorpses;
extern cvar_t sv_allowhealthbars;
//extern cvar_t sv_muzzlelight;
extern cvar_t ai_disable;
extern cvar_t ai_draw_route;
extern cvar_t sv_fade_props;
extern cvar_t sv_force_fadedistance;

// multiplayer server rules
extern cvar_t teamplay;
extern cvar_t fraglimit;
extern cvar_t timelimit;
extern cvar_t fragsleft;
extern cvar_t timeleft;
extern cvar_t friendlyfire;
extern cvar_t falldamage;
extern cvar_t weaponstay;
extern cvar_t forcerespawn;
extern cvar_t flashlight;
extern cvar_t aimcrosshair;
extern cvar_t decalfrequency;
extern cvar_t teamlist;
extern cvar_t teamoverride;
extern cvar_t defaultteam;
extern cvar_t allowmonsters;
extern cvar_t mp_dash_air;
extern cvar_t mp_alwaysgib;
extern cvar_t sv_ignore_triggers;
extern cvar_t mp_weaponbonus;
extern cvar_t mp_healthbonus;
extern cvar_t mp_hidecorpses;
extern cvar_t mp_spectator_cmd_delay;
extern cvar_t mp_allow_spectators;
extern cvar_t mp_spectator_notify;
extern cvar_t mp_servermsg_delay;
extern cvar_t mp_server_notify;
extern cvar_t mp_spawnprotect;
extern cvar_t mp_explodesatchels;
extern cvar_t mp_allow_bonuses;
extern cvar_t sv_train_debug;

extern cvar_t bot_show_all_points;
extern cvar_t bot_max;
extern cvar_t bot_min;

// Engine Cvars
extern cvar_t	*g_psv_gravity;
extern cvar_t	*g_psv_aim;
extern cvar_t	*g_psv_stepsize;
extern cvar_t	*g_footsteps;
extern cvar_t	*g_debugdraw;	// novodex physics debug
extern cvar_t	*p_speeds;
extern cvar_t	*g_physdebug;	// quake physics debug
extern cvar_t	*g_allow_physx;
extern cvar_t	g_sync_physic;

#endif		// GAME_H
