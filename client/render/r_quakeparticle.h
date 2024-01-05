/*
gl_rpart.h - quake-like particles
this code written for Paranoia 2: Savior modification
Copyright (C) 2013 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef GL_RPART_H
#define GL_RPART_H

#include "randomrange.h"
#include "custom_alloc.h"

#define MAX_PARTICLES		16384
#define MAX_PARTICLES_PER_ENTITY 1024 // it can grow out of control...
#define MAX_PARTINFOS		256	// various types of part-system

// built-in particle-system flags
#define FPART_BOUNCE		BIT(0)	// makes a bouncy particle
#define FPART_FRICTION		BIT(1)
#define FPART_VERTEXLIGHT	BIT(2)	// give some ambient light for it
#define FPART_STRETCH		BIT(3)
#define FPART_UNDERWATER	BIT(4)
#define FPART_INSTANT		BIT(5)
#define FPART_ADDITIVE		BIT(6)
#define FPART_NOTWATER		BIT(7)	// don't spawn in water
#define FPART_FADEIN		BIT(8) // diffusion - fade in alpha, and only then fade out
#define FPART_DISTANCESCALE BIT(9) // diffusion - scale with distance (dustmotes)
#define FPART_SKYBOX		BIT(10) // diffusion - only in 3d sky
#define FPART_FLOATING		BIT(11) // diffusion - float on water
#define FPART_FLOATING_ORIENTED		BIT(12) // diffusion - float on water, but oriented to the top and two-side
#define FPART_AFLOAT		BIT(13) // she is ready to settle down
#define FPART_SINEWAVE		BIT(14) // for the bubbles...
#define FPART_OPAQUE		BIT(15) // goes through solid render pass
#define FPART_NOTINSOLID	BIT(16) // don't draw in solid
#define FPART_TWOSIDE		BIT(17)

enum
{
	TYPE_DEFAULT = 0,
	TYPE_DUSTMOTE,
	TYPE_SPARKS,
	TYPE_SMOKE,
	TYPE_WATERSPLASH,
	TYPE_SAND,
	TYPE_DIRT,
	TYPE_FIREBALL,
	TYPE_BLOOD,
	TYPE_BUBBLES,
	TYPE_BEAMRING,
	TYPE_WATERDROP,
	TYPE_CUSTOM,
};

class CQuakePart
{
public:
	Vector		m_vecOrigin;	// position for current frame
	Vector		m_vecLastOrg;	// position from previous frame
	Vector		m_vecVelocity;	// linear velocity
	Vector		m_vecAccel;
	Vector		m_vecColor;
	Vector		m_vecColorVelocity;
	float		m_flAlpha;
	float		m_flAlphaVelocity;
	float		m_flRadius;
	float		m_flRadiusVelocity;
	float		m_flLength;
	float		m_flLengthVelocity;
	float		m_flRotation;	// texture ROLL angle
	float		m_flRotationVelocity;
	float		m_flBounceFactor;
	float		m_flDistance; // don't allow particle over a certain distance
	float		m_flStartAlpha;
	Vector		m_vecAddedVelocity;
	float		m_flDieTime;
	float		m_flSinSpeed;
	Vector		m_vecView; // orientation vector

	// only used with flag FPART_DISTANCESCALE
	float m_flMinScale;
	float m_flMaxScale;

	CQuakePart	*pNext;		// linked list
	int	m_hTexture;
	int ParticleType;

	float m_flTime;
	int	m_iFlags;
	Vector newLight;
	int EntIndex; // pointer to entity which emitted this particle

//	float AlphaDownScale;

	bool Evaluate( float gravity );
};

enum
{
	NORMAL_IGNORE = 0,
	NORMAL_OFFSET,
	NORMAL_DIRECTION,
	NORMAL_OFS_DIR,
};

class CQuakePartInfo
{
public:
	char		m_szName[32];	// effect name

	struct model_s	*m_pSprite;	// sprite
	int		m_hTexture;	// tga texture

	RandomRange	offset[3];
	RandomRange	velocity[3];
	RandomRange	accel[3];
	RandomRange	color[3];
	RandomRange	colorVel[3];
	RandomRange	alpha;
	RandomRange	alphaVel;
	RandomRange	radius;
	RandomRange	radiusVel;
	RandomRange	length;
	RandomRange	lengthVel;
	RandomRange	rotation;
	RandomRange rotationVel;
	RandomRange	bounce;
	RandomRange	frame;
	RandomRange	count;		// particle count

	int		normal;		// how to use normal
	int		flags;		// particle flags

	int EntIndex; // pointer to entity which emitted this particle
};

class CQuakePartSystem
{
	CQuakePart	*m_pFreeParticles;
	CQuakePart	m_pParticles[MAX_PARTICLES];

	CQuakePartInfo	m_pPartInfo[MAX_PARTINFOS];
	int		m_iNumPartInfo;

public:
			CQuakePartSystem( void );
	virtual		~CQuakePartSystem( void );

	// textures
	// for each texture there is an array
	int	m_hDefaultParticle;
	int	m_hSparks;
	int	m_hSmoke;
	int	m_hWaterSplash;
	int m_hBlood;
	int m_hBloodSplat;
	int m_hSand;
	int m_hDirt;
	int m_hDustMote;
	int m_hExplosion;
	int m_hBubble;
	int m_hBeamRing;
	int m_hRainDrop;

	CQuakePart *m_pActiveParticles;

	void		Clear( void );
	void		Update( void );
	void		FreeParticle( CQuakePart *pCur );
	CQuakePart	*AllocParticle( void );
	bool		AddParticle( CQuakePart *src, int texture = 0, int flags = 0 );
	void		ParsePartInfos( const char *filename );
	bool		ParsePartInfo( CQuakePartInfo *info, char *&pfile );
	bool		ParseRandomVector( char *&pfile, RandomRange out[3] );
	int		ParseParticleFlags( char *pfile );
	CQuakePartInfo	*FindPartInfo( const char *name );
	void		CreateEffect( int EntIndex, const char *name, const Vector &origin, const Vector &normal );
	void DrawParticles( MemBlock<CQuakePart> &ParticleArray );

	// example presets
	void ExplosionParticles( int EntIndex, const Vector &pos );
	void BulletParticles( int EntIndex, const Vector &org, const Vector &dir );
	void Bubble( int EntIndex, const Vector &pos, float Speed, int Distance, float DieTime, float SinSpeed );
	void Beamring( int EntIndex, const Vector &start, const Vector &end );
	void SparkParticles( int EntIndex, const Vector &org, const Vector &dir );
	void RicochetSparks( int EntIndex, const Vector &org, float scale );
	void SmokeParticles( int EntIndex, const Vector &pos, int count, float speed = 0, float scale = 0 );
	void Smoke( int EntIndex, int Particle, const Vector &pos, const Vector &vel, int count, float speed = 0.15, float scale = 30, float posRand = 0, float velRand = 0, Vector Color = Vector(1,1,1), float Distance = 0.0f, float Alpha = 0.5f );
	void GunSmoke( int EntIndex, const Vector &pos, Vector vel, int WeaponID );
	void WaterSplashParticle( int EntIndex, const Vector &pos, float size = 1.0f );
	void SmokeVolume( int EntIndex, int Particle, const Vector &pos, const Vector &PushVelocity, const Vector &PushVelocityRand, const Vector &Color, float Scale, float Alpha = 0.1f, int Distance = 666 );
	void BloodParticle( int EntIndex, const Vector &pos, float Scale, Vector Color, Vector Direction );
	void WaterDripLine( const Vector &start, const Vector &end, int Distance = 666 );
	void WaterDrop( int EntIndex, const Vector &pos );

	int ParticleCountPerEnt[8192]; // MAX_ENTITIES?

//	Vector ExplosionOrg;
};

extern CQuakePartSystem	g_pParticles;

CQuakePart InitializeParticle( void );

#endif//GL_RPART_H