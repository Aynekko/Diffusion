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

uniform sampler2D		u_ColorMap;
uniform sampler2D		u_NormalMap;

uniform vec4 u_GrassParams[3];
#define u_FogParams u_GrassParams[1]
uniform float u_GenericCondition;

varying vec2		var_TexDiffuse;
varying vec3		var_VertexLight;
varying vec3		var_ViewVec;

void main( void )
{
	vec4 diffuse = texture2D( u_ColorMap, var_TexDiffuse );

	diffuse.a = AlphaRescaling( u_ColorMap, var_TexDiffuse, diffuse.a );

	if( diffuse.a < 0.5 )
		discard;
	
#if !defined( GRASS_FULLBRIGHT )
	vec3 light_diffuse = var_VertexLight;

	// add bump
	if( bool(u_GenericCondition == 1.0f) )
	{
		vec3 N = normalmap2D( u_NormalMap, var_TexDiffuse );
		vec3 L = normalize( var_VertexLight );
		light_diffuse *= ComputeStaticBump( L, N );
	}

	diffuse.rgb *= light_diffuse;
#endif//GRASS_FULLBRIGHT

	if( u_FogParams.x + u_FogParams.y + u_FogParams.z + u_FogParams.w > 0.0 )
	{
		float fogFactor = saturate( exp2( -u_FogParams.w * ( gl_FragCoord.z / gl_FragCoord.w )));
		diffuse.rgb = mix( u_FogParams.xyz, diffuse.rgb, fogFactor );
	}

	gl_FragColor = diffuse;
}