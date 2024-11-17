/*
StudioSolid_vp.glsl - vertex uber shader for all solid studio meshes
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
#include "tnbasis.h"

attribute vec3		attr_Position;
attribute vec2		attr_TexCoord0;
attribute vec4		attr_BoneIndexes;
attribute vec4		attr_BoneWeights;

#if defined( STUDIO_VERTEX_LIGHTING )
attribute vec4		attr_LightColor;
attribute vec4		attr_LightVecs;
#endif

#if defined( STUDIO_SWAY_FOLIAGE )
uniform int			u_FoliageSwayHeight;
#endif

uniform vec4		u_BoneQuaternion[MAXSTUDIOBONES];
uniform vec3		u_BonePosition[MAXSTUDIOBONES];

uniform vec4		u_StudioLighting[2];
#define u_LightDir		u_StudioLighting[1].xyz
#if defined( STUDIO_VERTEX_LIGHTING )
uniform vec4		u_GammaTable[64];
#define u_LightStyles	u_StudioLighting[0]
#else
#define u_LightColor	u_StudioLighting[0].xyz
#define u_LightAmbient	u_StudioLighting[0].w
#define u_LightShade	u_StudioLighting[1].w
#endif

uniform vec3		u_MeshParams[3];
uniform vec4		u_StudioParams[3];
#define u_ViewOrigin	u_StudioParams[0].xyz
#define u_RealTime		u_StudioParams[0].w
#define u_ViewRight		u_StudioParams[1].xyz

varying vec3		var_LightDiffuse;
varying vec2		var_TexDiffuse;
varying vec3		var_LightVec;
varying vec3		var_ViewVec;
varying vec3		var_Normal;

#if defined( REFLECTION_CUBEMAP ) || defined( STUDIO_INTERIOR )
varying vec3		var_Position;
varying vec3		var_WorldNormal;
varying mat3		var_MatrixTBN;
#endif

varying vec4		var_ViewSpace;

void main( void )
{
	vec4 position = vec4( attr_Position, 1.0 );
	float MeshScale = u_MeshParams[2].x;

#if defined( STUDIO_SWAY_FOLIAGE )
	if( position.z > u_FoliageSwayHeight )
	{
		position.x += position.z * 0.025 * sin( position.z + u_RealTime * 0.25 );
		position.y += position.z * 0.025 * cos( position.z + u_RealTime * 0.25 );
	}
#endif
// wind experiment :)
//	vec3 TempWindVelocity = vec3( 0, 300, 0 );
//	TempWindVelocity = VectorRotate( TempWindVelocity, u_MeshAngles );
//	position.xyz += TempWindVelocity * position.z * position.z * 0.000005;

#if defined( STUDIO_BONEWEIGHTING )
	int boneIndex0 = int( attr_BoneIndexes[0] );
	int boneIndex1 = int( attr_BoneIndexes[1] );
	int boneIndex2 = int( attr_BoneIndexes[2] );
	int boneIndex3 = int( attr_BoneIndexes[3] );
	float flWeight0 = attr_BoneWeights[0] / 255.0;
	float flWeight1 = attr_BoneWeights[1] / 255.0;
	float flWeight2 = attr_BoneWeights[2] / 255.0;
	float flWeight3 = attr_BoneWeights[3] / 255.0;
	float flTotal = flWeight0 + flWeight1 + flWeight2 + flWeight3;
	if( flTotal < 1.0 ) flWeight0 += 1.0 - flTotal;	// compensate rounding error

	// compute hardware skinning with boneweighting
	mat4 boneMatrix = Mat4FromOriginQuat( u_BoneQuaternion[boneIndex0], u_BonePosition[boneIndex0] ) * flWeight0;
	if( boneIndex1 != -1 ) boneMatrix += Mat4FromOriginQuat( u_BoneQuaternion[boneIndex1], u_BonePosition[boneIndex1] ) * flWeight1;
	if( boneIndex2 != -1 ) boneMatrix += Mat4FromOriginQuat( u_BoneQuaternion[boneIndex2], u_BonePosition[boneIndex2] ) * flWeight2;
	if( boneIndex3 != -1 ) boneMatrix += Mat4FromOriginQuat( u_BoneQuaternion[boneIndex3], u_BonePosition[boneIndex3] ) * flWeight3;
#elif( MAXSTUDIOBONES == 1 )
	mat4 boneMatrix = Mat4FromOriginQuat( u_BoneQuaternion[0], u_BonePosition[0] );
#else
	int boneIndex0 = int( attr_BoneIndexes[0] );
	// compute hardware skinning without boneweighting
	mat4 boneMatrix = Mat4FromOriginQuat( u_BoneQuaternion[boneIndex0], u_BonePosition[boneIndex0] );
#endif
	vec4 worldpos = boneMatrix * position;

	// compute TBN
	mat3 tbn = ComputeTBN( boneMatrix );

	vec3 MeshAngles = u_MeshParams[1];
	tbn[0] = VectorRotate( tbn[0], MeshAngles );
	tbn[1] = VectorRotate( tbn[1], MeshAngles );
	tbn[2] = VectorRotate( tbn[2], MeshAngles );

	// compute vectors
	vec3 srcL = u_LightDir;
	srcL = VectorRotate( srcL, MeshAngles );
	vec3 srcV = ( u_ViewOrigin - worldpos.xyz );
	srcV = VectorRotate( srcV, MeshAngles );
	vec3 srcN = tbn[2];

	// transform vertex position into homogenous clip-space
	gl_Position = gl_ModelViewProjectionMatrix * worldpos;
	gl_ClipVertex = gl_ModelViewMatrix * worldpos;

#if defined( STUDIO_FULLBRIGHT )
	var_LightDiffuse = vec3( 1.0 );	// just get fullbright
#elif defined( STUDIO_VERTEX_LIGHTING )
	vec3 lightmap = vec3( 0.0 );
	vec3 deluxmap = vec3( 0.0 );
	float gammaIndex;

	if( u_LightStyles.x != 0.0 )
    {
        lightmap += UnpackVector( attr_LightColor.x ) * u_LightStyles.x;
        deluxmap += UnpackNormal( -attr_LightVecs.x ) * u_LightStyles.x;
    }

    if( u_LightStyles.y != 0.0 )
    {
        lightmap += UnpackVector( attr_LightColor.y ) * u_LightStyles.y;
        deluxmap += UnpackNormal( -attr_LightVecs.y ) * u_LightStyles.y;
    }

    if( u_LightStyles.z != 0.0 )
    {
        lightmap += UnpackVector( attr_LightColor.z ) * u_LightStyles.z;
        deluxmap += UnpackNormal( -attr_LightVecs.z ) * u_LightStyles.z;
    }

    if( u_LightStyles.w != 0.0 )
    {
        lightmap += UnpackVector( attr_LightColor.w ) * u_LightStyles.w;
        deluxmap += UnpackNormal( -attr_LightVecs.w ) * u_LightStyles.w;
    }

	var_LightDiffuse = min(( lightmap * LIGHTMAP_SHIFT ), 1.0 );
    srcL = ( deluxmap * LIGHTMAP_SHIFT ); // get lightvector

	// apply screen gamma
	gammaIndex = ( var_LightDiffuse.r * 255.0 );
	var_LightDiffuse.r = u_GammaTable[int( gammaIndex/4.0 )][int( mod( gammaIndex, 4.0 ))];
	gammaIndex = ( var_LightDiffuse.g * 255.0 );
	var_LightDiffuse.g = u_GammaTable[int( gammaIndex/4.0 )][int( mod( gammaIndex, 4.0 ))];
	gammaIndex = ( var_LightDiffuse.b * 255.0 );
	var_LightDiffuse.b = u_GammaTable[int( gammaIndex/4.0 )][int( mod( gammaIndex, 4.0 ))];
#else
	float AmbientLight = u_LightAmbient;

	#if defined( STUDIO_LIGHT_FLATSHADE )
		AmbientLight += u_LightShade * 0.8;
	#else
		vec3 L = normalize( srcL );
		float lightcos;

		lightcos = dot( normalize( srcN ), L );
		lightcos = min( lightcos, 1.0 );
		AmbientLight += u_LightShade;

		// do modified hemispherical lighting
		lightcos = ( lightcos + ( SHADE_LAMBERT - 1.0 )) / SHADE_LAMBERT;
		if( lightcos > 0.0 )
			AmbientLight -= u_LightShade * lightcos; 
		AmbientLight = max( AmbientLight, 0.0 );
	#endif
	var_LightDiffuse = u_LightColor * ( AmbientLight / 255.0 );
#endif
	var_TexDiffuse = attr_TexCoord0;

#if defined( STUDIO_HAS_CHROME )
	// compute chrome texcoords
	vec3 origin = normalize( -u_ViewOrigin + vec3( boneMatrix[3] ));
	vec3 chromeup = normalize( cross( origin, u_ViewRight ));
	vec3 chromeright = normalize( cross( origin, chromeup ));

	var_TexDiffuse.x *= ( dot( srcN, chromeright ) + 1.0 ) * 32.0;	// calc s coord
	var_TexDiffuse.y *= ( dot( srcN, chromeup ) + 1.0 ) * 32.0;	// calc t coord
#endif

#if defined( STUDIO_BUMP ) || defined( STUDIO_INTERIOR )
	// transform into tangent space
	var_LightVec = -srcL * tbn * MeshScale;
	var_ViewVec = srcV * tbn * MeshScale;
	var_Normal = srcN * tbn;
#else
	// leave in worldspace
	var_LightVec = -srcL * MeshScale;
	var_ViewVec = srcV * MeshScale;
	var_Normal = srcN;
#endif

#if defined( REFLECTION_CUBEMAP ) || defined( STUDIO_INTERIOR )
	var_Position = worldpos.xyz;
	var_MatrixTBN = tbn;
#endif

	var_ViewSpace = gl_Position;
}