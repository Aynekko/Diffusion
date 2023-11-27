// Author: Elie Michel
// License: CC BY 3.0
// July 2017 - shadertoy.com/view/ldSBWW

uniform sampler2D    u_ColorMap;
uniform vec3 u_WaterDrops[2];
#define u_ScreenSize	u_WaterDrops[0].xy
#define u_RealTime		u_WaterDrops[0].z
#define u_Visibility	u_WaterDrops[1].x
#define u_Intensity		u_WaterDrops[1].y

vec2 rand( vec2 c )
{
    mat2 m = mat2( 12.9898, 0.16180, 78.233, 0.31415 );
    return fract( sin( m * c ) * vec2( 43758.5453, 14142.1 ));
}

vec2 noise( vec2 p )
{
    vec2 co = floor( p );
    vec2 mu = fract( p );
    mu = 3.0 * mu * mu - 2.0 * mu * mu * mu;
    vec2 a = rand(( co+vec2( 0.0, 0.0 )));
    vec2 b = rand(( co+vec2( 1.0, 0.0 )));
    vec2 c = rand(( co+vec2( 0.0, 1.0 )));
    vec2 d = rand(( co+vec2( 1.0, 1.0 )));
    return mix( mix( a, b, mu.x ), mix( c, d, mu.x ), mu.y );
}

void main( void )
{
    float Transparency = u_Visibility; // ranged 0.0 - 1.0
    float DripIntensity = u_Intensity; // 0 and above
	
    vec2 iResolution = u_ScreenSize;
    vec2 c = gl_FragCoord.xy;

    vec2 u = c / iResolution,
    v = ( c * 0.1 ) / iResolution,
    n = noise( v * 200.0 ); // Displacement
    
    vec4 InColor = texture( u_ColorMap, u, 2.5 );
    vec4 f = InColor;
    
    // Loop through the different inverse sizes of drops
    for( float r = 4.0; r > 0.0; r--) 
    {
        vec2 x = 1.5 * iResolution * r * 0.015,  // Number of potential drops (in a grid)
        p = 6.28 * u * x + ( n - 0.5 ) * 2.0,
        s = sin( p );
        
        // Current drop properties. Coordinates are rounded to ensure a
        // consistent value among the fragment of a given drop.
        //vec4 d = texture( iChannel1, round( u * x - 0.25 ) / x );
        vec2 v = round( u * x - 0.25 ) / x;
        vec4 d = vec4( noise( v * 200.0 ), noise( v ));
        
        // Drop shape and fading
        float t = ( s.x + s.y ) * max( 0.0, 1.0 - fract( u_RealTime * ( d.b + 0.1 ) + d.g ) * 2.0 );
        
        // d.r -> only x% of drops are kept on, with x depending on the size of drops
        if ( d.r < DripIntensity * ( 5.0 - r ) * 0.08 && t > 0.5 )
		{
            // Drop normal
            vec3 v = normalize(-vec3( cos( p ), mix( 0.2, 2.0, t - 0.5 )));
            // fragColor = vec4( v * 0.5 + 0.5, 1.0 );  // show normals
            
            // Poor man's refraction (no visual need to do more)
            f = texture( u_ColorMap, u - v.xy * 0.3 );
        }
    }

    f = mix( InColor, f, Transparency );

    gl_FragColor = f;
}