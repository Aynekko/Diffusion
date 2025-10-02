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
#ifndef CONST_H
#define CONST_H
//
// Constants shared by the engine and dlls
// This header file included by engine files and DLL files.
// Most came from server.h

// edict->flags
#define FL_FLY		(1<<0)	// Changes the SV_Movestep() behavior to not need to be on ground
#define FL_SWIM		(1<<1)	// Changes the SV_Movestep() behavior to not need to be on ground (but stay in water)
#define FL_CONVEYOR		(1<<2)
#define FL_CLIENT		(1<<3)
#define FL_INWATER		(1<<4)
#define FL_MONSTER		(1<<5)
#define FL_GODMODE		(1<<6)
#define FL_NOTARGET		(1<<7)
#define FL_SKIPLOCALHOST	(1<<8)	// Don't send entity to local host, it's predicting this entity itself
#define FL_ONGROUND		(1<<9)	// At rest / on the ground
#define FL_PARTIALGROUND	(1<<10)	// not all corners are valid
#define FL_WATERJUMP	(1<<11)	// player jumping out of water
#define FL_FROZEN		(1<<12)	// Player is frozen for 3rd person camera
#define FL_FAKECLIENT	(1<<13)	// JAC: fake client, simulated server side; don't send network messages to them
#define FL_DUCKING		(1<<14)	// Player flag -- Player is fully crouched
#define FL_FLOAT		(1<<15)	// Apply floating force to this entity when in water
#define FL_GRAPHED		(1<<16)	// worldgraph has this ent listed as something that blocks a connection

#define FL_ABSTRANSFORM	(1<<17)	// force to recalc entity absolute position (origin and angles)
#define FL_ABSVELOCITY	(1<<18)	// force to recalc entity absolute velocity
#define FL_ABSAVELOCITY	(1<<19)	// force to recalc entity absolute avelocity

#define FL_PROXY		(1<<20)	// This is a spectator proxy
#define FL_ALWAYSTHINK	(1<<21)	// Brush model flag -- call think every frame regardless of nextthink - ltime (for constantly changing velocity/path)
#define FL_BASEVELOCITY	(1<<22)	// Base velocity has been applied this frame (used to convert base velocity into momentum)
#define FL_MONSTERCLIP	(1<<23)	// Only collide in with monsters who have FL_MONSTERCLIP set
#define FL_ONTRAIN		(1<<24)	// Player is _controlling_ a train, so movement commands should be ignored on client during prediction.
#define FL_WORLDBRUSH	(1<<25)	// Not moveable/removeable brush entity (really part of the world, but represented as an entity for transparency or something)
#define FL_SPECTATOR	(1<<26)	// This client is a spectator, don't run touch functions, etc.
#define FL_STUCKED		(1<<27)	// Client or monster that stuck in the BSP-geometry or convex hull
#define FL_UNBLOCKABLE	(1<<28)	// pusher that can't be blocked by the player or npc
#define FL_CUSTOMENTITY	(1<<29)	// This is a custom entity
#define FL_KILLME		(1<<30)	// This entity is marked for death -- This allows the engine to kill ents at the appropriate time
#define FL_DORMANT		(1<<31)	// Entity is dormant, no updates to client

// diffusion - m_iFlags2
#define F_ENTITY_BUSY			BIT(0) // "BUSY" icon will be shown when looking at this entity
#define F_ENTITY_UNUSEABLE		BIT(1) // don't show anything
#define F_BUTTON_SECRET			BIT(2) // don't show any icon in any state of the button // UPD isn't this the same as bit(1)? lol
#define F_BUTTON_LOCKED			BIT(3) // show locked icon
#define F_WEAPON_DESPAWN		BIT(4) // multiplayer: dropped weapon. mark it, so it will despawn after some time
#define F_PLAYER_HASITEM		BIT(5) // player has holdable item!
#define F_ENTITY_ONCEUSED		BIT(6) // a flag which can be removed in the entity after a certain sequence
#define F_ENTITY_ONFIRE			BIT(7) // monster is on fire and taking damage
#define F_CUSTOMFLAG1			BIT(8) // to use inside an entity code
#define F_CUSTOMFLAG2			BIT(9)
#define F_CUSTOMFLAG3			BIT(10)
#define F_NOT_A_MONSTER			BIT(11) // I'm a monster, but this flag will tell that I'm not (FL_MONSTER is set, but we make it seem like it is not - currently for overriding damage achievement)
#define F_FIRE_IMMUNE			BIT(12) // can't be lit on fire with flares
#define F_FROM_AMMOBOX			BIT(13) // this is a weapon spawned by equip or ammocrate
#define F_MONSTER_CANT_LOSE_ENEMY		BIT(14) // this monster will never have "enemy lost" condition
#define F_METAL_MONSTER			BIT(15) // indicates that this is made of metal (to use appropriate sounds etc.) - UNDONE should be done better
#define F_PLAYER_MENU			BIT(16) // player is in the menu - that is to allow usage of "menuactivate" console command
#define F_BOT					BIT(17) // it's a bot
#define F_NOBACKCULL			BIT(18) // NOT CURRENTLY USED disable server back-culling for this entity (monsters, env_sky, etc.) - sv_fade_props_backcull
#define F_MONSTER_BIGNODE				BIT(19) // is a monster, which uses separate, big nodes. (alien ship, robo)
#define F_PLAYER_BUTTONFREEZED		BIT(20) // a button has indicated to freeze the player in place for 2 seconds
#define F_PLAYER_DRONE			BIT(21) // it's a _playerdrone
#define F_MONSTER_CAN_SEE_THROUGH BIT(22) // monsters can now see through transparent brushes, this is a workaround for mapper's purposes
#define F_PLAYER_CONTROL BIT(23) // this entity is controlled by player (i.e. drone)

// Goes into globalvars_t.trace_flags
#define FTRACE_SIMPLEBOX		(1<<0)	// Traceline with a simple box
#define FTRACE_IGNORE_GLASS		(1<<1)	// traceline will be ignored entities with rendermode != kRenderNormal

// walkmove modes
#define WALKMOVE_NORMAL		0	// normal walkmove
#define WALKMOVE_WORLDONLY		1	// doesn't hit ANY entities, no matter what the solid type
#define WALKMOVE_CHECKONLY		2	// move, but don't touch triggers

// edict->movetype values
#define MOVETYPE_NONE		0	// never moves
//#define	MOVETYPE_ANGLENOCLIP	1
//#define	MOVETYPE_ANGLECLIP		2
#define MOVETYPE_WALK		3	// Player only - moving on the ground
#define MOVETYPE_STEP		4	// gravity, special edge handling -- monsters use this
#define MOVETYPE_FLY		5	// No gravity, but still collides with stuff
#define MOVETYPE_TOSS		6	// gravity/collisions
#define MOVETYPE_PUSH		7	// no clip to world, push and crush
#define MOVETYPE_NOCLIP		8	// No gravity, no collisions, still do velocity/avelocity
#define MOVETYPE_FLYMISSILE		9	// extra size to monsters
#define MOVETYPE_BOUNCE		10	// Just like Toss, but reflect velocity when contacting surfaces
#define MOVETYPE_BOUNCEMISSILE	11	// bounce w/o gravity
#define MOVETYPE_FOLLOW		12	// track movement of aiment
#define MOVETYPE_PUSHSTEP		13	// BSP model that needs physics/world collisions (uses nearest hull for world collision)
#define MOVETYPE_COMPOUND		14	// glue two entities together (simple movewith)
#define MOVETYPE_PHYSIC		15	// implemenation of physic engine

// edict->solid values
// NOTE: Some movetypes will cause collisions independent of SOLID_NOT/SOLID_TRIGGER when the entity moves
// SOLID only effects OTHER entities colliding with this one when they move - UGH!
#define SOLID_NOT			0	// no interaction with other objects
#define SOLID_TRIGGER		1	// touch on edge, but not blocking
#define SOLID_BBOX			2	// touch on edge, block
#define SOLID_SLIDEBOX		3	// touch on edge, but not an onground
#define SOLID_BSP			4	// bsp clip, touch on edge, block
#define SOLID_CUSTOM		5	// call external callbacks for tracing

// edict->deadflag values
#define DEAD_NO			0 	// alive
#define DEAD_DYING			1 	// playing death animation or still falling off of a ledge waiting to hit ground
#define DEAD_DEAD			2 	// dead. lying still.
#define DEAD_RESPAWNABLE		3
#define DEAD_DISCARDBODY		4

#define DAMAGE_NO			0
#define DAMAGE_YES			1
#define DAMAGE_AIM			2

// entity effects
#define EF_BRIGHTFIELD		BIT(0)	// swirling cloud of particles
#define EF_MUZZLEFLASH		BIT(1)	// single frame ELIGHT on entity attachment 0
#define EF_BRIGHTLIGHT		BIT(2)	// DLIGHT centered at entity origin
#define EF_DIMLIGHT			BIT(3)	// player flashlight
#define EF_INVLIGHT			BIT(4)	// get lighting from ceiling
#define EF_NOINTERP			BIT(5)	// don't interpolate the next frame
#define EF_LIGHT			BIT(6)	// rocket flare glow sprite
#define EF_NODRAW			BIT(7)	// don't draw entity
#define EF_SKYCAMERA		BIT(8)	// marker for env_sky
#define EF_PORTAL			BIT(9)	// marker for portal entity
#define EF_SCREEN			BIT(10)	// marker for func_monitor entity
#define EF_SCREENMOVIE		BIT(11)	// marker for func_screenmovie
#define EF_NUKE_ROCKET		BIT(12)	// marker for controllable rocket
#define EF_PROJECTED_LIGHT	BIT(13)	// marker for env_projector
#define EF_DYNAMIC_LIGHT	BIT(14)	// marker for env_dynlight
#define EF_CONVEYOR			BIT(15)	// new conveyor-style scrolling

// diffusion new stuff
#define EF_MYLASERSPOT		BIT(16) // local player's laser_spot
#define EF_PLAYERUSINGCAMERA BIT(17) // flag that player is using camera. I need this to show cinematic borders...
#define EF_MONSTERFLASHLIGHT BIT(18) // monster has his flashlight on
#define EF_PLAYERDRONECONTROL	BIT(19) // player is in 1st person drone mode (needed for screen effect)
#define EF_SKIPPVS			BIT(20)	// this entity skips PVS check in AddToFullPack, thus always present at all times
#define EF_ROTATING			BIT(21) // client-side func_rotating - non-solid and very simple
#define EF_UPSIDEDOWN		BIT(22) // player is upside down
#define EF_NOLIGHTLERP		BIT(23) // no light lerping on studio model
#define EF_NOREFLECT		BIT(24)	// Entity won't reflecting in mirrors
#define EF_REFLECTONLY		BIT(25)	// Entity will be drawing only in mirrors
#define EF_WATERSIDES		BIT(26)	// Do not remove sides for func_water entity
#define EF_FULLBRIGHT		BIT(27) // used in engine
#define EF_NOSHADOW			BIT(28) // used in engine
#define EF_MERGE_VISIBILITY	BIT(29)	// this entity allowed to merge vis (e.g. env_sky or portal camera)
#define EF_REQUEST_PHS		BIT(30)	// This entity requested phs bitvector instead of pvsbitvector in AddToFullPack calls

// custom flags for func_monitor
#define CF_NOASPECT			(1<<0)	// don't calc fov_y for func_monitor
#define CF_MONOCHROME		(1<<1)	// monocrhome screen

// custom flags for func_screenmovie
#define CF_LOOPED_MOVIE		(1<<0)	// loop movie at end point
#define CF_MONOCHROME		(1<<1)	// use monocrhome image
#define CF_MOVIE_SOUND		(1<<2)	// allow sound

// custom flags for env_projector
// BIT(0)	// unused
#define CF_TEXTURE			BIT(1)	// custom texture specified
#define CF_SPRITE			BIT(2)	// custom sprite specified
#define CF_MOVIE			BIT(3)	// custom movie specified
#define CF_ASPECT4X3		BIT(4)	// use normal aspect instead of quad
#define CF_ASPECT3X4		BIT(5)	// use portrait aspect instead of quad
#define CF_STATIC_ENTITY	BIT(6)	// this entity is completely static (non-moving brush or env_static)
#define CF_CUBEMAP			BIT(7)	// auto set on the client
#define CF_NOWORLD_PROJECTION		BIT(8)	// don't apply projection to the world brushes
#define CF_NOLIGHT_IN_SOLID	BIT(9)	// check lights who currently stuck in solid and disable this (point lights only)
#define CF_NOSHADOWS		BIT(10)	// ignore shadows for this light
#define CF_FLIPTEXTURE		BIT(11)	// mirror projection texture
#define CF_NOGRASSLIGHTING	BIT(12) // do not light grass (diffusion - minor optimization for insignificant lights)
#define CF_ONLYBRUSHSHADOWS BIT(13) // ignore studio models during shadow pass (minor optimization for particular cases)
#define CF_ONLYFORCEDSHADOWS BIT(14) // only entities with kRenderFxForceShadow will cast shadows

// entity flags
#define EFLAG_SLERP			1	// do studio interpolation of this entity
#define EFLAG_INTERMISSION		2	// it's a intermission spot

// should be defined at all
#define MAXSTUDIOPOSEPARAM		24
		
//
// temp entity events
//
#define	TE_BEAMPOINTS		0	// beam effect between two points
// coord coord coord (start position) 
// coord coord coord (end position) 
// short (sprite index) 
// byte (starting frame) 
// byte (frame rate in 0.1's) 
// byte (life in 0.1's) 
// byte (line width in 0.1's) 
// byte (noise amplitude in 0.01's) 
// byte,byte,byte (color)
// byte (brightness)
// byte (scroll speed in 0.1's)

#define	TE_BEAMENTPOINT		1	// beam effect between point and entity
// short (start entity) 
// coord coord coord (end position) 
// short (sprite index) 
// byte (starting frame) 
// byte (frame rate in 0.1's) 
// byte (life in 0.1's) 
// byte (line width in 0.1's) 
// byte (noise amplitude in 0.01's) 
// byte,byte,byte (color)
// byte (brightness)
// byte (scroll speed in 0.1's)

#define	TE_GUNSHOT		2	// particle effect plus ricochet sound
// coord coord coord (position) 

#define	TE_EXPLOSION		3	// additive sprite, 2 dynamic lights, flickering particles, explosion sound, move vertically 8 pps
// coord coord coord (position) 
// short (sprite index)
// byte (scale in 0.1's)
// byte (framerate)
// byte (flags)
//
// The Explosion effect has some flags to control performance/aesthetic features:
#define TE_EXPLFLAG_NONE		0	// all flags clear makes default Half-Life explosion
#define TE_EXPLFLAG_NOADDITIVE	1	// sprite will be drawn opaque (ensure that the sprite you send is a non-additive sprite)
#define TE_EXPLFLAG_NODLIGHTS		2	// do not render dynamic lights
#define TE_EXPLFLAG_NOSOUND		4	// do not play client explosion sound
#define TE_EXPLFLAG_NOPARTICLES	8	// do not draw particles
#define TE_EXPLFLAG_DRAWALPHA		16	// sprite will be drawn alpha
#define TE_EXPLFLAG_ROTATE		32	// rotate the sprite randomly

#define	TE_TAREXPLOSION		4	// Quake1 "tarbaby" explosion with sound
// coord coord coord (position) 

#define	TE_SMOKE			5	// alphablend sprite, move vertically 30 pps
// coord coord coord (position) 
// short (sprite index)
// byte (scale in 0.1's)
// byte (framerate)
// byte (rendermode) 0 = default alphablend, 1 = additive

#define	TE_TRACER			6	// tracer effect from point to point
// coord, coord, coord (start) 
// coord, coord, coord (end)

#define	TE_LIGHTNING		7	// TE_BEAMPOINTS with simplified parameters
// coord, coord, coord (start) 
// coord, coord, coord (end) 
// byte (life in 0.1's) 
// byte (width in 0.1's) 
// byte (amplitude in 0.01's)
// short (sprite model index)

#define	TE_BEAMENTS		8		
// short (start entity) 
// short (end entity) 
// short (sprite index) 
// byte (starting frame) 
// byte (frame rate in 0.1's) 
// byte (life in 0.1's) 
// byte (line width in 0.1's) 
// byte (noise amplitude in 0.01's) 
// byte,byte,byte (color)
// byte (brightness)
// byte (scroll speed in 0.1's)

#define	TE_SPARKS			9	// 8 random tracers with gravity, ricochet sprite
// coord coord coord (position) 

#define	TE_LAVASPLASH		10	// Quake1 lava splash
// coord coord coord (position) 

#define	TE_TELEPORT		11	// Quake1 teleport splash
// coord coord coord (position) 

#define TE_EXPLOSION2		12	// Quake1 colormaped (base palette) particle explosion with sound
// coord coord coord (position) 
// byte (starting color)
// byte (num colors)

#define TE_BSPDECAL			13	// Decal from the .BSP file 
// coord, coord, coord (x,y,z), decal position (center of texture in world)
// short (texture index of precached decal texture name)
// short (entity index)
// [optional - only included if previous short is non-zero (not the world)] short (index of model of above entity)

#define TE_IMPLOSION		14	// tracers moving toward a point
// coord, coord, coord (position)
// byte (radius)
// byte (count)
// byte (life in 0.1's) 

#define TE_SPRITETRAIL		15	// line of moving glow sprites with gravity, fadeout, and collisions
// coord, coord, coord (start) 
// coord, coord, coord (end) 
// short (sprite index)
// byte (count)
// byte (life in 0.1's) 
// byte (scale in 0.1's) 
// byte (velocity along vector in 10's)
// byte (randomness of velocity in 10's)

#define TE_BEAM			16	// obsolete

#define TE_SPRITE			17	// additive sprite, plays 1 cycle
// coord, coord, coord (position) 
// short (sprite index) 
// byte (scale in 0.1's) 
// byte (brightness)

#define TE_BEAMSPRITE		18	// A beam with a sprite at the end
// coord, coord, coord (start position) 
// coord, coord, coord (end position) 
// short (beam sprite index) 
// short (end sprite index) 

#define TE_BEAMTORUS		19	// screen aligned beam ring, expands to max radius over lifetime
// coord coord coord (center position) 
// coord coord coord (axis and radius) 
// short (sprite index) 
// byte (starting frame) 
// byte (frame rate in 0.1's) 
// byte (life in 0.1's) 
// byte (line width in 0.1's) 
// byte (noise amplitude in 0.01's) 
// byte,byte,byte (color)
// byte (brightness)
// byte (scroll speed in 0.1's)

#define TE_BEAMDISK			20	// disk that expands to max radius over lifetime
// coord coord coord (center position) 
// coord coord coord (axis and radius) 
// short (sprite index) 
// byte (starting frame) 
// byte (frame rate in 0.1's) 
// byte (life in 0.1's) 
// byte (line width in 0.1's) 
// byte (noise amplitude in 0.01's) 
// byte,byte,byte (color)
// byte (brightness)
// byte (scroll speed in 0.1's)

#define TE_BEAMCYLINDER		21	// cylinder that expands to max radius over lifetime
// coord coord coord (center position) 
// coord coord coord (axis and radius) 
// short (sprite index) 
// byte (starting frame) 
// byte (frame rate in 0.1's) 
// byte (life in 0.1's) 
// byte (line width in 0.1's) 
// byte (noise amplitude in 0.01's) 
// byte,byte,byte (color)
// byte (brightness)
// byte (scroll speed in 0.1's)

#define TE_BEAMFOLLOW		22	// create a line of decaying beam segments until entity stops moving
// short (entity:attachment to follow)
// short (sprite index)
// byte (life in 0.1's) 
// byte (line width in 0.1's) 
// byte,byte,byte (color)
// byte (brightness)

#define TE_GLOWSPRITE		23		
// coord, coord, coord (pos) short (model index) byte (scale / 10)

#define TE_BEAMRING			24	// connect a beam ring to two entities
// short (start entity) 
// short (end entity) 
// short (sprite index) 
// byte (starting frame) 
// byte (frame rate in 0.1's) 
// byte (life in 0.1's) 
// byte (line width in 0.1's) 
// byte (noise amplitude in 0.01's) 
// byte,byte,byte (color)
// byte (brightness)
// byte (scroll speed in 0.1's)

#define TE_STREAK_SPLASH	25		// oriented shower of tracers
// coord coord coord (start position) 
// coord coord coord (direction vector) 
// byte (color)
// short (count)
// short (base speed)
// short (random velocity)

#define TE_BEAMHOSE			26	// obsolete

#define TE_DLIGHT			27	// dynamic light, effect world, minor entity effect
// coord, coord, coord (pos) 
// byte (radius in 10's) 
// byte byte byte (color)
// byte (life in 10's)
// byte (decay rate in 10's)

#define TE_ELIGHT			28	// point entity light, no world effect
// short (entity:attachment to follow)
// coord coord coord (initial position) 
// coord (radius)
// byte byte byte (color)
// byte (life in 0.1's)
// coord (decay rate)

#define TE_TEXTMESSAGE		29
// short 1.2.13 x (-1 = center)
// short 1.2.13 y (-1 = center)
// byte Effect 0 = fade in/fade out
// 1 is flickery credits
// 2 is write out (training room)
// 4 bytes r,g,b,a color1	(text color)
// 4 bytes r,g,b,a color2	(effect color)
// ushort 8.8 fadein time
// ushort 8.8  fadeout time
// ushort 8.8 hold time
// optional ushort 8.8 fxtime	(time the highlight lags behing the leading text in effect 2)
// string text message		(512 chars max sz string)
#define TE_LINE			30
// coord, coord, coord	startpos
// coord, coord, coord	endpos
// short life in 0.1 s
// 3 bytes r, g, b

#define TE_BOX			31
// coord, coord, coord	boxmins
// coord, coord, coord	boxmaxs
// short life in 0.1 s
// 3 bytes r, g, b

#define TE_KILLBEAM			99	// kill all beams attached to entity
// short (entity)

#define TE_LARGEFUNNEL		100
// coord coord coord (funnel position)
// short (sprite index) 
// short (flags) 

#define	TE_BLOODSTREAM		101	// particle spray
// coord coord coord (start position)
// coord coord coord (spray vector)
// byte (color)
// byte (speed)

#define	TE_SHOWLINE		102	// line of particles every 5 units, dies in 30 seconds
// coord coord coord (start position)
// coord coord coord (end position)

#define TE_BLOOD			103	// particle spray
// coord coord coord (start position)
// coord coord coord (spray vector)
// byte (color)
// byte (speed)

#define TE_DECAL			104	// Decal applied to a brush entity (not the world)
// coord, coord, coord (x,y,z), decal position (center of texture in world)
// byte (texture index of precached decal texture name)
// short (entity index)

#define TE_FIZZ			105	// create alpha sprites inside of entity, float upwards
// short (entity)
// short (sprite index)
// byte (density)

#define TE_MODEL			106	// create a moving model that bounces and makes a sound when it hits
// coord, coord, coord (position) 
// coord, coord, coord (velocity)
// angle (initial yaw)
// short (model index)
// byte (bounce sound type)
// byte (life in 0.1's)

#define TE_EXPLODEMODEL		107	// spherical shower of models, picks from set
// coord, coord, coord (origin)
// coord (velocity)
// short (model index)
// short (count)
// byte (life in 0.1's)

#define TE_BREAKMODEL		108	// box of models or sprites
// coord, coord, coord (position)
// coord, coord, coord (size)
// coord, coord, coord (velocity)
// byte (random velocity in 10's)
// short (sprite or model index)
// byte (count)
// byte (life in 0.1 secs)
// byte (flags)

#define TE_GUNSHOTDECAL		109	// decal and ricochet sound
// coord, coord, coord (position)
// short (entity index???)
// byte (decal???)

#define TE_SPRITE_SPRAY		110	// spay of alpha sprites
// coord, coord, coord (position)
// coord, coord, coord (velocity)
// short (sprite index)
// byte (count)
// byte (speed)
// byte (noise)

#define TE_ARMOR_RICOCHET		111	// quick spark sprite, client ricochet sound. 
// coord, coord, coord (position)
// byte (scale in 0.1's)

#define TE_PLAYERDECAL		112	// ???
// byte (playerindex)
// coord, coord, coord (position)
// short (entity???)
// byte (decal number???)
// [optional] short (model index???)

#define TE_BUBBLES			113	// create alpha sprites inside of box, float upwards
// coord, coord, coord (min start position)
// coord, coord, coord (max start position)
// coord (float height)
// short (model index)
// byte (count)
// coord (speed)

#define TE_BUBBLETRAIL		114	// create alpha sprites along a line, float upwards
// coord, coord, coord (min start position)
// coord, coord, coord (max start position)
// coord (float height)
// short (model index)
// byte (count)
// coord (speed)

#define TE_BLOODSPRITE		115	// spray of opaque sprite1's that fall, single sprite2 for 1..2 secs (this is a high-priority tent)
// coord, coord, coord (position)
// short (sprite1 index)
// short (sprite2 index)
// byte (color)
// byte (scale)

#define TE_WORLDDECAL		116	// Decal applied to the world brush
// coord, coord, coord (x,y,z), decal position (center of texture in world)
// byte (texture index of precached decal texture name)

#define TE_WORLDDECALHIGH		117	// Decal (with texture index > 256) applied to world brush
// coord, coord, coord (x,y,z), decal position (center of texture in world)
// byte (texture index of precached decal texture name - 256)

#define TE_DECALHIGH		118	// Same as TE_DECAL, but the texture index was greater than 256
// coord, coord, coord (x,y,z), decal position (center of texture in world)
// byte (texture index of precached decal texture name - 256)
// short (entity index)

#define TE_PROJECTILE		119	// Makes a projectile (like a nail) (this is a high-priority tent)
// coord, coord, coord (position)
// coord, coord, coord (velocity)
// short (modelindex)
// byte (life)
// byte (owner)  projectile won't collide with owner (if owner == 0, projectile will hit any client).

#define TE_SPRAY			120	// Throws a shower of sprites or models
// coord, coord, coord (position)
// coord, coord, coord (direction)
// short (modelindex)
// byte (count)
// byte (speed)
// byte (noise)
// byte (rendermode)

#define TE_PLAYERSPRITES		121	// sprites emit from a player's bounding box (ONLY use for players!)
// byte (playernum)
// short (sprite modelindex)
// byte (count)
// byte (variance) (0 = no variance in size) (10 = 10% variance in size)

#define TE_PARTICLEBURST		122	// very similar to lavasplash.
// coord (origin)
// short (radius)
// byte (particle color)
// byte (duration * 10) (will be randomized a bit)

#define TE_FIREFIELD		123	// makes a field of fire.
// coord (origin)
// short (radius) (fire is made in a square around origin. -radius, -radius to radius, radius)
// short (modelindex)
// byte (count)
// byte (flags)
// byte (duration (in seconds) * 10) (will be randomized a bit)
//
// to keep network traffic low, this message has associated flags that fit into a byte:
#define TEFIRE_FLAG_ALLFLOAT	1 // all sprites will drift upwards as they animate
#define TEFIRE_FLAG_SOMEFLOAT	2 // some of the sprites will drift upwards. (50% chance)
#define TEFIRE_FLAG_LOOP	4 // if set, sprite plays at 15 fps, otherwise plays at whatever rate stretches the animation over the sprite's duration.
#define TEFIRE_FLAG_ALPHA	8 // if set, sprite is rendered alpha blended at 50% else, opaque
#define TEFIRE_FLAG_PLANAR	16 // if set, all fire sprites have same initial Z instead of randomly filling a cube. 
#define TEFIRE_FLAG_ADDITIVE	32 // if set, sprite is rendered as additive

#define TE_PLAYERATTACHMENT		124	// attaches a TENT to a player (this is a high-priority tent)
// byte (entity index of player)
// coord (vertical offset) ( attachment origin.z = player origin.z + vertical offset )
// short (model index)
// short (life * 10 );

#define TE_KILLPLAYERATTACHMENTS	125	// will expire all TENTS attached to a player.
// byte (entity index of player)

#define TE_MULTIGUNSHOT		126	// much more compact shotgun message
// This message is used to make a client approximate a 'spray' of gunfire.
// Any weapon that fires more than one bullet per frame and fires in a bit of a spread is
// a good candidate for MULTIGUNSHOT use. (shotguns)
//
// NOTE: This effect makes the client do traces for each bullet, these client traces ignore
// entities that have studio models.Traces are 4096 long.
//
// coord (origin)
// coord (origin)
// coord (origin)
// coord (direction)
// coord (direction)
// coord (direction)
// coord (x noise * 100)
// coord (y noise * 100)
// byte (count)
// byte (bullethole decal texture index)

#define TE_USERTRACER		127	// larger message than the standard tracer, but allows some customization.
// coord (origin)
// coord (origin)
// coord (origin)
// coord (velocity)
// coord (velocity)
// coord (velocity)
// byte ( life * 10 )
// byte ( color ) this is an index into an array of color vectors in the engine. (0 - )
// byte ( length * 10 )

// diffusion - same as TE_SMOKE but additive
#define	TE_FIRE			128	// alphablend sprite, move vertically 30 pps
// coord coord coord (position) 
// short (sprite index)
// byte (scale in 0.1's)
// byte (framerate)
// byte (rendermode) 0 = default alphablend, 1 = additive

#define TE_PLAYERPARAMS 129 // diffusion - just a hack so I don't need to create another message
#define TE_CLIENTSOUND 130 // diffusion - client sound at location...used for distant weapon sounds
#define TE_SMOKEGRENADE 131 // diffusion - used by monster_security_assassin
#define TE_BOAT_TRAIL 132 // diffusion
#define TE_CARPARAMS 133 // diffusion
#define TE_SCREENSHAKE 134 // diffusion - moved screenshake from engine to here
#define TE_WEAPONSHAKE 135
#define TE_ACHIEVEMENT 136 // pass an achievement stat
#define TE_UNREALSOUND 137 // killing spree!
#define TE_CLIENTEVENT 138 // force client studio event from server...
#define TE_BEAMPARTICLES 139 // gausniper beam rings
#define TE_STEP_PARTICLE 140
#define TE_PLAYER_GLITCH 141
#define TE_DRONEPARAMS 142
#define TE_TRIGGERTIMER 143 // trigger_timer parameters
#define TE_SUNLIGHT_SCALE 144
#define TE_NPCCLIP 145 // npc's reloading clip

#define MSG_BROADCAST		0	// unreliable to all
#define MSG_ONE			1	// reliable to one (msg_entity)
#define MSG_ALL			2	// reliable to all
#define MSG_INIT			3	// write to the init string
#define MSG_PVS			4	// Ents in PVS of org
#define MSG_PAS			5	// Ents in PAS of org
#define MSG_PVS_R			6	// Reliable to PVS
#define MSG_PAS_R			7	// Reliable to PAS
#define MSG_ONE_UNRELIABLE		8	// Send to one client, but don't put in reliable stream, put in unreliable datagram ( could be dropped )
#define MSG_SPEC			9	// Sends to all spectator proxies

// contents of a spot in the world
#define CONTENTS_NONE		0
#define CONTENTS_EMPTY		-1
#define CONTENTS_SOLID		-2
#define CONTENTS_WATER		-3
#define CONTENTS_SLIME		-4
#define CONTENTS_LAVA		-5
#define CONTENTS_SKY		-6
// These additional contents constants are defined in bspfile.h
#define CONTENTS_ORIGIN		-7	// removed at csg time
#define CONTENTS_CLIP		-8	// changed to contents_solid
#define CONTENTS_CURRENT_0		-9
#define CONTENTS_CURRENT_90		-10
#define CONTENTS_CURRENT_180		-11
#define CONTENTS_CURRENT_270		-12
#define CONTENTS_CURRENT_UP		-13
#define CONTENTS_CURRENT_DOWN		-14
#define CONTENTS_TRANSLUCENT		-15

#define CONTENTS_LADDER		-16

#define CONTENT_FLYFIELD		-17
#define CONTENT_GRAVITY_FLYFIELD	-18
#define CONTENT_FOG			-19
#define CONTENT_SPECIAL1		-20
#define CONTENT_SPECIAL2		-21
#define CONTENT_SPECIAL3		-22
#define CONTENT_SPOTLIGHT		-23	// in of cone of spotlight
#define CONTENT_CARBLOCKER	-24
#define CONTENT_BOATBLOCKER	-25

#define CONTENT_EMPTY		-1
#define CONTENT_SOLID		-2
#define CONTENT_WATER		-3
#define CONTENT_SLIME		-4
#define CONTENT_LAVA		-5
#define CONTENT_SKY			-6

// channels
#define CHAN_AUTO			0
#define CHAN_WEAPON			1
#define CHAN_VOICE			2
#define CHAN_ITEM			3
#define CHAN_BODY			4
#define CHAN_STREAM			5	// allocate stream channel from the static or dynamic area
#define CHAN_STATIC			6	// allocate channel from the static area 
#define CHAN_NETWORKVOICE_BASE	7	// voice data coming across the network
#define CHAN_NETWORKVOICE_END		500	// network voice data reserves slots (CHAN_NETWORKVOICE_BASE through CHAN_NETWORKVOICE_END).


// attenuation values
#define ATTN_NONE			0
#define ATTN_NORM			(float)0.8
#define ATTN_STATIC			(float)1.25 
#define ATTN_IDLE			(float)2

// pitch values
#define PITCH_NORM			100	// non-pitch shifted
#define PITCH_LOW			95	// other values are possible - 0-255, where 255 is very high
#define PITCH_HIGH			120

// volume values
#define VOL_NORM			1.0

// buttons
#define IN_ATTACK			(1<<0)
#define IN_JUMP			(1<<1)
#define IN_DUCK		(1<<2)
#define IN_FORWARD			(1<<3)
#define IN_BACK			(1<<4)
#define IN_USE			(1<<5)
#define IN_CANCEL			(1<<6)
#define IN_LEFT			(1<<7)
#define IN_RIGHT			(1<<8)
#define IN_MOVELEFT			(1<<9)
#define IN_MOVERIGHT		(1<<10)
#define IN_ATTACK2			(1<<11)
#define IN_RUN			(1<<12)
#define IN_RELOAD			(1<<13)
#define IN_ALT1			(1<<14)
#define IN_SCORE			(1<<15)   // Used by client.dll for when scoreboard is held down

// Break Model Defines
#define BREAK_TYPEMASK		0x4F
#define BREAK_GLASS			BIT(0)
#define BREAK_METAL			BIT(1)
#define BREAK_FLESH			BIT(2)
#define BREAK_WOOD			BIT(3)
#define BREAK_SMOKE			BIT(4)
#define BREAK_TRANS			BIT(5)
#define BREAK_CONCRETE		BIT(6)
#define BREAK_2				BIT(7)

// Colliding temp entity sounds
#define BOUNCE_GLASS		BREAK_GLASS
#define BOUNCE_METAL		BREAK_METAL
#define BOUNCE_FLESH		BREAK_FLESH
#define BOUNCE_WOOD			BREAK_WOOD
#define BOUNCE_SHRAP		BIT(4)
#define BOUNCE_SHELL		BIT(5)
#define BOUNCE_CONCRETE		BREAK_CONCRETE
#define BOUNCE_SHOTSHELL	BIT(7)
#define BOUNCE_EMPTYCLIP	BIT(8)

// Temp entity bounce sound types
#define TE_BOUNCE_NULL		0
#define TE_BOUNCE_SHELL		1
#define TE_BOUNCE_SHOTSHELL		2

// Rendering constants
enum 
{	
	kRenderNormal,		// src
	kRenderTransColor,		// c*a+dest*(1-a)
	kRenderTransTexture,	// src*a+dest*(1-a)
	kRenderGlow,		// src*a+dest -- No Z buffer checks
	kRenderTransAlpha,		// src*srca+dest*(1-srca)
	kRenderTransAdd,		// src*a+dest

	// diffusion - this a copy of TransTexture. glass textures have less light brightness on them,
	// but there's a bug - when an entity fades, it turns to texture mode.
	// there were rare occasions when a solid entity will receive much less light when we get close to it (because it was faded and dynamic light caught this surface)
	kRenderFade,	// src*a+dest*(1-a)
	kRenderConstantGlow, // instead of using Glow + FxNoDissipation (compatibility still saved)
};

enum 
{	
	kRenderFxNone = 0, 
	kRenderFxPulseSlow, 
	kRenderFxPulseFast, 
	kRenderFxPulseSlowWide, 
	kRenderFxPulseFastWide, 
	kRenderFxFadeSlow, 
	kRenderFxFadeFast, 
	kRenderFxSolidSlow, 
	kRenderFxSolidFast, 	   
	kRenderFxStrobeSlow, 
	kRenderFxStrobeFast, 
	kRenderFxStrobeFaster, 
	kRenderFxFlickerSlow, 
	kRenderFxFlickerFast,
	kRenderFxNoDissipation,
	kRenderFxDistort,			// Distort/scale/translate flicker
	kRenderFxHologram,			// kRenderFxDistort + distance fade
	kRenderFxDeadPlayer,		// kRenderAmt is the player index
	kRenderFxExplode,			// Scale up really big!
	kRenderFxGlowShell,			// Glowing Shell
	kRenderFxClampMinScale,		// Keep this sprite from getting very small (SPRITES only!)
	kRenderFxAurora,			// set particle trail for this entity // this is also used in engine as kRenderFxLightMultiplier...FIXME
	kRenderFxNoShadows,			// diffusion - no dynamic shadow cast
	kRenderFxFrontDissolve,		// diffusion - when you get very close to sprite, it will fade (glow sprites only) (kinda like reverse fade distance - maybe need to apply to models in future) // TODO make it better
	kRenderFxFullbright,		// diffusion - self-explanatory
	kRenderFxFullbrightNoShadows, // diffusion - same, but no dynamic shadows
	kRenderFxForceShadow,		// diffusion - this is for studio models, and for dynamic lights that are set to cast only brush shadows
	kRenderFxTwoSide,			// diffusion - sprites only! force render sprite on its both sides (i.e. for oriented ones)
	kRenderFxNoRefraction,		// diffusion - don't apply refraction (water only - it's always with refraction anyway)
	kRenderFxParticle,			// diffusion - quakeparticle
	kRenderFxOnlyShadows,		// diffusion - draw shadows only
	kRenderFxParticleLine,		// diffusion - env_particle_line
};

typedef unsigned int	string_t;

typedef unsigned char	byte;
typedef unsigned short	word;
typedef unsigned int	uint;

#undef true
#undef false

#ifndef __cplusplus
typedef enum { false, true }	qboolean;
#else 
typedef int qboolean;
#endif

typedef struct
{
	byte	r, g, b;
} color24;

typedef struct
{
	unsigned	r, g, b, a;
} colorVec;

#ifdef _WIN32
#pragma pack( push, 2 )
#endif

typedef struct
{
	unsigned short r, g, b, a;
} PackedColorVec;

#ifdef _WIN32
#pragma pack( pop )
#endif

typedef struct link_s
{
	struct link_s	*prev, *next;
} link_t;

typedef struct edict_s edict_t;

typedef struct plane_s
{
	vec3_t	normal;
	float	dist;
} plane_t;

typedef struct trace_s
{
	qboolean	allsolid;		// if true, plane is not valid
	qboolean	startsolid;	// if true, the initial point was in a solid area
	qboolean	inopen, inwater;
	float	fraction;		// time completed, 1.0 = didn't hit anything
	vec3_t	endpos;		// final position
	plane_t	plane;		// surface normal at impact
	edict_t	*ent;		// entity the surface is on
	int	hitgroup;		// 0 == generic, non zero is specific body part
} trace_t;

const Vector2D sunflower[50] =
{
	Vector2D( 0.000, 0.000 ),
	Vector2D( -0.019, -0.068 ),
	Vector2D( -0.105, 0.064 ),
	Vector2D( 0.116, 0.108 ),
	Vector2D( 0.086, -0.167 ),
	Vector2D( -0.208, -0.043 ),
	Vector2D( 0.016, 0.235 ),
	Vector2D( 0.241, -0.086 ),
	Vector2D( -0.158, -0.224 ),
	Vector2D( -0.184, 0.227 ),
	Vector2D( 0.283, 0.123 ),
	Vector2D( 0.044, -0.322 ),
	Vector2D( -0.337, 0.046 ),
	Vector2D( 0.141, 0.325 ),
	Vector2D( 0.286, -0.232 ),
	Vector2D( -0.312, -0.220 ),
	Vector2D( -0.132, 0.372 ),
	Vector2D( 0.406, 0.028 ),
	Vector2D( -0.085, -0.411 ),
	Vector2D( -0.383, 0.198 ),
	Vector2D( 0.302, 0.323 ),
	Vector2D( 0.236, -0.388 ),
	Vector2D( -0.448, -0.125 ),
	Vector2D( 0.000, 0.476 ),
	Vector2D( 0.468, -0.131 ),
	Vector2D( -0.258, -0.424 ),
	Vector2D( -0.346, 0.370 ),
	Vector2D( 0.458, 0.237 ),
	Vector2D( 0.107, -0.515 ),
	Vector2D( -0.534, 0.037 ),
	Vector2D( 0.182, 0.513 ),
	Vector2D( 0.452, -0.319 ),
	Vector2D( -0.437, -0.355 ),
	Vector2D( -0.228, 0.524 ),
	Vector2D( 0.575, 0.079 ),
	Vector2D( -0.080, -0.583 ),
	Vector2D( -0.548, 0.238 ),
	Vector2D( 0.382, 0.470 ),
	Vector2D( 0.354, -0.502 ),
	Vector2D( -0.586, -0.208 ),
	Vector2D( -0.043, 0.629 ),
	Vector2D( 0.625, -0.130 ),
	Vector2D( -0.298, -0.573 ),
	Vector2D( -0.477, 0.447 ),
	Vector2D( 0.565, 0.343 ),
	Vector2D( 0.180, -0.644 ),
	Vector2D( -0.676, 0.001 ),
	Vector2D( 0.185, 0.658 ),
	Vector2D( 0.591, -0.359 ),
	Vector2D( -0.510, -0.477 ),
};

#define DRONE_MAX_HEALTH 200
#define DRONE_MAX_AMMO 250

#define MAX_MOTD_LENGTH	4096

#endif//CONST_H