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
// Robin, 4-22-98: Moved set_suicide_frame() here from player.cpp to allow us to 
//				   have one without a hardcoded player.mdl in tf_client.cpp

/*

===== client.cpp ========================================================

  client/server game specific stuff

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "game/saverestore.h"
#include "player.h"
#include "spectator.h"
#include "game/client.h"
#include "entities/soundent.h"
#include "game/gamerules.h"
#include "game/game.h"
#include "customentity.h"
#include "weapons/weapons.h"
#include "weaponinfo.h"
#include "usercmd.h"
#include "netadr.h"
#include "pm_defs.h"
#include "const.h"
#include "nodes.h"

// diffusionbots
// START BOT
#include "bot/bot.h"

void BotCreate( const char *skin, const char *name, const char *skill );
void BotGenerateWaypointFile( int LookDistance );
extern int f_Observer;  // flag for observer mode
extern int f_botskill;  // default bot skill level
extern int f_botdontshoot;  // flag to disable targeting other bots
extern respawn_t bot_respawn[32];
float bot_check_time = 10.0;
int min_bots = 0;
int max_bots = 0;
// END BOT

extern DLL_GLOBAL ULONG		g_ulModelIndexPlayer;
extern DLL_GLOBAL BOOL		g_fGameOver;
extern DLL_GLOBAL int		g_iSkillLevel;
extern DLL_GLOBAL ULONG		g_ulFrameCount;

extern void CopyToBodyQue( CBaseEntity *pCorpse );
extern int giPrecacheGrunt;
extern int gmsgSayText;

extern int g_teamplay;

/*
 * used by kill command and disconnect command
 * ROBIN: Moved here from player.cpp, to allow multiple player models
 */
void set_suicide_frame(entvars_t* pev)
{       
	if (!FStrEq(STRING(pev->model), "models/player.mdl"))
		return; // allready gibbed

//	pev->frame		= $deatha11;
	pev->solid		= SOLID_NOT;
	pev->movetype	= MOVETYPE_TOSS;
	pev->deadflag	= DEAD_DEAD;
	pev->nextthink	= -1;
}


/*
===========
ClientConnect

called when a player connects to a server
============
*/

BOOL ClientConnect( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[ 128 ]  )
{	
	return g_pGameRules->ClientConnected( pEntity, pszName, pszAddress, szRejectReason );
}

/*
===========
ClientDisconnect

called when a player disconnects from a server

GLOBALS ASSUMED SET:  g_fGameOver
============
*/
void ClientDisconnect( edict_t *pEdict )
{
	if (g_fGameOver)
		return;

	CBaseEntity *pEntity = (CBaseEntity *)CBaseEntity::Instance( pEdict );

	char text[256] = "";
	if( pEntity->pev->netname && !(pEntity->pev->flags & FL_FAKECLIENT) )
	{
		snprintf( text, sizeof( text ), "- %s has left the game\n", STRING( pEntity->pev->netname ) );
		EMIT_SOUND( INDEXENT( 0 ), CHAN_STATIC, SND_PLAYER_LEFT, 1, ATTN_NONE );
	}

	MESSAGE_BEGIN( MSG_ALL, gmsgSayText, NULL );
		WRITE_BYTE( pEntity->entindex() );
		WRITE_STRING( text );
	MESSAGE_END();

	CSound *pSound;
	pSound = CSoundEnt::SoundPointerForIndex( CSoundEnt::ClientSoundIndex( pEdict ));
	{
		// since this client isn't around to think anymore, reset their sound. 
		if ( pSound )
			pSound->Reset();
	}

	// since the edict doesn't get deleted, fix it so it doesn't interfere.
	pEntity->pev->takedamage = DAMAGE_NO;// don't attract autoaim
	pEntity->pev->solid = SOLID_NOT;// nonsolid
	pEntity->RelinkEntity( TRUE );

	g_pGameRules->ClientDisconnected( pEdict );
}


// called by ClientKill and DeadThink
void respawn( CBaseEntity *pClient, BOOL fCopyCorpse)
{
	if (gpGlobals->coop || gpGlobals->deathmatch)
	{
		if ( fCopyCorpse )
		{
			// make a copy of the dead body for appearances sake
			CopyToBodyQue( pClient );
		}

		// respawn player
		pClient->Spawn( );
	}
	else
	{       // restart the entire server
		SERVER_COMMAND("reload\n");
	}
}

/*
============
ClientKill

Player entered the suicide command

GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
============
*/
void ClientKill( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;

	// Is the client spawned yet?
	if( !pEntity->pvPrivateData )
		return;

	if( g_fGameOver ) // diffusion
		return;

	// PrivateData is never deleted after it was created on first PutInServer so we will check for IsConnected flag too
	CBasePlayer *pPlayer = (CBasePlayer *)CBasePlayer::Instance( pev );

	if( !pPlayer || !pPlayer->IsConnected() )
		return;

	// prevent suiciding too often
	if( pPlayer->m_fNextSuicideTime > gpGlobals->time )
		return;

	pPlayer->m_fNextSuicideTime = gpGlobals->time + 5;  // don't let them suicide for 5 seconds after suiciding

	// prevent death in spectator mode
	if( pev->iuser1 != OBS_NONE )
	{
		ClientPrint( pev, HUD_PRINTCONSOLE, UTIL_VarArgs( "Can't suicide while in spectator mode!\n" ) );
		return;
	}

	// prevent death if already dead
	if( pev->deadflag != DEAD_NO )
	{
		ClientPrint( pev, HUD_PRINTCONSOLE, UTIL_VarArgs( "Can't suicide -- already dead!\n" ) );
		return;
	}

	// prevent death in welcome cam
	if( pPlayer->m_bInWelcomeCam )
	{
		ClientPrint( pev, HUD_PRINTCONSOLE, UTIL_VarArgs( "Can't suicide while in welcome cam mode!\n" ) );
		return;
	}

	// have the player kill themself
	pev->health = 0;
	pPlayer->Killed( pev, GIB_NEVER );
}

/*
===========
ClientPutInServer

called each time a player is spawned
============
*/
void ClientPutInServer( edict_t *pEntity )
{
	CBasePlayer *pPlayer;

	entvars_t *pev = &pEntity->v;

	pPlayer = GetClassPtr((CBasePlayer *)pev);
	pPlayer->SetCustomDecalFrames(-1); // Assume none;

	// Allocate a CBasePlayer for pev, and call spawn
	pPlayer->Spawn() ;

	// Reset interpolation during first frame
	pPlayer->pev->effects |= EF_NOINTERP;

	// Mark as PutInServer
	pPlayer->m_bPutInServer = TRUE;

	pPlayer->pev->iuser1 = 0;	// disable any spec modes
	pPlayer->pev->iuser2 = 0;
}

//==================================================================================
// diffusion - play sounds when saying something
//==================================================================================
void SaySounds( char *Text, CBasePlayer *pPlayer )
{	
	for( int i = 0; Text[i] != 0; i++ )
		Text[i] = tolower( Text[i] );
	
	if( !strncmp( Text, "lol", 3 ) || !strncmp( Text, "haha", 4 ) || !strncmp( Text, "ahah", 4 ) )
	{
		if( pPlayer->m_flNextChatSoundTime < gpGlobals->time )
			EMIT_SOUND( INDEXENT( 0 ), CHAN_STATIC, (char*)g_pGameRules->snd_lol[RANDOM_LONG(0,g_pGameRules->TotalServerSounds_lol - 1)][MAX_SOUNDPATH], 1, 0 );
		pPlayer->m_flNextChatSoundTime = gpGlobals->time + CHAT_SOUND_INTERVAL;
	}
	else if( !strncmp( Text, "meow", 4 ))
	{
		if( pPlayer->m_flNextChatSoundTime < gpGlobals->time )
			EMIT_SOUND( INDEXENT( 0 ), CHAN_STATIC, (char *)g_pGameRules->snd_meow[RANDOM_LONG( 0, g_pGameRules->TotalServerSounds_meow - 1 )][MAX_SOUNDPATH], 1, 0 );
		pPlayer->m_flNextChatSoundTime = gpGlobals->time + CHAT_SOUND_INTERVAL;
	}
}

//==================================================================================
// diffusion - multiplayer commands (timeleft etc.)
//==================================================================================
bool ChatCommands( char *Text, CBasePlayer *pPlayer )
{
	for( int i = 0; Text[i] != 0; i++ )
		Text[i] = tolower( Text[i] );
	
	if( FStrEq( Text, "timeleft" ) )
	{
		if( g_fGameOver )
			return true;
		
		int seconds = timeleft.value;
		int minutes = 0;

		if( seconds > 0 )
		{
			minutes = seconds / 60.0f;
			seconds = seconds - minutes * 60;
			if( seconds >= 10 )
				ClientPrint( pPlayer->pev, HUD_PRINTTALK, UTIL_VarArgs( "* ^5Time left^7: %i:%i\n", minutes, seconds ) );
			else
				ClientPrint( pPlayer->pev, HUD_PRINTTALK, UTIL_VarArgs( "* ^5Time left^7: %i:0%i\n", minutes, seconds ) );
		}
		else
			ClientPrint( pPlayer->pev, HUD_PRINTTALK, UTIL_VarArgs( "* ^5Time left^7: No time limit\n" ) );

		return true;
	}
	else if( FStrEq( Text, "fragsleft" ) )
	{
		if( g_fGameOver )
			return true;

		int frags_left = fragsleft.value;
		if( frags_left > 0 )
			ClientPrint( pPlayer->pev, HUD_PRINTTALK, UTIL_VarArgs( "* ^5Frags left^7: %i\n", frags_left ) );
		else
			ClientPrint( pPlayer->pev, HUD_PRINTTALK, UTIL_VarArgs( "* ^5Frags left^7: No frag limit\n" ) );

		return true;
	}
	
	return false;
}

//// HOST_SAY
// String comes in as
// say blah blah blah
// or as
// blah blah blah
//
void Host_Say( edict_t *pEntity, int teamonly )
{	
	CBasePlayer *client;
	int		j;
	char	*p;
	char	text[128];
	char    szTemp[256];
	const char *cpSay = "say";
	const char *cpSayTeam = "say_team";
	const char *pcmd = CMD_ARGV(0);

	// We can get a raw string now, without the "say " prepended
	if ( CMD_ARGC() == 0 )
		return;

	entvars_t *pev = &pEntity->v;
	CBasePlayer* player = GetClassPtr((CBasePlayer *)pev);

	// Not yet.
	if ( player->m_flNextChatTime > gpGlobals->time )
		 return;

	if ( !stricmp( pcmd, cpSay) || !stricmp( pcmd, cpSayTeam ) )
	{
		if ( CMD_ARGC() >= 2 )
			p = (char *)CMD_ARGS();
		else
			return; // say with a blank message, nothing to do
	}
	else  // Raw text, need to prepend argv[0]
	{
		if ( CMD_ARGC() >= 2 )
			sprintf( szTemp, "%s %s", ( char * )pcmd, (char *)CMD_ARGS() );
		else
			sprintf( szTemp, "%s", ( char * )pcmd ); // Just a one word command, use the first word...sigh

		p = szTemp;
	}

// remove quotes if present
	if (*p == '"')
	{
		p++;
		p[strlen(p)-1] = 0;
	}

// make sure the text has content
	char* pc;
	for ( pc = p; pc != NULL && *pc != 0; pc++ )
	{
		if ( !isspace( *pc ) )
		{
			pc = NULL;	// we've found an alphanumeric character,  so text is valid
			break;
		}
	}
	if ( pc != NULL )
		return;  // no character found, so say nothing

	SaySounds( (char *)CMD_ARGV( 1 ), player );

	// diffusion - report timeleft etc.
	if( ChatCommands( (char *)CMD_ARGV( 1 ), player ) )
		return;

// turn on color set 2  (color on,  no sound)
	if ( teamonly )
		sprintf( text, "%c(TEAM) %s: ", 2, STRING( pEntity->v.netname ) );
	else
		sprintf( text, "%c%s: ", 2, STRING( pEntity->v.netname ) );

	j = sizeof(text) - 2 - strlen(text);  // -2 for /n and null terminator
	if ( (int)strlen(p) > j )
		p[j] = 0;

	strcat( text, p );
	strcat( text, "\n" );

	player->m_flNextChatTime = gpGlobals->time + CHAT_INTERVAL;

	// loop through all players
	// Start with the first player.
	// This may return the world in single player if the client types something between levels or during spawn
	// so check it, or it will infinite loop

	client = NULL;
	while ( ((client = (CBasePlayer*)UTIL_FindEntityByClassname( client, "player" )) != NULL) && (!FNullEnt(client->edict())) ) 
	{
		if ( !client->pev )
			continue;
		
		if ( client->edict() == pEntity )
			continue;

		if ( !(client->IsNetClient()) )	// Not a client ? (should never be true)
			continue;

		if ( teamonly && g_pGameRules->PlayerRelationship(client, CBaseEntity::Instance(pEntity)) != GR_TEAMMATE )
			continue;

		MESSAGE_BEGIN( MSG_ONE, gmsgSayText, NULL, client->pev );
			WRITE_BYTE( ENTINDEX(pEntity) );
			WRITE_STRING( text );
		MESSAGE_END();

	}

	// print to the sending client
	MESSAGE_BEGIN( MSG_ONE, gmsgSayText, NULL, &pEntity->v );
		WRITE_BYTE( ENTINDEX(pEntity) );
		WRITE_STRING( text );
	MESSAGE_END();

	// echo to server console
	g_engfuncs.pfnServerPrint( text );

	char * temp;
	if ( teamonly )
		temp = "say_team";
	else
		temp = "say";
	
	// team match?
	if ( g_teamplay )
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" %s \"%s\"\n", 
			STRING( pEntity->v.netname ), 
			GETPLAYERUSERID( pEntity ),
			GETPLAYERAUTHID( pEntity ),
			g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pEntity ), "model" ),
			temp,
			p );
	}
	else
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%i>\" %s \"%s\"\n", 
			STRING( pEntity->v.netname ), 
			GETPLAYERUSERID( pEntity ),
			GETPLAYERAUTHID( pEntity ),
			GETPLAYERUSERID( pEntity ),
			temp,
			p );
	}
}


/*
===========
ClientCommand
called each time a player uses a "cmd" command
============
*/
extern float g_flWeaponCheat;

// Use CMD_ARGV,  CMD_ARGV, and CMD_ARGC to get pointers the character string command.
void ClientCommand( edict_t *pEntity )
{
	static int requestID = 0;
	char *pcmd = (char*)CMD_ARGV(0);
	char *pOldCmd = pcmd; // not lowered
	const char *pstr;

	entvars_t *pev = &pEntity->v;

	// Is the client spawned yet?
	if ( !pEntity->pvPrivateData )
		return;

	// PrivateData is never deleted after it was created on first PutInServer so we will check for IsConnected flag too
	CBasePlayer *pPlayer = (CBasePlayer *)CBasePlayer::Instance( pev );
	if( !pPlayer || !pPlayer->IsConnected() )
		return;

	if ( FIStrEq(pcmd, "say" ) )
	{
		Host_Say( pEntity, 0 );
		return;
	}
	else if ( FIStrEq(pcmd, "say_team" ) )
	{
		Host_Say( pEntity, 1 );
		return;
	}
	else if ( FIStrEq(pcmd, "fullupdate" ) )
	{
		pPlayer->ForceClientDllUpdate();
		return;
	}
	
	// diffusion - convert command to lowercase
//	for( int i = 0; pcmd[i] != 0; i++ )
//		pcmd[i] = tolower( pcmd[i] );

	bool Cheats = (g_flWeaponCheat != 0.0);
	
	if ( FIStrEq(pcmd, "give" ) ) // diffusion - created entities despawn instantly now
	{
		if ( Cheats )
		{
			int iszItem = ALLOC_STRING( CMD_ARGV(1) );	// Make a copy of the classname
			pPlayer->GiveNamedItem( STRING( iszItem ) );
		}
	}
	else if ( FIStrEq(pcmd, "e_c" ) ) // diffusion - same as "give" but the entity does not spawn inside of player, and you can set name
	{
		if( Cheats )
		{
			if( CMD_ARGC() > 1 )
			{
				int iszItem = ALLOC_STRING( CMD_ARGV( 1 ) );	// Make a copy of the classname
				int iszName = ALLOC_STRING( CMD_ARGV( 2 ) );	// targetname
				CBaseEntity *pEnt = CreateEntityByName( STRING( iszItem ) );

				if( !pEnt )
				{
					ALERT( at_error, "Can't create the entity!\n" );
					return;
				}

				UTIL_MakeVectors( pPlayer->pev->v_angle );
				pEnt->SetAbsOrigin( pPlayer->GetAbsOrigin() + gpGlobals->v_forward * 64 );

				// parameters for dynamic light (for testing)
				if( FStrEq( STRING( iszItem ), "env_dynlight" ) || FStrEq( STRING( iszItem ), "env_projector" ) )
				{
					if( CMD_ARGC() > 2 )
					{
						pEnt->pev->rendercolor.x = atoi( CMD_ARGV( 3 ) );
						pEnt->pev->rendercolor.y = atoi( CMD_ARGV( 4 ) );
						pEnt->pev->rendercolor.z = atoi( CMD_ARGV( 5 ) );
						pEnt->pev->scale = atoi( CMD_ARGV( 6 ) ) * (1.0f / 8.0f);
					}
				}
				else if( FStrEq( STRING( iszItem ), "prop_explosive_barrel" ) )
				{
					pEnt->pev->spawnflags |= BIT( 0 ); // can hold
					pEnt->ObjectCaps();
				}

				if( !FStringNull( iszName ) )
					pEnt->pev->targetname = iszName;
				DispatchSpawn( pEnt->edict() );
			}
			else
				ALERT( at_console, "-- Creates an entity of specified name.\n-- Usage: e_c <classname> <targetname>\n" );
		}
		else
			ALERT( at_console, "\"e_c\" is cheat protected.\n" );
	}
	else if( FIStrEq( pcmd, "upside_down" ) ) // diffusion 
	{
		if( Cheats )
		{
			if( pPlayer->pev->effects & EF_UPSIDEDOWN )
				pPlayer->pev->effects &= ~EF_UPSIDEDOWN;
			else
				pPlayer->pev->effects |= EF_UPSIDEDOWN;
		}
	}
	else if( FIStrEq( pcmd, "showobjective" ) )
	{
		pPlayer->IsShowingObjective = true;
	}
	else if( FIStrEq( pcmd, "hideobjective" ) )
	{
		pPlayer->IsShowingObjective = false;
	}
	else if ( FIStrEq(pcmd, "fire") )
	{
		if ( Cheats )
		{
			if (CMD_ARGC() > 1)
				UTIL_FireTargets(CMD_ARGV(1), pPlayer, pPlayer, USE_TOGGLE, 0);
			else
			{
				TraceResult tr;
				UTIL_MakeVectors(pev->v_angle);
				UTIL_TraceLine(
					pPlayer->EyePosition(),
					pPlayer->EyePosition() + gpGlobals->v_forward * 1024,
					dont_ignore_monsters, pEntity, &tr
				);

				if (tr.pHit)
				{
					CBaseEntity *pHitEnt = CBaseEntity::Instance(tr.pHit);
					if (pHitEnt)
					{
						pHitEnt->Use(pPlayer, pPlayer, USE_TOGGLE, 0);
						ClientPrint( pev, HUD_PRINTCONSOLE, UTIL_VarArgs( "Fired %s \"%s\"\n", STRING(pHitEnt->pev->classname), STRING(pHitEnt->pev->targetname) ) );
					}
				}
			}
		}
	}
	else if ( FIStrEq(pcmd,"inventoryslots") )
	{
		for( int i=0; i<5; i++ )
		{
			ALERT(at_console, "Slot %3d = %3d\n", i+1, pPlayer->CountSlot[i]);
		}
	}
	else if ( FIStrEq(pcmd, "drop" ) )
	{
		// player is dropping an item. 
		pPlayer->DropPlayerItem( (char *)CMD_ARGV( 1 ) );
	}
	else if ( FIStrEq(pcmd, "e_act" ) )
	{
		if ( Cheats )
		{
			if (CMD_ARGC() > 1)
			{
				CBaseEntity *pTarget = NULL;
				bool NotFound = true;
				int use_type = USE_TOGGLE;
				if( CMD_ARGC() > 2 )
				{
					switch( atoi(CMD_ARGV(2)) )
					{
					case 1: use_type = USE_ON; break;
					case 2: use_type = USE_OFF; break;
					}
				}

				while( (pTarget = UTIL_FindEntityByTargetname( pTarget, CMD_ARGV( 1 ) )) != NULL )
				{
					switch( use_type )
					{
					default:
					case USE_TOGGLE:
						pTarget->Use( pPlayer, pPlayer, USE_TOGGLE, 0 );
						ALERT( at_aiconsole, "Toggled entity \"%s\"\n", STRING( pTarget->pev->classname ) );
						break;
					case USE_ON:
						pTarget->Use( pPlayer, pPlayer, USE_ON, 0 );
						ALERT( at_aiconsole, "Turned ON entity \"%s\"\n", STRING( pTarget->pev->classname ) );
						break;
					case USE_OFF:
						pTarget->Use( pPlayer, pPlayer, USE_TOGGLE, 0 );
						ALERT( at_aiconsole, "Turned OFF entity \"%s\"\n", STRING( pTarget->pev->classname ) );
						break;
					}

					if( NotFound )
						NotFound = false;
				}

				if( NotFound )
					ALERT( at_error, "Can't find any entity with such name.\n" );
			}
			else
				ALERT(at_console, "-- Activates all entities of specified name. Use type can be 0 = toggle, 1 = on, 2 = off.\n-- Usage: e_act <targetname> <use_type>\n");
		}
		else
			ALERT(at_console, "\"e_act\" is cheat protected.\n");
	}
	else if( FIStrEq( pcmd, "e_listclass" ) )
	{
		if( Cheats )
		{
			if( CMD_ARGC() > 1 )
			{
				CBaseEntity *pTarget = NULL;
				bool NotFound = true;

				while( (pTarget = UTIL_FindEntityByClassname( pTarget, CMD_ARGV( 1 ) )) != NULL )
				{
					ALERT( at_console, "--- \"%s\"\n", STRING( pTarget->pev->targetname ) );
					if( NotFound )
						NotFound = false;
				}

				if( NotFound )
					ALERT( at_error, "Can't find any entity of such class.\n" );
			}
			else
				ALERT( at_console, "-- Lists all entities of specified class.\n-- Usage: ent_listclass <classname>\n" );
		}
		else
			ALERT( at_console, "\"e_listclass\" is cheat protected.\n" );
	}
	else if ( FIStrEq(pcmd, "e_del"))
	{
		if ( Cheats )
		{
			if (CMD_ARGC() > 1)
			{
				CBaseEntity* pTarget = NULL;
				int DeleteCount = 0;
				
				pTarget = UTIL_FindEntityByTargetname( pTarget, CMD_ARGV( 1 ) );
				while( pTarget != NULL )
				{
					// diffusion - hack for ambient generic - stop sound if we are deleting this entity
					if( FClassnameIs( pTarget, "ambient_generic" ) )
						pTarget->Use( pPlayer, pPlayer, USE_OFF, 0 );
					UTIL_Remove(pTarget);
					DeleteCount++;
					pTarget = UTIL_FindEntityByTargetname( pTarget, CMD_ARGV( 1 ) );
				}

				ALERT(at_console, "%3d entities deleted.\n", DeleteCount);
			}
			else
				ALERT(at_console, "-- Removes a name-specified entity.\n-- Usage: e_del <targetname>\n");
		}
		else
			ALERT(at_console, "\"e_del\" is cheat protected.\n");
	}
	else if( FIStrEq( pcmd, "e_delclass" ) )
	{
		if( Cheats )
		{
			if( CMD_ARGC() > 1 )
			{
				CBaseEntity *pTarget = NULL;
				int DeleteCount = 0;

				pTarget = UTIL_FindEntityByClassname( pTarget, CMD_ARGV( 1 ) );
				while( pTarget != NULL )
				{
					// diffusion - hack for ambient generic - stop sound if we are deleting this entity
					if( FClassnameIs( pTarget, "ambient_generic" ) )
						pTarget->Use( pPlayer, pPlayer, USE_OFF, 0 );
					UTIL_Remove( pTarget );
					DeleteCount++;
					pTarget = UTIL_FindEntityByClassname( pTarget, CMD_ARGV( 1 ) );
				}

				ALERT( at_console, "%3d entities deleted.\n", DeleteCount );
			}
			else
				ALERT( at_console, "-- Removes all entities of specified class.\n-- Usage: e_delclass <classname>\n" );
		}
		else
			ALERT( at_console, "\"e_delclass\" is cheat protected.\n" );
	}
	else if ( FIStrEq(pcmd, "menuactivate" ) )
	{
		if ( pPlayer->HasFlag(F_PLAYER_MENU) )
		{	
			if (CMD_ARGC() > 1)
			{
				CBaseEntity *pTarget = NULL;
				while( ( pTarget = UTIL_FindEntityByTargetname(pTarget, CMD_ARGV(1)) ) != NULL )
					pTarget->Use( pPlayer, pPlayer, USE_TOGGLE, 0);
			}
		}
	}
	else if ( FIStrEq(pcmd, "fov" ) )
	{
		if ( Cheats && CMD_ARGC() > 1)
			pPlayer->m_flFOV = atoi( CMD_ARGV(1) );
		else
			CLIENT_PRINTF( pEntity, print_console, UTIL_VarArgs( "\"fov\" is \"%d\"\n", (int)pPlayer->m_flFOV ) );
	}
	else if ( FIStrEq(pcmd, "use" ) )
	{
		pPlayer->SelectItem((char *)CMD_ARGV(1));
	}
	else if (((pstr = strstr(pcmd, "weapon_")) != NULL)  && (pstr == pcmd))
	{
		pPlayer->SelectItem(pcmd);
	}
	else if ( FIStrEq(pcmd, "lastinv" ))
	{
		pPlayer->SelectLastItem();
	}
	else if( FIStrEq(pcmd, "dash") )
	{
		pPlayer->DashButton = true;
	}
	else if( FIStrEq( pcmd, "electroblast" ) )
	{
		pPlayer->ElectroblastButton = true;
	}
	else if( FIStrEq( pcmd, "shield" ) )
	{
		// in multiplayer, shield only can be picked up, not activated
		if( !g_pGameRules->IsMultiplayer() )
		{
			if( pPlayer->ShieldAvailableLVL > 0 && pPlayer->IsAlive() && pPlayer->NextShieldChangeTime < gpGlobals->time )
			{
				pPlayer->ShieldOn = !pPlayer->ShieldOn;
				pPlayer->NextShieldChangeTime = gpGlobals->time + 0.5;
			}
		}
	}
	else if( FIStrEq( pcmd, "code" ) )
	{
		if( CMD_ARGC() > 2 ) // we have entindex and a code
		{
			int EntIndex = atoi( CMD_ARGV( 1 ) );
			int Code = atoi( CMD_ARGV( 2 ) );
			CBaseEntity *pCodeEnt = CBaseEntity::Instance( INDEXENT( EntIndex ) );
			if( pCodeEnt && FClassnameIs(pCodeEnt,"trigger_codeinput") )
				pCodeEnt->Use( pPlayer, pPlayer, USE_ON, (float)Code );
		}
	}
	else if ( FIStrEq(pcmd, "spectate"))
	{
		if (!(g_pGameRules->IsMultiplayer()))
		{
			ALERT(at_console, "Spectator is not allowed in single player mode.\n");
			return;
		}

		if( g_fGameOver )
			return;
	
		// Block too often spectator command usage
		if (pPlayer->m_flNextSpectatorCommand < gpGlobals->time)
		{
			pPlayer->m_flNextSpectatorCommand = gpGlobals->time + (mp_spectator_cmd_delay.value < 1.0 ? 1.0 : mp_spectator_cmd_delay.value);
			if (!pPlayer->IsObserver())
			{
				if ((pev->flags & FL_PROXY) || mp_allow_spectators.value > 0)
				{
					pPlayer->StartObserver();

					if (((int)mp_spectator_notify.value & 4) == 4)
					{
						// notify other clients of player switched to spectators
						UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("* %s switched to spectator mode\n",
							(pPlayer->pev->netname && STRING(pPlayer->pev->netname)[0] != 0) ? STRING(pPlayer->pev->netname) : "unconnected"));
					}

					// team match?
					if (g_teamplay)
					{
						UTIL_LogPrintf("\"%s<%i><%s><%s>\" switched to spectator mode\n",
							STRING(pPlayer->pev->netname),
							GETPLAYERUSERID(pPlayer->edict()),
							GETPLAYERAUTHID(pPlayer->edict()),
							g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model"));
					}
					else
					{
						UTIL_LogPrintf("\"%s<%i><%s><%i>\" switched to spectator mode\n",
							STRING(pPlayer->pev->netname),
							GETPLAYERUSERID(pPlayer->edict()),
							GETPLAYERAUTHID(pPlayer->edict()),
							GETPLAYERUSERID(pPlayer->edict()));
					}
				}
				else
					ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs("Spectator mode is disabled by server.\n"));
			}
			else
			{
				pPlayer->StopObserver();

				if (((int)mp_spectator_notify.value & 4) == 4)
				{
					// notify other clients of player left spectators
					UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("* %s has left spectator mode\n",
						(pPlayer->pev->netname && STRING(pPlayer->pev->netname)[0] != 0) ? STRING(pPlayer->pev->netname) : "unconnected"));
				}

				// team match?
				if (g_teamplay)
				{
					UTIL_LogPrintf("\"%s<%i><%s><%s>\" has left spectator mode\n",
						STRING(pPlayer->pev->netname),
						GETPLAYERUSERID(pPlayer->edict()),
						GETPLAYERAUTHID(pPlayer->edict()),
						g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model"));
				}
				else
				{
					UTIL_LogPrintf("\"%s<%i><%s><%i>\" has left spectator mode\n",
						STRING(pPlayer->pev->netname),
						GETPLAYERUSERID(pPlayer->edict()),
						GETPLAYERAUTHID(pPlayer->edict()),
						GETPLAYERUSERID(pPlayer->edict()));
				}
			}
		}
	}
	else if ( FIStrEq(pcmd, "specmode"))
	{
		if (pPlayer->IsObserver())
			pPlayer->Observer_SetMode(atoi(CMD_ARGV(1)));
	}
	else if ( FIStrEq(pcmd, "follownext"))
	{
		// No switching of view point in Free Overview
		if (pev->iuser1 == OBS_MAP_FREE)
			return;
		if (pPlayer->IsObserver())
		{
			if (pev->iuser1 == OBS_ROAMING)
				pPlayer->Observer_FindNextSpot(atoi(CMD_ARGV(1)) != 0);
			else
				pPlayer->Observer_FindNextPlayer(atoi(CMD_ARGV(1)) != 0);
		}
	}
	else if( FIStrEq( pcmd, "test" ) )
	{
		
	}
	else if( FIStrEq( pcmd, "bot_notarget" ) ) // this is basically notarget but for bots (they will ignore player). it's not related to spectator mode
	{
	if( !IS_DEDICATED_SERVER() )
	{
		if( CMD_ARGC() > 1 )  // is there an argument to the command?
		{
			f_Observer = atoi( CMD_ARGV( 1 ) );  // set observer flag
			CLIENT_PRINTF( pEntity, print_console, UTIL_VarArgs( "\"observer\" set to %d\n", (int)f_Observer ) );
		}
		else
			CLIENT_PRINTF( pEntity, print_console, UTIL_VarArgs( "\"observer\" is %d\n", (int)f_Observer ) );
	}
	else
		CLIENT_PRINTF( pEntity, print_console, "observer not allowed from client!\n" );
	}
	else if( FIStrEq( pcmd, "botskill" ) )
	{
		if( !IS_DEDICATED_SERVER() )
		{
			if( CMD_ARGC() > 1 )
			{
				f_botskill = atoi( CMD_ARGV( 1 ) );  // set default bot skill level
				CLIENT_PRINTF( pEntity, print_console, UTIL_VarArgs( "\"botskill\" set to %d\n", (int)f_botskill ) );
			}
			else
				CLIENT_PRINTF( pEntity, print_console, UTIL_VarArgs( "\"botskill\" is %d\n", (int)f_botskill ) );
		}
		else
			CLIENT_PRINTF( pEntity, print_console, "botskill not allowed from client!\n" );
	}
	else if( FIStrEq( pcmd, "botdontshoot" ) )
	{
		if( !IS_DEDICATED_SERVER() )
		{
			if( CMD_ARGC() > 1 )  // is there an argument to the command?
			{
				f_botdontshoot = atoi( CMD_ARGV( 1 ) );  // set bot shoot flag
				CLIENT_PRINTF( pEntity, print_console, UTIL_VarArgs( "\"botdontshoot\" set to %d\n", (int)f_botdontshoot ) );
			}
			else
				CLIENT_PRINTF( pEntity, print_console, UTIL_VarArgs( "\"botdontshoot\" is %d\n", (int)f_botdontshoot ) );
		}
		else
			CLIENT_PRINTF( pEntity, print_console, "botdontshoot not allowed from client!\n" );
	}
	else if( FIStrEq( pcmd, "bot_generatewp" ))
	{
		if( CMD_ARGC() > 1 )
			BotGenerateWaypointFile( atoi(CMD_ARGV(1)) );
		else
			BotGenerateWaypointFile( 600 );
	}
	else if( FIStrEq( pcmd, "bot_loadwp" ) )
	{
		BotLoadWaypointFile();
	}		 
	else if( g_pGameRules->ClientCommand( pPlayer, pcmd ) )
	{
		// MenuSelect returns true only if the command is properly handled,  so don't print a warning
	}
	else
	{
		// tell the user they entered an unknown command
		char command[128];

		// check the length of the command (prevents crash)
		// max total length is 192 ...and we're adding a string below ("Unknown command: %s\n")
		strncpy( command, pcmd, 127 );
		command[127] = '\0';

		// tell the user they entered an unknown command
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, UTIL_VarArgs( "Unknown command: %s\n", command ) );
	}
}


/*
========================
ClientUserInfoChanged

called after the player changes
userinfo - gives dll a chance to modify it before
it gets sent into the rest of the engine.
========================
*/
void ClientUserInfoChanged( edict_t *pEntity, char *infobuffer )
{
	// Is the client spawned yet?
	if ( !pEntity->pvPrivateData )
		return;

	// msg everyone if someone changes their name,  and it isn't the first time (changing no name to current name)
	if ( pEntity->v.netname && STRING(pEntity->v.netname)[0] != 0 && !FStrEq( STRING(pEntity->v.netname), g_engfuncs.pfnInfoKeyValue( infobuffer, "name" )) )
	{
		char sName[256];
		char *pName = g_engfuncs.pfnInfoKeyValue( infobuffer, "name" );
		strncpy( sName, pName, sizeof(sName) - 1 );
		sName[ sizeof(sName) - 1 ] = '\0';

		// First parse the name and remove any %'s
		for ( char *pApersand = sName; pApersand != NULL && *pApersand != 0; pApersand++ )
		{
			// Replace it with a space
			if ( *pApersand == '%' )
				*pApersand = ' ';
		}

		// Set the name
		g_engfuncs.pfnSetClientKeyValue( ENTINDEX(pEntity), infobuffer, "name", sName );

		char text[256];
		sprintf( text, "* %s changed name to %s\n", STRING(pEntity->v.netname), g_engfuncs.pfnInfoKeyValue( infobuffer, "name" ) );
		MESSAGE_BEGIN( MSG_ALL, gmsgSayText, NULL );
			WRITE_BYTE( ENTINDEX(pEntity) );
			WRITE_STRING( text );
		MESSAGE_END();

		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" changed name to \"%s\"\n", 
				STRING( pEntity->v.netname ), 
				GETPLAYERUSERID( pEntity ), 
				GETPLAYERAUTHID( pEntity ),
				g_engfuncs.pfnInfoKeyValue( infobuffer, "model" ), 
				g_engfuncs.pfnInfoKeyValue( infobuffer, "name" ) );
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" changed name to \"%s\"\n", 
				STRING( pEntity->v.netname ), 
				GETPLAYERUSERID( pEntity ), 
				GETPLAYERAUTHID( pEntity ),
				GETPLAYERUSERID( pEntity ), 
				g_engfuncs.pfnInfoKeyValue( infobuffer, "name" ) );
		}
	}

	g_pGameRules->ClientUserInfoChanged( GetClassPtr((CBasePlayer *)&pEntity->v), infobuffer );
}

static int g_serveractive = 0;

void ServerDeactivate( void )
{
//	ALERT( at_console, "ServerDeactivate()\n" );

	// It's possible that the engine will call this function more times than is necessary
	// Therefore, only run it one time for each call to ServerActivate 
	if ( g_serveractive != 1 )
		return;

	g_serveractive = 0;

	// Peform any shutdown operations here...
	WorldPhysic->FreeAllBodies();

	// purge all strings
	g_GameStringPool.FreeAll();
	g_GameStringPool.MakeEmptyString();
}

void ServerActivate( edict_t *pEdictList, int edictCount, int clientMax )
{
	int				i;
	CBaseEntity		*pClass;

//	ALERT( at_console, "ServerActivate()\n" );

	// Every call to ServerActivate should be matched by a call to ServerDeactivate
	g_serveractive = 1;

	LinkUserMessages ();

	// Clients have not been initialized yet
	for ( i = 0; i < edictCount; i++ )
	{
		if ( pEdictList[i].free )
			continue;
		
		// Clients aren't necessarily initialized until ClientPutInServer()
		// diffusion - https://github.com/ValveSoftware/halflife/issues/3282
		if ( (i > 0 && i <= clientMax) || !pEdictList[i].pvPrivateData )
			continue;

		pClass = CBaseEntity::Instance( &pEdictList[i] );
		// Activate this entity if it's got a class & isn't dormant
		if ( pClass && !(pClass->pev->flags & FL_DORMANT) )
			pClass->Activate();
		else
			ALERT( at_console, "Can't instance %s\n", STRING(pEdictList[i].v.classname) );
	}

	if( WorldGraph.m_fGraphPresent && !WorldGraph.m_fGraphPointersSet )
	{
		if( !WorldGraph.FSetGraphPointers() )
			ALERT( at_console, "**Graph pointers were not set!\n" );
		else
			ALERT( at_console, "**Graph Pointers Set!\n" );
	}
}


/*
================
PlayerPreThink

Called every frame before physics are run
================
*/
void PlayerPreThink( edict_t *pEntity )
{
//	ALERT( at_console, "PreThink( %g, frametime %g )\n", gpGlobals->time, gpGlobals->frametime );

	entvars_t *pev = &pEntity->v;
	CBasePlayer *pPlayer = (CBasePlayer *)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->PreThink( );
}

/*
================
PlayerPostThink

Called every frame after physics are run
================
*/
void PlayerPostThink( edict_t *pEntity )
{
//	ALERT( at_console, "PostThink( %g, frametime %g )\n", gpGlobals->time, gpGlobals->frametime );

	entvars_t *pev = &pEntity->v;
	CBasePlayer *pPlayer = (CBasePlayer *)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->PostThink( );
}



void ParmsNewLevel( void )
{
}


void ParmsChangeLevel( void )
{
	// retrieve the pointer to the save data
	SAVERESTOREDATA *pSaveData = (SAVERESTOREDATA *)gpGlobals->pSaveData;

	if ( pSaveData )
		pSaveData->connectionCount = BuildChangeList( pSaveData->levelList, MAX_LEVEL_CONNECTIONS );
}


//
// GLOBALS ASSUMED SET:  g_ulFrameCount
//
/*
void StartFrame( void )
{
//	ALERT( at_console, "SV_Physics( %g, frametime %g )\n", gpGlobals->time, gpGlobals->frametime );

	if ( g_pGameRules )
		g_pGameRules->Think();

	if ( g_fGameOver )
		return;

	gpGlobals->teamplay = teamplay.value;
	g_ulFrameCount++;
}*/

void StartFrame( void )
{
	// START BOT

	static int index, i;

	// loop through all the players...
	
	if( gpGlobals->maxClients > 1 ) // diffusion - not in singleplayer!
	{
		for( i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBaseEntity *pPlayer;

			pPlayer = UTIL_PlayerByIndex( i );

			if( !pPlayer )  // if invalid then continue with next index...
				continue;

			// check if this is a FAKECLIENT (i.e. is it a bot?)
			if( FBitSet( pPlayer->pev->flags, FL_FAKECLIENT ) )
			{
				CBot *pBot = (CBot *)pPlayer;

				// call the think function for the bot...
				pBot->BotThink();
			}
		}
	}
	// END BOT

	if( g_pGameRules )
		g_pGameRules->Think();

	if( g_fGameOver )
		return;

	gpGlobals->teamplay = teamplay.value;
	g_ulFrameCount++;
}

void EndFrame( void )
{
	if ( g_fGameOver )
		return;

	// update physic step
	WorldPhysic->Update( gpGlobals->frametime );
}

void ClientPrecache( void )
{
	// setup precaches always needed
	PRECACHE_SOUND("player/sprayer.wav");			// spray paint sound for PreAlpha
	
	PRECACHE_SOUND("player/fallpain1.wav");		
	PRECACHE_SOUND("player/fallpain2.wav");
	
	PRECACHE_SOUND("footsteps/step1.wav");		// walk on concrete
	PRECACHE_SOUND("footsteps/step2.wav");
	PRECACHE_SOUND("footsteps/step3.wav");
	PRECACHE_SOUND("footsteps/step4.wav");
	PRECACHE_SOUND("footsteps/step5.wav");
	PRECACHE_SOUND("footsteps/step6.wav");

	PRECACHE_SOUND("common/npc_step1.wav");		// NPC walk on concrete
	PRECACHE_SOUND("common/npc_step2.wav");
	PRECACHE_SOUND("common/npc_step3.wav");
	PRECACHE_SOUND("common/npc_step4.wav");

	PRECACHE_SOUND("footsteps/metal1.wav");		// walk on metal
	PRECACHE_SOUND("footsteps/metal2.wav");
	PRECACHE_SOUND("footsteps/metal3.wav");
	PRECACHE_SOUND("footsteps/metal4.wav");

	PRECACHE_SOUND("footsteps/dirt1.wav");		// walk on dirt
	PRECACHE_SOUND("footsteps/dirt2.wav");
	PRECACHE_SOUND("footsteps/dirt3.wav");
	PRECACHE_SOUND("footsteps/dirt4.wav");

	PRECACHE_SOUND("footsteps/duct1.wav");		// walk in duct
	PRECACHE_SOUND("footsteps/duct2.wav");
	PRECACHE_SOUND("footsteps/duct3.wav");
	PRECACHE_SOUND("footsteps/duct4.wav");

	PRECACHE_SOUND("footsteps/grate1.wav");		// walk on grate
	PRECACHE_SOUND("footsteps/grate2.wav");
	PRECACHE_SOUND("footsteps/grate3.wav");
	PRECACHE_SOUND("footsteps/grate4.wav");

	PRECACHE_SOUND("footsteps/slosh1.wav");		// walk in shallow water
	PRECACHE_SOUND("footsteps/slosh2.wav");
	PRECACHE_SOUND("footsteps/slosh3.wav");
	PRECACHE_SOUND("footsteps/slosh4.wav");

	PRECACHE_SOUND("footsteps/tile1.wav");		// walk on tile
	PRECACHE_SOUND("footsteps/tile2.wav");
	PRECACHE_SOUND("footsteps/tile3.wav");
	PRECACHE_SOUND("footsteps/tile4.wav");
	PRECACHE_SOUND("footsteps/tile5.wav");

	PRECACHE_SOUND("player/pl_swim1.wav");		// breathe bubbles
	PRECACHE_SOUND("player/pl_swim2.wav");
	PRECACHE_SOUND("player/pl_swim3.wav");
	PRECACHE_SOUND("player/pl_swim4.wav");

	PRECACHE_SOUND("footsteps/ladder1.wav");	// climb ladder rung
	PRECACHE_SOUND("footsteps/ladder2.wav");
	PRECACHE_SOUND("footsteps/ladder3.wav");
	PRECACHE_SOUND("footsteps/ladder4.wav");

	PRECACHE_SOUND("footsteps/wade1.wav");		// wade in water
	PRECACHE_SOUND("footsteps/wade2.wav");
	PRECACHE_SOUND("footsteps/wade3.wav");
	PRECACHE_SOUND("footsteps/wade4.wav");
//------------------------------------------------------------------------------------
	// diffusion additions - more step sounds

	PRECACHE_SOUND("player/water_jump_hard1.wav");
	PRECACHE_SOUND("player/water_jump_hard2.wav");
	PRECACHE_SOUND("player/water_jump1.wav");
	PRECACHE_SOUND("player/water_jump2.wav");
	PRECACHE_SOUND("player/water_jump3.wav");
	PRECACHE_SOUND("player/water_out1.wav");
	PRECACHE_SOUND("player/water_out2.wav");
	PRECACHE_SOUND("player/water_out3.wav");
//	PRECACHE_SOUND( "player/fling_whoosh.wav" );

	PRECACHE_SOUND("footsteps/wood1.wav");
	PRECACHE_SOUND("footsteps/wood2.wav");
	PRECACHE_SOUND("footsteps/wood3.wav");
	PRECACHE_SOUND("footsteps/wood4.wav");
	PRECACHE_SOUND("footsteps/wood5.wav");
	PRECACHE_SOUND("footsteps/wood6.wav");

	PRECACHE_SOUND("footsteps/snow1.wav");
	PRECACHE_SOUND("footsteps/snow2.wav");
	PRECACHE_SOUND("footsteps/snow3.wav");
	PRECACHE_SOUND("footsteps/snow4.wav");
	PRECACHE_SOUND("footsteps/snow5.wav");
	PRECACHE_SOUND("footsteps/snow6.wav");

	PRECACHE_SOUND("footsteps/carpet1.wav");
	PRECACHE_SOUND("footsteps/carpet2.wav");
	PRECACHE_SOUND("footsteps/carpet3.wav");
	PRECACHE_SOUND("footsteps/carpet4.wav");
	PRECACHE_SOUND("footsteps/carpet5.wav");
	PRECACHE_SOUND("footsteps/carpet6.wav");

	PRECACHE_SOUND("footsteps/grass1.wav");
	PRECACHE_SOUND("footsteps/grass2.wav");
	PRECACHE_SOUND("footsteps/grass3.wav");
	PRECACHE_SOUND("footsteps/grass4.wav");
	PRECACHE_SOUND("footsteps/grass5.wav");
	PRECACHE_SOUND("footsteps/grass6.wav");

	PRECACHE_SOUND("footsteps/gravel1.wav");
	PRECACHE_SOUND("footsteps/gravel2.wav");
	PRECACHE_SOUND("footsteps/gravel3.wav");
	PRECACHE_SOUND("footsteps/gravel4.wav");
	PRECACHE_SOUND("footsteps/gravel5.wav");
	PRECACHE_SOUND("footsteps/gravel6.wav");

	PRECACHE_SOUND("footsteps/glass1.wav");
	PRECACHE_SOUND("footsteps/glass2.wav");
	PRECACHE_SOUND("footsteps/glass3.wav");
	PRECACHE_SOUND("footsteps/glass4.wav");
	PRECACHE_SOUND("footsteps/glass5.wav");
	PRECACHE_SOUND("footsteps/glass6.wav");

	PRECACHE_SOUND( "footsteps/ladder_wood1.wav" );
	PRECACHE_SOUND( "footsteps/ladder_wood2.wav" );
	PRECACHE_SOUND( "footsteps/ladder_wood3.wav" );
	PRECACHE_SOUND( "footsteps/ladder_wood4.wav" );

	// new hit sounds
	PRECACHE_SOUND("hits/glass1.wav");
	PRECACHE_SOUND("hits/glass2.wav");
	PRECACHE_SOUND("hits/glass3.wav");
	PRECACHE_SOUND("hits/glass4.wav");

	PRECACHE_SOUND("hits/concrete1.wav");
	PRECACHE_SOUND("hits/concrete2.wav");
	PRECACHE_SOUND("hits/concrete3.wav");
	PRECACHE_SOUND("hits/concrete4.wav");

	PRECACHE_SOUND("hits/metal1.wav");
	PRECACHE_SOUND("hits/metal2.wav");
	PRECACHE_SOUND("hits/metal3.wav");
	PRECACHE_SOUND("hits/metal4.wav");

	PRECACHE_SOUND("hits/dirt1.wav");
	PRECACHE_SOUND("hits/dirt2.wav");
	PRECACHE_SOUND("hits/dirt3.wav");
	PRECACHE_SOUND("hits/dirt4.wav");

	PRECACHE_SOUND("hits/wood1.wav");
	PRECACHE_SOUND("hits/wood2.wav");
	PRECACHE_SOUND("hits/wood3.wav");
	PRECACHE_SOUND("hits/wood4.wav");

	PRECACHE_SOUND("player/dash.wav");
	PRECACHE_SOUND( "player/dash_underwater.wav" );
	PRECACHE_SOUND("player/stamina_low.wav");
	PRECACHE_SOUND("player/wallslide.wav");
	PRECACHE_SOUND("player/electroblast.wav");
	PRECACHE_SOUND( "player/shield_on.wav" );
	PRECACHE_SOUND( "weapons/lowammo.wav" );

//------------------------------------------------------------------------------------	
	
//	PRECACHE_SOUND("debris/wood1.wav");			// hit wood texture
//	PRECACHE_SOUND("debris/wood2.wav");
//	PRECACHE_SOUND("debris/wood3.wav");

	PRECACHE_SOUND("plats/train_use1.wav");		// use a train

	PRECACHE_SOUND("buttons/spark5.wav");		// hit computer texture
	PRECACHE_SOUND("buttons/spark6.wav");
//	PRECACHE_SOUND("debris/glass1.wav");
//	PRECACHE_SOUND("debris/glass2.wav");
//	PRECACHE_SOUND("debris/glass3.wav");

	PRECACHE_SOUND( SOUND_FLASHLIGHT_ON );
	PRECACHE_SOUND( SOUND_FLASHLIGHT_OFF );

// player gib sounds
	PRECACHE_SOUND("common/bodysplat1.wav");
	PRECACHE_SOUND("common/bodysplat2.wav");
	PRECACHE_SOUND("common/bodysplat3.wav");

// player pain sounds
	PRECACHE_SOUND("player/pl_pain2.wav");
	PRECACHE_SOUND("player/pl_pain4.wav");
	PRECACHE_SOUND("player/pl_pain5.wav");
	PRECACHE_SOUND("player/pl_pain6.wav");
	PRECACHE_SOUND("player/pl_pain7.wav");

	PRECACHE_MODEL("models/player.mdl");

	// hud sounds

	PRECACHE_SOUND("common/wpn_hudoff.wav");
	PRECACHE_SOUND("common/wpn_hudon.wav");
	PRECACHE_SOUND("common/wpn_moveselect.wav");
	PRECACHE_SOUND("common/wpn_select1.wav");
	PRECACHE_SOUND("common/wpn_select2.wav");
	PRECACHE_SOUND("common/wpn_select3.wav");
//	PRECACHE_SOUND("common/wpn_denyselect.wav");

	// use sound
	PRECACHE_SOUND("common/use.wav");
	PRECACHE_SOUND("common/use_no.wav");

	// underwater
	PRECACHE_SOUND("player/underwater.wav");

	// geiger sounds
	PRECACHE_SOUND("player/geiger6.wav");
	PRECACHE_SOUND("player/geiger5.wav");
	PRECACHE_SOUND("player/geiger4.wav");
	PRECACHE_SOUND("player/geiger3.wav");
	PRECACHE_SOUND("player/geiger2.wav");
	PRECACHE_SOUND("player/geiger1.wav");

	// misc sounds used by the game
	PRECACHE_SOUND("misc/achievement.wav");
	PRECACHE_SOUND( "misc/tutor.wav" );
	PRECACHE_SOUND( "misc/player_join.wav" );
	PRECACHE_SOUND( "misc/player_left.wav" );
	PRECACHE_SOUND( "misc/hitmarker.wav" );
	PRECACHE_SOUND( "misc/hitmarker_kill.wav" );

	if (giPrecacheGrunt)
		UTIL_PrecacheOther("monster_human_grunt");

	PRECACHE_MODEL( "sprites/muzzleflash1.spr" );
	PRECACHE_MODEL( "sprites/muzzleflash2.spr" );
	PRECACHE_MODEL( "sprites/muzzleflash3.spr" );
	PRECACHE_MODEL( "sprites/muzzleflash_alien1.spr" );
	PRECACHE_MODEL( "sprites/muzzleflash_alien2.spr" );
	PRECACHE_MODEL( "sprites/muzzleflash_ar_secondary.spr" );
	PRECACHE_MODEL( "sprites/glow01.spr" );

	PRECACHE_SOUND( "misc/code_push.wav" );

	PRECACHE_SOUND( "player/throw.wav" );

	PRECACHE_MODEL( "models/mechgibs.mdl" );

	PRECACHE_MODEL( "models/bonus.mdl" );
	PRECACHE_SOUND( "items/bonus_spawn.wav" );
	PRECACHE_SOUND( "items/bonus_pickup.wav" );

	// killing spree etc.
	PRECACHE_SOUND( "server/spree_killingspree.wav" );
	PRECACHE_SOUND( "server/spree_dominating.wav" );
	PRECACHE_SOUND( "server/spree_rampage.wav" );
	PRECACHE_SOUND( "server/spree_godlike.wav" );
	PRECACHE_SOUND( "server/spree_wickedsick.wav" );
	PRECACHE_SOUND(  "server/snd_headshot.wav" );
	PRECACHE_SOUND( "server/snd_spree_ended.wav" );
	PRECACHE_SOUND( "server/snd_doublekill.wav" );
	PRECACHE_SOUND( "server/snd_triplekill.wav" );
	PRECACHE_SOUND( "server/snd_multikill.wav" );
	PRECACHE_SOUND( "server/snd_megakill.wav" );
	PRECACHE_SOUND("server/snd_ultrakill.wav" );
	PRECACHE_SOUND( "server/snd_monsterkill.wav" );
}

/*
===============
GetGameDescription

Returns the descriptive name of this .dll.  E.g., Half-Life, or Team Fortress 2
===============
*/
const char *GetGameDescription()
{
	char		token[256];
	char		game[128];
	char		version[32];
	static char	text[128];
	
	char *afile = (char *)LOAD_FILE( "gameinfo.txt", NULL );
	char *pfile = afile;

	if( pfile )
	{
		Q_strncpy( game, "Diffusion", sizeof( game ));
		version[0] = '\0';

		while ( pfile )
		{
			pfile = COM_ParseFile( pfile, token );

			if( !Q_stricmp( token, "title" )) 
			{                                          
				pfile = COM_ParseFile( pfile, token );
				Q_strncpy( game, token, sizeof( game ));
			}
			else if( !Q_stricmp( token, "version" )) 
			{                                          
				pfile = COM_ParseFile(pfile, token);
				Q_strncpy( version, token, sizeof( version ));
			}
		}

		Q_snprintf( text, sizeof( text ), "%s %s", game, version );
		FREE_FILE( afile );
		return text;
	}

	return "Diffusion";
}

/*
================
Sys_Error

Engine is going to shut down, allows setting a breakpoint in game .dll to catch that occasion
================
*/
void Sys_Error( const char *error_string )
{
	// Default case, do nothing.  MOD AUTHORS:  Add code ( e.g., _asm { int 3 }; here to cause a breakpoint for debugging your game .dlls
}

/*
================
PlayerCustomization

A new player customization has been registered on the server
UNDONE:  This only sets the # of frames of the spray can logo
animation right now.
================
*/
void PlayerCustomization( edict_t *pEntity, customization_t *pCust )
{
	entvars_t *pev = &pEntity->v;
	CBasePlayer *pPlayer = (CBasePlayer *)GET_PRIVATE(pEntity);

	if (!pPlayer)
	{
		ALERT(at_console, "PlayerCustomization:  Couldn't get player!\n");
		return;
	}

	if (!pCust)
	{
		ALERT(at_console, "PlayerCustomization:  NULL customization!\n");
		return;
	}

	switch (pCust->resource.type)
	{
	case t_decal:
		pPlayer->SetCustomDecalFrames(pCust->nUserData2); // Second int is max # of frames.
		break;
	case t_sound:
	case t_skin:
	case t_model:
		// Ignore for now.
		break;
	default:
		ALERT(at_console, "PlayerCustomization:  Unknown customization type!\n");
		break;
	}
}

/*
================
SpectatorConnect

A spectator has joined the game
================
*/
void SpectatorConnect( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;
	CBaseSpectator *pPlayer = (CBaseSpectator *)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->SpectatorConnect( );
}

/*
================
SpectatorConnect

A spectator has left the game
================
*/
void SpectatorDisconnect( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;
	CBaseSpectator *pPlayer = (CBaseSpectator *)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->SpectatorDisconnect( );
}

/*
================
SpectatorConnect

A spectator has sent a usercmd
================
*/
void SpectatorThink( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;
	CBaseSpectator *pPlayer = (CBaseSpectator *)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->SpectatorThink( );
}

////////////////////////////////////////////////////////
// PAS and PVS routines for client messaging
//

/*
================
SetupVisibility

A client can have a separate "view entity" indicating that his/her view should depend on the origin of that
view entity.  If that's the case, then pViewEntity will be non-NULL and will be used.  Otherwise, the current
entity's origin is used.  Either is offset by the view_ofs to get the eye position.

From the eye position, we set up the PAS and PVS to use for filtering network messages to the client.  At this point, we could
 override the actual PAS or PVS values, or use a different origin.

NOTE:  Do not cache the values of pas and pvs, as they depend on reusable memory in the engine, they are only good for this one frame
================
*/
void SetupVisibility( edict_t *pViewEntity, edict_t *pClient, unsigned char **pvs, unsigned char **pas )
{
	Vector org;
	edict_t *pView = pClient;

	// Find the client's PVS
	if ( pViewEntity )
		pView = pViewEntity;

	if ( pClient->v.flags & FL_PROXY )
	{
		*pvs = NULL;	// the spectator proxy sees
		*pas = NULL;	// and hears everything
		return;
	}

	if( pView->v.effects & EF_MERGE_VISIBILITY )
		org = pView->v.origin;
	else
	{
		org = pView->v.origin + pView->v.view_ofs;
		if ( pView->v.flags & FL_DUCKING )
			org = org + ( VEC_HULL_MIN - VEC_DUCK_HULL_MIN );
	}

	*pvs = ENGINE_SET_PVS ( (float *)&org );
	*pas = ENGINE_SET_PAS ( (float *)&org );
}

#include "entity_state.h"

//===========================================================================
// IsBackCulled: if the entity is behind our back, we don't send it to client.
// Experimental!!! Function is copy-pasted from monster's FInViewCone.
//===========================================================================
bool IsBackCulled( CBasePlayer* pPlayer, CBaseEntity* e)
{
	if( !pPlayer || !e )
		return false;
	
	// how sv_fade_props work: 0 - disable all culling, 1 - enable front culling
	// any value above 1 enables backculling and equals minimum distance where entity will be culled
	if( sv_fade_props.value <= 1 )
		return false;

	// important entities must have this set, if we don't want them to be back-culled
	// skybox ents, monsters, players, projectiles, etc...
//	if( e->pev->flags2 & F_NOBACKCULL )
//		return false;

	// UPD: I decided to use only entities with fadedistance set.
	// if you want to enable solution above, make sure to uncomment some stuff
	// in env_sky ::Spawn function.
	if( e->pev->iuser4 <= 0 )
		return false;
	else if( e->pev->iuser4 > 0 && e->pev->iuser4 < 10 ) // don't bother calculating
		return true;

	int Distance, MinDistance;

	Distance = sv_fade_props.value; // must be at least this far from player

	int mdl = UTIL_GetModelType( e->pev->modelindex );

	// do not allow the distance to be less than this (huge scaled ents)
	// note: I add hacked 100 because it can still disappear at corners
	if( mdl == mod_brush )
		MinDistance = (e->pev->absmin - e->pev->absmax).Length() + 100;
	else if( mdl == mod_sprite ) // back-cull sprites right away
		MinDistance = 200; // ...but big sprites can disappear early?
	else if( mdl == mod_studio )
	{
		Vector modmins, modmaxs;
		UTIL_GetModelBounds( e->pev->modelindex, modmins, modmaxs );
		MinDistance = (e->pev->scale * ((modmaxs - modmins).Length() / 2.f)) + 100;
	}
	else
		MinDistance = e->pev->iuser4 / 4; // other ents with fadedistance

	// check entity bounds
	if( Distance < MinDistance )
		Distance = MinDistance;
	
	Vector vec2LOS;
	float flDot;

	Vector CenterOffset = (e->pev->mins + e->pev->maxs) / 2.f;
	Vector EntOrigin = e->GetAbsOrigin() + CenterOffset;

	// player is too close to entity.
	if( (int)(pPlayer->CameraOrigin - EntOrigin).Length() < Distance )
		return false;
	
	UTIL_MakeVectors( pPlayer->CameraAngles );

	vec2LOS = EntOrigin - pPlayer->CameraOrigin;
	vec2LOS = vec2LOS.Normalize();

	flDot = DotProduct( vec2LOS, gpGlobals->v_forward );
	
	// this value seems to work
	if( flDot < -0.1f )
		return true; // behind host's back
	else
		return false; // in front
}

//===========================================================================
// ComputeDistanceCulling: if the entity is far enough, we don't send it to client.
//===========================================================================
bool IsDistanceCulled( CBasePlayer *pPlayer, CBaseEntity *e )
{	
	if( !pPlayer || !e )
		return false;

	if( sv_fade_props.value <= 0 )
		return false;

	bool Force = (sv_force_fadedistance.value > 0); // only applied to entities which already have fadedistance set

	if( e->pev->iuser4 <= 0 ) // fade distance isn't set
		return false;
	else if( e->pev->iuser4 > 0 && e->pev->iuser4 < 10 ) // don't bother calculating
		return true;

	Vector CenterOffset = (e->pev->mins + e->pev->maxs) / 2.f;
	Vector EntOrigin = e->GetAbsOrigin() + CenterOffset;
	Vector HostOrigin = pPlayer->CameraOrigin;

	// calculate new, increased fade distance if we are zoomed
	// for FOV above 70 it is assumed that distance remains unchanged
	// !!!: same stuff is going on in R_ComputeFadingDistance on client
	float FOV = pPlayer->m_flFOV;
	if (FOV <= 0) FOV = 90;
	float FOVfactor = bound(30, FOV, 70) / 70;
	int FadeDistance;
	if( Force )
		FadeDistance = sv_force_fadedistance.value * (1 / FOVfactor);
	else
		FadeDistance = e->pev->iuser4 * (1 / FOVfactor);

	// renderamt is added so entity can be faded on client
	if( (int)(HostOrigin - EntOrigin).Length() > FadeDistance + ((e->pev->renderamt == 0) ? 255 : e->pev->renderamt) )
		return true;

	return false;
}

/*
AddToFullPack

Return 1 if the entity state has been filled in for the ent and the entity will be propagated to the client, 0 otherwise
--- state is the server maintained copy of the state info that is transmitted to the client
a MOD could alter values copied into state to send the "host" a different look for a particular entity update, etc.
--- e and ent are the entity that is being added to the update, if 1 is returned
--- host is the player's edict of the player whom we are sending the update to
--- player is 1 if the ent/e is a player and 0 otherwise
--- pSet is either the PAS or PVS that we previous set up.  We can use it to ask the engine to filter the entity against the PAS or PVS.
we could also use the pas/ pvs that we set in SetupVisibility, if we wanted to.  Caching the value is valid in that case, but still only for the current frame
*/
int AddToFullPack( struct entity_state_s *state, int e, edict_t *ent, edict_t *host, int hostflags, int player, unsigned char *pSet )
{
	int	i;

	// don't send if flagged for NODRAW and it's not the host getting the message
	if ( ( ent->v.effects & EF_NODRAW ) && ( ent != host ) )
		return 0;

	// Ignore ents without valid / visible models
	if ( !ent->v.modelindex || !STRING( ent->v.model ) )
		return 0;

	// Don't send spectators to other players
	if ( ( ent->v.flags & FL_SPECTATOR ) && ( ent != host ) )
		return 0;

	CBaseEntity *pEntity = CBaseEntity::Instance( ent );

	if( !pEntity )
		return 0;

	CBasePlayer *pPlayer = (CBasePlayer *)GET_PRIVATE( host );

	// diffusion - don't send faded entites' data to clients
	if( IsDistanceCulled( pPlayer, pEntity ) )
		return 0;

	// diffusion - don't send entites' data to clients if they are behind his back
	// very experimental feature, requires fadedistance to be set.
	if( IsBackCulled( pPlayer, pEntity ) )
		return 0;

	// diffusion - apply a flag to laser spot, will be using local origin on client
	if( FClassnameIs(pEntity,"laser_spot"))
	{
		if( pEntity->pev->owner == pPlayer->edict() )
		{
			if( !(pEntity->pev->effects & EF_MYLASERSPOT) )
				pEntity->pev->effects |= EF_MYLASERSPOT;
		}
	}

	// Ignore if not the host and not touching a PVS/PAS leaf
	// If pSet is NULL, then the test will always succeed and the entity will be added to the update
	// diffusion - skip PVS check if has an effect set - info_shader_params using it
	if ( ent != host )
	{
		if( !(ent->v.effects & EF_SKIPPVS) )
		{
			if( !ENGINE_CHECK_VISIBILITY( (const struct edict_s *)ent, pSet ) )
			{
				if( FBitSet( ent->v.effects, EF_PROJECTED_LIGHT ) && (ent->v.enemy != NULL) )
				{
					// projected light have second PVS point so we must check it too
					if( !ENGINE_CHECK_VISIBILITY( (const struct edict_s *)ent->v.enemy, pSet ) )
						return 0;
				}
				else
					return 0;
			}
		}
	}

	// Don't send entity to local client if the client says it's predicting the entity itself.
	if ( ent->v.flags & FL_SKIPLOCALHOST )
	{
		if ( ( hostflags & 1 ) && ( ent->v.owner == host ) )
			return 0;
	}
	
	if ( host->v.groupinfo )
	{
		UTIL_SetGroupTrace( host->v.groupinfo, GROUP_OP_AND );

		// Should always be set, of course
		if ( ent->v.groupinfo )
		{
			if ( g_groupop == GROUP_OP_AND )
			{
				if ( !(ent->v.groupinfo & host->v.groupinfo ) )
					return 0;
			}
			else if ( g_groupop == GROUP_OP_NAND )
			{
				if ( ent->v.groupinfo & host->v.groupinfo )
					return 0;
			}
		}

		UTIL_UnsetGroupTrace();
	}

	memset( state, 0, sizeof( *state ) );


	// Assign index so we can track this entity from frame to frame and
	// delta from it.
	state->number	  = e;
	state->entityType = ENTITY_NORMAL;
	
	// Flag custom entities.
	if ( ent->v.flags & FL_CUSTOMENTITY )
		state->entityType = ENTITY_BEAM;

	// 
	// Copy state data
	//

	// Round animtime to nearest millisecond
	state->animtime   = (int)(1000.0 * ent->v.animtime ) / 1000.0;

	if( state->entityType == ENTITY_BEAM )
	{
		CBeam *pBeam = (CBeam *)pEntity;
		state->origin = pBeam->GetAbsStartPos();
		state->angles = pBeam->GetAbsEndPos();
	}
	else
	{
		state->origin = pEntity->GetAbsOrigin();
		state->angles = pEntity->GetAbsAngles();
	}

	memcpy( state->mins, ent->v.mins, 3 * sizeof( float ) );
	memcpy( state->maxs, ent->v.maxs, 3 * sizeof( float ) );

	state->startpos = ent->v.startpos;
	state->endpos = ent->v.endpos;

	state->impacttime = ent->v.impacttime;
	state->starttime = ent->v.starttime;

	state->modelindex = ent->v.modelindex;
		
	state->frame      = ent->v.frame;

	state->skin       = ent->v.skin;
	state->effects    = ent->v.effects;

	// This non-player entity is being moved by the game .dll and not the physics simulation system
	//  make sure that we interpolate it's position on the client if it moves
	if( !player && ent->v.animtime && ent->v.velocity == g_vecZero && pEntity->m_hParent == NULL )
		state->eflags |= EFLAG_SLERP;

	if( FClassnameIs( &ent->v, "info_intermission" ))
		state->eflags |= EFLAG_INTERMISSION;

	state->scale	  = ent->v.scale;
	state->solid	  = ent->v.solid;
	state->colormap   = ent->v.colormap;

	state->movetype   = ent->v.movetype;
	state->sequence   = ent->v.sequence;
	state->framerate  = ent->v.framerate;
	state->body       = ent->v.body;

	for (i = 0; i < 4; i++)
		state->controller[i] = ent->v.controller[i];

	for (i = 0; i < 2; i++)
		state->blending[i]   = ent->v.blending[i];

	state->rendermode = ent->v.rendermode;
	state->renderamt = ent->v.renderamt;

	state->renderfx      = ent->v.renderfx;
	state->rendercolor.r = ent->v.rendercolor.x;
	state->rendercolor.g = ent->v.rendercolor.y;
	state->rendercolor.b = ent->v.rendercolor.z;

	state->fuser1 = ent->v.fuser1;
	state->fuser2 = ent->v.fuser2;
	state->fuser3 = ent->v.fuser3;
	state->fuser4 = ent->v.fuser4;
	state->iuser1 = ent->v.iuser1;
	state->iuser2 = ent->v.iuser2;
	state->iuser3 = ent->v.iuser3; // vertexlight cachenum
	state->iuser4 = ent->v.iuser4; // fadedistance (all entities)

	// diffusionposeparameters - I don't need this...for now
	// doesn't look like it's used properly anyway

	// copy poseparams across network
/*	state->vuser1.x	= pEntity->m_flPoseParameter[0];
	state->vuser1.y	= pEntity->m_flPoseParameter[1];
	state->vuser1.z	= pEntity->m_flPoseParameter[2];
	state->vuser2.x	= pEntity->m_flPoseParameter[3];
	state->vuser2.y	= pEntity->m_flPoseParameter[4];
	state->vuser2.z	= pEntity->m_flPoseParameter[5];
	state->vuser3.x	= pEntity->m_flPoseParameter[6];
	state->vuser3.y	= pEntity->m_flPoseParameter[7];
	state->vuser3.z	= pEntity->m_flPoseParameter[8];
	state->vuser4.x	= pEntity->m_flPoseParameter[9];
	state->vuser4.y	= pEntity->m_flPoseParameter[10];
	state->vuser4.z	= pEntity->m_flPoseParameter[11];
*/
	state->vuser1.x = ent->v.vuser1.x;
	state->vuser1.y = ent->v.vuser1.y;
	state->vuser1.z = ent->v.vuser1.z;
	state->vuser2.x = ent->v.vuser2.x;
	state->vuser2.y = ent->v.vuser2.y;
	state->vuser2.z = ent->v.vuser2.z;
	state->vuser3.x = ent->v.vuser3.x;
	state->vuser3.y = ent->v.vuser3.y;
	state->vuser3.x = ent->v.vuser3.x;
	state->vuser4.y = ent->v.vuser4.y;
	state->vuser4.z = ent->v.vuser4.z;
	state->vuser4.z = ent->v.vuser4.z;

	state->aiment = 0;
	if ( ent->v.aiment )
		state->aiment = ENTINDEX( ent->v.aiment );

	state->owner = 0;
	if ( ent->v.owner )
	{
		int owner = ENTINDEX( ent->v.owner );
		
		// Only care if owned by a player
		if ( owner >= 1 && owner <= gpGlobals->maxClients )
			state->owner = owner;	
	}

	state->onground = -1;
	if ( FBitSet( ent->v.flags, FL_ONGROUND ) && ent->v.groundentity != NULL )
		state->onground = ENTINDEX( ent->v.groundentity );

	// HACK:  Somewhat...
	// Class is overridden for non-players to signify a breakable glass object ( sort of a class? )
	if ( !player )
		state->playerclass  = ent->v.playerclass;

	// Special stuff for players only
	if ( player )
	{
		memcpy( state->basevelocity, ent->v.basevelocity, 3 * sizeof( float ) );

		state->weaponmodel  = MODEL_INDEX( STRING( ent->v.weaponmodel ) );
		state->gaitsequence = ent->v.gaitsequence;
		state->spectator = ent->v.flags & FL_SPECTATOR;
		state->friction     = ent->v.friction;
		state->fuser1 = ent->v.fuser1;	// gaitframe
		state->gravity      = ent->v.gravity;
//		state->team			= ent->v.team;	
		state->usehull      = ( ent->v.flags & FL_DUCKING ) ? 1 : 0;
		state->health		= ent->v.health;
	}

	state->weaponmodel  = MODEL_INDEX( STRING( ent->v.weaponmodel ) );
	state->team	= ent->v.team;

	return 1;
}

// defaults for clientinfo messages
#define	DEFAULT_VIEWHEIGHT	28

/*
===================
CreateBaseline

Creates baselines used for network encoding, especially for player data since players are not spawned until connect time.
===================
*/
void CreateBaseline( int player, int eindex, struct entity_state_s *baseline, struct edict_s *entity, int playermodelindex, vec3_t player_mins, vec3_t player_maxs )
{
	CBaseEntity *pEntity = CBaseEntity::Instance( entity );

	baseline->origin		= entity->v.origin;
	baseline->angles		= entity->v.angles;
	baseline->frame		= entity->v.frame;
	baseline->skin		= (short)entity->v.skin;
	baseline->body		= entity->v.body;

	// render information
	baseline->rendermode	= (byte)entity->v.rendermode;
	baseline->renderamt		= (byte)entity->v.renderamt;
	baseline->rendercolor.r	= (byte)entity->v.rendercolor.x;
	baseline->rendercolor.g	= (byte)entity->v.rendercolor.y;
	baseline->rendercolor.b	= (byte)entity->v.rendercolor.z;
	baseline->renderfx		= (byte)entity->v.renderfx;

	if ( player )
	{
		baseline->mins		= player_mins;
		baseline->maxs		= player_maxs;

		baseline->colormap		= eindex;
		baseline->modelindex	= playermodelindex;
		baseline->friction		= 1.0;
		baseline->movetype		= MOVETYPE_WALK;

		baseline->scale		= entity->v.scale;
		baseline->solid		= SOLID_SLIDEBOX;
		baseline->framerate		= 1.0;
		baseline->gravity		= 1.0;
		baseline->health = entity->v.health;
	}
	else
	{
		baseline->mins		= entity->v.mins;
		baseline->maxs		= entity->v.maxs;

		baseline->sequence		= entity->v.sequence;
		baseline->colormap		= entity->v.colormap;
		baseline->modelindex	= entity->v.modelindex;
		baseline->movetype		= entity->v.movetype;
		if( playermodelindex == 0 ) // diffusion - g-cont says "can't send effects field to baseline from start"
			baseline->effects = entity->v.effects;
		

		baseline->scale		= entity->v.scale;
		baseline->solid		= entity->v.solid;
		baseline->framerate		= entity->v.framerate;
		baseline->gravity		= entity->v.gravity;
		baseline->startpos		= entity->v.startpos; // xform
		baseline->endpos		= entity->v.endpos; // misc
		baseline->iuser1		= entity->v.iuser1;	// flags
		baseline->iuser2		= entity->v.iuser2;	// flags
		baseline->iuser3		= entity->v.iuser3;	// vertexlight cachenum
		baseline->iuser4		= entity->v.iuser4; // fadedistance

		if( pEntity )
		{
			// diffusionposeparameters - disabled this for now
		/*
			baseline->vuser1.x	= pEntity->m_flPoseParameter[0];
			baseline->vuser1.y	= pEntity->m_flPoseParameter[1];
			baseline->vuser1.z	= pEntity->m_flPoseParameter[2];
			baseline->vuser2.x	= pEntity->m_flPoseParameter[3];
			baseline->vuser2.y	= pEntity->m_flPoseParameter[4];
			baseline->vuser2.z	= pEntity->m_flPoseParameter[5];
			baseline->vuser3.x	= pEntity->m_flPoseParameter[6];
			baseline->vuser3.y	= pEntity->m_flPoseParameter[7];
			baseline->vuser3.z	= pEntity->m_flPoseParameter[8];
			baseline->vuser4.x	= pEntity->m_flPoseParameter[9];
			baseline->vuser4.y	= pEntity->m_flPoseParameter[10];
			baseline->vuser4.z	= pEntity->m_flPoseParameter[11];
		*/	
			baseline->vuser1.x = entity->v.vuser1.x;
			baseline->vuser1.y = entity->v.vuser1.y;
			baseline->vuser1.z = entity->v.vuser1.z;
			baseline->vuser2.x = entity->v.vuser2.x;
			baseline->vuser2.y = entity->v.vuser2.y;
			baseline->vuser2.z = entity->v.vuser2.z;
			baseline->vuser3.x = entity->v.vuser3.x;
			baseline->vuser3.y = entity->v.vuser3.y;
			baseline->vuser3.z = entity->v.vuser3.z;
			baseline->vuser4.x = entity->v.vuser4.x;
			baseline->vuser4.y = entity->v.vuser4.y;
			baseline->vuser4.z = entity->v.vuser4.z;
		}
	}
}

typedef struct
{
	char name[32];
	int	 field;
} entity_field_alias_t;

#define FIELD_ORIGIN0			0
#define FIELD_ORIGIN1			1
#define FIELD_ORIGIN2			2
#define FIELD_ANGLES0			3
#define FIELD_ANGLES1			4
#define FIELD_ANGLES2			5

static entity_field_alias_t entity_field_alias[]=
{
	{ "origin[0]",			0 },
	{ "origin[1]",			0 },
	{ "origin[2]",			0 },
	{ "angles[0]",			0 },
	{ "angles[1]",			0 },
	{ "angles[2]",			0 },
};

void Entity_FieldInit( struct delta_s *pFields )
{
	entity_field_alias[ FIELD_ORIGIN0 ].field		= DELTA_FINDFIELD( pFields, entity_field_alias[ FIELD_ORIGIN0 ].name );
	entity_field_alias[ FIELD_ORIGIN1 ].field		= DELTA_FINDFIELD( pFields, entity_field_alias[ FIELD_ORIGIN1 ].name );
	entity_field_alias[ FIELD_ORIGIN2 ].field		= DELTA_FINDFIELD( pFields, entity_field_alias[ FIELD_ORIGIN2 ].name );
	entity_field_alias[ FIELD_ANGLES0 ].field		= DELTA_FINDFIELD( pFields, entity_field_alias[ FIELD_ANGLES0 ].name );
	entity_field_alias[ FIELD_ANGLES1 ].field		= DELTA_FINDFIELD( pFields, entity_field_alias[ FIELD_ANGLES1 ].name );
	entity_field_alias[ FIELD_ANGLES2 ].field		= DELTA_FINDFIELD( pFields, entity_field_alias[ FIELD_ANGLES2 ].name );
}

/*
==================
Entity_Encode

Callback for sending entity_state_t info over network. 
==================
*/
void Entity_Encode( struct delta_s *pFields, const unsigned char *from, const unsigned char *to )
{
	entity_state_t *f, *t;
	int localplayer = 0;
	static int initialized = 0;

	if ( !initialized )
	{
		Entity_FieldInit( pFields );
		initialized = 1;
	}

	f = (entity_state_t *)from;
	t = (entity_state_t *)to;

	// Never send origin to local player, it's sent with more resolution in clientdata_t structure
	localplayer =  ( t->number - 1 ) == ENGINE_CURRENT_PLAYER();
	if ( localplayer )
	{
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN2 ].field );
	}

	if ( ( t->impacttime != 0 ) && ( t->starttime != 0 ) )
	{
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN2 ].field );

		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ANGLES0 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ANGLES1 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ANGLES2 ].field );
	}

	if ( ( t->movetype == MOVETYPE_FOLLOW ) &&
		 ( t->aiment != 0 ) )
	{
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN2 ].field );
	}
	else if ( t->aiment != f->aiment )
	{
		DELTA_SETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_SETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_SETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN2 ].field );
	}
}

static entity_field_alias_t player_field_alias[]=
{
	{ "origin[0]",			0 },
	{ "origin[1]",			0 },
	{ "origin[2]",			0 },
};

void Player_FieldInit( struct delta_s *pFields )
{
	player_field_alias[ FIELD_ORIGIN0 ].field		= DELTA_FINDFIELD( pFields, player_field_alias[ FIELD_ORIGIN0 ].name );
	player_field_alias[ FIELD_ORIGIN1 ].field		= DELTA_FINDFIELD( pFields, player_field_alias[ FIELD_ORIGIN1 ].name );
	player_field_alias[ FIELD_ORIGIN2 ].field		= DELTA_FINDFIELD( pFields, player_field_alias[ FIELD_ORIGIN2 ].name );
}

/*
==================
Player_Encode

Callback for sending entity_state_t for players info over network. 
==================
*/
void Player_Encode( struct delta_s *pFields, const unsigned char *from, const unsigned char *to )
{
	entity_state_t *f, *t;
	int localplayer = 0;
	static int initialized = 0;

	if ( !initialized )
	{
		Player_FieldInit( pFields );
		initialized = 1;
	}

	f = (entity_state_t *)from;
	t = (entity_state_t *)to;

	// Never send origin to local player, it's sent with more resolution in clientdata_t structure
	localplayer =  ( t->number - 1 ) == ENGINE_CURRENT_PLAYER();
	if ( localplayer )
	{
		DELTA_UNSETBYINDEX( pFields, player_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_UNSETBYINDEX( pFields, player_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_UNSETBYINDEX( pFields, player_field_alias[ FIELD_ORIGIN2 ].field );
	}

	if ( ( t->movetype == MOVETYPE_FOLLOW ) &&
		 ( t->aiment != 0 ) )
	{
		DELTA_UNSETBYINDEX( pFields, player_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_UNSETBYINDEX( pFields, player_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_UNSETBYINDEX( pFields, player_field_alias[ FIELD_ORIGIN2 ].field );
	}
	else if ( t->aiment != f->aiment )
	{
		DELTA_SETBYINDEX( pFields, player_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_SETBYINDEX( pFields, player_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_SETBYINDEX( pFields, player_field_alias[ FIELD_ORIGIN2 ].field );
	}
}

#define CUSTOMFIELD_ORIGIN0			0
#define CUSTOMFIELD_ORIGIN1			1
#define CUSTOMFIELD_ORIGIN2			2
#define CUSTOMFIELD_ANGLES0			3
#define CUSTOMFIELD_ANGLES1			4
#define CUSTOMFIELD_ANGLES2			5
#define CUSTOMFIELD_SKIN			6
#define CUSTOMFIELD_SEQUENCE		7
#define CUSTOMFIELD_ANIMTIME		8

entity_field_alias_t custom_entity_field_alias[]=
{
	{ "origin[0]",			0 },
	{ "origin[1]",			0 },
	{ "origin[2]",			0 },
	{ "angles[0]",			0 },
	{ "angles[1]",			0 },
	{ "angles[2]",			0 },
	{ "skin",				0 },
	{ "sequence",			0 },
	{ "animtime",			0 },
};

void Custom_Entity_FieldInit( struct delta_s *pFields )
{
	custom_entity_field_alias[ CUSTOMFIELD_ORIGIN0 ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ORIGIN0 ].name );
	custom_entity_field_alias[ CUSTOMFIELD_ORIGIN1 ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ORIGIN1 ].name );
	custom_entity_field_alias[ CUSTOMFIELD_ORIGIN2 ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ORIGIN2 ].name );
	custom_entity_field_alias[ CUSTOMFIELD_ANGLES0 ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANGLES0 ].name );
	custom_entity_field_alias[ CUSTOMFIELD_ANGLES1 ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANGLES1 ].name );
	custom_entity_field_alias[ CUSTOMFIELD_ANGLES2 ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANGLES2 ].name );
	custom_entity_field_alias[ CUSTOMFIELD_SKIN ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_SKIN ].name );
	custom_entity_field_alias[ CUSTOMFIELD_SEQUENCE ].field= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_SEQUENCE ].name );
	custom_entity_field_alias[ CUSTOMFIELD_ANIMTIME ].field= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANIMTIME ].name );
}

/*
==================
Custom_Encode

Callback for sending entity_state_t info ( for custom entities ) over network. 
==================
*/
void Custom_Encode( struct delta_s *pFields, const unsigned char *from, const unsigned char *to )
{
	entity_state_t *f, *t;
	int beamType;
	static int initialized = 0;

	if ( !initialized )
	{
		Custom_Entity_FieldInit( pFields );
		initialized = 1;
	}

	f = (entity_state_t *)from;
	t = (entity_state_t *)to;

	beamType = t->rendermode & 0x0f;
		
	if ( beamType != BEAM_POINTS && beamType != BEAM_ENTPOINT )
	{
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ORIGIN0 ].field );
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ORIGIN1 ].field );
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ORIGIN2 ].field );
	}

	if ( beamType != BEAM_POINTS )
	{
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANGLES0 ].field );
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANGLES1 ].field );
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANGLES2 ].field );
	}

	if ( beamType != BEAM_ENTS && beamType != BEAM_ENTPOINT )
	{
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_SKIN ].field );
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_SEQUENCE ].field );
	}

	// animtime is compared by rounding first
	// see if we really shouldn't actually send it
	if ( (int)f->animtime == (int)t->animtime )
	{
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANIMTIME ].field );
	}
}

/*
=================
RegisterEncoders

Allows game .dll to override network encoding of certain types of entities and tweak values, etc.
=================
*/
void RegisterEncoders( void )
{
	DELTA_ADDENCODER( "Entity_Encode", Entity_Encode );
	DELTA_ADDENCODER( "Custom_Encode", Custom_Encode );
	DELTA_ADDENCODER( "Player_Encode", Player_Encode );
}

int GetWeaponData( struct edict_s *player, struct weapon_data_s *info )
{
	memset( info, 0, 32 * sizeof( weapon_data_t ) );

	return 1;
}

/*
=================
UpdateClientData

Data sent to current client only
engine sets cd to 0 before calling.
=================
*/
void UpdateClientData ( const struct edict_s *ent, int sendweapons, struct clientdata_s *cd )
{
	if (!ent || !ent->pvPrivateData)
		return;
	entvars_t* pev = (entvars_t*)&ent->v;
	CBasePlayer* pl = dynamic_cast<CBasePlayer*>(CBasePlayer::Instance(pev));
	entvars_t* pevOrg = NULL;

	// if user is spectating different player in First person, override some vars
	if (pl && pl->pev->iuser1 == OBS_IN_EYE)
	{
		if (pl->m_hObserverTarget)
		{
			pevOrg = pev;
			pev = pl->m_hObserverTarget->pev;
			pl = dynamic_cast<CBasePlayer*>(CBasePlayer::Instance(pev));
		}
	}
	
	cd->flags			= ent->v.flags;
//	cd->flags2 = ent->v.flags2;
	cd->health			= ent->v.health;

	cd->viewmodel		= MODEL_INDEX( STRING( ent->v.viewmodel ) );

	cd->waterlevel		= ent->v.waterlevel;
	cd->watertype		= ent->v.watertype;
	cd->weapons		= 0; // not used

	// Vectors
	cd->origin			= ent->v.origin;
	cd->velocity		= ent->v.velocity;
	cd->view_ofs		= ent->v.view_ofs;
	cd->punchangle		= ent->v.punchangle;

	cd->bInDuck			= ent->v.bInDuck;
	cd->flTimeStepSound = ent->v.flTimeStepSound;
	cd->flDuckTime		= ent->v.flDuckTime;
	cd->flSwimTime		= ent->v.flSwimTime;
	cd->waterjumptime	= ent->v.teleport_time;

	strcpy( cd->physinfo, ENGINE_GETPHYSINFO( ent ) );

	cd->maxspeed		= ent->v.maxspeed;
	cd->fov				= ent->v.fov;
	cd->weaponanim		= ent->v.weaponanim;

	cd->pushmsec		= ent->v.pushmsec;

	//Spectator mode
	if (pevOrg != NULL)
	{
		// don't use spec vars from chased player
		cd->iuser1 = pevOrg->iuser1;
		cd->iuser2 = pevOrg->iuser2;
	}
	else
	{
		cd->iuser1 = pev->iuser1;
		cd->iuser2 = pev->iuser2;
	}
}

//======================================================================================================
// CmdStart: We're about to run this usercmd for the specified player.  We can set up groupinfo and masking here, etc.
// This is the time to examine the usercmd for anything extra.  This call happens even if think does not.
//======================================================================================================
void CmdStart( const edict_t *player, const struct usercmd_s *cmd, unsigned int random_seed )
{
	entvars_t *pev = (entvars_t *)&player->v;
	CBasePlayer *pl = ( CBasePlayer *) CBasePlayer::Instance( pev );

	if( !pl )
		return;

	if ( pl->pev->groupinfo != 0 )
		UTIL_SetGroupTrace( pl->pev->groupinfo, GROUP_OP_AND );

	pl->random_seed = random_seed;
}

/*
=================
CmdEnd

Each cmdstart is exactly matched with a cmd end, clean up any group trace flags, etc. here
=================
*/
void CmdEnd ( const edict_t *player )
{
	entvars_t *pev = (entvars_t *)&player->v;
	CBasePlayer *pl = ( CBasePlayer *) CBasePlayer::Instance( pev );

	if( !pl )
		return;

	if ( pl->pev->groupinfo != 0 )
		UTIL_UnsetGroupTrace();
}

/*
================================
ConnectionlessPacket

 Return 1 if the packet is valid.  Set response_buffer_size if you want to send a response packet.  Incoming, it holds the max
  size of the response_buffer, so you must zero it out if you choose not to respond.
================================
*/
int	ConnectionlessPacket( const struct netadr_s *net_from, const char *args, char *response_buffer, int *response_buffer_size )
{
	// Parse stuff from args
	int max_buffer_size = *response_buffer_size;

	// Zero it out since we aren't going to respond.
	// If we wanted to response, we'd write data into response_buffer
	*response_buffer_size = 0;

	// Since we don't listen for anything here, just respond that it's a bogus message
	// If we didn't reject the message, we'd return 1 for success instead.
	return 0;
}

/*
================================
GetHullBounds

  Engine calls this to enumerate player collision hulls, for prediction.  Return 0 if the hullnumber doesn't exist.
================================
*/
int GetHullBounds( int hullnumber, float *mins, float *maxs )
{
	int iret = 0;

	switch ( hullnumber )
	{
	case 0:				// Normal player
		mins = VEC_HULL_MIN;
		maxs = VEC_HULL_MAX;
		iret = 1;
		break;
	case 1:				// Crouched player
		mins = VEC_DUCK_HULL_MIN;
		maxs = VEC_DUCK_HULL_MAX;
		iret = 1;
		break;
	case 2:				// Point based hull
		mins = Vector( 0, 0, 0 );
		maxs = Vector( 0, 0, 0 );
		iret = 1;
		break;
	}

	return iret;
}

/*
================================
CreateInstancedBaselines

Create pseudo-baselines for items that aren't placed in the map at spawn time, but which are likely
to be created during play ( e.g., grenades, ammo packs, projectiles, corpses, etc. )
================================
*/
void CreateInstancedBaselines ( void )
{
	int iret = 0;
	entity_state_t state;

	memset( &state, 0, sizeof( state ) );

	// Create any additional baselines here for things like grendates, etc.
	// iret = ENGINE_INSTANCE_BASELINE( pc->pev->classname, &state );

	// Destroy objects.
	//UTIL_Remove( pc );
}

/*
================================
InconsistentFile

One of the ENGINE_FORCE_UNMODIFIED files failed the consistency check for the specified player
 Return 0 to allow the client to continue, 1 to force immediate disconnection ( with an optional disconnect message of up to 256 characters )
================================
*/
int	InconsistentFile( const edict_t *player, const char *filename, char *disconnect_message )
{
	// Server doesn't care?
	if ( CVAR_GET_FLOAT( "mp_consistency" ) != 1 )
		return 0;

	// Default behavior is to kick the player
	sprintf( disconnect_message, "Server is enforcing file consistency for %s\n", filename );

	// Kick now with specified disconnect message.
	return 1;
}

/*
================================
AllowLagCompensation

 The game .dll should return 1 if lag compensation should be allowed ( could also just set
  the sv_unlag cvar.
 Most games right now should return 0, until client-side weapon prediction code is written
  and tested for them ( note you can predict weapons, but not do lag compensation, too, 
  if you want.
================================
*/
int AllowLagCompensation( void )
{
	return 1;
}

/*
================================
ShouldCollide

  Called when the engine believes two entities are about to collide. Return 0 if you
  want the two entities to just pass through each other without colliding or calling the
  touch function.
================================
*/
int ShouldCollide( edict_t *pentTouched, edict_t *pentOther )
{
	CBaseEntity *pTouch = CBaseEntity::Instance( pentTouched );
	CBaseEntity *pOther = CBaseEntity::Instance( pentOther );

	if( pTouch && pOther )
		return pOther->ShouldCollide( pTouch );

	return 1;
}