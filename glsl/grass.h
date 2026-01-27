uniform float u_GrassWind;

vec4 GrassAnimateSimple( vec4 position, float time )
{
	float anim = time * 0.5 + position.z;
	position.x += sin( anim );
	position.y += cos( anim );
	return position;
}

vec4 GrassAnimateWind( vec4 position, float time )
{
	const vec2 WindDirection = vec2( 1.0, 0.4 ); // normalized XZ direction
	const float WindStrength = 1.25;
	const float WindSpeed = 1.5;

	float randomPhase = dot( position.xz, vec2(17.183, 93.761)) * 0.37;
	randomPhase = fract( sin( randomPhase ) * 43758.5453 );
	vec2 windDir = normalize(WindDirection);
	float t = time * WindSpeed + randomPhase * 6.28;
	// main large motion
	float wave = sin( t + position.x * 0.7 + position.z * 0.4 );
	// faster small turbulence
	float detail = 0.6 * sin( t * 3.1 + position.x * 2.1 + position.z * 1.7 );
	float combined = wave + detail;
	combined = clamp( combined, -1.0, 1.0 );
	// bend strength curve – stronger at top, softer near base
	vec3 windOffset = vec3( windDir.x, 0.0, windDir.y ) * combined;
	windOffset *= WindStrength;
	// tiny vertical bob / squash
	position.y += 0.08 * ( sin( t * 1.8 + randomPhase * 5.0 ) * 0.5 + 0.5 - 1.0 );

	// Apply wind
	position.xz += windOffset.xz;

	return position;
}

vec4 GrassAnimate( vec4 position, float time )
{
	if( bool( u_GrassWind == 0.0 ) )
		return GrassAnimateSimple( position, time );

	return GrassAnimateWind( position, time );
}