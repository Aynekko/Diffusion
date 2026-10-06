/*
holdable_item_controller.cpp - PD controller for held objects
Copyright (C) 2026 SNMetamorph
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
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "holdable_item_controller.h"
#include <PxConfig.h>
#include <PxPhysicsAPI.h>

using namespace physx;

namespace
{
	// linear spring gain in acceleration-space (in/s² per inch of displacement)
	// acceleration = spring * error - damping * velocity → mass-independent
	constexpr float linearSpring = 2000.0f;

	// linear damping gain in acceleration-space (in/s² per in/s of velocity)
	constexpr float linearDamping = 80.0f;

	// angular spring constant (rad/s² per rad of orientation error)
	// PD torque toward target rotation extracted from delta quaternion
	constexpr float angularSpring = 500.0f;

	// angular damping torque factor (rad/s² per rad/s of angular velocity per axis)
	// tuned for critical damping: k_d = 2*sqrt(k_p) ≈ 44.7
	// damps all three axes to suppress tumbling from collisions
	constexpr float angularDamping = 45.0f;
}

/*
=================
SetTarget

registers or updates the target pose (origin and orientation) toward which an entity's held object should be driven
=================
*/
void HoldableItemController::SetTarget( CBaseEntity *pEntity, const Vector &origin, const Vector4D &quat )
{
	Entry entry;
	entry.handle = pEntity;
	entry.target.origin = origin;
	entry.target.quat = quat;
	m_entries[pEntity->entindex()] = entry;
}

/*
=================
ClearTarget

stops driving the given entity's held object
=================
*/
void HoldableItemController::ClearTarget( CBaseEntity *pEntity )
{
	m_entries.erase( pEntity->entindex() );
}

/*
=================
ClearAllTargets

stops driving all held objects
=================
*/
void HoldableItemController::ClearAllTargets()
{
	m_entries.clear();
}

/*
=================
ApplyForces

applies PD control forces and torques each step to move every tracked dynamic body toward its target pose, dropping stale or invalid entries
=================
*/
void HoldableItemController::ApplyForces()
{
	for( auto it = m_entries.begin(); it != m_entries.end(); )
	{
		Entry &entry = it->second;
		if( entry.handle.Get() == NULL )
		{
			it = m_entries.erase( it );
			continue;
		}

		CBaseEntity *pEntity = (CBaseEntity *)entry.handle;
		if( !pEntity || !pEntity->m_pUserData )
		{
			it = m_entries.erase( it );
			continue;
		}

		PxActor *pActor = static_cast<PxActor*>( pEntity->m_pUserData );
		PxRigidDynamic *pRigid = pActor->is<PxRigidDynamic>();
		if( !pRigid || pRigid->getRigidBodyFlags().isSet( PxRigidBodyFlag::eKINEMATIC ) )
		{
			++it;
			continue;
		}

		Target &target = entry.target;
		PxTransform pose = pRigid->getGlobalPose();
		PxVec3 pos = pose.p;
		PxVec3 vel = pRigid->getLinearVelocity();

		// linear PD
		PxVec3 targetPos( target.origin.x, target.origin.y, target.origin.z );
		PxVec3 accel = ( targetPos - pos ) * linearSpring - vel * linearDamping;
		pRigid->addForce( accel, PxForceMode::eACCELERATION );

		// angular PD (quaternion-based)
		PxQuat objQuat = pose.q;
		PxQuat targetQuat( target.quat.x, target.quat.y, target.quat.z, target.quat.w );
		PxQuat delta = targetQuat * objQuat.getConjugate();

		PxReal angle;
		PxVec3 axis;
		delta.toRadiansAndUnitAxis( angle, axis );

		// normalize to [-pi, pi] (shortest arc) to match QuaternionToAxisAngle behavior
		if (angle > PxPi)
		{
			angle -= 2.0f * PxPi;
		}

		PxVec3 angVel = pRigid->getAngularVelocity();
		float velProj = angVel.dot( axis ); 
		PxVec3 torque = axis * ( angle * angularSpring - velProj * angularDamping );
		torque -= ( angVel - axis * velProj ) * angularDamping;
		pRigid->addTorque( torque, PxForceMode::eACCELERATION );

		++it;
	}
}
