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
#define MeshScale	u_MeshParams[2].x
#define MeshAngles	u_MeshParams[1]
uniform vec4		u_StudioParams[3];
#define u_ViewOrigin	u_StudioParams[0].xyz
#define u_RealTime		u_StudioParams[0].w
#define u_ViewRight		u_StudioParams[1].xyz

#if defined( STUDIO_VERTEX_LIGHTING ) && !defined( STUDIO_FULLBRIGHT )
	varying vec3		var_LightDiffuse[4];
	varying vec3		var_LightVec[4];
#else
	varying vec3		var_LightDiffuse;
	varying vec3		var_LightVec;
#endif

varying vec2		var_TexDiffuse;
varying vec3		var_ViewVec;
varying vec3		var_Normal;
varying float		var_Distance;

#if defined( REFLECTION_CUBEMAP ) || defined( STUDIO_INTERIOR )
varying vec3		var_Position;
varying vec3		var_WorldNormal;
varying mat3		var_MatrixTBN;
#endif

void main( void )
{
	vec4 position = vec4( attr_Position, 1.0 );

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
	// apply lightstyles
	float gammaIndex;

	if( u_LightStyles[0] != 0.0 )
	{
		var_LightDiffuse[0] = UnpackVector( attr_LightColor[0] );
			
		gammaIndex = ( var_LightDiffuse[0].r * 255.0 );
		var_LightDiffuse[0].r = u_GammaTable[int( gammaIndex/4.0 )][int( mod( gammaIndex, 4.0 ))];
		gammaIndex = ( var_LightDiffuse[0].g * 255.0 );
		var_LightDiffuse[0].g = u_GammaTable[int( gammaIndex/4.0 )][int( mod( gammaIndex, 4.0 ))];
		gammaIndex = ( var_LightDiffuse[0].b * 255.0 );
		var_LightDiffuse[0].b = u_GammaTable[int( gammaIndex/4.0 )][int( mod( gammaIndex, 4.0 ))];
			
		var_LightDiffuse[0] *= u_LightStyles[0] * LIGHTMAP_SHIFT;

		var_LightVec[0] = -UnpackNormal( -attr_LightVecs[0] );
		#if defined( STUDIO_BUMP ) || defined( STUDIO_INTERIOR )	
			var_LightVec[0] = var_LightVec[0] * tbn;
		#endif			
	}

	if( u_LightStyles[1] != 0.0 )
	{
		var_LightDiffuse[1] = UnpackVector( attr_LightColor[1] );
			
		gammaIndex = ( var_LightDiffuse[1].r * 255.0 );
		var_LightDiffuse[1].r = u_GammaTable[int( gammaIndex/4.0 )][int( mod( gammaIndex, 4.0 ))];
		gammaIndex = ( var_LightDiffuse[1].g * 255.0 );
		var_LightDiffuse[1].g = u_GammaTable[int( gammaIndex/4.0 )][int( mod( gammaIndex, 4.0 ))];
		gammaIndex = ( var_LightDiffuse[1].b * 255.0 );
		var_LightDiffuse[1].b = u_GammaTable[int( gammaIndex/4.0 )][int( mod( gammaIndex, 4.0 ))];
			
		var_LightDiffuse[1] *= u_LightStyles[1] * LIGHTMAP_SHIFT;

		var_LightVec[1] = -UnpackNormal( -attr_LightVecs[1] );
		#if defined( STUDIO_BUMP ) || defined( STUDIO_INTERIOR )	
			var_LightVec[1] = var_LightVec[1] * tbn;
		#endif			
	}

	if( u_LightStyles[2] != 0.0 )
	{
		var_LightDiffuse[2] = UnpackVector( attr_LightColor[2] );
			
		gammaIndex = ( var_LightDiffuse[2].r * 255.0 );
		var_LightDiffuse[2].r = u_GammaTable[int( gammaIndex/4.0 )][int( mod( gammaIndex, 4.0 ))];
		gammaIndex = ( var_LightDiffuse[2].g * 255.0 );
		var_LightDiffuse[2].g = u_GammaTable[int( gammaIndex/4.0 )][int( mod( gammaIndex, 4.0 ))];
		gammaIndex = ( var_LightDiffuse[2].b * 255.0 );
		var_LightDiffuse[2].b = u_GammaTable[int( gammaIndex/4.0 )][int( mod( gammaIndex, 4.0 ))];
			
		var_LightDiffuse[2] *= u_LightStyles[2] * LIGHTMAP_SHIFT;

		var_LightVec[2] = -UnpackNormal( -attr_LightVecs[2] );
		#if defined( STUDIO_BUMP ) || defined( STUDIO_INTERIOR )	
			var_LightVec[2] = var_LightVec[2] * tbn;
		#endif			
	}

	if( u_LightStyles[3] != 0.0 )
	{
		var_LightDiffuse[3] = UnpackVector( attr_LightColor[3] );
			
		gammaIndex = ( var_LightDiffuse[3].r * 255.0 );
		var_LightDiffuse[3].r = u_GammaTable[int( gammaIndex/4.0 )][int( mod( gammaIndex, 4.0 ))];
		gammaIndex = ( var_LightDiffuse[3].g * 255.0 );
		var_LightDiffuse[3].g = u_GammaTable[int( gammaIndex/4.0 )][int( mod( gammaIndex, 4.0 ))];
		gammaIndex = ( var_LightDiffuse[3].b * 255.0 );
		var_LightDiffuse[3].b = u_GammaTable[int( gammaIndex/4.0 )][int( mod( gammaIndex, 4.0 ))];
			
		var_LightDiffuse[3] *= u_LightStyles[3] * LIGHTMAP_SHIFT;

		var_LightVec[3] = -UnpackNormal( -attr_LightVecs[3] );
		#if defined( STUDIO_BUMP ) || defined( STUDIO_INTERIOR )	
			var_LightVec[3] = var_LightVec[3] * tbn;
		#endif			
	}
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
	#ifndef STUDIO_VERTEX_LIGHTING
		var_LightVec = -srcL * tbn * MeshScale;
	#endif
	var_ViewVec = srcV * tbn * MeshScale;
	var_Normal = srcN * tbn;
#else
	// leave in worldspace
	#ifndef STUDIO_VERTEX_LIGHTING	
		var_LightVec = -srcL * MeshScale;
	#endif		
	var_ViewVec = srcV * MeshScale;
	var_Normal = srcN;
#endif

	var_Distance = length( srcV * MeshScale );

#if defined( REFLECTION_CUBEMAP ) || defined( STUDIO_INTERIOR )
	var_Position = worldpos.xyz;
	var_MatrixTBN = tbn;
#endif
}