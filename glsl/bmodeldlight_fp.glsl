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
#if defined( BMODEL_EMBOSS )
#include "emboss.h"
#endif
#if defined( BMODEL_SPECULAR )
#include "specular.h"
#endif
#if defined( BMODEL_INTERIOR )
#include "interior.h"
#endif
#include "alpha2coverage.h"

#if defined( BMODEL_MULTI_LAYERS )
uniform sampler2DArray	u_ColorMap;
uniform sampler2DArray	u_NormalMap;
uniform float   	u_DynLightBrightness[TERRAIN_NUM_LAYERS];
#if defined( BMODEL_SPECULAR )
uniform float		u_GlossSmoothness[TERRAIN_NUM_LAYERS];
uniform float		u_GlossScale[TERRAIN_NUM_LAYERS];
#endif
#if defined( BMODEL_EMBOSS )
uniform float		u_EmbossScale[TERRAIN_NUM_LAYERS];
#endif
#else
uniform sampler2D	u_ColorMap;
uniform sampler2D	u_NormalMap;
uniform float   	u_DynLightBrightness;
#if defined( BMODEL_SPECULAR )
uniform float		u_GlossSmoothness;
uniform float		u_GlossScale;
#endif
#if defined( BMODEL_EMBOSS )
uniform float		u_EmbossScale;
#endif
#endif

#if defined( BMODEL_WATER_REFRACTION )
uniform sampler2D	u_DepthMap;
uniform vec2		u_ScreenSizeInv;
#endif

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

uniform vec4			u_LightParams[6];
#define u_LightDir		u_LightParams[0]
#define u_LightDiffuse	u_LightParams[1]
#define u_ShadowParams	u_LightParams[2]
#define u_LightOrigin	u_LightParams[3]
#define u_FogParams		u_LightParams[5]

uniform vec4		u_RenderColor;

#if defined( BMODEL_LIGHT_PROJECTION )
varying vec4		var_ProjCoord;
#endif

#if defined( BMODEL_HAS_SHADOWS )
varying vec4		var_ShadowCoord;
#endif

varying vec2		var_TexDiffuse;
varying vec2		var_TexGlobal;
varying vec3		var_LightVec;
varying vec3		var_ViewVec;
varying vec3		var_Normal;
varying mat3		var_MatrixTBN;
varying vec3		var_Position;

void main( void )
{
	vec4 diffuse = vec4( 0.0 );
	vec3 glossmap = vec3( 0.0 );
	vec3 emboss = vec3( 1.0 );
	float shadow = 1.0;

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

	vec4 tex_projection = texture2DProj( u_ProjectMap, var_ProjCoord );

	// ignore black pixels of the flashlight/projection texture
	if( length( tex_projection.rgb ) == 0.0 )
		discard;

	#if defined( BMODEL_HAS_SHADOWS )
		// compute shadow early to ignore the rest of the shader
		shadow = ShadowProj( var_ShadowCoord, u_ShadowParams.xy, dot( normalize( var_Normal ), L ));
		if( shadow <= 0.0 ) discard; // fast reject
	#endif
#elif defined( BMODEL_LIGHT_OMNIDIRECTIONAL )
	L = normalize( var_LightVec );
	#if defined( BMODEL_HAS_SHADOWS )
		shadow = ShadowOmni( -var_LightVec, u_ShadowParams );
		if( shadow <= 0.0 ) discard; // fast reject
	#endif
#endif

	// get diffuse texture and check alpha
#if defined( BMODEL_MULTI_LAYERS )
	diffuse = TerrainApplyDiffuse( u_ColorMap, var_TexDiffuse, mask0, mask1, mask2, mask3 );
#else
	#if defined( BMODEL_WATER )
		diffuse = texture2D( u_ColorMap, var_TexDiffuse * 0.25 );
	#else
		diffuse = texture2D( u_ColorMap, var_TexDiffuse );
	#endif
	#if defined( BMODEL_SPECULAR )
		glossmap = diffuse.rgb;
	#endif
	diffuse.rgb *= diffuse.a;
#endif

#if !defined( BMODEL_INTERIOR )
	if( diffuse.a == 0.0 ) discard;
	#if !defined( BMODEL_KRENDERTRANSTEXTURE )
		if( diffuse.a < 0.5 ) discard;
	#endif
#endif

	vec3 V = normalize( var_ViewVec );
	vec3 N;

        // compute the normal term
#if defined( BMODEL_WATER ) && defined( BMODEL_WATER_REFRACTION )
	// rotate water normal to worldpsace
	vec3 WaterNormal = normalmap2D( u_NormalMap, var_TexDiffuse );
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

	float GlossScale = 0.0;
	float GlossSmoothness = 0.0;
	float EmbossScale = 0.0;

	// compute the material defines
#if defined( BMODEL_MULTI_LAYERS )
	float InitGlossScale[TERRAIN_NUM_LAYERS];
	float InitGlossSmoothness[TERRAIN_NUM_LAYERS];
	float InitEmbossScale[TERRAIN_NUM_LAYERS];
	#if defined( BMODEL_EMBOSS )
	InitEmbossScale = u_EmbossScale;
	#endif
	#if defined( BMODEL_SPECULAR )
	InitGlossScale = u_GlossScale; 
	InitGlossSmoothness = u_GlossSmoothness;
	#endif
	vec3 MaterialDefines = TerrainCalcMaterialDefines( InitGlossScale, InitGlossSmoothness, InitEmbossScale, mask0, mask1, mask2, mask3 );
	#if defined( BMODEL_SPECULAR )
	GlossScale = MaterialDefines.x; 
	GlossSmoothness = MaterialDefines.y;
	#endif
	#if defined( BMODEL_EMBOSS )
	EmbossScale = MaterialDefines.z;
	#endif
	float Brightness = TerrainCalcDynLightBrightness( u_DynLightBrightness, mask0, mask1, mask2, mask3 ); 
#else
	#if defined( BMODEL_SPECULAR )
	GlossScale = u_GlossScale;
	GlossSmoothness = u_GlossSmoothness;
	#endif
	#if defined( BMODEL_EMBOSS )
	EmbossScale = u_EmbossScale;
	#endif
	float Brightness = u_DynLightBrightness; 
#endif

	// compute specular and emboss term
#if defined( BMODEL_MULTI_LAYERS )
	#if defined( BMODEL_SPECULAR )
		glossmap = DiffuseToGlossmapTerrain( u_ColorMap, var_TexDiffuse, mask0, mask1, mask2, mask3 );
	#endif
	#if defined( BMODEL_EMBOSS )
		emboss = EmbossFilterTerrain( u_ColorMap, var_TexDiffuse, mask0, mask1, mask2, mask3, EmbossScale );
	#endif
#else
	#if defined( BMODEL_SPECULAR )
		#if defined( BMODEL_WATER ) && defined( BMODEL_WATER_REFRACTION )
			glossmap = vec3( 0.5 );
		#else
			glossmap = DiffuseToGlossmap( glossmap );
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
	if( diffuse.a < 0.98 )
		diffuse = InteriorMapping( diffuse, var_TexDiffuse, N, 0, var_ViewVec, var_Position ); // u_realtime is currently not used
#endif

#if defined( BMODEL_BUMP )
	// now, rotate normalmap to worldspace
	N = normalize( var_MatrixTBN * N );
#endif

#if defined( BMODEL_WATER_REFRACTION )
	
	float alpha = u_RenderColor.a;
	alpha = RemapVal( alpha, vec4( 0.0, 1.0, 0.263, 0.500 ) );
	alpha *= 0.015; // magic as well...

	float WaterAbsorbFactor = 1.0;
	const float u_zFar = -4096; // for consistency

	float fOwnDepth = gl_FragCoord.z;
	fOwnDepth = linearizeDepth( u_zFar, fOwnDepth );
	fOwnDepth = RemapVal( fOwnDepth, vec4( Z_NEAR, u_zFar, 0.0, 1.0 ) );

	float fSampledDepth = texture2D( u_DepthMap, gl_FragCoord.xy * u_ScreenSizeInv ).r;
	fSampledDepth = linearizeDepth( u_zFar, fSampledDepth );
	fSampledDepth = RemapVal( fSampledDepth, vec4( Z_NEAR, u_zFar, 0.0, 1.0 ) );

	float depthDelta = abs( fOwnDepth - fSampledDepth );
	float WaterAbsorbScale = clamp( alpha - (1.0 / 255.0), 0.0, 1.0 ) * 50.0;
	WaterAbsorbFactor = 1.0 - saturate( exp2( -768.0 * WaterAbsorbScale * depthDelta ));
#endif 

	vec3 light = vec3( 1.0 );
	float RenderModeModifier = 1.0;

#if defined( BMODEL_KRENDERTRANSTEXTURE )
	#if defined( BMODEL_WATER_REFRACTION ) 
		RenderModeModifier = 1.0 - exp( -WaterAbsorbFactor * 0.5 );
	#else 
//		RenderModeModifier = 0.2;
	#endif
#endif

#if defined( BMODEL_LIGHT_PROJECTION )
	light = u_LightDiffuse.rgb * DLIGHT_SCALE;	// light color

	// texture or procedural spotlight
	light *= Brightness * RenderModeModifier * tex_projection.rgb;
#elif defined( BMODEL_LIGHT_OMNIDIRECTIONAL )
	light = u_LightDiffuse.rgb * DLIGHT_SCALE;

	light *= Brightness * RenderModeModifier * textureCube( u_ProjectMap, -var_LightVec ).rgb;
#endif

	// do modified hemisperical lighting
	float NdotL = saturate( dot( N, L ));
	if( NdotL <= 0.0 ) discard; // fast reject

	diffuse.rgb *= light.rgb * NdotL * atten * shadow;

	// !!! remove this if HDR is implemented
	// this is a hack to remove overbright on brighter textures
	float hack = 1.0 - (0.5 * length( diffuse.rgb ));
	if( hack < 0.5 ) hack = 0.5;
	diffuse.rgb *= hack;

	// apply specular lighting
	#if defined( BMODEL_SPECULAR )
		vec3 gloss = ComputeSpecular( N, V, L, glossmap, GlossSmoothness, GlossScale ) * ( light * 0.5 ) * NdotL * atten * shadow;
		#if defined( BMODEL_EMBOSS )
			gloss *= emboss;
		#endif
		#if defined( BMODEL_WATER_REFRACTION ) 
			gloss *= RenderModeModifier;       
		#endif
		diffuse.rgb += gloss * shadow;
	#endif

	if( u_FogParams.w > 0.0 )
	{
		float dist = length( var_ViewVec );
		float fogFactor = exp( -dist * u_FogParams.w );
		fogFactor = clamp( fogFactor, 0.0, 1.0 );
		atten = mix( 0.0, atten, fogFactor );
	}

	// compute final color
	gl_FragColor = diffuse;
}
