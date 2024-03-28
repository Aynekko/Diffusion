/*
mathlib.h - math subroutines for GLSL
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

#ifndef MATHLIB_H
#define MATHLIB_H

#ifndef saturate
#define saturate( x )	clamp( x, 0.0, 1.0 )
#endif

#define PLANE_S		vec4( 1.0, 0.0, 0.0, 0.0 )
#define PLANE_T		vec4( 0.0, 1.0, 0.0, 0.0 )
#define PLANE_R		vec4( 0.0, 0.0, 1.0, 0.0 )
#define PLANE_Q		vec4( 0.0, 0.0, 0.0, 1.0 )

#define sqr( x )		( x * x )
#define rcp( x )		( 1.0 / x )

#define UnpackVector( c )	fract( c * vec3( 1.0, 256.0, 65536.0 ))
#define UnpackNormal( c )	(( fract( c * vec3( 1.0, 256.0, 65536.0 )) - 0.5 ) * 2.0 )

#define Q_mix( A, B, frac )	( A + ( B - A ) * frac )

#define RAD2DEG( x )	( x * (180.0 / M_PI))
#define DEG2RAD( x )	( x * (M_PI / 180.0))

// remap a value in the range [A,B] to [C,D].
float RemapVal( float val, const in vec4 bounds )
{
	return bounds.z + (bounds.w - bounds.z) * (val - bounds.x) / (bounds.y - bounds.x);
}

// remap a value in the range [A,B] to [C,D].
float RemapVal( float val, float A, float B, float C, float D )
{
	return C + (D - C) * (val - A) / (B - A);
}

float GetLuminance( vec3 color )
{
	return dot( color, vec3( 0.2126, 0.7152, 0.0722 ) );
}

float GetFresnel( const vec3 V, const vec3 N, float fresnelExp, float scale )
{
	return 0.001 + pow( 1.0 - max( dot( N, V ), 0.0 ), fresnelExp ) * scale;
}

float linearizeDepth( float zfar, float depth )
{
	//	return 2.0 * Z_NEAR * zfar / ( zfar + Z_NEAR - ( 2.0 * depth - 1.0 ) * ( zfar - Z_NEAR ));
	return -zfar * Z_NEAR / (depth * (zfar - Z_NEAR) - zfar);
}

float linearizeDepth( float depth, float znear, float zfar )
{
	return zfar * znear / (depth * (zfar - znear) - zfar);
}

float ComputeStaticBump( const vec3 L, const vec3 N )
{
	vec3 srcB = L;
	const float ambientClip = 0.1;

	// remap static bump to positive range
	srcB.z = RemapVal( srcB.z, -1.0, 1.0, ambientClip, 1.0 );
	vec3 B = normalize( srcB );

	return saturate( dot( N, B ) );
}

vec3 ConvertSRGBToLinear( vec3 color )
{
#if 1
	vec3 linearRGBLo = color / 12.92;
	vec3 linearRGBHi = pow( (color + 0.055) / 1.055, vec3( 2.4 ) );
	vec3 linearRGB = mix( linearRGBLo, linearRGBHi, step( 0.04045, color ) );
	return linearRGB;
#else
	return pow( color, vec3( 2.2 ) );
#endif
}

vec3 ConvertLinearToSRGB( vec3 color )
{
#if 1
	vec3 sRGBLo = color * 12.92;
	vec3 sRGBHi = (pow( abs( color ), vec3( 1.0 / 2.4 ) ) * 1.055) - 0.055;
	vec3 sRGB = mix( sRGBLo, sRGBHi, step( 0.0031308, color ) );
	return sRGB;
#else
	return pow( color, vec3( 1.0 / 2.2 ) );
#endif
}

vec2 GetTexCoordsForVertex( int vertexNum )
{
	if( vertexNum == 0 || vertexNum == 4 || vertexNum == 8 || vertexNum == 12 )
		return vec2( 0.0, 1.0 );
	if( vertexNum == 1 || vertexNum == 5 || vertexNum == 9 || vertexNum == 13 )
		return vec2( 0.0, 0.0 );
	if( vertexNum == 2 || vertexNum == 6 || vertexNum == 10 || vertexNum == 14 )
		return vec2( 1.0, 0.0 );
	return vec2( 1.0, 1.0 );
}

vec3 GetNormalForVertex( int vertexNum )
{
	if( vertexNum == 0 || vertexNum == 1 || vertexNum == 2 || vertexNum == 3 )
		return vec3( 0.0, 1.0, 0.0 );
	if( vertexNum == 4 || vertexNum == 5 || vertexNum == 6 || vertexNum == 7 )
		return vec3( -1.0, 0.0, 0.0 );
	if( vertexNum == 8 || vertexNum == 9 || vertexNum == 10 || vertexNum == 11 )
		return vec3( -0.707107, 0.707107, 0.0 );
	return vec3( 0.707107, 0.707107, 0.0 );
}

void MakeNormalVectors( const vec3 forward, inout vec3 right, inout vec3 up )
{
	// this rotate and negate guarantees a vector not colinear with the original
	right = vec3( forward.z, -forward.x, forward.y );
	right = normalize( right - forward * dot( right, forward ) );
	up = cross( right, forward );
}

vec2 VogelDiskSample( int sampleIndex, int samplesCount, float phi )
{
	const float goldenAngle = 2.4;
	float r = sqrt( float( sampleIndex ) + 0.5 ) * inversesqrt( float( samplesCount ) );
	float theta = sampleIndex * goldenAngle + phi;
	return r * vec2( cos( theta ), sin( theta ) );
}

float InterleavedGradientNoise( vec2 position_screen )
{
	const vec3 magic = vec3( 0.06711056, 0.00583715, 52.9829189 );
	return fract( magic.z * fract( dot( position_screen, magic.xy ) ) );
}

float GetShadowOffset( float NdotL )
{
	float bias = 0.0005 * tan( acos( saturate( NdotL ) ) );
	return clamp( bias, 0.0, 0.005 );
}

float randomFunction( in float seed, in vec2 uv )
{
	return fract( sin( dot( uv, vec2( 12.9898, 78.233 ) * seed ) ) * 43758.5453 );
}

float randomFunction2( in float seed, in vec2 co )
{
	float dt = dot( co.xy, vec2( 12.9898, 8.233 ) * seed );
	float sn = mod( dt, 3.14 );

	return fract( sin( sn ) * 43758.5453 );
}

float min3( vec3 v )
{
	return min( min( v.x, v.y ), v.z );
}

vec3 VectorRotate( vec3 In, vec3 Rotation )
{
	vec3 ang = Rotation; // entity angles

	vec3 RotatedVec = In;

	float angX = ang.x * 0.0175 + M_PI;
	float angY = ang.y * 0.0175 + M_PI;
	float angZ = ang.z * 0.0175 + M_PI;

	mat3 m3;
	m3[0][0] = RotatedVec.x;
	m3[1][0] = 0;
	m3[2][0] = 0;
	m3[0][1] = 0;
	m3[1][1] = -RotatedVec.y * cos( angZ );
	m3[2][1] = RotatedVec.z * sin( angZ );
	m3[0][2] = 0;
	m3[1][2] = -RotatedVec.y * sin( angZ );
	m3[2][2] = -RotatedVec.z * cos( angZ );

	RotatedVec = m3 * vec3( 1.0 );

	mat3 m2;
	m2[0][0] = -RotatedVec.x * cos( angX );
	m2[1][0] = 0;
	m2[2][0] = -RotatedVec.z * sin( angX );
	m2[0][1] = 0;
	m2[1][1] = RotatedVec.y;
	m2[2][1] = 0;
	m2[0][2] = RotatedVec.x * sin( angX );
	m2[1][2] = 0;
	m2[2][2] = -RotatedVec.z * cos( angX );

	RotatedVec = m2 * vec3( 1.0 );

	mat3 m;
	m[0][0] = -RotatedVec.x * cos( angY );
	m[1][0] = RotatedVec.y * sin( angY );
	m[2][0] = 0;
	m[0][1] = -RotatedVec.x * sin( angY );
	m[1][1] = -RotatedVec.y * cos( angY );
	m[2][1] = 0;
	m[0][2] = 0;
	m[1][2] = 0;
	m[2][2] = RotatedVec.z;

	RotatedVec = m * vec3( 1.0 );

	return RotatedVec;
}

vec3 VectorAngles( vec3 vecSrc )
{
	vec3 forward = vecSrc;

	vec3 ang;

	ang.z = 0.0f;

	if( bool( forward.x != 0.0f ) || bool( forward.y != 0.0f ) )
	{
		float tmp;

		ang.y = RAD2DEG( atan( forward.y, forward.x ) );
		if( ang.y < 0 ) ang.y += 360;

		tmp = sqrt( forward.x * forward.x + forward.y * forward.y );
		ang.x = RAD2DEG( atan( -forward.z, tmp ) );
		if( ang.x < 0 ) ang.x += 360;
	}
	else
	{
		// fast case
		ang.y = 0.0f;
		if( forward.z > 0 )
			ang.x = 270.0f;
		else ang.x = 90.0f;
	}

	return ang;
}

vec3 ForwardFromAngles( vec3 angles )
{
	vec3 forward;

	float cp = cos( DEG2RAD( angles.x ) );
	float cy = cos( DEG2RAD( angles.y ) );
	float sy = sin( DEG2RAD( angles.y ) );
	float sp = sin( DEG2RAD( angles.x ) );

	forward.x = cp * cy;
	forward.y = cp * sy;
	forward.z = -sp;

	return forward;
}

#endif//MATHLIB_H