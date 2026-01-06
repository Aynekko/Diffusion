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
/*

===== generic grenade.cpp ========================================================

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons/weapons.h"
#include "nodes.h"
#include "entities/soundent.h"
#include "decals.h"
#include "player.h"

//===================grenade

LINK_ENTITY_TO_CLASS( grenade, CGrenade );
LINK_ENTITY_TO_CLASS( grenade_emp, CGrenade );

// Grenades flagged with this will be triggered when the owner calls detonateSatchelCharges
#define SF_DETONATE		0x0001

BEGIN_DATADESC( CGrenade )
	DEFINE_FUNCTION( Smoke ),
	DEFINE_FUNCTION( BounceTouch ),
	DEFINE_FUNCTION( SlideTouch ),
	DEFINE_FUNCTION( ExplodeTouch ),
	DEFINE_FUNCTION( DangerSoundThink ),
	DEFINE_FUNCTION( PreDetonate ),
	DEFINE_FUNCTION( Detonate ),
	DEFINE_FUNCTION( DetonateUse ),
	DEFINE_FUNCTION( TumbleThink ),
	DEFINE_FUNCTION( SmokeGrenadeThink ),
	DEFINE_FIELD( LastBounceSoundTime, FIELD_TIME ),
	DEFINE_FIELD( SendWaterSplash, FIELD_BOOLEAN ),
	DEFINE_FIELD( IsEMPGrenade, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hSprite, FIELD_EHANDLE ),
END_DATADESC()

//
// Grenade Explode
//
void CGrenade::Explode( Vector vecSrc, Vector vecAim )
{
	TraceResult tr;
	UTIL_TraceLine ( GetAbsOrigin(), GetAbsOrigin() + Vector ( 0, 0, -32 ),  ignore_monsters, ENT(pev), & tr);

	Explode( &tr, DMG_BLAST );
}

// UNDONE: temporary scorching for PreAlpha - find a less sleazy permenant solution.
void CGrenade::Explode( TraceResult *pTrace, int bitsDamageType )
{
	pev->model = iStringNull;//invisible
	pev->solid = SOLID_NOT;// intangible

	pev->takedamage = DAMAGE_NO;

	// Pull out of the wall a bit
	if ( pTrace->flFraction != 1.0 )
	{
	//	float vecPlaneScale = (pev->dmg - 24) * 0.6f;
	//	if( vecPlaneScale < 1 )
	//		vecPlaneScale = 1;
	//	SetAbsOrigin( pTrace->vecEndPos + (pTrace->vecPlaneNormal * vecPlaneScale ));
		SetAbsOrigin( pTrace->vecEndPos + (pTrace->vecPlaneNormal * 0.6) ); // diffusion - https://github.com/ValveSoftware/halflife/issues/3244
	}

	Vector absOrigin = GetAbsOrigin();

	int iContents = UTIL_PointContents ( absOrigin );
	
	if( !IsEMPGrenade )
	{
		MESSAGE_BEGIN( MSG_PAS, gmsgTempEnt, absOrigin );
			WRITE_BYTE( TE_EXPLOSION );	// This makes a dynamic light and the explosion sprites/sound
			WRITE_COORD( absOrigin.x );	// Send to PAS because of the sound
			WRITE_COORD( absOrigin.y );
			WRITE_COORD( absOrigin.z );

			if( iContents != CONTENTS_WATER )
				WRITE_SHORT( g_sModelIndexFireball );
			else
				WRITE_SHORT( g_sModelIndexWExplosion );

			int spritescale = (pev->dmg - 50) * .60;
			if( spritescale < 20 )
				spritescale = 20;
			WRITE_BYTE( spritescale ); // scale * 10
			WRITE_BYTE( 15 ); // framerate
			WRITE_BYTE( TE_EXPLFLAG_NONE );
		MESSAGE_END();
	}
	else // EMP
	{
		byte r = 0;
		byte g = 140;
		byte b = 255;
		Vector vecOrigin = GetAbsOrigin();
		MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, vecOrigin );
			WRITE_BYTE( TE_BEAMCYLINDER );
			WRITE_COORD( vecOrigin.x );
			WRITE_COORD( vecOrigin.y );
			WRITE_COORD( vecOrigin.z + 4 );
			WRITE_COORD( vecOrigin.x );
			WRITE_COORD( vecOrigin.y );
			WRITE_COORD( vecOrigin.z + 512 ); // reach damage radius over .4 seconds
			WRITE_SHORT( g_sBlastTexture );
			WRITE_BYTE( 0 ); // startframe
			WRITE_BYTE( 0 ); // framerate
			WRITE_BYTE( 4 ); // life
			WRITE_BYTE( 32 );  // width
			WRITE_BYTE( 16 );   // noise
			WRITE_BYTE( r ); // R
			WRITE_BYTE( g ); // G
			WRITE_BYTE( b ); // B
			WRITE_BYTE( 255 ); //brightness
			WRITE_BYTE( 0 ); // speed
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_PAS, gmsgTempEnt, vecOrigin );
			WRITE_BYTE( TE_DLIGHT );
			WRITE_COORD( vecOrigin.x );	// X
			WRITE_COORD( vecOrigin.y );	// Y
			WRITE_COORD( vecOrigin.z + 4 );	// Z
			WRITE_BYTE( 32 );		// radius * 0.1
			WRITE_BYTE( r );		// r
			WRITE_BYTE( g );		// g
			WRITE_BYTE( b );		// b
			WRITE_BYTE( 10 );		// time * 10
			WRITE_BYTE( 32 );		// decay * 0.1
			WRITE_BYTE( 150 ); // brightness
			WRITE_BYTE( 1 ); // shadows
		MESSAGE_END();
	}

	// diffusion - throw a piece of metal
	if( iContents != CONTENTS_WATER )
	{
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, absOrigin );
			WRITE_BYTE( TE_BREAKMODEL );

			// position
			WRITE_COORD( absOrigin.x );
			WRITE_COORD( absOrigin.y );
			WRITE_COORD( absOrigin.z + 10 );
			// size
			WRITE_COORD( 0.01 );
			WRITE_COORD( 0.01 );
			WRITE_COORD( 0.01 );
			// velocity
			WRITE_COORD( 0 );
			WRITE_COORD( 0 );
			WRITE_COORD( 0 );
			// randomization of the velocity
			WRITE_BYTE( 30 );
			// Model
			WRITE_SHORT( g_sModelIndexMetalGibs );	//model id#
				// # of shards
			WRITE_BYTE( 2 );
			// duration
			WRITE_BYTE( 20 );
			// flags
			WRITE_BYTE( BREAK_METAL );
		MESSAGE_END();
	}

	int sndentindex = 0;
	if( pev->owner )
		sndentindex = ENTINDEX( pev->owner );

	CSoundEnt::InsertSound ( bits_SOUND_COMBAT, absOrigin, NORMAL_EXPLOSION_VOLUME, 3.0, sndentindex );
	entvars_t *pevOwner;
	if ( pev->owner )
		pevOwner = VARS( pev->owner );
	else
		pevOwner = NULL;

	pev->owner = NULL; // can't traceline attack owner if this is set

	RadiusDamage ( pev, pevOwner, pev->dmg, pev->dmg * 3, CLASS_NONE, bitsDamageType );

	CBaseEntity *pEntity = CBaseEntity::Instance( pTrace->pHit );

	if( !IsEMPGrenade )
	{
		if( RANDOM_FLOAT( 0, 1 ) < 0.5 )
		{
			if( pEntity && UTIL_GetModelType( pEntity->pev->modelindex ) == mod_studio )
				UTIL_StudioDecalTrace( pTrace, DECAL_SCORCH1 );
			else UTIL_DecalTrace( pTrace, DECAL_SCORCH1 );
		}
		else
		{
			if( pEntity && UTIL_GetModelType( pEntity->pev->modelindex ) == mod_studio )
				UTIL_StudioDecalTrace( pTrace, DECAL_SCORCH2 );
			else UTIL_DecalTrace( pTrace, DECAL_SCORCH2 );
		}
	}
	else
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "weapons/emp_explode.wav", 1.0, ATTN_NORM );

	switch ( RANDOM_LONG( 0, 2 ) )
	{
		case 0:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/debris1.wav", 0.55, ATTN_NORM);	break;
		case 1:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/debris2.wav", 0.55, ATTN_NORM);	break;
		case 2:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/debris3.wav", 0.55, ATTN_NORM);	break;
	}

	pev->effects |= EF_NODRAW;

	SetThink( &CGrenade::Smoke );

	SetAbsVelocity( g_vecZero );
	SetNextThink(0.3);
}


void CGrenade :: Smoke( void )
{
	Vector absOrigin = GetAbsOrigin();

	if( !IsEMPGrenade )
	{
		if( UTIL_PointContents( absOrigin ) == CONTENTS_WATER )
		{
			UTIL_Bubbles( absOrigin - Vector( 64, 64, 64 ), absOrigin + Vector( 64, 64, 64 ), 100 );
		}
		else
		{
			/*		MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, absOrigin );
						WRITE_BYTE( TE_SMOKE );
						WRITE_COORD( absOrigin.x );
						WRITE_COORD( absOrigin.y );
						WRITE_COORD( absOrigin.z );
						WRITE_SHORT( g_sModelIndexSmoke );
						int spritescale = (pev->dmg - 50) * 0.80;
						if( spritescale < 20 )
							spritescale = 20;
						WRITE_BYTE( spritescale ); // scale * 10
						WRITE_BYTE( 12  ); // framerate
					MESSAGE_END();*/
		}
	}

	UTIL_Remove( this );
}

void CGrenade::Killed( entvars_t *pevAttacker, int iGib )
{
	Detonate( );
}

void CGrenade::ClearEffects( void )
{
	// clear sound channel (alien grunt throws beeping grenade)
	EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "common/null.wav", 0.2, 3.0 );

	if( m_hSprite != NULL )
	{
		UTIL_Remove( m_hSprite );
		m_hSprite = NULL;
	}
}


// Timed grenade, this think is called when time runs out.
void CGrenade::DetonateUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	SetThink(&CGrenade::Detonate );
	SetNextThink( value );
}

void CGrenade::PreDetonate( void )
{
	// diffusion - not inserting entindex here, so monster could run away from his own failed throw
	CSoundEnt::InsertSound ( bits_SOUND_DANGER, GetAbsOrigin(), 400, 0.3 );

	SetThink(&CGrenade::Detonate );
	SetNextThink( 1.0 );
}


void CGrenade::Detonate( void )
{
	TraceResult tr;
	// trace starts here!
	Vector vecSpot = GetAbsOrigin() + Vector ( 0 , 0 , 8 );
	UTIL_TraceLine ( vecSpot, vecSpot + Vector ( 0, 0, -40 ),  ignore_monsters, ENT(pev), & tr);

	if( IsEMPGrenade )
		Explode( &tr, DMG_EMP );
	else
		Explode( &tr, DMG_BLAST );
}


//
// Contact grenade, explode when it touches something
// 
void CGrenade::ExplodeTouch( CBaseEntity *pOther )
{
	// clear sound channel (alien grunt throws beeping grenade)
	EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "common/null.wav", 0.2, 3.0 );
	
	TraceResult tr;
	Vector		vecSpot;// trace starts here!

	pev->enemy = pOther->edict();

	vecSpot = GetAbsOrigin() - GetAbsVelocity().Normalize() * 32;
	UTIL_TraceLine( vecSpot, vecSpot + GetAbsVelocity().Normalize() * 64, ignore_monsters, edict(), &tr );

	Explode( &tr, DMG_BLAST );
}

void CGrenade::DangerSoundThink( void )
{
	if( !IsInWorld( ))
	{
		UTIL_Remove( this );
		return;
	}

	int sndentindex = 0;
	if( pev->owner )
		sndentindex = ENTINDEX( pev->owner );
	CSoundEnt::InsertSound( bits_SOUND_DANGER, GetAbsOrigin() + GetAbsVelocity() * 0.5f, GetAbsVelocity().Length() * 0.5f, 0.2, sndentindex );

	if( !DoWaterCheck )
	{
		// spawned underwater, no need to splash
		if( pev->waterlevel > 0 )
			SendWaterSplash = true;

		DoWaterCheck = true;
	}

	if( pev->waterlevel != 0 )
	{
		Vector vecVelocity = GetAbsVelocity();
		vecVelocity *= 0.5f;
		SetAbsVelocity( vecVelocity );
		if( !SendWaterSplash )
		{
			SendWaterSplash = true;
			Vector org = GetAbsOrigin();
			MakeWaterSplash( org + Vector( 0, 0, 512 ), org, 1 );
		}
	}
	else
	{
		if( SendWaterSplash )
			SendWaterSplash = false;
	}

	SetNextThink( 0.2 );
}


void CGrenade::BounceTouch( CBaseEntity *pOther )
{
	// don't hit the guy that launched this grenade
	if ( pOther->edict() == pev->owner )
		return;

	// only do damage if we're moving fairly fast
	if( m_flNextAttack < gpGlobals->time && GetAbsVelocity().Length() > 100 )
	{
		entvars_t *pevOwner = VARS( pev->owner );

		if( pevOwner )
		{
			TraceResult tr = UTIL_GetGlobalTrace( );
			ClearMultiDamage( );
			pOther->TraceAttack( pevOwner, 1, gpGlobals->v_forward, &tr, DMG_CLUB ); 
			ApplyMultiDamage( pev, pevOwner );
		}

		m_flNextAttack = gpGlobals->time + 1.0; // debounce
	}

	Vector vecTestVelocity;

	// this is my heuristic for modulating the grenade velocity because grenades dropped purely vertical
	// or thrown very far tend to slow down too quickly for me to always catch just by testing velocity. 
	// trimming the Z velocity a bit seems to help quite a bit.
	vecTestVelocity = GetAbsVelocity(); 
	vecTestVelocity.z *= 0.45;

	if( !m_fRegisteredSound && vecTestVelocity.Length() <= 60 )
	{
		//ALERT( at_console, "Grenade Registered!: %f\n", vecTestVelocity.Length() );

		// grenade is moving really slow. It's probably very close to where it will ultimately stop moving. 
		// go ahead and emit the danger sound.
		
		// register a radius louder than the explosion, so we make sure everyone gets out of the way
		CSoundEnt::InsertSound ( bits_SOUND_DANGER, GetAbsOrigin(), pev->dmg / 0.4, 0.3 );
		m_fRegisteredSound = TRUE;
	}

	if( pev->flags & FL_ONGROUND )
	{
		// add a bit of static friction
		Vector vecVelocity = GetAbsVelocity();
		vecVelocity *= 0.8f;
		SetAbsVelocity( vecVelocity );

		pev->sequence = 1;
		SetAbsAngles( Vector( 0, RANDOM_LONG( -160, 160 ), 0 ) );
	}
	else
	{
		// play bounce sound
		BounceSound();
		Vector vecVelocity = GetAbsVelocity();
		vecVelocity *= 0.98f;
		SetAbsVelocity( vecVelocity );
	}

	pev->framerate = GetAbsVelocity().Length() / 200.0;

	if( pev->framerate > 1.0 )
		pev->framerate = 1;
	else if( pev->framerate < 0.5 )
		pev->framerate = 0;

}

void CGrenade::SlideTouch( CBaseEntity *pOther )
{
	// don't hit the guy that launched this grenade
	if( pOther->edict() == pev->owner )
		return;

	// SetLocalAvelocity( Vector( 300, 300, 300 ));

	if( pev->flags & FL_ONGROUND )
	{
		// add a bit of static friction
		Vector vecVelocity = GetAbsVelocity();
		vecVelocity *= 0.95f;
		SetAbsVelocity( vecVelocity );

		if( vecVelocity.x != 0 || vecVelocity.y != 0 )
		{
			// maintain sliding sound
		}
	}
	else
		BounceSound();
}

void CGrenade :: BounceSound( void )
{
	if( gpGlobals->time > LastBounceSoundTime + 0.2 )
	{
		switch( RANDOM_LONG( 0, 2 ))
		{
		case 0: EMIT_SOUND( edict(), CHAN_VOICE, "weapons/grenade_hit1.wav", 0.25, ATTN_NORM ); break;
		case 1: EMIT_SOUND( edict(), CHAN_VOICE, "weapons/grenade_hit2.wav", 0.25, ATTN_NORM ); break;
		case 2: EMIT_SOUND( edict(), CHAN_VOICE, "weapons/grenade_hit3.wav", 0.25, ATTN_NORM ); break;
		}

		LastBounceSoundTime = gpGlobals->time;

		pev->pain_finished++; // bounce counter for smoke grenade
	}
}

void CGrenade :: TumbleThink( void )
{
	if( !IsInWorld( ))
	{
		UTIL_Remove( this );
		return;
	}

	SetNextThink( 0.1 );

	if( pev->dmgtime - 1 < gpGlobals->time )
	{
		int sndentindex = 0;
		if( pev->owner )
			sndentindex = ENTINDEX( pev->owner );
		CSoundEnt::InsertSound( bits_SOUND_DANGER, GetAbsOrigin() + GetAbsVelocity() * (pev->dmgtime - gpGlobals->time), 400, 0.1, sndentindex );
	}

	if( pev->dmgtime <= gpGlobals->time )
		SetThink(&CGrenade::Detonate );

	if( !DoWaterCheck )
	{
		// spawned underwater, no need to splash
		if( pev->waterlevel > 0 )
			SendWaterSplash = true;

		DoWaterCheck = true;
	}

	if( pev->waterlevel != 0 )
	{
		Vector vecVelocity = GetAbsVelocity();
		vecVelocity *= 0.5f;
		SetAbsVelocity( vecVelocity );
		pev->framerate = 0.2;
		if( !SendWaterSplash )
		{
			SendWaterSplash = true;
			Vector org = GetAbsOrigin();
			MakeWaterSplash( org + Vector( 0, 0, 512 ), org, 1 );
		}
	}
	else
	{
		if( SendWaterSplash )
			SendWaterSplash = false;
	}

	// blink sprite while flying
	if( !m_hSprite )
	{
		m_hSprite = CSprite::SpriteCreate( "sprites/glow01.spr", GetAbsOrigin(), FALSE );
		if( m_hSprite )
		{
			CSprite *pSprite = (CSprite*)(CBaseEntity*)m_hSprite;
			pSprite->SetParent( this );
			if( IsEMPGrenade )
				pSprite->SetTransparency( kRenderConstantGlow, 0, 140, 255, 255, 0 );
			else
				pSprite->SetTransparency( kRenderConstantGlow, 255, 50, 50, 255, 0 );
			pSprite->SetScale( 0.1f );
			pSprite->pev->effects |= EF_NODRAW;
		}
	}

	if( m_hSprite )
	{
		if( gpGlobals->time > sprite_change_time )
		{
			if( m_hSprite->pev->effects & EF_NODRAW )
			{
				m_hSprite->pev->effects &= ~EF_NODRAW;
				const Vector LightOrg = GetAbsOrigin();
				MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, LightOrg );
				WRITE_BYTE( TE_DLIGHT );
					WRITE_COORD( LightOrg.x );		// origin
					WRITE_COORD( LightOrg.y );
					WRITE_COORD( LightOrg.z + 10.0f );
					WRITE_BYTE( 15 );	// radius
					if( IsEMPGrenade )
					{
						WRITE_BYTE( 0 );	// R
						WRITE_BYTE( 140 );	// G
						WRITE_BYTE( 255 );	// B
					}
					else
					{
						WRITE_BYTE( 255 );	// R
						WRITE_BYTE( 50 );	// G
						WRITE_BYTE( 50 );	// B
					}
					WRITE_BYTE( 20 );	// life * 10
					WRITE_BYTE( 20 ); // decay
					WRITE_BYTE( 255 ); // brightness
					WRITE_BYTE( 0 ); // shadows off
				MESSAGE_END();
			}
			else
				m_hSprite->pev->effects |= EF_NODRAW;

			sprite_change_time = gpGlobals->time + 0.25f;
		}
	}
}


void CGrenade:: Spawn( void )
{
	pev->movetype = MOVETYPE_BOUNCE;
	if( IsEMPGrenade )
		pev->classname = MAKE_STRING( "grenade_emp" );
	else
		pev->classname = MAKE_STRING( "grenade" );
	SetFlag(F_NOBACKCULL);
	
	pev->solid = SOLID_BBOX;

	SET_MODEL( ENT( pev ), "models/weapons/grenade.mdl" );

	if( IsEMPGrenade )
		pev->skin = 1;
		
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));

	pev->dmg = 100;
	m_fRegisteredSound = FALSE;
}

CGrenade *CGrenade::ShootContact( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity )
{
	CGrenade *pGrenade = GetClassPtr( (CGrenade *)NULL );
	pGrenade->Spawn();
	// contact grenades arc lower
	pGrenade->pev->gravity = 0.5;// lower gravity since grenade is aerodynamic and engine doesn't know it.
	UTIL_SetOrigin( pGrenade, vecStart );
	pGrenade->SetLocalVelocity( vecVelocity );
	pGrenade->SetLocalAngles( UTIL_VecToAngles( pGrenade->GetLocalVelocity( )));
	pGrenade->pev->owner = ENT(pevOwner);
	
	// make monsters afraid of it while in the air
	pGrenade->SetThink(&CGrenade::DangerSoundThink );
	pGrenade->SetNextThink( 0 );
	
	// Tumble in air
	Vector avelocity( RANDOM_FLOAT ( 100, 500 ), 0, 0 );
	pGrenade->SetLocalAvelocity( avelocity );
	
	// Explode on contact
	pGrenade->SetTouch(&CGrenade::ExplodeTouch );

	pGrenade->pev->dmg = 100;

	return pGrenade;
}

CGrenade *CGrenade::ShootSmoke( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity )
{
	CGrenade *pGrenade = GetClassPtr( (CGrenade *)NULL );
	pGrenade->Spawn();
	// contact grenades arc lower
	pGrenade->pev->gravity = 0.6;// lower gravity since grenade is aerodynamic and engine doesn't know it.
	pGrenade->pev->friction = 0.8;
	UTIL_SetOrigin( pGrenade, vecStart );
	pGrenade->SetAbsVelocity( vecVelocity );
	pGrenade->SetLocalAngles( UTIL_VecToAngles( pGrenade->GetAbsVelocity() ) );
	if( pevOwner )
		pGrenade->pev->owner = ENT( pevOwner );

	pGrenade->SetTouch( &CGrenade::BounceTouch );	// Bounce if touched

	// perform "on ground" checks
	pGrenade->SetThink( &CGrenade::SmokeGrenadeThink );
	pGrenade->SetNextThink( 0 );

	SET_MODEL( pGrenade->edict(), "models/weapons/w_smoke.mdl" );

	pGrenade->pev->sequence = RANDOM_LONG( 3, 5 );
	pGrenade->pev->body = 1;
	pGrenade->pev->framerate = 1.0f;
	pGrenade->pev->pain_finished = 0; // bounce counter for smoke grenade

	return pGrenade;
}

void CGrenade::SmokeGrenadeThink( void )
{
	if( pev->flags & FL_ONGROUND || pev->pain_finished > 5 )
	{
		pev->sequence = 1;
		SetAbsAngles( Vector( 0, RANDOM_LONG( -160, 160 ), 0 ) );
		SetThink( &CGrenade::SmokeGrenadeExplode );
		SetNextThink( 1.0 );
		return;
	}

	if( !DoWaterCheck )
	{
		// spawned underwater, no need to splash
		if( pev->waterlevel > 0 )
			SendWaterSplash = true;

		DoWaterCheck = true;
	}

	// watersplash upon contact with water surface
	if( pev->waterlevel != 0 )
	{
		if( !SendWaterSplash )
		{
			SendWaterSplash = true;
			Vector org = GetAbsOrigin();
			MakeWaterSplash( org + Vector( 0, 0, 512 ), org, 1 );

			Vector vecVelocity = GetAbsVelocity();
			vecVelocity *= 0.5f;
			SetAbsVelocity( vecVelocity );
			pev->gravity *= 0.35f;
		}
	}
	else
	{
		if( SendWaterSplash )
			SendWaterSplash = false;
	}

	SetNextThink( 0 );
}

void CGrenade::SmokeGrenadeExplode( void )
{
	Vector org = GetAbsOrigin() + Vector( 0, 0, 32 );
	MESSAGE_BEGIN( MSG_PAS, gmsgTempEnt, org );
		WRITE_BYTE( TE_SMOKEGRENADE );
		WRITE_COORD( org.x );
		WRITE_COORD( org.y );
		WRITE_COORD( org.z );
		WRITE_BYTE( 75 ); // num of sprites
		WRITE_BYTE( 2 ); // speed of decay (*0.01)
		WRITE_BYTE( 70 ); // scale
		WRITE_BYTE( 100 ); // randomize position offset of each sprite (+/- 100 here)
	MESSAGE_END();

	pev->gravity = 0.5f;

	if( pev->waterlevel == 0 )
		SetAbsVelocity( Vector(RANDOM_FLOAT(-50,50), RANDOM_FLOAT(-50,50), 250) );

	Vector absOrigin = GetAbsOrigin();

	if( UTIL_PointContents( absOrigin ) == CONTENTS_WATER )
		UTIL_Bubbles( absOrigin - Vector( 64, 64, 64 ), absOrigin + Vector( 64, 64, 64 ), 100 );

	SetThink( &CBaseEntity::SUB_Remove );
	SetNextThink( 30 );
}

CGrenade * CGrenade:: ShootTimed( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity, float time, bool IsEMP )
{
	CGrenade *pGrenade = GetClassPtr( (CGrenade *)NULL );
	pGrenade->IsEMPGrenade = IsEMP;
	pGrenade->Spawn();

	UTIL_SetOrigin( pGrenade, vecStart );
	pGrenade->SetAbsVelocity( vecVelocity );
	pGrenade->SetLocalAngles( UTIL_VecToAngles( pGrenade->GetAbsVelocity( )));
	if( pevOwner )
		pGrenade->pev->owner = ENT( pevOwner );
	
	pGrenade->SetTouch(&CGrenade::BounceTouch );	// Bounce if touched
	
	// Take one second off of the desired detonation time and set the think to PreDetonate. PreDetonate
	// will insert a DANGER sound into the world sound list and delay detonation for one second so that 
	// the grenade explodes after the exact amount of time specified in the call to ShootTimed(). 

	pGrenade->pev->dmgtime = gpGlobals->time + time;
	pGrenade->SetThink(&CGrenade::TumbleThink );
	pGrenade->SetNextThink( 0.1 );

	if( time < 0.1 )
	{
		pGrenade->SetNextThink( 0 );
		pGrenade->SetLocalVelocity( g_vecZero );
	}
		
	pGrenade->pev->sequence = RANDOM_LONG( 3, 5 );
	pGrenade->pev->framerate = 1.0;

	pGrenade->pev->gravity = 0.6;
	pGrenade->pev->friction = 0.8;

	pGrenade->pev->dmg = 130;
	pGrenade->pev->scale = 0.75; // diffusion - the grenade was too big

	return pGrenade;
}

CGrenade * CGrenade :: ShootSatchelCharge( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity )
{
	CGrenade *pGrenade = GetClassPtr( (CGrenade *)NULL );
	pGrenade->pev->movetype = MOVETYPE_BOUNCE;
	pGrenade->pev->classname = MAKE_STRING( "grenade" );
	
	pGrenade->pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pGrenade->pev), "models/weapons/grenade.mdl");	// Change this to satchel charge model

	UTIL_SetSize(pGrenade->pev, Vector( 0, 0, 0), Vector(0, 0, 0));

	pGrenade->pev->dmg = 200;
	UTIL_SetOrigin( pGrenade, vecStart );
	pGrenade->SetAbsVelocity( vecVelocity );
	pGrenade->SetAbsAngles( g_vecZero );
	pGrenade->pev->owner = ENT(pevOwner);
	
	// Detonate in "time" seconds
	pGrenade->SetThink( &CBaseEntity::SUB_DoNothing );
	pGrenade->SetUse(&CGrenade::DetonateUse );
	pGrenade->SetTouch(&CGrenade::SlideTouch );
	pGrenade->pev->spawnflags = SF_DETONATE;

	pGrenade->pev->friction = 0.9;

	return pGrenade;
}

void CGrenade :: UseSatchelCharges( entvars_t *pevOwner, SATCHELCODE code )
{
	edict_t *pentFind;
	edict_t *pentOwner;

	if ( !pevOwner )
		return;

	CBaseEntity	*pOwner = CBaseEntity::Instance( pevOwner );

	pentOwner = pOwner->edict();

	pentFind = FIND_ENTITY_BY_CLASSNAME( NULL, "grenade" );
	while ( !FNullEnt( pentFind ) )
	{
		CBaseEntity *pEnt = Instance( pentFind );
		if ( pEnt )
		{
			if ( FBitSet( pEnt->pev->spawnflags, SF_DETONATE ) && pEnt->pev->owner == pentOwner )
			{
				if ( code == SATCHEL_DETONATE )
					pEnt->Use( pOwner, pOwner, USE_ON, 0 );
				else	// SATCHEL_RELEASE
					pEnt->pev->owner = NULL;
			}
		}
		pentFind = FIND_ENTITY_BY_CLASSNAME( pentFind, "grenade" );
	}
}

//======================end grenade

class CAPCProjectile : public CGrenade
{
	DECLARE_CLASS( CAPCProjectile, CGrenade );
	void Spawn( void );
	void Precache( void );
	void AccelerateThink( void );

	DECLARE_DATADESC();

	Vector m_vecForward;
};

LINK_ENTITY_TO_CLASS( apc_projectile, CAPCProjectile );

BEGIN_DATADESC( CAPCProjectile )
	DEFINE_FIELD( m_vecForward, FIELD_VECTOR ),
	DEFINE_FUNCTION( AccelerateThink ),
END_DATADESC()

void CAPCProjectile::Spawn( void )
{
	Precache();
	// motor
	pev->movetype = MOVETYPE_TOSS;
	pev->gravity = 0.45;
	pev->solid = SOLID_BBOX;

	SET_MODEL( ENT( pev ), "models/weapons/grenade.mdl" );
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
	RelinkEntity( TRUE );
	
	SetTouch( &CGrenade::ExplodeTouch );

	UTIL_MakeVectors( GetAbsAngles() );
	m_vecForward = gpGlobals->v_forward;

	SetThink( &CAPCProjectile::AccelerateThink );
	SetNextThink( 0.1 );

	pev->dmg = 120.0f;
}


void CAPCProjectile::Precache( void )
{
	PRECACHE_MODEL( "models/weapons/grenade.mdl" );
	PRECACHE_SOUND( "car/apc_fire.wav" );
	PRECACHE_SOUND( "car/apc_fire_d.wav" );
}


void CAPCProjectile::AccelerateThink( void )
{
	// check world boundaries
	if( !IsInWorld( FALSE ) )
	{
		UTIL_Remove( this );
		return;
	}

	// accelerate
	float flSpeed = GetAbsVelocity().Length();
	if( flSpeed < 1800 )
	{
		SetAbsVelocity( GetAbsVelocity() + m_vecForward * 200 );
	}

	// re-aim
	SetAbsAngles( UTIL_VecToAngles( GetAbsVelocity() ) );

	SetNextThink( 0.1 );
}