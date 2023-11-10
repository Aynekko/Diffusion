/*
cubemap.h - handle reflection probes
Copyright (C) 2019 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef CUBEMAP_H
#define CUBEMAP_H

uniform samplerCube	u_EnvMap0;
uniform samplerCube	u_EnvMap1;

uniform vec3			u_Cubemap[7];
#define BoxMins0		u_Cubemap[0]
#define BoxMins1		u_Cubemap[1]
#define BoxMaxs0		u_Cubemap[2]
#define BoxMaxs1		u_Cubemap[3]
#define CubeOrigin0		u_Cubemap[4]
#define CubeOrigin1		u_Cubemap[5]
#define CubeMipCount0	u_Cubemap[6].x
#define CubeMipCount1	u_Cubemap[6].y
#define LerpFactor		u_Cubemap[6].z

vec3 CubemapBoxParallaxCorrected( const vec3 vReflVec, vec3 vPosition, const vec3 vCubePos, const vec3 vBoxExtentsMin, const vec3 vBoxExtentsMax )
{
	// parallax correction for local cubemaps using a box as geometry proxy
//	vec3 cubeCenter = ( vBoxExtentsMin + vBoxExtentsMax ) * 0.5;
//	vec3 cubeExtent = vBoxExtentsMax - cubeCenter;
//	vPosition = clamp( vPosition, cubeCenter - cubeExtent * 0.8 , cubeCenter + cubeExtent * 0.8 );

	// min/max intersection
	vec3 vBoxIntersectionMax = (vBoxExtentsMax - vPosition) / vReflVec;
	vec3 vBoxIntersectionMin = (vBoxExtentsMin - vPosition) / vReflVec;

	vec3 vFurthestPlane;

	// intersection test
	vFurthestPlane.x = (vReflVec.x > 0.0) ? vBoxIntersectionMax.x : vBoxIntersectionMin.x;
	vFurthestPlane.y = (vReflVec.y > 0.0) ? vBoxIntersectionMax.y : vBoxIntersectionMin.y;
	vFurthestPlane.z = (vReflVec.z > 0.0) ? vBoxIntersectionMax.z : vBoxIntersectionMin.z;
	float fDistance = min( min( vFurthestPlane.x, vFurthestPlane.y ), vFurthestPlane.z );

	// apply new reflection position
	vec3 vInterectionPos = vPosition + vReflVec * fDistance;
	return (vInterectionPos - vCubePos);
}

vec3 GetReflectionProbe( const vec3 vPos, const vec3 vView, const vec3 nWorld, const vec3 glossmap, const float smoothness )
{
	vec3 I = normalize( vPos - vView ); // in world space
	vec3 NW = normalize( nWorld );
	vec3 wRef = normalize( reflect( I, NW ) );
	vec3 R1 = CubemapBoxParallaxCorrected( wRef, vPos, CubeOrigin0, BoxMins0, BoxMaxs0 );
	vec3 R2 = CubemapBoxParallaxCorrected( wRef, vPos, CubeOrigin1, BoxMins1, BoxMaxs1 );
	vec3 srcColor0 = textureCubeLod( u_EnvMap0, R1, CubeMipCount0 - smoothness * CubeMipCount0 ).rgb;
	vec3 srcColor1 = textureCubeLod( u_EnvMap1, R2, CubeMipCount1 - smoothness * CubeMipCount1 ).rgb;
	vec3 reflectance = mix( srcColor0, srcColor1, LerpFactor );

	return reflectance;
}

vec3 CubemapReflectionProbe( const vec3 vPos, const vec3 wRef, const vec3 glossmap )
{
	vec3 R1 = CubemapBoxParallaxCorrected( wRef, vPos, CubeOrigin0, BoxMins0, BoxMaxs0 );
	vec3 R2 = CubemapBoxParallaxCorrected( wRef, vPos, CubeOrigin1, BoxMins1, BoxMaxs1 );
	vec3 srcColor0 = textureCubeLod( u_EnvMap0, R1, glossmap.r * (CubeMipCount0 - 2.0) ).rgb;
	vec3 srcColor1 = textureCubeLod( u_EnvMap1, R2, glossmap.r * (CubeMipCount1 - 2.0) ).rgb;
	vec3 CubemapReflection = mix( srcColor0, srcColor1, LerpFactor );

	return CubemapReflection;
}

#endif//CUBEMAP_H