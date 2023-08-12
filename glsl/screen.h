#ifndef SCREEN_H
#define SCREEN_H

uniform sampler2D	u_ScreenMap;
uniform vec2		u_ScreenSizeInv;

vec2 GetDistortedTexCoords( in vec3 N, float fade, float RefractScale )
{
	vec2 screenCoord = gl_FragCoord.xy * u_ScreenSizeInv;
	screenCoord.x += N.x * RefractScale * screenCoord.x * fade;
	screenCoord.y += N.y * RefractScale * screenCoord.y * fade;
	screenCoord.x = clamp( screenCoord.x, 0.002, 0.998 );
	screenCoord.y = clamp( screenCoord.y, 0.002, 0.998 );
        return screenCoord;
}

vec3 GetScreenColorForRefraction( in vec3 N, float fade, float RefractScale )
{
	vec2 screenCoord = GetDistortedTexCoords( N, fade, RefractScale );
	return texture2D( u_ScreenMap, screenCoord ).rgb;
}


#endif//SCREEN_H