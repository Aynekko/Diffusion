/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/


//=========================================================
// shock - projectile shot from shockrifles.
// diffusion - NOTENOTE: I have disabled the beams for now, they eat a lot but don't contribute to visuals as much
// look for the comment //shockbeamdisable, if you want to re-enable them
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"effects.h"
#include	"weapons/weapons.h"
#include	"customentity.h"
#include	"shock.h"

LINK_ENTITY_TO_CLASS(shock_beam, CShock)

BEGIN_DATADESC( CShock )
//	DEFINE_FIELD(m_pBeam, FIELD_CLASSPTR), shockbeamdisable
//	DEFINE_FIELD(m_pNoise, FIELD_CLASSPTR),
	DEFINE_FIELD(m_pSprite, FIELD_CLASSPTR),
	DEFINE_FIELD(m_pSprite2, FIELD_CLASSPTR),
	DEFINE_FUNCTION( Touch ),
	DEFINE_FUNCTION( FlyThink ),
	DEFINE_FUNCTION( CreateEffects ),
	DEFINE_FIELD( EffectsCreated, FIELD_INTEGER ),
END_DATADESC()

void CShock::Spawn(void)
{
	Precache();
	pev->movetype = MOVETYPE_FLY;

	pev->solid = SOLID_BBOX;
	
	SET_MODEL(ENT(pev), "models/shock_effect.mdl");
	UTIL_SetOrigin(this, pev->origin);

	pev->dmg = 1;
	UTIL_SetSize( pev, g_vecZero, g_vecZero );
//	UTIL_SetSize( pev, Vector( -4, -4, -4 ), Vector( 4, 4, 4 ) );
	if( pev->owner )
	{
		m_hOwner = Instance( pev->owner );
		m_iClass = m_hOwner->Classify();
	}

	SetThink( &CShock::FlyThink );
	SetNextThink( 0 );
}

void CShock::Precache()
{
	PRECACHE_MODEL("sprites/shockbeam.spr");
	PRECACHE_MODEL("sprites/lgtning.spr");
	PRECACHE_MODEL("models/shock_effect.mdl");
	PRECACHE_MODEL("sprites/shock.spr");
	PRECACHE_SOUND("weapons/shock_impact_light.wav");
	PRECACHE_SOUND("weapons/shock_impact_heavy.wav");
}

void CShock::FlyThink()
{
	// without this it can crash the game.
	if( m_hOwner == NULL )
		pev->owner = NULL;
	else
	{
		if( GotOwnerClass == 0 )
		{
			m_iClass = m_hOwner->Classify();
			GotOwnerClass = 1;
		}
	}

	// had to do this here, because spawnflag couldn't come through.
	if( EffectsCreated == 0 )
	{
		CreateEffects();
		EffectsCreated = 1;
	}
	
	// relink beams after loading the game
	// shockbeamdisable
	/*
	if( BeamsRelinked == 0 )
	{
		if (m_pNoise)
		{
			m_pNoise->EntsInit( entindex(), entindex() );
			m_pNoise->SetStartAttachment( 1 );
			m_pNoise->SetEndAttachment( 2 );
			m_pNoise->RelinkBeam();
		}
		if (m_pBeam)
		{
			m_pBeam->EntsInit( entindex(), entindex() );
			m_pBeam->SetStartAttachment( 1 );
			m_pBeam->SetEndAttachment( 2 );
			m_pBeam->RelinkBeam();
		}
		BeamsRelinked = 1;
	}
	*/

	if (m_pSprite)
	{
		if(m_pSprite->pev->frame > 9) // FIXME hardcoded!!! how to get the amount of frames from the sprite? AnimateUntilDead doesn't seem to work
			m_pSprite->pev->frame = 0;
		m_pSprite->pev->frame++;
	}
	
	if (pev->waterlevel == 3)
	{
		entvars_t *pevOwner = VARS(pev->owner);
//		EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/shock_impact.wav", VOL_NORM, ATTN_NORM);
		RadiusDamage(pev->origin, pev, pevOwner ? pevOwner : pev, pev->dmg * 3, 150, CLASS_NONE, DMG_SHOCK );
		ClearEffects();
		SetThink( &CBaseEntity::SUB_Remove );
		SetNextThink( 0 );
	}
	else
	{
		SetNextThink( 0.1 );
	}
}

void CShock::Touch(CBaseEntity *pOther)
{
	// Do not collide with the owner.
	if (ENT(pOther->pev) == pev->owner)
		return;

	ClearEffects();

	TraceResult tr = UTIL_GetGlobalTrace( );

	Vector vecOrg = GetAbsOrigin() + tr.vecPlaneNormal * 3;

	MESSAGE_BEGIN( MSG_PVS, gmsgTempEnt, vecOrg );
		WRITE_BYTE(TE_DLIGHT);
		WRITE_COORD(vecOrg.x);	// X
		WRITE_COORD(vecOrg.y);	// Y
		WRITE_COORD(vecOrg.z);	// Z
		if( HasSpawnFlags( SHOCK_ALIENSHIP ) )
			WRITE_BYTE( 10 );		// radius * 0.1
		else
			WRITE_BYTE( 5 );
		if( HasSpawnFlags( SHOCK_ALIENSHIP ) )  // red
		{
			WRITE_BYTE( 255 );		// r
			WRITE_BYTE( 0 );		// g
			WRITE_BYTE( 50 );		// b
		}
		else
		{
			WRITE_BYTE( 50 );		// r
			WRITE_BYTE( 128 );		// g
			WRITE_BYTE( 220 );		// b
		}
		WRITE_BYTE( 2 );		// time * 10
		WRITE_BYTE( 20 );		// decay * 0.1
		WRITE_BYTE( 100 ); // brightness
		WRITE_BYTE( 0 ); // shadows
	MESSAGE_END( );

	UTIL_Sparks( vecOrg );

	CBaseMonster* pMonster = pOther->MyMonsterPointer();

	if (pOther->pev->takedamage )
	{
		int damageType = DMG_SHOCK;
		if (pMonster && !pMonster->IsAlive())
			damageType |= DMG_CLUB;

		ClearMultiDamage();
		entvars_t *pevOwner = VARS(pev->owner);
		entvars_t *pevAttacker = pevOwner ? pevOwner : pev;
		pOther->TraceAttack(pevAttacker, pev->dmg, pev->velocity.Normalize(), &tr, damageType );
		ApplyMultiDamage(pev, pevAttacker);
		/* removed, better to do utilsparks above at all times
		if (pOther->IsPlayer() && (UTIL_PointContents(pev->origin) != CONTENTS_WATER))
		{
			const Vector position = tr.vecEndPos;
			MESSAGE_BEGIN( MSG_ONE, SVC_TEMPENTITY, NULL, pOther->pev );
				WRITE_BYTE( TE_SPARKS );
				WRITE_COORD( position.x );
				WRITE_COORD( position.y );
				WRITE_COORD( position.z );
			MESSAGE_END();
		}
		*/
	}

	// splat sound
	if( HasSpawnFlags( SHOCK_ALIENSHIP ) )
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/shock_impact_heavy.wav", VOL_NORM, ATTN_NORM, 0, RANDOM_LONG(75,100));
	else
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/shock_impact_light.wav", 0.5, ATTN_NORM, 0, RANDOM_LONG(95,115));

	entvars_t *pevOwner;
	if ( pev->owner )
		pevOwner = VARS( pev->owner );
	else
		pevOwner = NULL;

//	pev->owner = NULL; // can't traceline attack owner if this is set
	RadiusDamage( GetAbsOrigin(), pev, pevOwner, 3, 150, m_iClass, DMG_SHOCK );

	pev->modelindex = 0;
	pev->solid = SOLID_NOT;
	SetThink( &CBaseEntity::SUB_Remove );
	SetNextThink(0.1); // let the sound play
}

void CShock::CreateEffects(void)
{
	if( !m_pSprite )
		m_pSprite = CSprite::SpriteCreate( "sprites/shock.spr", GetAbsOrigin(), TRUE );

	if( m_pSprite )
	{
		m_pSprite->SetAttachment( edict(), 0 );
		if( HasSpawnFlags(SHOCK_ALIENSHIP) )
		{
			m_pSprite->pev->scale = 0.6;
			m_pSprite->SetTransparency( kRenderTransAdd, 255, 0, 50, 150, 0 );
		}
		else
		{
			m_pSprite->pev->scale = 0.1;
			m_pSprite->SetTransparency( kRenderTransAdd, 80, 160, 255, 150, 0 );
		}
	}

	if( !m_pSprite2 )
		m_pSprite2 = CSprite::SpriteCreate( "sprites/shockbeam.spr", GetAbsOrigin(), FALSE );
		
	if( m_pSprite2 )
	{
		m_pSprite2->SetAttachment( edict(), 0 );
		if( HasSpawnFlags( SHOCK_ALIENSHIP ) )
		{
			m_pSprite2->pev->scale = 0.6;
			m_pSprite2->SetTransparency( kRenderTransAdd, 255, 0, 50, 200, 0 );
		}
		else
		{
			m_pSprite2->pev->scale = 0.2;
			m_pSprite2->SetTransparency( kRenderTransAdd, 80, 160, 255, 200, 0 );
		}
	}

	// shockbeamdisable
	/*
	if( !m_pBeam )
		m_pBeam = CBeam::BeamCreate( "sprites/lgtning.spr", 30 );

	if (m_pBeam)
	{
		m_pBeam->EntsInit( entindex(), entindex() );
		m_pBeam->SetStartAttachment( 1 );
		m_pBeam->SetEndAttachment( 2 );
		m_pBeam->SetBrightness( 180 );
		m_pBeam->SetScrollRate( 10 );
		m_pBeam->SetNoise( 0 );
		m_pBeam->SetFlags( BEAM_FSHADEOUT );
		if( pev->flags & SHOCK_ALIENSHIP )
			m_pBeam->SetColor( 255, 0, 50 );
		else
			m_pBeam->SetColor( 0, 255, 255 );
		//m_pBeam->pev->spawnflags = SF_BEAM_TEMPORARY;
		m_pBeam->RelinkBeam();
	}
	else
		ALERT(at_console, "Could not create shockbeam beam!\n");

	if( !m_pNoise )
		m_pNoise = CBeam::BeamCreate( "sprites/lgtning.spr", 30 );

	if (m_pNoise)
	{
		m_pNoise->EntsInit( entindex(), entindex() );
		m_pNoise->SetStartAttachment( 1 );
		m_pNoise->SetEndAttachment( 2 );
		m_pNoise->SetBrightness( 180 );
		m_pNoise->SetScrollRate( 30 );
		m_pNoise->SetNoise( 30 );
		m_pNoise->SetFlags( BEAM_FSHADEOUT );
		m_pNoise->SetColor( 255, 255, 173 );
		m_pNoise->RelinkBeam();
	}
	else
		ALERT(at_console, "Could not create shockbeam noise!\n");
	*/
}

void CShock::ClearEffects()
{
	if (m_pBeam)
	{
		UTIL_Remove( m_pBeam );
		m_pBeam = NULL;
	}

	if (m_pNoise)
	{
		UTIL_Remove( m_pNoise );
		m_pNoise = NULL;
	}

	if (m_pSprite)
	{
		UTIL_Remove( m_pSprite );
		m_pSprite = NULL;
	}

	if (m_pSprite2)
	{
		UTIL_Remove( m_pSprite2 );
		m_pSprite2 = NULL;
	}
}