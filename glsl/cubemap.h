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

uniform samplerCube	u_CubemapBox;

uniform vec3		u_Cubemap[3];
#define BoxMins		u_Cubemap[0]
#define BoxMaxs		u_Cubemap[1]
#define CubeOrigin	u_Cubemap[2]

vec3 CubemapBoxParallaxCorrected( const vec3 vReflVec, vec3 vPosition, const vec3 vCubePos, const vec3 vBoxExtentsMin, const vec3 vBoxExtentsMax )
{
	// parallax correction for local cubemaps using a box as geometry proxy
//	vec3 cubeCenter = ( vBoxExtentsMin + vBoxExtentsMax ) * 0.5;
//	vec3 cubeExtent = vBoxExtentsMax - cubeCenter;
//	vPosition = clamp( vPosition, cubeCenter - cubeExtent * 0.8 , cubeCenter + cubeExtent * 0.8 );

	// min/max intersection
	const float epsilon = 0.0001;
	vec3 invRefl = 1.0 / (vReflVec + sign(vReflVec) * epsilon);
	vec3 vBoxIntersectionMax = (vBoxExtentsMax - vPosition) * invRefl;
	vec3 vBoxIntersectionMin = (vBoxExtentsMin - vPosition) * invRefl;

	vec3 vFurthestPlane;

	// intersection test
	vFurthestPlane.x = (vReflVec.x > 0.0) ? vBoxIntersectionMax.x : vBoxIntersectionMin.x;
	vFurthestPlane.y = (vReflVec.y > 0.0) ? vBoxIntersectionMax.y : vBoxIntersectionMin.y;
	vFurthestPlane.z = (vReflVec.z > 0.0) ? vBoxIntersectionMax.z : vBoxIntersectionMin.z;
	float fDistance = min( min( vFurthestPlane.x, vFurthestPlane.y ), vFurthestPlane.z );

	// apply new reflection position
	vec3 vIntersectionPos = vPosition + vReflVec * fDistance;
	return (vIntersectionPos - vCubePos);
}

vec3 GetReflectionProbe( const vec3 vPos, const vec3 vView, const vec3 nWorld, const float CubeLod )
{
	vec3 I = normalize( vPos - vView ); // in world space
	vec3 NW = normalize( nWorld );
	vec3 wRef = normalize( reflect( I, NW ) );

	vec3 R = CubemapBoxParallaxCorrected( wRef, vPos, CubeOrigin, BoxMins, BoxMaxs );
	vec3 Cubemap = textureCubeLod( u_CubemapBox, R, CubeLod ).rgb;

	return Cubemap;
}

vec3 CubemapReflectionProbe( const vec3 vPos, const vec3 wRef, const float CubeLod )
{
	vec3 R = CubemapBoxParallaxCorrected( wRef, vPos, CubeOrigin, BoxMins, BoxMaxs );
	vec3 Cubemap = textureCubeLod( u_CubemapBox, R, CubeLod ).rgb;

	return Cubemap;
}

#endif//CUBEMAP_H