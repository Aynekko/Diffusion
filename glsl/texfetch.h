#ifndef TEXFETCH_H
#define TEXFETCH_H

#include "const.h"
#include "mathlib.h"

vec4 colormap2D( sampler2D tex, const vec2 uv )
{
	vec4 sample = texture2D( tex, uv );
	return vec4( ConvertSRGBToLinear( sample.rgb ), sample.a );
}

#if defined( GLSL_ALLOW_TEXTURE_ARRAY )
vec4 colormap2DArray( sampler2DArray tex, vec2 uv, float layer )
{
	vec4 sample = texture2DArray( tex, vec3( uv, layer ) );
	return vec4( ConvertSRGBToLinear( sample.rgb ), sample.a );
}
#endif

// get support for various normalmap encode
vec3 normalmap2D( sampler2D tex, const vec2 uv )
{
	vec4 normalmap = texture2D( tex, uv );
	vec3 N;

#if defined( NORMAL_AG_PARABOLOID )
	N.x = 2.0 * (normalmap.a - 0.5);
	N.y = 2.0 * (normalmap.g - 0.5);
	N.z = sqrt( 1.0 - saturate( dot( N.xy, N.xy ) ) );
#elif defined( NORMAL_RG_PARABOLOID )
	N.x = 2.0 * (normalmap.g - 0.5);
	N.y = 2.0 * (normalmap.r - 0.5);
	N.z = sqrt( 1.0 - saturate( dot( N.xy, N.xy ) ) );
#elif defined( NORMAL_3DC_PARABOLOID )
	N.x = -2.0 * (normalmap.g - 0.5);
	N.y = -2.0 * (normalmap.r - 0.5);
	N.z = sqrt( 1.0 - saturate( dot( N.xy, N.xy ) ) );
#else
	// uncompressed normals
	N = (normalmap.xyz * 255.0) / 127.0 - 1.0;
#endif
	N.y = -N.y; // !!!
	N = normalize( N );

	return N;
}

#if defined( GLSL_ALLOW_TEXTURE_ARRAY )
vec3 normalmap2DArray( sampler2DArray tex, const vec2 uv, const float layer )
{
	vec4 normalmap = texture2DArray( tex, vec3( uv, layer ) );
	vec3 N;

#if defined( NORMAL_AG_PARABOLOID )
	N.x = 2.0 * (normalmap.a - 0.5);
	N.y = 2.0 * (normalmap.g - 0.5);
	N.z = 1.0 - saturate( dot( N.xy, N.xy ) );
#elif defined( NORMAL_RG_PARABOLOID )
	N.x = 2.0 * (normalmap.g - 0.5);
	N.y = 2.0 * (normalmap.r - 0.5);
	N.z = 1.0 - saturate( dot( N.xy, N.xy ) );
#elif defined( NORMAL_3DC_PARABOLOID )
	N.x = -2.0 * (normalmap.g - 0.5);
	N.y = -2.0 * (normalmap.r - 0.5);
	N.z = 1.0 - saturate( dot( N.xy, N.xy ) );
#else
	// uncompressed normals
	N = (2.0 * (normalmap.xyz - 0.5));
#endif
	N.y = -N.y; // !!!
	N = normalize( N );

	return N;
}
#endif

vec4 reflectmap2D( sampler2D tex, vec4 projTC, vec3 N, vec3 fragCoord, float refraction )
{
#if defined( BMODEL_WATER )
	projTC.x += N.x * refraction * fragCoord.x * 0.35;
	projTC.y -= N.y * refraction * fragCoord.y * 0.35;
	projTC.z += N.z * refraction * fragCoord.x * 0.35;
#else
	projTC.x += N.x * refraction * 2.0;
	projTC.y -= N.y * refraction * 2.0;
	projTC.z += N.z * refraction * 2.0;
#endif
	return texture2DProj( tex, projTC );
}

vec4 cubic( float x )
{
	float x2 = x * x;
	const mat4 mat = mat4( vec4( -0.166667, 0.5, -0.5, 0.166667 ),
		vec4( 0.5, -1.0, 0.5, 0.0 ),
		vec4( -0.5, 0.0, 0.5, 0.0 ),
		vec4( 0.166667, 0.666667, 0.166667, 0.0 ) );

	vec4 xx = vec4( x2 * x, x2, x, 1.0 );

	return  mat * xx;
}

vec4 textureBicubic( sampler2D sampler, vec2 texCoords, float LOD, vec2 texSize )
{
	vec2 pixelSize = vec2( 1.0 ) / texSize;

	texCoords = texCoords * texSize - vec2( 0.5 );

	vec2 fxy = fract( texCoords );
	texCoords -= fxy;

	vec4 xcubic = cubic( fxy.x );
	vec4 ycubic = cubic( fxy.y );

	vec4 c = texCoords.xxyy + vec2( -0.5, 1.5 ).xyxy;

	vec4 s = vec4( xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw );
	vec4 offset = c + vec4( xcubic.yw, ycubic.yw ) / s;

	offset *= pixelSize.xxyy;

	vec4 sample0 = texture2DLod( sampler, offset.xz, LOD );
	vec4 sample1 = texture2DLod( sampler, offset.yz, LOD );
	vec4 sample2 = texture2DLod( sampler, offset.xw, LOD );
	vec4 sample3 = texture2DLod( sampler, offset.yw, LOD );

	float sx = s.x / (s.x + s.y);
	float sy = s.z / (s.z + s.w);

	return mix( mix( sample3, sample2, sx ), mix( sample1, sample0, sx ), sy );
}

vec3 RGBtoHSV( in vec3 RGB )
{
	vec4 P = (RGB.g < RGB.b) ? vec4( RGB.bg, -1.0, 2.0 / 3.0 ) : vec4( RGB.gb, 0.0, -1.0 / 3.0 );
	vec4 Q = (RGB.r < P.x) ? vec4( P.xyw, RGB.r ) : vec4( RGB.r, P.yzx );
	float C = Q.x - min( Q.w, Q.y );
	float H = abs( (Q.w - Q.y) / (6.0 * C + Epsilon) + Q.z );
	vec3 HCV = vec3( H, C, Q.x );
	float S = HCV.y / (HCV.z + Epsilon);
	return vec3( HCV.x, S, HCV.z );
}

vec3 HSVtoRGB( in vec3 HSV )
{
	float H = HSV.x;
	float R = abs( H * 6.0 - 3.0 ) - 1.0;
	float G = 2.0 - abs( H * 6.0 - 2.0 );
	float B = 2.0 - abs( H * 6.0 - 4.0 );
	vec3 RGB = clamp( vec3( R, G, B ), 0.0, 1.0 );
	return((RGB - 1.0) * HSV.y + 1.0) * HSV.z;
}

vec4 sharpen( in sampler2D tex, in vec2 coords, in vec2 ScreenSizeInv )
{
	float dx = ScreenSizeInv.x;
	float dy = ScreenSizeInv.y;
	vec4 sum = vec4( 0.0 );
	sum -= texture2DLod( tex, coords + vec2( -1.0 * dx, 0.0 * dy ), 0.0 );
	sum -= texture2DLod( tex, coords + vec2( 0.0 * dx, -1.0 * dy ), 0.0 );
	sum += 5. * texture2DLod( tex, coords + vec2( 0.0 * dx, 0.0 * dy ), 0.0 );
	sum -= texture2DLod( tex, coords + vec2( 0.0 * dx, 1.0 * dy ), 0.0 );
	sum -= texture2DLod( tex, coords + vec2( 1.0 * dx, 0.0 * dy ), 0.0 );
	return sum;
}

#endif//TEXFETCH_H