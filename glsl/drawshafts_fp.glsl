/*
drawshafts_fp.glsl - render sun shafts
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

uniform sampler2D	u_ScreenMap;
uniform sampler2D	u_ColorMap;
uniform vec4		u_Sunshafts[2];
#define u_LightOrigin	u_Sunshafts[0].xyz
#define u_Brightness	u_Sunshafts[0].w
#define u_LightDiffuse	u_Sunshafts[1].xyz

varying vec2       	var_TexCoord;

vec3 blendSoftLight( const vec3 a, const vec3 b )
{
	vec3 c = 2.0 * a * b * ( 1.0 + a * (  1.0 - b ));
 
	vec3 a_sqrt = sqrt( a );
	vec3 d = ( a  +  b * ( a_sqrt - a )) * 2.0 - a_sqrt;

	return( length( b ) < 0.5 ) ? c : d;
}

void main( void )
{
	vec2 sunVec = saturate( u_LightOrigin.xy ) - var_TexCoord;
	vec2 tc = var_TexCoord;

	float sunDist = saturate( u_LightOrigin.z ) * saturate( 1.0 - saturate( length( sunVec ) * 0.2 ));
	sunVec *= 0.1 * u_LightOrigin.z;

	float Brightness = u_Brightness;
	if( Brightness <= 0 ) Brightness = 1.0;

	vec4 accum = texture2D( u_ColorMap, tc );
	float fShaftsMask = saturate( 1.00001 - accum.w );
	float fBlend = accum.w * Brightness;	// shaft brightness

	/*
	// original
	const int Steps = 32;
	const float step = 8.0 / Steps;
	for( int i = 1; i <= Steps; i++ )
	{	
		accum += texture2D( u_ColorMap, tc + sunVec.xy * (0.5 + step * i) ) * (1.0 - (0.5 + step * i) / 8.0);
	}
	accum /= Steps;
	*/

	// 28 steps (unrolled, optimized)
	accum += texture2D( u_ColorMap, sunVec.xy * 0.75 + tc ) * 0.90625;
	accum += texture2D( u_ColorMap, sunVec.xy * 1.00 + tc ) * 0.87500;
	accum += texture2D( u_ColorMap, sunVec.xy * 1.25 + tc ) * 0.84375;
	accum += texture2D( u_ColorMap, sunVec.xy * 1.50 + tc ) * 0.81250;
	accum += texture2D( u_ColorMap, sunVec.xy * 1.75 + tc ) * 0.78125;
	accum += texture2D( u_ColorMap, sunVec.xy * 2.00 + tc ) * 0.75000;
	accum += texture2D( u_ColorMap, sunVec.xy * 2.25 + tc ) * 0.71875;
	accum += texture2D( u_ColorMap, sunVec.xy * 2.50 + tc ) * 0.68750;
	accum += texture2D( u_ColorMap, sunVec.xy * 2.75 + tc ) * 0.65625;
	accum += texture2D( u_ColorMap, sunVec.xy * 3.00 + tc ) * 0.62500;
	accum += texture2D( u_ColorMap, sunVec.xy * 3.25 + tc ) * 0.59375;
	accum += texture2D( u_ColorMap, sunVec.xy * 3.50 + tc ) * 0.56250;
	accum += texture2D( u_ColorMap, sunVec.xy * 3.75 + tc ) * 0.53125;
	accum += texture2D( u_ColorMap, sunVec.xy * 4.00 + tc ) * 0.50000;
	accum += texture2D( u_ColorMap, sunVec.xy * 4.25 + tc ) * 0.46875;
	accum += texture2D( u_ColorMap, sunVec.xy * 4.50 + tc ) * 0.43750;
	accum += texture2D( u_ColorMap, sunVec.xy * 4.75 + tc ) * 0.40625;
	accum += texture2D( u_ColorMap, sunVec.xy * 5.00 + tc ) * 0.37500;
	accum += texture2D( u_ColorMap, sunVec.xy * 5.25 + tc ) * 0.34375;
	accum += texture2D( u_ColorMap, sunVec.xy * 5.50 + tc ) * 0.31250;
	accum += texture2D( u_ColorMap, sunVec.xy * 5.75 + tc ) * 0.28125;
	accum += texture2D( u_ColorMap, sunVec.xy * 6.00 + tc ) * 0.25000;
	accum += texture2D( u_ColorMap, sunVec.xy * 6.25 + tc ) * 0.21875;
	accum += texture2D( u_ColorMap, sunVec.xy * 6.50 + tc ) * 0.18750;
	accum += texture2D( u_ColorMap, sunVec.xy * 6.75 + tc ) * 0.15625;
	accum += texture2D( u_ColorMap, sunVec.xy * 7.00 + tc ) * 0.12500;
	accum += texture2D( u_ColorMap, sunVec.xy * 7.25 + tc ) * 0.09375;
	accum += texture2D( u_ColorMap, sunVec.xy * 7.50 + tc ) * 0.06250;
	accum /= 28;

 	accum *= 2.0 * vec4( sunDist, sunDist, sunDist, 1.0 );
	
 	vec3 cScreen = texture2D( u_ScreenMap, var_TexCoord ).rgb;      
	vec3 sunColor = u_LightDiffuse;

	vec3 outColor = cScreen + accum.rgb * fBlend * sunColor * ( 1.0 - cScreen );
	outColor = blendSoftLight( outColor, sunColor * fShaftsMask * 0.5 + 0.5 );

	gl_FragColor = vec4( outColor, 1.0 );
}