/*
GrassSolid_fp.glsl - fragment uber shader for grass meshes
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
#include "alpha2coverage.h"
#if defined( GRASS_SUN_SHADOW )
#include "sun_shadow.h"
#endif

uniform sampler2D		u_ColorMap;
uniform sampler2D		u_NormalMap;

uniform vec4 u_GrassParams[3];
#define u_FogParams u_GrassParams[1]
uniform float u_GenericCondition;

varying vec2		var_TexDiffuse;
varying vec3		var_VertexLight;
varying vec3		var_ViewVec;

#if defined( GRASS_SUN_SHADOW )
varying vec4		var_SunCoord;
varying vec3		var_LightRest;
varying vec3		var_LightSun;
#endif

void main( void )
{
	vec4 diffuse = texture2D( u_ColorMap, var_TexDiffuse );

	diffuse.a = AlphaRescaling( u_ColorMap, var_TexDiffuse, diffuse.a );

	if( diffuse.a < 0.5 )
		discard;
	
#if !defined( GRASS_FULLBRIGHT )
	vec3 light_diffuse = var_VertexLight;

	#if defined( GRASS_SUN_SHADOW )
		float sunLit = SunShadowValue( var_SunCoord, length( var_ViewVec ));
		// grass light is clamped to 1.0 where the world goes up to 2.0, so taking the sun out of the clamped value barely moves it. darken by the ratio the ground under it gets instead, fully lit stays as it was
		vec3 lit = min( var_LightRest + var_LightSun, 2.0 );
		vec3 shadowed = min( var_LightRest + var_LightSun * sunLit, 2.0 );
		light_diffuse *= shadowed / max( lit, vec3( 0.001 ));
	#endif

	// add bump
	if( bool(u_GenericCondition == 1.0f) )
	{
		vec3 N = normalmap2D( u_NormalMap, var_TexDiffuse );
		vec3 L = normalize( var_VertexLight );
		light_diffuse *= ComputeStaticBump( L, N );
	}

	diffuse.rgb *= light_diffuse;
#endif//GRASS_FULLBRIGHT

	if( u_FogParams.w > 0.0 )
	{
		float dist = length( var_ViewVec );
		float fogFactor = exp( -dist * u_FogParams.w );
		fogFactor = clamp( fogFactor, 0.0, 1.0 );
		diffuse.rgb = mix( u_FogParams.xyz, diffuse.rgb, fogFactor );
	}

	gl_FragColor = diffuse;
}