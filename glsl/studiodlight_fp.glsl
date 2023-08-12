/*
StudioDlight_fp.glsl - fragment uber shader for all dlight types for studio meshes
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
#include "matrix.h"
#include "emboss.h"
#include "specular.h"
#include "interior.h"

uniform sampler2D	u_ColorMap;
uniform sampler2D	u_NormalMap;

#if defined( STUDIO_HAS_COLORMASK )
	uniform sampler2D	u_ColorMask;
#endif

#if defined( STUDIO_LIGHT_PROJECTION )
uniform sampler2D	u_ProjectMap;
#elif defined( STUDIO_LIGHT_OMNIDIRECTIONAL )
uniform samplerCube	u_ProjectMap;
#endif

#if defined( STUDIO_HAS_SHADOWS )
	#if defined( STUDIO_LIGHT_PROJECTION )
		#include "shadow_proj.h"
	#elif defined( STUDIO_LIGHT_OMNIDIRECTIONAL )
		#include "shadow_omni.h"
	#endif
#endif

uniform vec4		u_LightDir;
uniform vec4		u_LightDiffuse;
uniform vec4		u_LightOrigin;
uniform vec4		u_ShadowParams;
uniform vec4		u_FogParams;
uniform vec4		u_RenderColor;
uniform vec3            u_MeshAngles;

uniform float		u_MeshScale;
uniform float	        u_GlossSmoothness;
uniform float	        u_GlossScale;
uniform float	        u_EmbossScale;
uniform float		u_DynLightBrightness;
uniform float           u_RealTime;

// shared variables
#if defined( STUDIO_LIGHT_PROJECTION )
varying vec4		var_ProjCoord;
#endif

#if defined( STUDIO_HAS_SHADOWS )
varying vec4		var_ShadowCoord;
#endif

varying vec2		var_TexDiffuse;
varying vec3		var_LightVec;
varying vec3		var_ViewVec;
varying vec3		var_Position;
varying vec3		var_Normal;

#if defined( STUDIO_BUMP ) || defined( STUDIO_INTERIOR )
varying mat3	        var_MatrixTBN;
#endif

void main( void )
{		
	float dist = length( var_LightVec );
	float atten = 1.0 - saturate( pow( dist * u_LightOrigin.w, u_LightDiffuse.a ));
	if( atten <= 0.0 ) discard; // fast reject
	vec3 L = vec3( 0.0 );

#if defined( STUDIO_LIGHT_PROJECTION )
	L = normalize( var_LightVec );
	
	// spot attenuation
	float spotDot = dot( normalize( u_LightDir.xyz ), L );
	float fov = ( u_LightDir.w * FOV_MULT * ( M_PI / 180.0 ));
	float spotCos = cos( fov + fov );
	if( spotDot < spotCos ) discard;
#elif defined( STUDIO_LIGHT_OMNIDIRECTIONAL )
	L = normalize( var_LightVec );
#endif

	vec3 V = normalize( var_ViewVec );
	vec3 N;

        // compute the normal term
#if defined( STUDIO_BUMP )
	N = normalmap2D( u_NormalMap, var_TexDiffuse );
#else
	N = normalize( var_Normal );
#endif

	// two side materials support
	if( bool( gl_FrontFacing )) N = -N;

	// compute the material defines
	float GlossScale = u_GlossScale; 
	float GlossSmoothness = u_GlossSmoothness; 
	float EmbossScale = u_EmbossScale;	
	float Brightness = u_DynLightBrightness; 

	// compute the diffuse, emboss and specular term
	vec4 diffuse = texture2D( u_ColorMap, var_TexDiffuse );
#if defined( STUDIO_SPECULAR )
	vec3 glossmap = DiffuseToGlossmap( u_ColorMap, var_TexDiffuse );
#endif
        // apply emboss filter
#if defined( STUDIO_EMBOSS )
	vec3 emboss = EmbossFilter( u_ColorMap, var_TexDiffuse, EmbossScale );
	diffuse.rgb *= emboss;
#endif

#if defined( STUDIO_HAS_COLORMASK )
	vec4 colormask = texture2D( u_ColorMask, var_TexDiffuse );
	if( colormask.a > 0.5 )
		diffuse.rgb *= u_RenderColor.rgb;
#else
	diffuse.rgb *= u_RenderColor.rgb; // kRenderTransColor
#endif

#if defined( STUDIO_INTERIOR )
	diffuse = InteriorMapping( diffuse, var_TexDiffuse, N, u_RealTime, var_ViewVec * var_MatrixTBN, var_Position );
#endif

	if( diffuse.a < 0.5 ) discard; // no glowing on tex with alpha

#if defined( STUDIO_BUMP )
	// now, rotate normalmap to worldspace
	N = normalize( var_MatrixTBN * N );
#endif

	vec3 light = vec3( 1.0 );
	float shadow = 1.0;

#if defined( STUDIO_LIGHT_PROJECTION )
	light = u_LightDiffuse.rgb * DLIGHT_SCALE;	// light color

	// texture or procedural spotlight
	light *= 2.0 * Brightness * texture2DProj( u_ProjectMap, var_ProjCoord ).rgb;

	#if defined( STUDIO_HAS_SHADOWS )
		shadow = ShadowProj( var_ShadowCoord, u_ShadowParams.xy, dot( N, L ));
		if( shadow <= 0.0 ) discard; // fast reject
	#endif
#elif defined( STUDIO_LIGHT_OMNIDIRECTIONAL )
	light = u_LightDiffuse.rgb * DLIGHT_SCALE;
	light *= 2 * Brightness * textureCube( u_ProjectMap, -var_LightVec ).rgb;

	#if defined( STUDIO_HAS_SHADOWS )

	// fix shadows?
	// the original line was: shadow = ShadowOmni( -var_LightVec, u_ShadowParams);
	// that's all. everything below (between the lines) is my mess trying to compensate
	// but I think it works :)

	vec3 ShadowVec = VectorRotate( var_LightVec, u_MeshAngles ); // rotate entity angles to worldspace
	shadow = ShadowOmni( -ShadowVec, u_ShadowParams );

	if( shadow <= 0.0 ) discard; // fast reject
	#endif
#endif

#if defined( STUDIO_FOG_EXP )
	float fogFactor = saturate( exp2( -u_FogParams.w * ( gl_FragCoord.z / gl_FragCoord.w )));
	atten = Q_mix( 0.0, atten, fogFactor );
#endif
	// do modified hemisperical lighting
	float NdotL = max(( dot( N, L ) + ( SHADE_LAMBERT - 1.0 )) / SHADE_LAMBERT, 0.0 );
	if( NdotL <= 0.0 ) discard; // fast reject

	diffuse.rgb *= light.rgb * NdotL * atten * shadow;

	// apply specular lighting
#if defined( STUDIO_SPECULAR )
	float NdotLGloss = saturate( dot( N, L ));
	vec3 gloss = ComputeSpecular( N, V, L, glossmap, GlossSmoothness, GlossScale ) * ( light * 0.5 ) * NdotLGloss * atten * shadow;
	#if defined( STUDIO_EMBOSS )
		gloss *= emboss;
	#endif
	diffuse.rgb += gloss * shadow;
#endif

	// compute final color
	gl_FragColor = diffuse;
}