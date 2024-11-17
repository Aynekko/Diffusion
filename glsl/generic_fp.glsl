/*
generic_fp.glsl - generic shader
Copyright (C) 2014 Uncle Mike

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

uniform sampler2D		u_ColorMap;

uniform vec4		u_FogParams;

varying vec2	var_TexCoord;
varying vec3	var_VertexColor;
varying vec4	var_ViewSpace;
#if defined( FOG_USE_ALPHA )
varying float	var_VertexAlpha;
#endif

void main( void )
{
	vec4 diffuse = texture2D( u_ColorMap, var_TexCoord );

	if( diffuse.a < 0.5 )
		discard;

	if( u_FogParams.x + u_FogParams.y + u_FogParams.z + u_FogParams.w > 0.0 )
	{
		diffuse.rgb *= var_VertexColor;
		#if defined( FOG_USE_ALPHA )
		diffuse.a *= var_VertexAlpha;
		#endif
		// apply global fog
		float dist = length( var_ViewSpace );
	//	fogFactor = saturate( exp2( -u_FogParams.w * ( gl_FragCoord.z / gl_FragCoord.w )));
		float fogFactor = 1.0 / exp(dist * u_FogParams.w );
		fogFactor = clamp( fogFactor, 0.0, 1.0 );
		diffuse.rgb = mix( u_FogParams.xyz, diffuse.rgb, fogFactor );
	}
	gl_FragColor = diffuse;
}