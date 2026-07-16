/*
event_handler.h - class for handling PhysX simulation events
Copyright (C) 2023 SNMetamorph
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
#include <PxConfig.h>
#include <PxPhysicsAPI.h>
#include <PxSimulationEventCallback.h>
#include <queue>
#include <vector>
#include <memory>
#include <utility>

class EventHandler : public physx::PxSimulationEventCallback
{
public:
	using TouchEventsQueue = std::vector<std::pair<physx::PxActor*, physx::PxActor*>>;
	struct WaterContactPair
	{
		physx::PxActor *waterTriggerActor;
		physx::PxActor *objectActor;

		bool operator==(const WaterContactPair &operand) const {
			return waterTriggerActor == operand.waterTriggerActor && objectActor == operand.objectActor;
		}
	};

	// a ragdoll part hit something, forwarded to the owner's PhysicsImpact()
	struct ImpactEvent
	{
		physx::PxActor	*ragdollActor;	// owner receives the event
		physx::PxActor	*otherActor;	// what it hit (world/prop/ragdoll)
		physx::PxVec3	position;
		physx::PxVec3	normal;
		float		force;		// contact impulse magnitude
		int		part;		// ragdoll part index (filter word1), -1 if unknown
	};

	void onWorldInit();
	void onWorldShutdown();

	// call before releasing any actor, drops queued events that reference it
	void purgeActor(physx::PxActor *actor);

	TouchEventsQueue &getTouchEventsQueue();
	std::vector<WaterContactPair> &getWaterContactPairs();
	std::vector<ImpactEvent> &getImpactEvents();

	virtual void onConstraintBreak(physx::PxConstraintInfo* constraints, physx::PxU32 count) {};
	virtual void onWake(physx::PxActor** actors, physx::PxU32 count) {};
	virtual void onSleep(physx::PxActor** actors, physx::PxU32 count) {};
	virtual void onTrigger(physx::PxTriggerPair* pairs, physx::PxU32 count);
	virtual void onAdvance(const physx::PxRigidBody* const* bodyBuffer, const physx::PxTransform* poseBuffer, const physx::PxU32 count) {};
	virtual void onContact(const physx::PxContactPairHeader& pairHeader, const physx::PxContactPair* pairs, physx::PxU32 nbPairs);

private:
	void queueRagdollImpact(const physx::PxContactPairHeader &pairHeader, const physx::PxContactPair &pair);

	struct Impl
	{
		// pre-sized for busy frames, can still grow
		Impl()
		{
			touchEventsQueue.reserve( 256 );
			waterContactPairs.reserve( 64 );
			impactEvents.reserve( 256 );
		}

		TouchEventsQueue touchEventsQueue;
		std::vector<WaterContactPair> waterContactPairs;
		std::vector<ImpactEvent> impactEvents;
	};
	std::unique_ptr<Impl> m_impl;
};
