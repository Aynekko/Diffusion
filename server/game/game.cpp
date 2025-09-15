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
#include "extdll.h"
#include "eiface.h"
#include "util.h"
#include "game/game.h"
#include "cbase.h"
#include "game/client.h"

cvar_t	displaysoundlist = {"displaysoundlist","0"};

// multiplayer server rules
cvar_t	fragsleft	= {"mp_fragsleft","0", FCVAR_SERVER | FCVAR_UNLOGGED };	  // Don't spam console/log files/users with this changing
cvar_t	timeleft	= {"mp_timeleft","0" , FCVAR_SERVER | FCVAR_UNLOGGED };	  // "      "

// multiplayer server rules
cvar_t	teamplay	= {"mp_teamplay","0", FCVAR_SERVER };
cvar_t	fraglimit	= {"mp_fraglimit","0", FCVAR_SERVER };
cvar_t	timelimit	= {"mp_timelimit","0", FCVAR_SERVER };
cvar_t	friendlyfire= {"mp_friendlyfire","0", FCVAR_SERVER };
cvar_t	falldamage	= {"mp_falldamage","1", FCVAR_SERVER };
cvar_t	weaponstay	= {"mp_weaponstay","0", FCVAR_SERVER };
cvar_t	forcerespawn= {"mp_forcerespawn","1", FCVAR_SERVER };
cvar_t	flashlight	= {"mp_flashlight","1", FCVAR_SERVER };
cvar_t	aimcrosshair= {"mp_autocrosshair","1", FCVAR_SERVER };
cvar_t	decalfrequency = {"decalfrequency","1", FCVAR_SERVER };
cvar_t	teamlist = {"mp_teamlist","hgrunt;scientist", FCVAR_SERVER };
cvar_t	teamoverride = {"mp_teamoverride","1" };
cvar_t	defaultteam = {"mp_defaultteam","0" };
cvar_t	allowmonsters={"mp_allowmonsters","0", FCVAR_SERVER };

cvar_t	mp_chattime = {"mp_chattime","10", FCVAR_SERVER };
cvar_t	debugdraw = { "phys_debug", "0", FCVAR_ARCHIVE };
cvar_t	physdebug = { "phys_qdebug", "0", 0 };
cvar_t	physstats = { "p_speeds", "0", FCVAR_ARCHIVE };

//START BOT
cvar_t   cvar_bot = { "bot", "" };
cvar_t	bot_show_all_points = { "bot_show_all_points", "0", FCVAR_CHEAT };
cvar_t bot_max = { "bot_max", "3", FCVAR_SERVER };
cvar_t bot_min = { "bot_min", "0", FCVAR_SERVER };
//END BOT

// DiffusionRegen
cvar_t	sv_regeneration	= { "sv_regeneration","1", FCVAR_ARCHIVE };

// Diffusion
cvar_t	sv_allowhealthbars = { "sv_allowhealthbars", "1", FCVAR_ARCHIVE };
cvar_t ai_draw_route = { "ai_draw_route", "0", FCVAR_CHEAT };
cvar_t  ai_disable = { "ai_disable", "0", FCVAR_CHEAT };
cvar_t  sv_ignore_triggers = { "sv_ignore_triggers", "0", FCVAR_CHEAT };
cvar_t	mp_dash_air = { "mp_dash_air", "1", FCVAR_SERVER };
cvar_t	mp_alwaysgib = { "mp_alwaysgib", "0", FCVAR_SERVER };
cvar_t	mp_killercamera = { "mp_killercamera", "1", FCVAR_SERVER };
cvar_t	mp_weaponbonus = { "mp_weaponbonus", "1", FCVAR_SERVER };
cvar_t	mp_hidecorpses = { "mp_hidecorpses", "0", FCVAR_SERVER };
cvar_t	mp_spectator_cmd_delay = { "mp_spectator_cmd_delay", "2", FCVAR_SERVER };
cvar_t	mp_allow_spectators = { "mp_allow_spectators", "1", FCVAR_SERVER };
cvar_t	mp_spectator_notify = { "mp_spectator_notify", "7", FCVAR_SERVER };
cvar_t	mp_servermsg_delay = { "mp_servermsg_delay", "30", FCVAR_SERVER };
cvar_t	mp_server_notify = { "mp_server_notify", "0", FCVAR_SERVER };
cvar_t	mp_spawnprotect = { "mp_spawnprotect", "3", FCVAR_SERVER };
cvar_t	mp_healthbonus = { "mp_healthbonus", "0", FCVAR_SERVER };
cvar_t	mp_explodesatchels = { "mp_explodesatchels", "1", FCVAR_SERVER };
cvar_t mp_allow_bonuses = { "mp_allow_bonuses", "1", FCVAR_SERVER };
cvar_t sv_train_debug = { "sv_train_debug", "0", FCVAR_SPONLY };
cvar_t mp_maxturrets = { "mp_maxturrets", "3", FCVAR_SERVER }; // how many turrets a player can spawn

// sv_fade_props: 0 - disable all server culling, 1 - enable distance culling, 2 and more - also enable back-culling, where number means distance in units (i.e. sv_fade_props 500)
cvar_t	sv_fade_props = { "sv_fade_props", "1", FCVAR_ARCHIVE };
cvar_t	sv_force_fadedistance = { "sv_force_fadedistance", "0", FCVAR_CHEAT };
cvar_t	sv_cubemap_culling = { "sv_cubemap_culling", "0", FCVAR_SPONLY | FCVAR_CHEAT }; // this disables culling for all env_statics, func_wall and func_illusionary

// Engine Cvars
cvar_t 	*g_psv_gravity = NULL;
cvar_t	*g_psv_aim = NULL;
cvar_t	*g_footsteps = NULL;
cvar_t	*g_psv_stepsize = NULL;
cvar_t	*g_debugdraw = NULL;
cvar_t	*g_physdebug = NULL;
cvar_t	*p_speeds = NULL;
cvar_t	*g_allow_physx = NULL;
cvar_t	g_sync_physic = { "sv_sync_physic", "0", FCVAR_ARCHIVE };

void Cmd_ShowTriggers_f( void )
{
	edict_t *pEdict = INDEXENT( 1 );
	if( !pEdict ) return;

	for( int i = 1; i < gpGlobals->maxEntities; i++, pEdict++ )
	{
		if( pEdict->free )	// Not in use
			continue;
		
		CBaseEntity *pEntity = CBaseEntity :: Instance( pEdict );
		if( !pEntity || !FBitSet( pEntity->m_iFlags, MF_TRIGGER|MF_LADDER ))
			continue;

		if( FBitSet( pEntity->m_iFlags, MF_LADDER ))
		{
			if( pEntity->pev->renderamt <= 0 )
				pEntity->pev->renderamt = 255;
			else pEntity->pev->renderamt = 0;
		}
		else if( FBitSet( pEntity->m_iFlags, MF_TRIGGER ))
		{
			if( FBitSet( pEntity->pev->effects, EF_NODRAW ))
				pEntity->pev->effects &= ~EF_NODRAW;
			else pEntity->pev->effects |= EF_NODRAW;
		}
	}
}

// Register your console variables here
// This gets called one time when the game is initialied
void GameDLLInit( void )
{
	// Register cvars here:

	g_psv_gravity = CVAR_GET_POINTER( "sv_gravity" );
	g_psv_aim = CVAR_GET_POINTER( "sv_aim" );
	g_footsteps = CVAR_GET_POINTER( "mp_footsteps" );
	g_psv_stepsize = CVAR_GET_POINTER( "sv_stepsize" );
	CVAR_REGISTER( &g_sync_physic );

	g_engfuncs.pfnAddServerCommand( "showtriggers_toggle", Cmd_ShowTriggers_f );

	g_engfuncs.pfnAddServerCommand( "dump_entity_sizes", DumpEntitySizes_f );
	g_engfuncs.pfnAddServerCommand( "dump_entity_names", DumpEntityNames_f );

#ifdef HAVE_STRINGPOOL
	g_engfuncs.pfnAddServerCommand( "dump_strings", DumpStrings_f );
#endif
	CVAR_REGISTER (&displaysoundlist);

	CVAR_REGISTER (&teamplay);
	CVAR_REGISTER (&fraglimit);
	CVAR_REGISTER (&timelimit);

	CVAR_REGISTER (&fragsleft);
	CVAR_REGISTER (&timeleft);
	CVAR_REGISTER (&debugdraw);
	CVAR_REGISTER (&physstats);
	CVAR_REGISTER (&physdebug);

	CVAR_REGISTER (&friendlyfire);
	CVAR_REGISTER (&falldamage);
	CVAR_REGISTER (&weaponstay);
	CVAR_REGISTER (&forcerespawn);
	CVAR_REGISTER (&flashlight);
	CVAR_REGISTER (&aimcrosshair);
	CVAR_REGISTER (&decalfrequency);
	CVAR_REGISTER (&teamlist);
	CVAR_REGISTER (&teamoverride);
	CVAR_REGISTER (&defaultteam);
	CVAR_REGISTER (&allowmonsters);

	CVAR_REGISTER (&mp_chattime);

	// server debug drawing support requires build up to 1940
	g_debugdraw = CVAR_GET_POINTER( "phys_debug" );
	g_physdebug = CVAR_GET_POINTER( "qphys_debug" );
	p_speeds = CVAR_GET_POINTER( "p_speeds" );
	g_allow_physx = CVAR_GET_POINTER( "sv_allow_PhysX" );

	// DiffusionRegen
	CVAR_REGISTER ( &sv_regeneration );

	CVAR_REGISTER( &sv_allowhealthbars );

	CVAR_REGISTER ( &ai_disable );
	CVAR_REGISTER( &ai_draw_route );
	CVAR_REGISTER ( &sv_fade_props );
	CVAR_REGISTER( &sv_force_fadedistance );
	CVAR_REGISTER( &sv_cubemap_culling );
	CVAR_REGISTER( &sv_ignore_triggers );
	CVAR_REGISTER( &mp_dash_air );
	CVAR_REGISTER( &mp_alwaysgib );
	CVAR_REGISTER( &mp_killercamera );
	CVAR_REGISTER( &mp_weaponbonus );
	CVAR_REGISTER( &mp_healthbonus );
	CVAR_REGISTER( &mp_hidecorpses );
	CVAR_REGISTER( &mp_spectator_cmd_delay );
	CVAR_REGISTER( &mp_allow_spectators );
	CVAR_REGISTER( &mp_spectator_notify );
	CVAR_REGISTER( &mp_servermsg_delay );
	CVAR_REGISTER( &mp_server_notify );
	CVAR_REGISTER( &mp_spawnprotect );
	CVAR_REGISTER( &mp_explodesatchels );
	CVAR_REGISTER( &mp_allow_bonuses );
	CVAR_REGISTER( &sv_train_debug );
	CVAR_REGISTER( &mp_maxturrets );

	//START BOT
	CVAR_REGISTER( &cvar_bot );
	CVAR_REGISTER( &bot_show_all_points );
	CVAR_REGISTER( &bot_max );
	CVAR_REGISTER( &bot_min );
	//END BOT

	// Yes in the Xash3D we can register messages here
	LinkUserMessages();
}

void GameDLLShutdown( void )
{
	WorldPhysic->FreePhysic();	// release physic world
}