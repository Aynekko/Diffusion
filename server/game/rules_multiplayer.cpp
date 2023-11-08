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
//
// multiplay_gamerules.cpp
//
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons/weapons.h"
#include "game/gamerules.h"
#include "game/skill.h"
#include "game/game.h"
#include "items.h"
#include "hltv.h"
#include <iostream>
#include <ctime>
#include "bot/bot.h"

extern DLL_GLOBAL CGameRules *g_pGameRules;
extern DLL_GLOBAL BOOL g_fGameOver;

extern int g_teamplay;

float g_flIntermissionStartTime = 0;

extern respawn_t bot_respawn[32];
void BotCreate( const char *skin, const char *name, const char *skill );


int CountPlayers( void );

//*********************************************************
// Rules for the half-life multiplayer game.
//*********************************************************

CHalfLifeMultiplay :: CHalfLifeMultiplay()
{
	int i;
	
	RefreshSkillData();
	m_flIntermissionEndTime = 0;
	g_flIntermissionStartTime = 0;
	
	// 11/8/98
	// Modified by YWB:  Server .cfg file is now a cvar, so that 
	// server ops can run multiple game servers, with different server .cfg files,
	// from a single installed directory.
	// Mapcyclefile is already a cvar.

	// 3/31/99
	// Added lservercfg file cvar, since listen and dedicated servers should not
	// share a single config file. (sjb)
	if ( IS_DEDICATED_SERVER() )
	{
		// dedicated server
		char *servercfgfile = (char *)CVAR_GET_STRING( "servercfgfile" );

		if ( servercfgfile && servercfgfile[0] )
		{
			char szCommand[256];
			
			ALERT( at_console, "Executing dedicated server config file\n" );
			sprintf( szCommand, "exec %s\n", servercfgfile );
			SERVER_COMMAND( szCommand );
		}
	}
	else
	{
		// listen server
		char *lservercfgfile = (char *)CVAR_GET_STRING( "lservercfgfile" );

		if ( lservercfgfile && lservercfgfile[0] )
		{
			char szCommand[256];
			
			ALERT( at_console, "Executing listen server config file\n" );
			sprintf( szCommand, "exec %s\n", lservercfgfile );
			SERVER_COMMAND( szCommand );
		}
	}

	InitServerMessages();
	InitServerSounds();

	BotCheckTime = gpGlobals->time + 10.0;
	
	static float previous_time = 999999999;
	if( previous_time > gpGlobals->time )
	{
		for( int index = 0; index < 32; index++ )
		{
			if( (bot_respawn[index].is_used) && (bot_respawn[index].state != BOT_NEED_TO_RESPAWN) )
			{
				// bot has already been "kicked" by server so just set flag
				bot_respawn[index].state = BOT_NEED_TO_RESPAWN;

				// if the map was changed set the respawn time...
				RespawnTime = 5.0;
			}
		}
	}

	// initialize bonuses (a skull icon which eventually spawns after player's death)
	for( i = 0; i < MAX_ACTIVE_BONUSES; i++ )
		BonusSpawnPoints[i] = g_vecZero;

	NextBonusSpawnTime = gpGlobals->time;
}

BOOL CHalfLifeMultiplay::ClientCommand( CBasePlayer *pPlayer, const char *pcmd )
{
	return CGameRules::ClientCommand(pPlayer, pcmd);
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::RefreshSkillData( void )
{
	// load all default values
	CGameRules::RefreshSkillData();
}

// longest the intermission can last, in seconds
#define MAX_INTERMISSION_TIME		120

extern cvar_t timeleft, fragsleft;
extern cvar_t mp_chattime;

//=========================================================
// Think: main function which loops during the server's worktime
//=========================================================
void CHalfLifeMultiplay :: Think ( void )
{
	///// Check game rules /////
	static int last_frags;
	static int last_time;

	int frags_remaining = 0;
	int time_remaining = 0;

	if ( g_fGameOver )   // someone else quit the game already
	{
		// bounds check
		int time = (int)CVAR_GET_FLOAT( "mp_chattime" );
		if ( time < 1 )
			CVAR_SET_STRING( "mp_chattime", "1" );
		else if ( time > MAX_INTERMISSION_TIME )
			CVAR_SET_STRING( "mp_chattime", UTIL_dtos1( MAX_INTERMISSION_TIME ) );

		m_flIntermissionEndTime = g_flIntermissionStartTime + mp_chattime.value;

		// check to see if we should change levels now
	/*	if ( m_flIntermissionEndTime < gpGlobals->time )
		{
			if ( m_iEndIntermissionButtonHit  // check that someone has pressed a key, or the max intermission time is over
				|| ( ( g_flIntermissionStartTime + MAX_INTERMISSION_TIME ) < gpGlobals->time) ) 
				ChangeLevel(); // intermission is over
		}*/

		// diffusion
		if( m_flIntermissionEndTime < gpGlobals->time )
			ChangeLevel();

		return;
	}

	float flTimeLimit = timelimit.value * 60;
	float flFragLimit = fraglimit.value;

	time_remaining = (int)(flTimeLimit ? ( flTimeLimit - gpGlobals->time ) : 0);
	
	if ( flTimeLimit != 0 && gpGlobals->time >= flTimeLimit )
	{
		GoToIntermission();
		return;
	}

	if ( flFragLimit )
	{
		int bestfrags = 9999;
		int remain;

		// check if any player is over the frag limit
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );

			if ( pPlayer && pPlayer->pev->frags >= flFragLimit )
			{
				GoToIntermission();
				return;
			}

			if ( pPlayer )
			{
				remain = flFragLimit - pPlayer->pev->frags;
				if ( remain < bestfrags )
					bestfrags = remain;
			}
		}

		frags_remaining = bestfrags;
	}

	// Updates when frags change
	if ( frags_remaining != last_frags )
		g_engfuncs.pfnCvar_DirectSet( &fragsleft, UTIL_VarArgs( "%i", frags_remaining ) );

	// Updates once per second
	if ( timeleft.value != last_time )
		g_engfuncs.pfnCvar_DirectSet( &timeleft, UTIL_VarArgs( "%i", time_remaining ) );

	last_frags = frags_remaining;
	last_time  = time_remaining;

	ManageMessages();

	ManageBots();

	ManageBonuses();
}

//=========================================================================
// ManageBonuses: try to spawn a bonus (skull icon) in a place where a player has died
//=========================================================================
void CGameRules::ManageBonuses(void)
{
	if( mp_allow_bonuses.value == 0 )
		return;
	
	if( gpGlobals->time < NextBonusSpawnTime )
		return;

	// count active bonuses on the map
	int count = 0;
	CBaseEntity *pActiveBonus = NULL;
	while( (pActiveBonus = UTIL_FindEntityByClassname( pActiveBonus, "env_bonus" )) != NULL )
	{
		count++;
		if( count >= MAX_ACTIVE_BONUSES ) // map is filled with bonuses
		{
			NextBonusSpawnTime = gpGlobals->time + BONUS_FREQUENCY_TIME;
			return;
		}
	}

	// search for a valid spawn coordinate (not a zero vector), go from bottom
	for( int i = MAX_ACTIVE_BONUSES - 1; i >= 0; i-- )
	{
		if( !BonusSpawnPoints[i].IsNull() )
		{
			CBaseEntity *pBonus = CBaseEntity::Create( "env_bonus", BonusSpawnPoints[i], g_vecZero, 0 );
			if( pBonus )
			{
				pBonus->SetThink( &CBaseEntity::SUB_Remove );
				pBonus->SetNextThink( 60 );
			}
			BonusSpawnPoints[i] = g_vecZero;
			NextBonusSpawnTime = gpGlobals->time + BONUS_FREQUENCY_TIME;
			return;
		}
	}

	// there was nothing to spawn and there was not enough bonuses.
	// just push the check time further
	NextBonusSpawnTime = gpGlobals->time + BONUS_FREQUENCY_TIME;
}

//=========================================================================
// ManageMessages: display messages in chat
//=========================================================================
void CGameRules::ManageMessages(void)
{
	if( (mp_server_notify.value > 0) && (MessagesLoaded > 0) )
		ServerHasMessages = true;
	else
		ServerHasMessages = false;

	if( ServerHasMessages == true )
	{
		if( gpGlobals->time > m_flLastTimeServerMsgShown )
		{
			m_flServerMsgDelay = mp_servermsg_delay.value;
			m_flServerMsgDelay = bound( 10, m_flServerMsgDelay, 120 );
			m_flLastTimeServerMsgShown = gpGlobals->time + m_flServerMsgDelay;

			int RandomMessage = RANDOM_LONG( 1, MessagesLoaded );

			// handle specific commands
			if( !strncmp( Message[RandomMessage][MAX_SERVER_MESSAGE_LENGTH], "!", 1 ) )
			{
				if( !strncmp( Message[RandomMessage][MAX_SERVER_MESSAGE_LENGTH], "!localtime", 5 ) )
				{
					time_t rawtime;
					struct tm *timeinfo;
					char timebuf[80];

					time( &rawtime );
					timeinfo = localtime( &rawtime );
					strftime( timebuf, sizeof( timebuf ), "%d.%m.%Y %H:%M:%S", timeinfo );
					UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "* ^5Server local time^7: %s\n", (char *)timebuf ) );
				}
				else if( !strncmp( Message[RandomMessage][MAX_SERVER_MESSAGE_LENGTH], "!timeleft", 9 ) )
				{
					int seconds = timeleft.value;
					int minutes = 0;

					if( seconds > 0 )
					{
						minutes = seconds / 60.0f;
						seconds = seconds - minutes * 60;
						if( seconds >= 10 )
							UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "* ^5Time left^7: %i:%i\n", minutes, seconds ) );
						else
							UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "* ^5Time left^7: %i:0%i\n", minutes, seconds ) );
					}
					else
						UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "* ^5Time left^7: No time limit\n" ) );
				}
				else if( !strncmp( Message[RandomMessage][MAX_SERVER_MESSAGE_LENGTH], "!fragsleft", 9 ) )
				{
					int frags_left = fragsleft.value;
					if( frags_left > 0 )
						UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "* ^5Frags left^7: %i\n", frags_left ) );
					else
						UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "* ^5Frags left^7: No frag limit\n" ) );
				}
				else if( !strncmp( Message[RandomMessage][MAX_SERVER_MESSAGE_LENGTH], "!currentmap", 11 ) )
				{
					UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "* ^5Current map^7: %s\n", STRING( gpGlobals->mapname ) ) );
				}
			}
			else // just show written text
				UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "* ^5Server^7: %s\n", (char *)Message[RandomMessage][MAX_SERVER_MESSAGE_LENGTH] ) );
		}
	}
}

//=========================================================================
// ManageBots: add/remove bots during the game when players enter/leave
// the server; fill the quota.
//=========================================================================
void CGameRules::ManageBots(void)
{
	if( g_fGameOver )
		return;
	
	int index = 0;
	
	if( RespawnTime > 1.0 && gpGlobals->time >= RespawnTime )
	{
		BotCheckTime = gpGlobals->time + 5.0;
		
		// find bot needing to be respawned from previous map or server restart
		while( (index < 32) && (bot_respawn[index].state != BOT_NEED_TO_RESPAWN) )
			index++;

		if( index < 32 )
		{
			bot_respawn[index].state = BOT_IS_RESPAWNING;
			bot_respawn[index].is_used = FALSE;      // free up this slot

			// respawn 1 bot then wait a while (otherwise engine crashes)
			BotCreate( bot_respawn[index].skin, bot_respawn[index].name, bot_respawn[index].skill );
			RespawnTime = gpGlobals->time + 1.0;  // set next respawn time

		}
		else
			RespawnTime = 0;
	}

	int min_bots = (int)CVAR_GET_FLOAT( "bot_min" );
	int max_bots = (int)CVAR_GET_FLOAT( "bot_max" );
	bool CvarsChanged = false;

	if( min_bots >= max_bots )
	{
		min_bots = max_bots - 1;
		CvarsChanged = true;
	}

	if( min_bots < 0 )
	{
		min_bots = 0;
		CvarsChanged = true;
	}

	if( CvarsChanged )
	{
		CVAR_SET_STRING( "bot_min", UTIL_dtos1(min_bots) );
		CVAR_SET_STRING( "bot_max", UTIL_dtos1(max_bots) );
	}
	
	if( gpGlobals->time > BotCheckTime )
	{
		BotCheckTime = gpGlobals->time + 5.0;
		
		// count active players on the map
		int playercount = 0;// CountPlayers();
		for( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( i );

			// skip invalid players
			if( !pPlayer )
				continue;

			// skip spectators
		//	if( pPlayer->IsObserver() )
		//		continue;

			// skip connected players but they haven't spawned (in welcome cam)
		//	if( pPlayer->m_bInWelcomeCam )
		//		continue;

			const char *model = g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model" );
			if( model[0] <= 0 )
				continue;

			playercount++;
		}
		
		int botcount = 0;
		for( index = 0; index < 32; index++ )
		{
			if( bot_respawn[index].is_used )
				botcount++;
		}

		// enough players, kick the bot
		if( (playercount > max_bots) && (botcount > min_bots) )
		{
			for( index = 0; index < 32; index++ )
			{
				if( bot_respawn[index].is_used )
				{
					char cmd[40];
					sprintf( cmd, "kick \"%s\"\n", bot_respawn[index].name );
					SERVER_COMMAND( cmd );
					bot_respawn[index].is_used = FALSE;
					bot_respawn[index].state = BOT_IDLE;
					break;
				}
			}

		}

		// find bot needing to be spawned...
		if( playercount < max_bots && playercount < gpGlobals->maxClients )
		{
			index = 0;
			while( (index < 32) && (bot_respawn[index].is_used) )
				index++;

			if( index < 32 )
			{
				bot_respawn[index].state = BOT_IS_RESPAWNING;

				// spawn 1 bot then wait a while (otherwise engine crashes)
				BotCreate( NULL, NULL, NULL );
			}
		}
	}
}

//=========================================================================
// InitServerMessages: parse the text file line-by-line, get notification
// messages for the server to display.
//=========================================================================
void CGameRules::InitServerMessages(void)
{	
	char* pMsgFile = (char*)LOAD_FILE("server/messages.txt", NULL);
	char* File = pMsgFile;
	char line[MAX_SERVER_MESSAGE_LENGTH + 1];
	int iMessageNum = 0;
	int TooLong = 0;

	if (!File)
	{
		ALERT(at_warning, "server/messages.txt couldn't be loaded.\n");
		return;
	}

	MessagesLoaded = 0;

	while( ((pMsgFile = COM_ParseFile(pMsgFile, line)) != NULL) && (MessagesLoaded < MAX_SERVER_MESSAGES) )
	{	
		if (strlen(line) > MAX_SERVER_MESSAGE_LENGTH)
		{
			TooLong++;
			continue;
		}

		iMessageNum++;
		Message[iMessageNum][MAX_SERVER_MESSAGE_LENGTH] = copystring(line);
		MessagesLoaded++;
	}

	ALERT(at_aiconsole, "%i server notification messages were loaded.\n", MessagesLoaded);
	if( TooLong > 0)
		ALERT(at_warning, "%i server notifications were too long and weren't parsed.\n", TooLong);

	FREE_FILE(File);

	m_flLastTimeServerMsgShown = gpGlobals->time + 10;
}

//=========================================================================
// InitServerSounds: parse the text file line-by-line, get server sounds
//=========================================================================
void CGameRules::InitServerSounds( void )
{
	char *pSndFile;
	char line[MAX_SOUNDPATH + 1];
	int SoundsParsed;

	// ------ first, load "lol" sounds -------------
	pSndFile = (char *)LOAD_FILE( "server/sounds_lol.txt", NULL );

	if( !pSndFile )
	{
		ALERT( at_warning, "server/sounds_lol.txt couldn't be loaded.\n" );
		return;
	}

	SoundsParsed = 0;
	while( ((pSndFile = COM_ParseFile( pSndFile, line )) != NULL) && (SoundsParsed < MAX_SERVER_MESSAGES) )
	{
		if( strlen( line ) > MAX_SOUNDPATH )
			continue;

		// need to make sure that sound exists before adding it into array.
		// so there won't be a situation when there's no sound playing after typing a message
		if( UTIL_SoundExists( copystring( line ) ) )
		{
			snd_lol[SoundsParsed][MAX_SOUNDPATH] = copystring( line );
			SoundsParsed++;
		}
	}

	TotalServerSounds_lol = 0;
	for( int i = 0; i < SoundsParsed; i++ )
	{
		PRECACHE_SOUND( snd_lol[i][MAX_SOUNDPATH] );
		TotalServerSounds_lol++;
	}

	ALERT( at_aiconsole, "%i -lol- sounds were loaded.\n", TotalServerSounds_lol );

	FREE_FILE( pSndFile );

	// ----------------------------------------------------

	// ------ load "meow" sounds -------------
	pSndFile = (char *)LOAD_FILE( "server/sounds_meow.txt", NULL );

	if( !pSndFile )
	{
		ALERT( at_warning, "server/sounds_meow.txt couldn't be loaded.\n" );
		return;
	}

	SoundsParsed = 0;
	while( ((pSndFile = COM_ParseFile( pSndFile, line )) != NULL) && (SoundsParsed < MAX_SERVER_MESSAGES) )
	{
		if( strlen( line ) > MAX_SOUNDPATH )
			continue;

		// need to make sure that sound exists before adding it into array.
		// so there won't be a situation when there's no sound playing after typing a message
		if( UTIL_SoundExists( copystring( line ) ) )
		{
			snd_meow[SoundsParsed][MAX_SOUNDPATH] = copystring( line );
			SoundsParsed++;
		}
	}

	TotalServerSounds_meow = 0;
	for( int i = 0; i < SoundsParsed; i++ )
	{
		PRECACHE_SOUND( snd_meow[i][MAX_SOUNDPATH] );
		TotalServerSounds_meow++;
	}

	ALERT( at_aiconsole, "%i -meow- sounds were loaded.\n", TotalServerSounds_meow );

	FREE_FILE( pSndFile );
	// ----------------------------------------------------

	// load next sounds the similar way...
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsMultiplayer( void )
{
	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsDeathmatch( void )
{
	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsCoOp( void )
{
	return gpGlobals->coop;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::FShouldSwitchWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon )
{
	if ( !pWeapon->CanDeploy() )
	{
		// that weapon can't deploy anyway.
		return FALSE;
	}

	if ( !pPlayer->m_pActiveItem )
	{
		// player doesn't have an active item!
		return TRUE;
	}

	if ( !pPlayer->m_pActiveItem->CanHolster() )
	{
		// can't put away the active item.
		return FALSE;
	}

	if ( pWeapon->iWeight() > pPlayer->m_pActiveItem->iWeight() )
		return TRUE;

	return FALSE;
}

BOOL CHalfLifeMultiplay :: GetNextBestWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pCurrentWeapon )
{
	CBasePlayerItem *pCheck;
	CBasePlayerItem *pBest;// this will be used in the event that we don't find a weapon in the same category.
	int iBestWeight;
	int i;

	iBestWeight = -1;// no weapon lower than -1 can be autoswitched to
	pBest = NULL;

	if ( !pCurrentWeapon->CanHolster() )
		return FALSE; // can't put this gun away right now, so can't switch.

	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		pCheck = pPlayer->m_rgpPlayerItems[ i ];

		while ( pCheck )
		{
			if ( pCheck->iWeight() > -1 && pCheck->iWeight() == pCurrentWeapon->iWeight() && pCheck != pCurrentWeapon )
			{
				// this weapon is from the same category. 
				if ( pCheck->CanDeploy() )
				{
					if ( pPlayer->SwitchWeapon( pCheck ) )
						return TRUE;
				}
			}
			else if ( pCheck->iWeight() > iBestWeight && pCheck != pCurrentWeapon )// don't reselect the weapon we're trying to get rid of
			{
				//ALERT ( at_console, "Considering %s\n", STRING( pCheck->pev->classname ) );
				// we keep updating the 'best' weapon just in case we can't find a weapon of the same weight
				// that the player was using. This will end up leaving the player with his heaviest-weighted 
				// weapon. 
				if ( pCheck->CanDeploy() )
				{
					// if this weapon is useable, flag it as the best
					iBestWeight = pCheck->iWeight();
					pBest = pCheck;
				}
			}

			pCheck = pCheck->m_pNext;
		}
	}

	// if we make it here, we've checked all the weapons and found no useable 
	// weapon in the same catagory as the current weapon. 
	
	// if pBest is null, we didn't find ANYTHING. Shouldn't be possible- should always 
	// at least get the crowbar, but ya never know.
	if ( !pBest )
		return FALSE;

	pPlayer->SwitchWeapon( pBest );

	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay :: ClientConnected( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[ 128 ] )
{
	return TRUE;
}

extern int gmsgSayText;
extern int gmsgGameMode;

void CHalfLifeMultiplay :: UpdateGameMode( CBasePlayer *pPlayer )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgGameMode, NULL, pPlayer->edict() );
		WRITE_BYTE( 0 );  // game mode none
	MESSAGE_END();
}

void CHalfLifeMultiplay :: InitHUD( CBasePlayer *pl )
{
	// notify other clients of player joining the game
	if( pl->pev->flags & FL_FAKECLIENT ) // bot
	{
		UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "* ^5Server^7: Added bot %s\n", STRING( pl->pev->netname )) );
	}
	else
	{

		UTIL_ClientPrintAll( HUD_PRINTNOTIFY, UTIL_VarArgs( "%s has joined the game\n",
			(pl->pev->netname && STRING( pl->pev->netname )[0] != 0) ? STRING( pl->pev->netname ) : "unconnected" ) );

		EMIT_SOUND( INDEXENT( 0 ), CHAN_STATIC, SND_PLAYER_JOIN, 1, ATTN_NONE );
	}

	// team match?
	if ( g_teamplay )
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" entered the game\n",  
			STRING( pl->pev->netname ), 
			GETPLAYERUSERID( pl->edict() ),
			GETPLAYERAUTHID( pl->edict() ),
			g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pl->edict() ), "model" ) );
	}
	else
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%i>\" entered the game\n",  
			STRING( pl->pev->netname ), 
			GETPLAYERUSERID( pl->edict() ),
			GETPLAYERAUTHID( pl->edict() ),
			GETPLAYERUSERID( pl->edict() ) );
	}

	UpdateGameMode( pl );

	// sending just one score makes the hud scoreboard active;  otherwise
	// it is just disabled for single play
	MESSAGE_BEGIN( MSG_ONE, gmsgScoreInfo, NULL, pl->edict() );
		WRITE_BYTE( ENTINDEX(pl->edict()) );
		WRITE_SHORT( 0 );
		WRITE_SHORT( 0 );
		WRITE_SHORT( 0 );
		WRITE_SHORT( 0 );
		if( pl->HasFlag( F_BOT ) )
			WRITE_BYTE( 1 );
		else
			WRITE_BYTE( 0 );
	MESSAGE_END();

	// Send player spectator status (it is not used in client dll)
	MESSAGE_BEGIN(MSG_ALL, gmsgSpectator);
		WRITE_BYTE(pl->entindex());
		WRITE_BYTE(pl->IsObserver());
	MESSAGE_END();

	SendMOTDToClient( pl->edict() );

	// loop through all active players and send their score info to the new client
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if( plr )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgScoreInfo, NULL, pl->edict() );
				WRITE_BYTE( i ); // client number
				WRITE_SHORT( plr->pev->frags );
				WRITE_SHORT( plr->m_iDeaths );
				WRITE_SHORT( 0 );
				WRITE_SHORT( GetTeamIndex( plr->m_szTeamName ) + 1 );
				if( pl->HasFlag( F_BOT ) )
					WRITE_BYTE( 1 );
				else
					WRITE_BYTE( 0 );
			MESSAGE_END();
		}
	}

	if ( g_fGameOver )
	{
		MESSAGE_BEGIN( MSG_ONE, SVC_INTERMISSION, NULL, pl->edict() );
		MESSAGE_END();
	}
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay :: ClientDisconnected( edict_t *pClient )
{
	if (!pClient)
		return;
	
	CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pClient );

	if ( pPlayer )
	{
		UTIL_FireTargets( "game_playerleave", pPlayer, pPlayer, USE_TOGGLE, 0 );

		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" disconnected\n",  
				STRING( pPlayer->pev->netname ), 
				GETPLAYERUSERID( pPlayer->edict() ),
				GETPLAYERAUTHID( pPlayer->edict() ),
				g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model" ) );
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" disconnected\n",  
				STRING( pPlayer->pev->netname ), 
				GETPLAYERUSERID( pPlayer->edict() ),
				GETPLAYERAUTHID( pPlayer->edict() ),
				GETPLAYERUSERID( pPlayer->edict() ) );
		}

		// BugFixedHL - occasional crash after player leaved server while using tank.
		if (pPlayer->m_pTank != NULL)
			pPlayer->m_pTank->Use(pPlayer, pPlayer, USE_OFF, 0); // Stop controlling the tank

		if( pPlayer->pCar != NULL )
			pPlayer->pCar->Use( pPlayer, pPlayer, USE_OFF, 0 ); // get out of the damn car

		pPlayer->RemoveAllItems( TRUE );// destroy all of the players weapons and items

		// Tell all clients this player isn't a spectator anymore
		MESSAGE_BEGIN(MSG_ALL, gmsgSpectator);
			WRITE_BYTE(ENTINDEX(pClient));
			WRITE_BYTE(0);
		MESSAGE_END();
	}
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay :: FlPlayerFallDamage( CBasePlayer *pPlayer )
{
	int iFallDamage = (int)falldamage.value;

	switch ( iFallDamage )
	{
	case 1: // progressive
		pPlayer->m_flFallVelocity -= PLAYER_MAX_SAFE_FALL_SPEED;
		return pPlayer->m_flFallVelocity * DAMAGE_FOR_FALL_SPEED;
		break;
	default:
	case 0:// no damage
		return 0;
		break;
	}
} 

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
	return TRUE;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay :: PlayerThink( CBasePlayer *pPlayer )
{
	if ( g_fGameOver )
	{
		// check for button presses
		if ( pPlayer->m_afButtonPressed & ( IN_DUCK | IN_ATTACK | IN_ATTACK2 | IN_USE | IN_JUMP ) )
			m_iEndIntermissionButtonHit = TRUE;

		// clear attack/use commands from player
		pPlayer->m_afButtonPressed = 0;
		pPlayer->pev->button = 0;
		pPlayer->m_afButtonReleased = 0;
	}

	// diffusion - just spawned, "appear"
	// player is partially visible when spawnprotected
	else if( !pPlayer->IsSpawnProtected && pPlayer->pev->rendermode == kRenderTransTexture && pPlayer->pev->renderamt < 255 )
	{
		pPlayer->pev->renderamt += 350 * gpGlobals->frametime;
		if( pPlayer->pev->renderamt >= 255 )
		{
			pPlayer->pev->rendermode = kRenderNormal;
			pPlayer->pev->renderamt = 255;
		}
	}
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay :: PlayerSpawn( CBasePlayer *pPlayer )
{
	// Start welcome cam for new players
	if( !pPlayer->m_bPutInServer && !pPlayer->HasFlag(F_BOT) )
	{
		pPlayer->StartWelcomeCam();
		return;
	}
	
	BOOL		addDefault;
	CBaseEntity	*pWeaponEntity = NULL;

	pPlayer->AddWeapon( WEAPON_SUIT );

	pPlayer->m_iBonusWeaponID = 0;
	
	addDefault = TRUE;

	while ( pWeaponEntity = UTIL_FindEntityByClassname( pWeaponEntity, "game_player_equip" ))
	{
		pWeaponEntity->Touch( pPlayer );
		addDefault = FALSE;
	}

	if ( addDefault )
	{
		pPlayer->GiveNamedItem( "weapon_knife" );
		pPlayer->GiveNamedItem( "weapon_9mmhandgun" );
		pPlayer->GiveAmmo( 68, "9mm", _9MM_MAX_CARRY ); // 4 full reloads

		switch (pPlayer->m_iBonusWeaponLevel)
		{
		case 3:
			pPlayer->GiveNamedItem("weapon_hkmp5");
			pPlayer->GiveAmmo(MP5_DEFAULT_GIVE, "hkmp5ammo", _9MM_MAX_CARRY);
			pPlayer->m_iBonusWeaponID = WEAPON_HKMP5;
		break;
		case 4:
			pPlayer->GiveNamedItem("weapon_9mmAR");
			pPlayer->GiveAmmo(MRC_DEFAULT_GIVE, "mrcbullets", _9MM_MAX_CARRY);
			pPlayer->m_iBonusWeaponID = WEAPON_MRC;
		break;
		case 5:
			pPlayer->GiveNamedItem( "weapon_g36c" );
			pPlayer->GiveAmmo( G36C_DEFAULT_GIVE, "g36cammo", _9MM_MAX_CARRY );
			pPlayer->m_iBonusWeaponID = WEAPON_G36C;
		break;
		case 6:
			pPlayer->GiveNamedItem("weapon_9mmAR");
			pPlayer->GiveAmmo(MRC_DEFAULT_GIVE, "mrcbullets", _9MM_MAX_CARRY);
			pPlayer->GiveAmmo(AMMO_M203BOX_GIVE, "ARgrenades", M203_GRENADE_MAX_CARRY);
			pPlayer->m_iBonusWeaponID = WEAPON_MRC;
		break;
		case 7:
		case 8:
			pPlayer->GiveNamedItem("weapon_gausniper");
			pPlayer->GiveAmmo(AMMO_URANIUMBOX_GIVE, "uranium", URANIUM_MAX_CARRY);
			pPlayer->m_iBonusWeaponID = WEAPON_GAUSS;
		break;
		case 9:
		case 10:
			pPlayer->GiveNamedItem("weapon_ar2");
			pPlayer->GiveAmmo(AMMO_URANIUMBOX_GIVE, "uranium", URANIUM_MAX_CARRY);
			pPlayer->m_iBonusWeaponID = WEAPON_AR2;
		break;
		}
	}

	// diffusion - spawn protection
	if( mp_spawnprotect.value < 0 || mp_spawnprotect.value > 10 )
		ALERT( at_warning, "Spawn protection can be from 0 to 10 seconds!\n" );
	mp_spawnprotect.value = bound( 0, mp_spawnprotect.value, 10 );
	if( mp_spawnprotect.value > 0 )
	{
		pPlayer->SpawnProtectTime = gpGlobals->time + mp_spawnprotect.value;
		pPlayer->IsSpawnProtected = true;
	}

	UTIL_FireTargets("game_playerspawn", pPlayer, pPlayer, USE_TOGGLE, 0);

	// diffusion - make player model slowly appear visually
	pPlayer->pev->rendermode = kRenderTransTexture;
	pPlayer->pev->renderamt = 50;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay :: FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	return TRUE;
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay :: FlPlayerSpawnTime( CBasePlayer *pPlayer )
{
	return gpGlobals->time;//now!
}

BOOL CHalfLifeMultiplay :: AllowAutoTargetCrosshair( void )
{
	return ( aimcrosshair.value != 0 );
}

//=========================================================
// IPointsForKill - how many points awarded to anyone
// that kills this player?
//=========================================================
int CHalfLifeMultiplay :: IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled )
{
	return 1;
}


//=========================================================
// PlayerKilled - someone/something killed this player
//=========================================================
void CHalfLifeMultiplay :: PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	DeathNotice( pVictim, pKiller, pInflictor );

	pVictim->m_iDeaths += 1;

	if( pVictim->KillingSpreeLevel > 0 )
	{
		MESSAGE_BEGIN( MSG_ALL, gmsgTempEnt );
			WRITE_BYTE( TE_UNREALSOUND );
			WRITE_BYTE( SPREE_SND_SPREE_ENDED );
		MESSAGE_END();
	}

	CBasePlayer *pTheKiller = (CBasePlayer *)CBaseEntity::Instance( pKiller );

	// in multiplayer, handle bonus level if a player killed us (make sure it's not a suicide...)
	if( ((pKiller->flags & FL_CLIENT) || (pKiller->flags & FL_FAKECLIENT)) && (pKiller != pVictim->pev) && (mp_weaponbonus.value > 0) )
	{
		if( pVictim->m_iBonusWeaponLevel < 10 )
			pVictim->m_iBonusWeaponLevel++;

		// check the bonus of our killer 
		if( pTheKiller )
		{
			if( pTheKiller->m_iBonusWeaponLevel > 0 )
			{
				pTheKiller->m_iBonusWeaponLevel -= 2;
				if( pTheKiller->m_iBonusWeaponLevel < 0 )
					pTheKiller->m_iBonusWeaponLevel = 0;
			}
		}
	}

	// add hp bonus
	if( mp_healthbonus.value > 0 )
	{
		if( pTheKiller && pTheKiller->IsAlive() )
			pTheKiller->pev->health += mp_healthbonus.value;
	}

	// killing spree
	if( pTheKiller && (pTheKiller != pVictim) )
	{
		if( gpGlobals->time - pTheKiller->LastConsecutiveKillTime < 5.0f )
			pTheKiller->ConsecutiveKillLevel++;

		pTheKiller->LastConsecutiveKillTime = gpGlobals->time;
		pTheKiller->ConsecutiveKills++;
	}

	// try to create or remember a bonus item spawn
	if( mp_allow_bonuses.value > 0 )
	{
		for( int i = 0; i < MAX_ACTIVE_BONUSES; i++ )
		{
			// free spot
			if( BonusSpawnPoints[i].IsNull() )
			{
				BonusSpawnPoints[i] = pVictim->GetAbsOrigin();
				break;
			}
		}
	}

	UTIL_FireTargets( "game_playerdie", pVictim, pVictim, USE_TOGGLE, 0 );
	CBasePlayer *peKiller = NULL;
	CBaseEntity *ktmp = CBaseEntity::Instance( pKiller );
	if ( ktmp && (ktmp->Classify() == CLASS_PLAYER) )
		peKiller = (CBasePlayer*)ktmp;

	if ( pVictim->pev == pKiller )  
		pKiller->frags -= 1; // killed self
	else if ( ktmp && ktmp->IsPlayer() )
	{
		// if a player dies in a deathmatch game and the killer is a client, award the killer some points
		pKiller->frags += IPointsForKill( peKiller, pVictim );	
		UTIL_FireTargets( "game_playerkill", ktmp, ktmp, USE_TOGGLE, 0 );
	}
	else
		pKiller->frags -= 1; // killed by the world

	// update the scores
	// killed scores
	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE( ENTINDEX(pVictim->edict()) );
		WRITE_SHORT( pVictim->pev->frags );
		WRITE_SHORT( pVictim->m_iDeaths );
		WRITE_SHORT( 0 );
		WRITE_SHORT( GetTeamIndex( pVictim->m_szTeamName ) + 1 );
		if( pVictim->HasFlag( F_BOT ) )
			WRITE_BYTE( 1 );
		else
			WRITE_BYTE( 0 );
	MESSAGE_END();

	// killers score, if it's a player
	CBaseEntity *ep = CBaseEntity::Instance( pKiller );
	if ( ep && ep->Classify() == CLASS_PLAYER )
	{
		CBasePlayer *PK = (CBasePlayer*)ep;

		MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
			WRITE_BYTE( ENTINDEX(PK->edict()) );
			WRITE_SHORT( PK->pev->frags );
			WRITE_SHORT( PK->m_iDeaths );
			WRITE_SHORT( 0 );
			WRITE_SHORT( GetTeamIndex( PK->m_szTeamName) + 1 );
			if( PK->HasFlag( F_BOT ) )
				WRITE_BYTE( 1 );
			else
				WRITE_BYTE( 0 );
		MESSAGE_END();

		// let the killer paint another decal as soon as he'd like.
		PK->m_flNextDecalTime = gpGlobals->time;
	}

	// BugFixedHL - comment out
//	if ( pVictim->HasNamedPlayerItem("weapon_satchel") )
//		DeactivateSatchels( pVictim );
}

//=========================================================
// Deathnotice. 
//=========================================================
void CHalfLifeMultiplay::DeathNotice( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pevInflictor )
{
	// Work out what killed the player, and send a message to all clients about it
	CBaseEntity *Killer = CBaseEntity::Instance( pKiller );

	const char *killer_weapon_name = "world";		// by default, the player is killed by the world
	int killer_index = 0;
	
	// Hack to fix name change
	char *tau = "tau_cannon";
	char *gluon = "gluon gun";

	if ( (pKiller->flags & FL_CLIENT) || (pKiller->flags & FL_FAKECLIENT) ) // diffusion - don't forget a bot :)
	{
		killer_index = ENTINDEX(ENT(pKiller));
		
		if ( pevInflictor )
		{
			if ( pevInflictor == pKiller )
			{
				// If the inflictor is the killer,  then it must be their current weapon doing the damage
				CBasePlayer *pPlayer = (CBasePlayer*)CBaseEntity::Instance( pKiller );
				
				if( pPlayer->BlastDMGOverride )
					killer_weapon_name = (const char *)"electroblast";
				else if ( pPlayer->m_pActiveItem )
					killer_weapon_name = pPlayer->m_pActiveItem->pszName();
			}
			else
				killer_weapon_name = STRING( pevInflictor->classname );  // it's just that easy
		}
	}
	else
	{
		killer_weapon_name = STRING( pevInflictor->classname );
		if( FStrEq( killer_weapon_name, "_playersentry") && (pevInflictor->owner != NULL) )
		{
			killer_index = ENTINDEX( pevInflictor->owner );
		}
	}

	// strip the monster_* or weapon_* from the inflictor's classname
	if ( strncmp( killer_weapon_name, "weapon_", 7 ) == 0 )
		killer_weapon_name += 7;
	else if ( strncmp( killer_weapon_name, "monster_", 8 ) == 0 )
		killer_weapon_name += 8;
	else if ( strncmp( killer_weapon_name, "func_", 5 ) == 0 )
		killer_weapon_name += 5;

	MESSAGE_BEGIN( MSG_ALL, gmsgDeathMsg );
		WRITE_BYTE( killer_index );						// the killer
		WRITE_BYTE( ENTINDEX(pVictim->edict()) );		// the victim
		WRITE_STRING( killer_weapon_name );		// what they were killed by (should this be a string?)
	MESSAGE_END();

	// replace the code names with the 'real' names
	if ( !strcmp( killer_weapon_name, "egon" ) )
		killer_weapon_name = gluon;
	else if ( !strcmp( killer_weapon_name, "gauss" ) )
		killer_weapon_name = tau;

	if ( pVictim->pev == pKiller )  
	{
		// killed self

		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" committed suicide with \"%s\"\n",  
				STRING( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pVictim->edict() ), "model" ),
				killer_weapon_name );		
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" committed suicide with \"%s\"\n",  
				STRING( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				GETPLAYERUSERID( pVictim->edict() ),
				killer_weapon_name );		
		}
	}
	else if ( pKiller->flags & FL_CLIENT )
	{
		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" killed \"%s<%i><%s><%s>\" with \"%s\"\n",  
				STRING( pKiller->netname ),
				GETPLAYERUSERID( ENT(pKiller) ),
				GETPLAYERAUTHID( ENT(pKiller) ),
				g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( ENT(pKiller) ), "model" ),
				STRING( pVictim->pev->netname ),
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pVictim->edict() ), "model" ),
				killer_weapon_name );
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" killed \"%s<%i><%s><%i>\" with \"%s\"\n",  
				STRING( pKiller->netname ),
				GETPLAYERUSERID( ENT(pKiller) ),
				GETPLAYERAUTHID( ENT(pKiller) ),
				GETPLAYERUSERID( ENT(pKiller) ),
				STRING( pVictim->pev->netname ),
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				GETPLAYERUSERID( pVictim->edict() ),
				killer_weapon_name );
		}
	}
	else
	{ 
		// killed by the world

		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" committed suicide with \"%s\" (world)\n",
				STRING( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ), 
				GETPLAYERAUTHID( pVictim->edict() ),
				g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pVictim->edict() ), "model" ),
				killer_weapon_name );		
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" committed suicide with \"%s\" (world)\n",
				STRING( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ), 
				GETPLAYERAUTHID( pVictim->edict() ),
				GETPLAYERUSERID( pVictim->edict() ),
				killer_weapon_name );		
		}
	}

	MESSAGE_BEGIN( MSG_SPEC, SVC_DIRECTOR );
		WRITE_BYTE ( 9 );	// command length in bytes
		WRITE_BYTE ( DRC_CMD_EVENT );	// player killed
		WRITE_SHORT( ENTINDEX(pVictim->edict()) );	// index number of primary entity
		if (pevInflictor)
			WRITE_SHORT( ENTINDEX(ENT(pevInflictor)) );	// index number of secondary entity
		else
			WRITE_SHORT( ENTINDEX(ENT(pKiller)) );	// index number of secondary entity
		WRITE_LONG( 7 | DRC_FLAG_DRAMATIC);   // eventflags (priority and flags)
	MESSAGE_END();

//  Print a standard message
	// TODO: make this go direct to console
	return; // just remove for now
/*
	char	szText[ 128 ];

	if ( pKiller->flags & FL_MONSTER )
	{
		// killed by a monster
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " was killed by a monster.\n" );
		return;
	}

	if ( pKiller == pVictim->pev )
	{
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " commited suicide.\n" );
	}
	else if ( pKiller->flags & FL_CLIENT )
	{
		strcpy ( szText, STRING( pKiller->netname ) );

		strcat( szText, " : " );
		strcat( szText, killer_weapon_name );
		strcat( szText, " : " );

		strcat ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, "\n" );
	}
	else if ( pKiller == g_pWorld )
	{
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " fell or drowned or something.\n" );
	}
	else if ( pKiller->solid == SOLID_BSP )
	{
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " was mooshed.\n" );
	}
	else
	{
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " died mysteriously.\n" );
	}

	UTIL_ClientPrintAll( szText );
*/
}

//=========================================================
// PlayerGotWeapon - player has grabbed a weapon that was
// sitting in the world
//=========================================================
void CHalfLifeMultiplay :: PlayerGotWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon )
{
}

//=========================================================
// FlWeaponRespawnTime - what is the time in the future
// at which this weapon may spawn?
//=========================================================
float CHalfLifeMultiplay :: FlWeaponRespawnTime( CBasePlayerItem *pWeapon )
{
	if ( weaponstay.value > 0 )
	{
		// make sure it's only certain weapons
		if ( !(pWeapon->iFlags() & ITEM_FLAG_LIMITINWORLD) )
			return gpGlobals->time + 0;		// weapon respawns almost instantly
	}

	return gpGlobals->time + WEAPON_RESPAWN_TIME;
}

// when we are within this close to running out of entities,  items 
// marked with the ITEM_FLAG_LIMITINWORLD will delay their respawn
#define ENTITY_INTOLERANCE	100

//=========================================================
// FlWeaponRespawnTime - Returns 0 if the weapon can respawn 
// now,  otherwise it returns the time at which it can try
// to spawn again.
//=========================================================
float CHalfLifeMultiplay :: FlWeaponTryRespawn( CBasePlayerItem *pWeapon )
{
	if ( pWeapon && pWeapon->m_iId && (pWeapon->iFlags() & ITEM_FLAG_LIMITINWORLD) )
	{
		if ( NUMBER_OF_ENTITIES() < (gpGlobals->maxEntities - ENTITY_INTOLERANCE) )
			return 0;

		// we're past the entity tolerance level,  so delay the respawn
		return FlWeaponRespawnTime( pWeapon );
	}

	return 0;
}

//=========================================================
// VecWeaponRespawnSpot - where should this weapon spawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHalfLifeMultiplay :: VecWeaponRespawnSpot( CBasePlayerItem *pWeapon )
{
	return pWeapon->GetAbsOrigin();
}

//=========================================================
// WeaponShouldRespawn - any conditions inhibiting the
// respawning of this weapon?
//=========================================================
int CHalfLifeMultiplay :: WeaponShouldRespawn( CBasePlayerItem *pWeapon )
{
	if ( pWeapon->pev->spawnflags & SF_NORESPAWN )
		return GR_WEAPON_RESPAWN_NO;

	if( pWeapon->HasFlag( F_FROM_AMMOBOX ) )
		return GR_AMMO_RESPAWN_NO;

	return GR_WEAPON_RESPAWN_YES;
}

//=========================================================
// CanHaveWeapon - returns FALSE if the player is not allowed
// to pick up this weapon
//=========================================================
BOOL CHalfLifeMultiplay::CanHavePlayerItem( CBasePlayer *pPlayer, CBasePlayerItem *pItem )
{
	if ( weaponstay.value > 0 )
	{
		if ( pItem->iFlags() & ITEM_FLAG_LIMITINWORLD )
			return CGameRules::CanHavePlayerItem( pPlayer, pItem );

		// check if the player already has this weapon
		for ( int i = 0 ; i < MAX_ITEM_TYPES ; i++ )
		{
			CBasePlayerItem *it = pPlayer->m_rgpPlayerItems[i];

			while ( it != NULL )
			{
				if ( it->m_iId == pItem->m_iId )
					return FALSE;

				it = it->m_pNext;
			}
		}
	}

	return CGameRules::CanHavePlayerItem( pPlayer, pItem );
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::CanHaveItem( CBasePlayer *pPlayer, CItem *pItem )
{
	return TRUE;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::PlayerGotItem( CBasePlayer *pPlayer, CItem *pItem )
{
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::ItemShouldRespawn( CItem *pItem )
{
	if ( pItem->pev->spawnflags & SF_NORESPAWN )
		return GR_ITEM_RESPAWN_NO;

	if( pItem->HasFlag( F_FROM_AMMOBOX ) )
		return GR_AMMO_RESPAWN_NO;

	return GR_ITEM_RESPAWN_YES;
}


//=========================================================
// At what time in the future may this Item respawn?
//=========================================================
float CHalfLifeMultiplay::FlItemRespawnTime( CItem *pItem )
{
	return gpGlobals->time + ITEM_RESPAWN_TIME;
}

//=========================================================
// Where should this item respawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHalfLifeMultiplay::VecItemRespawnSpot( CItem *pItem )
{
	return pItem->GetAbsOrigin();
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::PlayerGotAmmo( CBasePlayer *pPlayer, char *szName, int iCount )
{
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsAllowedToSpawn( CBaseEntity *pEntity )
{
//	if ( pEntity->pev->flags & FL_MONSTER )
//		return FALSE;

	return TRUE;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::AmmoShouldRespawn( CBasePlayerAmmo *pAmmo )
{
	if ( pAmmo->HasSpawnFlags(SF_NORESPAWN) )
		return GR_AMMO_RESPAWN_NO;

	if( pAmmo->HasFlag(F_FROM_AMMOBOX) )
		return GR_AMMO_RESPAWN_NO;

	return GR_AMMO_RESPAWN_YES;
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay::FlAmmoRespawnTime( CBasePlayerAmmo *pAmmo )
{
	return gpGlobals->time + AMMO_RESPAWN_TIME;
}

//=========================================================
//=========================================================
Vector CHalfLifeMultiplay::VecAmmoRespawnSpot( CBasePlayerAmmo *pAmmo )
{
	return pAmmo->GetAbsOrigin();
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay::FlHealthChargerRechargeTime( void )
{
	return 60;
}


float CHalfLifeMultiplay::FlHEVChargerRechargeTime( void )
{
	return 30;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::DeadPlayerWeapons( CBasePlayer *pPlayer )
{
	return GR_PLR_DROP_GUN_ACTIVE;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::DeadPlayerAmmo( CBasePlayer *pPlayer )
{
	return GR_PLR_DROP_AMMO_ACTIVE;
}

edict_t *CHalfLifeMultiplay::GetPlayerSpawnSpot( CBasePlayer *pPlayer )
{
	edict_t *pentSpawnSpot = CGameRules::GetPlayerSpawnSpot( pPlayer );	
	if ( IsMultiplayer() && pentSpawnSpot->v.target )
	{
		UTIL_FireTargets( STRING(pentSpawnSpot->v.target), pPlayer, pPlayer, USE_TOGGLE, 0 );
	}

	return pentSpawnSpot;
}


//=========================================================
//=========================================================
int CHalfLifeMultiplay::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
	if( !pPlayer || !pTarget || !pPlayer->IsPlayer() || !pTarget->IsPlayer() )
		return GR_NOTTEAMMATE;
	// Spectators are teammates, but not players in welcomecam mode
	if( ((CBasePlayer *)pPlayer)->IsObserver() && !((CBasePlayer *)pPlayer)->m_bInWelcomeCam &&
		((CBasePlayer *)pTarget)->IsObserver() && !((CBasePlayer *)pTarget)->m_bInWelcomeCam )
		return GR_TEAMMATE;
	// half life deathmatch has only enemies and spectators
	return GR_NOTTEAMMATE;
}

BOOL CHalfLifeMultiplay :: PlayFootstepSounds( CBasePlayer *pl, float fvol )
{
	if ( g_footsteps && g_footsteps->value == 0 )
		return FALSE;

	if ( pl->IsOnLadder() || pl->GetAbsVelocity().Length2D() > 220 )
		return TRUE;  // only make step sounds in multiplayer if the player is moving fast enough

	return FALSE;
}

BOOL CHalfLifeMultiplay :: FAllowFlashlight( void ) 
{ 
	return flashlight.value != 0; 
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay :: FAllowMonsters( void )
{
	return ( allowmonsters.value != 0 );
}

//=========================================================
//======== CHalfLifeMultiplay private functions ===========
#define INTERMISSION_TIME		6

void CHalfLifeMultiplay :: GoToIntermission( void )
{
	if ( g_fGameOver )
		return;  // intermission has already been triggered, so ignore.

	MESSAGE_BEGIN(MSG_ALL, SVC_INTERMISSION);
	MESSAGE_END();

	// bounds check
	int time = (int)CVAR_GET_FLOAT( "mp_chattime" );
	if ( time < 1 )
		CVAR_SET_STRING( "mp_chattime", "1" );
	else if ( time > MAX_INTERMISSION_TIME )
		CVAR_SET_STRING( "mp_chattime", UTIL_dtos1( MAX_INTERMISSION_TIME ) );

	m_flIntermissionEndTime = gpGlobals->time + ( (int)mp_chattime.value );
	g_flIntermissionStartTime = gpGlobals->time;

	g_fGameOver = TRUE;
	m_iEndIntermissionButtonHit = FALSE;
}

#define MAX_RULE_BUFFER 1024

typedef struct mapcycle_item_s
{
	struct mapcycle_item_s *next;

	char mapname[ 32 ];
	int  minplayers, maxplayers;
	char rulebuffer[ MAX_RULE_BUFFER ];
} mapcycle_item_t;

typedef struct mapcycle_s
{
	struct mapcycle_item_s *items;
	struct mapcycle_item_s *next_item;
} mapcycle_t;

/*
==============
DestroyMapCycle

Clean up memory used by mapcycle when switching it
==============
*/
void DestroyMapCycle( mapcycle_t *cycle )
{
	mapcycle_item_t *p, *n, *start;
	p = cycle->items;
	if ( p )
	{
		start = p;
		p = p->next;
		while ( p != start )
		{
			n = p->next;
			delete p;
			p = n;
		}
		
		delete cycle->items;
	}
	cycle->items = NULL;
	cycle->next_item = NULL;
}

/*
==============
ReloadMapCycleFile


Parses mapcycle.txt file into mapcycle_t structure
==============
*/
int ReloadMapCycleFile( char *filename, mapcycle_t *cycle )
{
	char szBuffer[ MAX_RULE_BUFFER ];
	char szMap[ 32 ];
	int length;
	char *pFileList;
	char *aFileList = pFileList = (char*)LOAD_FILE( filename, &length );
	int hasbuffer;
	mapcycle_item_s *item, *newlist = NULL, *next;
	char szToken[512];

	if ( pFileList && length )
	{
		// the first map name in the file becomes the default
		while ( 1 )
		{
			hasbuffer = 0;
			memset( szBuffer, 0, MAX_RULE_BUFFER );

			pFileList = COM_ParseFile( pFileList, szToken );
			if( szToken[0] == '\0' )
				break;

			strcpy( szMap, szToken );

			// Any more tokens on this line?
			if ( COM_TokenWaiting( pFileList ) )
			{
				pFileList = COM_ParseFile( pFileList, szToken );
				if( szToken[0] != '\0' )
				{
					hasbuffer = 1;
					strcpy( szBuffer, szToken );
				}
			}

			// Check map
			if ( IS_MAP_VALID( szMap ) )
			{
				// Create entry
				char *s;

				item = new mapcycle_item_s;

				strcpy( item->mapname, szMap );

				item->minplayers = 0;
				item->maxplayers = 0;

				memset( item->rulebuffer, 0, MAX_RULE_BUFFER );

				if ( hasbuffer )
				{
					s = g_engfuncs.pfnInfoKeyValue( szBuffer, "minplayers" );
					if ( s && s[0] )
					{
						item->minplayers = Q_atoi( s );
						item->minplayers = Q_max( item->minplayers, 0 );
						item->minplayers = Q_min( item->minplayers, gpGlobals->maxClients );
					}
					s = g_engfuncs.pfnInfoKeyValue( szBuffer, "maxplayers" );
					if ( s && s[0] )
					{
						item->maxplayers = Q_atoi( s );
						item->maxplayers = Q_max( item->maxplayers, 0 );
						item->maxplayers = Q_min( item->maxplayers, gpGlobals->maxClients );
					}

					// Remove keys
					//
					g_engfuncs.pfnInfo_RemoveKey( szBuffer, "minplayers" );
					g_engfuncs.pfnInfo_RemoveKey( szBuffer, "maxplayers" );

					strcpy( item->rulebuffer, szBuffer );
				}

				item->next = cycle->items;
				cycle->items = item;
			}
			else
				ALERT( at_console, "Skipping %s from mapcycle, not a valid map\n", szMap );
		}

		FREE_FILE( aFileList );
	}

	// Fixup circular list pointer
	item = cycle->items;

	// Reverse it to get original order
	while ( item )
	{
		next = item->next;
		item->next = newlist;
		newlist = item;
		item = next;
	}
	cycle->items = newlist;
	item = cycle->items;

	// Didn't parse anything
	if ( !item )
		return 0;

	while ( item->next )
		item = item->next;

	item->next = cycle->items;
	
	cycle->next_item = item->next;

	return 1;
}

/*
==============
CountPlayers

Determine the current # of active players on the server for map cycling logic
==============
*/
int CountPlayers( void )
{
	int	num = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

		if ( pEnt )
			num = num + 1;
	}

	return num;
}

/*
==============
ExtractCommandString

Parse commands/key value pairs to issue right after map xxx command is issued on server
 level transition
==============
*/
void ExtractCommandString( char *s, char *szCommand )
{
	// Now make rules happen
	char pkey[512];
	char value[512];	// use two buffers so compares
								// work without stomping on each other
	char *o;
	
	if ( *s == '\\' )
		s++;

	while (1)
	{
		o = pkey;
		while ( *s != '\\' )
		{
			if ( !*s )
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;

		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		strcat( szCommand, pkey );
		if( value[0] != '\0' )
		{
			strcat( szCommand, " " );
			strcat( szCommand, value );
		}
		strcat( szCommand, "\n" );

		if (!*s)
			return;
		s++;
	}
}

/*
==============
ChangeLevel

Server is changing to a new level, check mapcycle.txt for map name and setup info
==============
*/
void CHalfLifeMultiplay :: ChangeLevel( void )
{
	static char szPreviousMapCycleFile[ 256 ];
	static mapcycle_t mapcycle;

	char szNextMap[32];
	char szFirstMapInList[32];
	char szCommands[ 1500 ];
	char szRules[ 1500 ];
	int minplayers = 0, maxplayers = 0;
	strcpy( szFirstMapInList, "hldm1" );  // the absolute default level is hldm1

	int	curplayers;
	BOOL do_cycle = TRUE;

	// find the map to change to
	char *mapcfile = (char*)CVAR_GET_STRING( "mapcyclefile" );
	ASSERT( mapcfile != NULL );

	szCommands[ 0 ] = '\0';
	szRules[ 0 ] = '\0';

	curplayers = CountPlayers();

	// Has the map cycle filename changed?
	if ( stricmp( mapcfile, szPreviousMapCycleFile ) )
	{
		strcpy( szPreviousMapCycleFile, mapcfile );

		DestroyMapCycle( &mapcycle );

		if ( !ReloadMapCycleFile( mapcfile, &mapcycle ) || ( !mapcycle.items ) )
		{
			ALERT( at_console, "Unable to load map cycle file %s\n", mapcfile );
			do_cycle = FALSE;
		}
	}

	if ( do_cycle && mapcycle.items )
	{
		BOOL keeplooking = FALSE;
		BOOL found = FALSE;
		mapcycle_item_s *item;

		// Assume current map
		strcpy( szNextMap, STRING(gpGlobals->mapname) );
		strcpy( szFirstMapInList, STRING(gpGlobals->mapname) );

		// Traverse list
		for ( item = mapcycle.next_item; item->next != mapcycle.next_item; item = item->next )
		{
			keeplooking = FALSE;

			ASSERT( item != NULL );

			if ( item->minplayers != 0 )
			{
				if ( curplayers >= item->minplayers )
				{
					found = TRUE;
					minplayers = item->minplayers;
				}
				else
					keeplooking = TRUE;
			}

			if ( item->maxplayers != 0 )
			{
				if ( curplayers <= item->maxplayers )
				{
					found = TRUE;
					maxplayers = item->maxplayers;
				}
				else
					keeplooking = TRUE;
			}

			if ( keeplooking )
				continue;

			found = TRUE;
			break;
		}

		if ( !found )
			item = mapcycle.next_item;		
		
		// Increment next item pointer
		mapcycle.next_item = item->next;

		// Perform logic on current item
		strcpy( szNextMap, item->mapname );

		ExtractCommandString( item->rulebuffer, szCommands );
		strcpy( szRules, item->rulebuffer );
	}

	if ( !IS_MAP_VALID(szNextMap) )
		strcpy( szNextMap, szFirstMapInList );

	g_fGameOver = TRUE;

	ALERT( at_console, "CHANGE LEVEL: %s\n", szNextMap );

	if ( minplayers || maxplayers )
		ALERT( at_console, "PLAYER COUNT:  min %i max %i current %i\n", minplayers, maxplayers, curplayers );

	if( szRules != '\0' )
		ALERT( at_console, "RULES:  %s\n", szRules );
	
	CHANGE_LEVEL( szNextMap, NULL );

	if( szCommands[0] != '\0' )
		SERVER_COMMAND( szCommands );
}

#define MAX_MOTD_CHUNK	  60
#define MAX_MOTD_LENGTH   1536 // (MAX_MOTD_CHUNK * 4)

void CHalfLifeMultiplay :: SendMOTDToClient( edict_t *client )
{
	// read from the MOTD.txt file
	int length, char_count = 0;
	char *pFileList;
	char *aFileList = pFileList = (char*)LOAD_FILE( (char *)CVAR_GET_STRING( "motdfile" ), &length );

	// send the server name
	MESSAGE_BEGIN( MSG_ONE, gmsgServerName, NULL, client );
		WRITE_STRING( CVAR_GET_STRING("hostname") );
	MESSAGE_END();

	// Send the message of the day
	// read it chunk-by-chunk,  and send it in parts

	while ( pFileList && *pFileList && char_count < MAX_MOTD_LENGTH )
	{
		char chunk[MAX_MOTD_CHUNK+1];
		
		if ( strlen( pFileList ) < MAX_MOTD_CHUNK )
			strcpy( chunk, pFileList );
		else
		{
			strncpy( chunk, pFileList, MAX_MOTD_CHUNK );
			chunk[MAX_MOTD_CHUNK] = 0;		// strncpy doesn't always append the null terminator
		}

		char_count += strlen( chunk );
		if ( char_count < MAX_MOTD_LENGTH )
			pFileList = aFileList + char_count; 
		else
			*pFileList = 0;

		MESSAGE_BEGIN( MSG_ONE, gmsgMOTD, NULL, client );
			WRITE_BYTE( *pFileList ? FALSE : TRUE );	// FALSE means there is still more message to come
			WRITE_STRING( chunk );
		MESSAGE_END();
	}

	FREE_FILE( aFileList );
}
	

