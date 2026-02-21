/*
deluxemap.h - directional lightmaps
Copyright (C) 2016 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef DELUXEMAP_H
#define DELUXEMAP_H

uniform sampler2D	u_LightMap;
#if !defined( BMODEL_WATER )
uniform sampler2D	u_DeluxeMap;
#endif

#if !defined( BMODEL_WATER )
vec3 deluxemap2D( sampler2D tex, const vec2 uv )
{
	vec3 deluxmap = texture2D( tex, uv ).xyz;
	return (( deluxmap - 0.5 ) * 2.0 );
}
#endif

void ApplyLightStyle( const vec3 lminfo, const vec3 N, const vec3 V, const vec3 glossmap, float GlossSmoothness, float GlossScale, inout vec3 light, inout vec3 gloss )
{
	vec4 lmsrc = texture2D( u_LightMap, lminfo.xy );
	vec3 lightmap = lmsrc.rgb;
#if defined( BMODEL_WATER )
	vec3 L = vec3( 1.0 );
#else
	vec3 deluxmap = deluxemap2D( u_DeluxeMap, lminfo.xy );
	vec3 L = normalize( deluxmap );
#endif

#if defined( BMODEL_BUMP )
        float NdotB = ComputeStaticBump( L, N );
	lightmap *= NdotB;
#endif
	light += ( lightmap ) * lminfo.z;

#if defined( BMODEL_SPECULAR )
	float NdotLGloss = saturate( dot( N, L ));
	vec3 specular = ComputeSpecular( N, V, L, glossmap, GlossSmoothness, GlossScale ) * lightmap * lminfo.z * lmsrc.a * NdotLGloss;
	gloss += specular;
#endif
}

#endif//DELUXEMAP_H