/*
GrassDlight_vp.glsl - vertex uber shader for all dlight types for grass meshes
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
#include "matrix.h"
#include "grass.h"

attribute vec4		attr_Position;	// gl_VertexID emulation (already preserved by & 15)
attribute vec4		attr_Normal;

uniform mat4		u_ModelMatrix;
uniform mat4		u_LightViewProjectionMatrix;
uniform vec4		u_LightParams[7];
#define u_LightOrigin	u_LightParams[3]
#define u_FogParams		u_LightParams[4]
#define u_ViewOrigin	u_LightParams[5].xyz
#define u_RealTime		u_LightParams[5].w
#define u_GrassFadeStart u_LightParams[6].x
#define u_GrassFadeDist	u_LightParams[6].y
#define u_GrassFadeEnd	u_LightParams[6].z

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
	float dist = distance( u_ViewOrigin, ( u_ModelMatrix * vec4( attr_Position.xyz, 1.0 )).xyz );
	float scale = clamp(( u_GrassFadeEnd - dist ) / u_GrassFadeDist, 0.0, 1.0 );
	int vertexID = int( attr_Position.w ) - int( 4.0 * floor( attr_Position.w * 0.25 ));		// equal to gl_VertexID & 3
	vec4 position = vec4( attr_Position.xyz + attr_Normal.xyz * ( attr_Normal.w * scale ), 1.0 );	// in object space

	if( /*bool( dist < GRASS_ANIM_DIST ) &&*/ bool( vertexID == 1 || vertexID == 2 ))
	{
		position = GrassAnimate( position, u_RealTime );
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
	var_TexDiffuse = GetTexCoordsForVertex( int( attr_Position.w ));
	gl_ClipVertex = gl_ModelViewMatrix * worldpos;

#if defined( GRASS_LIGHT_PROJECTION )
	var_ProjCoord = ( Mat4Texture( -0.5 ) * u_LightViewProjectionMatrix ) * worldpos;
#if defined( GRASS_HAS_SHADOWS )
	var_ShadowCoord = ( Mat4Texture( 0.5 ) * u_LightViewProjectionMatrix ) * worldpos;
#endif
#endif
	// these things are in worldspace and not a normalized
	var_Normal = -GetNormalForVertex( int( attr_Position.w ));
	var_LightVec = ( u_LightOrigin.xyz - worldpos.xyz );
}