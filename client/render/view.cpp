//=======================================================================
//			Copyright (C) Shambler Team 2005
//			r_view.cpp - multipass renderer
//=======================================================================

#include "hud.h"
#include "utils.h"
#include "r_view.h"
#include "eiface.h"
#include "ref_params.h"
#include "pm_movevars.h"
#include "pm_defs.h"
#include "r_local.h"
#include "r_studioint.h"
#include "r_studio.h"
#include "event_api.h"
#include <mathlib.h>
#include "ammohistory.h"
#include "cdll_exp.h"

// thirdperson camera
void CAM_Think( void ) {}
void CL_CameraOffset( float *ofs ) { g_vecZero.CopyToArray( ofs ); }
int CL_IsThirdPerson( void )
{ 
	if( gHUD.m_iCameraMode == 1 )
		return 1;

	return 0;
}

cl_entity_t *v_intermission_spot;
float v_idlescale;
int pause = 0;

float GunPosZCurrent = 0;

bool UnderwaterSoundPlaying = false;

// spectator mode
Vector		v_origin, v_angles, v_cl_angles, v_sim_org, v_lastAngles;
Vector	vJumpOrigin;
Vector	vJumpAngles;
float		v_frametime, v_lastDistance;
float		v_cameraRelaxAngle = 5.0f;
float		v_cameraFocusAngle = 35.0f;
qboolean	v_resetCamera = 1;

#define	HL2_BOB_CYCLE_MIN	1.0f
#define	HL2_BOB_CYCLE_MAX	0.45f
#define	HL2_BOB			0.002f
#define	HL2_BOB_UP		0.5f
float    g_lateralBob;
float    g_verticalBob;
float RemapVal( float val, float A, float B, float C, float D);

cvar_t	*gl_test;
cvar_t	*r_extensions;
cvar_t	*cl_bobcycle;
cvar_t	*cl_bob;
cvar_t	*cl_bobup;
cvar_t	*cl_waterdist;
cvar_t	*cl_chasedist;
cvar_t	*cl_weaponlag;
cvar_t	*cl_vsmoothing;
cvar_t	*v_centermove;
cvar_t	*v_centerspeed;
cvar_t	*cl_viewsize;
cvar_t	*gl_renderer;
cvar_t	*gl_check_errors;
cvar_t	*r_finish;
cvar_t	*r_clear;
cvar_t	*r_speeds;
cvar_t	*r_novis;
cvar_t	*r_nocull;
cvar_t	*r_nosort;
cvar_t	*r_lockpvs;
cvar_t	*r_dynamic;
cvar_t	*r_shadows;
cvar_t	*r_lightmap;
cvar_t	*vid_gamma;
cvar_t	*vid_brightness;
cvar_t	*r_adjust_fov;
cvar_t	*r_wireframe;
cvar_t	*r_fullbright;
cvar_t	*r_allow_3dsky;
cvar_t	*gl_allow_portals;
cvar_t	*gl_allow_screens;
cvar_t	*r_drawentities;
cvar_t	*r_drawworld;
cvar_t	*r_drawsprites;
cvar_t *r_drawmodels;
cvar_t	*r_recursion_depth;
cvar_t	*r_lighting_modulate;
cvar_t	*r_lighting_extended;
cvar_t	*r_lightstyle_lerping;
cvar_t	*r_recursive_world_node;
cvar_t	*r_polyoffset;
cvar_t	*r_grass;
cvar_t	*r_grass_alpha;
cvar_t	*r_grass_lighting;
cvar_t	*r_grass_shadows;
cvar_t	*r_grass_fade_start;
cvar_t	*r_grass_fade_dist;
cvar_t	*r_show_renderpass;
cvar_t	*r_show_light_scissors;
cvar_t	*r_show_normals;
cvar_t	*r_show_lightprobes;
cvar_t	*r_fade_props;
cvar_t	*r_show_cubemaps;
cvar_t *r_bloom_sample_size;
cvar_t *r_bloom_fast_sample;
cvar_t *r_bloom_darken;
cvar_t *r_bloom_alpha;
cvar_t *r_bloom_diamond_size;
cvar_t *r_bloom_intensity;
cvar_t *r_bloom_sprites;
cvar_t *r_bloom;
cvar_t	*thirdperson;
cvar_t	*r_sun_allowed;
cvar_t *cl_hitsound;
cvar_t *cl_muzzlelight;
cvar_t *cl_localweaponanims;
cvar_t *cl_achievement_notify;
cvar_t *cl_showmsgs;
cvar_t *cl_viewmodelblend;
cvar_t *cl_tutor;
cvar_t *cl_showhealthbars;
cvar_t *gl_sunshafts;
cvar_t *gl_sunshafts_blur;
cvar_t *gl_sunshafts_brightness;
cvar_t *gl_ssao;
cvar_t *gl_ssao_debug;
cvar_t *gl_lensflare;
cvar_t *gl_water_refraction;
cvar_t *gl_water_planar;
cvar_t *gl_exposure;
cvar_t *gl_emboss;
cvar_t *gl_specular;
cvar_t *gl_cubemaps;
cvar_t *gl_bump;
cvar_t *cubeshot; // advanced envshot :)
cvar_t *cl_notbn;
cvar_t *r_show_tbn;

cvar_t  *r_blur;
cvar_t *r_blur_threshold;
cvar_t *r_blur_strength; // only horizontal blur
cvar_t *r_shadowquality;
cvar_t *r_mirrorquality;
cvar_t *r_testdlight;

cvar_t	v_iyaw_cycle	= { "v_iyaw_cycle", "2", 0, 2 };
cvar_t	v_iroll_cycle	= { "v_iroll_cycle", "0.5", 0, 0.5 };
cvar_t	v_ipitch_cycle	= { "v_ipitch_cycle", "1", 0, 1 };
cvar_t	v_iyaw_level  	= { "v_iyaw_level", "0.3", 0, 0.3f };
cvar_t	v_iroll_level 	= { "v_iroll_level", "0.1", 0, 0.1f };
cvar_t	v_ipitch_level	= { "v_ipitch_level", "0.3", 0, 0.3f };

cvar_t *ui_is_active;
cvar_t *ui_backgroundmap_active;

int g_iPlayerClass;
int g_iTeamNumber;
int g_iUser1 = 0;
int g_iUser2 = 0;
int g_iUser3 = 0;
bool g_bDucked = false;
float g_fFrametime = 0.0f;

float localanim_NextPAttackTime = 0;
float localanim_NextSAttackTime = 0;
int localanim_WeaponID = 0;
bool localanim_AllowRpgShoot = false;

//============================================================================== 
//				VIEW RENDERING 
//============================================================================== 

//============================================================
// diffusion - moved screen shake from engine to here
//============================================================
void CalcShake( void )
{
	int	i;
	float	fraction, freq;
	float	localAmp;

	if( gHUD.shake.time == 0.0f )
		return;

	if( (tr.time > gHUD.shake.time) || gHUD.shake.amplitude <= 0 || gHUD.shake.frequency <= 0 )
	{
		memset( &gHUD.shake, 0, sizeof( gHUD.shake ) );
		return;
	}

	if( tr.time > gHUD.shake.next_shake )
	{
		// higher frequency means we recalc the extents more often and perturb the display again
		gHUD.shake.next_shake = tr.time + (1.0f / gHUD.shake.frequency);

		// compute random shake extents (the shake will settle down from this)
		for( i = 0; i < 3; i++ )
			gHUD.shake.offset[i] = RANDOM_FLOAT( -gHUD.shake.amplitude, gHUD.shake.amplitude );
		gHUD.shake.angle = RANDOM_FLOAT( -gHUD.shake.amplitude * 0.25f, gHUD.shake.amplitude * 0.25f );
	}

	// ramp down amplitude over duration (fraction goes from 1 to 0 linearly with slope 1/duration)
	fraction = (gHUD.shake.time - tr.time) / gHUD.shake.duration;

	// ramp up frequency over duration
	if( fraction )
	{
		freq = (gHUD.shake.frequency / fraction);
	}
	else
	{
		freq = 0;
	}

	// square fraction to approach zero more quickly
	fraction *= fraction;

	// Sine wave that slowly settles to zero
	fraction = fraction * sin( tr.time * freq );

	// add to view origin
	gHUD.shake.applied_offset = gHUD.shake.offset * fraction;

	// add to roll
	gHUD.shake.applied_angle = gHUD.shake.angle * fraction;

	// drop amplitude a bit, less for higher frequency shakes
	localAmp = gHUD.shake.amplitude * ( g_fFrametime / (gHUD.shake.duration * gHUD.shake.frequency));
	gHUD.shake.amplitude -= localAmp;
}

void ApplyShake( float *origin, float *angles, float factor )
{
	if( origin ) VectorMA( origin, factor, gHUD.shake.applied_offset, origin );
	if( angles ) angles[ROLL] += gHUD.shake.applied_angle * factor;
}

//============================================================
// V_ChangeView: switch between first/third person view.
//============================================================
void V_ChangeView(void)
{
	if (CVAR_TO_BOOL(thirdperson))
	{
		if (g_iUser1)
		{
			gEngfuncs.Con_Printf("Thirdperson is not allowed in spectator mode.\n");
			gHUD.m_iCameraMode = 0;
		}
		else
			gHUD.m_iCameraMode = 1;
	}
		
	else
		gHUD.m_iCameraMode = 0;
}

//=======================================================================
// PlayWallSlideSound: play a sound of friction between player and walls
//=======================================================================
void PlayWallSlideSound( struct ref_params_s *pparams )
{
	bool SoundDisabled = false;

	if( (pparams->waterlevel > 1) || g_iUser1 || pparams->intermission || (pparams->health <= 0) )
		SoundDisabled = true;

	if( SoundDisabled )
	{
		gEngfuncs.pEventAPI->EV_StopSound( gEngfuncs.GetLocalPlayer()->index, CHAN_STATIC, "player/wallslide.wav" );
		return;
	}

	Vector PlayerOrg = gEngfuncs.GetLocalPlayer()->origin;
	pmtrace_t LeftTrace, RightTrace, FrontTrace, BackTrace;

	// first I checked the centers of the sides, but it's better to check the corners after all
	Vector LeftTraceOrigin = PlayerOrg; LeftTraceOrigin.x -= 17;		LeftTraceOrigin.y += 17;
	Vector RightTraceOrigin = PlayerOrg; RightTraceOrigin.x += 17;		RightTraceOrigin.y += 17;
	Vector FrontTraceOrigin = PlayerOrg; FrontTraceOrigin.x -= 17;		FrontTraceOrigin.y -= 17;
	Vector BackTraceOrigin = PlayerOrg; BackTraceOrigin.x += 17;		BackTraceOrigin.y -= 17;

	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( PlayerOrg, LeftTraceOrigin, PM_NORMAL, -1, &LeftTrace );
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( PlayerOrg, RightTraceOrigin, PM_NORMAL, -1, &RightTrace );
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( PlayerOrg, FrontTraceOrigin, PM_NORMAL, -1, &FrontTrace );
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( PlayerOrg, BackTraceOrigin, PM_NORMAL, -1, &BackTrace );
//	gEngfuncs.Con_NPrintf( 2, "%f %f %f %f\n", LeftTrace.fraction, RightTrace.fraction, FrontTrace.fraction, BackTrace.fraction );

	int pitch = 50 + pparams->simvel.Length2D() * 0.2;
	pitch = bound( 50, pitch, 100 );

	float volume = 0;

	// we are touching the wall at some point.
	if( (LeftTrace.fraction < 1) || (RightTrace.fraction < 1) || (FrontTrace.fraction < 1) || (BackTrace.fraction < 1) )
		volume = pparams->simvel.Length2D() * 0.0005;

	volume = bound( 0, volume, 0.15 );
	if( volume < 0.01 ) volume = 0;

	// need to "play" sound all the time even at 0 volume so it wouldn't start from the beginning on every touch
	gEngfuncs.pEventAPI->EV_PlaySound( gEngfuncs.GetLocalPlayer()->index, NULL, CHAN_STATIC, "player/wallslide.wav", volume, 0, SND_CHANGE_VOL|SND_CHANGE_PITCH, pitch );
}

//=======================================================================
// PlayFlingWhooshSound: just like Portal! :)
//=======================================================================
void PlayFlingWhooshSound( struct ref_params_s *pparams )
{
	int min_fling_speed = 400;
	static float woosh_vol = 0.0f;

	float fWooshVolume = pparams->simvel.Length() - min_fling_speed;

	if( CL_IsDead() || pparams->waterlevel == 3 )
		fWooshVolume = 0.0f;

	if( fWooshVolume <= 0.0f && woosh_vol <= 0.0f )
	{
		gEngfuncs.pEventAPI->EV_StopSound( gEngfuncs.GetLocalPlayer()->index, CHAN_STATIC, "player/fling_whoosh.wav" );
		return;
	}

	fWooshVolume /= 2000.0f;
	if( fWooshVolume > 1.0f )
		fWooshVolume = 1.0f;

	woosh_vol = CL_UTIL_Approach( fWooshVolume, woosh_vol, g_fFrametime );

	gEngfuncs.pEventAPI->EV_PlaySound( gEngfuncs.GetLocalPlayer()->index, NULL, CHAN_STATIC, "player/fling_whoosh.wav", woosh_vol, 0, SND_CHANGE_VOL | SND_CHANGE_PITCH, PITCH_NORM );
}


//==========================
// V_Init
//==========================
void V_Init( void )
{
	v_centermove	= CVAR_REGISTER( "v_centermove", "0.15", 0 );
	v_centerspeed	= CVAR_REGISTER( "v_centerspeed","500", 0 );

	cl_bobcycle	= CVAR_REGISTER( "cl_bobcycle","0.8", 0 );
	cl_bob		= CVAR_REGISTER( "cl_bob","0.01", 0 );
	cl_bobup		= CVAR_REGISTER( "cl_bobup","0.5", 0 );
	cl_waterdist	= CVAR_REGISTER( "cl_waterdist","4", 0 );
	cl_chasedist	= CVAR_REGISTER( "cl_chasedist","112", 0 );
	cl_weaponlag	= CVAR_REGISTER( "cl_weaponlag", "0.3", FCVAR_ARCHIVE );
	cl_hitsound = CVAR_REGISTER( "cl_hitsound", "0", FCVAR_ARCHIVE );
	cl_achievement_notify = CVAR_REGISTER( "cl_achievement_notify", "1", FCVAR_ARCHIVE );
	cl_showhealthbars = CVAR_REGISTER( "cl_showhealthbars", "1", FCVAR_ARCHIVE );
	cl_localweaponanims = CVAR_REGISTER( "cl_localweaponanims", "0", FCVAR_ARCHIVE );
	cl_muzzlelight = CVAR_REGISTER( "cl_muzzlelight", "1", FCVAR_ARCHIVE );
	cl_showmsgs = CVAR_REGISTER( "cl_showmsgs", "0", FCVAR_ARCHIVE );
	cl_viewmodelblend = CVAR_REGISTER( "cl_viewmodelblend", "0", FCVAR_ARCHIVE );
	cl_tutor = CVAR_REGISTER( "cl_tutor", "1", FCVAR_ARCHIVE );
	cl_notbn = CVAR_REGISTER( "cl_notbn", "0", 0 );
	r_show_tbn = CVAR_REGISTER( "r_show_tbn", "0", 0 );

	// setup some engine cvars for custom rendering
	r_extensions	= CVAR_GET_POINTER( "gl_allow_extensions" );
	r_finish		= CVAR_GET_POINTER( "gl_finish" );
	r_clear		= CVAR_GET_POINTER( "gl_clear" );
	r_speeds		= CVAR_GET_POINTER( "r_speeds" );
	gl_test = CVAR_GET_POINTER( "gl_test" );
	cl_viewsize	= CVAR_GET_POINTER( "viewsize" );

	r_novis		= CVAR_GET_POINTER( "r_novis" );
	r_nocull		= CVAR_GET_POINTER( "r_nocull" );
	r_nosort		= CVAR_GET_POINTER( "gl_nosort" );
	r_lockpvs		= CVAR_GET_POINTER( "r_lockpvs" );
	r_dynamic		= CVAR_GET_POINTER( "r_dynamic" );
	r_lightmap	= CVAR_GET_POINTER( "r_lightmap" );
	r_wireframe	= CVAR_GET_POINTER( "gl_wireframe" );
	r_adjust_fov	= CVAR_GET_POINTER( "r_adjust_fov" );
	gl_check_errors	= CVAR_GET_POINTER( "gl_check_errors" );
	vid_gamma		= CVAR_GET_POINTER( "gamma" );
	vid_brightness	= CVAR_GET_POINTER( "brightness" );
	r_polyoffset	= CVAR_GET_POINTER( "gl_polyoffset" );

	r_fullbright	= CVAR_GET_POINTER( "r_fullbright" );
	r_drawentities	= CVAR_GET_POINTER( "r_drawentities" );
	r_lighting_modulate	= CVAR_GET_POINTER( "r_lighting_modulate" );
	r_lightstyle_lerping= CVAR_GET_POINTER( "cl_lightstyle_lerping" );
	r_lighting_extended	= CVAR_GET_POINTER( "r_lighting_extended" );

	cl_vsmoothing	= CVAR_REGISTER( "cl_vsmoothing", "0.05", FCVAR_ARCHIVE );
	gl_allow_portals	= CVAR_REGISTER( "gl_allow_portals", "1", FCVAR_ARCHIVE|FCVAR_CLIENTDLL );
	gl_allow_screens	= CVAR_REGISTER( "gl_allow_screens", "1", FCVAR_ARCHIVE|FCVAR_CLIENTDLL );
	gl_renderer	= CVAR_REGISTER( "gl_renderer", "1", FCVAR_CLIENTDLL|FCVAR_ARCHIVE ); 
	r_shadows		= CVAR_REGISTER( "r_shadows", "2", FCVAR_CLIENTDLL|FCVAR_ARCHIVE ); 
	r_allow_3dsky	= CVAR_REGISTER( "gl_allow_3dsky", "1", FCVAR_CHEAT );
	r_recursion_depth	= CVAR_REGISTER( "gl_recursion_depth", "1", FCVAR_ARCHIVE );
	r_recursive_world_node = CVAR_REGISTER( "gl_recursive_world_node", "0", FCVAR_ARCHIVE );
	r_drawmodels = CVAR_REGISTER( "r_drawmodels", "1", FCVAR_CHEAT );

	r_grass = CVAR_REGISTER( "r_grass", "1", FCVAR_ARCHIVE );
	r_grass_alpha = CVAR_REGISTER( "r_grass_alpha", "0.5", FCVAR_ARCHIVE );
	r_grass_lighting = CVAR_REGISTER( "r_grass_lighting", "1", FCVAR_ARCHIVE );
	r_grass_shadows = CVAR_REGISTER( "r_grass_shadows", "1", FCVAR_ARCHIVE );
	r_grass_fade_start = CVAR_REGISTER( "r_grass_fade_start", "2048", FCVAR_ARCHIVE );
	r_grass_fade_dist = CVAR_REGISTER( "r_grass_fade_dist", "2048", FCVAR_ARCHIVE );

	r_drawworld = CVAR_REGISTER("r_drawworld", "1", FCVAR_CHEAT);
	r_drawsprites = CVAR_REGISTER( "r_drawsprites", "1", FCVAR_CHEAT );
	r_show_renderpass = CVAR_REGISTER( "r_show_renderpass", "0", 0 );
	r_show_light_scissors = CVAR_REGISTER( "r_show_light_scissors", "0", 0 );
	r_show_normals = CVAR_REGISTER( "r_show_normals", "0", 0 );
	r_show_lightprobes = CVAR_REGISTER( "r_show_lightprobes", "0", 0 );

	r_fade_props = CVAR_REGISTER( "r_fade_props", "1", FCVAR_ARCHIVE );
	r_show_cubemaps = CVAR_REGISTER("r_show_cubemaps", "0", FCVAR_ARCHIVE);
	r_bloom_alpha = CVAR_REGISTER( "r_bloom_alpha", "1", FCVAR_ARCHIVE );
	r_bloom_sample_size = CVAR_REGISTER( "r_bloom_sample_size", "32", FCVAR_ARCHIVE );
	r_bloom_fast_sample = CVAR_REGISTER( "r_bloom_fast_sample", "1", FCVAR_ARCHIVE );
	r_bloom_darken = CVAR_REGISTER( "r_bloom_darken", "0", FCVAR_ARCHIVE );
	r_bloom_diamond_size = CVAR_REGISTER( "r_bloom_diamond_size", "5", FCVAR_ARCHIVE );
	r_bloom_intensity = CVAR_REGISTER( "r_bloom_intensity", "2", FCVAR_ARCHIVE );
	r_bloom_sprites = CVAR_REGISTER( "r_bloom_sprites", "0", FCVAR_ARCHIVE );
	r_bloom = CVAR_REGISTER( "r_bloom", "0", FCVAR_ARCHIVE );
	thirdperson = CVAR_REGISTER("thirdperson", "0", FCVAR_CHEAT);
	r_blur = CVAR_REGISTER( "r_blur", "1", FCVAR_ARCHIVE );
	r_blur_threshold = CVAR_REGISTER( "r_blur_threshold", "5", FCVAR_ARCHIVE );
	r_blur_strength = CVAR_REGISTER( "r_blur_strength", "1", FCVAR_ARCHIVE );
	r_sun_allowed = CVAR_REGISTER("r_sun_allowed", "1", FCVAR_ARCHIVE);
	gl_sunshafts = CVAR_REGISTER( "gl_sunshafts", "1", FCVAR_ARCHIVE );
	gl_sunshafts_blur = CVAR_REGISTER( "gl_sunshafts_blur", "0.15", FCVAR_ARCHIVE );
	gl_sunshafts_brightness = CVAR_REGISTER( "gl_sunshafts_brightness", "0", FCVAR_ARCHIVE );
	gl_ssao = CVAR_REGISTER( "gl_ssao", "1", FCVAR_ARCHIVE );
	gl_ssao_debug = CVAR_REGISTER( "gl_ssao_debug", "0", FCVAR_CHEAT );
	r_shadowquality = CVAR_REGISTER( "r_shadowquality", "0", FCVAR_ARCHIVE );
	r_mirrorquality = CVAR_REGISTER( "r_mirrorquality", "0", FCVAR_ARCHIVE );
	r_testdlight = CVAR_REGISTER( "r_testdlight", "0", FCVAR_CHEAT );
	gl_lensflare = CVAR_REGISTER( "gl_lensflare", "1", FCVAR_ARCHIVE );
	gl_water_refraction = CVAR_REGISTER( "gl_water_refraction", "1", FCVAR_ARCHIVE );
	gl_water_planar = CVAR_REGISTER( "gl_water_planar", "0", FCVAR_ARCHIVE );
	gl_exposure = CVAR_REGISTER( "gl_exposure", "1", FCVAR_ARCHIVE );
	gl_emboss = CVAR_REGISTER( "gl_emboss", "1", FCVAR_ARCHIVE );
	gl_specular = CVAR_REGISTER( "gl_specular", "1", FCVAR_ARCHIVE );
	gl_bump = CVAR_REGISTER( "gl_bump", "1", FCVAR_ARCHIVE );
	cubeshot = CVAR_REGISTER( "cubeshot", "0", FCVAR_UNLOGGED );

	// cubemaps
	gEngfuncs.pfnAddCommand( "buildcubemaps", CL_BuildCubemaps_f );
	gl_cubemaps = CVAR_REGISTER( "gl_cubemaps", "0", FCVAR_ARCHIVE );

	ADD_COMMAND( "centerview", V_StartPitchDrift );

	ui_is_active = CVAR_REGISTER( "ui_is_active", "0", FCVAR_UNLOGGED );
	ui_backgroundmap_active = CVAR_REGISTER( "ui_backgroundmap_active", "0", FCVAR_UNLOGGED );
}

float V_CalcBob ( struct ref_params_s *pparams )
{
	static	double	bobtime;
	static float	bob;
	float	cycle;
	static float	lasttime;
	Vector	vel;
	

	if ( pparams->onground == -1 || pparams->time == lasttime )
	{
		// just use old value
		return bob;	
	}

	lasttime = pparams->time;

	bobtime += pparams->frametime;
	cycle = bobtime - (int)( bobtime / cl_bobcycle->value ) * cl_bobcycle->value;
	cycle /= cl_bobcycle->value;
	
	if ( cycle < cl_bobup->value )
		cycle = M_PI * cycle / cl_bobup->value;
	else
		cycle = M_PI + M_PI * ( cycle - cl_bobup->value )/( 1.0 - cl_bobup->value );

	// bob is proportional to simulated velocity in the xy plane
	// (don't count Z, or jumping messes it up)
	VectorCopy( pparams->simvel, vel );
	vel[2] = 0;

	bob = sqrt( vel[0] * vel[0] + vel[1] * vel[1] ) * cl_bob->value;
	bob = bob * 0.3 + bob * 0.7 * sin(cycle);
//	bob = min( bob, 4 );
//	bob = max( bob, -7 );
	return bob;
}

float V_CalcNewBob ( struct ref_params_s *pparams )  // diffusion hl2 bob
{
	static    float bobtime;
	static    float lastbobtime;
	float    cycle;
    
	Vector    vel;
	VectorCopy( pparams->simvel, vel );
	vel[2] = 0;
    
	if ( pparams->onground == -1 || pparams->time == lastbobtime )
		return 0.0f;
    
	float speed = sqrt(vel[0] * vel[0] + vel[1] * vel[1]);
    
	speed = bound( -320, speed, 320 );
    
	float bob_offset = RemapVal( speed, 0, 320, 0.0f, 1.0f );
    
	bobtime += ( pparams->time - lastbobtime ) * bob_offset;
	lastbobtime = pparams->time;
    
	// Calculate the vertical bob
	cycle = bobtime - (int)(bobtime/HL2_BOB_CYCLE_MAX)*HL2_BOB_CYCLE_MAX;
	cycle /= HL2_BOB_CYCLE_MAX;
    
	if ( cycle < HL2_BOB_UP )
		cycle = M_PI * cycle / HL2_BOB_UP;
	else
		cycle = M_PI + M_PI*(cycle-HL2_BOB_UP)/(1.0 - HL2_BOB_UP);
    
	g_verticalBob = speed*0.01f;

	if( gEngfuncs.GetLocalPlayer()->curstate.effects & EF_UPSIDEDOWN )
		g_verticalBob = g_verticalBob*0.3 - g_verticalBob*0.7*sin(cycle);
	else
		g_verticalBob = g_verticalBob * 0.3 + g_verticalBob * 0.7 * sin( cycle );
    
	g_verticalBob = bound( -15.0f, g_verticalBob, 4.0f );
    
	//Calculate the lateral bob
	cycle = bobtime - (int)(bobtime/HL2_BOB_CYCLE_MAX*2)*HL2_BOB_CYCLE_MAX*2;
	cycle /= HL2_BOB_CYCLE_MAX*2;
    
	if ( cycle < HL2_BOB_UP )
		cycle = M_PI * cycle / HL2_BOB_UP;
	else
		cycle = M_PI + M_PI*(cycle-HL2_BOB_UP)/(1.0 - HL2_BOB_UP);
    
	g_lateralBob = speed*0.004f;
	g_lateralBob = g_lateralBob*0.3 + g_lateralBob*0.7*sin(cycle);
    
	g_lateralBob = bound( -7.0f, g_lateralBob, 4.0f );
    
	//NOTENOTE: We don't use this return value in our case (need to restructure the calculation function setup!)
	return 0.0f;
}

extern cvar_t *cl_forwardspeed;

struct
{
	float	pitchvel;
	int  	nodrift;
	float	driftmove;
	double	laststop;
} pd;

//==========================
// V_StartPitchDrift
//==========================
void V_StartPitchDrift( void )
{
	if( pd.laststop == GET_CLIENT_TIME( ))
	{
		// something else is keeping it from drifting
		return;
	}

	if( pd.nodrift || !pd.pitchvel )
	{
		pd.pitchvel = v_centerspeed->value;
		pd.driftmove = 0;
		pd.nodrift = 0;
	}
}

//==========================
// V_StopPitchDrift
//==========================
void V_StopPitchDrift( void )
{
	pd.laststop = GET_CLIENT_TIME();
	pd.nodrift = 1;
	pd.pitchvel = 0;
}

/*
===============
V_DriftPitch

Moves the client pitch angle towards idealpitch sent by the server.

If the user is adjusting pitch manually, either with lookup/lookdown,
mlook and mouse, or klook and keyboard, pitch drifting is constantly stopped.
===============
*/
void V_DriftPitch( struct ref_params_s *pparams )
{
	if( gEngfuncs.IsNoClipping() || !pparams->onground || pparams->demoplayback )
	{
		pd.driftmove = 0;
		pd.pitchvel = 0;
		return;
	}

	if( pd.nodrift )
	{
		if( fabs( pparams->cmd->forwardmove ) < cl_forwardspeed->value )
		{
			pd.driftmove = 0;
		}
		else
		{
			pd.driftmove += pparams->frametime;
		}

		if( pd.driftmove > v_centermove->value )
			V_StartPitchDrift ();
		return;
	}
	
	float delta = pparams->idealpitch - pparams->cl_viewangles[PITCH];

	if( !delta )
	{
		pd.pitchvel = 0;
		return;
	}

	float move = pparams->frametime * pd.pitchvel;
	pd.pitchvel += pparams->frametime * v_centerspeed->value;
	
	if( delta > 0 )
	{
		if( move > delta )
		{
			pd.pitchvel = 0;
			move = delta;
		}

		pparams->cl_viewangles[PITCH] += move;
	}
	else if( delta < 0 )
	{
		if( move > -delta )
		{
			pd.pitchvel = 0;
			move = -delta;
		}

		pparams->cl_viewangles[PITCH] -= move;
	}
}

// diffusion - this is unfinished
void V_CalcSpectatorRefdef(struct ref_params_s* pparams)
{
	static vec3_t velocity(0.0f, 0.0f, 0.0f);

	static int lastWeaponModelIndex = 0;
	static int lastViewModelIndex = 0;

	cl_entity_t* ent = gEngfuncs.GetEntityByIndex(g_iUser2);

	pparams->onlyClientDraw = false;

	// refresh position
	VectorCopy(pparams->simorg, v_sim_org);

	// get old values
	VectorCopy(pparams->cl_viewangles, v_cl_angles);
	VectorCopy(pparams->viewangles, v_angles);
	VectorCopy(pparams->vieworg, v_origin);

	if ((g_iUser1 == OBS_IN_EYE /*|| gHUD.m_Spectator.m_pip->value == INSET_IN_EYE*/) && ent)
	{
		// calculate player velocity
		float timeDiff = ent->curstate.msg_time - ent->prevstate.msg_time;

		if (timeDiff > 0)
		{
			vec3_t distance;
			VectorSubtract(ent->prevstate.origin, ent->curstate.origin, distance);
			distance *= 1 / timeDiff;

			velocity[0] = velocity[0] * 0.9f + distance[0] * 0.1f;
			velocity[1] = velocity[1] * 0.9f + distance[1] * 0.1f;
			velocity[2] = velocity[2] * 0.9f + distance[2] * 0.1f;

			VectorCopy(velocity, pparams->simvel);
		}

		// predict missing client data and set weapon model ( in HLTV mode, inset in eye mode or in AG )
		if (gEngfuncs.IsSpectateOnly()) // FIXME this doesn't work! it never activates.
		{
			V_GetInEyePos(g_iUser2, pparams->simorg, pparams->cl_viewangles);

			pparams->health = 1;

			cl_entity_t* gunModel = gEngfuncs.GetViewModel();

			if (lastWeaponModelIndex != ent->curstate.weaponmodel)
			{
				// weapon model changed
				lastWeaponModelIndex = ent->curstate.weaponmodel;
				lastViewModelIndex = V_FindViewModelByWeaponModel(lastWeaponModelIndex);
				if (lastViewModelIndex)
					gEngfuncs.pfnWeaponAnim(0, 0);	// reset weapon animation
				else
				{
					// model not found
					gunModel->model = NULL;	// disable weapon model
					lastWeaponModelIndex = lastViewModelIndex = 0;
				}
			}

			if (lastViewModelIndex)
			{
				gunModel->model = IEngineStudio.GetModelByIndex(lastViewModelIndex);
				gunModel->curstate.modelindex = lastViewModelIndex;
				gunModel->curstate.frame = 0;
				gunModel->curstate.colormap = 0;
				gunModel->index = g_iUser2;
			}
			else
				gunModel->model = NULL;	// disable weapon model
		}
		else
		{
			// only get viewangles from entity
			VectorCopy(ent->angles, pparams->cl_viewangles);
			pparams->cl_viewangles[PITCH] *= -3.0f;	// see CL_ProcessEntityUpdate()
		}
	}

	v_frametime = pparams->frametime;

	if (pparams->nextView == 0)
	{
		// first renderer cycle, full screen

		switch (g_iUser1)
		{
		case OBS_CHASE_LOCKED:
			V_GetChasePos(g_iUser2, Vector(0,0,0), v_origin, v_angles);
			break;

		case OBS_CHASE_FREE:
			V_GetChasePos(g_iUser2, v_cl_angles, v_origin, v_angles);
			break;

		case OBS_ROAMING:
			VectorCopy(v_cl_angles, v_angles);
			VectorCopy(v_sim_org, v_origin);
			break;

		case OBS_IN_EYE:
			V_GetInEyePos(g_iUser2, v_origin, v_angles); //V_CalcFirstPersonRefdef(pparams);   // FIXME - it's supposed to be here. viewmodel doesn't work
			break;

		case OBS_MAP_FREE:
	//		pparams->onlyClientDraw = true;
	//		V_GetMapFreePosition(v_cl_angles, v_origin, v_angles);
			break;

		case OBS_MAP_CHASE:
	//		pparams->onlyClientDraw = true;
	//		V_GetMapChasePosition(g_iUser2, v_cl_angles, v_origin, v_angles);
			break;
		}

//		if (gHUD.m_Spectator.m_pip->value)
//			pparams->nextView = 1;	// force a second renderer view

//		gHUD.m_Spectator.m_iDrawCycle = 0;
	}
/*	else
	{
		// second renderer cycle, inset window

		// set inset parameters
		pparams->viewport[0] = XRES(gHUD.m_Spectator.m_OverviewData.insetWindowX);	// change viewport to inset window
		pparams->viewport[1] = YRES(gHUD.m_Spectator.m_OverviewData.insetWindowY);
		pparams->viewport[2] = XRES(gHUD.m_Spectator.m_OverviewData.insetWindowWidth);
		pparams->viewport[3] = YRES(gHUD.m_Spectator.m_OverviewData.insetWindowHeight);
		pparams->nextView = 0;	// on further view

		// override some settings in certain modes
		switch ((int)gHUD.m_Spectator.m_pip->value)
		{
		case INSET_CHASE_FREE: V_GetChasePos(g_iUser2, v_cl_angles, v_origin, v_angles);
			break;

		case INSET_IN_EYE:	V_CalcFirstPersonRefdef(pparams);
			break;

		case INSET_MAP_FREE:	pparams->onlyClientDraw = true;
//			V_GetMapFreePosition(v_cl_angles, v_origin, v_angles);
			break;

		case INSET_MAP_CHASE:	pparams->onlyClientDraw = true;

//			if (g_iUser1 == OBS_ROAMING)
//				V_GetMapChasePosition(0, v_cl_angles, v_origin, v_angles);
//			else
//				V_GetMapChasePosition(g_iUser2, v_cl_angles, v_origin, v_angles);

			break;
		}

		gHUD.m_Spectator.m_iDrawCycle = 1;
	}*/

	// write back new values into pparams
	VectorCopy(v_cl_angles, pparams->cl_viewangles);
	VectorCopy(v_angles, pparams->viewangles);
	VectorCopy(v_origin, pparams->vieworg);
}

int V_FindViewModelByWeaponModel(int weaponindex)
{
	// diffusion - FIXME need to check all the models and assignations...
	static char* modelmap[][2] = {
		{ "models/p_crossbow.mdl",		"models/v_crossbow.mdl"		},
		{ "models/p_crowbar.mdl",		"models/v_crowbar.mdl"		},
		{ "models/p_egon.mdl",			"models/v_egon.mdl"			},
		{ "models/p_gausniper.mdl",		"models/v_gausniper.mdl"	},
		{ "models/p_9mmhandgun.mdl",	"models/v_9mmhandgun.mdl"	},
		{ "models/p_grenade.mdl",		"models/v_grenade.mdl"		},
		{ "models/p_hgun.mdl",			"models/v_hgun.mdl"			},
		{ "models/p_9mmAR.mdl",			"models/v_9mmAR.mdl"		},
		{ "models/p_357.mdl",			"models/v_357.mdl"			},
		{ "models/p_rpg.mdl",			"models/v_rpg.mdl"			},
		{ "models/p_shotgun.mdl",		"models/v_shotgun.mdl"		},
		{ "models/p_squeak.mdl",		"models/v_squeak.mdl"		},
		{ "models/p_tripmine.mdl",		"models/v_tripmine.mdl"		},
		{ "models/p_satchel_radio.mdl",	"models/v_satchel_radio.mdl"},
		{ "models/p_satchel.mdl",		"models/v_satchel.mdl"		},
		{ NULL, NULL } };

	struct model_s* weaponModel = IEngineStudio.GetModelByIndex(weaponindex);

	if (weaponModel)
	{
		int len = strlen(weaponModel->name);
		int i = 0;

		while (modelmap[i] != NULL)
		{
			if (!_strnicmp(weaponModel->name, modelmap[i][0], len))
				return gEngfuncs.pEventAPI->EV_FindModelIndex(modelmap[i][1]);
			i++;
		}

		return 0;
	}
	else
		return 0;

}

void V_GetInEyePos(int target, Vector& origin, Vector& angles)
{
	if (!target)
	{
		// just copy a save in-map position
		VectorCopy(vJumpAngles, angles);
		VectorCopy(vJumpOrigin, origin);
		return;
	}

	cl_entity_t* ent = gEngfuncs.GetEntityByIndex(target);

	if (!ent)
		return;

	VectorCopy(ent->origin, origin);
	VectorCopy(ent->angles, angles);

	angles[PITCH] *= -3.0f;	// see CL_ProcessEntityUpdate()

	if (ent->curstate.solid == SOLID_NOT)
	{
		angles[ROLL] = 80;	// dead view angle
		origin[2] += -8; // PM_DEAD_VIEWHEIGHT
	}
	else if (ent->curstate.usehull == 1)
		origin[2] += 12; // VEC_DUCK_VIEW;
	else
		// exact eye position can't be caluculated since it depends on
		// client values like cl_bobcycle, this offset matches the default values
		origin[2] += 28; // DEFAULT_VIEWHEIGHT
}

void V_GetChasePos(int target, Vector& cl_angles, Vector& origin, Vector& angles)
{
	cl_entity_t* ent = NULL;

	if (target)
		ent = gEngfuncs.GetEntityByIndex(target);

	if (!ent)
	{
		// just copy a save in-map position
		VectorCopy(vJumpAngles, angles);
		VectorCopy(vJumpOrigin, origin);
		return;
	}

/*	if (gHUD.m_Spectator.m_autoDirector->value)
	{
		if (g_iUser3)
			V_GetDirectedChasePosition(ent, gEngfuncs.GetEntityByIndex(g_iUser3),
				angles, origin);
		else
			V_GetDirectedChasePosition(ent, (cl_entity_t*)0xFFFFFFFF, angles, origin);
	}
	else
	{*/
		if (cl_angles == g_vecZero)	// no mouse angles given, use entity angles ( locked mode )
		{
			angles = ent->angles;
			angles[0] *= -1;
		}
		else
			angles = cl_angles;

		origin = ent->origin;

		origin[2] += 28; // DEFAULT_VIEWHEIGHT - some offset

		V_GetChaseOrigin(angles, origin, cl_chasedist->value, origin);
//	}

	v_resetCamera = false;
}

void V_GetChaseOrigin(const Vector& angles, const Vector& origin, float distance, Vector& returnvec)
{
	Vector vecStart, vecEnd;
	pmtrace_t* trace;
	int maxLoops = 8;

	Vector forward, right, up;

	// trace back from the target using the player's view angles
	AngleVectors(angles, forward, right, up);
	forward = -forward;

	vecStart = origin;
	vecEnd = vecStart + forward * distance;

	int ignoreent = -1;	// first, ignore no entity
	cl_entity_t* ent = NULL;

	while (maxLoops > 0)
	{
		trace = gEngfuncs.PM_TraceLine(vecStart, vecEnd, PM_TRACELINE_PHYSENTSONLY, 2, ignoreent);
		if (trace->ent <= 0) break; // we hit the world or nothing, stop trace

		ent = GET_ENTITY(PM_GetPhysEntInfo(trace->ent));
		if (ent == NULL) break;

		// hit non-player solid BSP, stop here
		if (ent->curstate.solid == SOLID_BSP && !ent->player)
			break;

		// if close enought to end pos, stop, otherwise continue trace
		if ((vecEnd - trace->endpos).Length() < 1.0f)
		{
			break;
		}
		else
		{
			ignoreent = trace->ent;	// ignore last hit entity
			vecStart = trace->endpos;
		}
		maxLoops--;
	}

//	if( ent )
//		ConPrintf( "Trace loops %i , entity %i, model %s, solid %i\n", (8 - maxLoops), ent->curstate.number, ent->model->name, ent->curstate.solid );

//	returnvec = trace->endpos + trace->plane.normal * 8;
	VectorMA( trace->endpos, 4, trace->plane.normal, returnvec );
}

//==========================
// V_CalcGunAngle
//==========================
void V_CalcGunAngle( struct ref_params_s *pparams )
{	
	cl_entity_t *viewent;
	
	viewent = GET_VIEWMODEL();
	if( !viewent ) return;

	viewent->angles[YAW] = pparams->viewangles[YAW] + pparams->crosshairangle[YAW];
	viewent->angles[PITCH] = pparams->viewangles[PITCH] + pparams->crosshairangle[PITCH] * 0.25f;
	viewent->angles[ROLL] -= v_idlescale * sin( pparams->time * v_iroll_cycle.value ) * v_iroll_level.value;
	
	// don't apply all of the v_ipitch to prevent normally unseen parts of viewmodel from coming into view.
	viewent->angles[PITCH] -= v_idlescale * sin( pparams->time*v_ipitch_cycle.value ) * (v_ipitch_level.value * 0.5f);
	viewent->angles[YAW] -= v_idlescale * sin( pparams->time*v_iyaw_cycle.value ) * v_iyaw_level.value;

	viewent->angles[0] -= pparams->punchangle[0] / 2;
	viewent->angles[1] += pparams->punchangle[1] / 2;
	viewent->angles[2] += pparams->punchangle[2] / 2;

	viewent->latched.prevangles = viewent->angles;
	viewent->curstate.angles = viewent->angles;
}

//==========================
// V_CalcViewModelLag
//==========================
void V_CalcViewModelLag( ref_params_t *pparams, Vector &origin, Vector &angles, const Vector &original_angles )
{
	static Vector m_vecLastFacing;
	Vector vOriginalOrigin = origin;
	Vector vOriginalAngles = angles;

	// Calculate our drift
	Vector forward, right, up;

	AngleVectors( angles, forward, right, up );

	if( pparams->frametime != 0.0f )	// not in paused
	{
		Vector vDifference;

		vDifference = forward - m_vecLastFacing;

		float flSpeed = 3.0f;

		// If we start to lag too far behind, we'll increase the "catch up" speed.
		// Solves the problem with fast cl_yawspeed, m_yaw or joysticks rotating quickly.
		// The old code would slam lastfacing with origin causing the viewmodel to pop to a new position
		float flDiff = vDifference.Length();

		if(( flDiff > cl_weaponlag->value ) && ( cl_weaponlag->value > 0.0f ))
		{
			float flScale = flDiff / cl_weaponlag->value;
			flSpeed *= flScale;
		}

		// FIXME:  Needs to be predictable?
		m_vecLastFacing = m_vecLastFacing + vDifference * ( flSpeed * pparams->frametime );
		// Make sure it doesn't grow out of control!!!
		m_vecLastFacing = m_vecLastFacing.Normalize();
		origin = origin + (vDifference * -1.0f) * flSpeed * 0.25;  // diffusion *0.25
	}

	AngleVectors( original_angles, forward, right, up );

	float pitch = original_angles[PITCH];

	if( gEngfuncs.GetLocalPlayer()->curstate.effects & EF_UPSIDEDOWN )
	{
		if( pitch > 0.0f )
			pitch -= 360.0f;
		else if( pitch < -360.0f )
			pitch += 360.0f;
	}
	else
	{
		if( pitch > 180.0f )
			pitch -= 360.0f;
		else if( pitch < -180.0f )
			pitch += 360.0f;
	}

	if( cl_weaponlag->value <= 0.0f )
	{
		origin = vOriginalOrigin;
		angles = vOriginalAngles;
	}
	else
	{
		if( gEngfuncs.GetLocalPlayer()->curstate.effects & EF_UPSIDEDOWN )
		{
			origin = origin + forward * (-(pitch + 180) * 0.015f);  // diffusion - was 0.035f
			origin = origin + right * (-(pitch + 180) * 0.01f);    // was 0.03f
			origin = origin + up * (-(pitch + 180) * 0.02f);
		}
		else
		{
			origin = origin + forward * (-pitch * 0.015f);  // diffusion - was 0.035f
			origin = origin + right * (-pitch * 0.01f);    // was 0.03f
			origin = origin + up * (-pitch * 0.02f);
		}
	}

	// diffusion - addidle for weapon model
	origin[ROLL] += sin(pparams->time * 0.5) * 0.05;
	origin[PITCH] += sin(pparams->time) * 0.15;
	origin[YAW] += sin(pparams->time * 2) * 0.15;

	// diffusion - lower the weapon
	static float add = 0.0f;
	if( gHUD.WeaponLowered )
		add = CL_UTIL_Approach( 25.0f, add, 60 * g_fFrametime );
	else
		add = CL_UTIL_Approach( 0.0f, add, 60 * g_fFrametime );
	if( add > 0.0f )
	{
		angles.x += add;
		angles.y += add;
		origin += forward * add * 0.1f + right * add * 0.2f;
		origin.z += add * 0.05f;
	}
}

//==========================
// V_AddIdle
//==========================
void V_AddIdle( struct ref_params_s *pparams )
{
	pparams->viewangles[ROLL] += v_idlescale * sin(pparams->time*v_iroll_cycle.value) * v_iroll_level.value;
	pparams->viewangles[PITCH] += v_idlescale * sin(pparams->time*v_ipitch_cycle.value) * v_ipitch_level.value;
	pparams->viewangles[YAW] += v_idlescale * sin(pparams->time*v_iyaw_cycle.value) * v_iyaw_level.value;
}

//==========================
// V_CalcViewRoll
//==========================
void V_CalcViewRoll( struct ref_params_s *pparams )
{
	float   sign, side, value;
	Vector  right;
	
	cl_entity_t *viewentity = GET_ENTITY( pparams->viewentity );
	if( !viewentity ) return;

	AngleVectors( viewentity->angles, NULL, right, NULL );
	side = DotProduct( pparams->simvel, right );
	sign = side < 0 ? -1 : 1;
	side = fabs( side );
	value = pparams->movevars->rollangle;

	if( side < pparams->movevars->rollspeed )
		side = side * value / pparams->movevars->rollspeed;
	else side = value;

	side = side * sign;		
	pparams->viewangles[ROLL] += side;

	if( pparams->health <= 0 && ( pparams->viewheight[2] != 0 ))
	{
		// only roll the view if the player is dead and the viewheight[2] is nonzero 
		// this is so deadcam in multiplayer will work.
		pparams->viewangles[ROLL] = 80; // dead view angle
		return;
	}
}

//==========================
// V_CalcSendOrigin
//==========================
void V_CalcSendOrigin( struct ref_params_s *pparams )
{
	// never let view origin sit exactly on a node line, because a water plane can
	// dissapear when viewed with the eye exactly on it.
	// FIXME, we send origin at 1/128 now, change this?
	// the server protocol only specifies to 1/16 pixel, so add 1/32 in each axis
	pparams->vieworg[0] += 1.0f / 32;
	pparams->vieworg[1] += 1.0f / 32;
	pparams->vieworg[2] += 1.0f / 32;
}

// remaps an angular variable to a 3 band function:
// 0 <= t < start :		f(t) = 0
// start <= t <= end :	f(t) = end * spline(( t-start) / (end-start) )  // s curve between clamped and linear
// end < t :			f(t) = t
float RemapAngleRange( float startInterval, float endInterval, float value, RemapAngleRange_CurvePart_t *peCurvePart )
{
	// Fixup the roll
	value = AngleNormalize( value );
	float absAngle = fabs(value);

	// beneath cutoff?
	if( absAngle < startInterval )
	{
		if( peCurvePart )
		{
			*peCurvePart = RemapAngleRange_CurvePart_Zero;
		}
		value = 0;
	}
	// in spline range?
	else if( absAngle <= endInterval )
	{
		float newAngle = SimpleSpline(( absAngle - startInterval ) / ( endInterval-startInterval )) * endInterval;

		// grab the sign from the initial value
		if( value < 0 )
			newAngle *= -1;

		if( peCurvePart )
			*peCurvePart = RemapAngleRange_CurvePart_Spline;

		value = newAngle;
	}
	// else leave it alone, in linear range
	else if( peCurvePart )
		*peCurvePart = RemapAngleRange_CurvePart_Linear;

	return value;
}

float ApplyViewLocking( ref_params_t *params, float flAngleRaw, float flAngleClamped, ViewLockData_t &lockData, RemapAngleRange_CurvePart_t eCurvePart )
{
	// If we're set up to never lock this degree of freedom, return the clamped value.
	if( lockData.flLockInterval == 0 )
		return flAngleClamped;

	float flAngleOut = flAngleClamped;

	// Lock the view if we're in the linear part of the curve, and keep it locked
	// until some duration after we return to the flat (zero) part of the curve.
	if(( eCurvePart == RemapAngleRange_CurvePart_Linear ) || ( lockData.bLocked && ( eCurvePart == RemapAngleRange_CurvePart_Spline )))
	{
		lockData.bLocked = true;
		lockData.flUnlockTime = params->time + lockData.flLockInterval;
		flAngleOut = flAngleRaw;
	}
	else
	{
		if(( lockData.bLocked ) && ( params->time > lockData.flUnlockTime ))
		{
			lockData.bLocked = false;
			if ( lockData.flUnlockBlendInterval > 0 )
				lockData.flUnlockTime = params->time;
			else
				lockData.flUnlockTime = 0;
		}

		if ( !lockData.bLocked )
		{
			if( lockData.flUnlockTime != 0 )
			{
				// Blend out from the locked raw view (no remapping) to a remapped view.
				float flBlend = RemapValClamped( params->time-lockData.flUnlockTime, 0, lockData.flUnlockBlendInterval, 0, 1 );
				Msg( "BLEND %f\n", flBlend );

				flAngleOut = Lerp( flBlend, flAngleRaw, flAngleClamped );

				if( flBlend >= 1.0f )
					lockData.flUnlockTime = 0;
			}
			else
			{
				// Not blending out from a locked view to a remapped view.
				Msg( "CLAMPED\n" );
				flAngleOut = flAngleClamped;
			}
		}
		else
		{
			Msg( "STILL LOCKED\n" );
			flAngleOut = flAngleRaw;
		}
	}

	return flAngleOut;
}

//==========================
// V_CalcCameraRefdef
//==========================
void V_CalcCameraRefdef( struct ref_params_s *pparams )
{
	static float lasttime, oldz = 0;

	// get viewentity and monster eyeposition
	cl_entity_t *view = GET_ENTITY( pparams->viewentity );

 	if( view )
	{
		pparams->vieworg = view->origin;
		pparams->viewangles = view->angles;

		studiohdr_t *viewmonster = (studiohdr_t *)IEngineStudio.Mod_Extradata( view->model );

		if( viewmonster && view->curstate.eflags & EFLAG_SLERP )
		{
			Vector forward;
			AngleVectors( pparams->viewangles, forward, NULL, NULL );

			Vector viewpos = viewmonster->eyeposition;

			if( viewpos == g_vecZero )
				viewpos = Vector( 0, 0, 8 );	// monster_cockroach

			pparams->vieworg += viewpos + forward * 8;	// best value for humans
			// NOTE: fov computation moved into r_main.cpp
		}

		// this is smooth stair climbing in thirdperson mode but not affected for client model :(
		if( !pparams->smoothing && pparams->onground && view->origin[2] - oldz > 0.0f && viewmonster != NULL )
		{
			float steptime;
		
			steptime = pparams->time - lasttime;
			if( steptime < 0 ) steptime = 0;

			oldz += steptime * 150.0f;

			if( oldz > view->origin[2] )
				oldz = view->origin[2];
			if( view->origin[2] - oldz > pparams->movevars->stepsize )
				oldz = view->origin[2] - pparams->movevars->stepsize;

			pparams->vieworg[2] += oldz - view->origin[2];
		}
		else
		{
			oldz = view->origin[2];
		}

		lasttime = pparams->time;                 	

		if( view->curstate.effects & EF_NUKE_ROCKET )
			pparams->viewangles.x = -pparams->viewangles.x; // stupid quake bug!

		// g-cont. apply shake to camera
	//	gEngfuncs.V_CalcShake();
	//	gEngfuncs.V_ApplyShake( pparams->vieworg, pparams->viewangles, 1.0f );
		CalcShake();
		ApplyShake( pparams->vieworg, pparams->viewangles, 1.0f );
	}
}

#define MAX_SPOTS	16

//==========================
// V_FindIntermissionSpot
//==========================
cl_entity_t *V_FindIntermissionSpot( struct ref_params_s *pparams )
{
	int spotindex[MAX_SPOTS];
	cl_entity_t *ent;
	int i, j;

	// found intermission point
	for( i = 0, j = 0; i < pparams->max_entities; i++ )
	{
		ent = GET_ENTITY( i );

		if( ent && ent->curstate.eflags & EFLAG_INTERMISSION )
		{
			spotindex[j] = ent->index;
			if( ++j >= MAX_SPOTS ) break; // full
		}
	}	
	
	// ok, we have list of intermission spots
	if( j != 0 )
	{
		if( j > 1 ) j = RANDOM_LONG( 0, j );
		ent = GET_ENTITY( spotindex[j-1] );
	}
	else
	{
		// defaulted to player view
		ent = gEngfuncs.GetLocalPlayer();
	}	

	return ent;
}

//==========================
// V_CalcIntermissionRefdef
//==========================
void V_CalcIntermissionRefdef( struct ref_params_s *pparams )
{
	// old code
/*	if (!v_intermission_spot)
		v_intermission_spot = V_FindIntermissionSpot( pparams );

	pparams->vieworg = v_intermission_spot->origin;
	pparams->viewangles = v_intermission_spot->angles;

	cl_entity_t *view = GET_VIEWMODEL();	
	view->model = NULL;

	// allways idle in intermission
	float old = v_idlescale;
	v_idlescale = 1;
	V_AddIdle( pparams );
	v_idlescale = old;
*/
	cl_entity_t* ent, * view;
	float		old;

	// ent is the player model ( visible when out of body )
	ent = gEngfuncs.GetLocalPlayer();

	// view is the weapon model (only visible from inside body )
	view = gEngfuncs.GetViewModel();

	VectorCopy(pparams->simorg, pparams->vieworg);
	VectorCopy(pparams->cl_viewangles, pparams->viewangles);

	view->model = NULL;

	// always idle in intermission
	old = v_idlescale;
	v_idlescale = 1;

	V_AddIdle(pparams);

//	if (gEngfuncs.IsSpectateOnly())
//	{
//		// in HLTV we must go to 'intermission' position by ourself
//		VectorCopy(gHUD.m_Spectator.m_cameraOrigin, pparams->vieworg);
//		VectorCopy(gHUD.m_Spectator.m_cameraAngles, pparams->viewangles);
//	}

	v_idlescale = old;

	v_cl_angles = pparams->cl_viewangles;
	v_origin = pparams->vieworg;
	v_angles = pparams->viewangles;
}

//==========================
// V_CalcThirdPersonRefdef
//==========================
void V_CalcThirdPersonRefdef( struct ref_params_s * pparams )
{
	static float lasttime, oldz = 0;

	pparams->vieworg = pparams->simorg;
	pparams->vieworg += pparams->viewheight;
	pparams->viewangles = pparams->cl_viewangles;

	V_CalcSendOrigin( pparams );

	// this is smooth stair climbing in thirdperson mode but not affected for client model :(
	if( !pparams->smoothing && pparams->onground && pparams->simorg[2] - oldz > 0.0f )
	{
		float steptime;
		
		steptime = pparams->time - lasttime;
		if( steptime < 0 ) steptime = 0;

		oldz += steptime * 150.0f;

		if( oldz > pparams->simorg[2] )
			oldz = pparams->simorg[2];
		if( pparams->simorg[2] - oldz > pparams->movevars->stepsize )
			oldz = pparams->simorg[2] - pparams->movevars->stepsize;

		pparams->vieworg[2] += oldz - pparams->simorg[2];
	}
	else
		oldz = pparams->simorg[2];

	lasttime = pparams->time;

	V_GetChaseOrigin( pparams->viewangles, pparams->vieworg, cl_chasedist->value, pparams->vieworg );

	float pitch = pparams->viewangles[PITCH];

	// normalize angles
	if( pitch > 180.0f ) 
		pitch -= 360.0f;
	else if( pitch < -180.0f )
		pitch += 360.0f;

	// player pitch is inverted
	pitch /= -3.0;

	cl_entity_t *ent = GET_LOCAL_PLAYER();

	// slam local player's pitch value
	ent->angles[PITCH] = pitch;
	ent->curstate.angles[PITCH] = pitch;
	ent->prevstate.angles[PITCH] = pitch;
	ent->latched.prevangles[PITCH] = pitch;
}

//==========================
// V_CalcWaterLevel
//==========================
float V_CalcWaterLevel( struct ref_params_s *pparams )
{
	float waterOffset = 0.0f;

	if( pparams->waterlevel >= 2 )
	{
		int waterEntity = WATER_ENTITY( pparams->simorg );
		float waterDist = cl_waterdist->value;

		if( waterEntity >= 0 && waterEntity < pparams->max_entities )
		{
			cl_entity_t *pwater = GET_ENTITY( waterEntity );
			if( pwater && ( pwater->model != NULL ))
				waterDist += ( pwater->curstate.scale * 16.0f );
		}

		Vector point = pparams->vieworg;

		// eyes are above water, make sure we're above the waves
		if( pparams->waterlevel == 2 )	
		{
			point.z -= waterDist;

			for( int i = 0; i < waterDist; i++ )
			{
				int contents = POINT_CONTENTS( point );
				if( contents > CONTENTS_WATER )
					break;
				point.z += 1;
			}
			waterOffset = (point.z + waterDist) - pparams->vieworg[2];
		}
		else
		{
			// eyes are under water. Make sure we're far enough under
			point[2] += waterDist;

			for( int i = 0; i < waterDist; i++ )
			{
				int contents = POINT_CONTENTS( point );
				if( contents <= CONTENTS_WATER )
					break;

				point.z -= 1;
			}
			waterOffset = (point.z - waterDist) - pparams->vieworg[2];
		}
	}

	return waterOffset;
}

#define ORIGIN_BACKUP	64
#define ORIGIN_MASK		( ORIGIN_BACKUP - 1 )

typedef struct 
{
	Vector	Origins[ORIGIN_BACKUP];
	float	OriginTime[ORIGIN_BACKUP];

	Vector	Angles[ORIGIN_BACKUP];
	float	AngleTime[ORIGIN_BACKUP];

	int	CurrentOrigin;
	int	CurrentAngle;
} viewinterp_t;

// diffusion - I tried to get this to work for the whole month (still learning)
// - thanks to g-cont for a bit of help
// - main function is from Quake 3
//=====================================================================
// V_LandDip: move the player's view down and up after landing.
//=====================================================================
static void V_LandDip( struct ref_params_s *pparams)
{
	#define LAND_DEFLECT_TIME 0.15
	#define LAND_RETURN_TIME 0.3
	float delta;
	static float landtime;
	static float f;
	float landChange;

	if( !pparams->onground && pparams->health > 0 )
	{
		if( gEngfuncs.GetLocalPlayer()->curstate.effects & EF_UPSIDEDOWN )
		{
			if( pparams->simvel[2] > 200 )
				landtime = pparams->time;
		}
		else
		{
			if( pparams->simvel[2] < -200 )
				landtime = pparams->time;
		}
	}

	if (pparams->onground)
	{
		if( landtime > pparams->time )
		{
			landtime = 0;
			return;
		}
		
		if( gEngfuncs.GetLocalPlayer()->curstate.effects & EF_UPSIDEDOWN )
			landChange = 4;
		else
			landChange = -4;

		delta = pparams->time - landtime;

		if (delta > (LAND_DEFLECT_TIME + LAND_RETURN_TIME) )
		{
			landtime = 0;
			delta = 0;
			return;
		}

		if (delta < LAND_DEFLECT_TIME)
		{
			f = delta / LAND_DEFLECT_TIME;
			pparams->simorg[2] += landChange * f;
		}
		else if (delta < LAND_DEFLECT_TIME + LAND_RETURN_TIME)
		{
			delta -= LAND_DEFLECT_TIME;
			f = 1.0 - ( delta / LAND_RETURN_TIME );
			pparams->simorg[2] += landChange * f;
		}
	}
}


// diffusion - land dip from CS:GO
/*
void V_LandDip( struct ref_params_s *pparams)
{
	static float landtime;

	if( !pparams->onground )
	{
		landtime = pparams->time;
	}
	else
	{
		float landseconds = max(pparams->time - landtime, 0.0f);
		float landFraction = SimpleSpline( landseconds / 0.25f );
		clamp( landFraction, 0.0f, 1.0f );
		float flDipAmount = (1 / 300) * 0.1f;  //200 = flOldFallVel
		int dipHighOffset = 64;
		int dipLowOffset = dipHighOffset - 4;
		Vector temp = pparams->simorg;
		temp.z = ( ( dipLowOffset - flDipAmount ) * landFraction ) +
				( dipHighOffset * ( 1 - landFraction ) );

			if ( temp.z > dipHighOffset )
			{
				temp.z = dipHighOffset;
			}
			pparams->simorg[2] -= ( dipHighOffset - temp.z );
	}
}
*/

// diffusion - that's super lame, sure there's an easier way for that
// it's also using different landchange value, otherwise it's the same
//=====================================================================
// V_LandDipWeapon: move the player's weapon when he lands.
//=====================================================================
static void V_LandDipWeapon( struct ref_params_s *pparams)
{
	#define WEP_DEFLECT_TIME 0.1
	#define WEP_RETURN_TIME 0.25
	float delta;
	static float weptime;
	float f;
	float wepChange;

	if( !pparams->onground &&
		(
			(!( gEngfuncs.GetLocalPlayer()->curstate.effects & EF_UPSIDEDOWN ) && (pparams->simvel[2] < -200)) ||
			((gEngfuncs.GetLocalPlayer()->curstate.effects & EF_UPSIDEDOWN) && (pparams->simvel[2] > 200))
		) 	
	)
		weptime = pparams->time;
	else
	{
		if( gEngfuncs.GetLocalPlayer()->curstate.effects & EF_UPSIDEDOWN )
			wepChange = -0.4;
		else
			wepChange = 0.4;

		delta = pparams->time - weptime;

		if (delta < 0)
			return;

		if (delta > (WEP_DEFLECT_TIME + WEP_RETURN_TIME) )
		{
			weptime = 0;
			delta = 0;
			return;
		}

		if ( delta < WEP_DEFLECT_TIME)
		{
			f = delta / WEP_DEFLECT_TIME;
			pparams->simorg[2] += wepChange * f;
		}
		else if ( delta < WEP_DEFLECT_TIME + WEP_RETURN_TIME )
		{
			delta -= WEP_DEFLECT_TIME;
			f = 1.0 - ( delta / WEP_RETURN_TIME );
			pparams->simorg[2] += wepChange * f;
		}
	}

}

//==========================
// V_InterpolatePos
//==========================
void V_InterpolatePos( struct ref_params_s *pparams )
{
	static Vector lastorg;
	static viewinterp_t	ViewInterp;

	// view is the weapon model (only visible from inside body )
	cl_entity_t *view = GET_VIEWMODEL();

	Vector delta = pparams->simorg - lastorg;

	if( delta.Length() != 0.0f )
	{
		ViewInterp.Origins[ViewInterp.CurrentOrigin & ORIGIN_MASK] = pparams->simorg;
		ViewInterp.OriginTime[ViewInterp.CurrentOrigin & ORIGIN_MASK] = pparams->time;
		ViewInterp.CurrentOrigin++;

		lastorg = pparams->simorg;
	}

	if( cl_vsmoothing->value && pparams->smoothing && ( pparams->maxclients > 1 ))
	{
		int i, foundidx;
		float t;

		if( cl_vsmoothing->value < 0.0f )
			CVAR_SET_FLOAT( "cl_vsmoothing", 0.0 );

		t = pparams->time - cl_vsmoothing->value;

		for( i = 1; i < ORIGIN_MASK; i++ )
		{
			foundidx = ViewInterp.CurrentOrigin - 1 - i;

			if( ViewInterp.OriginTime[foundidx & ORIGIN_MASK] <= t )
				break;
		}

		if( i < ORIGIN_MASK &&  ViewInterp.OriginTime[foundidx & ORIGIN_MASK] != 0.0f )
		{
			// Interpolate
			Vector delta, neworg;
			double dt, frac;

			dt = ViewInterp.OriginTime[(foundidx + 1) & ORIGIN_MASK] - ViewInterp.OriginTime[foundidx & ORIGIN_MASK];
			if( dt > 0.0f )
			{
				frac = ( t - ViewInterp.OriginTime[foundidx & ORIGIN_MASK] ) / dt;
				delta = ViewInterp.Origins[( foundidx + 1 ) & ORIGIN_MASK] - ViewInterp.Origins[foundidx & ORIGIN_MASK];
				frac = min( 1.0, frac );

				neworg = ViewInterp.Origins[foundidx & ORIGIN_MASK] + delta * frac;

				// dont interpolate large changes (less than 64 units)
				if( delta.Length() < 64 )
				{
					delta = neworg - pparams->simorg;
					pparams->simorg += delta;
					pparams->vieworg += delta;
					view->origin += delta;
				}
			}
		}
	}
}

//===============================================================================
// diffusion - test: local animations for weapons (right now shooting only...XD)
// This must be always re-checked if you do any changes in weapons, otherwise don't use it
// ...or just write proper weapon prediction :>
// SEE ALSO: CheckForLocalWeaponShootAnimation in hud_msg.cpp
//===============================================================================
void V_LocalWeaponAnimations( struct ref_params_s *pparams )
{
	if( !LocalWeaponAnims() )
		return;

	if( !RP_NORMALPASS() )
		return;

	if( CL_IsThirdPerson() )
		return;

	// weapons are hidden
	if( gHUD.m_iHideHUDDisplay & (HIDEHUD_WPNS | HIDEHUD_ALL | HIDEHUD_WPNS_HOLDABLEITEM | HIDEHUD_WPNS_CUSTOM) )
		return;

	cl_entity_t *player = gEngfuncs.GetLocalPlayer();

	bool Attack1 = (gHUD.m_iKeyBits & IN_ATTACK) ? true : false;
	bool Attack2 = (gHUD.m_iKeyBits & IN_ATTACK2) ? true : false;
	int CurrentPAmmoClip = 0;
	int CurrentSAmmoClip = 0;
	int MaxSAmmoCapacity = 0;
	bool Underwater = (pparams->waterlevel == 3);
	if( gHUD.m_Ammo.m_pWeapon )
	{
		CurrentPAmmoClip = gHUD.m_Ammo.m_pWeapon->iClip;
		CurrentSAmmoClip = gWR.CountAmmo( gHUD.m_Ammo.m_pWeapon->iAmmo2Type );
		MaxSAmmoCapacity = gHUD.m_Ammo.m_pWeapon->iMax2; // check if weapon has secondary ammo at all
	}

	if( !gHUD.CanShoot )
	{
		Attack1 = false;
		Attack2 = false;
	}

	if( Attack1 && (tr.time < localanim_NextPAttackTime) )
		return;

	if( Attack1 && CurrentPAmmoClip == 0 )
		return;

	if( Attack2 && (tr.time < localanim_NextSAttackTime) )
		return;

	if( Attack2 && CurrentSAmmoClip == 0 && MaxSAmmoCapacity != -1 )
		return;
	
	switch( localanim_WeaponID )
	{
	case WEAPON_HKMP5:
		if( Attack1 && !Underwater )
		{
			gEngfuncs.pfnWeaponAnim( RANDOM_LONG( 4, 6 ), 0 );
			switch( RANDOM_LONG( 0, 2 ) )
			{
			case 0: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/mp5-1.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			case 1: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/mp5-2.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			case 2: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/mp5-3.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			}
			localanim_NextPAttackTime = tr.time + 0.1;
			R_MakeWeaponShake( WEAPON_HKMP5, 0, true );
			PlayLowAmmoSound( player->index, player->origin, (CurrentPAmmoClip <= 10 ? CurrentPAmmoClip : 0) );
		}
	break;
	case WEAPON_MRC:
		if( Attack1 && !Underwater )
		{
			gEngfuncs.pfnWeaponAnim( RANDOM_LONG( 5, 7 ), 0 );
			switch( RANDOM_LONG( 0, 2 ) )
			{
			case 0: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/hks1.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			case 1: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/hks2.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			case 2: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/hks3.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			}
			localanim_NextPAttackTime = tr.time + 0.1;
			R_MakeWeaponShake( WEAPON_MRC, 0, true );
			PlayLowAmmoSound( player->index, player->origin, (CurrentPAmmoClip <= 15 ? CurrentPAmmoClip : 0) );
		}
		if( Attack2 && !Underwater )
		{
			gEngfuncs.pfnWeaponAnim( 2, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/glauncher.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 1.0;
			R_MakeWeaponShake( WEAPON_MRC, 1, true );
		}
	break;
	case WEAPON_AR2:
		if( Attack1 && !Underwater )
		{
			gEngfuncs.pfnWeaponAnim( 3, 0 );
			switch( RANDOM_LONG( 0, 3 ) )
			{
			case 0: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "drone/aliendrone_shoot1.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			case 1: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "drone/aliendrone_shoot2.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			case 2: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "drone/aliendrone_shoot3.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			case 3: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "drone/aliendrone_shoot4.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			}
			localanim_NextPAttackTime = tr.time + 0.1;
			R_MakeWeaponShake( WEAPON_AR2, 0, true );
		}
		if( Attack2 && !Underwater )
		{
			gEngfuncs.pfnWeaponAnim( 1, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/ar2_secondary.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 1.0;
			R_MakeWeaponShake( WEAPON_AR2, 1, true );
		}
	break;
	case WEAPON_GLOCK:
		if( Attack1 || Attack2 )
		{
			gEngfuncs.pfnWeaponAnim( 3, 0 );
			switch( RANDOM_LONG( 0, 2 ) )
			{
			case 0: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/pistol1.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			case 1: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/pistol2.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			case 2: gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/pistol3.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) ); break;
			}
			if( Attack1 )
				localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 0.3;
			else
				localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 0.2;
		}
	break;
	case WEAPON_CROSSBOW:
		if( Attack1 )
		{
			if( CurrentPAmmoClip == 1 ) // last bolt
				gEngfuncs.pfnWeaponAnim( 6, 0 );
			else
				gEngfuncs.pfnWeaponAnim( 4, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/xbow_fire1.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_NextPAttackTime = tr.time + 0.75;
		}
	break;
	case WEAPON_DEAGLE:
		if( Attack1 && !Underwater )
		{
			gEngfuncs.pfnWeaponAnim( 2, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/deagle_shot1.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_NextPAttackTime = tr.time + 0.75;
			R_MakeWeaponShake( WEAPON_DEAGLE, 0, true );
		}
	break;
	case WEAPON_FIVESEVEN:
		if( Attack1 && !Underwater )
		{
			if( CurrentPAmmoClip == 1 ) // last bullet
				gEngfuncs.pfnWeaponAnim( 3, 0 );
			else
				gEngfuncs.pfnWeaponAnim( 1, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/fiveseven_shoot.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_NextPAttackTime = tr.time + 0.2;
		}
	break;
	case WEAPON_G36C:
		if( Attack1 && !Underwater )
		{
			gEngfuncs.pfnWeaponAnim( 3, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/g36c_shoot.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_NextPAttackTime = tr.time + 0.1;
			R_MakeWeaponShake( WEAPON_G36C, 0, true );
			PlayLowAmmoSound( player->index, player->origin, (CurrentPAmmoClip <= 10 ? CurrentPAmmoClip : 0) );
		}
	break;
	case WEAPON_KNIFE:
		if( Attack1 )
		{
			gEngfuncs.pfnWeaponAnim( 3, 0 );
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 0.5;
		}
		if( Attack2 )
		{
			gEngfuncs.pfnWeaponAnim( 6, 0 );
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 1.25;
		}
	break;
	case WEAPON_RPG:
		if( Attack1 && localanim_AllowRpgShoot )
		{
			gEngfuncs.pfnWeaponAnim( 3, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/rocketfire1.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_AllowRpgShoot = false; // do not allow any more animations, until we catch reload or deploy
		}
	break;
	case WEAPON_SHOTGUN:
		if( Attack1 && !Underwater )
		{
			gEngfuncs.pfnWeaponAnim( 1, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/shotgun_single.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 1.2;
			R_MakeWeaponShake( WEAPON_SHOTGUN, 0, true );
		}
		if( Attack2 && !Underwater )
		{
			if( CurrentPAmmoClip > 1 )
			{
				gEngfuncs.pfnWeaponAnim( 2, 0 );
				gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/shotgun_dbl.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
				R_MakeWeaponShake( WEAPON_SHOTGUN, 1, true );
			}
			else
			{
				gEngfuncs.pfnWeaponAnim( 1, 0 );
				gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/shotgun_single.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
				R_MakeWeaponShake( WEAPON_SHOTGUN, 0, true );
			}
			localanim_NextPAttackTime = localanim_NextSAttackTime = tr.time + 1.6;
		}
	break;
	case WEAPON_SHOTGUN_XM:
		if( Attack1 && !Underwater )
		{
			gEngfuncs.pfnWeaponAnim( 1, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/xm_fire.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_NextPAttackTime = tr.time + 0.5;
			R_MakeWeaponShake( WEAPON_SHOTGUN_XM, 0, true );
		}
	break;
	case WEAPON_SNIPER:
		if( Attack1 && !Underwater )
		{
			gEngfuncs.pfnWeaponAnim( 2, 0 );
			gEngfuncs.pEventAPI->EV_PlaySound( player->index, player->origin, CHAN_WEAPON, "weapons/sniper_wpn_fire.wav", VOL_NORM, 0.6, 0, RANDOM_LONG( 98, 103 ) );
			localanim_NextPAttackTime = tr.time + 1.0;
			R_MakeWeaponShake( WEAPON_SNIPER, 0, true );
		}
	break;
	}
}

//==========================
// V_CalcFirstPersonRefdef
//==========================
void V_CalcFirstPersonRefdef( struct ref_params_s *pparams )
{
	cl_entity_t* view;
	cl_entity_t* plr;
	Vector	angles;
	static float lasttime;
	static bool IsUpsideDown;
	
	V_DriftPitch( pparams );
	
	if (gEngfuncs.IsSpectateOnly())
		plr = gEngfuncs.GetEntityByIndex(g_iUser2);
	else
		plr = gEngfuncs.GetLocalPlayer(); // ent is the player model ( visible when out of body )

	if( plr->curstate.effects & EF_UPSIDEDOWN )
	{
		if( !IsUpsideDown )
		{
			pparams->cl_viewangles[PITCH] = -180;
			pparams->cl_viewangles[YAW] += 180;
			IsUpsideDown = true;
		}
	}
	else
	{
		if( IsUpsideDown )
		{
			pparams->cl_viewangles[PITCH] = 0;
			pparams->cl_viewangles[YAW] += 180;
			IsUpsideDown = false;
		}
	}
	
	view = GET_VIEWMODEL();

	V_LocalWeaponAnimations( pparams );
		
	//==========================================

	float bob = V_CalcBob( pparams );
	pparams->simorg.z += bob;
	
	V_LandDip( pparams );

	pparams->vieworg = pparams->simorg;
	pparams->vieworg += pparams->viewheight;
//	pparams->vieworg.z += bob;

	pparams->viewangles = pparams->cl_viewangles;

//	gEngfuncs.V_CalcShake();
//	gEngfuncs.V_ApplyShake( pparams->vieworg, pparams->viewangles, 1.0f );
	CalcShake();
	ApplyShake( pparams->vieworg, pparams->viewangles, 1.0f );

	//================================================
	// I'm drunk
	//================================================
	float oldidlescale = v_idlescale;
	if( gHUD.DrunkLevel > 0 )
	{
		v_idlescale = gHUD.DrunkLevel;
		V_AddIdle( pparams );
	}
	v_idlescale = oldidlescale;

	//================================================
	// screen wiggling - standing on a windy cliff :)
	//================================================
	static float wiggling_mult = 0;

	if( gHUD.WigglingEffect )
	{
		if( wiggling_mult < 1 )
			wiggling_mult += 0.35 * g_fFrametime;		
	}
	else
	{
		if( wiggling_mult > 0 )
			wiggling_mult -= 0.5 * g_fFrametime;
	}

	wiggling_mult = bound( 0, wiggling_mult, 1 );
		
	if( wiggling_mult > 0 )
	{
		static float angle = 0;
		static float tmtm = 0;
		if( tmtm > tr.time + 1 )
			tmtm = tr.time;
		static float Roll = 0;
		if( tr.time > tmtm )
		{
			angle = RANDOM_FLOAT( -5, 5 ); // amplitude
			tmtm = tr.time + 0.75;
		}

		if( tr.time != tr.oldtime )
			Roll = CL_UTIL_Approach( angle, Roll, g_fFrametime );
		pparams->viewangles[ROLL] = Roll * wiggling_mult;
	}
	else
	{
#if 0
		// turning roll like in first Mirror's Edge
		float AddRoll = bound( -5.0f, gHUD.MxMy.x, 5.0f );
		if( !CVAR_TO_BOOL( ui_is_active ) && (tr.time != tr.oldtime) )
			pparams->viewangles[ROLL] -= AddRoll;
#endif
	}
	//=================================================

	V_CalcSendOrigin( pparams );

	float waterOffset = V_CalcWaterLevel( pparams );
	pparams->vieworg[2] += waterOffset;
	
	V_CalcViewRoll( pparams );
	v_idlescale = 0;
	V_AddIdle( pparams );

	float WaterEffect = gEngfuncs.GetLocalPlayer()->curstate.vuser1.x;
	if( WaterEffect > 0 )
	{
		pparams->viewangles[ROLL] += sin( pparams->time * 0.5 ) * 0.05 * WaterEffect * 0.5;
		pparams->viewangles[PITCH] += sin( pparams->time ) * 0.15 * WaterEffect * 0.5;
		pparams->viewangles[YAW] += sin( pparams->time * 2 ) * 0.15 * WaterEffect * 0.5;
	}

	V_LandDipWeapon( pparams );

	// offsets
	AngleVectors( pparams->cl_viewangles, pparams->forward, pparams->right, pparams->up );

	Vector lastAngles = view->angles = pparams->cl_viewangles;

	V_CalcGunAngle( pparams );
	
	// use predicted origin as view origin.
	view->origin = pparams->simorg;      
	view->origin += pparams->viewheight;
	view->origin.z += waterOffset;
	
	// Let the viewmodel shake at about 10% of the amplitude
//	gEngfuncs.V_ApplyShake( view->origin, view->angles, 0.9f );
	ApplyShake( view->origin, view->angles, 0.9f );
/*
	view->origin += pparams->forward * bob * 0.4f;
	view->origin.z += bob;

	view->angles[PITCH] -= bob * 0.3f;
	view->angles[YAW] -= bob * 0.5f;
	view->angles[ROLL] -= bob * 1.0f;
	view->origin.z -= 1;
*/

//---------diffusion hl2 bob start
	Vector    forward, right;

	AngleVectors( view->angles, forward, right, NULL );

	V_CalcNewBob ( pparams );
	
	// Apply bob, but scaled down to 40%
	if( gEngfuncs.GetLocalPlayer()->curstate.effects & EF_UPSIDEDOWN )
	{
		view->origin[0] = view->origin[0] - g_verticalBob * 0.1f * forward[0];
		view->origin[1] = view->origin[1] - g_verticalBob * 0.1f * forward[1];
		view->origin[2] = view->origin[2] - g_verticalBob * 0.1f * forward[2];
		// Z bob a bit more
		view->origin[2] -= g_verticalBob * 0.1f;
		// bob the angles
		angles[ROLL] -= g_verticalBob * 0.3f;
		angles[PITCH] += g_verticalBob * 0.8f;
		angles[YAW] += g_lateralBob * 0.5f;
		view->origin[0] = view->origin[0] - g_lateralBob * 0.8f * right[0];
		view->origin[1] = view->origin[1] - g_lateralBob * 0.8f * right[1];
		view->origin[2] = (view->origin[2] - g_lateralBob * 0.8f * right[2]) + 1;  // weapon height lower a bit
	}
	else
	{
		view->origin[0] = view->origin[0] + g_verticalBob * 0.1f * forward[0];
		view->origin[1] = view->origin[1] + g_verticalBob * 0.1f * forward[1];
		view->origin[2] = view->origin[2] + g_verticalBob * 0.1f * forward[2];
		// Z bob a bit more
		view->origin[2] += g_verticalBob * 0.1f;
		// bob the angles
		angles[ROLL] += g_verticalBob * 0.3f;
		angles[PITCH] -= g_verticalBob * 0.8f;
		angles[YAW] -= g_lateralBob * 0.5f;
		view->origin[0] = view->origin[0] + g_lateralBob * 0.8f * right[0];
		view->origin[1] = view->origin[1] + g_lateralBob * 0.8f * right[1];
		view->origin[2] = (view->origin[2] + g_lateralBob * 0.8f * right[2]) - 1;  // weapon height lower a bit
	}
	
//----------hl2 bob end
	
	// diffusion - lower the gun when walking, just like CS:GO :)
//	float LowerGunAmount = pparams->simvel.Length2D() * 0.003;
//	LowerGunAmount = bound(0, LowerGunAmount, 1);
//	view->origin[2] -= LowerGunAmount;
	// another solution:
	if( gHUD.m_iKeyBits & (IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT) )
	{
		if( pparams->simvel.Length2D() > 0 && (GunPosZCurrent < 1 ) )
			GunPosZCurrent += pparams->simvel.Length2D() * 0.015 * g_fFrametime;
	}
	else
	{
		if( pparams->simvel.Length2D() <= 150 && (GunPosZCurrent > 0 ) )
			GunPosZCurrent -= 3 * g_fFrametime;
	}
	if( gEngfuncs.GetLocalPlayer()->curstate.effects & EF_UPSIDEDOWN )
		view->origin.z += GunPosZCurrent;
	else
		view->origin.z -= GunPosZCurrent;
	
	// for zoom! change the model visibility
	if( gHUD.IsZoomed )
	{
		GET_VIEWMODEL()->curstate.rendermode = kRenderTransTexture;
		GET_VIEWMODEL()->curstate.renderamt = 0;
	}

	// and move the gun in the direction opposite to movement
	float VelX = -pparams->simvel[0] * 0.004;
	VelX = bound(0,VelX,1);
	view->origin.x += VelX;
	float VelY = -pparams->simvel[1] * 0.004;
	VelY = bound(0,VelY,1);
	view->origin.y += VelY;
	
	// fudge position around to keep amount of weapon visible
	// roughly equal with different FOV
	if( pparams->viewsize == 110 )
		view->origin[2] += 1;
	else if( pparams->viewsize == 100 )
		view->origin[2] += 2;
	else if( pparams->viewsize == 90 )
		view->origin[2] += 1;
	else if( pparams->viewsize == 80 )
		view->origin[2] += 0.5;

	V_CalcViewModelLag( pparams, view->origin, view->angles, lastAngles );
	
	pparams->viewangles += pparams->punchangle;

	static float oldz = 0;

	if( !pparams->smoothing && pparams->onground && pparams->simorg[2] - oldz > 0.0f )
	{
		float steptime;
		
		steptime = pparams->time - lasttime;
		if( steptime < 0 ) steptime = 0;

		oldz += steptime * 150.0f;

		if( oldz > pparams->simorg[2] )
			oldz = pparams->simorg[2];
		if( pparams->simorg[2] - oldz > pparams->movevars->stepsize )
			oldz = pparams->simorg[2] - pparams->movevars->stepsize;

		pparams->vieworg[2] += oldz - pparams->simorg[2];
		view->origin.z += oldz - pparams->simorg[2];
	}
	else
		oldz = pparams->simorg[2];

	lasttime = pparams->time;

	// smooth player view in multiplayer
	V_InterpolatePos( pparams );

	if (pparams->viewentity > pparams->maxclients)
	{
		cl_entity_t* viewentity;
		viewentity = gEngfuncs.GetEntityByIndex(pparams->viewentity);
		
		if (viewentity)
		{
			VectorCopy(viewentity->origin, pparams->vieworg);
			VectorCopy(viewentity->angles, pparams->viewangles);

			// Store off overridden viewangles
			v_angles = pparams->viewangles;
		}
	}
}

//==========================
// V_CalcRefdef
//==========================
void V_CalcRefdef( struct ref_params_s *pparams )
{
	// store a local copy in case we need to calc firstperson later
	memcpy( &tr.viewparams, pparams, sizeof( ref_params_t ) );
	g_fFrametime = pparams->frametime;
	pause = pparams->paused;
	if( pause ) return;

	// dead. switch to thirdperson, only in multiplayer
	if( !CVAR_TO_BOOL( thirdperson ) && (pparams->maxclients > 1) )
	{
		if( pparams->health <= 0 )
			gHUD.m_iCameraMode = 1;
		else
			gHUD.m_iCameraMode = 0;
	}

	// switch to 1st-person in spectator mode (camera issues)
	if (g_iUser1 && (gHUD.m_iCameraMode == 1))
		gHUD.m_iCameraMode = 0;
	
	if( pparams->intermission )
		V_CalcIntermissionRefdef( pparams );
	else if( pparams->viewentity > pparams->maxclients )
		V_CalcCameraRefdef( pparams );
	else if( gHUD.m_iCameraMode )
		V_CalcThirdPersonRefdef( pparams );
	else if (pparams->spectator || g_iUser1)	// g_iUser true if in spectator mode
		V_CalcSpectatorRefdef(pparams);
	else
		V_CalcFirstPersonRefdef( pparams );

	PlayWallSlideSound( pparams );
	PlayFlingWhooshSound( pparams );

	// roll camera when in car
	static float car_roll_ang = 0.0f;
	if( gHUD.InCar )
	{
		int roll_dir = (gHUD.m_iKeyBits & IN_MOVELEFT) ? -1 : 1;
		if( !(gHUD.m_iKeyBits & IN_MOVELEFT) && !(gHUD.m_iKeyBits & IN_MOVERIGHT) )
			roll_dir = 0;
		static float car_roll_ang = 0.0f;
		car_roll_ang = CL_UTIL_Approach( gHUD.CarSpeed * 0.05 * roll_dir, car_roll_ang, 3 * g_fFrametime );
		pparams->viewangles[ROLL] += car_roll_ang;
	}

	// diffusion - play this sound when underwater
	if( pparams->waterlevel == 3 )
	{
		UnderwaterSoundPlaying = true;
		gEngfuncs.pEventAPI->EV_PlaySound( gEngfuncs.GetLocalPlayer()->index, NULL, CHAN_STATIC, "player/underwater.wav", 0.3, 2.0, SND_CHANGE_VOL | SND_CHANGE_PITCH, 100 );
	}
	else
	{
		if( UnderwaterSoundPlaying )
		{
			gEngfuncs.pEventAPI->EV_StopSound( gEngfuncs.GetLocalPlayer()->index, CHAN_STATIC, "player/underwater.wav" );
			UnderwaterSoundPlaying = false;
		}
	}

	// diffusion - cache viewangles and origin if they have changed
	if( PrevViewAngles != pparams->viewangles )
		PrevViewAngles = pparams->viewangles;

	if( PrevViewOrg != pparams->vieworg )
		PrevViewOrg = pparams->vieworg;

	// diffusion - cool thingy ^^ camera moves slightly with cursor
	if( CVAR_TO_BOOL( ui_is_active ) )
	{
		int posx = 0;
		int posy = 0;
		gEngfuncs.GetMousePosition( &posx, &posy );
		if( !IS_NAN( posx ) && !IS_NAN( posy ) )
		{
			float cposx = RemapVal( posx, 0, ScreenWidth, -1.0f, 1.0f );
			float cposy = RemapVal( posy, 0, ScreenHeight, -1.0f, 1.0f );
			pparams->viewangles.y -= cposx;
			pparams->viewangles.x += cposy;
		}
	}
}