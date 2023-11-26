/*
BrushOmniLight_fp.glsl - fragment uber shader for all dlight types for grass meshes
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
#include "alpha2coverage.h"

#if defined( GRASS_LIGHT_PROJECTION )
uniform sampler2D		u_ProjectMap;
#elif defined( GRASS_LIGHT_OMNIDIRECTIONAL )
uniform samplerCube		u_ProjectMap;
#endif
uniform sampler2D		u_ColorMap;

#if defined( GRASS_HAS_SHADOWS )
	#if defined( GRASS_LIGHT_PROJECTION )
		#include "shadow_proj.h"
	#elif defined( GRASS_LIGHT_OMNIDIRECTIONAL )
		#include "shadow_omni.h"
	#endif
#endif

uniform vec4		u_LightParams[7];
#define u_LightDir		u_LightParams[0]
#define u_LightDiffuse	u_LightParams[1]
#define u_ShadowParams	u_LightParams[2]
#define u_LightOrigin	u_LightParams[3]
#define u_FogParams		u_LightParams[4]
#define u_DynLightBrightness u_LightParams[6].w

varying vec2		var_TexDiffuse;
varying vec3		var_LightVec;
varying vec3		var_Normal;

#if defined( GRASS_LIGHT_PROJECTION )
varying vec4		var_ProjCoord;
#if defined( GRASS_HAS_SHADOWS )
varying vec4		var_ShadowCoord;
#endif
#endif

void main( void )
{
	float dist = length( var_LightVec );
	float atten = 1.0 - saturate( pow( dist * u_LightOrigin.w, u_LightDiffuse.a ));
	if( atten <= 0.0 ) discard; // fast reject
	vec3 L = vec3( 0.0 );

#if defined( GRASS_LIGHT_PROJECTION )
	L = normalize( var_LightVec );
	
	// spot attenuation
	float spotDot = dot( normalize( u_LightDir.xyz ), L );
	float fov = ( u_LightDir.w * 0.25 * ( M_PI / 180 ));
	float spotCos = cos( fov + fov );
	if( spotDot < spotCos ) discard;
#elif defined( GRASS_LIGHT_OMNIDIRECTIONAL )
	L = normalize( var_LightVec );
#endif
	vec3 N = normalize( var_Normal );

	// compute the diffuse term
	vec4 diffuse = texture2D( u_ColorMap, var_TexDiffuse );

	diffuse.a = AlphaRescaling( u_ColorMap, var_TexDiffuse, diffuse.a );

	if( diffuse.a < 0.5 )
		discard;

	vec3 light = vec3( 1.0 );
	float shadow = 1.0;

	if( bool( gl_FrontFacing )) L = -L;

	float Brightness = u_DynLightBrightness;

#if defined( GRASS_LIGHT_PROJECTION )
	light = u_LightDiffuse.rgb;	// light color

	// texture or procedural spotlight
	light *= 1.5 * Brightness * texture2DProj( u_ProjectMap, var_ProjCoord ).rgb;
//	atten *= smoothstep( spotCos, spotCos + 0.1, spotDot ) * 0.5;
	#if defined( GRASS_HAS_SHADOWS )
		shadow = ShadowProj( var_ShadowCoord, u_ShadowParams.xy, dot( N, L ));
		if( shadow <= 0.0 ) discard; // fast reject
	#endif
#elif defined( GRASS_LIGHT_OMNIDIRECTIONAL )
	light = u_LightDiffuse.rgb;
	light *= 1.5 * Brightness * textureCube( u_ProjectMap, -var_LightVec ).rgb;
	#if defined( GRASS_HAS_SHADOWS )
		shadow = ShadowOmni( -var_LightVec, u_ShadowParams );
		if( shadow <= 0.0 ) discard; // fast reject
	#endif
#endif

#if defined( GRASS_FOG_EXP )
	float fogFactor = saturate( exp2( -u_FogParams.w * ( gl_FragCoord.z / gl_FragCoord.w )));
	atten = mix( 0, atten, fogFactor );
#endif
	// do modified hemisperical lighting
	float NdotL = max( (dot( N, L ) + ( SHADE_LAMBERT - 1.0f )) / SHADE_LAMBERT, 0.0 );
	diffuse.rgb *= light.rgb * NdotL * atten * DLIGHT_SCALE;

	// compute final color
	gl_FragColor = diffuse * shadow;
}