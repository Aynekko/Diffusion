/*
https://github.com/scanberg/hbao
Horizon Based Ambient Occlusion
Implementation of the method from Nvidia's SampleSDK
*/

#include "const.h"
#include "mathlib.h"

uniform sampler2D	u_DepthMap;
uniform sampler2D	u_BlendTexture;
uniform vec2 		u_HBAOParams[5];
#define FocalLen	u_HBAOParams[0]
#define UVToViewA	u_HBAOParams[1]
#define UVToViewB	u_HBAOParams[2]
#define LinMAD		u_HBAOParams[3]
#define ScreenRes	u_HBAOParams[4]

varying vec2        var_TexCoord;

vec2 AORes = ScreenRes * 0.5;
vec2 InvAORes = 1.0 / AORes;
vec2 NoiseScale = AORes * 0.25;

const float AOStrength = 1.1;
const float R = 4.0;
const float R2 = R * R * R * 2;
const float NegInvR2 = - 1.0 / R2;
const float TanBias = tan( 8.0 * M_PI / 180.0 );
const float MaxRadiusPixels = 36.0;

const int NumDirections = 5;
const int NumSamples = 4;


float ViewSpaceZFromDepth(float d)
{
	// [0,1] -> [-1,1] clip space
	d = d * 2.0 - 1.0;

	// Get view space Z
	return -1.0 / (LinMAD.x * d + LinMAD.y);
}

vec3 UVToViewSpace(vec2 uv, float z)
{
	uv = UVToViewA * uv + UVToViewB;
	return vec3(uv * z, z);
}

vec3 GetViewPos(vec2 uv)
{
	float z = ViewSpaceZFromDepth(texture2D(u_DepthMap, uv).r);
	return UVToViewSpace(uv, z);
}

float Length2(vec3 V)
{
	return dot(V, V);
}

float InvLength(vec2 V)
{
	return inversesqrt(dot(V, V));
}

float Tangent(vec3 P, vec3 S)
{
	return -(P.z - S.z) * InvLength(S.xy - P.xy);
}

float BiasedTangent(vec3 V)
{
	return (V.z * 3.0) * InvLength(V.xy) + TanBias;
}

float TanToSin(float x)
{
	return x * inversesqrt(x*x + 1.0);
}

vec3 MinDiff(vec3 P, vec3 Pr, vec3 Pl)
{
	vec3 V1 = Pr - P;
	vec3 V2 = P - Pl;
	return (Length2(V1) < Length2(V2)) ? V1 : V2;
}

vec2 SnapUVOffset(vec2 uv)
{
	return round(uv * AORes) * InvAORes;
}

float Falloff(float d2)
{
	return d2 * NegInvR2 + 1.0;
}

vec2 RotateDirections(vec2 Dir, vec2 CosSin)
{
	return vec2(Dir.x*CosSin.x - Dir.y*CosSin.y, Dir.x*CosSin.y + Dir.y*CosSin.x);
}

void ComputeSteps(inout vec2 stepSizeUv, inout float numSteps, float rayRadiusPix, float rand)
{
	// Avoid oversampling if numSteps is greater than the kernel radius in pixels
	numSteps = min(NumSamples, rayRadiusPix);

	// Divide by Ns+1 so that the farthest samples are not fully attenuated
	float stepSizePix = rayRadiusPix / (numSteps + 1);

	// Clamp numSteps if it is greater than the max kernel footprint
	float maxNumSteps = MaxRadiusPixels / stepSizePix;
	if(maxNumSteps < numSteps)
	{
		// Use dithering to avoid AO discontinuities
		numSteps = floor(maxNumSteps + rand);
		numSteps = max(numSteps, 1);
		stepSizePix = MaxRadiusPixels / numSteps;
	}

	// Step size in uv space
	stepSizeUv = stepSizePix * InvAORes;
}

float HorizonOcclusion(vec2 deltaUV, vec3 P, vec3 dPdu, vec3 dPdv, float randstep, float numSamples)
{
	float ao = 0;

	// Offset the first coord with some noise
	vec2 uv = var_TexCoord + SnapUVOffset( randstep * deltaUV );
	deltaUV = SnapUVOffset( deltaUV );

	// Calculate the tangent vector
	vec3 T = deltaUV.x * dPdu + deltaUV.y * dPdv;

	// Get the angle of the tangent vector from the viewspace axis
	float tanH = BiasedTangent( T );
	float sinH = TanToSin( tanH );

	float tanS;
	float d2;
	vec3 S;

	// Sample to find the maximum angle
	for( float s = 1; s <= numSamples; ++s )
	{
		uv += deltaUV;
		S = GetViewPos( uv );
		tanS = Tangent( P, S );
		d2 = Length2( S - P ) * 0.2;

		// Is the sample within the radius and the angle greater?
		if( d2 < R2 && tanS > tanH )
		{
			float sinS = TanToSin( tanS );
			// Apply falloff based on the distance
			ao += Falloff( d2 ) * ( sinS - sinH );

			tanH = tanS;
			sinH = sinS;
		}
	}

	return ao;
}

void main(void)
{
	float numDirections = NumDirections;

	vec3 P, Pr, Pl, Pt, Pb;
	P = GetViewPos(var_TexCoord);

	// Sample neighboring pixels
	Pr = GetViewPos(var_TexCoord + vec2( InvAORes.x, 0));
	Pl = GetViewPos(var_TexCoord + vec2(-InvAORes.x, 0));
	Pt = GetViewPos(var_TexCoord + vec2( 0, InvAORes.y));
	Pb = GetViewPos(var_TexCoord + vec2( 0,-InvAORes.y));

	// Calculate tangent basis vectors using the minimu difference
	vec3 dPdu = MinDiff(P, Pr, Pl);
	vec3 dPdv = MinDiff(P, Pt, Pb) * (AORes.y * InvAORes.x);

	// Get the random samples from the noise texture
	vec3 random = texture2D(u_BlendTexture, var_TexCoord * NoiseScale).rgb;

	// Calculate the projected size of the hemisphere
	vec2 rayRadiusUV = 1.5 * R * FocalLen / -P.z;
	float rayRadiusPix = rayRadiusUV.x * AORes.x;

	float ao = 1.0;

	// Make sure the radius of the evaluated hemisphere is more than a pixel
	if(rayRadiusPix > 1.0)
	{
		ao = 0.0;
		float numSteps;
		vec2 stepSizeUV;

		// Compute the number of steps
		ComputeSteps(stepSizeUV, numSteps, rayRadiusPix, random.z);

		float alpha = 2.0 * M_PI / numDirections;

		// Calculate the horizon occlusion of each direction
		for(float d = 0; d < numDirections; ++d)
		{
			float theta = alpha * d;

			// Apply noise to the direction
			vec2 dir = RotateDirections(vec2(cos( theta ), sin( theta )), random.xy);
			vec2 deltaUV = dir * stepSizeUV;

			// Sample the pixels along the direction
			ao += HorizonOcclusion(deltaUV, P, dPdu, dPdv, random.z, numSteps);
		}

		// Average the results and produce the final AO
		ao = 1.0 - ao / numDirections * AOStrength;
		ao *= 1.25;
	}  

	gl_FragColor = vec4( ao );
}