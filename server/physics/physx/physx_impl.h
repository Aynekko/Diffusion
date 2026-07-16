/*
physx_impl.h - part of PhysX physics engine implementation
Copyright (C) 2012 Uncle Mike
Copyright (C) 2022 SNMetamorph
Copyright (C) 2025 Bacontsu
Copyright (C) 2026 rickomax

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/
#pragma once
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "physic.h"
#include "studio.h"
#include <memory>
#include <vector>
#include <map>
#include <string>
#include "io_streams.h"
#include "error_stream.h"
#include "assert_handler.h"

#include <PxConfig.h>
#include <PxPhysicsAPI.h>
#include <PxSimulationEventCallback.h>
#include <PxScene.h>
#include <PxActor.h>
#include <cooking/PxTriangleMeshDesc.h>
#include <cooking/PxConvexMeshDesc.h>
#include <PxMaterial.h>
#include <cooking/PxCooking.h>
#include <geometry/PxTriangle.h>
	
class EventHandler;
class ContactModifyCallback;
class HoldableItemController;

#define RAGDOLL_PARTS	10
#define RAGDOLL_JOINTS	9
#define RAGDOLL_ADJUSTMENTS	64
#define RAGDOLL_IMPACT_SOUNDS	8

struct RagdollConfig
{
	bool valid;
	char boneNames[RAGDOLL_PARTS][32];

	// per-part config
	float limits[RAGDOLL_PARTS][6];
	int numLimits[RAGDOLL_PARTS];

	// swingY/swingZ axes in world space at the reference pose;
	float swingAxisY[RAGDOLL_PARTS][3];
	float swingAxisZ[RAGDOLL_PARTS][3];
	bool hasAxes[RAGDOLL_PARTS];

	// relative part mass; hasRatio false falls back to k_RagdollMassRatio
	float massRatio[RAGDOLL_PARTS];
	bool hasRatio[RAGDOLL_PARTS];

	// 'adjustment "bone" x y z' lines
	char adjustBoneNames[RAGDOLL_ADJUSTMENTS][32];
	float adjustEuler[RAGDOLL_ADJUSTMENTS][3];
	int numAdjustments;

	// 'impact_soft/impact_hard "sound.wav"' lines, 'impact_force N' flips the tier
	char impactSoft[RAGDOLL_IMPACT_SOUNDS][64];
	int numImpactSoft;
	char impactHard[RAGDOLL_IMPACT_SOUNDS][64];
	int numImpactHard;
	float impactForce;
};

#define MAX_RAGDOLLS		256	// live ragdoll cap; spawning past it evicts the oldest corpse
#define MAX_RAGDOLL_CONFIGS	256	// distinct corpse models cached per level

// per-model parsed config cache entry)
struct RagdollConfigEntry
{
	char modelName[128];
	RagdollConfig config;
};

// a queued ragdoll spawn
struct PendingRagdoll
{
	int	entindex;
	int	numBones;	// 0 = nothing captured, sample the bones at spawn time
	matrix4x4	boneWorld[MAXSTUDIOBONES];
};

// one live ragdoll
struct RagdollDesc
{
	int entindex;			// owning corpse entity
	int serialnumber;		// edict serial, to detect a recycled slot
	int numBones;			// bones in the model skeleton
	bool asleep;			// all parts sleeping (network-silent)
	float lastSendTime;		// last bone snapshot time
	float spawnTime;		// when the ragdoll was created
	float fadeStartTime;		// non-zero once the lifetime expired and the corpse is fading out
	float waterFrac[RAGDOLL_PARTS];	// submerged fraction per part (0..1), sampled at the update tick
	float prevWaterFrac[RAGDOLL_PARTS];	// previous tick's fraction, to catch a part crossing the surface
	int bones[RAGDOLL_PARTS];	// model bone index of each part
	physx::PxRigidDynamic *bodies[RAGDOLL_PARTS];
	physx::PxJoint *joints[RAGDOLL_JOINTS];
	int bonePart[MAXSTUDIOBONES];	// which part drives each bone
	int studioParent[MAXSTUDIOBONES];	// parent bone index, cached from the model
	matrix4x4 boneOffset[MAXSTUDIOBONES];	// each bone's transform relative to its part body

	// per-bone override for config 'adjustment
	bool boneAdjusted[MAXSTUDIOBONES];
	matrix4x4 boneAdjustLocal[MAXSTUDIOBONES];

	// impact events suppressed until this time (set on save restore)
	float impactGraceUntil;

	// joint limits spawn widened to contain the death pose
	float limitBlendDuration;
	bool limitBlendDone;
	float limitSpring;		// spring strength at the limits (0 = hard stop)
	float limitDamping;		// damping at the limits when the spring is on
	float jointLimitStart[RAGDOLL_JOINTS][6];	// widened limits at spawn (radians)
	float jointLimitEnd[RAGDOLL_JOINTS][6];		// authored limits to blend toward
};

class CPhysicPhysX : public IPhysicLayer
{
public:
	CPhysicPhysX();
	void	InitPhysic( void );
	void	FreePhysic( void );
	void	Update( float flTimeDelta );
	void	EndFrame( void );
	void	RemoveBody( edict_t *pEdict );
	void	*CreateBodyFromEntity( CBaseEntity *pEntity );
	void	*CreateBoxFromEntity( CBaseEntity *pObject );
	void	*CreateRagdollEntity( CBaseEntity *pObject );
	void	PrecacheRagdoll( const char *szModelName );
	void	ReloadRagdollConfigs( void );
	const char *GetRagdollImpactSound( const char *szModelName, float flForce, float *flVolume );
	void	SendRagdollPose( CBaseEntity *pPlayer, int entindex );
	void	*CreateKinematicBodyFromEntity( CBaseEntity *pEntity );
	void	*CreateStaticBodyFromEntity( CBaseEntity *pObject );
	void	*CreateTriggerFromEntity( CBaseEntity *pEntity );
	void	*RestoreBody( CBaseEntity *pEntity );
	void	SaveBody( CBaseEntity *pObject );
	void	SaveRagdoll( CBaseEntity *pObject );
	bool	Initialized( void ) { return (m_pPhysics != NULL); }
	void	SetOrigin( CBaseEntity *pEntity, const Vector &origin );
	void	SetAngles( CBaseEntity *pEntity, const Vector &angles );
	void	SetVelocity( CBaseEntity *pEntity, const Vector &velocity );
	void	SetAvelocity( CBaseEntity *pEntity, const Vector &velocity );
	void	MoveObject( CBaseEntity *pEntity, const Vector &finalPos );
	void	RotateObject( CBaseEntity *pEntity, const Vector &finalAngle );
	void	SetLinearMomentum( CBaseEntity *pEntity, const Vector &momentum );
	void	AddImpulse( CBaseEntity *pEntity, const Vector &impulse, const Vector &position, float factor );
	void	AddForce( CBaseEntity *pEntity, const Vector &force, ForceMode mode = ForceMode::Force );
	void	AddTorque( CBaseEntity *pEntity, const Vector &torque, ForceMode mode = ForceMode::Force );
	void	SetHoldableTarget( CBaseEntity *pEntity, const Vector &targetOrigin, const Vector4D &targetQuat ) override;
	void	ClearHoldableTarget( CBaseEntity *pEntity ) override;
	void	GetTransform( CBaseEntity *pEntity, matrix4x4 &out );
	void	EnableCollision( CBaseEntity *pEntity, int fEnable );
	void	MakeKinematic( CBaseEntity *pEntity, int fEnable );
	int		FLoadTree( char *szMapName );
	int		CheckBINFile( char *szMapName );
	int		BuildCollisionTree( char *szMapName );
	bool	UpdateEntityTransform( CBaseEntity *pEntity );
	void	UpdateEntityAABB( CBaseEntity *pEntity );
	bool	UpdateActorPos( CBaseEntity *pEntity );
	void	SetupWorld( void );	
	void	FreeWorld( void );
	void	DrawPSpeeds( void );

	void	TeleportCharacter( CBaseEntity *pEntity );
	void	TeleportActor( CBaseEntity *pEntity );
	void	MoveCharacter( CBaseEntity *pEntity );
	void	MoveKinematic( CBaseEntity *pEntity );
	void	SweepTest( CBaseEntity *pTouch, const Vector &start, const Vector &mins, const Vector &maxs, const Vector &end, struct trace_s *tr );
	void	SweepEntity( CBaseEntity *pEntity, const Vector &start, const Vector &end, struct gametrace_s *tr );
	bool	IsBodySleeping( CBaseEntity *pEntity );
	void	*GetCookingInterface( void ) { return m_pCooking; }
	void	*GetPhysicInterface( void ) { return m_pPhysics; }

private:
	// misc routines
	bool DebugEnabled() const;
	void *SpawnRagdoll( CBaseEntity *pObject, const PendingRagdoll *pPending = NULL );
	void UpdateRagdolls( void );
	const RagdollConfig *GetRagdollConfig( const char *szModelName );
	bool RagdollComputeReferencePose( studiohdr_t *phdr, matrix4x4 refWorld[] );
	int FindRagdoll( int entindex ) const;
	void ReleaseRagdoll( size_t index, bool releaseBodies );
	void RagdollSendBones( RagdollDesc &rag, CBaseEntity *pEntity, int msgDest, edict_t *pTarget, Vector &mins, Vector &maxs );
	void RagdollApplyWaterForces( void );
	bool TracingStateChanges(physx::PxActor *actor) const;
	void HandleEvents();
	int	ConvertEdgeToIndex( model_t *model, int edge );
	physx::PxConvexMesh	*ConvexMeshFromBmodel( entvars_t *pev, int modelindex );
	physx::PxConvexMesh	*ConvexMeshFromStudio( entvars_t *pev, int modelindex, int32_t body, int32_t skin );
	physx::PxConvexMesh	*ConvexMeshFromEntity( CBaseEntity *pObject );
	physx::PxTriangleMesh *TriangleMeshFromBmodel( entvars_t *pev, int modelindex );
	physx::PxTriangleMesh *TriangleMeshFromStudio( entvars_t *pev, int modelindex, int32_t body, int32_t skin );
	physx::PxTriangleMesh *TriangleMeshFromEntity( CBaseEntity *pObject );
	physx::PxActor *ActorFromEntity( CBaseEntity *pObject );
	physx::PxBounds3 GetIntersectionBounds( const physx::PxBounds3 &a, const physx::PxBounds3 &b );
	CBaseEntity	*EntityFromActor( physx::PxActor *pObject );
	bool CheckCollision(physx::PxRigidActor *pActor);
	void ToggleCollision(physx::PxRigidActor *pActor, bool enabled);
	void UpdateCharacterBounds( CBaseEntity *pEntity, physx::PxShape *pShape );
	int	CheckFileTimes( const char *szFile1, const char *szFile2 );
	void StudioCalcBoneQuaterion( mstudiobone_t *pbone, mstudioanim_t *panim, Vector4D &q );
	void StudioCalcBonePosition( mstudiobone_t *pbone, mstudioanim_t *panim, Vector &pos );
	bool P_SpeedsMessage( char *out, size_t size );
	void CacheNameForModel( model_t *model, char *hullfile, size_t size, uint32_t hash, const char *suffix );
	uint32_t GetHashForModelState( model_t *model, int32_t body, int32_t skin );

private:
	physx::PxPhysics *m_pPhysics;	// pointer to the PhysX engine
	physx::PxFoundation	*m_pFoundation;
	physx::PxDefaultCpuDispatcher *m_pDispatcher;
	physx::PxScene *m_pScene;	// pointer to world scene
	model_t	*m_pWorldModel;	// pointer to worldmodel

	char m_szMapName[256];
	// flat bounded storage, counts never pass the caps
	RagdollDesc m_ragdolls[MAX_RAGDOLLS];
	int m_numRagdolls;
	PendingRagdoll m_pendingRagdolls[MAX_RAGDOLLS];
	int m_numPendingRagdolls;
	RagdollConfigEntry m_ragdollConfigs[MAX_RAGDOLL_CONFIGS];
	int m_numRagdollConfigs;
	float m_flRagdollUpdateTime;
	bool m_fLoaded;	// collision tree is loaded and actual
	bool m_fDisableWarning;	// some warnings will be swallowed
	bool m_fWorldChanged;	// world is changed refresh the statics in case their scale was changed too
	bool m_traceStateChanges;
	double m_flAccumulator;

	physx::PxTriangleMesh *m_pSceneMesh;
	physx::PxActor *m_pSceneActor;	// scene with installed shape
	physx::PxBounds3 m_worldBounds;
	physx::PxMaterial *m_pDefaultMaterial;
	physx::PxMaterial *m_pConveyorMaterial;

	char p_speeds_msg[1024];	// debug message

	AssertHandler m_assertHandler;
	ErrorCallback m_errorCallback;
	std::unique_ptr<EventHandler> m_eventHandler;
	std::unique_ptr<ContactModifyCallback> m_contactModifyCallback;
	std::unique_ptr<HoldableItemController> m_holdableController;
	physx::PxCooking *m_pCooking;
	physx::PxDefaultAllocator m_Allocator;
	physx::PxPvd *m_pVisualDebugger;
};
