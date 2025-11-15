#include "extdll.h"
#include "util.h"
#include "game/client.h"
#include "cbase.h"
#include "player.h"
#include "items.h"
#include "effects.h"
#include "weapons/weapons.h"
#include "entities/soundent.h"
#include "game/gamerules.h"
#include "game/teamplay_gamerules.h"
#include "animation.h"

#include "bot.h"

#include <sys/types.h>
#include <sys/stat.h>

extern DLL_GLOBAL ULONG g_ulModelIndexPlayer;

// Set in combat.cpp.  Used to pass the damage inflictor for death messages.
extern entvars_t *g_pevLastInflictor;

extern DLL_GLOBAL BOOL g_fGameOver;

#define PLAYER_SEARCH_RADIUS (float)40

int f_Observer = 0;  // flag to indicate if player is in observer mode
int f_botskill = 3;  // default bot skill level
int f_botdontshoot = 0;

Vector vWaypoint[MAX_WAYPOINTS];
int ConnectedPoints[MAX_WAYPOINTS][MAX_WAYPOINT_CONNECTIONS]; // max 5 connections per waypoint
int ConnectionsForPoint[MAX_WAYPOINTS]; // how many connections for this point
int PointFlag[MAX_WAYPOINTS];
int WaypointsLoaded = 0;

// waypoint flags
// note: could do this as bits, but no such functionality implemented.
// only one flag per point.
#define W_FLAG_SNIPER		1 // s
#define W_FLAG_CROUCH		2 // c
#define W_FLAG_FLASHLIGHT	3 // f
#define W_FLAG_JUMP			4 // j
#define W_FLAG_SNIPERCROUCH 5 // sc
#define W_FLAG_LADDER		6 // l

// array of bot respawn information
respawn_t bot_respawn[32] = {
   {FALSE, BOT_IDLE, "", "", "", NULL}, {FALSE, BOT_IDLE, "", "", "", NULL},
   {FALSE, BOT_IDLE, "", "", "", NULL}, {FALSE, BOT_IDLE, "", "", "", NULL},
   {FALSE, BOT_IDLE, "", "", "", NULL}, {FALSE, BOT_IDLE, "", "", "", NULL},
   {FALSE, BOT_IDLE, "", "", "", NULL}, {FALSE, BOT_IDLE, "", "", "", NULL},
   {FALSE, BOT_IDLE, "", "", "", NULL}, {FALSE, BOT_IDLE, "", "", "", NULL},
   {FALSE, BOT_IDLE, "", "", "", NULL}, {FALSE, BOT_IDLE, "", "", "", NULL},
   {FALSE, BOT_IDLE, "", "", "", NULL}, {FALSE, BOT_IDLE, "", "", "", NULL},
   {FALSE, BOT_IDLE, "", "", "", NULL}, {FALSE, BOT_IDLE, "", "", "", NULL},
   {FALSE, BOT_IDLE, "", "", "", NULL}, {FALSE, BOT_IDLE, "", "", "", NULL},
   {FALSE, BOT_IDLE, "", "", "", NULL}, {FALSE, BOT_IDLE, "", "", "", NULL},
   {FALSE, BOT_IDLE, "", "", "", NULL}, {FALSE, BOT_IDLE, "", "", "", NULL},
   {FALSE, BOT_IDLE, "", "", "", NULL}, {FALSE, BOT_IDLE, "", "", "", NULL},
   {FALSE, BOT_IDLE, "", "", "", NULL}, {FALSE, BOT_IDLE, "", "", "", NULL},
   {FALSE, BOT_IDLE, "", "", "", NULL}, {FALSE, BOT_IDLE, "", "", "", NULL},
   {FALSE, BOT_IDLE, "", "", "", NULL}, {FALSE, BOT_IDLE, "", "", "", NULL}};

#define MAX_SKINS 18

// indicate which models are currently used for random model allocation
BOOL skin_used[MAX_SKINS] =
{
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
};

// store the names of the models...
static const char *bot_skins[MAX_SKINS] = 
{
	"a",
	"b",
	"c",
	"d",
	"e",
	"f",
	"g",
	"h",
	"i",
	"j",
	"k",
	"l",
	"m",
	"n",
	"o",
	"p",
	"q",
	"r",
};

// store the player names for each of the models...
static const char *bot_names[MAX_SKINS] =
{
	"Jackal",
	"Weasel",
	"Oscar",
	"Cutter",
	"Slemmy",
	"Harmless Potato",
	"Connor",
	"Maverick",
	"Bandit",
	"Lunatic",
	"Mike Oxlong",
	"Dixie Normous",
	"Hugh Jass",
	"Deez Nuts",
	"Punisher",
	"Botzilla",
	"Nut Jobber",
	"Trash Can Kicker",
};

// how often (out of 1000 times) the bot will pause, based on bot skill
static const float pause_frequency[5] = 
{
	4,
	7,
	10,
	15,
	20
};

// how long the bot will delay when paused, based on bot skill
static const float pause_time[5][2] =
{
	{0.2f, 0.5f},
	{0.5f, 1.0f},
	{0.7f, 1.3f},
	{1.0f, 1.7f},
	{1.2f, 2.0f}
};

extern ammo_check_t ammo_check[];

// sounds for TakeDamage speaking effects...
char hgrunt_sounds[][30] = { HG_SND1, HG_SND2, HG_SND3, HG_SND4, HG_SND5 };
char barney_sounds[][30] = { BA_SND1, BA_SND2, BA_SND3, BA_SND4, BA_SND5 };
char scientist_sounds[][30] = { SC_SND1, SC_SND2, SC_SND3, SC_SND4, SC_SND5 };

bool bWaypointsLoaded = false;

bool GotFlag( const char* token )
{
	if( !strcmp( token, "s" ) )
	{
		PointFlag[WaypointsLoaded] = W_FLAG_SNIPER;
		return true;
	}
	else if( !strcmp( token, "sc" ) )
	{
		PointFlag[WaypointsLoaded] = W_FLAG_SNIPERCROUCH;
		return true;
	}
	else if( !strcmp( token, "c" ) )
	{
		PointFlag[WaypointsLoaded] = W_FLAG_CROUCH;
		return true;
	}
	else if( !strcmp( token, "f" ) )
	{
		PointFlag[WaypointsLoaded] = W_FLAG_FLASHLIGHT;
		return true;
	}
	else if( !strcmp( token, "j" ) )
	{
		PointFlag[WaypointsLoaded] = W_FLAG_JUMP;
		return true;
	}
	else if( !strcmp( token, "l" ) )
	{
		PointFlag[WaypointsLoaded] = W_FLAG_LADDER;
		return true;
	}

	return false;
}

void BotLoadWaypointFile(void)
{
	char szFilename[MAX_PATH];
	Q_snprintf( szFilename, sizeof( szFilename ), "maps/%s.wp", STRING( gpGlobals->mapname ) );
	char *pWPFile = (char *)LOAD_FILE( szFilename, NULL );
	char token[256];
	char *afile = pWPFile;

	if( !afile )
	{
		ALERT( at_aiconsole, "Can't load waypoint file for map %s.\n", STRING( gpGlobals->mapname ) );
		bWaypointsLoaded = true; // don't check again
		return;
	}

	memset( vWaypoint, 0, sizeof( vWaypoint ) );
	memset( ConnectedPoints, -1, sizeof( ConnectedPoints ) );
	memset( ConnectionsForPoint, 0, sizeof( ConnectionsForPoint ) );
	memset( PointFlag, 0, sizeof( PointFlag ) );
	WaypointsLoaded = 0;
	int linecount = 0;

	while( ((afile = COM_ParseFile( afile, token )) != NULL) && (WaypointsLoaded < MAX_WAYPOINTS) )
	{
		// load the vector - the location of a point
		UTIL_StringToVector( vWaypoint[WaypointsLoaded], token );

		// now try to parse a point number (the number of a point connected to this point)
		afile = COM_ParseLine( afile, token );
		if( afile && token[0] > 0 ) // found the first connection
		{
			ConnectedPoints[WaypointsLoaded][0] = Q_atoi( token );

			if( ConnectedPoints[WaypointsLoaded][0] > -1 )
				ConnectionsForPoint[WaypointsLoaded] = 1;
			else
				ALERT( at_aiconsole, "Point %i has no connections.\n", WaypointsLoaded );

			// second connected point
			afile = COM_ParseLine( afile, token );
			if( afile && token[0] > 0 )
			{
				if( GotFlag(token) )
				{
					WaypointsLoaded++;
					continue;
				}
				
				ConnectedPoints[WaypointsLoaded][1] = Q_atoi( token );
				ConnectionsForPoint[WaypointsLoaded] = 2;

				// third connected point
				afile = COM_ParseLine( afile, token );
				if( afile && token[0] > 0 )
				{	
					if( GotFlag( token ) )
					{
						WaypointsLoaded++;
						continue;
					}
					
					ConnectedPoints[WaypointsLoaded][2] = Q_atoi( token );
					ConnectionsForPoint[WaypointsLoaded] = 3;

					// 4th
					afile = COM_ParseLine( afile, token );
					if( afile && token[0] > 0 )
					{
						if( GotFlag( token ) )
						{
							WaypointsLoaded++;
							continue;
						}
						
						ConnectedPoints[WaypointsLoaded][3] = Q_atoi( token );
						ConnectionsForPoint[WaypointsLoaded] = 4;

						// 5th and last
						afile = COM_ParseLine( afile, token );
						if( afile && token[0] > 0 )
						{
							if( GotFlag( token ) )
							{
								WaypointsLoaded++;
								continue;
							}
							
							ConnectedPoints[WaypointsLoaded][4] = Q_atoi( token );
							ConnectionsForPoint[WaypointsLoaded] = 5;

							// now, flags
							afile = COM_ParseLine( afile, token );
							if( afile && token[0] > 0 )
							{
								GotFlag( token );
							}
						}
					}
				}
			}
		}
		else
		{
			ALERT( at_warning, "Point %i with no connections!\n", linecount );
			linecount++;
			continue;
		}

		WaypointsLoaded++;
	}

	FREE_FILE( pWPFile );

	if( WaypointsLoaded == 0 )
	{
		ALERT( at_error, "Waypoint file with no connections!\n" );
		bWaypointsLoaded = true; // don't check again
		return;
	}

	ALERT( at_aiconsole, "Waypoints for map %s are loaded, %i points.\n", STRING( gpGlobals->mapname ), WaypointsLoaded - 1 );

	bWaypointsLoaded = true;
}

void BotGenerateWaypointFile( int LookDistance ) // this can be called via "bot_generatewp" command
{
	char szFilename[MAX_PATH];
	Q_snprintf( szFilename, sizeof( szFilename ), "maps/%s_points.txt", STRING( gpGlobals->mapname ) );
	char *pWPFile = (char *)LOAD_FILE( szFilename, NULL );
	char token[256];
	char *afile = pWPFile;

	if( !afile )
	{
		ALERT( at_error, "Can't load point file %s_points.txt to generate waypoints!\n", STRING( gpGlobals->mapname ) );
		return;
	}
	
	memset( vWaypoint, 0, sizeof( vWaypoint ) );
	memset( ConnectedPoints, -1, sizeof( ConnectedPoints ) );
	memset( ConnectionsForPoint, 0, sizeof( ConnectionsForPoint ) );
	memset( PointFlag, 0, sizeof( PointFlag ) );
	WaypointsLoaded = 0;

	// load the points (vectors)
	while( ((afile = COM_ParseFile( afile, token )) != NULL) && (WaypointsLoaded < MAX_WAYPOINTS) )
	{
		// load the vector - the location of a point
		UTIL_StringToVector( vWaypoint[WaypointsLoaded], token );
		WaypointsLoaded++;
	}

	ALERT( at_aiconsole, "Building waypoint file for map %s (%i points)...\n", STRING( gpGlobals->mapname ), WaypointsLoaded - 1 );

	// for each loaded waypoint...
	for( int w = 0; w < WaypointsLoaded; w++ )
	{
		// ...find all nearby points
		int wcount = 0;

		for( int c = 0; c < WaypointsLoaded; c++ )
		{
			if( wcount == MAX_WAYPOINT_CONNECTIONS )
				break;
						
			if( c == w )
				continue;

			if( (vWaypoint[w] - vWaypoint[c]).Length() > LookDistance )
				continue;

			// this point is close enough. check if we can see it
			TraceResult trw;

			UTIL_TraceLine( vWaypoint[w], vWaypoint[c], ignore_monsters, dont_ignore_glass, INDEXENT( 1 ), &trw );

			// visible!
			if( trw.flFraction == 1.0f )
			{
				// that point is now connected to this point
				ConnectedPoints[w][wcount] = c;
				wcount++;
				ConnectionsForPoint[w] = wcount;
			}
		}
	}

	// the connections were built. now we need to print the file...
	char szFullPath[MAX_PATH];
	//	GET_GAME_DIR( szFullPath ); UNDONE!!!!!!!!!!!!
	Q_snprintf( szFullPath, sizeof( szFullPath ), "diffusion/maps/%s.wp", STRING( gpGlobals->mapname ) );
	FILE *newWPfile = fopen( szFullPath, "w" ); // empty file
	if( newWPfile )
	{
		for( int print = 0; print < WaypointsLoaded; print++ )
		{
			if( ConnectionsForPoint[print] == 0 )
				fprintf( newWPfile, "\"%i %i %i\" -1\n", (int)vWaypoint[print].x, (int)vWaypoint[print].y, (int)vWaypoint[print].z );
			else if( ConnectionsForPoint[print] == 1 )
				fprintf( newWPfile, "\"%i %i %i\" %i\n", (int)vWaypoint[print].x, (int)vWaypoint[print].y, (int)vWaypoint[print].z, ConnectedPoints[print][0] );
			else if( ConnectionsForPoint[print] == 2 )
				fprintf( newWPfile, "\"%i %i %i\" %i %i\n", (int)vWaypoint[print].x, (int)vWaypoint[print].y, (int)vWaypoint[print].z, ConnectedPoints[print][0], ConnectedPoints[print][1] );
			else if( ConnectionsForPoint[print] == 3 )
				fprintf( newWPfile, "\"%i %i %i\" %i %i %i\n", (int)vWaypoint[print].x, (int)vWaypoint[print].y, (int)vWaypoint[print].z, ConnectedPoints[print][0], ConnectedPoints[print][1], ConnectedPoints[print][2] );
			else if( ConnectionsForPoint[print] == 4 )
				fprintf( newWPfile, "\"%i %i %i\" %i %i %i %i\n", (int)vWaypoint[print].x, (int)vWaypoint[print].y, (int)vWaypoint[print].z, ConnectedPoints[print][0], ConnectedPoints[print][1], ConnectedPoints[print][2], ConnectedPoints[print][3] );
			else if( ConnectionsForPoint[print] == 5 )
				fprintf( newWPfile, "\"%i %i %i\" %i %i %i %i %i\n", (int)vWaypoint[print].x, (int)vWaypoint[print].y, (int)vWaypoint[print].z, ConnectedPoints[print][0], ConnectedPoints[print][1], ConnectedPoints[print][2], ConnectedPoints[print][3], ConnectedPoints[print][4] );

			// flags are not generated nor printed. must be set up by user, manually
		}

		fclose( newWPfile );

		ALERT( at_aiconsole, "Waypoints for map %s are generated, .wp file written.\n", STRING( gpGlobals->mapname ) );
	}
	else
		ALERT( at_error, "Can't write new waypoint file!\n" );

	FREE_FILE( pWPFile );
}


LINK_ENTITY_TO_CLASS( bot, CBot );


inline edict_t *CREATE_FAKE_CLIENT( const char *netname )
{
   return (*g_engfuncs.pfnCreateFakeClient)( netname );
}

inline char *GET_INFOBUFFER( edict_t *e )
{
   return (*g_engfuncs.pfnGetInfoKeyBuffer)( e );
}

inline char *GET_INFO_KEY_VALUE( char *infobuffer, const char *key )
{
   return (g_engfuncs.pfnInfoKeyValue( infobuffer, key ));
}

inline void SET_CLIENT_KEY_VALUE( int clientIndex, char *infobuffer,
								  const char *key, const char *value )
{
   (*g_engfuncs.pfnSetClientKeyValue)( clientIndex, infobuffer, key, value );
}


void BotDebug( const char *buffer )
{
	// print out debug messages to the HUD of all players
   // this allows you to print messages from bots to your display
   // as you are playing the game.  Use STRING(pev->netname) in
   // buffer to see the name of the bot.

   UTIL_ClientPrintAll( HUD_PRINTNOTIFY, buffer );
}

void BotCreate(const char *skin, const char *name, const char *skill)
{
	edict_t *BotEnt;
	CBot *BotClass;
	int skill_level;
	char c_skill[2];
	char c_skin[BOT_SKIN_LEN+1];
	char c_name[BOT_NAME_LEN+1];
	char c_index[4];
	int i, j;
	size_t length;
	int index;  // index into array of predefined names
	bool found = false;

	if ((skin == NULL) || (*skin == 0))
	{
		// pick a random skin
		index = RANDOM_LONG( 0, MAX_SKINS - 1 );

		// check if this skin has already been used...
		while (skin_used[index] == TRUE)
		{
			index++;

			if (index == MAX_SKINS)
			index = 0;
		}

		skin_used[index] = TRUE;

		// check if all skins are used...
		for (i = 0; i < MAX_SKINS; i++)
		{
			if (skin_used[i] == FALSE)
				break;
		}

		// if all skins are used, reset used to FALSE for next selection
		if (i == MAX_SKINS)
		{
			for (i = 0; i < MAX_SKINS; i++)
				skin_used[i] = FALSE;
		}

		Q_strncpy( c_skin, bot_skins[index], sizeof( c_skin ));
   }
   else
   {
		Q_strncpy( c_skin, skin, BOT_SKIN_LEN);
		c_skin[BOT_SKIN_LEN] = 0;  // make sure c_skin is null terminated
   }

	for (i = 0; c_skin[i] != 0; i++)
		c_skin[i] = tolower( c_skin[i] );  // convert to all lowercase

	index = 0;

	while ((!found) && (index < MAX_SKINS))
	{
		if (strcmp(c_skin, bot_skins[index]) == 0)
			found = TRUE;
		else
			index++;
	}

   if( found )
   {
	  if ((name != NULL) && (*name != 0))
	  {
		 Q_strncpy( c_name, name, BOT_NAME_LEN );
		 c_name[BOT_NAME_LEN] = 0;  // make sure c_name is null terminated
	  }
	  else
	  {
		 Q_strncpy( c_name, bot_names[index], sizeof( c_name ));
	  }
   }
   else
   {
	  char dir_name[32];
	  char filename[128];

	  struct stat stat_str;

	  GET_GAME_DIR(dir_name);

	  Q_snprintf(filename, sizeof( filename ), "%s\\models\\player\\%s", dir_name, c_skin);

	  if (stat(filename, &stat_str) != 0)
	  {
		 Q_snprintf( filename, sizeof( filename ), "valve\\models\\player\\%s", c_skin );
		 if (stat(filename, &stat_str) != 0)
		 {
			char err_msg[80];

			Q_snprintf( err_msg, sizeof( err_msg ), "model \"%s\" is unknown.\n", c_skin );
			UTIL_ClientPrintAll( HUD_PRINTNOTIFY, err_msg );
			if (IS_DEDICATED_SERVER())
			   printf(err_msg);

			UTIL_ClientPrintAll( HUD_PRINTNOTIFY,
			   "use barney, gina, gman, gordon, helmet, hgrunt,\n");
			if (IS_DEDICATED_SERVER())
			   printf("use barney, gina, gman, gordon, helmet, hgrunt,\n");
			UTIL_ClientPrintAll( HUD_PRINTNOTIFY,
			   "    recon, robo, scientist, or zombie\n");
			if (IS_DEDICATED_SERVER())
			   printf("    recon, robo, scientist, or zombie\n");
			return;
		 }
	  }

	  // copy the name of the model to the bot's name...
	  Q_strncpy( c_name, skin, BOT_SKIN_LEN);
	  c_name[BOT_SKIN_LEN] = 0;  // make sure c_skin is null terminated

	  length = strlen( c_name );

	  // remove any illegal characters from name...
	  for( i = 0; i < length; i++ )
	  {
		  if( (c_name[i] <= ' ') || (c_name[i] > '~') ||
			  (c_name[i] == '"') )
		  {
			  for( j = i; j < length; j++ )  // shuffle chars left (and null)
				  c_name[j] = c_name[j + 1];
			  length--;
		  }
	  }
   }

	skill_level = 0;

	if( (skill != NULL) && (*skill != 0) )
		skill_level = Q_atoi( skill );
	else
		skill_level = f_botskill;

	if ((skill_level < 1) || (skill_level > 5))
		skill_level = f_botskill;

	Q_snprintf( c_skill, sizeof( c_skill ), "%d", skill_level );
	
	BotEnt = CREATE_FAKE_CLIENT( c_name );

	if (FNullEnt( BotEnt ))
	{
		UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "Max. Players reached.  Can't create bot!\n");

		if (IS_DEDICATED_SERVER())
			printf("Max. Players reached.  Can't create bot!\n");
	}
	else
	{
		char ptr[128];  // allocate space for message from ClientConnect
		char *infobuffer;
		int clientIndex;

		if( BotEnt->pvPrivateData != NULL )
			FREE_PRIVATE( BotEnt );
		BotEnt->pvPrivateData = NULL;
		BotEnt->v.frags = 0;

		index = 0;
		while ((bot_respawn[index].is_used) && (index < 32))
			index++;

		if (index >= 32)
		{
			UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "Can't create bot!\n");
			return;
		}

		sprintf(c_index, "%d", index);

		bot_respawn[index].is_used = TRUE;  // this slot is used

		// don't store the name here, it might change if same as another
		Q_strncpy(bot_respawn[index].skin, c_skin, sizeof( bot_respawn[index].skin ));
		Q_strncpy(bot_respawn[index].skill, c_skill, sizeof( bot_respawn[index].skill ));

		sprintf(ptr, "Creating bot \"%s\" using model %s with skill=%d\n", c_name, c_skin, skill_level);
		UTIL_ClientPrintAll( HUD_PRINTNOTIFY, ptr);

		if (IS_DEDICATED_SERVER())
			printf("%s", ptr);

		BotClass = GetClassPtr( (CBot *) VARS(BotEnt) );
		infobuffer = GET_INFOBUFFER( BotClass->edict( ) );
		clientIndex = ENTINDEX( BotClass->edict() );// BotClass->entindex();

		CHalfLifeTeamplay *TeamGameRules = NULL;
		if( g_pGameRules->IsTeamplay() )
		{
			TeamGameRules = (CHalfLifeTeamplay *)g_pGameRules;
			SET_CLIENT_KEY_VALUE( clientIndex, infobuffer, "model", (char*)TeamGameRules->TeamWithFewestPlayers() );
		}
		else
			SET_CLIENT_KEY_VALUE( clientIndex, infobuffer, "model", "robo" );

		SET_CLIENT_KEY_VALUE( clientIndex, infobuffer, "skill", c_skill );
		SET_CLIENT_KEY_VALUE( clientIndex, infobuffer, "index", c_index );

		// randomize the skin
		char TmpText[8];
		sprintf( TmpText, "%d", RANDOM_LONG( 0, 255 ) );
		SET_CLIENT_KEY_VALUE( clientIndex, infobuffer, "topcolor", TmpText );
		sprintf( TmpText, "%d", RANDOM_LONG( 0, 255 ) );
		SET_CLIENT_KEY_VALUE( clientIndex, infobuffer, "bottomcolor", TmpText );
		
		ClientConnect( BotClass->edict( ), c_name, "127.0.0.1", ptr );
		
	//	DispatchSpawn( BotClass->edict( ) );
		ClientPutInServer( BotClass->edict() );
	}
}

void CBot::BotFixIdealPitch( void )
{
	// check for wrap around of angle...
	if( pev->idealpitch > 180 )
		pev->idealpitch -= 360;

	if( pev->idealpitch < -180 )
		pev->idealpitch += 360;
}

void CBot::BotFixIdealYaw( void )
{
	// check for wrap around of angle...
	if( pev->ideal_yaw > 180 )
		pev->ideal_yaw -= 360;

	if( pev->ideal_yaw < -180 )
		pev->ideal_yaw += 360;
}

void CBot::Spawn( )
{
	SetFlag( F_BOT );

	CBasePlayer::Spawn();

	pev->flags |= FL_FAKECLIENT;

	char *infobuffer = GET_INFOBUFFER( edict() );
	
	// set the respawn index value based on key from BotCreate
	respawn_index = Q_atoi( GET_INFO_KEY_VALUE( infobuffer, "index" ) );

	bot_respawn[respawn_index].pBot = (CBasePlayer *)this;

	// get the bot's name and save it in respawn array...
	strcpy(bot_respawn[respawn_index].name, STRING(pev->netname));

	bot_respawn[respawn_index].state = BOT_IDLE;

	pev->ideal_yaw = pev->v_angle.y;
	pev->yaw_speed = BOT_YAW_SPEED;

	// bot starts out in "paused" state since it hasn't moved yet...
	bot_was_paused = TRUE;
	v_prev_origin = GetAbsOrigin();

	f_shoot_time = 0;

	// get bot's skill level (0=good, 4=bad)
	bot_skill = Q_atoi( GET_INFO_KEY_VALUE( infobuffer, "skill" ) );
	bot_skill--;  // make 0 based for array index (now 0-4)

	f_max_speed = CVAR_GET_FLOAT("sv_maxspeed");

	f_speed_check_time = gpGlobals->time + 2.0;

	ladder_dir = 0;

	// pick a wander direction (50% of the time to the left, 50% to the right)
	if (RANDOM_LONG(1, 100) <= 50)
		wander_dir = WANDER_LEFT;
	else
		wander_dir = WANDER_RIGHT;

	f_pause_time = 0;
	f_find_item = gpGlobals->time + 1;

	bot_model = 0;
	if ((strcmp( model_name, "hgrunt" ) == 0) ||
	(strcmp( model_name, "recon" ) == 0))
	{
		bot_model = MODEL_HGRUNT;
	}
	else if (strcmp( model_name, "barney") == 0)
	{
		bot_model = MODEL_BARNEY;
	}
	else if (strcmp( model_name, "scientist") == 0)
	{
		bot_model = MODEL_SCIENTIST;
	}

	f_pain_time = gpGlobals->time + 5.0;

	b_use_health_station = FALSE;
	b_use_HEV_station = FALSE;
	b_use_button = FALSE;
	f_use_button_time = 0;
	b_lift_moving = FALSE;
	f_use_ladder_time = 0;
	f_fire_gauss = -1;  // -1 means not charging gauss gun

	b_see_tripmine = FALSE;
	b_shoot_tripmine = FALSE;

	f_weapon_inventory_time = 0;
	f_dont_avoid_wall_time = 0;
	f_bot_use_time = 0;
	f_wall_on_right = 0;
	f_wall_on_left = 0;

	pBotEnemy = NULL;
	pBotUser = NULL;
	pBotPickupItem = NULL;

	memset( vNearbyWaypoints, 0, sizeof( vNearbyWaypoints ) );
	iNearbyWaypointNum = 0;
	iNearbyWaypoints = 0;
	CurrentWaypointID = -1;
	f_trynextwaypoint_time = 0;
	iWaypointHistory[0] = -1;
	iWaypointHistory[1] = -1;

	last_electroblast_time = 0;
	next_dash_time = 0;

	camping_turn_time = 0;
	IsCamping = false;
	camping_end_time = 0;
	next_camping_time = 0;
	CrouchMove = false;

	f_duck_release_time = 0;

	IsHoldingAttackButton = false;
	stop_hold_attack_button_time = 0;

	if( g_pGameRules->IsTeamplay() ) // is team play enabled?
	{
		ObjectCaps();
		SetUse( &CBot::BotUse );
	}
}


int CBot::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker,
					  float flDamage, int bitsDamageType )
{
   
	CBaseEntity *pAttacker = CBaseEntity::Instance(pevAttacker);
   char sound[40];
   int ret_damage;

   // do the damage first...
   ret_damage = CBasePlayer::TakeDamage( pevInflictor, pevAttacker, flDamage,
										 bitsDamageType );

   // if the bot doesn't have an enemy and someone is shooting at it then
   // turn in the attacker's direction...
   if (pBotEnemy == NULL)
   {
	  // face the attacker...
	  Vector v_enemy = pAttacker->GetAbsOrigin() - GetAbsOrigin();
	  Vector bot_angles = UTIL_VecToAngles( v_enemy );

	  pev->ideal_yaw = bot_angles.y;

	  // check for wrap around of angle...
	  BotFixIdealYaw();

	  // stop using health or HEV stations...
	  b_use_health_station = FALSE;
	  b_use_HEV_station = FALSE;
   }

   // check if bot model is known, attacker is not a bot,
   // time for pain sound, and bot has some of health left...

   if ((bot_model != 0) && (pAttacker->IsNetClient()) &&
	   (f_pain_time <= gpGlobals->time) && (pev->health > 0) &&
	   ( !IS_DEDICATED_SERVER() ))
   {
	  float distance = (pAttacker->GetAbsOrigin() - GetAbsOrigin()).Length( );

	  // check if the distance to attacker is close enough (otherwise
	  // the attacker is too far away to hear the pain sounds)

	  if (distance <= 400)
	  {
		 // speak pain sounds about 50% of the time
		 if (RANDOM_LONG(1, 100) <= 50)
		 {
			f_pain_time = gpGlobals->time + 5.0;

			if (bot_model == MODEL_HGRUNT)
			   strcpy( sound, hgrunt_sounds[RANDOM_LONG(0,4)] );
			else if (bot_model == MODEL_BARNEY)
			   strcpy( sound, barney_sounds[RANDOM_LONG(0,4)] );
			else if (bot_model == MODEL_SCIENTIST)
			   strcpy( sound, scientist_sounds[RANDOM_LONG(0,4)] );

			EMIT_SOUND(ENT(pevAttacker), CHAN_VOICE, sound,
					   RANDOM_FLOAT(0.9, 1.0), ATTN_NORM);
		 }
	  }
   }

   return ret_damage;
}


void CBot::BotUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// check the bot and player are on the same team...
	if (UTIL_TeamsMatch(g_pGameRules->GetTeamID(this),
						g_pGameRules->GetTeamID(pActivator)))
	{
		if (pBotEnemy == NULL)  // is bot NOT currently engaged in combat?
		{
			if (pBotUser == NULL)  // does bot NOT have a "user"
			{
				// tell teammate that bot will cover them...
				EMIT_SOUND(ENT(pActivator->pev), CHAN_VOICE, USE_TEAMPLAY_SND, 1.0, ATTN_NORM);

				pBotUser = pActivator;
				f_bot_use_time = gpGlobals->time;
			}
			else
			{
				// tell teammate that you'll see them later..
				EMIT_SOUND(ENT(pActivator->pev), CHAN_VOICE, USE_TEAMPLAY_LATER_SND, 1.0, ATTN_NORM);

				pBotUser = NULL;
				f_bot_use_time = 0;
			}
		}
		else
		{
			EMIT_SOUND(ENT(pActivator->pev), CHAN_VOICE, USE_TEAMPLAY_ENEMY_SND, 1.0, ATTN_NORM);
		}
	}
}

int CBot::BotInFieldOfView(Vector dest)
{
   // find angles from source to destination...
   Vector entity_angles = UTIL_VecToAngles( dest );

   // make yaw angle 0 to 360 degrees if negative...
   if (entity_angles.y < 0)
	  entity_angles.y += 360;

   // get bot's current view angle...
   float view_angle = pev->v_angle.y;

   // make view angle 0 to 360 degrees if negative...
   if (view_angle < 0)
	  view_angle += 360;

   // return the absolute value of angle to destination entity
   // zero degrees means straight ahead,  45 degrees to the left or
   // 45 degrees to the right is the limit of the normal view angle

   return abs((int)view_angle - (int)entity_angles.y);
}


BOOL CBot::BotEntityIsVisible( Vector dest )
{
	Vector BotEyes = GetAbsOrigin() + pev->view_ofs;
	
	// can't jump here
	if( dest.z - GetAbsOrigin().z > 45.0f )
		return FALSE;
	
	TraceResult tr;

   // trace a line from bot's eyes to destination...
   UTIL_TraceLine( GetAbsOrigin() + pev->view_ofs, dest, ignore_monsters,
				   ENT(pev), &tr );

   // check if line of sight to object is not blocked (i.e. visible)
   if (tr.flFraction >= 1.0)
	  return TRUE;
   else
	  return FALSE;
}


float CBot::BotChangeYaw( float speed )
{
	float ideal;
	float current;
	float current_180;  // current +/- 180 degrees
	float diff;

	// turn from the current v_angle yaw to the ideal_yaw by selecting
	// the quickest way to turn to face that direction
   
	current = pev->v_angle.y;
	ideal = pev->ideal_yaw;

	// find the difference in the current and ideal angle
	diff = abs(current - ideal);

	// check if the bot is already facing the ideal_yaw direction...
	if (diff <= 1)
		return diff;  // return number of degrees turned

	// check if difference is less than the max degrees per turn
	if (diff < speed)
		speed = diff;  // just need to turn a little bit (less than max)

	// here we have four cases, both angle positive, one positive and
	// the other negative, one negative and the other positive, or
	// both negative.  handle each case separately...

	if ((current >= 0) && (ideal >= 0))  // both positive
	{
		if (current > ideal)
			current -= speed;
		else
			current += speed;
	}
	else if ((current >= 0) && (ideal < 0))
	{
		current_180 = current - 180;

		if (current_180 > ideal)
			current += speed;
		else
			current -= speed;
	}
	else if ((current < 0) && (ideal >= 0))
	{
		current_180 = current + 180;
		if (current_180 > ideal)
			current += speed;
		else
			current -= speed;
	}
	else  // (current < 0) && (ideal < 0)  both negative
	{
		if (current > ideal)
			current -= speed;
		else
			current += speed;
	}

	// check for wrap around of angle...
	if (current > 180)
		current -= 360;
	if (current < -180)
		current += 360;

	pev->v_angle.y = current;

	// don't zero it, we can be aiming
	if( !pBotEnemy )
		pev->angles.x = 0;

	pev->angles.y = pev->v_angle.y;
	pev->angles.z = 0;

	return speed;  // return number of degrees turned
}


void CBot::BotOnLadder( void )
{
	// moves the bot up or down a ladder.  if the bot can't move
	// (i.e. get's stuck with someone else on ladder), the bot will
	// change directions and go the other way on the ladder.

	if (ladder_dir == LADDER_UP)  // is the bot currently going up?
	{
		pev->v_angle.x = -60;  // look upwards

		// check if the bot hasn't moved much since the last location...
		if (moved_distance <= 0.25)
		{
			// the bot must be stuck, change directions...
			pev->v_angle.x = 60;  // look downwards
			ladder_dir = LADDER_DOWN;
		}
	}
	else if (ladder_dir == LADDER_DOWN)  // is the bot currently going down?
	{
		pev->v_angle.x = 60;  // look downwards

		// check if the bot hasn't moved much since the last location...
		if (moved_distance <= 0.25)
		{
			// the bot must be stuck, change directions...
			pev->v_angle.x = -60;  // look upwards
			ladder_dir = LADDER_UP;
		}
	}
	else  // the bot hasn't picked a direction yet, try going up...
	{
		pev->v_angle.x = -60;  // look upwards
		ladder_dir = LADDER_UP;
	}

	// move forward (i.e. in the direction the bot is looking, up or down)
	pev->button |= IN_FORWARD;
}


void CBot::BotUnderWater( void )
{
   // handle movements under water.  right now, just try to keep from
   // drowning by swimming up towards the surface and look to see if
   // there is a surface the bot can jump up onto to get out of the
   // water.  bots DON'T like water!

   Vector v_src, v_forward;
   TraceResult tr;
   int contents;

   // swim up towards the surface
   pev->v_angle.x = -60;  // look upwards

   // move forward (i.e. in the direction the bot is looking, up or down)
   pev->button |= IN_FORWARD;

   // set gpGlobals angles based on current view angle (for TraceLine)
   UTIL_MakeVectors( pev->v_angle );

   // look from eye position straight forward (remember: the bot is looking
   // upwards at a 60 degree angle so TraceLine will go out and up...

   v_src = GetAbsOrigin() + pev->view_ofs;  // EyePosition()
   v_forward = v_src + gpGlobals->v_forward * 90;

   // trace from the bot's eyes straight forward...
   UTIL_TraceLine( v_src, v_forward, dont_ignore_monsters, ENT(pev), &tr);

   // check if the trace didn't hit anything (i.e. nothing in the way)...
   if (tr.flFraction >= 1.0)
   {
	  // find out what the contents is of the end of the trace...
	  contents = UTIL_PointContents( tr.vecEndPos );

	  // check if the trace endpoint is in open space...
	  if (contents == CONTENTS_EMPTY)
	  {
		 // ok so far, we are at the surface of the water, continue...

		 v_src = tr.vecEndPos;
		 v_forward = v_src;
		 v_forward.z -= 90;

		 // trace from the previous end point straight down...
		 UTIL_TraceLine( v_src, v_forward, dont_ignore_monsters,
						 ENT(pev), &tr);

		 // check if the trace hit something...
		 if (tr.flFraction < 1.0)
		 {
			contents = UTIL_PointContents( tr.vecEndPos );

			// if contents isn't water then assume it's land, jump!
			if (contents != CONTENTS_WATER)
			{
			   pev->button |= IN_JUMP;
			}
		 }
	  }
   }
}

void CBot::BotFindItem( void )
{
	CBaseEntity *pEntity = NULL;
   CBaseEntity *pPickupEntity = NULL;
   Vector pickup_origin;
   Vector entity_origin;
   float radius = 400;
   BOOL can_pickup;
   float min_distance;
   char item_name[40];
   TraceResult tr;
   Vector vecStart;
   Vector vecEnd;
   int angle_to_entity;

   pBotPickupItem = NULL;

   // do not look for items when camping
	if( IsCamping )
		return;

   min_distance = radius + 1.0;

   while ((pEntity = UTIL_FindEntityInSphere( pEntity, GetAbsOrigin(), radius )) != NULL)
   {
	  can_pickup = FALSE;  // assume can't use it until known otherwise

	  strcpy(item_name, STRING(pEntity->pev->classname));

	  // see if this is a "func_" type of entity (func_button, etc.)...
	  if (strncmp("func_", item_name, 5) == 0)
	  {
		 // BModels have 0,0,0 for origin so must use VecBModelOrigin...
		 entity_origin = VecBModelOrigin(pEntity->pev);

		 vecStart = GetAbsOrigin() + pev->view_ofs;
		 vecEnd = entity_origin;

		 angle_to_entity = BotInFieldOfView( vecEnd - vecStart );

		 // check if entity is outside field of view (+/- 45 degrees)
		 if (angle_to_entity > 45)
			continue;  // skip this item if bot can't "see" it

		 // check if entity is a ladder (ladders are a special case)...
		 // diffusion - don't use, try waypoints...
		 if( 0 ) // (strcmp("func_ladder", item_name) == 0)
		 {
			// force ladder origin to same z coordinate as bot since
			// the VecBModelOrigin is the center of the ladder.  For
			// LONG ladders, the center MAY be hundreds of units above
			// the bot.  Fake an origin at the same level as the bot...
			/*
			entity_origin.z = GetAbsOrigin().z;
			vecEnd = entity_origin;

			// trace a line from bot's eyes to func_ladder entity...
			UTIL_TraceLine( vecStart, vecEnd, dont_ignore_monsters, ENT(pev), &tr);

			// check if traced all the way up to the entity (didn't hit wall)
			if (tr.flFraction >= 1.0)
			{
			   // find distance to item for later use...
			   float distance = (vecEnd - vecStart).Length( );

			   // use the ladder about 100% of the time, if haven't
			   // used a ladder in at least 5 seconds...
			   if ((RANDOM_LONG(1, 100) <= 100) &&
				   ((f_use_ladder_time + 20) < gpGlobals->time))
			   {
				  // if close to ladder...
				  if (distance < 100)
				  {
					 // don't avoid walls for a while
					 f_dont_avoid_wall_time = gpGlobals->time + 5.0;
				  }

				  can_pickup = TRUE;
			   }
			}*/
		 }
		 else
		 {
			// trace a line from bot's eyes to func_ entity...
			UTIL_TraceLine( vecStart, vecEnd, dont_ignore_monsters, ENT(pev), &tr);

			// check if traced all the way up to the entity (didn't hit wall)
			if (strcmp(item_name, STRING(tr.pHit->v.classname)) == 0)
			{
			   // find distance to item for later use...
			   float distance = (vecEnd - vecStart).Length( );

			   // check if entity is wall mounted health charger...
			   if (strcmp("func_healthcharger", item_name) == 0)
			   {
				  // check if the bot can use this item and
				  // check if the recharger is ready to use (has power left)...
				  if ((pev->health < 100) && (pEntity->pev->frame == 0))
				  {
					 // check if flag not set...
					 if (!b_use_health_station)
					 {
						// check if close enough and facing it directly...
						if ((distance < PLAYER_SEARCH_RADIUS) &&
							(angle_to_entity <= 10))
						{
						   b_use_health_station = TRUE;
						   f_use_health_time = gpGlobals->time;
						}

						// if close to health station...
						if (distance < 100)
						{
						   // don't avoid walls for a while
						   f_dont_avoid_wall_time = gpGlobals->time + 5.0;
						}

						can_pickup = TRUE;
					 }
				  }
				  else
				  {
					 // don't need or can't use this item...
					 b_use_health_station = FALSE;
				  }
			   }

			   // check if entity is wall mounted HEV charger...
			   else if (strcmp("func_recharge", item_name) == 0)
			   {
				  // check if the bot can use this item and
				  // check if the recharger is ready to use (has power left)...
				  if ((pev->armorvalue < MAX_NORMAL_BATTERY) &&
					  (HasWeapon(WEAPON_SUIT)) &&
					  (pEntity->pev->frame == 0))
				  {
					 // check if flag not set and facing it...
					 if (!b_use_HEV_station)
					 {
						// check if close enough and facing it directly...
						if ((distance < PLAYER_SEARCH_RADIUS) &&
							(angle_to_entity <= 10))
						{
						   b_use_HEV_station = TRUE;
						   f_use_HEV_time = gpGlobals->time;
						}

						// if close to HEV recharger...
						if (distance < 100)
						{
						   // don't avoid walls for a while
						   f_dont_avoid_wall_time = gpGlobals->time + 5.0;
						}

						can_pickup = TRUE;
					 }
				  }
				  else
				  {
					 // don't need or can't use this item...
					 b_use_HEV_station = FALSE;
				  }
			   }

			   // check if entity is a button...
			   else if (!strcmp("func_button", item_name) )
			   {
				  // use the button about 100% of the time, if haven't
				  // used a button in at least 5 seconds...
				  if ((RANDOM_LONG(1, 100) <= 100) &&
					  ((f_use_button_time + 5) < gpGlobals->time))
				  {
					 // check if flag not set and facing it...
					 if (!b_use_button)
					 {
						// check if close enough and facing it directly...
						if ((distance < PLAYER_SEARCH_RADIUS) &&
							(angle_to_entity <= 10))
						{
						   b_use_button = TRUE;
						   b_lift_moving = FALSE;
						   f_use_button_time = gpGlobals->time;
						}

						// if close to button...
						if (distance < 100)
						{
						   // don't avoid walls for a while
						   f_dont_avoid_wall_time = gpGlobals->time + 5.0;
						}

						can_pickup = TRUE;
					 }
				  }
				  else
				  {
					 // don't need or can't use this item...
					 b_use_button = FALSE;
				  }
			   }
			}
		 }
	  }

	  // check if entity is an armed tripmine beam
	  else if (strcmp("beam", item_name) == 0)
	  {
		 CBeam *pBeam = (CBeam *)pEntity;

//         Vector v_beam_start = pBeam->GetStartPos();
//         Vector v_beam_end = pBeam->GetEndPos();
//
//         if (FInViewCone( &v_beam_start ) && FVisible( v_beam_start ))
//         {
//            BotDebug("I see a beam start!\n");
//         }
//
//         if (FInViewCone( &v_beam_end ) && FVisible( v_beam_end ))
//         {
//            BotDebug("I see a beam end!\n");
//         }
	  }

	  else  // everything else...
	  {
		 entity_origin = pEntity->GetAbsOrigin();

		 vecStart = GetAbsOrigin() + pev->view_ofs;
		 vecEnd = entity_origin;

		 // find angles from bot origin to entity...
		 angle_to_entity = BotInFieldOfView( vecEnd - vecStart );

		 // check if entity is outside field of view (+/- 45 degrees)
		 if (angle_to_entity > 45)
			continue;  // skip this item if bot can't "see" it

		 if( strcmp( "ammocrate", item_name ) == 0 && !pEntity->HasFlag( F_ENTITY_BUSY ) && !pEntity->HasFlag( F_ENTITY_UNUSEABLE ) )
		 {
			 // diffusion - the crate can be inside invisible brush collision
			 TraceResult trCrate;
			 UTIL_TraceLine( GetAbsOrigin() + pev->view_ofs, vecEnd, ignore_monsters, ENT( pev ), &tr );
			 if( tr.flFraction != 1.0f )
			 {
				if( tr.pHit != NULL )
				{
					if( tr.pHit->v.rendermode == kRenderTransTexture && tr.pHit->v.renderamt == 0 )
						can_pickup = TRUE;
				}
			 }
		 }

		 // check if line of sight to object is not blocked (i.e. visible)
		 if (BotEntityIsVisible( vecEnd ))
		 {
			if (strncmp("weapon_", item_name, 7) == 0)
			{
			   CBasePlayerWeapon *pWeapon = (CBasePlayerWeapon *)pEntity;

			   if ((pWeapon->m_pPlayer) || (pWeapon->pev->effects & EF_NODRAW))
			   {
				  // someone owns this weapon or it hasn't respawned yet
				  continue;
			   }

			   if (g_pGameRules->CanHavePlayerItem( this, pWeapon ))
			   {
				  can_pickup = TRUE;
			   }
			}
			// check if entity is ammo...
			else if (strncmp("ammo_", item_name, 5) == 0)
			{
			   CBasePlayerAmmo *pAmmo = (CBasePlayerAmmo *)pEntity;
			   BOOL ammo_found = FALSE;
			   int i;

			   // check if the item is not visible (i.e. has not respawned)
			   if (pAmmo->pev->effects & EF_NODRAW)
				  continue;

			   i = 0;
			   while (ammo_check[i].ammo_name[0])
			   {
				  if (strcmp(ammo_check[i].ammo_name, item_name) == 0)
				  {
					 ammo_found = TRUE;

					 // see if the bot can pick up this item...
					 if (g_pGameRules->CanHaveAmmo( this,
						 ammo_check[i].weapon_name, ammo_check[i].max_carry))
					 {
						can_pickup = TRUE;
						break;
					 }
				  }

				  i++;
			   }
			   if (ammo_found == FALSE)
			   {
				  sprintf(message, "unknown ammo: %s\n", item_name);
				  BotDebug(message);
			   }
			}

			// check if entity is a battery...
			else if (strcmp("item_battery", item_name) == 0)
			{
			   CItem *pBattery = (CItem *)pEntity;

			   // check if the item is not visible (i.e. has not respawned)
			   if (pBattery->pev->effects & EF_NODRAW)
				  continue;

			   // check if the bot can use this item...
			   if ((pev->armorvalue < MAX_NORMAL_BATTERY) &&
				   (HasWeapon(WEAPON_SUIT)))
			   {
				  can_pickup = TRUE;
			   }
			}

			// check if entity is a healthkit...
			else if (strcmp("item_healthkit", item_name) == 0)
			{
			   CItem *pHealthKit = (CItem *)pEntity;

			   // check if the item is not visible (i.e. has not respawned)
			   if (pHealthKit->pev->effects & EF_NODRAW)
				  continue;

			   // check if the bot can use this item...
			   if (pev->health < 100)
			   {
				  can_pickup = TRUE;
			   }
			}

			// check if entity is a packed up weapons box...
			else if (strcmp("weaponbox", item_name) == 0)
			{
			   can_pickup = TRUE;
			}

			else if( strcmp( "env_bonus", item_name ) == 0 )
			{
				can_pickup = TRUE;
			}

			// check if entity is the spot from RPG laser
			else if (strcmp("laser_spot", item_name) == 0)
			{
			}
			// check if entity is a weapon...
			// check if entity is an armed tripmine
			else if (strcmp("monster_tripmine", item_name) == 0)
			{
			   float distance = (pEntity->GetAbsOrigin() - GetAbsOrigin()).Length( );

			   if (b_see_tripmine)
			   {
				  // see if this tripmine is closer to bot...
				  if (distance < (v_tripmine_origin - GetAbsOrigin()).Length())
				  {
					 v_tripmine_origin = pEntity->GetAbsOrigin();
					 b_shoot_tripmine = FALSE;

					 // see if bot is far enough to shoot the tripmine...
					 if (distance >= 375)
					 {
						b_shoot_tripmine = TRUE;
					 }
				  }
			   }
			   else
			   {
				  b_see_tripmine = TRUE;
				  v_tripmine_origin = pEntity->GetAbsOrigin();
				  b_shoot_tripmine = FALSE;

				  // see if bot is far enough to shoot the tripmine...
				  if (distance >= 375)  // 375 is damage radius
				  {
					 b_shoot_tripmine = TRUE;
				  }
			   }
			}

			// check if entity is an armed satchel charge
			else if (strcmp("monster_satchel", item_name) == 0)
			{
			}

			// check if entity is a snark (squeak grenade)
			else if (strcmp("monster_snark", item_name) == 0)
			{
			}

		 }  // end if object is visible
	  }  // end else not "func_" entity

	  if (can_pickup) // if the bot found something it can pickup...
	  {
		 float distance = (entity_origin - GetAbsOrigin()).Length( );

		 // see if it's the closest item so far...
		 if (distance < min_distance)
		 {
			min_distance = distance;        // update the minimum distance
			pPickupEntity = pEntity;        // remember this entity
			pickup_origin = entity_origin;  // remember location of entity
		 }
	  }
   }  // end while loop

   if (pPickupEntity != NULL)
   {
	  // let's head off toward that item...
	  Vector v_item = pickup_origin - GetAbsOrigin();

	  Vector bot_angles = UTIL_VecToAngles( v_item );
	  if( GetAbsVelocity().Length2D() > 20 )
	  {
		  pev->ideal_yaw = bot_angles.y;

		  // check for wrap around of angle...
		  BotFixIdealYaw();

		  pBotPickupItem = pPickupEntity;  // save the item bot is trying to get
	  }
   }
}


void CBot::BotUseLift( void )
{
   // just need to press the button once, when the flag gets set...
   if (f_use_button_time == gpGlobals->time)
   {
	  pev->button = IN_USE;

	  f_pause_time = gpGlobals->time + 1;

	  // face opposite from the button
	  pev->ideal_yaw = -pev->ideal_yaw;  // rotate 180 degrees

	  // check for wrap around of angle...
	  BotFixIdealYaw();
   }

   // check if the bot has waited too long for the lift to move...
   if (((f_use_button_time + 2.0) < gpGlobals->time) &&
	   (!b_lift_moving))
   {
	  // clear use button flag
	  b_use_button = FALSE;

	  // bot doesn't have to set f_find_item since the bot
	  // should already be facing away from the button

	  f_move_speed = f_max_speed;
   }

   // check if lift has started moving...
   if ((moved_distance > 1) && (!b_lift_moving))
   {
	  b_lift_moving = TRUE;
   }

   // check if lift has stopped moving...
   if ((moved_distance <= 0.25) && (b_lift_moving))
   {
	  TraceResult tr1, tr2;
	  Vector v_src, v_forward, v_right, v_left;
	  Vector v_down, v_forward_down, v_right_down, v_left_down;

	  b_use_button = FALSE;

	  // TraceLines in 4 directions to find which way to go...

	  UTIL_MakeVectors( pev->v_angle );

	  v_src = GetAbsOrigin() + pev->view_ofs;
	  v_forward = v_src + gpGlobals->v_forward * 90;
	  v_right = v_src + gpGlobals->v_right * 90;
	  v_left = v_src + gpGlobals->v_right * -90;

	  v_down = pev->v_angle;
	  v_down.x = v_down.x + 45;  // look down at 45 degree angle

	  UTIL_MakeVectors( v_down );

	  v_forward_down = v_src + gpGlobals->v_forward * 100;
	  v_right_down = v_src + gpGlobals->v_right * 100;
	  v_left_down = v_src + gpGlobals->v_right * -100;

	  // try tracing forward first...
	  UTIL_TraceLine( v_src, v_forward, dont_ignore_monsters, ENT(pev), &tr1);
	  UTIL_TraceLine( v_src, v_forward_down, dont_ignore_monsters, ENT(pev), &tr2);

	  // check if we hit a wall or didn't find a floor...
	  if ((tr1.flFraction < 1.0) || (tr2.flFraction >= 1.0))
	  {
		 // try tracing to the RIGHT side next...
		 UTIL_TraceLine( v_src, v_right, dont_ignore_monsters, ENT(pev), &tr1);
		 UTIL_TraceLine( v_src, v_right_down, dont_ignore_monsters, ENT(pev), &tr2);

		 // check if we hit a wall or didn't find a floor...
		 if ((tr1.flFraction < 1.0) || (tr2.flFraction >= 1.0))
		 {
			// try tracing to the LEFT side next...
			UTIL_TraceLine( v_src, v_left, dont_ignore_monsters, ENT(pev), &tr1);
			UTIL_TraceLine( v_src, v_left_down, dont_ignore_monsters, ENT(pev), &tr2);

			// check if we hit a wall or didn't find a floor...
			if ((tr1.flFraction < 1.0) || (tr2.flFraction >= 1.0))
			{
			   // only thing to do is turn around...
			   pev->ideal_yaw += 180;  // turn all the way around
			}
			else
			{
			   pev->ideal_yaw += 90;  // turn to the LEFT
			}
		 }
		 else
		 {
			pev->ideal_yaw -= 90;  // turn to the RIGHT
		 }

		 // check for wrap around of angle...
		 BotFixIdealYaw();
	  }

	  BotChangeYaw( pev->yaw_speed );

	  f_move_speed = f_max_speed;
   }
}

void CBot::BotTurnAtWall( TraceResult *tr )
{
	Vector Normal;
	float Y, Y1, Y2, D1, D2, Z;

	// Find the normal vector from the trace result.  The normal vector will
	// be a vector that is perpendicular to the surface from the TraceResult.

	Normal = UTIL_VecToAngles( tr->vecPlaneNormal );

	// Since the bot keeps it's view angle in -180 < x < 180 degrees format,
	// and since TraceResults are 0 < x < 360, we convert the bot's view
	// angle (yaw) to the same format at TraceResult.

	Y = pev->v_angle.y;
	Y = Y + 180;
	if( Y > 359 ) Y -= 360;

	if( Normal.y < 0 )
		Normal.y += 360;

	// Here we compare the bots view angle (Y) to the Normal - 90 degrees (Y1)
	// and the Normal + 90 degrees (Y2).  These two angles (Y1 & Y2) represent
	// angles that are parallel to the wall surface, but heading in opposite
	// directions.  We want the bot to choose the one that will require the
	// least amount of turning (saves time) and have the bot head off in that
	// direction.

	Y1 = Normal.y - 90;
	if( RANDOM_LONG( 1, 100 ) <= 50 )
	{
		Y1 = Y1 - RANDOM_FLOAT( 5.0, 20.0 );
	}
	if( Y1 < 0 ) Y1 += 360;

	Y2 = Normal.y + 90;
	if( RANDOM_LONG( 1, 100 ) <= 50 )
	{
		Y2 = Y2 + RANDOM_FLOAT( 5.0, 20.0 );
	}
	if( Y2 > 359 ) Y2 -= 360;

	// D1 and D2 are the difference (in degrees) between the bot's current
	// angle and Y1 or Y2 (respectively).

	D1 = fabs( Y - Y1 );
	if( D1 > 179 ) D1 = fabs( D1 - 360 );
	D2 = fabs( Y - Y2 );
	if( D2 > 179 ) D2 = fabs( D2 - 360 );

	// If difference 1 (D1) is more than difference 2 (D2) then the bot will
	// have to turn LESS if it heads in direction Y1 otherwise, head in
	// direction Y2.  I know this seems backwards, but try some sample angles
	// out on some graph paper and go through these equations using a
	// calculator, you'll see what I mean.

	if( D1 > D2 )
		Z = Y1;
	else
		Z = Y2;

	// convert from TraceResult 0 to 360 degree format back to bot's
	// -180 to 180 degree format.

	if( Z > 180 )
		Z -= 360;

	// set the direction to head off into...
	pev->ideal_yaw = Z;

	BotFixIdealYaw( );
}

BOOL CBot::BotCantMoveForward( TraceResult *tr )
{
   // use some TraceLines to determine if anything is blocking the current
   // path of the bot.

   Vector v_src, v_forward;

   UTIL_MakeVectors( pev->v_angle );

   // first do a trace from the bot's eyes forward...

   v_src = GetAbsOrigin() + pev->view_ofs;  // EyePosition()
   v_forward = v_src + gpGlobals->v_forward * 40;

   // trace from the bot's eyes straight forward...
   UTIL_TraceLine( v_src, v_forward, dont_ignore_monsters, ENT(pev), tr);

   // check if the trace hit something...
   if (tr->flFraction < 1.0)
   {
	  return TRUE;  // bot's head will hit something
   }

   // bot's head is clear, check at waist level...

   v_src = GetAbsOrigin();
   v_forward = v_src + gpGlobals->v_forward * 40;

   // trace from the bot's waist straight forward...
   UTIL_TraceLine( v_src, v_forward, dont_ignore_monsters, ENT(pev), tr);

   // check if the trace hit something...
   if (tr->flFraction < 1.0)
   {
	  return TRUE;  // bot's body will hit something
   }

   return FALSE;  // bot can move forward, return false
}


BOOL CBot::BotCanJumpUp( void )
{
   // What I do here is trace 3 lines straight out, one unit higher than
   // the highest normal jumping distance.  I trace once at the center of
   // the body, once at the right side, and once at the left side.  If all
   // three of these TraceLines don't hit an obstruction then I know the
   // area to jump to is clear.  I then need to trace from head level,
   // above where the bot will jump to, downward to see if there is anything
   // blocking the jump.  There could be a narrow opening that the body
   // will not fit into.  These horizontal and vertical TraceLines seem
   // to catch most of the problems with falsely trying to jump on something
   // that the bot can not get onto.

   TraceResult tr;
   Vector v_jump, v_source, v_dest;

   // convert current view angle to vectors for TraceLine math...

   v_jump = pev->v_angle;
   v_jump.x = 0;  // reset pitch to 0 (level horizontally)
   v_jump.z = 0;  // reset roll to 0 (straight up and down)

   UTIL_MakeVectors( v_jump );

   // use center of the body first...

   // maximum jump height is 45, so check one unit above that (46)
   v_source = GetAbsOrigin() + Vector(0, 0, -36 + 46);
   v_dest = v_source + gpGlobals->v_forward * 24;

   // trace a line forward at maximum jump height...
   UTIL_TraceLine( v_source, v_dest, dont_ignore_monsters, ENT(pev), &tr);

   // if trace hit something, return FALSE
   if (tr.flFraction < 1.0)
	  return FALSE;

   // now check same height to one side of the bot...
   v_source = GetAbsOrigin() + gpGlobals->v_right * 16 + Vector(0, 0, -36 + 46);
   v_dest = v_source + gpGlobals->v_forward * 24;

   // trace a line forward at maximum jump height...
   UTIL_TraceLine( v_source, v_dest, dont_ignore_monsters, ENT(pev), &tr);

   // if trace hit something, return FALSE
   if (tr.flFraction < 1.0)
	  return FALSE;

   // now check same height on the other side of the bot...
   v_source = GetAbsOrigin() + gpGlobals->v_right * -16 + Vector(0, 0, -36 + 46);
   v_dest = v_source + gpGlobals->v_forward * 24;

   // trace a line forward at maximum jump height...
   UTIL_TraceLine( v_source, v_dest, dont_ignore_monsters, ENT(pev), &tr);

   // if trace hit something, return FALSE
   if (tr.flFraction < 1.0)
	  return FALSE;

   // now trace from head level downward to check for obstructions...

   // start of trace is 24 units in front of bot, 72 units above head...
   v_source = GetAbsOrigin() + gpGlobals->v_forward * 24;

   // offset 72 units from top of head (72 + 36)
   v_source.z = v_source.z + 108;

   // end point of trace is 99 units straight down from start...
   // (99 is 108 minus the jump limit height which is 45 - 36 = 9)
   v_dest = v_source + Vector(0, 0, -99);

   // trace a line straight down toward the ground...
   UTIL_TraceLine( v_source, v_dest, dont_ignore_monsters, ENT(pev), &tr);

   // if trace hit something, return FALSE
   if (tr.flFraction < 1.0)
	  return FALSE;

   // now check same height to one side of the bot...
   v_source = GetAbsOrigin() + gpGlobals->v_right * 16 + gpGlobals->v_forward * 24;
   v_source.z = v_source.z + 108;
   v_dest = v_source + Vector(0, 0, -99);

   // trace a line straight down toward the ground...
   UTIL_TraceLine( v_source, v_dest, dont_ignore_monsters, ENT(pev), &tr);

   // if trace hit something, return FALSE
   if (tr.flFraction < 1.0)
	  return FALSE;

   // now check same height on the other side of the bot...
   v_source = GetAbsOrigin() + gpGlobals->v_right * -16 + gpGlobals->v_forward * 24;
   v_source.z = v_source.z + 108;
   v_dest = v_source + Vector(0, 0, -99);

   // trace a line straight down toward the ground...
   UTIL_TraceLine( v_source, v_dest, dont_ignore_monsters, ENT(pev), &tr);

   // if trace hit something, return FALSE
   if (tr.flFraction < 1.0)
	  return FALSE;

   return TRUE;
}


BOOL CBot::BotCanDuckUnder( void )
{
   // What I do here is trace 3 lines straight out, one unit higher than
   // the ducking height.  I trace once at the center of the body, once
   // at the right side, and once at the left side.  If all three of these
   // TraceLines don't hit an obstruction then I know the area to duck to
   // is clear.  I then need to trace from the ground up, 72 units, to make
   // sure that there is something blocking the TraceLine.  Then we know
   // we can duck under it.

   TraceResult tr;
   Vector v_duck, v_source, v_dest;

   // convert current view angle to vectors for TraceLine math...

   v_duck = pev->v_angle;
   v_duck.x = 0;  // reset pitch to 0 (level horizontally)
   v_duck.z = 0;  // reset roll to 0 (straight up and down)

   UTIL_MakeVectors( v_duck );

   // use center of the body first...

   // duck height is 36, so check one unit above that (37)
   v_source = GetAbsOrigin() + Vector(0, 0, -36 + 37);
   v_dest = v_source + gpGlobals->v_forward * 24;

   // trace a line forward at duck height...
   UTIL_TraceLine( v_source, v_dest, dont_ignore_monsters, ENT(pev), &tr);

   // if trace hit something, return FALSE
   if (tr.flFraction < 1.0)
	  return FALSE;

   // now check same height to one side of the bot...
   v_source = GetAbsOrigin() + gpGlobals->v_right * 16 + Vector(0, 0, -36 + 37);
   v_dest = v_source + gpGlobals->v_forward * 24;

   // trace a line forward at duck height...
   UTIL_TraceLine( v_source, v_dest, dont_ignore_monsters, ENT(pev), &tr);

   // if trace hit something, return FALSE
   if (tr.flFraction < 1.0)
	  return FALSE;

   // now check same height on the other side of the bot...
   v_source = GetAbsOrigin() + gpGlobals->v_right * -16 + Vector(0, 0, -36 + 37);
   v_dest = v_source + gpGlobals->v_forward * 24;

   // trace a line forward at duck height...
   UTIL_TraceLine( v_source, v_dest, dont_ignore_monsters, ENT(pev), &tr);

   // if trace hit something, return FALSE
   if (tr.flFraction < 1.0)
	  return FALSE;

   // now trace from the ground up to check for object to duck under...

   // start of trace is 24 units in front of bot near ground...
   v_source = GetAbsOrigin() + gpGlobals->v_forward * 24;
   v_source.z = v_source.z - 35;  // offset to feet + 1 unit up

   // end point of trace is 72 units straight up from start...
   v_dest = v_source + Vector(0, 0, 72);

   // trace a line straight up in the air...
   UTIL_TraceLine( v_source, v_dest, dont_ignore_monsters, ENT(pev), &tr);

   // if trace didn't hit something, return FALSE
   if (tr.flFraction >= 1.0)
	  return FALSE;

   // now check same height to one side of the bot...
   v_source = GetAbsOrigin() + gpGlobals->v_right * 16 + gpGlobals->v_forward * 24;
   v_source.z = v_source.z - 35;  // offset to feet + 1 unit up
   v_dest = v_source + Vector(0, 0, 72);

   // trace a line straight up in the air...
   UTIL_TraceLine( v_source, v_dest, dont_ignore_monsters, ENT(pev), &tr);

   // if trace didn't hit something, return FALSE
   if (tr.flFraction >= 1.0)
	  return FALSE;

   // now check same height on the other side of the bot...
   v_source = GetAbsOrigin() + gpGlobals->v_right * -16 + gpGlobals->v_forward * 24;
   v_source.z = v_source.z - 35;  // offset to feet + 1 unit up
   v_dest = v_source + Vector(0, 0, 72);

   // trace a line straight up in the air...
   UTIL_TraceLine( v_source, v_dest, dont_ignore_monsters, ENT(pev), &tr);

   // if trace didn't hit something, return FALSE
   if (tr.flFraction >= 1.0)
	  return FALSE;

   return TRUE;
}


BOOL CBot::BotShootTripmine( void )
{
   if (b_shoot_tripmine != TRUE)
	  return FALSE;

   // aim at the tripmine and fire the glock...

   Vector v_enemy = v_tripmine_origin - GetGunPosition( );

   pev->v_angle = UTIL_VecToAngles( v_enemy );

   pev->angles.x = 0;
   pev->angles.y = pev->v_angle.y;
   pev->angles.z = 0;

   pev->ideal_yaw = pev->v_angle.y;

   // check for wrap around of angle...
   BotFixIdealYaw();

   pev->v_angle.x = -pev->v_angle.x;  //adjust pitch to point gun

   return (BotFireWeapon( v_tripmine_origin, WEAPON_BERETTA, TRUE ));
}


BOOL CBot::BotFollowUser( void )
{
   BOOL user_visible;
   float f_distance;

   Vector vecEnd = pBotUser->GetAbsOrigin() + pBotUser->pev->view_ofs;

   pev->v_angle.x = 0;  // reset pitch to 0 (level horizontally)
   pev->v_angle.z = 0;  // reset roll to 0 (straight up and down)

   pev->angles.x = 0;
   pev->angles.y = pev->v_angle.y;
   pev->angles.z = 0;

   if (!pBotUser->IsAlive( ))
   {
	  // the bot's user is dead!
	  pBotUser = NULL;
	  return FALSE;
   }

   user_visible = FInViewCone( &vecEnd ) && FVisible( vecEnd );

   // check if the "user" is still visible or if the user has been visible
   // in the last 5 seconds (or the player just starting "using" the bot)

   if (user_visible || (f_bot_use_time + 5 > gpGlobals->time))
   {
	  if (user_visible)
		 f_bot_use_time = gpGlobals->time;  // reset "last visible time"

	  // face the user
	  Vector v_user = pBotUser->GetAbsOrigin() - GetAbsOrigin();
	  Vector bot_angles = UTIL_VecToAngles( v_user );

	  pev->ideal_yaw = bot_angles.y;

	  // check for wrap around of angle...
	  BotFixIdealYaw();

	  f_distance = v_user.Length( );  // how far away is the "user"?

	  if (f_distance > 200)      // run if distance to enemy is far
		 f_move_speed = f_max_speed;
	  else if (f_distance > BOT_KNIFE_DISTANCE )  // walk if distance is closer
		 f_move_speed = f_max_speed / 2;
	  else                     // don't move if close enough
		 f_move_speed = 0.0;

	  return TRUE;
   }
   else
   {
	  // person to follow has gone out of sight...
	  pBotUser = NULL;

	  return FALSE;
   }
}


BOOL CBot::BotCheckWallOnLeft( void )
{
   Vector v_src, v_left;
   TraceResult tr;

   UTIL_MakeVectors( pev->v_angle );

   // do a trace to the left...

   v_src = GetAbsOrigin();
   v_left = v_src + gpGlobals->v_right * -40;  // 40 units to the left

   UTIL_TraceLine( v_src, v_left, dont_ignore_monsters, ENT(pev), &tr);

   // check if the trace hit something...
   if (tr.flFraction < 1.0)
   {
	  if (f_wall_on_left == 0)
		 f_wall_on_left = gpGlobals->time;

	  return TRUE;
   }

   return FALSE;
}


BOOL CBot::BotCheckWallOnRight( void )
{
   Vector v_src, v_right;
   TraceResult tr;

   UTIL_MakeVectors( pev->v_angle );

   // do a trace to the right...

   v_src = GetAbsOrigin();
   v_right = v_src + gpGlobals->v_right * 40;  // 40 units to the right

   UTIL_TraceLine( v_src, v_right, dont_ignore_monsters, ENT(pev), &tr);

   // check if the trace hit something...
   if (tr.flFraction < 1.0)
   {
	  if (f_wall_on_right == 0)
		 f_wall_on_right = gpGlobals->time;

	  return TRUE;
   }

   return FALSE;
}


void CBot::BotThink( void )
{
	Vector v_diff;             // vector from previous to current location
	float degrees_turned;

	// check if someone kicked the bot off of the server (DON'T RESPAWN!)...
	if ((pev->takedamage == DAMAGE_NO) && (respawn_index >= 0))
	{
		pev->health = 0;
		pev->deadflag = DEAD_DEAD;  // make the kicked bot be dead

		bot_respawn[respawn_index].is_used = FALSE;  // this slot is now free
		bot_respawn[respawn_index].state = BOT_IDLE;
		respawn_index = -1;  // indicate no slot used

		// fall through to next if statement (respawn_index will be -1)
	}

	// is the round over (time/frag limit) or has the bot been removed?
	if ((g_fGameOver) || (respawn_index == -1))
	{
		CSound *pSound;

		// keep resetting the sound entity until the bot is respawned...
		pSound = CSoundEnt::SoundPointerForIndex( CSoundEnt::ClientSoundIndex( edict() ) );
		if ( pSound )
		{
			pSound->Reset();
		}

		return;
	}

	pev->button = 0;  // make sure no buttons are pressed

	if( gpGlobals->time < f_duck_release_time )
		pev->button |= IN_DUCK;

	// if the bot is dead, randomly press fire to respawn...
	if( (pev->health < 1) || (pev->deadflag != DEAD_NO) )
	{
		if( RANDOM_LONG( 1, 100 ) > 60 )
			pev->button = IN_ATTACK;

		g_engfuncs.pfnRunPlayerMove( edict(), pev->v_angle, f_move_speed,
			0, 0, pev->button, 0,
			gpGlobals->frametime * 1000 );
		return;
	}

	// see if it's time to check for sv_maxspeed change...
	if (f_speed_check_time <= gpGlobals->time)
	{
		// store current sv_maxspeed setting for quick access
		f_max_speed = CVAR_GET_FLOAT("sv_maxspeed");

		// set next check time to 2 seconds from now
		f_speed_check_time = gpGlobals->time + RANDOM_FLOAT(2.0, 3.0);
	}

	// see how far bot has moved since the previous position...
	v_diff = v_prev_origin - GetAbsOrigin();

	moved_distance = v_diff.Length();

	v_prev_origin = GetAbsOrigin();  // save current position as previous

	f_move_speed = f_max_speed;  // set to max speed unless known otherwise

	// turn towards ideal_yaw by yaw_speed degrees
	if( pBotEnemy )
		degrees_turned = BotChangeYaw( BOT_YAW_SPEED_ENEMYTRACK );
	else
		degrees_turned = BotChangeYaw( BOT_YAW_SPEED );

 //   if (degrees_turned >= pev->yaw_speed)
	if( degrees_turned >= 45 )
	{
		// if bot is still turning, turn in place by setting speed to 0
		f_move_speed = 0;
	}
	else if( degrees_turned >= 30 )
	{
		f_move_speed *= 0.25;
	}
	else if( degrees_turned >= 20 )
	{
		f_move_speed *= 0.333;
	}

	/*else*/ if (IsOnLadder( ))  // check if the bot is on a ladder...
	{
		f_use_ladder_time = gpGlobals->time;

		BotOnLadder();  // go handle the ladder movement
	}

	else  // else handle movement related actions...
	{
		// bot is not on a ladder so clear ladder direction flag...
		ladder_dir = 0;

		if (f_botdontshoot == 0)  // is bot targeting turned on?
			pBotEnemy = BotFindEnemy( );
		else
			pBotEnemy = NULL;  // clear enemy pointer (no enemy for you!)

		if( IsCamping )
		{
			// camp time is over
			if( gpGlobals->time > camping_end_time )
			{
				IsCamping = false;
				f_duck_release_time = 0;
				next_camping_time = gpGlobals->time + RANDOM_LONG( 10, 20 );
			}
			else if( pBotEnemy != NULL && ((pBotEnemy->GetAbsOrigin() - GetAbsOrigin()).Length() < 200) )
			{
				// enemy is too close. stop camping
				IsCamping = false;
				camping_end_time = 0;
				f_duck_release_time = 0;
				// next_camping_time is not reset here, because we were interrupted
			}
			else
			{
				f_move_speed = 0; // stay still and camp like a rat you are

				// look for enemies
				if( camping_turn_time < gpGlobals->time )
				{
					Vector LookVector = pev->v_angle;
					if( RANDOM_LONG( 0, 1 ) == 1 )
						LookVector[YAW] += RANDOM_LONG( 20, 50 );
					else
						LookVector[YAW] -= RANDOM_LONG( 20, 50 );
					
					// do not look at wall
					TraceResult tr;
					int degrees = 0;
					while( degrees < 360 )
					{
						UTIL_MakeVectors( LookVector );
						const Vector start = GetAbsOrigin() + pev->view_ofs;
						const Vector end = start + gpGlobals->v_forward * 1000;
						UTIL_TraceLine( start, end, dont_ignore_monsters, ENT( pev ), &tr );
						float view_distance = (start - tr.vecEndPos).Length();
						if( view_distance > 900 )
							break;

						degrees += 15;
						LookVector[YAW] += 15;
						if( degrees >= 360 )
						{
							IsCamping = false;
							f_duck_release_time = 0;
							next_camping_time = gpGlobals->time + RANDOM_LONG( 10, 20 );
							ALERT( at_console, "Bad bot camping spot at %i %i %i, check waypoint\n", (int)GetAbsOrigin().x, (int)GetAbsOrigin().y, (int)GetAbsOrigin().z );
						}
					}
					//ALERT( at_console, "bot camping turn attempts: %i\n", degrees / 15 );

					pev->ideal_yaw = LookVector[YAW];
					BotFixIdealYaw();
					camping_turn_time = gpGlobals->time + RANDOM_FLOAT( 1.5f, 4.0f );
				}
			}
		}

		if (pBotEnemy != NULL)  // does an enemy exist?
		{
			BotShootAtEnemy( );  // shoot at the enemy
		}
		else if (f_pause_time > gpGlobals->time)  // is bot "paused"?
		{
			// you could make the bot look left then right, or look up
			// and down, to make it appear that the bot is hunting for
			// something (don't do anything right now)

			f_move_speed = 0;
		}
		// is bot being "used" and can still follow "user"?
		else if ((pBotUser != NULL) && BotFollowUser( ))
		{
			// do nothing here!
		}
		else
		{
			// no enemy, let's just wander around...
			pev->v_angle.x = 0;  // reset pitch to 0 (level horizontally)
			pev->v_angle.z = 0;  // reset roll to 0 (straight up and down)

			pev->angles.x = 0;
			pev->angles.y = pev->v_angle.y;
			pev->angles.z = 0;

			// check if bot should look for items now or not...
			if (f_find_item < gpGlobals->time)
			{
				BotFindItem( );  // see if there are any visible items
			}

			if( pBotPickupItem )
			{
				if( (pBotPickupItem->GetAbsOrigin() - GetAbsOrigin()).Length() < 100 )
				{
					if( FClassnameIs( pBotPickupItem, "ammocrate" ) && !pBotPickupItem->HasFlag( F_ENTITY_BUSY ) )
					{
						pBotPickupItem->Use( this, this, USE_TOGGLE, 0 );
					}
				}
			}

			// check if bot sees a tripmine...
			if (b_see_tripmine)
			{
				// check if bot can shoot the tripmine...
				if ((b_shoot_tripmine) && BotShootTripmine( ))
				{
					// shot at tripmine, may or may not have hit it, clear
					// flags anyway, next BotFindItem will see it again if
					// it is still there...

					b_shoot_tripmine = FALSE;
					b_see_tripmine = FALSE;

					// pause for a while to allow tripmine to explode...
					f_pause_time = gpGlobals->time + 0.5;
				}
				else  // run away!!!
				{
					Vector tripmine_angles;

					tripmine_angles = UTIL_VecToAngles( v_tripmine_origin - GetAbsOrigin() );

					// face away from the tripmine
					pev->ideal_yaw = -tripmine_angles.y;

					// check for wrap around of angle...
					BotFixIdealYaw();

					b_see_tripmine = FALSE;

					f_move_speed = 0;  // don't run while turning
				}
			}
			else if (b_use_health_station) // check if should use wall mounted health station...
			{
				if ((f_use_health_time + 10.0) > gpGlobals->time)
				{
					f_move_speed = 0;  // don't move while using health station
					pev->button = IN_USE;
				}
				else
				{
					// bot is stuck trying to "use" a health station...

					b_use_health_station = FALSE;

					// don't look for items for a while since the bot
					// could be stuck trying to get to an item
					f_find_item = gpGlobals->time + 1;
				}
			}
			else if (b_use_HEV_station)  // check if should use wall mounted HEV station...
			{
				if ((f_use_HEV_time + 10.0) > gpGlobals->time)
				{
					f_move_speed = 0;  // don't move while using HEV station
					pev->button = IN_USE;
				}
				else
				{
					// bot is stuck trying to "use" a HEV station...

					b_use_HEV_station = FALSE;

					// don't look for items for a while since the bot
					// could be stuck trying to get to an item
					f_find_item = gpGlobals->time + 1;
				}
			}
			else if (b_use_button)
			{
				f_move_speed = 0;  // don't move while using elevator
				BotUseLift();
			}
			else
			{
				TraceResult tr;

				if (pev->waterlevel == 3)  // check if the bot is underwater...
					BotUnderWater( );

				// diffusion - bot stuck...
				if( GetAbsVelocity().Length2D() < 10 )
				{
					if( !IsCamping )
					{
						pev->ideal_yaw -= 25;
						BotFixIdealYaw();
					}
					if( pBotPickupItem )
						f_find_item = gpGlobals->time + 1;
				}

				if( !BotTryWaypoint() )
				{
					// check if there is a wall on the left...
					if( !BotCheckWallOnLeft() )
					{
						// if there was a wall on the left over 1/2 a second ago then
						// 20% of the time randomly turn between 45 and 60 degrees

						if( (f_wall_on_left != 0) && (f_wall_on_left <= gpGlobals->time - 0.5) && (RANDOM_LONG( 1, 100 ) <= 20) )
						{
							pev->ideal_yaw += RANDOM_LONG( 45, 60 );

							// check for wrap around of angle...
							BotFixIdealYaw();

							f_move_speed = 0;  // don't move while turning
							f_dont_avoid_wall_time = gpGlobals->time + 1.0;
						}

						f_wall_on_left = 0;  // reset wall detect time
					}

					// check if there is a wall on the right...
					if( !BotCheckWallOnRight() )
					{
						// if there was a wall on the right over 1/2 a second ago then
						// 20% of the time randomly turn between 45 and 60 degrees

						if( (f_wall_on_right != 0) && (f_wall_on_right <= gpGlobals->time - 0.5) && (RANDOM_LONG( 1, 100 ) <= 20) )
						{
							pev->ideal_yaw -= RANDOM_LONG( 45, 60 );

							// check for wrap around of angle...
							BotFixIdealYaw();

							f_move_speed = 0;  // don't move while turning
							f_dont_avoid_wall_time = gpGlobals->time + 1.0;
						}

						f_wall_on_right = 0;  // reset wall detect time
					}

					// check if bot is about to hit a wall.  TraceResult gets returned
					if( 0 )//(f_dont_avoid_wall_time <= gpGlobals->time) && BotCantMoveForward( &tr ) )
					{
						// ADD LATER
						// need to check if bot can jump up or duck under here...
						// ADD LATER

						BotTurnAtWall( &tr );
					}
					// check if the bot hasn't moved much since the last location
					else if( (moved_distance <= 0.1) && (!bot_was_paused) )
					{
						// the bot must be stuck!

						if( BotCanJumpUp() )  // can the bot jump onto something?
						{
							pev->button |= IN_JUMP;  // jump up and move forward
							pev->button |= IN_DUCK;
							f_duck_release_time = gpGlobals->time + 1;
						}
						else if( BotCanDuckUnder() )  // can the bot duck under something?
						{
							pev->button |= IN_DUCK;  // duck down and move forward
							f_duck_release_time = gpGlobals->time + 1;
						}
						else
						{
							f_move_speed = 0;  // don't move while turning

							// turn randomly between 30 and 60 degress
							if( wander_dir == WANDER_LEFT )
								pev->ideal_yaw += RANDOM_LONG( 30, 60 );
							else
								pev->ideal_yaw -= RANDOM_LONG( 30, 60 );

							// check for wrap around of angle...
							BotFixIdealYaw();

							// is the bot trying to get to an item?...
							if( pBotPickupItem != NULL )
							{
								// don't look for items for a while since the bot
								// could be stuck trying to get to an item
								f_find_item = gpGlobals->time + 1;
							}
						}
					}

					// should the bot pause for a while here?
					// don't pause if being "used"
					if( 0 )//(RANDOM_LONG( 1, 1000 ) <= pause_frequency[bot_skill]) && (pBotUser == NULL) ) 
					{
						// set the time that the bot will stop "pausing"
						f_pause_time = gpGlobals->time +
						RANDOM_FLOAT( pause_time[bot_skill][0],
						pause_time[bot_skill][1] );
						f_move_speed = 0;  // don't move while turning
					}
				}
			}
		}
	}

	if (f_move_speed < 1)
		bot_was_paused = TRUE;
	else
		bot_was_paused = FALSE;

	g_engfuncs.pfnRunPlayerMove( edict( ), pev->v_angle, f_move_speed, 0, 0, pev->button, 0, gpGlobals->frametime * 1000 );
}

//======================================================================
// diffusion - simple waypoint system
//======================================================================

void CBot::FindNearbyWaypoints( float Distance )
{
	TraceResult tr;
	int i;
	iNearbyWaypoints = 0;
	memset( vNearbyWaypoints, 0, sizeof( vNearbyWaypoints ) );

	static bool ReverseSearch = false;

	if( ReverseSearch ) // add some "randomize" factor...
	{
		for( i = WaypointsLoaded - 1; i >= 0; i-- )
		{
			// it's close enough
			if( (GetAbsOrigin() - vWaypoint[i]).Length() <= Distance )
			{
				if( fabs( GetAbsOrigin().z - vWaypoint[i].z ) < 100 )
				{
					vNearbyWaypoints[iNearbyWaypoints] = vWaypoint[i];
					iNearbyWaypoints++;
				}
			}
		}
	}
	else
	{
		for( i = 0; i < WaypointsLoaded; i++ )
		{
			// it's close enough
			if( (GetAbsOrigin() - vWaypoint[i]).Length() <= Distance )
			{
				if( fabs( GetAbsOrigin().z - vWaypoint[i].z ) < 100 )
				{
					vNearbyWaypoints[iNearbyWaypoints] = vWaypoint[i];
					iNearbyWaypoints++;
				}
			}
		}
	}

	if( RANDOM_LONG(0,4) == 1 )
		ReverseSearch = !ReverseSearch;

//	ALERT( at_console, "Bot %i found %i nearby points\n", ENTINDEX(edict()), iNearbyWaypoints );
}

// diffusion - simple points, just find a visible one and try to go there
// just a temporary solution until real waypoints come (never)
bool CBot::BotTryWaypoint( void )
{
	if( WaypointsLoaded == 0 )
		return false;

	if( IsCamping ) // sitting at a camping point
	{
		CurrentWaypointID = -1; // forget about waypoints
		iWaypointHistory[0] = -1;
		iWaypointHistory[1] = -1;
		return true;
	}

	if( pBotEnemy != NULL )
	{
		CurrentWaypointID = -1; // forget about waypoints
		CrouchMove = false;
		iWaypointHistory[0] = -1;
		iWaypointHistory[1] = -1;
		return false; // in combat
	}

	if( pBotPickupItem != NULL )
	{
		CurrentWaypointID = -1; // forget about waypoints
		CrouchMove = false;
		iWaypointHistory[0] = -1;
		iWaypointHistory[1] = -1;
		return false; // trying to pick up an item
	}

	bool WaypointFound = false;
	int WaypointNumber = 0;
	static int points_used = 0;
	bool AtWaypoint = false;

	// we have an active waypoint, heading there
	if( CurrentWaypointID > -1 )
	{
		// it's still far enough. let's go
		if( PointFlag[CurrentWaypointID] == W_FLAG_LADDER && (GetAbsOrigin() - vWaypoint[CurrentWaypointID]).Length() > 30 )
		{
			// let's head off toward that point... (and look at it)
			Vector vPoint = vWaypoint[CurrentWaypointID] - GetAbsOrigin();
			pev->v_angle = UTIL_VecToAngles( vPoint );

			if( pev->v_angle.x > 180 )
				pev->v_angle.x -= 360;

			if( pev->v_angle.y > 180 )
				pev->v_angle.y -= 360;

			pev->angles.x = -pev->v_angle.x / 3;
			pev->angles.y = pev->v_angle.y;
			pev->angles.z = 0;

			pev->idealpitch = pev->v_angle.x;
			BotFixIdealPitch();
			pev->ideal_yaw = pev->v_angle.y;
			BotFixIdealYaw();

			if( GetAbsVelocity().Length() < 10 ) // stuck again...
			{
				CurrentWaypointID = -1;
				f_find_item = gpGlobals->time + 1;
			}
		}
		else if( (GetAbsOrigin() - vWaypoint[CurrentWaypointID]).Length2D() > 30 )
		{ 
			// let's head off toward that point...
			Vector vPoint = vWaypoint[CurrentWaypointID] - GetAbsOrigin();
			Vector bot_angles = UTIL_VecToAngles( vPoint );

			pev->ideal_yaw = bot_angles.y;
			// check for wrap around of angle...
			BotFixIdealYaw();

		//	ALERT( at_console, "Bot %i still going to last found waypoint\n", ENTINDEX( edict() ) );

		//	UTIL_Sparks( vWaypoint[CurrentWaypointID] ); // DEBUGDEBUG

			// stuck on something, try to jump
			if( GetAbsVelocity().Length2D() < 10 )
			{
				// do duckjump
				pev->button |= IN_DUCK;
				f_duck_release_time = gpGlobals->time + 1;
				pev->button |= IN_JUMP;
				if( !IsStuck )
				{
					// stuck on a way to waypoint
					f_stuck_time = gpGlobals->time;
					IsStuck = true;
				}
				else
				{
					if( gpGlobals->time > f_stuck_time + 3 )
					{
						CurrentWaypointID = -1; // forget about waypoints
						CrouchMove = false;
						f_stuck_time = 0;
						IsStuck = false;
					}
				}
			}
			else
			{
				if( !CrouchMove )
				{
					if( m_flStaminaValue > 50 && gpGlobals->time > next_dash_time ) // try dash
					{
						pev->button |= IN_FORWARD;
						DashButton = true;
						next_dash_time = gpGlobals->time + RANDOM_FLOAT( 7.0f, 15.0f );
					}
				}
			}

			if( CrouchMove )
				pev->button |= IN_DUCK;

			return true;
		}
		else // we are too close to this point (arrived!), invalidate it and choose another
		{	
			TraceResult trArrivalTest;
			UTIL_TraceLine( GetAbsOrigin() + pev->view_ofs, vWaypoint[CurrentWaypointID], ignore_monsters, ENT( pev ), &trArrivalTest );

			if( trArrivalTest.flFraction < 1.0f )
			{
				// we have arrived in 2D field, but we can't see this waypoint.
				// maybe we are on a different floor by mistake
				CurrentWaypointID = -1; // forget about waypoints
				CrouchMove = false;
				iWaypointHistory[0] = -1;
				iWaypointHistory[1] = -1;
				return false;
			}
			
			AtWaypoint = true;

			if( PointFlag[CurrentWaypointID] == W_FLAG_CROUCH )
				CrouchMove = true;
			else
				CrouchMove = false;

			if( PointFlag[CurrentWaypointID] == W_FLAG_SNIPER || PointFlag[CurrentWaypointID] == W_FLAG_SNIPERCROUCH )
			{
				if( next_camping_time < gpGlobals->time )
				{
					if( HasWeapon( WEAPON_SNIPER ) || HasWeapon( WEAPON_CROSSBOW ) || HasWeapon( WEAPON_GAUSS ) )
					{
						//	ALERT( at_console, "FLAG %i\n", PointFlag[CurrentWaypointID] );
						camping_end_time = gpGlobals->time + RANDOM_LONG( 20, 40 );
						IsCamping = true;

						// select the weapon and start camping
						if( HasWeapon( WEAPON_SNIPER ) )
							SelectItem( "weapon_sniper" );
						else if( HasWeapon( WEAPON_GAUSS ) )
							SelectItem( "weapon_gausniper" );
						else if( HasWeapon( WEAPON_CROSSBOW ) )
							SelectItem( "weapon_crossbow" );

						if( PointFlag[CurrentWaypointID] == W_FLAG_SNIPERCROUCH )
							f_duck_release_time = camping_end_time;

						CurrentWaypointID = -1;

						return true;
					}
				}
			}

			if( PointFlag[CurrentWaypointID] == W_FLAG_FLASHLIGHT )
			{
				if( !FlashlightIsOn() )
					FlashlightTurnOn();
			}
			else
			{
				if( FlashlightIsOn() )
					FlashlightTurnOff();
			}
		}
	}

	if( AtWaypoint || (CurrentWaypointID == -1 && gpGlobals->time > f_trynextwaypoint_time) ) // search for a new point every now and then
	{
		// we are at a point. find the connections to this point and choose it
		if( AtWaypoint )
		{
			iWaypointHistory[1] = iWaypointHistory[0];
			if( iWaypointHistory[1] == -1 )
				iWaypointHistory[1] = CurrentWaypointID;
			iWaypointHistory[0] = CurrentWaypointID;

			int NewWaypointID = -1;

			if( ConnectionsForPoint[CurrentWaypointID] > 1 ) // multiple points, choose one
			{
				int tries = 20; // to avoid infinite loop
				while( tries > 0 )
				{
					NewWaypointID = ConnectedPoints[CurrentWaypointID][RANDOM_LONG( 0, ConnectionsForPoint[CurrentWaypointID] - 1 )];
					if( NewWaypointID != iWaypointHistory[1] ) // don't choose the one where we just came from
						break;

					tries--;
					if( tries == 0 )
					{
						// random function didn't fire or something went wrong...
						CurrentWaypointID = -1; // forget about waypoints
						CrouchMove = false;
						break;
					}
				}
			}
			else
				NewWaypointID = ConnectedPoints[CurrentWaypointID][0]; // only one point, the only choice

			if( NewWaypointID > -1 )
			{
				if( PointFlag[CurrentWaypointID] == W_FLAG_CROUCH && PointFlag[NewWaypointID] == W_FLAG_CROUCH )
				{
					// moving to the next point crouching
					CrouchMove = true;
				}
				else if( PointFlag[NewWaypointID] != W_FLAG_CROUCH )
				{
					CrouchMove = false;

					if( PointFlag[CurrentWaypointID] == W_FLAG_JUMP && PointFlag[NewWaypointID] == W_FLAG_JUMP )
					{
						pev->button |= IN_JUMP;
						pev->button |= IN_DUCK;
						f_duck_release_time = gpGlobals->time + 1;
					}
				}

				if( PointFlag[CurrentWaypointID] == W_FLAG_LADDER && PointFlag[NewWaypointID] != W_FLAG_LADDER )
					pev->button |= IN_JUMP;
				
				iNearbyWaypointNum = NewWaypointID;
				WaypointFound = true;
				CurrentWaypointID = NewWaypointID;
			}
		}
		else
		{
			FindNearbyWaypoints( 500 );

			int RememberNum[3]; // remember 3 valid points
			memset( RememberNum, -1, sizeof( RememberNum ) );

			int ValidPoint = 0;

			for( int i = 0; i < iNearbyWaypoints; i++ )
			{
				if( ValidPoint == 3 )
					break; // found enough points
				
				TraceResult tr;
				UTIL_TraceLine( GetAbsOrigin() + pev->view_ofs, vNearbyWaypoints[i], ignore_monsters, ENT( pev ), &tr );
				// visible!
				if( tr.flFraction == 1.0 )
				{
					 // it's a valid point. Remember it
					 RememberNum[ValidPoint] = i;
					 ValidPoint++;
				}

			}

			// we could have found a few valid points. choose one of them
			if( RememberNum[0] != -1 ) // at least one point is valid
			{
			//	iNearbyWaypointNum = RememberNum[RANDOM_LONG( 0, ValidPoint - 1 )];
				// but which one is the closest?
				iNearbyWaypointNum = RememberNum[0];
				if( ValidPoint > 1 )
				{
					float NearbyDistance = (GetAbsOrigin() - vNearbyWaypoints[iNearbyWaypointNum]).Length();
					for( int c = 0; c < ValidPoint; c++ )
					{
						float NewNearbyDistance = (GetAbsOrigin() - vNearbyWaypoints[RememberNum[c]]).Length();
						if( NewNearbyDistance < NearbyDistance )
						{
							iNearbyWaypointNum = RememberNum[c];
							NearbyDistance = NewNearbyDistance;
						}
					}
				}

				WaypointFound = true;
			}

			f_trynextwaypoint_time = gpGlobals->time + 0.5;
		}
	}

	if( WaypointFound )
	{
		// let's head off toward that point...
		Vector NewPoint;

		if( CurrentWaypointID < 0 )
			NewPoint = vNearbyWaypoints[iNearbyWaypointNum];
		else
			NewPoint = vWaypoint[CurrentWaypointID];

		Vector vPoint = NewPoint - GetAbsOrigin();

		Vector bot_angles = UTIL_VecToAngles( vPoint );

		pev->ideal_yaw = bot_angles.y;

		// check for wrap around of angle...
		BotFixIdealYaw();

	//	ALERT( at_aiconsole, "Bot %i has chosen a point %.f %.f %.f\n", ENTINDEX( edict() ), NewPoint.x, NewPoint.y, NewPoint.z );

		AtWaypoint = false;

		// find out the id of this waypoint
		for( int f = 0; f < WaypointsLoaded; f++ )
		{
			if( vWaypoint[f] == NewPoint )
			{
				CurrentWaypointID = f;
				break;
			}
		}

		return true;
	}

	return false;
}
