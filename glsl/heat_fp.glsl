// Shader by Sleicreider: https://www.shadertoy.com/view/XsVSRd
// textureless edition by Peace: https://www.shadertoy.com/user/Peace
// modified by Aynekko

#include "const.h"
#include "mathlib.h"

uniform sampler2D u_DepthMap;
uniform sampler2D u_ColorMap;
uniform vec4 u_GenericCondition;
#define u_ScreenSize	u_GenericCondition.xy
#define u_RealTime		u_GenericCondition.z
#define u_Scale			u_GenericCondition.w

varying vec2 var_TexCoord;
varying vec3 var_VertexColor;

float rand(vec2 co)
{
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

float noise(vec2 n) 
{
	const vec2 d = vec2(0.0, 1.0);
	vec2 b = floor(n), f = smoothstep(vec2(0.0), vec2(1.0), fract(n));
	return mix(mix(rand(b), rand(b + d.yx), f.x), mix(rand(b + d.xy), rand(b + d.yy), f.x), f.y);
}

void main( void )
{
	float fSampledDepth = texture2D( u_DepthMap, var_TexCoord ).r;
	fSampledDepth = linearizeDepth( 4096, fSampledDepth );
	fSampledDepth = RemapVal( fSampledDepth, Z_NEAR, 4096, 0.0, 1.0 );
	
	vec2 p_m = gl_FragCoord.xy / u_ScreenSize;
	vec2 dst_offset = vec2(0.0);

	if( fSampledDepth > 0.5 )
	{
		vec2 p_d = p_m;

		p_d.y -= u_RealTime * 0.025 * ( 1.0 / fSampledDepth );
    
		vec4 dst_map_val = vec4( noise(p_d * vec2(50)) * 0.25 * u_Scale );
    
		dst_offset = dst_map_val.xy;
		dst_offset *= 2.;
		dst_offset *= 0.01;
	
		// reduce effect towards Y top
		dst_offset *= (1. - p_m.y);
	}
    
	vec2 dist_tex_coord = p_m.xy + dst_offset;
	vec4 original = texture2D(u_ColorMap, var_TexCoord);
	vec4 distorted = texture2D(u_ColorMap, dist_tex_coord);
	gl_FragColor = vec4( mix(original.rgb, distorted.rgb, fSampledDepth * u_Scale), 1.0 );
}