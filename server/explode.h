/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#ifndef EXPLODE_H
#define EXPLODE_H


#define	SF_ENVEXPLOSION_NODAMAGE	BIT(0) // when set, ENV_EXPLOSION will not actually inflict damage
#define	SF_ENVEXPLOSION_REPEATABLE	BIT(1) // can this entity be refired?
#define SF_ENVEXPLOSION_NOFIREBALL	BIT(2) // don't draw the fireball
#define SF_ENVEXPLOSION_NOSMOKE		BIT(3) // don't draw the smoke
#define SF_ENVEXPLOSION_NODECAL		BIT(4) // don't make a scorch mark
#define SF_ENVEXPLOSION_NOSPARKS	BIT(5)
#define SF_ENVEXPLOSION_NOSOUND		BIT(6) // don't make sound

extern DLL_GLOBAL	short	g_sModelIndexFireball;
extern DLL_GLOBAL	short	g_sModelIndexSmoke;
extern DLL_GLOBAL	short	g_sModelIndexFire;
extern DLL_GLOBAL short g_sModelIndexMetalGibs;

extern void ExplosionCreate( const Vector &center, const Vector &angles, edict_t *pInflictor, edict_t *pAttacker, int magnitude, BOOL doDamage );

#endif			//EXPLODE_H
