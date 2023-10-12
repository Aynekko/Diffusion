// thanks to kylebakerio for the interior initial setup code
// thanks to SNMetamorph for grid setup and randomization code
// thanks to ncuxonaT for room position fix on stretched textures and helping to implement an atlas technique instead of cubemaps

uniform sampler2D		u_InteriorMap;
uniform vec2			u_InteriorGrid;
uniform float			u_InteriorLightState;

#define SEED_CHANGING_FREQ 0.5
const float INTERIOR_BACK_PLANE_SCALE = 0.5;
const vec2 ATLAS_SCALE = vec2(0.5 , 0.25); // atlas 2x4
const float IntRefraction = 0.1;

vec4 InteriorMapping( vec4 diffuse, vec2 TexDiffuse, vec3 N, float Time, vec3 ViewVec, vec3 vPos )
{
	// Time: u_realtime is currently not used in lighting change
	vec2 gridSize = u_InteriorGrid;

	vec2 uv = fract( TexDiffuse );
	vec2 slicedUv = fract(uv * gridSize);

	// add refraction
	#if defined( STUDIO_BUMP ) || defined( BMODEL_BUMP )
		slicedUv.x += N.x * IntRefraction;
		slicedUv.y += N.y * IntRefraction;
	#endif

	vec2 blockNums = floor(TexDiffuse * gridSize);

	float staticSeed = 500; // room positions
	float randomSeed = staticSeed;//don't change          floor(Time * SEED_CHANGING_FREQ); // lighting change

	float staticVal = randomFunction2(staticSeed, blockNums);
    float randomVal = staticVal;   //randomFunction(randomSeed, blockNums);

	// derivations of the fragment position
    vec3 pos_dx = dFdx( vPos );
    vec3 pos_dy = dFdy( vPos );
    // derivations of the texture coordinate
    vec2 texC_dx = dFdx( TexDiffuse );
    vec2 texC_dy = dFdy( TexDiffuse );
    // tangent vector and binormal vector
    vec3 t = texC_dy.y * pos_dx - texC_dx.y * pos_dy;
    vec3 b = texC_dx.x * pos_dy - texC_dy.x * pos_dx;
    float multiplier = length(b) / length(t);

#if !defined( STUDIO_INTERIOR )
    vec3 n = normalize(cross(pos_dx, pos_dy));
    ViewVec *= mat3(normalize(t), normalize(b), n);
#endif
    
    ViewVec.x *= multiplier * gridSize.x / gridSize.y;

	// setup room view position
	vec3 sampleDir = normalize( ViewVec );

#if defined( STUDIO_INTERIOR )
	sampleDir *= vec3(-1,-1,1);
#endif

	vec3 viewInv = 1.0 / sampleDir;
	vec3 pos = vec3( slicedUv * 2.0 - 1.0, -1.0 );
	float fmin = min3( abs( viewInv ) - viewInv * pos );
	sampleDir = sampleDir * fmin + pos;

    vec2 interiorUV = sampleDir.xy * mix( 1.0 , INTERIOR_BACK_PLANE_SCALE , sampleDir.z * 0.5 + 0.5);

    interiorUV = interiorUV * 0.5 + vec2(0.5);
    interiorUV *= ATLAS_SCALE;    
    
    float rnd = randomFunction2( 1, blockNums );
    interiorUV.x += ATLAS_SCALE.x * float(rnd > 0.5);
    interiorUV.y += ATLAS_SCALE.y * floor(rnd * 8.0);

	// for some randomizaton
	vec4 interior0 = texture2D( u_InteriorMap, interiorUV );
	vec4 interior1 = interior0;
	vec4 interior2 = interior0;
	vec4 interior3 = interior0;
	vec4 interior4 = interior0;

	float LightChance = u_InteriorLightState * 0.01;

	// randomize room lighting
	if( randomVal <= 0.2 )
	{
		if( randomFunction2( 1, vec2(randomVal,randomVal) ) >= LightChance )
			interior0.rgb *= 0.2;
		if( randomFunction2( 1, vec2(randomVal,randomVal) ) >= LightChance )
			interior1.rgb *= 0.2;
		if( randomFunction2( 1, vec2(randomVal,randomVal) ) >= LightChance )
			interior2.rgb *= 0.2;
		if( randomFunction2( 1, vec2(randomVal,randomVal) ) >= LightChance )
			interior3.rgb *= 0.2;
		if( randomFunction2( 1, vec2(randomVal,randomVal) ) >= LightChance )
			interior4.rgb *= 0.2;
	}
	else if( randomVal <= 0.4 )
	{
		if( randomFunction2( 1, vec2(randomVal,randomVal) ) >= LightChance )
			interior0.rgb *= 0.2;
		if( randomFunction2( 1, vec2(randomVal,randomVal) ) >= LightChance )
			interior1.rgb *= 0.2;
		if( randomFunction2( 1, vec2(randomVal,randomVal) ) >= LightChance )
			interior2.rgb *= 0.2;
		if( randomFunction2( 1, vec2(randomVal,randomVal) ) >= LightChance )
			interior3.rgb *= 0.2;
		if( randomFunction2( 1, vec2(randomVal,randomVal) ) >= LightChance )
			interior4.rgb *= 0.2;
	}
	else if( randomVal <= 0.6 )
	{
		if( randomFunction2( 1, vec2(randomVal,randomVal) ) >= LightChance )
			interior0.rgb *= 0.2;
		if( randomFunction2( 1, vec2(randomVal,randomVal) ) >= LightChance )
			interior1.rgb *= 0.2;
		if( randomFunction2( 1, vec2(randomVal,randomVal) ) >= LightChance )
			interior2.rgb *= 0.2;
		if( randomFunction2( 1, vec2(randomVal,randomVal) ) >= LightChance )
			interior3.rgb *= 0.2;
		if( randomFunction2( 1, vec2(randomVal,randomVal) ) >= LightChance )
			interior4.rgb *= 0.2;
	}
	else if( randomVal <= 0.8 )
	{
		if( randomFunction2( 1, vec2(randomVal,randomVal) ) >= LightChance )
			interior0.rgb *= 0.2;
		if( randomFunction2( 1, vec2(randomVal,randomVal) ) >= LightChance )
			interior1.rgb *= 0.2;
		if( randomFunction2( 1, vec2(randomVal,randomVal) ) >= LightChance )
			interior2.rgb *= 0.2;
		if( randomFunction2( 1, vec2(randomVal,randomVal) ) >= LightChance )
			interior3.rgb *= 0.2;
		if( randomFunction2( 1, vec2(randomVal,randomVal) ) >= LightChance )
			interior4.rgb *= 0.2;
	}
	else
	{
		if( randomFunction2( 1, vec2(randomVal,randomVal) ) >= LightChance )
			interior0.rgb *= 0.2;
		if( randomFunction2( 1, vec2(randomVal,randomVal) ) >= LightChance )
			interior1.rgb *= 0.2;
		if( randomFunction2( 1, vec2(randomVal,randomVal) ) >= LightChance )
			interior2.rgb *= 0.2;
		if( randomFunction2( 1, vec2(randomVal,randomVal) ) >= LightChance )
			interior3.rgb *= 0.2;
		if( randomFunction2( 1, vec2(randomVal,randomVal) ) >= LightChance )
			interior4.rgb *= 0.2;
	}

	// randomize room positions
	if (staticVal <= 0.2)
        diffuse = diffuse * diffuse.a + interior0 * (1 - diffuse.a);
    else if (staticVal <= 0.4)
        diffuse = diffuse * diffuse.a + interior1 * (1 - diffuse.a);
    else if (staticVal <= 0.6)
        diffuse = diffuse * diffuse.a + interior2 * (1 - diffuse.a);
    else if (staticVal <= 0.8)
        diffuse = diffuse * diffuse.a + interior3 * (1 - diffuse.a);
    else
        diffuse = diffuse * diffuse.a + interior4 * (1 - diffuse.a);

	return diffuse;
}