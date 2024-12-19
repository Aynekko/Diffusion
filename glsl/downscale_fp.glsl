#include "const.h"
#include "mathlib.h"
#include "texfetch.h"

uniform sampler2D   u_ColorMap;
uniform vec2		u_ScreenSizeInv;
uniform float		u_GenericCondition;

varying vec2        var_TexCoord;

vec4 textureBicubic(sampler2D sampler, vec2 texCoords)
{
   vec2 texSize = textureSize(u_ColorMap, 0);
   vec2 invTexSize = 1.0 / texSize;
   
   texCoords = texCoords * texSize - 0.5;
   
	vec2 fxy = fract(texCoords);
	texCoords -= fxy;

	vec4 xcubic = cubic(fxy.x);
	vec4 ycubic = cubic(fxy.y);

	vec4 c = texCoords.xxyy + vec2 (-0.5, +1.5).xyxy;

	vec4 s = vec4(xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw);
	vec4 offset = c + vec4 (xcubic.yw, ycubic.yw) / s;

	offset *= invTexSize.xxyy;

	vec4 sample0 = texture(sampler, offset.xz);
	vec4 sample1 = texture(sampler, offset.yz);
	vec4 sample2 = texture(sampler, offset.xw);
	vec4 sample3 = texture(sampler, offset.yw);

	float sx = s.x / (s.x + s.y);
	float sy = s.z / (s.z + s.w);

	return mix(
		mix(sample3, sample2, sx), mix(sample1, sample0, sx)
	, sy);
}

void main( void )
{
#if 0 // original
	vec4 color = texture2D( u_ColorMap, var_TexCoord, 0.0 );
	gl_FragColor = color;
#endif
	
#if 1 // bicubic filter + sharpen
	vec4 filter = textureBicubic( u_ColorMap, var_TexCoord );
	vec4 sharpen = sharpen( u_ColorMap, var_TexCoord, u_ScreenSizeInv );

	gl_FragColor = vec4( mix( filter.rgb, sharpen.rgb, u_GenericCondition ), 1.0 );
#endif
}