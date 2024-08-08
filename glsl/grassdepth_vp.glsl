/*
GrassDepth_vp.glsl - grass shadow pass
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

attribute vec4		attr_Position;	// gl_VertexID emulation (already preserved by & 15)
attribute vec4		attr_Normal;

uniform mat4		u_ModelMatrix;
uniform vec4 u_GrassParams[2];
#define u_ViewOrigin		u_GrassParams[0].xyz
#define u_RealTime			u_GrassParams[0].w
#define u_GrassFadeStart	u_GrassParams[1].x
#define u_GrassFadeDist		u_GrassParams[1].y
#define u_GrassFadeEnd		u_GrassParams[1].z

varying vec2		var_TexCoord;
varying vec3 var_VertexColor;

void main( void )
{
	float dist = distance( u_ViewOrigin, ( u_ModelMatrix * vec4( attr_Position.xyz, 1.0 )).xyz );
	float scale = clamp(( u_GrassFadeEnd - dist ) / u_GrassFadeDist, 0.0, 1.0 );
	int vertexID = int( attr_Position.w ) - int( 4.0 * floor( attr_Position.w * 0.25 ));		// equal to gl_VertexID & 3
	vec4 position = vec4( attr_Position.xyz + attr_Normal.xyz * ( attr_Normal.w * scale ), 1.0 );	// in object space

	if( bool( dist < GRASS_ANIM_DIST ) && bool( vertexID == 1 || vertexID == 2 ))
	{
		position.x += sin( position.z + u_RealTime * 0.5 );
		position.y += cos( position.z + u_RealTime * 0.5 );
	}

	vec4 worldpos = u_ModelMatrix * position;

	// interactive grass!
	vec3 dir = worldpos.xyz - u_ViewOrigin;
	// length 2D - works better to have similar results when standing or crouching
	float dist_to_bush = sqrt( dir.x * dir.x + dir.y * dir.y );
	if( dist_to_bush <= 50 )
	{
		vec3 angles = VectorAngles( worldpos.xyz - u_ViewOrigin );
		vec3 forward = ForwardFromAngles( angles );
		float move_dist = 1.0 / clamp( 0.001 * pow(1.2, dist_to_bush), 0.001, 2.5 ); // empirical
		if( move_dist > 10.0 ) move_dist = 10.0;
		worldpos.xy += move_dist * normalize( forward ).xy;
		worldpos.z -= move_dist * 0.5;
	}

	gl_Position = gl_ModelViewProjectionMatrix * worldpos;
	var_TexCoord = GetTexCoordsForVertex( int( attr_Position.w ));
	gl_ClipVertex = gl_ModelViewMatrix * worldpos;
	var_VertexColor = vec3(0.0);
}