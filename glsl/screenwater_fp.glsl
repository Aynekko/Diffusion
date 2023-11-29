// thanks to ncuxonaT and SNMetamorph for help

uniform sampler2D u_ColorMap;  // screen map
uniform sampler2D u_ScreenMap; // distortion map
uniform vec4 u_ScreenWater;
#define u_ScreenSizeInv u_ScreenWater.xy
#define u_RealTime		u_ScreenWater.z
#define u_Speed			u_ScreenWater.w

void main( void )
{
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = gl_FragCoord.xy * u_ScreenSizeInv;
	float Speed = u_Speed;
    vec2 distortionVec = texture2D( u_ScreenMap, fract(Speed * 0.05 * uv.xy + u_RealTime * vec2(0.0, 0.125)) ).rg;
    vec2 uvShift = (2.0 * distortionVec - vec2(1.0));

    float scaleX = min(1.0, smoothstep(0.0, 0.05, uv.x) - smoothstep(0.95, 1.0, uv.x)); 
    float scaleY = min(1.0, smoothstep(0.0, 0.05, uv.y) - smoothstep(0.95, 1.0, uv.y));

    // scale shift here
	uv += 0.05 * uvShift * scaleX * scaleY;

    // Time varying pixel color
    vec4 outputColor = texture2D(u_ColorMap, fract(uv));

    // Output to screen
    gl_FragColor = outputColor;
}