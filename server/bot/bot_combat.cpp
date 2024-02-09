

#include "extdll.h"
#include "util.h"
#include "game/client.h"
#include "cbase.h"
#include "player.h"
#include "items.h"
#include "effects.h"
#include "weapons/weapons.h"
#include "entities/soundent.h"
#include "game/gamerules.h"
#include "animation.h"
#include "monsters.h"

#include "bot.h"


extern int f_Observer;  // flag to indicate if player is in observer mode

// weapon firing delay based on skill (min and max delay for each weapon)
float primary_fire_delay[WEAPON_G36C +1][5][2] = {
   // WEAPON_NONE - NOT USED
   {{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}},
   // WEAPON_KNIFE
   {{0.0f, 0.1f}, {0.2f, 0.3f}, {0.3f, 0.5f}, {0.4f, 0.6f}, {0.6f, 1.0f}},
   // WEAPON_BERETTA (9mm)
   {{0.0f, 0.1f}, {0.1f, 0.2f}, {0.2f, 0.3f}, {0.3f, 0.4f}, {0.4f, 0.5f}},
   // WEAPON_DEAGLE (357)
   {{0.0f, 0.25f}, {0.2f, 0.5f}, {0.4f, 0.8f}, {1.0f, 1.3f}, {1.5f, 2.0f}},
   // WEAPON_MP5 (9mmAR)
   {{0.0f, 0.1f}, {0.1f, 0.3f}, {0.3f, 0.5f}, {0.4f, 0.6f}, {0.5f, 0.8f}},
   // WEAPON_CHAINGUN - NOT USED
   {{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}},
   // WEAPON_CROSSBOW
   {{0.0f, 0.25f}, {0.2f, 0.4f}, {0.5f, 0.7f}, {0.8f, 1.0f}, {1.0f, 1.3f}},
   // WEAPON_SHOTGUN
   {{0.0f, 0.25f}, {0.2f, 0.5f}, {0.4f, 0.8f}, {0.6f, 1.2f}, {0.8f, 2.0f}},
   // WEAPON_RPG
   {{1.0f, 3.0f}, {2.0f, 4.0f}, {3.0f, 5.0f}, {4.0f, 6.0f}, {5.0f, 7.0f}},
   // WEAPON_GAUSS
   {{0.0f, 0.1f}, {0.2f, 0.3f}, {0.3f, 0.5f}, {0.5f, 0.8f}, {1.0f, 1.2f}},
   // WEAPON_EGON
   {{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}},
   // WEAPON_HORNETGUN
   {{0.0f, 0.1f}, {0.25f, 0.4f}, {0.4f, 0.7f}, {0.6f, 1.0f}, {1.0f, 1.5f}},
   // WEAPON_HANDGRENADE
   {{1.0f, 1.4f}, {1.4f, 2.0f}, {1.8f, 2.6f}, {2.0f, 3.0f}, {2.5f, 3.8f}},
   // WEAPON_TRIPMINE
   {{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}},
   // WEAPON_SATCHEL
   {{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}},
   // WEAPON_SNARK
   {{0.0f, 0.1f}, {0.1f, 0.2f}, {0.2f, 0.5f}, {0.5f, 0.7f}, {0.6f, 1.0f}},
   // WEAPON_AR2
   {{0.0f, 0.1f}, {0.1f, 0.3f}, {0.3f, 0.5f}, {0.4f, 0.6f}, {0.5f, 0.8f}},
   // WEAPON_DRONE - NOT USED
   {{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}},
   // WEAPON_SENTRY - NOT USED
   {{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}},
   // WEAPON_HKMP5
   {{0.0f, 0.1f}, {0.1f, 0.3f}, {0.3f, 0.5f}, {0.4f, 0.6f}, {0.5f, 0.8f}},
   // WEAPON_FIVESEVEN
   {{0.0f, 0.1f}, {0.1f, 0.2f}, {0.2f, 0.3f}, {0.3f, 0.4f}, {0.4f, 0.5f}},
   // WEAPON_SNIPER
   {{0.0f, 0.25f}, {0.2f, 0.4f}, {0.5f, 0.7f}, {0.8f, 1.0f}, {1.0f, 1.3f}},
   // WEAPON_SHOTGUN_XM
   {{0.0f, 0.25f}, {0.2f, 0.5f}, {0.4f, 0.8f}, {0.6f, 1.2f}, {0.8f, 2.0f}},
   // WEAPON_G36C
   {{0.0f, 0.1f}, {0.1f, 0.3f}, {0.3f, 0.5f}, {0.4f, 0.6f}, {0.5f, 0.8f}},
   };

float secondary_fire_delay[WEAPON_G36C +1][5][2] = {
	// WEAPON_NONE - NOT USED
	{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}},
	// WEAPON_KNIFE - Not applicable
	{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}},
	// WEAPON_BERETTA (9mm)
	{{0.0f, 0.1f}, {0.0f, 0.1f}, {0.1f, 0.2f}, {0.1f, 0.2f}, {0.2f, 0.4f}},
	// WEAPON_DEAGLE (357)
	{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}},
	// WEAPON_MP5 (9mmAR)
	{{0.0f, 0.3f}, {0.5f, 0.8f}, {0.7f, 1.0f}, {1.0f, 1.6f}, {1.4f, 2.0f}},
	// WEAPON_CHAINGUN - NOT USED
	{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}},
	// WEAPON_CROSSBOW
	{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}},
	// WEAPON_SHOTGUN
	{{0.0f, 0.25f}, {0.2f, 0.5f}, {0.4f, 0.8f}, {0.6f, 1.2f}, {0.8f, 2.0f}},
	// WEAPON_RPG - Not applicable
	{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}},
	// WEAPON_GAUSS
	{{0.2f, 0.5f}, {0.3f, 0.7f}, {0.5f, 1.0f}, {0.8f, 1.5f}, {1.0f, 2.0f}},
	// WEAPON_EGON - Not applicable
	{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}},
	// WEAPON_HORNETGUN
	{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}},
	// WEAPON_HANDGRENADE - Not applicable
	{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}},
	// WEAPON_TRIPMINE - Not applicable
	{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}},
	// WEAPON_SATCHEL
	{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}},
	// WEAPON_SNARK - Not applicable
	{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}},
	// WEAPON_AR2
	{{0.0f, 0.1f}, {0.1f, 0.3f}, {0.3f, 0.5f}, {0.4f, 0.6f}, {0.5f, 0.8f}},
	// WEAPON_DRONE - Not applicable
	{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}},
	// WEAPON_SENTRY - Not applicable
	{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}},
	// WEAPON_HKMP5 - Not applicable
	{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}},
	// WEAPON_FIVESEVEN - Not applicable
	{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}},
	// WEAPON_SNIPER - Not applicable
	{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}},
	// WEAPON_SHOTGUN_XM - Not applicable
	{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}},
	// WEAPON_G36C - Not applicable
	{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}},
	};   

ammo_check_t ammo_check[] = {
   {"ammo_glockclip", "9mm", _9MM_MAX_CARRY},
   {"ammo_9mmclip", "9mm", _9MM_MAX_CARRY},
   {"ammo_9mmAR", "mrcbullets", _9MM_MAX_CARRY},
   {"ammo_9mmbox", "9mm", _9MM_MAX_CARRY},
   {"ammo_mp5clip", "mrcbullets", _9MM_MAX_CARRY},
   {"ammo_chainboxclip", "9mm", _9MM_MAX_CARRY},
   {"ammo_mp5grenades", "ARgrenades", M203_GRENADE_MAX_CARRY},
   {"ammo_ARgrenades", "ARgrenades", M203_GRENADE_MAX_CARRY},
   {"ammo_buckshot", "buckshot", BUCKSHOT_MAX_CARRY},
   {"ammo_crossbow", "bolts", BOLT_MAX_CARRY},
   {"ammo_357", "deagle", DEAGLE_MAX_CARRY},
   {"ammo_rpgclip", "rockets", ROCKET_MAX_CARRY},
   {"ammo_egonclip", "uranium", URANIUM_MAX_CARRY},
   {"ammo_gaussclip", "uranium", URANIUM_MAX_CARRY},

   {"ammo_ar2", "ar2ammo", AR2_MAX_CARRY},
   {"ammo_ar2ball", "ar2ball", M203_GRENADE_MAX_CARRY},
   {"ammo_fiveseven", "ammo57", _57_MAX_CARRY },
   {"ammo_g36c", "g36cammo", _9MM_MAX_CARRY},
   {"ammo_hkmp5", "hkmp5ammo", _9MM_MAX_CARRY },
	{"ammo_sniper", "sniperammo", SNIPER_MAX_CARRY},
   {"", 0, 0}};

// sounds for Bot taunting after a kill...
char barney_taunt[][30] = { BA_TNT1, BA_TNT2, BA_TNT3, BA_TNT4, BA_TNT5 };
char scientist_taunt[][30] = { SC_TNT1, SC_TNT2, SC_TNT3, SC_TNT4, SC_TNT5 };


CBaseEntity * CBot::BotFindEnemy( void )
{
   Vector vecEnd;
   static bool flag = true;

   if (pBotEnemy != NULL)  // does the bot already have an enemy?
   {
	  vecEnd = pBotEnemy->EyePosition();

	  // if the enemy is dead or has switched to botcam mode...
	  if (!pBotEnemy->IsAlive() || (pBotEnemy->pev->effects & EF_NODRAW))
	  {
		 if (!pBotEnemy->IsAlive())  // is the enemy dead?, assume bot killed it
		 {
			// check if this player is not a bot (i.e. not fake client)...
			if (pBotEnemy->IsNetClient() && !IS_DEDICATED_SERVER())
			{
			   // speak taunt sounds about 10% of the time
				/*
			   if (RANDOM_LONG(1, 100) <= 10)
			   {
				  if (bot_model == MODEL_BARNEY)
					 strcpy( sound, barney_taunt[RANDOM_LONG(0,4)] );
				  else if (bot_model == MODEL_SCIENTIST)
					 strcpy( sound, scientist_taunt[RANDOM_LONG(0,4)] );

				  EMIT_SOUND(ENT(pBotEnemy->pev), CHAN_VOICE, sound,
							 RANDOM_FLOAT(0.9, 1.0), ATTN_NORM);
			   }*/
			}

			// stay here longer. I'm having a good time here!
			if( IsCamping )
				camping_end_time += RANDOM_LONG(15,30);
		 }

		 // don't have an enemy anymore so null out the pointer...
		 pBotEnemy = NULL;
	  }
	  else if ( FInViewCone( &vecEnd ) && FVisible( vecEnd ) )
	  {
		 // if enemy is still visible and in field of view, keep it

		 // face the enemy
		 Vector v_enemy = pBotEnemy->pev->origin - pev->origin;
		 Vector bot_angles = UTIL_VecToAngles( v_enemy );

		 pev->ideal_yaw = bot_angles.y;

		 // check for wrap around of angle...
		 BotFixIdealYaw();

		 return pBotEnemy;
	  }
   }

   int i;
   float nearestdistance = 1500;
   CBaseEntity *pNewEnemy = NULL;

   // search the world for players...
   for (i = 1; i <= gpGlobals->maxClients; i++)
   {
	  CBasePlayer *pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( i );

	  // skip invalid players and skip self (i.e. this bot)
	  if ((!pPlayer) || (pPlayer == this))
		 continue;

	  // skip this player if not alive (i.e. dead or dying)
	  if (pPlayer->pev->deadflag != DEAD_NO)
		 continue;

	  // skip players that are not bots in observer mode...
	  if (pPlayer->IsNetClient() && f_Observer)
		 continue;

	  // skip players that are in botcam mode...
	  if (pPlayer->pev->effects & EF_NODRAW)
		 continue;

	  // don't waste time on spawn-protected or godmode players
	  if( pPlayer->IsSpawnProtected || (pPlayer->pev->flags & FL_GODMODE) )
		  continue;

	  // is team play enabled?
	  if (g_pGameRules->IsTeamplay())
	  {
		 // don't target your teammates if team names match...
		 if (UTIL_TeamsMatch(g_pGameRules->GetTeamID(this),
							 g_pGameRules->GetTeamID(pPlayer)))
			continue;
	  }

	  vecEnd = pPlayer->EyePosition();

	  // see if bot can see the player...
	  if (FInViewCone( &vecEnd ) && FVisible( vecEnd ))
	  {
		 float distance = (pPlayer->pev->origin - pev->origin).Length();
		 if (distance < nearestdistance)
		 {
			nearestdistance = distance;
			pNewEnemy = pPlayer;

			pBotUser = NULL;  // don't follow user when enemy found
		 }
	  }
   }

	// diffusion - now perform search for turrets.
	CBaseEntity *pTurret = NULL;
	while( (pTurret = UTIL_FindEntityByClassname( pTurret, "_playersentry" )) != NULL )
	{
		// get the owner - player
		CBaseEntity *pTurretOwner = CBaseEntity::Instance( pTurret->pev->owner );
		if( pTurretOwner )
		{
			if( !pTurret->IsAlive() )
				continue;

			// bot's turret
			if( pTurretOwner == this )
				continue;

			if( g_pGameRules->IsTeamplay() )
			{
				// don't target your teammates if team names match...
				if( UTIL_TeamsMatch( g_pGameRules->GetTeamID( this ), g_pGameRules->GetTeamID( pTurretOwner ) ) )
					continue;
			}

			vecEnd = pTurret->EyePosition();

			// it's an enemy turret
			if( FInViewCone( &vecEnd ) && FVisible( vecEnd ) )
			{
				float turret_distance = (pTurret->pev->origin - pev->origin).Length();
				// this turret is farther then current enemy player
				if( turret_distance > nearestdistance )
					continue;

				nearestdistance = turret_distance;
				pNewEnemy = pTurret;
				pBotUser = NULL;  // don't follow user when enemy found
			}
		}
		else
			continue;
	}

	if (pNewEnemy)
	{
		// face the enemy
		Vector v_enemy = pNewEnemy->pev->origin - pev->origin;
		Vector bot_angles = UTIL_VecToAngles( v_enemy );

		pev->ideal_yaw = bot_angles.y;

		// check for wrap around of angle...
		BotFixIdealYaw();
	}

	return (pNewEnemy);
}


Vector CBot::BotBodyTarget( CBaseEntity *pBotEnemy )
{
   Vector target;
   float f_distance;
   float f_scale;
   int d_x, d_y, d_z;

   f_distance = (pBotEnemy->pev->origin - pev->origin).Length();

   if (f_distance > 1000)
	  f_scale = 1.0;
   else if (f_distance > 100)
	  f_scale = f_distance / 1000.0;
   else
	  f_scale = 0.1;

   switch (bot_skill)
   {
	  case 0:
		 // VERY GOOD, same as from CBasePlayer::BodyTarget (in player.h)
		 target = pBotEnemy->Center() + pBotEnemy->pev->view_ofs * RANDOM_FLOAT( 0.5, 1.1 );
		 d_x = 0;  // no offset
		 d_y = 0;
		 d_z = 0;
		 break;
	  case 1:
		 // GOOD, offset a little for x, y, and z
		 target = pBotEnemy->Center() + pBotEnemy->pev->view_ofs;
		 d_x = RANDOM_FLOAT(-5, 5) * f_scale;
		 d_y = RANDOM_FLOAT(-5, 5) * f_scale;
		 d_z = RANDOM_FLOAT(-9, 9) * f_scale;
		 break;
	  case 2:
		 // FAIR, offset somewhat for x, y, and z
		 target = pBotEnemy->Center() + pBotEnemy->pev->view_ofs;
		 d_x = RANDOM_FLOAT(-9, 9) * f_scale;
		 d_y = RANDOM_FLOAT(-9, 9) * f_scale;
		 d_z = RANDOM_FLOAT(-15, 15) * f_scale;
		 break;
	  case 3:
		 // POOR, offset for x, y, and z
		 target = pBotEnemy->Center() + pBotEnemy->pev->view_ofs;
		 d_x = RANDOM_FLOAT(-16, 16) * f_scale;
		 d_y = RANDOM_FLOAT(-16, 16) * f_scale;
		 d_z = RANDOM_FLOAT(-20, 20) * f_scale;
		 break;
	  case 4:
		 // BAD, offset lots for x, y, and z
		 target = pBotEnemy->Center() + pBotEnemy->pev->view_ofs;
		 d_x = RANDOM_FLOAT(-20, 20) * f_scale;
		 d_y = RANDOM_FLOAT(-20, 20) * f_scale;
		 d_z = RANDOM_FLOAT(-27, 27) * f_scale;
		 break;
   }

   target = target + Vector(d_x, d_y, d_z);

   return target;
}


void CBot::BotWeaponInventory( void )
{
	int i;

	// initialize the elements of the weapons arrays...
	for (i = 0; i < MAX_WEAPONS; i++)
	{
		weapon_ptr[i] = NULL;
		primary_ammo[i] = 0;
		secondary_ammo[i] = 0;
	}

	// find out which weapons the bot is carrying...
	for (i = 0; i < MAX_ITEM_TYPES; i++)
	{
		CBasePlayerItem *pItem = NULL;

		if (m_rgpPlayerItems[i])
		{
			pItem = m_rgpPlayerItems[i];
			while (pItem)
			{
				weapon_ptr[pItem->m_iId] = pItem;  // store pointer to item
				pItem = pItem->m_pNext;
			}
		}
	}

	// find out how much ammo of each type the bot is carrying...
	for (i = 0; i < MAX_AMMO_SLOTS; i++)
	{
		if (!CBasePlayerItem::AmmoInfoArray[i].pszName)
			continue;

	// diffusion: added clips check (   + ((CBasePlayerWeapon*)m_rgpPlayerItems[i])->m_iClip;   )

		if (strcmp("9mm", CBasePlayerItem::AmmoInfoArray[i].pszName) == 0)
		{
			primary_ammo[WEAPON_BERETTA] = m_rgAmmo[i];
			if( weapon_ptr[WEAPON_BERETTA] )
				primary_ammo[WEAPON_BERETTA] += ((CBasePlayerWeapon *)weapon_ptr[WEAPON_BERETTA])->m_iClip;
		}
		else if (strcmp("357", CBasePlayerItem::AmmoInfoArray[i].pszName) == 0)
		{
			primary_ammo[WEAPON_DEAGLE] = m_rgAmmo[i];
			if( weapon_ptr[WEAPON_DEAGLE] )
				primary_ammo[WEAPON_DEAGLE] += ((CBasePlayerWeapon *)weapon_ptr[WEAPON_DEAGLE])->m_iClip;
		}
		else if( strcmp( "mrcbullets", CBasePlayerItem::AmmoInfoArray[i].pszName ) == 0 )
		{
			primary_ammo[WEAPON_MRC] = m_rgAmmo[i];
			if( weapon_ptr[WEAPON_MRC] )
				primary_ammo[WEAPON_MRC] += ((CBasePlayerWeapon *)weapon_ptr[WEAPON_MRC])->m_iClip;
		}
		else if( strcmp( "g36cammo", CBasePlayerItem::AmmoInfoArray[i].pszName ) == 0 )
		{
			primary_ammo[WEAPON_G36C] = m_rgAmmo[i];
			if( weapon_ptr[WEAPON_G36C] )
				primary_ammo[WEAPON_G36C] += ((CBasePlayerWeapon *)weapon_ptr[WEAPON_G36C])->m_iClip;
		}
		else if( strcmp( "hkmp5ammo", CBasePlayerItem::AmmoInfoArray[i].pszName ) == 0 )
		{
			primary_ammo[WEAPON_HKMP5] = m_rgAmmo[i];
			if( weapon_ptr[WEAPON_HKMP5] )
				primary_ammo[WEAPON_HKMP5] += ((CBasePlayerWeapon *)weapon_ptr[WEAPON_HKMP5])->m_iClip;
		}
		else if( strcmp( "ammo57", CBasePlayerItem::AmmoInfoArray[i].pszName ) == 0 )
		{
			primary_ammo[WEAPON_FIVESEVEN] = m_rgAmmo[i];
			if( weapon_ptr[WEAPON_FIVESEVEN] )
				primary_ammo[WEAPON_FIVESEVEN] += ((CBasePlayerWeapon *)weapon_ptr[WEAPON_FIVESEVEN])->m_iClip;
		}
		else if( strcmp( "ARgrenades", CBasePlayerItem::AmmoInfoArray[i].pszName ) == 0 )
		{
			secondary_ammo[WEAPON_MRC] = m_rgAmmo[i];
		}
		else if (strcmp("bolts", CBasePlayerItem::AmmoInfoArray[i].pszName) == 0)
		{
			primary_ammo[WEAPON_CROSSBOW] = m_rgAmmo[i];
			if( weapon_ptr[WEAPON_CROSSBOW] )
				primary_ammo[WEAPON_CROSSBOW] += ((CBasePlayerWeapon *)weapon_ptr[WEAPON_CROSSBOW])->m_iClip;
		}
		else if (stricmp("buckshot", CBasePlayerItem::AmmoInfoArray[i].pszName) == 0)
		{
			primary_ammo[WEAPON_SHOTGUN] = m_rgAmmo[i];
			if( weapon_ptr[WEAPON_SHOTGUN] )
				primary_ammo[WEAPON_SHOTGUN] += ((CBasePlayerWeapon *)weapon_ptr[WEAPON_SHOTGUN])->m_iClip;
			primary_ammo[WEAPON_SHOTGUN_XM] = m_rgAmmo[i];
			if( weapon_ptr[WEAPON_SHOTGUN_XM] )
				primary_ammo[WEAPON_SHOTGUN_XM] += ((CBasePlayerWeapon *)weapon_ptr[WEAPON_SHOTGUN_XM])->m_iClip;
		}
		else if( stricmp( "rockets", CBasePlayerItem::AmmoInfoArray[i].pszName ) == 0 )
		{
			primary_ammo[WEAPON_RPG] = m_rgAmmo[i];
			if( weapon_ptr[WEAPON_RPG] )
				primary_ammo[WEAPON_RPG] += ((CBasePlayerWeapon *)weapon_ptr[WEAPON_RPG])->m_iClip;
		}
		else if( stricmp( "sniperammo", CBasePlayerItem::AmmoInfoArray[i].pszName ) == 0 )
		{
			primary_ammo[WEAPON_SNIPER] = m_rgAmmo[i];
			if( weapon_ptr[WEAPON_SNIPER] )
				primary_ammo[WEAPON_SNIPER] += ((CBasePlayerWeapon *)weapon_ptr[WEAPON_SNIPER])->m_iClip;
		}
		else if (strcmp("uranium", CBasePlayerItem::AmmoInfoArray[i].pszName) == 0)
		{
			primary_ammo[WEAPON_GAUSS] = m_rgAmmo[i];
		}
		else if( strcmp( "ar2ammo", CBasePlayerItem::AmmoInfoArray[i].pszName ) == 0 )
		{
			primary_ammo[WEAPON_AR2] = m_rgAmmo[i];
			if( weapon_ptr[WEAPON_AR2] )
				primary_ammo[WEAPON_AR2] += ((CBasePlayerWeapon *)weapon_ptr[WEAPON_AR2])->m_iClip;
		}
		else if( strcmp( "ar2balls", CBasePlayerItem::AmmoInfoArray[i].pszName ) == 0 )
		{
			secondary_ammo[WEAPON_AR2] = m_rgAmmo[i];
		}
		else if (stricmp("Hand Grenade", CBasePlayerItem::AmmoInfoArray[i].pszName) == 0)
		primary_ammo[WEAPON_HANDGRENADE] = m_rgAmmo[i];
		else if (stricmp("Trip Mine", CBasePlayerItem::AmmoInfoArray[i].pszName) == 0)
		primary_ammo[WEAPON_TRIPMINE] = m_rgAmmo[i];
		else if (stricmp("Satchel Charge", CBasePlayerItem::AmmoInfoArray[i].pszName) == 0)
		primary_ammo[WEAPON_SATCHEL] = m_rgAmmo[i];
		else if (stricmp("Snarks", CBasePlayerItem::AmmoInfoArray[i].pszName) == 0)
		primary_ammo[WEAPON_SNARK] = m_rgAmmo[i];
	}
}


// specifing a weapon_choice allows you to choose the weapon the bot will
// use (assuming enough ammo exists for that weapon)
// BotFireWeapon will return TRUE if weapon was fired, FALSE otherwise
// primary is used to indicate whether you want primary or secondary fire
// if you have specified a weapon using weapon_choice

BOOL CBot::BotFireWeapon( Vector v_enemy_origin, int weapon_choice, BOOL primary )
{
   CBasePlayerItem *new_weapon;
   BOOL enemy_below;

   // is it time to check weapons inventory yet?
   if (f_weapon_inventory_time <= gpGlobals->time)
   {
	  // check weapon and ammo inventory then update check time...
	  BotWeaponInventory();
	  f_weapon_inventory_time = gpGlobals->time + 1.0;
   }

   Vector v_enemy = v_enemy_origin - GetGunPosition( );

   float distance = v_enemy.Length();  // how far away is the enemy?

   // is enemy at least 45 units below bot? (for handgrenades and snarks)
   if (v_enemy_origin.z < (pev->origin.z - 45))
	  enemy_below = TRUE;
   else
	  enemy_below = FALSE;

   // do electroblast
   if( BlastChargesReady > 0 && distance < 200 && (gpGlobals->time > last_electroblast_time + 15) )
   {
	   last_electroblast_time = gpGlobals->time;
	   ElectroblastButton = true;
	   // set next time to "shoot"
	   f_shoot_time = gpGlobals->time + 0.2 + (bot_skill * 0.1);
	   return TRUE;
   }

   if( IsCamping )
   {
		goto camping_weapons;
   }

	// if bot is carrying the crowbar...
	if (HasWeapon(WEAPON_KNIFE))
	{
		// if close to enemy, and skill level is 1, 2 or 3, use the crowbar
		if (((distance <= BOT_KNIFE_DISTANCE) /*&& (bot_skill <= 2)*/ && (weapon_choice == 0)) || (weapon_choice == WEAPON_KNIFE))
		{
			new_weapon = weapon_ptr[WEAPON_KNIFE];

			// check if the bot isn't already using this item...
			if( m_pActiveItem != new_weapon )
			{
				SelectItem( "weapon_knife" );
				stop_hold_attack_button_time = 0;
				IsHoldingAttackButton = false;
			}

			bool use_secondary = (RANDOM_LONG( 0, 100 ) > 75);
			if( use_secondary )
				pev->button |= IN_ATTACK2;
			else
				pev->button |= IN_ATTACK;  // use primary attack (whack! whack!)

			// set next time to "shoot"
			f_shoot_time = gpGlobals->time + 0.1 + RANDOM_FLOAT(primary_fire_delay[WEAPON_KNIFE][bot_skill][0], primary_fire_delay[WEAPON_KNIFE][bot_skill][1]);
			return TRUE;
		}
	}

   // if bot is carrying any hand grenades and enemy is below bot...
   if ((HasWeapon(WEAPON_HANDGRENADE)) && (enemy_below))
   {
	  long use_grenade = RANDOM_LONG(1,100);

	  // use hand grenades about 30% of the time...
	  if (((distance > 250) && (distance < 750) &&
		   (weapon_choice == 0) && (use_grenade <= 30)) ||
		  (weapon_choice == WEAPON_HANDGRENADE))
	  {
// BigGuy - START
		 new_weapon = weapon_ptr[WEAPON_HANDGRENADE];

		 // check if the bot isn't already using this item...
		 if( m_pActiveItem != new_weapon )
		 {
			 SelectItem( "weapon_handgrenade" );  // select the hand grenades
			 stop_hold_attack_button_time = 0;
			 IsHoldingAttackButton = false;
		 }

		 pev->button |= IN_ATTACK;  // use primary attack (boom!)

		 // set next time to "shoot"
		 f_shoot_time = gpGlobals->time + 0.1 + 
			RANDOM_FLOAT(primary_fire_delay[WEAPON_HANDGRENADE][bot_skill][0],
						 primary_fire_delay[WEAPON_HANDGRENADE][bot_skill][1]);
		 return TRUE;
// BigGuy - END
	  }
   }

   if( (HasWeapon( WEAPON_SHOTGUN_XM )) && (pev->waterlevel != 3) )
   {
	   // if close enough for good shotgun blasts...
	   if( ((distance < 700) && (weapon_choice == 0)) ||
		   (weapon_choice == WEAPON_SHOTGUN_XM) )
	   {
		   new_weapon = weapon_ptr[WEAPON_SHOTGUN_XM];

		   // check if the bot has any ammo left for this weapon...
		   if( primary_ammo[WEAPON_SHOTGUN_XM] > 0 )
		   {
			   // check if the bot isn't already using this item...
			   if( m_pActiveItem != new_weapon )
			   {
				   SelectItem( "weapon_shotgun_xm" );  // select the shotgun
				   stop_hold_attack_button_time = 0;
				   IsHoldingAttackButton = false;
			   }

			   long use_secondary = RANDOM_LONG( 1, 100 );

			   // use secondary attack about 30% of the time
			   if( (use_secondary <= 30) && (primary_ammo[WEAPON_SHOTGUN_XM] >= 2) )
			   {
				   // BigGuy - START
				   pev->button |= IN_ATTACK2;  // use secondary attack (bang! bang!)

				   // set next time to shoot
				   f_shoot_time = gpGlobals->time + 1.0 +
					   RANDOM_FLOAT( secondary_fire_delay[WEAPON_SHOTGUN_XM][bot_skill][0],
						   secondary_fire_delay[WEAPON_SHOTGUN_XM][bot_skill][1] );
			   }
			   // BigGuy - END
			   else
			   {
				   pev->button |= IN_ATTACK;  // use primary attack (bang! bang!)

				   // set next time to shoot
				   f_shoot_time = gpGlobals->time + 0.75 +
					   RANDOM_FLOAT( primary_fire_delay[WEAPON_SHOTGUN_XM][bot_skill][0],
						   primary_fire_delay[WEAPON_SHOTGUN_XM][bot_skill][1] );
			   }

			   return TRUE;
		   }
	   }
   }

   // if the bot is carrying the shotgun (can't use underwater)...
   if ((HasWeapon(WEAPON_SHOTGUN)) && (pev->waterlevel != 3))
   {
	  // if close enough for good shotgun blasts...
	  if (((distance < 600) && (weapon_choice == 0)) ||
		  (weapon_choice == WEAPON_SHOTGUN))
	  {
		 new_weapon = weapon_ptr[WEAPON_SHOTGUN];

		 // check if the bot has any ammo left for this weapon...
		 if (primary_ammo[WEAPON_SHOTGUN] > 0)
		 {
			// check if the bot isn't already using this item...
			 if( m_pActiveItem != new_weapon )
			 {
				 SelectItem( "weapon_shotgun" );  // select the shotgun
				 stop_hold_attack_button_time = 0;
				 IsHoldingAttackButton = false;
			 }

			long use_secondary = RANDOM_LONG(1,100);

			// use secondary attack about 30% of the time
			if ((use_secondary <= 30) && (primary_ammo[WEAPON_SHOTGUN] >= 2))
			{
			   pev->button |= IN_ATTACK2;  // use secondary attack (bang! bang!)

			   // set next time to shoot
			   f_shoot_time = gpGlobals->time + 1.0 +
				  RANDOM_FLOAT(secondary_fire_delay[WEAPON_SHOTGUN][bot_skill][0],
							   secondary_fire_delay[WEAPON_SHOTGUN][bot_skill][1]);
			}
			else
			{
			   pev->button |= IN_ATTACK;  // use primary attack (bang! bang!)

			   // set next time to shoot
			   f_shoot_time = gpGlobals->time + 0.75 +
				  RANDOM_FLOAT(primary_fire_delay[WEAPON_SHOTGUN][bot_skill][0],
							   primary_fire_delay[WEAPON_SHOTGUN][bot_skill][1]);
			}

			return TRUE;
		 }
	  }
   }

   // if the bot is carrying the 357/DEAGLE, (can't use underwater)...
   if ((HasWeapon(WEAPON_DEAGLE)) && (pev->waterlevel != 3))
   {
	  // if close enough for 357 shot...
	  if (((distance < 1000) && (weapon_choice == 0)) ||
		  (weapon_choice == WEAPON_DEAGLE))
	  {
		 new_weapon = weapon_ptr[WEAPON_DEAGLE];

		 // check if the bot has any ammo left for this weapon...
		 if (primary_ammo[WEAPON_DEAGLE] > 0)
		 {
			// check if the bot isn't already using this item...
			 if( m_pActiveItem != new_weapon )
			 {
				 SelectItem( "weapon_357" );
				 stop_hold_attack_button_time = 0;
				 IsHoldingAttackButton = false;
			 }

			pev->button |= IN_ATTACK;  // use primary attack (bang! bang!)

			// set next time to shoot
			f_shoot_time = gpGlobals->time + 0.75 +
			   RANDOM_FLOAT(primary_fire_delay[WEAPON_DEAGLE][bot_skill][0],
							primary_fire_delay[WEAPON_DEAGLE][bot_skill][1]);

			return TRUE;
		 }
	  }
   }

   if( (HasWeapon( WEAPON_FIVESEVEN )) && (pev->waterlevel != 3) )
   {
	   // if close enough for 357 shot...
	   if( ((distance < 1000) && (weapon_choice == 0)) ||
		   (weapon_choice == WEAPON_FIVESEVEN) )
	   {
		   new_weapon = weapon_ptr[WEAPON_FIVESEVEN];

		   // check if the bot has any ammo left for this weapon...
		   if( primary_ammo[WEAPON_FIVESEVEN] > 0 )
		   {
			   // check if the bot isn't already using this item...
			   if( m_pActiveItem != new_weapon )
			   {
				   SelectItem( "weapon_fiveseven" );
				   stop_hold_attack_button_time = 0;
				   IsHoldingAttackButton = false;
			   }

			   pev->button |= IN_ATTACK;  // use primary attack (bang! bang!)

			   // set next time to shoot
			   f_shoot_time = gpGlobals->time + 0.25 +
				   RANDOM_FLOAT( primary_fire_delay[WEAPON_FIVESEVEN][bot_skill][0],
					   primary_fire_delay[WEAPON_FIVESEVEN][bot_skill][1] );

			   return TRUE;
		   }
	   }
   }

	// if the bot is carrying the MRC (can't use underwater)...
	if ((HasWeapon(WEAPON_MRC)) && (pev->waterlevel != 3))
	{
		long use_secondary = RANDOM_LONG(1,100);

		// use secondary attack about 15% of the time...
		if (((distance > 300) && (distance < 1000) && (weapon_choice == 0) && (use_secondary <= 15)) || ((weapon_choice == WEAPON_MRC) && (primary == FALSE)))
		{
			// at some point we need to fire upwards in the air slightly
			// for long distance kills.  for right now, just fire the
			// grenade at the poor sucker.

			new_weapon = weapon_ptr[WEAPON_MRC];

			// check if the bot has any ammo left for this weapon...
			if (secondary_ammo[WEAPON_MRC] > 0)
			{
				// check if the bot isn't already using this item...
				if( m_pActiveItem != new_weapon )
				{
					SelectItem( "weapon_9mmAR" );  // select the 9mmAR (MP5)
					stop_hold_attack_button_time = 0;
					IsHoldingAttackButton = false;
				}

				// aim correctly
				Vector EnemyCalculatedPos = v_enemy_origin;
				EnemyCalculatedPos += ((EnemyCalculatedPos - pev->origin).Length() / 800) * pBotEnemy->pev->velocity;
				Vector vecToss = VecCheckThrow( pev, GetGunPosition(), EnemyCalculatedPos, 800, 0.5 );
				pev->v_angle = UTIL_VecToAngles( vecToss );

				if( pev->v_angle.x > 180 )
					pev->v_angle.x -= 360;

				if( pev->v_angle.y > 180 )
					pev->v_angle.y -= 360;

				pev->angles.x = -pev->v_angle.x / 3;
				pev->angles.y = pev->v_angle.y;
				pev->angles.z = 0;

				pev->idealpitch = pev->v_angle.x;
				BotFixIdealPitch();

				pev->button |= IN_ATTACK2;  // use secodnary attack (boom!)

				f_move_speed = 0;

				// set next time to shoot
				// if( RANDOM_LONG( 0, 100 ) > 75 )
				f_shoot_time = gpGlobals->time + RANDOM_FLOAT( secondary_fire_delay[WEAPON_MRC][bot_skill][0], secondary_fire_delay[WEAPON_MRC][bot_skill][1] );
				//else
				//f_shoot_time = gpGlobals->time;

				return TRUE;
			}
		}

		// if close enough for good MP5 shot...
		if (((distance < 1000) && (weapon_choice == 0)) || (weapon_choice == WEAPON_MRC))
		{
			new_weapon = weapon_ptr[WEAPON_MRC];

			// check if the bot has any ammo left for this weapon...
			if (primary_ammo[WEAPON_MRC] > 0)
			{
				// check if the bot isn't already using this item...
				if( m_pActiveItem != new_weapon )
				{
					SelectItem( "weapon_9mmAR" );  // select the 9mmAR (MP5)
					stop_hold_attack_button_time = 0;
					IsHoldingAttackButton = false;
				}

				pev->button |= IN_ATTACK;  // use primary attack (bang! bang!)

				if( !IsHoldingAttackButton && stop_hold_attack_button_time == 0 )
				{
					IsHoldingAttackButton = true;
					stop_hold_attack_button_time = gpGlobals->time + RANDOM_FLOAT( 0.3, 0.7 );
				}

				if( IsHoldingAttackButton )
				{
					if( stop_hold_attack_button_time < gpGlobals->time )
					{
						stop_hold_attack_button_time = 0;
						f_shoot_time = gpGlobals->time + RANDOM_FLOAT( primary_fire_delay[WEAPON_MRC][bot_skill][0], primary_fire_delay[WEAPON_MRC][bot_skill][1] );
						IsHoldingAttackButton = false;
					}
					else
						f_shoot_time = gpGlobals->time;
				}

				// set next time to shoot
				//	if( RANDOM_LONG( 0, 100 ) > 75 )
				//		f_shoot_time = gpGlobals->time + RANDOM_FLOAT( primary_fire_delay[WEAPON_MRC][bot_skill][0], primary_fire_delay[WEAPON_MRC][bot_skill][1] );
				//	else
				//f_shoot_time = gpGlobals->time;

				return TRUE;
			}
		}
	}

	if( (HasWeapon( WEAPON_AR2 )) && (pev->waterlevel != 3) )
	{
		long use_secondary = RANDOM_LONG( 1, 100 );

		// use secondary attack about 15% of the time...
		if( ((distance > 300) && (distance < 1500) && (weapon_choice == 0) && (use_secondary <= 15)) || ((weapon_choice == WEAPON_AR2) && (primary == FALSE)) )
		{
			new_weapon = weapon_ptr[WEAPON_AR2];

			// check if the bot has any ammo left for this weapon...
			if( secondary_ammo[WEAPON_AR2] > 0 )
			{
				// check if the bot isn't already using this item...
				if( m_pActiveItem != new_weapon )
				{
					SelectItem( "weapon_ar2" );
					stop_hold_attack_button_time = 0;
					IsHoldingAttackButton = false;
				}

				pev->button |= IN_ATTACK2;  // use secondary attack (boom!)

				// set next time to shoot
				f_shoot_time = gpGlobals->time + 0.5 + RANDOM_FLOAT( secondary_fire_delay[WEAPON_AR2][bot_skill][0], secondary_fire_delay[WEAPON_AR2][bot_skill][1] );

				return TRUE;
			}
		}

		// if close enough for good AR2 shot...
		if( ((distance < 1200) && (weapon_choice == 0)) || (weapon_choice == WEAPON_AR2) )
		{
			new_weapon = weapon_ptr[WEAPON_AR2];

			// check if the bot has any ammo left for this weapon...
			if( primary_ammo[WEAPON_AR2] > 0 )
			{
				// check if the bot isn't already using this item...
				if( m_pActiveItem != new_weapon )
				{
					SelectItem( "weapon_ar2" );
					stop_hold_attack_button_time = 0;
					IsHoldingAttackButton = false;
				}

				pev->button |= IN_ATTACK;  // use primary attack (bang! bang!)

				if( !IsHoldingAttackButton && stop_hold_attack_button_time == 0 )
				{
					IsHoldingAttackButton = true;
					stop_hold_attack_button_time = gpGlobals->time + RANDOM_FLOAT( 0.3, 0.7 );
				}

				if( IsHoldingAttackButton )
				{
					if( stop_hold_attack_button_time < gpGlobals->time )
					{
						stop_hold_attack_button_time = 0;
						f_shoot_time = gpGlobals->time + RANDOM_FLOAT( primary_fire_delay[WEAPON_AR2][bot_skill][0], primary_fire_delay[WEAPON_AR2][bot_skill][1] );
						IsHoldingAttackButton = false;
					}
					else
						f_shoot_time = gpGlobals->time;
				}

				// set next time to shoot
				//	if( RANDOM_LONG( 0, 100 ) > 75 )
				//		f_shoot_time = gpGlobals->time + RANDOM_FLOAT( primary_fire_delay[WEAPON_AR2][bot_skill][0], primary_fire_delay[WEAPON_AR2][bot_skill][1] );
				//	else
				//f_shoot_time = gpGlobals->time;

				return TRUE;
			}
		}
	}

	if( (HasWeapon( WEAPON_HKMP5 )) && (pev->waterlevel != 3) )
	{
		if( ((distance < 1000) && (weapon_choice == 0)) || (weapon_choice == WEAPON_HKMP5) )
		{
			new_weapon = weapon_ptr[WEAPON_HKMP5];

			// check if the bot has any ammo left for this weapon...
			if( primary_ammo[WEAPON_HKMP5] > 0 )
			{
				// check if the bot isn't already using this item...
				if( m_pActiveItem != new_weapon )
				{
					SelectItem( "weapon_hkmp5" );
					stop_hold_attack_button_time = 0;
					IsHoldingAttackButton = false;
				}

				pev->button |= IN_ATTACK;  // use primary attack (bang! bang!)

				if( !IsHoldingAttackButton && stop_hold_attack_button_time == 0 )
				{
					IsHoldingAttackButton = true;
					stop_hold_attack_button_time = gpGlobals->time + RANDOM_FLOAT( 0.3, 0.7 );
				}

				if( IsHoldingAttackButton )
				{
					if( stop_hold_attack_button_time < gpGlobals->time )
					{
						stop_hold_attack_button_time = 0;
						f_shoot_time = gpGlobals->time + RANDOM_FLOAT( primary_fire_delay[WEAPON_HKMP5][bot_skill][0], primary_fire_delay[WEAPON_HKMP5][bot_skill][1] );
						IsHoldingAttackButton = false;
					}
					else
						f_shoot_time = gpGlobals->time;
				}

				// set next time to shoot
				//	if( RANDOM_LONG( 0, 100 ) > 75 )
				//		f_shoot_time = gpGlobals->time + RANDOM_FLOAT( primary_fire_delay[WEAPON_HKMP5][bot_skill][0], primary_fire_delay[WEAPON_HKMP5][bot_skill][1] );
				//	else
				//f_shoot_time = gpGlobals->time;

				return TRUE;
			}
		}
	}

camping_weapons:
	//===================== WEAPON_GAUSS =======================================================================
	// if the bot is carrying the gauss gun (can't use underwater)...
	if ((HasWeapon(WEAPON_GAUSS)) && (pev->waterlevel != 3))
	{
		if ((weapon_choice == 0) || (weapon_choice == WEAPON_GAUSS))
		{
			new_weapon = weapon_ptr[WEAPON_GAUSS];

			// check if the bot has any ammo left for this weapon...
			if (primary_ammo[WEAPON_GAUSS] > 1)
			{
				// check if the bot isn't already using this item...
				if( m_pActiveItem != new_weapon )
				{
					SelectItem( "weapon_gausniper" );  // select the gauss gun
					stop_hold_attack_button_time = 0;
					IsHoldingAttackButton = false;
				}

				// diffusion - it's a sniper rifle, no charging
				long use_secondary = 0;// RANDOM_LONG( 1, 100 );

				// are we charging the gauss gun?
				if (f_fire_gauss > 0)
				{
					// is it time to fire the charged gauss gun?
					if (f_fire_gauss >= gpGlobals->time)
					{
						// we DON'T set pev->button here to release the secondary
						// fire button which will fire the charged gauss gun

						f_fire_gauss = -1;  // -1 means not charging gauss gun

						// set next time to shoot
						f_shoot_time = gpGlobals->time + 1.0 + RANDOM_FLOAT(secondary_fire_delay[WEAPON_GAUSS][bot_skill][0], secondary_fire_delay[WEAPON_GAUSS][bot_skill][1]);
					}
					else
					{
						pev->button |= IN_ATTACK;  // charge the gauss gun
						f_shoot_time = gpGlobals->time;  // keep charging
					}
				}
				else if ((use_secondary <= 20) && (primary_ammo[WEAPON_GAUSS] >= 10))
				{
					// release secondary fire in 0.5 seconds...
					f_fire_gauss = gpGlobals->time + 0.5;

					pev->button |= IN_ATTACK;  // charge the gauss gun
					f_shoot_time = gpGlobals->time; // keep charging
				}
				else
				{
					pev->button |= IN_ATTACK;  // use primary attack (bang! bang!)

					// set next time to shoot
					f_shoot_time = gpGlobals->time + 0.2 + RANDOM_FLOAT(primary_fire_delay[WEAPON_GAUSS][bot_skill][0], primary_fire_delay[WEAPON_GAUSS][bot_skill][1]);
				}

				return TRUE;
			}
		}
	}
	//===================== WEAPON_CROSSBOW =======================================================================
	// if the bot is carrying the crossbow...
	if (HasWeapon(WEAPON_CROSSBOW))
	{
		// if bot is not too close for crossbow and not too far...
		if (( (IsCamping || ((distance > 250) && (distance < 1500)) ) && (weapon_choice == 0)) || (weapon_choice == WEAPON_CROSSBOW))
		{
			new_weapon = weapon_ptr[WEAPON_CROSSBOW];

			// check if the bot has any ammo left for this weapon...
			if (primary_ammo[WEAPON_CROSSBOW] > 0)
			{
				// check if the bot isn't already using this item...
				if( m_pActiveItem != new_weapon )
				{
					SelectItem( "weapon_crossbow" );  // select the crossbow
					stop_hold_attack_button_time = 0;
					IsHoldingAttackButton = false;
				}

				// aim correctly
				Vector EnemyCalculatedPos = v_enemy_origin;
				EnemyCalculatedPos += ((EnemyCalculatedPos - pev->origin).Length() / 3500) * pBotEnemy->pev->velocity;
				Vector vecToss = VecCheckThrow( pev, GetGunPosition(), EnemyCalculatedPos, 3500, 0.35 ); // 3500 is BOLT_AIR_VELOCITY, 0.35 is pev->gravity of the bolt
				pev->v_angle = UTIL_VecToAngles( vecToss );

				if( pev->v_angle.x > 180 )
					pev->v_angle.x -= 360;

				if( pev->v_angle.y > 180 )
					pev->v_angle.y -= 360;

				pev->angles.x = -pev->v_angle.x / 3;
				pev->angles.y = pev->v_angle.y;
				pev->angles.z = 0;

				pev->idealpitch = pev->v_angle.x;
				BotFixIdealPitch();

				pev->button |= IN_ATTACK;  // use primary attack (bang! bang!)

				// set next time to shoot
				f_shoot_time = gpGlobals->time + 0.75 + RANDOM_FLOAT(primary_fire_delay[WEAPON_CROSSBOW][bot_skill][0], primary_fire_delay[WEAPON_CROSSBOW][bot_skill][1]);

				return TRUE;
			}
		}
	}
	//===================== WEAPON_SNIPER =======================================================================
	if( HasWeapon( WEAPON_SNIPER ) )
	{
		// if bot is not too close for crossbow and not too far...
		if( ((IsCamping || ((distance > 300) && (distance < 3000))) && (weapon_choice == 0)) || (weapon_choice == WEAPON_SNIPER) )
		{
			new_weapon = weapon_ptr[WEAPON_SNIPER];

			// check if the bot has any ammo left for this weapon...
			if( primary_ammo[WEAPON_SNIPER] > 0 )
			{
				// check if the bot isn't already using this item...
				if( m_pActiveItem != new_weapon )
				{
					SelectItem( "weapon_sniper" );  // select the crossbow
					stop_hold_attack_button_time = 0;
					IsHoldingAttackButton = false;
				}

				pev->button |= IN_ATTACK;  // use primary attack (bang! bang!)

				// set next time to shoot
				f_shoot_time = gpGlobals->time + 0.75 + RANDOM_FLOAT( primary_fire_delay[WEAPON_SNIPER][bot_skill][0], primary_fire_delay[WEAPON_SNIPER][bot_skill][1] );

				return TRUE;
			}
		}
	}

	//===================== WEAPON_G36C =======================================================================
	if( (HasWeapon( WEAPON_G36C )) && (pev->waterlevel != 3) )
	{
		if( ((IsCamping || (distance < 1300)) && (weapon_choice == 0)) || (weapon_choice == WEAPON_G36C) )
		{
			new_weapon = weapon_ptr[WEAPON_G36C];

			// check if the bot has any ammo left for this weapon...
			if( primary_ammo[WEAPON_G36C] > 0 )
			{
				// check if the bot isn't already using this item...
				if( m_pActiveItem != new_weapon )
				{
					SelectItem( "weapon_g36c" );
					stop_hold_attack_button_time = 0;
					IsHoldingAttackButton = false;
				}

				pev->button |= IN_ATTACK;  // use primary attack (bang! bang!)

				if( !IsHoldingAttackButton && stop_hold_attack_button_time == 0 )
				{
					IsHoldingAttackButton = true;
					stop_hold_attack_button_time = gpGlobals->time + RANDOM_FLOAT( 0.3, 0.7 );
				}

				if( IsHoldingAttackButton )
				{
					if( stop_hold_attack_button_time < gpGlobals->time )
					{
						stop_hold_attack_button_time = 0;
						f_shoot_time = gpGlobals->time + RANDOM_FLOAT( primary_fire_delay[WEAPON_G36C][bot_skill][0], primary_fire_delay[WEAPON_G36C][bot_skill][1] );
						IsHoldingAttackButton = false;
					}
					else
						f_shoot_time = gpGlobals->time;
				}

				// set next time to shoot
				//   f_shoot_time = gpGlobals->time + RANDOM_FLOAT( primary_fire_delay[WEAPON_G36C][bot_skill][0], primary_fire_delay[WEAPON_G36C][bot_skill][1] );
				//f_shoot_time = gpGlobals->time;

				return TRUE;
			}
		}
	}

	//===================== WEAPON_RPG =======================================================================
	// if the bot is carrying the RPG...
	if (HasWeapon(WEAPON_RPG))
	{
		// don't use the RPG unless the enemy is pretty far away...
		if (((distance > 300) && (weapon_choice == 0)) || (weapon_choice == WEAPON_RPG))
		{
			new_weapon = weapon_ptr[WEAPON_RPG];

			// check if the bot has any ammo left for this weapon...
			if (primary_ammo[WEAPON_RPG] > 0)
			{
				// check if the bot isn't already using this item...
				if( m_pActiveItem != new_weapon )
				{
					SelectItem( "weapon_rpg" );  // select the RPG rocket launcher
					stop_hold_attack_button_time = 0;
					IsHoldingAttackButton = false;
				}

				pev->button |= IN_ATTACK;  // use primary attack (bang! bang!)

				// set next time to shoot
				f_shoot_time = gpGlobals->time + 1.5 + RANDOM_FLOAT(primary_fire_delay[WEAPON_RPG][bot_skill][0], primary_fire_delay[WEAPON_RPG][bot_skill][1]);

				return TRUE;
			}
		}
	}

	// if we got here, we can't camp - no suitable weapons
	IsCamping = false;
	camping_end_time = 0;
	f_duck_release_time = 0;

	//===================== WEAPON_BERETTA =======================================================================
	// if the bot is carrying the 9mm glock...
	if (HasWeapon(WEAPON_BERETTA))
	{
		// if nothing else was selected, try the good ol' 9mm glock...
		if (((distance < 1200) && (weapon_choice == 0)) || (weapon_choice == WEAPON_BERETTA))
		{
			new_weapon = weapon_ptr[WEAPON_BERETTA];

			// check if the bot has any ammo left for this weapon...
			if (primary_ammo[WEAPON_BERETTA] > 0)
			{
				// check if the bot isn't already using this item...
				if( m_pActiveItem != new_weapon )
				{
					SelectItem( "weapon_9mmhandgun" );  // select the trusty 9mm glock
					stop_hold_attack_button_time = 0;
					IsHoldingAttackButton = false;
				}

				//	int use_secondary = RANDOM_LONG(1,100);
				bool use_secondary = (distance < 300);

				// use secondary attack about 30% of the time
				if (use_secondary)
				{
					pev->button |= IN_ATTACK2;  // use secondary attack (bang! bang!)

					// set next time to shoot
					//   f_shoot_time = gpGlobals->time + 0.2 + RANDOM_FLOAT(secondary_fire_delay[WEAPON_BERETTA][bot_skill][0], secondary_fire_delay[WEAPON_BERETTA][bot_skill][1]);
					f_shoot_time = gpGlobals->time;
				}
				else
				{
					pev->button |= IN_ATTACK;  // use primary attack (bang! bang!)

					// set next time to shoot
					f_shoot_time = gpGlobals->time + 0.3 + RANDOM_FLOAT(primary_fire_delay[WEAPON_BERETTA][bot_skill][0], primary_fire_delay[WEAPON_BERETTA][bot_skill][1]);
				}

				return TRUE;
			}
		}
	}

	// didn't have any available weapons or ammo, return FALSE
	return FALSE;
}


void CBot::BotShootAtEnemy( void )
{	
	float f_distance;

	// aim for the head and/or body
	Vector v_enemy = BotBodyTarget( pBotEnemy ) - GetGunPosition();

	pev->v_angle = UTIL_VecToAngles( v_enemy );

	if( pev->v_angle.x > 180 )
		pev->v_angle.x -= 360;

	if( pev->v_angle.y > 180 )
		pev->v_angle.y -= 360;

	pev->angles.x = -pev->v_angle.x / 3;
	pev->angles.y = pev->v_angle.y;
	pev->angles.z = 0;

	pev->idealpitch = pev->v_angle.x;
	BotFixIdealPitch();

	pev->ideal_yaw = pev->v_angle.y;
	BotFixIdealYaw();

	// is it time to shoot yet?
	if (f_shoot_time <= gpGlobals->time)
	{
		// select the best weapon to use at this distance and fire...
		BotFireWeapon( pBotEnemy->pev->origin );
	}

	v_enemy.z = 0;  // ignore z component (up & down)

	f_distance = v_enemy.Length();  // how far away is the enemy scum?

	if( !IsCamping )
	{
		if( f_distance > 300 )      // run if distance to enemy is far
			f_move_speed = f_max_speed;
		else if( f_distance > BOT_KNIFE_DISTANCE )  // walk if distance is closer
			f_move_speed = f_max_speed / 2;
		else                     // don't move if close enough
			f_move_speed = 0;

		if( f_distance > 800 )
		{
			if( f_duck_release_time > gpGlobals->time )
				f_move_speed = 0;
			
			// randomly decide to crouch
			if( RANDOM_LONG( 0, 100 ) > 75 )
			{
				f_duck_release_time = gpGlobals->time + 2;
			}
		}
	}
	else
		f_move_speed = 0.0;
}



