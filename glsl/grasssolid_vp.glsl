/*
GrassSolid_vp.glsl - vertex uber shader for grass meshes
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
attribute vec4		attr_LightColor;
attribute vec4		attr_LightStyles;

uniform mat4		u_ModelMatrix;
uniform float		u_LightStyleValues[MAX_LIGHTSTYLES];
uniform vec4		u_GammaTable[64];
uniform vec4 u_GrassParams[3];
#define u_GrassFadeStart	u_GrassParams[2].x
#define u_GrassFadeDist		u_GrassParams[2].y
#define u_GrassFadeEnd		u_GrassParams[2].z
#define u_ViewOrigin		u_GrassParams[0].xyz
#define u_RealTime			u_GrassParams[0].w

varying vec2		var_TexDiffuse;
varying vec3		var_VertexLight;
varying vec3		var_ViewVec;

#if defined( GRASS_SUN_SHADOW )
uniform mat4		u_SunMatrix;	// world -> sun light clip
varying vec4		var_SunCoord;
varying vec3		var_LightRest;	// unclamped, everything but the sun style
varying vec3		var_LightSun;	// unclamped, the sun style alone
#endif

// clamp to the lightmap range, then screen gamma through the table
vec3 GrassLightToScreen( vec3 light )
{
	light = min(( light * LIGHTMAP_SHIFT ), 1.0 );

	float gammaIndex = ( light.r * 255.0 );
	light.r = u_GammaTable[int( gammaIndex * 0.25 )][int( mod( gammaIndex, 4 ))];
	gammaIndex = ( light.g * 255.0 );
	light.g = u_GammaTable[int( gammaIndex * 0.25 )][int( mod( gammaIndex, 4 ))];
	gammaIndex = ( light.b * 255.0 );
	light.b = u_GammaTable[int( gammaIndex * 0.25 )][int( mod( gammaIndex, 4 ))];

	return light;
}

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

#if !defined( GRASS_FULLBRIGHT )
	vec3 lightAll = vec3( 0.0 );
	vec3 lightSun = vec3( 0.0 );
	vec3 styleLight;

#if defined( GRASS_APPLY_STYLE0 )
	styleLight = UnpackVector( attr_LightColor.x ) * u_LightStyleValues[int( attr_LightStyles[0] )];
	lightAll += styleLight;
	#if defined( GRASS_SUN_STYLE0 )
		lightSun += styleLight;
	#endif
#endif

#if defined( GRASS_APPLY_STYLE1 )
	styleLight = UnpackVector( attr_LightColor.y ) * u_LightStyleValues[int( attr_LightStyles[1] )];
	lightAll += styleLight;
	#if defined( GRASS_SUN_STYLE1 )
		lightSun += styleLight;
	#endif
#endif

#if defined( GRASS_APPLY_STYLE2 )
	styleLight = UnpackVector( attr_LightColor.z ) * u_LightStyleValues[int( attr_LightStyles[2] )];
	lightAll += styleLight;
	#if defined( GRASS_SUN_STYLE2 )
		lightSun += styleLight;
	#endif
#endif

#if defined( GRASS_APPLY_STYLE3 )
	styleLight = UnpackVector( attr_LightColor.w ) * u_LightStyleValues[int( attr_LightStyles[3] )];
	lightAll += styleLight;
	#if defined( GRASS_SUN_STYLE3 )
		lightSun += styleLight;
	#endif
#endif

	var_VertexLight = GrassLightToScreen( lightAll );

#if defined( GRASS_SUN_SHADOW )
	// the fragment shader darkens by these in the world's range
	var_LightRest = ( lightAll - lightSun ) * LIGHTMAP_SHIFT;
	var_LightSun = lightSun * LIGHTMAP_SHIFT;
#endif
#endif//GRASS_FULLBRIGHT

	var_ViewVec = ( u_ViewOrigin - worldpos.xyz );

#if defined( GRASS_SUN_SHADOW )
	// the swayed and pushed-aside position, so the shadow moves with the blade
	var_SunCoord = ( Mat4Texture( 0.5 ) * u_SunMatrix ) * worldpos;
#endif
}