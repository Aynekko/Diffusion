/*
BmodelDlight_fp.glsl - fragment uber shader for all dlight types for bmodel surfaces
Copyright (C) 2015 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "const.h"
#include "mathlib.h"
#include "texfetch.h"
#include "terrain.h"
#include "emboss.h"
#include "specular.h"
#include "interior.h"
#include "alpha2coverage.h"

#if defined( BMODEL_MULTI_LAYERS )
uniform sampler2DArray	u_ColorMap;
uniform sampler2DArray	u_NormalMap;
uniform float   	u_DynLightBrightness[TERRAIN_NUM_LAYERS];
uniform float		u_GlossSmoothness[TERRAIN_NUM_LAYERS];
uniform float		u_GlossScale[TERRAIN_NUM_LAYERS];
uniform float		u_EmbossScale[TERRAIN_NUM_LAYERS];
#else
uniform sampler2D	u_ColorMap;
uniform sampler2D	u_NormalMap;
uniform float   	u_DynLightBrightness;
uniform float		u_GlossSmoothness;
uniform float		u_GlossScale;
uniform float		u_EmbossScale;
#endif

uniform sampler2D	u_ScreenMap;	// screen copy

#if defined( BMODEL_LIGHT_PROJECTION )
uniform sampler2D	u_ProjectMap;
#elif defined( BMODEL_LIGHT_OMNIDIRECTIONAL )
uniform samplerCube	u_ProjectMap;
#endif

#if defined( BMODEL_HAS_SHADOWS )
	#if defined( BMODEL_LIGHT_PROJECTION )
		#include "shadow_proj.h"
	#elif defined( BMODEL_LIGHT_OMNIDIRECTIONAL )
		#include "shadow_omni.h"
	#endif
#endif

uniform vec4			u_LightParams[5];
#define u_LightDir		u_LightParams[0]
#define u_LightDiffuse	u_LightParams[1]
#define u_ShadowParams	u_LightParams[2]
#define u_LightOrigin	u_LightParams[3]

uniform vec4		u_RenderColor;
uniform vec4		u_FogParams;
uniform vec2		u_ScreenSizeInv;
uniform float           u_RealTime;

#if defined( BMODEL_LIGHT_PROJECTION )
varying vec4		var_ProjCoord;
#endif

#if defined( BMODEL_HAS_SHADOWS )
varying vec4		var_ShadowCoord;
#endif

varying vec2		var_TexDiffuse;
varying vec2		var_TexGlobal;
varying vec3		var_LightVec;
varying vec3	        var_ViewVec; 
varying vec3		var_Normal;
varying mat3	        var_MatrixTBN;
varying vec3		var_Position;

void main( void )
{
	vec4 diffuse = vec4( 0.0 );
	vec3 glossmap = vec3( 0.0 );
	vec3 emboss = vec3( 1.0 );

        // compute the masks for terrain
#if defined( BMODEL_MULTI_LAYERS )
	vec4 mask0, mask1, mask2, mask3;
	TerrainReadMask( var_TexGlobal, mask0, mask1, mask2, mask3 );
#endif

	float dist = length( var_LightVec );
	float atten = 1.0 - saturate( pow( dist * u_LightOrigin.w, u_LightDiffuse.a ));
	if( atten <= 0.0 ) discard; // fast reject
	vec3 L = vec3( 0.0 );

#if defined( BMODEL_LIGHT_PROJECTION )
	L = normalize( var_LightVec );

	// spot attenuation
	float spotDot = dot( normalize( u_LightDir.xyz ), L );
	float fov = ( u_LightDir.w * FOV_MULT * ( M_PI / 180.0 ));
	float spotCos = cos( fov + fov );
	if( spotDot < spotCos ) discard;
#elif defined( BMODEL_LIGHT_OMNIDIRECTIONAL )
	L = normalize( var_LightVec );
#endif

	vec3 V = normalize( var_ViewVec );
	vec3 N;

        // compute the normal term
#if defined( BMODEL_WATER ) && defined( BMODEL_WATER_REFRACTION )
	// rotate water normal to worldpsace
	vec3 WaterNormal = normalmap2D( u_NormalMap, var_TexDiffuse * 2.5 );
	N = WaterNormal;
#elif defined( BMODEL_MULTI_LAYERS )
	#if defined( BMODEL_BUMP )
		N = TerrainApplyNormal( u_NormalMap, var_TexDiffuse, mask0, mask1, mask2, mask3 );
	#else
		N = normalize( var_Normal );
	#endif	
#else
	#if defined( BMODEL_BUMP )
		N = normalmap2D( u_NormalMap, var_TexDiffuse );
	#else
		N = normalize( var_Normal );
	#endif
#endif

	// two side materials support
	if( bool( gl_FrontFacing )) N = -N;

	// compute the matetial defines
#if defined( BMODEL_MULTI_LAYERS )
	vec3 MaterialDefines = TerrainCalcMaterialDefines( u_GlossScale, u_GlossSmoothness, u_EmbossScale, mask0, mask1, mask2, mask3 );
	float GlossScale = MaterialDefines.x; 
	float GlossSmoothness = MaterialDefines.y; 
	float EmbossScale = MaterialDefines.z;
	float Brightness = TerrainCalcDynLightBrightness( u_DynLightBrightness, mask0, mask1, mask2, mask3 ); 
#else
	float GlossScale = u_GlossScale; 
	float GlossSmoothness = u_GlossSmoothness; 
	float EmbossScale = u_EmbossScale;
	float Brightness = u_DynLightBrightness; 
#endif

	// compute the diffuse, specular and emboss term
#if defined( BMODEL_MULTI_LAYERS )
	diffuse = TerrainApplyDiffuse( u_ColorMap, var_TexDiffuse, mask0, mask1, mask2, mask3 );
	#if defined( BMODEL_SPECULAR )
		glossmap = DiffuseToGlossmapTerrain( u_ColorMap, var_TexDiffuse, mask0, mask1, mask2, mask3 );
	#endif
	#if defined( BMODEL_EMBOSS )
		emboss = EmbossFilterTerrain( u_ColorMap, var_TexDiffuse, mask0, mask1, mask2, mask3, EmbossScale );
	#endif
#else
	diffuse = texture2D( u_ColorMap, var_TexDiffuse );
	#if defined( BMODEL_SPECULAR )
            #if defined( BMODEL_WATER ) && defined( BMODEL_WATER_REFRACTION )
		glossmap = vec3( 0.5 );
            #else
		glossmap = DiffuseToGlossmap( u_ColorMap, var_TexDiffuse );
            #endif
	#endif
	#if defined( BMODEL_EMBOSS )
		emboss = EmbossFilter( u_ColorMap, var_TexDiffuse, EmbossScale );
	#endif
#endif
        // apply emboss filter
#if defined( BMODEL_EMBOSS )
	diffuse.rgb *= emboss;
#endif
	diffuse.rgb *= u_RenderColor.rgb; // kRenderTransColor

#if defined( BMODEL_INTERIOR )
	diffuse = InteriorMapping( diffuse, var_TexDiffuse, N, 0, var_ViewVec, var_Position ); // u_realtime is currently not used
#endif

	if( diffuse.a < 0.5 ) discard;

#if defined( BMODEL_BUMP )
	// now, rotate normalmap to worldspace
	N = normalize( var_MatrixTBN * N );
#endif

	vec3 light = vec3( 1.0 );
	float shadow = 1.0;
	float RenderModeModifier = 1.0;

#if defined( BMODEL_KRENDERTRANSTEXTURE )
	RenderModeModifier = 0.2;
#endif

#if defined( BMODEL_LIGHT_PROJECTION )
	light = u_LightDiffuse.rgb * DLIGHT_SCALE;	// light color

	// texture or procedural spotlight
	light *= 2 * Brightness * RenderModeModifier * texture2DProj( u_ProjectMap, var_ProjCoord ).rgb;

	#if defined( BMODEL_HAS_SHADOWS )
		shadow = ShadowProj( var_ShadowCoord, u_ShadowParams.xy, dot( N, L ));
		if( shadow <= 0.0 ) discard; // fast reject
	#endif
#elif defined( BMODEL_LIGHT_OMNIDIRECTIONAL )
	light = u_LightDiffuse.rgb;

	light *= Brightness * RenderModeModifier * textureCube( u_ProjectMap, -var_LightVec ).rgb;
	#if defined( BMODEL_HAS_SHADOWS )
		shadow = ShadowOmni( -var_LightVec, u_ShadowParams );
		if( shadow <= 0.0 ) discard; // fast reject
	#endif
#endif

#if defined( BMODEL_FOG_EXP )
	float fogFactor = saturate( exp2( -u_FogParams.w * ( gl_FragCoord.z / gl_FragCoord.w )));
	atten = Q_mix( 0.0, atten, fogFactor );
#endif
	// do modified hemisperical lighting
	float NdotL = max(( dot( N, L ) + ( SHADE_LAMBERT - 1.0 )) / SHADE_LAMBERT, 0.0 );
	if( NdotL <= 0.0 ) discard; // fast reject

	diffuse.rgb *= light.rgb * NdotL * atten * shadow;

	// apply specular lighting
	#if defined( BMODEL_SPECULAR )
		float NdotLGloss = saturate( dot( N, L ));
		vec3 gloss = ComputeSpecular( N, V, L, glossmap, GlossSmoothness, GlossScale ) * ( light * 0.5 ) * NdotLGloss * atten * shadow;
		#if defined( BMODEL_EMBOSS )
			gloss *= emboss;
		#endif
		diffuse.rgb += gloss * shadow;
	#endif

#if defined( BMODEL_TRANSLUCENT )
	vec3 screenmap = texture2D( u_ScreenMap, gl_FragCoord.xy * u_ScreenSizeInv ).xyz;
	screenmap.rgb *= light.rgb * NdotL * atten;
	diffuse.rgb = Q_mix( screenmap, diffuse.rgb, u_RenderColor.a );
#endif

	// compute final color
	gl_FragColor = diffuse;
}