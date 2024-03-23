/*
bilateral_fp.glsl - bilateral filter
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

#include "const.h"
#include "mathlib.h"

uniform sampler2D	u_ScreenMap;
uniform sampler2D	u_DepthMap;
uniform float		u_zFar;

varying vec3		var_Position;
varying vec2		var_TexCoord;
varying vec4		var_TexCoords[4];

float AddSampleContribution( in vec2 texCoord, in float depth, in float SampleContribution, inout vec4 color )
{
	// Load the depth from the depth map, transform the depth into eye space
	float sampleDepth = texture2D( u_DepthMap, texCoord ).r;
	sampleDepth = linearizeDepth( u_zFar, RemapVal( sampleDepth, 0.0, 0.8, 0.0, 1.0 ));

	// Check for depth discontinuities
	if( abs( depth - sampleDepth ) > 5.0 )
		return 0.0;

	//float SampleMulti = clamp( 1.0 - abs( depth - sampleDepth ) * 0.15, 0.0, 1.0 );

	// Sample the color map and add its contribution
	color += texture2D( u_ScreenMap, texCoord ) * SampleContribution;

	return SampleContribution;
}

void main( void )
{
	float depth = texture2D( u_DepthMap, var_TexCoord ).r;
	depth = linearizeDepth( u_zFar, RemapVal( depth, 0.0, 0.8, 0.0, 1.0 ));

	// Blur using a 9x9 bilateral Gaussian filter
	float weightSum = 0.153170;
	vec4 color = texture2D( u_ScreenMap, var_TexCoord ) * weightSum;

	weightSum += AddSampleContribution( var_TexCoords[0].st, depth, 0.144893, color );
	weightSum += AddSampleContribution( var_TexCoords[0].pq, depth, 0.144893, color );

	weightSum += AddSampleContribution( var_TexCoords[1].st, depth, 0.122649, color );
	weightSum += AddSampleContribution( var_TexCoords[1].pq, depth, 0.122649, color );

	weightSum += AddSampleContribution( var_TexCoords[2].st, depth, 0.092903, color );
	weightSum += AddSampleContribution( var_TexCoords[2].pq, depth, 0.092903, color );

	weightSum += AddSampleContribution( var_TexCoords[3].st, depth, 0.062970, color );
	weightSum += AddSampleContribution( var_TexCoords[3].pq, depth, 0.062970, color );

	// Write the final color
	gl_FragColor = color / weightSum;
}