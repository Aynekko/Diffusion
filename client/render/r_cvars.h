/*
r_cvars.h - renderer console variables list
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

#ifndef R_CVARS_H
#define R_CVARS_H

extern cvar_t	*gl_renderer;
extern cvar_t	*gl_anisotropy;
extern cvar_t	*gl_test;	// just cvar for testify new effects
extern cvar_t	*r_debug;	//show renderer info
extern cvar_t	*r_extensions;
extern cvar_t	*gl_shadows;
extern cvar_t	*r_finish;
extern cvar_t	*r_clear;
extern cvar_t	*r_speeds;
extern cvar_t *cl_hitmarker;
extern cvar_t *cl_muzzlelight;
extern cvar_t *cl_localweaponanims;
extern cvar_t *cl_achievement_notify;
extern cvar_t *cl_subtitles;
extern cvar_t	*r_dynamic;
extern cvar_t	*r_novis;
extern cvar_t	*r_nocull;
extern cvar_t	*r_nosort;
extern cvar_t	*r_lockpvs;
extern cvar_t	*r_lightmap;
extern cvar_t	*r_adjust_fov;
extern cvar_t	*r_wireframe;
extern cvar_t	*r_fullbright;
extern cvar_t	*r_drawentities;
extern cvar_t	*r_drawworld;
extern cvar_t	*r_drawsprites;
extern cvar_t	*r_sprites_batch;
extern cvar_t *r_drawmodels;
extern cvar_t	*r_allow_3dsky;
extern cvar_t	*gl_allow_portals;
extern cvar_t	*gl_allow_screens;
extern cvar_t	*r_recursion_depth;
extern cvar_t	*r_lighting_modulate;
extern cvar_t	*r_lightstyle_lerping;
extern cvar_t	*r_lighting_extended;
extern cvar_t	*r_cached_box_culling;
extern cvar_t	*r_polyoffset;
extern cvar_t	*r_grass;
extern cvar_t	*r_grass_lighting;
extern cvar_t	*r_grass_shadows;
extern cvar_t	*r_grass_fade_start;
extern cvar_t	*r_grass_fade_dist;
extern cvar_t	*gl_check_errors;
extern cvar_t	*vid_gamma;
extern cvar_t	*vid_brightness;
extern cvar_t	*r_show_renderpass;
extern cvar_t	*r_show_light_scissors;
extern cvar_t	*r_show_normals;
extern cvar_t	*r_show_lightprobes;
extern cvar_t	*r_fade_props;
extern cvar_t	*r_show_cubemaps;
extern cvar_t	*r_bloom_sprites;
extern cvar_t	*r_bloom;
extern cvar_t *r_blur;
extern cvar_t *r_blur_threshold;
extern cvar_t *r_blur_strength;
extern cvar_t *gl_sunshafts;
extern cvar_t *gl_sunshafts_blur;
extern cvar_t *gl_sunshafts_brightness;
extern cvar_t *gl_sunshafts_adaptive;
extern cvar_t *gl_renderscale;
extern cvar_t *gl_ssao;
extern cvar_t *gl_hbao;
extern cvar_t *gl_ssao_debug;
extern cvar_t *gl_tonemap;
extern cvar_t	*thirdperson;
extern cvar_t *cl_showmsgs;
extern cvar_t *cl_viewmodelblend;
extern cvar_t *cl_viewmodel_offset_x;
extern cvar_t *cl_viewmodel_offset_y;
extern cvar_t *cl_viewmodel_offset_z;
extern cvar_t *cl_viewmodel_extras;
extern cvar_t *cl_tutor;
extern cvar_t *cl_showhealthbars;
extern cvar_t *cl_largehud;
extern cvar_t *cl_centerhud;
extern cvar_t *r_shadowquality;
extern cvar_t *r_mirrorquality;
extern cvar_t *r_testdlight;
extern cvar_t *gl_lensflare;
extern cvar_t *gl_water_refraction;
extern cvar_t *gl_water_planar;
extern cvar_t *gl_exposure;
extern cvar_t *gl_emboss;
extern cvar_t *gl_specular;
extern cvar_t *gl_cubemaps;
extern cvar_t *gl_bump;
extern cvar_t *gl_heateffect_force;
extern cvar_t *cl_notbn;
extern cvar_t *r_show_tbn;
extern cvar_t *r_flashlightlockposition;
extern cvar_t *cl_crosshair;
extern cvar_t *cl_useicon;
extern cvar_t *cl_oldammohud;
extern cvar_t *room_type;
extern cvar_t *room_type_auto;
extern cvar_t *ui_is_active; // internal cvar
extern cvar_t *ui_videooptions_active;
extern cvar_t *ui_forcenoblur;
extern cvar_t *cl_background;
extern cvar_t *hud_fontscale;
extern cvar_t *r_pvs_radius;
extern cvar_t *r_decals_ext;

#endif//R_CVARS_H