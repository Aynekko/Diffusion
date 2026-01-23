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
//  cdll_int.c
//
// this implementation handles the linking of the engine to the DLL
//

#include <windows.h>
#include <shlwapi.h>     // for PathRemoveFileSpecA
#include <vector>
#include <sstream>

#include "hud.h"
#include "utils.h"
#include "r_local.h"
#include <mathlib.h>
#include "usercmd.h"
#include "entity_state.h"
#include "r_particle.h"
#include "cdll_exp.h"
#include "discord.h"
#include "build.h"
#include "keydefs.h"
#include "buildtime.h"

#define USE_DIFFUSION // use diffusion.exe
//#define USE_HALFLIFE // use hl.exe
// otherwise xash3d.exe will be used

int developer_level;
int g_iXashEngineBuildNumber;
cl_enginefunc_t gEngfuncs;
render_api_t gRenderfuncs;
CHud gHUD;

//==========================================================
// Initialize: called when the DLL is first loaded.
//==========================================================
extern "C" __declspec(dllexport) int Initialize( cl_enginefunc_t *pEnginefuncs, int iVersion )
{
	gEngfuncs = *pEnginefuncs;

	if( iVersion != CLDLL_INTERFACE_VERSION )
		return 0;

	memcpy( &gEngfuncs, pEnginefuncs, sizeof( cl_enginefunc_t ) );

	// get developer level
	developer_level = (int)CVAR_GET_FLOAT( "developer" );

	if( !CVAR_GET_POINTER( "host_clientloaded" ) )
		return 0;	// Not a Xash3D engine

	g_iXashEngineBuildNumber = (int)CVAR_GET_FLOAT( "build" ); // 0 for old builds or GoldSrc
	if( g_iXashEngineBuildNumber <= 0 )
		g_iXashEngineBuildNumber = (int)CVAR_GET_FLOAT( "buildnum" );

	// diffusion - execute mod-specific stuff
	ClientCmd( "exec diffusion.cfg\n" );

	Msg( "^2Client DLL build date^7: ^5%s %s^7\n", BUILD_DATE, BUILD_TIME );

	return 1;
}

/*
================================
HUD_GetHullBounds

Engine calls this to enumerate player collision hulls, for prediction.  Return 0 if the hullnumber doesn't exist.
================================
*/
extern "C" __declspec(dllexport)  int HUD_GetHullBounds( int hullnumber, float *mins, float *maxs )
{
	int iret = 0;

	switch( hullnumber )
	{
	case 0:	// Normal player
		Vector( -16, -16, -36 ).CopyToArray( mins );
		Vector(  16,  16,  36 ).CopyToArray( maxs );
		iret = 1;
		break;
	case 1:	// Crouched player
		Vector( -16, -16, -18 ).CopyToArray( mins );
		Vector(  16,  16,  18 ).CopyToArray( maxs );
		iret = 1;
		break;
	case 2:	// Point based hull
		g_vecZero.CopyToArray( mins );
		g_vecZero.CopyToArray( maxs );
		iret = 1;
		break;
	}
	return iret;
}

/*
================================
HUD_ConnectionlessPacket

Return 1 if the packet is valid.  Set response_buffer_size if you want to send a response packet.
Incoming, it holds the max size of the response_buffer, so you must zero it out if you choose not to respond.
================================
*/
extern "C" __declspec(dllexport)  int HUD_ConnectionlessPacket( const struct netadr_s *, const char *, char *, int *response_buffer_size )
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

extern "C" __declspec(dllexport)  void HUD_PlayerMoveInit( struct playermove_s *ppmove )
{
	PM_Init( ppmove );
}

extern "C" __declspec(dllexport)  char HUD_PlayerMoveTexture( char *name )
{
	return PM_FindTextureType( name );
}

extern "C" __declspec(dllexport)  void HUD_PlayerMove( struct playermove_s *ppmove, int server )
{
	PM_Move( ppmove, server );
}

/*
==========================
	HUD_VidInit

Called when the game initializes
and whenever the vid_mode is changed
so the HUD can reinitialize itself.
==========================
*/
extern "C" __declspec(dllexport) int HUD_VidInit(void)
{	
	gHUD.VidInit();

	R_VidInit();

	V_VidInit();

	return 1;
}

/*
==========================
	HUD_Init

Called whenever the client connects
to a server.  Reinitializes all
the hud variables.
==========================
*/

std::string GetArgsAfterGame()
{
	const char *fullCmd = GetCommandLineA();
	if( !fullCmd || !*fullCmd )
		return "";

	std::string cmd( fullCmd );

	// Step 1: Skip the executable path (quoted or not)
	size_t pos = 0;
	if( cmd[pos] == '"' )
	{
		// Quoted exe path
		size_t endQuote = cmd.find( '"', 1 );
		if( endQuote != std::string::npos )
			pos = endQuote + 1;
		else
			pos = cmd.size(); // malformed
	}
	else
	{
		// Unquoted exe path - skip until first space
		pos = cmd.find( ' ' );
		if( pos == std::string::npos )
			return ""; // no args at all
	}

	// Now pos is after exe name - skip leading whitespace
	while( pos < cmd.size() && std::isspace( static_cast<unsigned char>( cmd[pos] ) ) )
		++pos;

	// Step 2: Look for -game and skip it + its value
	while( pos < cmd.size() )
	{
		// Skip whitespace between tokens
		while( pos < cmd.size() && std::isspace( static_cast<unsigned char>( cmd[pos] ) ) )
			++pos;

		if( pos >= cmd.size() )
			break;

		// Check if current token is -game (case-insensitive usually, but HL uses lowercase)
		if( cmd.compare( pos, 5, "-game", 0, 5 ) == 0 ||
			cmd.compare( pos, 6, "-game ", 0, 6 ) == 0 ) // sometimes space follows immediately
		{
			pos += 5; // skip "-game"

			// Skip whitespace after -game
			while( pos < cmd.size() && std::isspace( static_cast<unsigned char>( cmd[pos] ) ) )
				++pos;

			// Now skip the game/mod name (quoted or unquoted word)
			if( pos < cmd.size() && cmd[pos] == '"' )
			{
				// Quoted mod name
				++pos; // skip opening "
				size_t endQuote = cmd.find( '"', pos );
				if( endQuote != std::string::npos )
					pos = endQuote + 1;
				else
					pos = cmd.size(); // malformed - take rest as-is
			}
			else
			{
				// Unquoted mod name (single word, no spaces)
				while( pos < cmd.size() && !std::isspace( static_cast<unsigned char>( cmd[pos] ) ) )
					++pos;
			}

			// Skip any whitespace after the mod value
			while( pos < cmd.size() && std::isspace( static_cast<unsigned char>( cmd[pos] ) ) )
				++pos;

			// Everything from here is what we want: -console, +map, etc.
			return cmd.substr( pos );
		}

		// Not -game - skip this entire token
		while( pos < cmd.size() && !std::isspace( static_cast<unsigned char>( cmd[pos] ) ) )
			++pos;
	}

	// If no -game found or nothing after it
	return "";
}

extern "C" __declspec(dllexport) void HUD_Init( void )
{
	//-------------------STUB START
	// get all arguments after steam launch (-steam -game modname)
	std::string tailArgs = GetArgsAfterGame();
	const char *launch_args = tailArgs.c_str();

	// get mod path
	static char modDir[256] = { 0 };

	const char *gameDir = gEngfuncs.pfnGetGameDirectory();
	if( gameDir && *gameDir )
	{
		strncpy( modDir, gameDir, sizeof( modDir ) - 1 );
		modDir[sizeof( modDir ) - 1] = '\0';
	}

	// ïet full path to hl.exe
	char exePath[MAX_PATH] = { 0 };
	GetModuleFileNameA( NULL, exePath, MAX_PATH );

	// remove the filename part leaves the directory (e.g. "C:\Steam\steamapps\common\Half-Life\")
	PathRemoveFileSpecA( exePath );

	// Build path to xash3d.exe in the same folder
	char xashPath[MAX_PATH] = { 0 };
#if defined USE_DIFFUSION
	snprintf( xashPath, sizeof( xashPath ), "%s\\%s\\diffusion.exe", exePath, gameDir );
#elif defined USE_HALFLIFE
	snprintf( xashPath, sizeof( xashPath ), "%s\\%s\\hl.exe", exePath, gameDir );
#else
	snprintf( xashPath, sizeof( xashPath ), "%s\\%s\\xash3d.exe", exePath, gameDir );
#endif

	// prepare path to Xash3D (hardcode or load from config)
	static char args[256] = { 0 };
#ifndef USE_DIFFUSION
	// xash3d.exe and hl.exe will use this
	snprintf( args, sizeof( args ), "-game %s %s", gameDir, launch_args );
#endif

	STARTUPINFO si = { sizeof( si ) };
	PROCESS_INFORMATION pi;

	char cmdLine[512];
	snprintf( cmdLine, sizeof( cmdLine ), "\"%s\" %s", xashPath, args );

	if( CreateProcessA( NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ) ) {
		// Optional: Wait briefly or signal via file/IPC if transferring state
		CloseHandle( pi.hProcess );
		CloseHandle( pi.hThread );

		// Exit current process (hl.exe)
		ExitProcess( 0 );
	}
	else {
		// Handle error, e.g., MessageBox
	}

	return; // we don't need anything else, this is a stub 32-bit dll
	//-------------------STUB END

#if XASH_64BIT && XASH_WIN32
	discord_integration::initialize();
#endif
	
	InitInput();
	gHUD.Init();

	if( g_fRenderInitialized )
		R_Init();

	// particles allowed in anyway
	g_pParticleSystems = new CParticleSystemManager();

	// diffusion - load achievement stats
	gHUD.m_StatusIconsAchievement.LoadAchievementFile();
}

extern "C" __declspec(dllexport)  void HUD_Shutdown( void )
{
	// diffusion - save achievement stats
	gHUD.m_StatusIconsAchievement.SaveAchievementFile();

#if XASH_64BIT && XASH_WIN32
	discord_integration::shutdown();
#endif
	
	ShutdownInput();

	if( g_fRenderInitialized )
		R_Shutdown();

	if( g_pParticleSystems )
	{
		delete g_pParticleSystems;
		g_pParticleSystems = NULL;
	}
}

/*
==========================
	HUD_Redraw

called every screen frame to
redraw the HUD.
===========================
*/
extern "C" __declspec(dllexport)  int HUD_Redraw(float time, int intermission)
{
	return gHUD.Redraw( time, intermission );
}

/*
==========================
	HUD_UpdateClientData

called every time shared client
dll/engine data gets changed,
and gives the cdll a chance
to modify the data.

returns 1 if anything has been changed, 0 otherwise.
==========================
*/
extern "C" __declspec(dllexport)  int HUD_UpdateClientData(client_data_t * pcldata, float flTime)
{
#if XASH_64BIT && XASH_WIN32
	discord_integration::on_update_client_data();
#endif
	
	return gHUD.UpdateClientData( pcldata, flTime );
}

/*
==========================
	HUD_Reset

Called at start and end of demos to restore to "non"HUD state.
==========================
*/
extern "C" __declspec(dllexport)  void HUD_Reset(void)
{
	gHUD.VidInit();
}

static int *VGUI_GetRect( void )
{
	static int extent[4];
#ifdef _WIN32
	RECT wrect;

	if( GetWindowRect( GetActiveWindow(), &wrect ))
	{
		if( !wrect.left )
		{
			// fullscreen
			extent[0] = wrect.left;
			extent[1] = wrect.top;
			extent[2] = wrect.right;
			extent[3] = wrect.bottom;
		}
		else
		{
			// window
			extent[0] = wrect.left + 4;
			extent[1] = wrect.top + 30;
			extent[2] = wrect.right - 4;
			extent[3] = wrect.bottom - 4;
		}
	}
#else
	extent[0] = gEngfuncs.GetWindowCenterX() - ScreenWidth / 2;
	extent[1] = gEngfuncs.GetWindowCenterY() - ScreenHeight / 2;
	extent[2] = gEngfuncs.GetWindowCenterX() + ScreenWidth / 2;
	extent[3] = gEngfuncs.GetWindowCenterY() + ScreenHeight / 2;
#endif
	return extent;	
}

/*
==========================
HUD_Frame

Called by engine every frame that client .dll is loaded
==========================
*/
extern "C" __declspec(dllexport)  void HUD_Frame( double time )
{
#if XASH_64BIT && XASH_WIN32
	discord_integration::on_frame();
#endif

	if( tr.bGammaTableUpdate )
	{
		// put the gamma into GLSL-friendly array
		for( int i = 0; i < 256; i++ )
			tr.gamma_table[i / 4][i % 4] = (float)TEXTURE_TO_TEXGAMMA( i ) / 255.0f;

		tr.bGammaTableUpdate = false;
	}
}

extern "C" __declspec(dllexport) int HUD_Key_Event( int eventcode, int keynum, const char *pszCurrentBinding )
{
	bool bUseButton = !Q_strcmp( pszCurrentBinding, "+use" );
	bool bEscButton = false;
	if( !bUseButton )
		bEscButton = (keynum == K_ESCAPE || !Q_strcmp( pszCurrentBinding, "escape" ));
	
	if( gHUD.m_Puzzle.m_iFlags & HUD_ACTIVE && (bEscButton || bUseButton) )
	{
		gHUD.m_Puzzle.m_iFlags &= ~HUD_ACTIVE;
		return 0;
	}
	else if( gHUD.m_CodeInput.m_iFlags & HUD_ACTIVE )
	{
		if( bEscButton )
		{
			gHUD.m_CodeInput.DisableCodeScreen();
			return 0;
		}

		if( keynum >= K_NUM0 && keynum <= K_NUM9 )
		{
			// if it's a number key, but not a slot command, fallback to typing the number anyway
			if( Q_strncmp( pszCurrentBinding, "slot", 4 ) )
			{
				gHUD.m_CodeInput.EnterCode( keynum - K_NUM0 );
				return 1;
			}
		}
	}
	else if( gHUD.PlayingDrums )
	{
		if( keynum >= K_NUM0 && keynum <= K_NUM9 )
		{
			// if it's a number key, but not a slot command, fallback to typing the number anyway
			if( Q_strncmp( pszCurrentBinding, "slot", 4 ) )
			{
				gHUD.DrumsInput( keynum - K_NUM0 );
				return 1;
			}
		}
	}

	if( gHUD.m_PseudoGUI.m_iFlags & HUD_ACTIVE )
	{
		if( keynum == K_MWHEELDOWN )
		{
			gHUD.m_PseudoGUI.scrolled_lines++; // scroll down
			return 0;
		}
		else if( keynum == K_MWHEELUP )
		{
			gHUD.m_PseudoGUI.scrolled_lines--; // scroll up
			return 0;
		}
	}

	if( (gHUD.m_PseudoGUI.m_iFlags & HUD_ACTIVE) && (keynum == K_ENTER || keynum == K_MOUSE1 || bUseButton || !Q_strcmp(pszCurrentBinding, "+attack")) )
	{
		gHUD.m_PseudoGUI.CloseWindow( keynum == K_MOUSE1 || !Q_strcmp( pszCurrentBinding, "+attack" ) );
		return 0;
	}

	if( gHUD.m_CodeInput.CodeInputScreenIsOn && bEscButton )
	{
		gHUD.m_CodeInput.DisableCodeScreen();
		return 0;
	}

	if( bUseButton )
	{
		// currently drawing a tutor
		if( gHUD.m_StatusIconsTutor.IsTutorDrawing && (gHUD.m_flTime > gHUD.m_StatusIconsTutor.TutorStartTime + 1) && !gHUD.m_StatusIconsTutor.alpha_direction )
		{
			gHUD.m_StatusIconsTutor.alpha_direction = true; // let's hide the tutor
			return 0;
		}

		if( gHUD.m_PseudoGUI.m_iFlags & HUD_ACTIVE ) // make sure we are not viewing a note
			return 0;
	}

	return 1;
}

extern "C" __declspec(dllexport)  void HUD_PostRunCmd(struct local_state_s*, local_state_s*, struct usercmd_s*, int, double, unsigned int)
{
}

extern "C" __declspec(dllexport)  void HUD_VoiceStatus( int entindex, qboolean bTalking )
{
	if( entindex == -1 )
		gHUD.m_ScreenEffects.ShouldDrawVoiceIcon = bTalking;
	else if( entindex == -2 )
		gHUD.m_Scoreboard.ThisPlayerTalking = bTalking;
	else
	{
		if( entindex > 0 && entindex <= MAX_PLAYERS + 1 )
			g_PlayerExtraInfo[entindex].isTalking = bTalking;
	}
}

extern "C" __declspec(dllexport)  void HUD_DirectorMessage( int iSize, void *pbuf )
{
}

extern "C" __declspec(dllexport) void Demo_ReadBuffer( int size, unsigned char *buffer )
{
}

extern "C" __declspec(dllexport) cl_entity_t *HUD_GetUserEntity( int index )
{
	return NULL;
}

cldll_func_t cldll_func = 
{
	Initialize,
	HUD_Init,
	HUD_VidInit,
	HUD_Redraw,
	HUD_UpdateClientData,
	HUD_Reset,
	HUD_PlayerMove,
	HUD_PlayerMoveInit,
	HUD_PlayerMoveTexture,
	IN_ActivateMouse,
	IN_DeactivateMouse,
	IN_MouseEvent,
	IN_ClearStates,
	IN_Accumulate,
	CL_CreateMove,
	CL_IsThirdPerson,
	CL_CameraOffset,
	KB_Find,
	CAM_Think,
	V_CalcRefdef,
	HUD_AddEntity,
	HUD_CreateEntities,
	HUD_DrawNormalTriangles,
	HUD_DrawTransparentTriangles,
	HUD_StudioEvent,
	HUD_PostRunCmd,
	HUD_Shutdown,
	HUD_TxferLocalOverrides,
	HUD_ProcessPlayerState,
	HUD_TxferPredictionData,
	Demo_ReadBuffer,
	HUD_ConnectionlessPacket,
	HUD_GetHullBounds,
	HUD_Frame,
	HUD_Key_Event,
	HUD_TempEntUpdate,
	HUD_GetUserEntity,
	HUD_VoiceStatus,
	HUD_DirectorMessage,
	HUD_GetStudioModelInterface,
	NULL,	// HUD_ChatInputPosition,
	HUD_GetRenderInterface,
	NULL,	/// HUD_ClipMoveToEntity
};

extern "C" void DLLEXPORT GetClientAPI( void *pv )
{
	cldll_func_t *pcldll_func = (cldll_func_t *)pv;
	*pcldll_func = cldll_func;
}
