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
#ifndef WEAPONS_H
#define WEAPONS_H

#include "effects.h"
#include "game/user_messages.h"

class CBasePlayer;

void DeactivateSatchels( CBasePlayer *pOwner );
void PlayPickupSound( CBaseEntity *pPlayer, int Id = 250 );
void MakeWeaponShake( CBaseEntity *pPlayer, int Weapon, int Mode );
void PlayClientSound( CBaseEntity *pEntity, int Type, int Mode = 0, int LowAmmoVolume = 0, Vector org = g_vecZero );

#define SF_AMMO_DONTFALL BIT(0) // diffusion - this ammo doesn't fall to ground and stays in place

// Contact Grenade / Timed grenade / Satchel Charge
class CGrenade : public CBaseMonster
{
	DECLARE_CLASS( CGrenade, CBaseMonster );
public:
	void Spawn( void );

	typedef enum { SATCHEL_DETONATE = 0, SATCHEL_RELEASE } SATCHELCODE;

	static CGrenade *ShootTimed( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity, float time, bool IsEMP = false );
	static CGrenade *ShootContact( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity );
	static CGrenade *ShootSmoke( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity );
	static CGrenade *ShootSatchelCharge( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity );
	static void UseSatchelCharges( entvars_t *pevOwner, SATCHELCODE code );

	void Explode( Vector vecSrc, Vector vecAim );
	void Explode( TraceResult *pTrace, int bitsDamageType );
	void Smoke( void );

	void BounceTouch( CBaseEntity *pOther );
	void SlideTouch( CBaseEntity *pOther );
	void ExplodeTouch( CBaseEntity *pOther );
	void DangerSoundThink( void );
	void SmokeGrenadeThink( void );
	void PreDetonate( void );
	void Detonate( void );
	void SmokeGrenadeExplode( void );
	void DetonateUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void TumbleThink( void );
	void ClearEffects( void );

	DECLARE_DATADESC();

	virtual void BounceSound( void );
	virtual int BloodColor( void ) { return DONT_BLEED; }
	virtual void Killed( entvars_t *pevAttacker, int iGib );
	virtual BOOL IsProjectile( void ) { return TRUE; }

	BOOL m_fRegisteredSound;// whether or not this grenade has issued its DANGER sound to the world sound list yet.

	float LastBounceSoundTime; // diffusion - don't play sound too often

	bool SendWaterSplash;
	Vector OrgAboveWater; // last not-in-water valid position

	bool IsEMPGrenade;
};

#define MAX_NORMAL_BATTERY	100

// weapon weight factors (for auto-switching)   (-1 = noswitch)
#define CROWBAR_WEIGHT		0
#define BERETTA_WEIGHT		10
#define DEAGLE_WEIGHT		15
#define MP5_WEIGHT			15
#define SHOTGUN_WEIGHT		15
#define CROSSBOW_WEIGHT		10
#define RPG_WEIGHT			20
#define GAUSS_WEIGHT		20
#define EGON_WEIGHT			20
#define HORNETGUN_WEIGHT	10
#define HANDGRENADE_WEIGHT	5
#define SNARK_WEIGHT		5
#define SATCHEL_WEIGHT		-10
#define TRIPMINE_WEIGHT		-10

// weapon clip/carry ammo capacities
#define URANIUM_MAX_CARRY		10
#define AR2_MAX_CARRY			250
#define	_9MM_MAX_CARRY			200
#define DEAGLE_MAX_CARRY		21
#define BUCKSHOT_MAX_CARRY		60
#define BOLT_MAX_CARRY			15
#define ROCKET_MAX_CARRY		5
#define HANDGRENADE_MAX_CARRY	10
#define SMOKEGRENADE_MAX_CARRY	3
#define SATCHEL_MAX_CARRY		5
#define TRIPMINE_MAX_CARRY		5
#define SNARK_MAX_CARRY			15
#define HORNET_MAX_CARRY		8
#define M203_GRENADE_MAX_CARRY	4
#define SNIPER_MAX_CARRY		15
#define _57_MAX_CARRY			100

// the maximum amount of ammo each weapon's clip can hold
#define WEAPON_NOCLIP			-1

//#define CROWBAR_MAX_CLIP		WEAPON_NOCLIP
#define BERETTA_MAX_CLIP			15
#define DEAGLE_MAX_CLIP			7 
#define MRC_MAX_CLIP			50
#define MP5_MAX_CLIP			30
#define MP5_DEFAULT_AMMO		50  // 25? What were they smoking in Valve?
#define SHOTGUN_MAX_CLIP		12
#define SHOTGUNXM_MAX_CLIP		8
#define CROSSBOW_MAX_CLIP		5
#define RPG_MAX_CLIP			1
#define GAUSS_MAX_CLIP			50
#define EGON_MAX_CLIP			WEAPON_NOCLIP
#define HORNETGUN_MAX_CLIP		WEAPON_NOCLIP
#define HANDGRENADE_MAX_CLIP	WEAPON_NOCLIP
#define SATCHEL_MAX_CLIP		WEAPON_NOCLIP
#define TRIPMINE_MAX_CLIP		WEAPON_NOCLIP
#define SNARK_MAX_CLIP			WEAPON_NOCLIP
#define AR2_MAX_CLIP			30
#define SNIPER_MAX_CLIP			5
#define FIVESEVEN_MAX_CLIP		20
#define G36C_MAX_CLIP			30

// the default amount of ammo that comes with each gun when it spawns
#define BERETTA_DEFAULT_GIVE			BERETTA_MAX_CLIP * 2
#define DEAGLE_DEFAULT_GIVE			7
#define MRC_DEFAULT_GIVE			50
#define MP5_DEFAULT_GIVE			MP5_MAX_CLIP * 2
#define MRC_GR_DEFAULT_GIVE			0
#define SHOTGUN_DEFAULT_GIVE		12
#define CROSSBOW_DEFAULT_GIVE		5
#define RPG_DEFAULT_GIVE			1
#define GAUSS_DEFAULT_GIVE			100
#define EGON_DEFAULT_GIVE			20
#define HANDGRENADE_DEFAULT_GIVE	1
#define SATCHEL_DEFAULT_GIVE		1
#define TRIPMINE_DEFAULT_GIVE		1
#define SNARK_DEFAULT_GIVE			5
#define HIVEHAND_DEFAULT_GIVE		8
#define AR2_DEFAULT_GIVE			50
#define SNIPER_DEFAULT_GIVE			10
#define G36C_DEFAULT_GIVE			G36C_MAX_CLIP * 2

// The amount of ammo given to a player by an ammo item.
#define AMMO_URANIUMBOX_GIVE	5
#define AMMO_BERETTACLIP_GIVE	BERETTA_MAX_CLIP
#define AMMO_DEAGLEBOX_GIVE		DEAGLE_MAX_CLIP
#define AMMO_MRCCLIP_GIVE		MRC_MAX_CLIP
#define AMMO_CHAINBOX_GIVE		200
#define AMMO_M203BOX_GIVE		1
#define AMMO_BUCKSHOTBOX_GIVE	8
#define AMMO_CROSSBOWCLIP_GIVE	CROSSBOW_MAX_CLIP
#define AMMO_RPGCLIP_GIVE		RPG_MAX_CLIP
#define AMMO_SNARKBOX_GIVE		5
#define AMMO_SNIPER_GIVE		5
#define AMMO_FIVESEVEN_GIVE		FIVESEVEN_MAX_CLIP
#define AMMO_HKMP5_GIVE			MP5_MAX_CLIP
#define AMMO_G36C_GIVE			MP5_MAX_CLIP

// diffusion - weapon damage
#define DMG_WPN_MRC				15
#define DMG_WPN_CROSSBOW		110
//#define DMG_WPN_DEAGLE		using BULLET_PLAYER_357 damage (70)
#define DMG_WPN_FIVESEVEN		25
#define DMG_WPN_GAUSS			10
#define DMG_WPN_GAUSS_FULL		150
//#define DMG_WPN_HKMP5			using BULLET_PLAYER_MP5 damage (15)
#define DMG_WPN_PISTOL			20
//#define DMG_WPN_SHOTGUN		using BULLET_PLAYER_BUCKSHOT damage (13x6 primary, 13x12 sec.)
#define DMG_WEAPON_SHOTGUN_XM	15
#define DMG_WPN_KNIFE			27
#define DMG_WPN_SNIPER			150

// bullet types
typedef	enum
{
	BULLET_NONE = 0,
	BULLET_PLAYER_9MM, // glock
	BULLET_PLAYER_12MM, // sniper
	BULLET_PLAYER_MP5, // mp5
	BULLET_PLAYER_357, // python
	BULLET_PLAYER_BUCKSHOT, // shotgun
	BULLET_PLAYER_CROWBAR, // crowbar swipe

	BULLET_MONSTER_9MM,
	BULLET_MONSTER_MP5,
	BULLET_MONSTER_12MM,
} Bullet;


#define ITEM_FLAG_SELECTONEMPTY		1
#define ITEM_FLAG_NOAUTORELOAD		2
#define ITEM_FLAG_NOAUTOSWITCHEMPTY	4
#define ITEM_FLAG_LIMITINWORLD		8
#define ITEM_FLAG_EXHAUSTIBLE		16 // A player can totally exhaust their ammo supply and lose this weapon

#define WEAPON_IS_ONTARGET 0x40

typedef struct
{
	int		iSlot;
	int		iPosition;
	const char	*pszAmmo1;	// ammo 1 type
	int		iMaxAmmo1;		// max ammo 1
	const char	*pszAmmo2;	// ammo 2 type
	int		iMaxAmmo2;		// max ammo 2
	const char	*pszName;
	int		iMaxClip;
	int		iId;
	int		iFlags;
	int		iWeight;// this value used to determine this weapon's importance in autoselection.
} ItemInfo;

typedef struct
{
	const char *pszName;
	int iId;
} AmmoInfo;

// Items that the player has in their inventory that they can use
class CBasePlayerItem : public CBaseAnimating
{
	DECLARE_CLASS( CBasePlayerItem, CBaseAnimating );
public:
	virtual void SetObjectCollisionBox( void );

	DECLARE_DATADESC();

	virtual int AddToPlayer( CBasePlayer *pPlayer );	// return TRUE if the item you want the item added to the player inventory
	virtual int AddDuplicate( CBasePlayerItem *pItem ) { return FALSE; }	// return TRUE if you want your duplicate removed from world
	void DestroyItem( void );
	void DefaultTouch( CBaseEntity *pOther );	// default weapon touch
	void FallThink ( void );// when an item is first spawned, this think is run to determine when the object has hit the ground.
	void Materialize( void );// make a weapon visible and tangible
	void AttemptToMaterialize( void );  // the weapon desires to become visible and tangible, if the game rules allow for it
	CBaseEntity* Respawn ( void );// copy a weapon
	void FallInit( void );
	void CheckRespawn( void );
	virtual int GetItemInfo(ItemInfo *p) { return 0; };	// returns 0 if struct not filled out
	virtual BOOL CanDeploy( void ) { return TRUE; };
	virtual BOOL Deploy( ) { return TRUE; };		// returns is deploy was successful
		 

	virtual BOOL CanHolster( void ) { return TRUE; };		// can this weapon be put away right now?
	virtual void Holster( void );
	virtual void UpdateItemInfo( void ) { return; };

	virtual void ItemPreFrame( void ) { return; }		// called each frame by the player PreThink
	virtual void ItemPostFrame( void ) { return; }		// called each frame by the player PostThink

	virtual void Drop( void );
	virtual void Kill( void );
	virtual void AttachToPlayer ( CBasePlayer *pPlayer );

	virtual int PrimaryAmmoIndex() { return -1; };
	virtual int SecondaryAmmoIndex() { return -1; };

	virtual int UpdateClientData( CBasePlayer *pPlayer ) { return 0; }

	virtual CBasePlayerItem *GetWeaponPtr( void ) { return NULL; };

	static ItemInfo ItemInfoArray[ MAX_WEAPONS ];
	static AmmoInfo AmmoInfoArray[ MAX_AMMO_SLOTS ];

	CBasePlayer	*m_pPlayer;
	CBasePlayerItem	*m_pNext;
	int		m_iId;				// WEAPON_???

	virtual int iItemSlot( void ) { return 0; }		// return 0 to MAX_ITEMS_SLOTS, used in hud

	int		iItemPosition( void ) { return ItemInfoArray[ m_iId ].iPosition; }
	const char	*pszAmmo1( void )	{ return ItemInfoArray[ m_iId ].pszAmmo1; }
	int		iMaxAmmo1( void )	{ return ItemInfoArray[ m_iId ].iMaxAmmo1; }
	const char	*pszAmmo2( void )	{ return ItemInfoArray[ m_iId ].pszAmmo2; }
	int		iMaxAmmo2( void )	{ return ItemInfoArray[ m_iId ].iMaxAmmo2; }
	const char	*pszName( void )	{ return ItemInfoArray[ m_iId ].pszName; }
	int		iMaxClip( void )	{ return ItemInfoArray[ m_iId ].iMaxClip; }
	int		iWeight( void )	{ return ItemInfoArray[ m_iId ].iWeight; }
	int		iFlags( void )	{ return ItemInfoArray[ m_iId ].iFlags; }
};

// inventory items that 
class CBasePlayerWeapon : public CBasePlayerItem
{
	DECLARE_CLASS( CBasePlayerWeapon, CBasePlayerItem );
public:
	DECLARE_DATADESC();

	// generic weapon versions of CBasePlayerItem calls
	virtual int AddToPlayer( CBasePlayer *pPlayer );
	virtual int AddDuplicate( CBasePlayerItem *pItem );

	virtual int ExtractAmmo( CBasePlayerWeapon *pWeapon );		// Return TRUE if you can add ammo to yourself when picked up
	virtual int ExtractClipAmmo( CBasePlayerWeapon *pWeapon );		// Return TRUE if you can add ammo to yourself when picked up

	virtual int AddWeapon( void ) { ExtractAmmo( this ); return TRUE; };	// Return TRUE if you want to add yourself to the player

	// generic "shared" ammo handlers
	BOOL AddPrimaryAmmo( int iCount, char *szName, int iMaxClip, int iMaxCarry );
	BOOL AddSecondaryAmmo( int iCount, char *szName, int iMaxCarry );

	virtual void UpdateItemInfo( void ) {};	// updates HUD state

	int m_iPlayEmptySound;
	int m_fFireOnEmpty;		// True when the gun is empty and the player is still holding down the
							// attack key(s)
	virtual BOOL PlayEmptySound( void );
	virtual void ResetEmptySound( void );

	virtual void SendWeaponAnim( int iAnim, int skiplocal = 0, int body = 0 );  // skiplocal is 1 if client is predicting weapon animations

	virtual BOOL CanDeploy( void );
	virtual BOOL IsUseable( void );
	BOOL DefaultDeploy( char *szViewModel, char *szWeaponModel, int iAnim, char *szAnimExt, int skiplocal = 0, int body = 0 );
	int DefaultReload( int iClipSize, int iAnim, float fDelay, int body = 0 );

	virtual void ItemPostFrame( void );	// called each frame by the player PostThink
	// called by CBasePlayerWeapons ItemPostFrame()
	virtual void PrimaryAttack( void ) { return; }				// do "+ATTACK"
	virtual void SecondaryAttack( void ) { return; }			// do "+ATTACK2"
	virtual void Reload( void ) { return; }						// do "+RELOAD"
	virtual void WeaponIdle( void ) { return; }					// called when no buttons pressed
	virtual int UpdateClientData( CBasePlayer *pPlayer );		// sends hud info to client dll, if things have changed
	virtual void RetireWeapon( void );
	virtual BOOL ShouldWeaponIdle( void ) {return FALSE; };
	virtual void Holster( void );
	virtual BOOL UseDecrement( void ) { return FALSE; };
	virtual void LowAmmoMsg(CBasePlayer* pPlayer);
	
	int	PrimaryAmmoIndex(); 
	int	SecondaryAmmoIndex(); 

	void PrintState( void );

	virtual CBasePlayerItem *GetWeaponPtr( void ) { return (CBasePlayerItem *)this; };

	float m_flPumpTime;
	int	m_fInSpecialReload;			// Are we in the middle of a reload for the shotguns
	float	m_flNextPrimaryAttack;		// soonest time ItemPostFrame will call PrimaryAttack
	float	m_flNextSecondaryAttack;		// soonest time ItemPostFrame will call SecondaryAttack
	float	m_flTimeWeaponIdle;			// soonest time ItemPostFrame will call WeaponIdle
	int	m_iPrimaryAmmoType;			// "primary" ammo index into players m_rgAmmo[]
	int	m_iSecondaryAmmoType;		// "secondary" ammo index into players m_rgAmmo[]
	int	m_iClip;				// number of shots left in the primary weapon clip, -1 it not used
	int	m_iClientClip;			// the last version of m_iClip sent to hud dll
	int	m_iClientWeaponState;		// the last version of the weapon state sent to hud dll (is current weapon, is on target)
	int	m_fInReload;			// Are we in the middle of a reload;

	int	m_iDefaultAmmo;// how much ammo you get when you pick up this weapon as placed by a level designer.

};

class CBasePlayerAmmo : public CBaseEntity
{
	DECLARE_CLASS( CBasePlayerAmmo, CBaseEntity );
public:
	virtual void Spawn( void );
	void DefaultTouch( CBaseEntity *pOther ); // default weapon touch
	virtual BOOL AddAmmo( CBaseEntity *pOther ) { return TRUE; };

	CBaseEntity* Respawn( void );
	void Materialize( void );

	DECLARE_DATADESC();
};

extern DLL_GLOBAL	short	g_sModelIndexLaser;// holds the index for the laser beam
extern DLL_GLOBAL	const char *g_pModelNameLaser;

extern DLL_GLOBAL	short	g_sModelIndexLaserDot;// holds the index for the laser beam dot
extern DLL_GLOBAL	short	g_sModelIndexFireball;// holds the index for the fireball
extern DLL_GLOBAL	short	g_sModelIndexSmoke;// holds the index for the smoke cloud
extern DLL_GLOBAL	short	g_sModelIndexWExplosion;// holds the index for the underwater explosion
extern DLL_GLOBAL	short	g_sModelIndexBubbles;// holds the index for the bubbles model
extern DLL_GLOBAL	short	g_sModelIndexBloodDrop;// holds the sprite index for blood drops
//extern DLL_GLOBAL	short	g_sModelIndexBloodSpray;// holds the sprite index for blood spray (bigger)
extern DLL_GLOBAL	short	g_sModelIndexFire;
extern DLL_GLOBAL short g_sModelIndexMetalGibs;
extern DLL_GLOBAL short g_sShellsModel;

extern DLL_GLOBAL	short	g_sModelIndexSpore1;  //shocktrooper
extern DLL_GLOBAL	short	g_sModelIndexSpore2;
extern DLL_GLOBAL	short	g_sModelIndexSpore3;

extern DLL_GLOBAL  	short	g_sModelIndexBigSpit;
extern DLL_GLOBAL  	short	g_sModelIndexTinySpit;
extern DLL_GLOBAL short g_sBlastTexture;

extern void ClearMultiDamage(void);
extern void ApplyMultiDamage(entvars_t* pevInflictor, entvars_t* pevAttacker );
extern void AddMultiDamage( entvars_t *pevInflictor, CBaseEntity *pEntity, float flDamage, int bitsDamageType);

extern void DecalGunshot( TraceResult *pTrace, int iBulletType );
extern void SpawnBlood(Vector vecSpot, int bloodColor, float flDamage);
extern int DamageDecal( CBaseEntity *pEntity, int bitsDamageType );
extern void RadiusDamage( Vector vecSrc, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, float flRadius, int iClassIgnore, int bitsDamageType );

typedef struct 
{
	CBaseEntity		*pEntity;
	float			amount;
	int			type;
} MULTIDAMAGE;

extern MULTIDAMAGE gMultiDamage;


#define LOUD_GUN_VOLUME		1000
#define NORMAL_GUN_VOLUME		600
#define QUIET_GUN_VOLUME		200

#define BRIGHT_GUN_FLASH		512
#define NORMAL_GUN_FLASH		256
#define DIM_GUN_FLASH		128

#define BIG_EXPLOSION_VOLUME	2048
#define NORMAL_EXPLOSION_VOLUME	1024
#define SMALL_EXPLOSION_VOLUME	512

#define WEAPON_ACTIVITY_VOLUME	64

#define VECTOR_CONE_1DEGREES	Vector( 0.00873, 0.00873, 0.00873 )
#define VECTOR_CONE_2DEGREES	Vector( 0.01745, 0.01745, 0.01745 )
#define VECTOR_CONE_3DEGREES	Vector( 0.02618, 0.02618, 0.02618 )
#define VECTOR_CONE_4DEGREES	Vector( 0.03490, 0.03490, 0.03490 )
#define VECTOR_CONE_5DEGREES	Vector( 0.04362, 0.04362, 0.04362 )
#define VECTOR_CONE_6DEGREES	Vector( 0.05234, 0.05234, 0.05234 )
#define VECTOR_CONE_7DEGREES	Vector( 0.06105, 0.06105, 0.06105 )
#define VECTOR_CONE_8DEGREES	Vector( 0.06976, 0.06976, 0.06976 )
#define VECTOR_CONE_9DEGREES	Vector( 0.07846, 0.07846, 0.07846 )
#define VECTOR_CONE_10DEGREES	Vector( 0.08716, 0.08716, 0.08716 )
#define VECTOR_CONE_15DEGREES	Vector( 0.13053, 0.13053, 0.13053 )
#define VECTOR_CONE_20DEGREES	Vector( 0.17365, 0.17365, 0.17365 )

//=========================================================
// CWeaponBox - a single entity that can store weapons
// and ammo. 
//=========================================================
class CWeaponBox : public CBaseEntity
{
	DECLARE_CLASS( CWeaponBox, CBaseEntity );

	void Precache( void );
	void Spawn( void );
	void Touch( CBaseEntity *pOther );
	void KeyValue( KeyValueData *pkvd );
	BOOL IsEmpty( void );
	int  GiveAmmo( int iCount, char *szName, int iMax, int *pIndex = NULL );
	void SetObjectCollisionBox( void );

public:
	void Kill ( void );

	DECLARE_DATADESC();

	BOOL HasWeapon( CBasePlayerItem *pCheckItem );
	BOOL PackWeapon( CBasePlayerItem *pWeapon );
	BOOL PackAmmo( int iszName, int iCount );
	
	CBasePlayerItem	*m_rgpPlayerItems[MAX_ITEM_TYPES];// one slot for each 

	int m_rgiszAmmo[MAX_AMMO_SLOTS];// ammo names
	int m_rgAmmo[MAX_AMMO_SLOTS];// ammo quantities

	int m_cAmmoTypes;// how many ammo types packed into this box (if packed by a level designer)
};

class CLaserSpot : public CBaseEntity
{
	DECLARE_CLASS( CLaserSpot, CBaseEntity );

	void Spawn( void );
	void Precache( void );

	int ObjectCaps( void ) { return FCAP_DONT_SAVE; }

public:
	void Suspend( float flSuspendTime );
	void Revive( void );

	DECLARE_DATADESC();
	
	static CLaserSpot *CreateSpot( void );
};

#endif // WEAPONS_H