/*
sun_shadow.h - directional sun shadow sampling for the baked lightmap pass
Copyright (C) 2026 rickomax

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef SUN_SHADOW_H
#define SUN_SHADOW_H

uniform sampler2DShadow	u_SunShadowMap;
uniform vec4		u_SunShadowParams;	// x = texel size, y = shadow strength, z = cascade far distance, w = fade band width

// sample the directional shadow map, returns 1.0 fully lit and (1 - strength) fully shadowed.
float SunShadowValue( vec4 projection, float viewDist )
{
	vec3 coord = projection.xyz / projection.w;

	// outside the cascade footprint means no dynamic shadow, keep it lit
	if( coord.x < 0.0 || coord.x > 1.0 || coord.y < 0.0 || coord.y > 1.0 || coord.z > 1.0 )
		return 1.0;

	vec2 texel = vec2( u_SunShadowParams.x );
	float rotation = InterleavedGradientNoise( gl_FragCoord.xy ) * 6.2832;
	float shadow = 0.0;

	for( int i = 0; i < 8; i++ )
	{
		vec2 offset = texel * VogelDiskSample( i, 8, rotation ) * 2.0;
		shadow += shadow2D( u_SunShadowMap, vec3( coord.xy + offset, coord.z )).r;
	}
	shadow *= 0.125;

	// let the shadow only darken down to the intensity floor
	shadow = max( shadow, 1.0 - u_SunShadowParams.y );

	// fade the dynamic term back to lit toward the cascade edge so the far range keeps the baked mask
	float fade = saturate(( viewDist - ( u_SunShadowParams.z - u_SunShadowParams.w )) / max( u_SunShadowParams.w, 1.0 ));
	return mix( shadow, 1.0, fade );
}

#endif//SUN_SHADOW_H
