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

int developer_level;
int g_iXashEngineBuildNumber;
cl_enginefunc_t gEngfuncs;
render_api_t gRenderfuncs;
CHud gHUD;

//==========================================================
// Initialize: called when the DLL is first loaded.
//==========================================================
int Initialize( cl_enginefunc_t *pEnginefuncs, int iVersion )
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
int HUD_GetHullBounds( int hullnumber, float *mins, float *maxs )
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
int HUD_ConnectionlessPacket( const struct netadr_s *, const char *, char *, int *response_buffer_size )
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

void HUD_PlayerMoveInit( struct playermove_s *ppmove )
{
	PM_Init( ppmove );
}

char HUD_PlayerMoveTexture( char *name )
{
	return PM_FindTextureType( name );
}

void HUD_PlayerMove( struct playermove_s *ppmove, int server )
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
int HUD_VidInit(void)
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
void HUD_Init( void )
{
#ifdef XASH_64BIT
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

void HUD_Shutdown( void )
{
	// diffusion - save achievement stats
	gHUD.m_StatusIconsAchievement.SaveAchievementFile();

#ifdef XASH_64BIT
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
int HUD_Redraw(float time, int intermission)
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
int HUD_UpdateClientData(client_data_t * pcldata, float flTime)
{
#ifdef XASH_64BIT
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
void HUD_Reset(void)
{
	gHUD.VidInit();
}

static int *VGUI_GetRect( void )
{
	static int extent[4];
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
	return extent;	
}

/*
==========================
HUD_Frame

Called by engine every frame that client .dll is loaded
==========================
*/
void HUD_Frame( double time )
{
#ifdef XASH_64BIT
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

int HUD_Key_Event( int eventcode, int keynum, const char *pszCurrentBinding )
{
	if( gHUD.m_Puzzle.m_iFlags & HUD_ACTIVE && !Q_strcmp( pszCurrentBinding, "escape" ) )
	{
		gHUD.m_Puzzle.m_iFlags &= ~HUD_ACTIVE;
		return 0;
	}
	
	if( (gHUD.m_PseudoGUI.m_iFlags & HUD_ACTIVE) && (keynum == K_ENTER || keynum == K_MOUSE1 || !Q_strcmp(pszCurrentBinding, "+attack") || !Q_strcmp( pszCurrentBinding, "escape" )) )
	{
		gHUD.m_PseudoGUI.CloseWindow( keynum == K_MOUSE1 || !Q_strcmp( pszCurrentBinding, "+attack" ) );
		return 0;
	}

	if( gHUD.m_CodeInput.CodeInputScreenIsOn && !Q_strcmp( pszCurrentBinding, "escape" ) )
	{
		gHUD.m_CodeInput.DisableCodeScreen();
		return 0;
	}

	if( !Q_strcmp(pszCurrentBinding, "+use") )
	{
		// currently drawing a tutor
		if( gHUD.m_StatusIconsTutor.IsTutorDrawing && (gHUD.m_flTime > gHUD.m_StatusIconsTutor.TutorStartTime + 1) && !gHUD.m_StatusIconsTutor.x_direction )
		{
			gHUD.m_StatusIconsTutor.x_direction = true; // let's hide the tutor
			return 0;
		}

		if( gHUD.m_PseudoGUI.m_iFlags & HUD_ACTIVE ) // make sure we are not viewing a note
			return 0;
	}

	return 1;
}

void HUD_PostRunCmd(struct local_state_s*, local_state_s*, struct usercmd_s*, int, double, unsigned int)
{
}

void HUD_VoiceStatus( int entindex, qboolean bTalking )
{
	if( entindex == -1 )
		gHUD.m_ScreenEffects.ShouldDrawVoiceIcon = bTalking;

	if( entindex > 0 && entindex <= MAX_PLAYERS + 1 )
		g_PlayerExtraInfo[entindex].isTalking = bTalking;
}

void HUD_DirectorMessage( int iSize, void *pbuf )
{
}

void Demo_ReadBuffer( int size, unsigned char *buffer )
{
}

cl_entity_t *HUD_GetUserEntity( int index )
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