/*
utils.h - utility code
Copyright (C) 2011 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef UTILS_H
#define UTILS_H

#include "cvardef.h"
#include "exportdef.h"

#include "net_api.h"

#include <stdio.h> // for safe_sprintf()
#include <stdarg.h>  // "
#include <string> // for strncpy()

#ifndef _WIN32
#include <limits.h>
#define MAX_PATH PATH_MAX
#endif // _WIN32

typedef unsigned char byte;
typedef unsigned short word;

typedef int (*pfnUserMsgHook)( const char *pszName, int iSize, void *pbuf );
typedef int (*cmpfunc)( const void *a, const void *b );

extern int developer_level;
extern int r_currentMessageNum;
extern float v_idlescale;

extern int g_iXashEngineBuildNumber;
extern bool g_fRenderInitialized;

#define SND_SPAWNING		(1<<8)		// we're spawing, used in some cases for ambients 
#define SND_STOP			(1<<5)		// stop sound
#define SND_CHANGE_VOL		(1<<6)		// change sound vol
#define SND_CHANGE_PITCH	(1<<7)		// change sound pitch
#define SND_LOCALSOUND		(1<<9)	// not paused, not looped, for internal use
#define SND_STOP_LOOPING		(1<<10)	// stop all looping sounds on the entity.
#define SND_FILTER_CLIENT		(1<<11)	// don't send sound from local player if prediction was enabled
#define SND_RESTORE_POSITION		(1<<12)	// passed playing position and the forced end

enum
{
	DEV_NONE = 0,
	DEV_NORMAL,
	DEV_EXTENDED
};

#ifdef _WIN32
typedef HMODULE dllhandle_t;
#else
typedef void *dllhandle_t;
#endif

typedef struct dllfunc_s
{
	const char *name;
	void	**func;
} dllfunc_t;

client_sprite_t *GetSpriteList( client_sprite_t *pList, const char *psz, int iRes, int iCount );
extern void DrawQuad( float xmin, float ymin, float xmax, float ymax );
extern void ALERT( ALERT_TYPE level, const char *szFmt, ... );

struct model_s *Mod_Handle( int modelIndex );

// dll managment
bool Sys_LoadLibrary( const char *dllname, dllhandle_t *handle, const dllfunc_t *fcts = NULL );
void *Sys_GetProcAddress( dllhandle_t handle, const char *name );
void Sys_FreeLibrary( dllhandle_t *handle );
bool Sys_RemoveFile(const char* path);

// ScreenHeight returns the height of the screen, in pixels
#define ScreenHeight	(gHUD.m_scrinfo.iHeight)
// ScreenWidth returns the width of the screen, in pixels
#define ScreenWidth		(gHUD.m_scrinfo.iWidth)

// Use this to set any co-ords in 640x480 space
#define XRES( x )		((int)(float( x ) * ((float)ScreenWidth / 640.0f) + 0.5f))
#define YRES( y )		((int)(float( y ) * ((float)ScreenHeight / 480.0f) + 0.5f))

// use this to project world coordinates to screen coordinates
#define XPROJECT( x )	(( 1.0f + (x)) * ScreenWidth * 0.5f )
#define YPROJECT( y )	(( 1.0f - (y)) * ScreenHeight * 0.5f )

extern const float g_hullcolor[8][3];
extern const int g_boxpnt[6][4];

template<typename T, size_t N>
char( &ArraySizeHelper( T( & )[N] ) )[N];
#define ARRAYSIZED(x) sizeof(ArraySizeHelper(x))

static size_t get_map_name( char *dest, size_t count )
{
	auto map_path = gEngfuncs.pfnGetLevelName();

	const char *slash = strrchr( map_path, '/' );
	if( !slash )
		slash = map_path - 1;

	const char *dot = strrchr( map_path, '.' );
	if( !dot )
		dot = map_path + strlen( map_path );

	size_t bytes_to_copy = Q_min( count - 1, static_cast<size_t>(dot - slash - 1) );

	Q_strncpy( dest, slash + 1, bytes_to_copy + 1 );
	dest[bytes_to_copy] = '\0';

	return bytes_to_copy;
}

static size_t get_player_count()
{
	size_t player_count = 0;

	for( int i = 0; i < MAX_PLAYERS; ++i ) {
		// Make sure the information is up to date.
		GetPlayerInfo( i + 1, &g_PlayerInfoList[i + 1] );

		// This player slot is empty.
		if( g_PlayerInfoList[i + 1].name == nullptr )
			continue;

		++player_count;
	}

	return player_count;
}

inline void remove_color_characters( char *input_string )
{
	char *read = input_string;
	char *write = input_string;

	while( *read ) {
		if( *read == '^' && isdigit( *(read + 1) ) ) {
			read += 2;
		}
		else {
			*write++ = *read++;
		}
	}

	*write = '\0';
}

inline int ConsoleStringLen( const char *string )
{
	int _width, _height;
	GetConsoleStringSize( string, &_width, &_height );
	return _width;
}

void ScaleColors( int &r, int &g, int &b, int a );

float PackColor( const color24 &color );

inline void UnpackRGB(int &r, int &g, int &b, unsigned long ulRGB)
{
	r = (ulRGB & 0xFF0000) >>16;
	g = (ulRGB & 0xFF00) >> 8;
	b = ulRGB & 0xFF;
}

SpriteHandle LoadSprite( const char *pszName );

typedef struct leaflist_s
{
	int		count;
	int		maxcount;
	bool		overflowed;
	short		*list;
	Vector		mins, maxs;
	struct mnode_s	*headnode;	// for overflows where each leaf can't be stored individually
} leaflist_t;

float CL_UTIL_Approach( float target, float value, float speed );
void CL_UTIL_Sparks( Vector org );
void CalcShake( void );
void ApplyShake( float *origin, float *angles, float factor );

struct mleaf_s *Mod_PointInLeaf( Vector p, struct mnode_s *node );
bool Mod_BoxVisible( const Vector mins, const Vector maxs, const byte *visbits );
bool Mod_CheckEntityPVS( cl_entity_t *ent );
bool Mod_CheckTempEntityPVS( struct tempent_s *pTemp );
bool Mod_CheckEntityLeafPVS( const Vector &absmin, const Vector &absmax, struct mleaf_s *leaf );
bool Mod_CheckBoxVisible( const Vector &absmin, const Vector &absmax );
void Mod_GetFrames( int modelIndex, int &numFrames );
int Mod_GetType( int modelIndex );

bool R_ScissorForAABB( const Vector &absmin, const Vector &absmax, float *x, float *y, float *w, float *h );
bool R_ScissorForCorners( const Vector bbox[8], float *x, float *y, float *w, float *h );
bool R_AABBToScreen( const Vector &absmin, const Vector &absmax, Vector2D &scrmin, Vector2D &scrmax, wrect_t *rect = NULL );
void R_DrawScissorRectangle( float x, float y, float w, float h );
void R_TransformWorldToDevice( const Vector &world, Vector &ndc );
void R_TransformDeviceToScreen( const Vector &ndc, Vector &screen );
bool R_ClipPolygon( int numPoints, Vector *points, const struct mplane_s *plane, int *numClipped, Vector *clipped );
void R_SplitPolygon( int numPoints, Vector *points, const struct mplane_s *plane, int *numFront, Vector *front, int *numBack, Vector *back );
bool UTIL_IsPlayer( int idx );
bool UTIL_IsLocal( int idx );

extern int HUD_AddEntity( int type, struct cl_entity_s *ent, const char *modelname );
extern void HUD_CreateEntities( void );
extern void HUD_StudioEvent( const struct mstudioevent_s *event, const struct cl_entity_s *entity );
extern void HUD_TxferLocalOverrides( struct entity_state_s *state, const struct clientdata_s *client );
extern void HUD_ProcessPlayerState( struct entity_state_s *dst, const struct entity_state_s *src );
extern void HUD_TxferPredictionData( struct entity_state_s *ps, const struct entity_state_s *pps, struct clientdata_s *pcd, const struct clientdata_s *ppcd, struct weapon_data_s *wd, const struct weapon_data_s *pwd );
extern void HUD_TempEntUpdate( double frametime, double client_time, double cl_gravity, struct tempent_s **ppTempEntFree, struct tempent_s **ppTempEntActive, int (*Callback_AddVisibleEntity)(struct cl_entity_s *pEntity), void (*Callback_TempEntPlaySound)(struct tempent_s *pTemp, float damp) );

extern int HUD_GetRenderInterface( int version, render_api_t *renderfuncs, render_interface_t *callback );
extern int HUD_GetStudioModelInterface( int version, struct r_studio_interface_s **ppinterface, struct engine_studio_api_s *pstudio );

extern int CL_ButtonBits( int );
extern void CL_ResetButtonBits( int bits );
extern void InitInput( void );
extern void ShutdownInput( void );
extern int KB_ConvertString( char *in, char **ppout );
extern void IN_Init( void );
extern void IN_Move( float frametime, struct usercmd_s *cmd );
extern void IN_Shutdown( void );

extern void IN_ActivateMouse( void );
extern void IN_DeactivateMouse( void );
extern void IN_MouseEvent( int mstate );
extern void IN_Accumulate( void );
extern void IN_ClearStates( void );
extern void *KB_Find( const char *name );
extern void CL_CreateMove( float frametime, struct usercmd_s *cmd, int active );
extern int CL_IsDead( void );

extern void HUD_DrawNormalTriangles( void );
extern void HUD_DrawTransparentTriangles( void );

extern void PM_Init( struct playermove_s *ppmove );
extern void PM_Move( struct playermove_s *ppmove, int server );
extern char PM_FindTextureType( const char *name );
extern void V_CalcRefdef( struct ref_params_s *pparams );

void UTIL_CreateAurora( cl_entity_t *ent, const char *file, int attachment, float lifetime = 0.0f );
void UTIL_RemoveAurora( cl_entity_t *ent );
extern int PM_GetPhysEntInfo( int ent );

extern void CAM_Think( void );
extern void CL_CameraOffset( float *ofs );
extern int CL_IsThirdPerson( void );

// xxx need client dll function to get and clear impuse
extern cvar_t *in_joystick;
extern int g_weaponselect;

void UTIL_ReplaceKeyBindings( const char *inbuf, int inbufsizebytes, char *outbuf );
   
#endif // UTILS_H
