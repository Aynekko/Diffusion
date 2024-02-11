/*
tonemap_fp.glsl - tone mapping for HDRL
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

uniform sampler2D	u_ScreenMap;
uniform sampler2D	u_HDRExposure;
uniform float		u_GenericCondition;

varying vec2		var_TexCoord;

void main( void )
{
    vec4 color = texture2DLod( u_ScreenMap, var_TexCoord, 0.0 );

    float exposure = texture2D( u_HDRExposure, vec2( 0.5 )).r;
    float gamma = 1.5 * exposure;

	if( bool( u_GenericCondition == 1.0f ))
	{
		const vec2 lum = vec2( 0.7, 0.7 );   // fixed constant instead of adaptation curve
        float Lp = ( 8.0 / lum.x ) * max( color.x, max( color.y, color.z ));
        float LmSqr = ( lum.y + 8.0 * lum.y ) * ( lum.y + 8.0 * lum.y );
        float toneScalar = ( Lp * ( 1.0 + ( Lp / ( LmSqr )))) / ( 1.0 + Lp );
		color *= toneScalar;
	}

    gl_FragColor = color * gamma;
}