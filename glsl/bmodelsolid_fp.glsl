/*
BmodelSolid_fp.glsl - fragment uber shader for all solid bmodel surfaces
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
#include "screen.h"
#include "emboss.h"
#include "specular.h"
#include "deluxemap.h"
#include "cubemap.h"
#include "interior.h"
#include "alpha2coverage.h"

#if defined( BMODEL_MULTI_LAYERS )
uniform sampler2DArray	u_ColorMap;
uniform sampler2DArray	u_NormalMap;
uniform float		u_GlossSmoothness[TERRAIN_NUM_LAYERS];
uniform float		u_GlossScale[TERRAIN_NUM_LAYERS];
uniform float		u_EmbossScale[TERRAIN_NUM_LAYERS];
#else
uniform sampler2D	u_ColorMap;
uniform sampler2D	u_NormalMap;
uniform float		u_GlossSmoothness;
uniform float		u_GlossScale;
uniform float		u_EmbossScale;
uniform float       u_ReflectScale;
#endif

#if defined( BMODEL_REFLECTION_PLANAR ) || defined( BMODEL_WATER_PLANAR )
uniform sampler2D	u_WaterTex;     // water diffuse texture to mix with reflection
uniform float           u_PlanarReflectScale;
#endif

uniform sampler2D	u_DepthMap;
uniform sampler2D	u_GlowMap;

uniform vec4		u_RenderColor;
uniform vec4		u_FogParams;
uniform vec3		u_ViewOrigin;
uniform float		u_RealTime;
uniform float		u_zFar; 
uniform float       u_Fresnel;

// shared variables
varying vec2		var_TexDiffuse;
varying vec2		var_TexGlobal;
varying vec3		var_TexLight0;
varying vec3		var_TexLight1;
varying vec3		var_TexLight2;
varying vec3		var_TexLight3;
varying vec3		var_ViewVec; 
varying vec3		var_Normal;
varying mat3		var_MatrixTBN;

#if defined( BMODEL_REFLECTION_PLANAR ) || defined( BMODEL_WATER_PLANAR ) || defined( BMODEL_PORTAL )
varying vec4		var_TexMirror;	// mirror coords
#endif

#if defined( REFLECTION_CUBEMAP ) || defined( BMODEL_INTERIOR )
varying vec3		var_Position;
varying vec3		var_WorldNormal;
#endif

void main( void )
{
	vec4 diffuse = vec4( 0.0 );
	vec3 glossmap = vec3( 0.0 );
	vec3 emboss = vec3( 1.0 );
	vec3 N = normalize( var_Normal );

	vec4 planar_reflection = vec4( 0.0 );
	vec3 cubemap_reflection = vec3( 0.0 );
	vec3 screenmap;

	float alpha = u_RenderColor.a;
	float fogFactor = 0.0f;
	float fresnel = 0.0f;

        // compute the masks for terrain
#if defined( BMODEL_MULTI_LAYERS )
	vec4 mask0, mask1, mask2, mask3;
	TerrainReadMask( var_TexGlobal, mask0, mask1, mask2, mask3 );
#endif

	vec3 V = normalize( var_ViewVec * var_MatrixTBN );

	// compute the normal term
#if defined( BMODEL_WATER ) 
        #if defined( BMODEL_WATER_REFRACTION )
        	vec3 WaterNormal = normalmap2D( u_NormalMap, var_TexDiffuse * 2.5 );
        	N = normalize( WaterNormal );
    	#endif 
#elif defined( BMODEL_MULTI_LAYERS )
	#if defined( BMODEL_BUMP )
		N = TerrainApplyNormal( u_NormalMap, var_TexDiffuse, mask0, mask1, mask2, mask3 );
	#endif	
#else
	#if defined( BMODEL_BUMP )
		#if defined( BMODEL_INTERIOR )
			N = normalmap2D( u_NormalMap, fract(var_TexDiffuse * u_InteriorGrid));
		#else
			N = normalmap2D( u_NormalMap, var_TexDiffuse );
		#endif
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
#else
	float GlossScale = u_GlossScale; 
	float GlossSmoothness = u_GlossSmoothness; 
	float EmbossScale = u_EmbossScale;
#endif

	// compute the diffuse, specular and emboss term
#if defined( BMODEL_REFLECTION_PLANAR ) || defined( BMODEL_WATER_PLANAR ) || defined( BMODEL_PORTAL )
	#if defined( BMODEL_WATER_PLANAR )
		diffuse = colormap2D( u_WaterTex, var_TexDiffuse );
		planar_reflection = texture2DProj( u_ColorMap, var_TexMirror );
		diffuse.rgb = Q_mix( planar_reflection.rgb, diffuse.rgb, u_PlanarReflectScale );
		glossmap = vec3( 0.5 );
	#elif defined( BMODEL_PORTAL )
		diffuse = texture2DProj( u_ColorMap, var_TexMirror );
	#else
		planar_reflection = reflectmap2D( u_ColorMap, var_TexMirror, N, gl_FragCoord.xyz, 0.15 ) * u_PlanarReflectScale;
		diffuse = planar_reflection;
	#endif
#elif defined( BMODEL_MULTI_LAYERS )
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
                #if defined( BMODEL_WATER )
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

#if defined( ALPHA_RESCALING )
	diffuse.a = AlphaRescaling( u_ColorMap, var_TexDiffuse, diffuse.a );
#endif

#if !defined( BMODEL_INTERIOR )
	if( diffuse.a < 0.5 ) discard;
#endif

#if defined( BMODEL_MONOCHROME )
	diffuse.rgb = vec3( GetLuminance( diffuse.rgb )); 
#endif

	vec3 light = vec3( 0.0 ); // completely black if have no lightmaps
	vec3 gloss = vec3( 0.0 );

#if !defined( BMODEL_FULLBRIGHT )
		// lighting the world polys
	#if defined( BMODEL_APPLY_STYLE0 )
	        ApplyLightStyle( var_TexLight0, N, V, glossmap, GlossSmoothness, GlossScale * 0.5, light, gloss );
	#endif

	#if defined( BMODEL_APPLY_STYLE1 )
	        ApplyLightStyle( var_TexLight1, N, V, glossmap, GlossSmoothness, GlossScale * 0.5, light, gloss );
	#endif

	#if defined( BMODEL_APPLY_STYLE2 )
	        ApplyLightStyle( var_TexLight2, N, V, glossmap, GlossSmoothness, GlossScale * 0.5, light, gloss );
	#endif

	#if defined( BMODEL_APPLY_STYLE3 )
	        ApplyLightStyle( var_TexLight3, N, V, glossmap, GlossSmoothness, GlossScale * 0.5, light, gloss );
	#endif

	#if defined( BMODEL_APPLY_STYLE0 ) || defined( BMODEL_APPLY_STYLE1 ) || defined( BMODEL_APPLY_STYLE2 ) || defined( BMODEL_APPLY_STYLE3 )
		light = min(( light * LIGHTMAP_SHIFT ), 2.0 );
		gloss = min(( gloss * LIGHTMAP_SHIFT ), 2.0 );
	#endif

	diffuse.rgb *= light;

	// apply specular lighting
	#if defined( BMODEL_SPECULAR )
		diffuse.rgb *= saturate( 1.0 - GetLuminance( gloss ));
		diffuse.rgb += gloss;
	#endif
#endif

#if defined( BMODEL_INTERIOR )
	diffuse = InteriorMapping( diffuse, var_TexDiffuse, N, 0, var_ViewVec, var_Position ); // u_realtime is currently not used
#endif

/*
#if defined( BMODEL_TRANSLUCENT ) // this is never used currently
	screenmap = texture2D( u_ScreenMap, gl_FragCoord.xy * u_ScreenSizeInv ).rgb;
	diffuse.rgb = Q_mix( screenmap, diffuse.rgb, u_RenderColor.a );
#endif
*/
	// apply fullbright pixels
#if defined( BMODEL_HAS_LUMA )
	diffuse.rgb += texture2D( u_GlowMap, var_TexDiffuse ).rgb;
#endif

//------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------
#if defined( BMODEL_WATER ) && defined( BMODEL_WATER_REFRACTION )
	
	float WaterBorderFactor = 1.0, WaterAbsorbFactor = 1.0, WaterRefractFactor = 1.0;
	float RefractScale = 0.25;
	alpha *= 0.015;

	float fOwnDepth = gl_FragCoord.z;
	fOwnDepth = linearizeDepth( u_zFar, fOwnDepth );
	fOwnDepth = RemapVal( fOwnDepth, Z_NEAR, u_zFar, 0.0, 1.0 );

	float fSampledDepth = texture2D( u_DepthMap, gl_FragCoord.xy * u_ScreenSizeInv ).r;
	fSampledDepth = linearizeDepth( u_zFar, fSampledDepth );
	fSampledDepth = RemapVal( fSampledDepth, Z_NEAR, u_zFar, 0.0, 1.0 );

	float depthDelta = fOwnDepth - fSampledDepth;
	float WaterAbsorbScale = clamp( alpha - (1.0 / 255.0), 0.0, 1.0 ) * 50.0;
	WaterBorderFactor = 1.0 - saturate( exp2( -768.0 * 100.0 * depthDelta ));
	WaterRefractFactor = 1.0 - saturate( exp2( -768.0 * 5.0 * depthDelta ));
	WaterAbsorbFactor = 1.0 - saturate( exp2( -768.0 * WaterAbsorbScale * depthDelta ));

	float fDistortedDepth = texture2D( u_DepthMap, GetDistortedTexCoords( N, WaterRefractFactor, RefractScale )).r;
	fDistortedDepth = linearizeDepth( u_zFar, fDistortedDepth );
	fDistortedDepth = RemapVal( fDistortedDepth, Z_NEAR, u_zFar, 0.0, 1.0 );

	WaterRefractFactor *= step( fDistortedDepth, fOwnDepth );

	screenmap = GetScreenColorForRefraction( N, WaterRefractFactor, RefractScale );
	vec4 WaterColor = u_RenderColor;

	#if defined( REFLECTION_CUBEMAP ) && (!defined( BMODEL_REFLECTION_PLANAR ) && !defined( BMODEL_WATER_PLANAR ))
		cubemap_reflection = GetReflectionProbe( var_Position, u_ViewOrigin, N, glossmap, 0.5 ) * u_ReflectScale; 
                fresnel = GetFresnel( V, N, u_Fresnel, u_ReflectScale );
                diffuse.rgb = mix( diffuse.rgb, cubemap_reflection, fresnel );

	#elif (defined( BMODEL_REFLECTION_PLANAR ) || defined( BMODEL_WATER_PLANAR )) && !defined( REFLECTION_CUBEMAP )
		planar_reflection = reflectmap2D( u_ColorMap, var_TexMirror, N, gl_FragCoord.xyz, 0.15 ) * u_PlanarReflectScale;
                fresnel = GetFresnel( V, N, u_Fresnel, u_PlanarReflectScale );
                diffuse.rgb = mix( diffuse.rgb, planar_reflection.rgb, fresnel );

	#elif (defined( BMODEL_REFLECTION_PLANAR ) || defined( BMODEL_WATER_PLANAR )) && defined( REFLECTION_CUBEMAP )
		cubemap_reflection = GetReflectionProbe( var_Position, u_ViewOrigin, N, glossmap, 0.5 ) * u_ReflectScale; 
		planar_reflection = reflectmap2D( u_ColorMap, var_TexMirror, N, gl_FragCoord.xyz, 0.15 ) * u_PlanarReflectScale;
                fresnel = GetFresnel( V, N, u_Fresnel, u_PlanarReflectScale );
                vec3 average_reflection = mix( cubemap_reflection, planar_reflection.rgb, 0.5 );
                diffuse.rgb = mix( diffuse.rgb, average_reflection, fresnel );
	#endif

	#if defined( BMODEL_FOG_EXP )
		vec4 FogParams = u_FogParams;
		fogFactor = saturate( exp2( -FogParams.w * ( gl_FragCoord.z / gl_FragCoord.w )));
		diffuse.rgb = Q_mix( FogParams.xyz, diffuse.rgb, fogFactor );
	#endif

	vec3 borderSmooth = mix( screenmap, screenmap * WaterColor.rgb, WaterBorderFactor ); // smooth transition between water and ground
	vec3 refracted = mix( borderSmooth, diffuse.rgb * WaterColor.rgb, WaterAbsorbFactor ); // mix between refracted light and own water color
   
	diffuse.rgb = refracted;

	gl_FragColor = diffuse;

	// return so we don't touch fog twice
	return;
#endif
//------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------
        // apply cubemap reflection
#if defined( BMODEL_WATER ) && !defined( BMODEL_WATER_REFRACTION )
	// skip cubemap
#elif defined( REFLECTION_CUBEMAP )
	#if defined( BMODEL_BUMP )
		mat3 tbnBasis = mat3( normalize( var_MatrixTBN[0] ), normalize( var_MatrixTBN[1] ), normalize( var_MatrixTBN[2] ));
		vec3 NW = normalize( tbnBasis * N );
	#else
		vec3 NW = var_WorldNormal;
	#endif
        fresnel = GetFresnel( V, N, u_Fresnel, u_ReflectScale );
	cubemap_reflection = GetReflectionProbe( var_Position, u_ViewOrigin, NW, glossmap, GlossSmoothness ) * u_ReflectScale;
								//	gl_FragColor = vec4( cubemap_reflection, 1.0 );
								//	return;
	diffuse.rgb += cubemap_reflection * fresnel;
#endif

        // apply global fog
#if defined( BMODEL_FOG_EXP )
	fogFactor = saturate( exp2( -u_FogParams.w * ( gl_FragCoord.z / gl_FragCoord.w )));
	diffuse.rgb = Q_mix( u_FogParams.xyz, diffuse.rgb, fogFactor );
#endif//BMODEL_FOG_EXP
	
	gl_FragColor = vec4( diffuse.rgb, diffuse.a * alpha );
}