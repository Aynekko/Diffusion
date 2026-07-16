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
#include "event_handler.h"
#include "collision_filter_data.h"
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include <algorithm>

using namespace physx;

/*
=================
onWorldInit

allocates the event handler's internal implementation state
=================
*/
void EventHandler::onWorldInit()
{
	m_impl = std::make_unique<Impl>();
}

/*
=================
onWorldShutdown

releases the event handler's internal implementation state
=================
*/
void EventHandler::onWorldShutdown()
{
	m_impl.reset();
}

/*
=================
purgeActor

drops queued events referencing an actor that is about to be released
=================
*/
void EventHandler::purgeActor(PxActor *actor)
{
	if (!m_impl)
	{
		return;
	}

	auto &touch = m_impl->touchEventsQueue;
	touch.erase(std::remove_if(touch.begin(), touch.end(), [actor](const auto &e) {
		return e.first == actor || e.second == actor;
	}), touch.end());

	auto &impacts = m_impl->impactEvents;
	impacts.erase(std::remove_if(impacts.begin(), impacts.end(), [actor](const ImpactEvent &e) {
		return e.ragdollActor == actor;
	}), impacts.end());

	// null it where it's the other side of a kept impact
	for (ImpactEvent &e : impacts)
	{
		if (e.otherActor == actor)
		{
			e.otherActor = nullptr;
		}
	}

	auto &water = m_impl->waterContactPairs;
	water.erase(std::remove_if(water.begin(), water.end(), [actor](const WaterContactPair &p) {
		return p.waterTriggerActor == actor || p.objectActor == actor;
	}), water.end());
}

/*
=================
getTouchEventsQueue

returns the queued touch events
=================
*/
EventHandler::TouchEventsQueue& EventHandler::getTouchEventsQueue()
{
	return m_impl->touchEventsQueue;
}

/*
=================
getWaterContactPairs

returns the recorded water-trigger contact pairs
=================
*/
std::vector<EventHandler::WaterContactPair>& EventHandler::getWaterContactPairs()
{
	return m_impl->waterContactPairs;
}

/*
=================
getImpactEvents

returns the queued ragdoll impact events
=================
*/
std::vector<EventHandler::ImpactEvent>& EventHandler::getImpactEvents()
{
	return m_impl->impactEvents;
}

/*
=================
queueRagdollImpact

queues an ImpactEvent when a ragdoll part hits something, keeping the strongest contact point
=================
*/
void EventHandler::queueRagdollImpact(const PxContactPairHeader &pairHeader, const PxContactPair &pair)
{
	PxActor *a0 = pairHeader.actors[0];
	PxActor *a1 = pairHeader.actors[1];
	if (!a0 || !a1)
	{
		return;
	}

	// ragdoll parts tag word0 of their shape filter data, the same bit the filter shader keys on
	const PxShape *s0 = pair.shapes[0];
	const PxShape *s1 = pair.shapes[1];
	const bool r0 = s0 && (s0->getSimulationFilterData().word0 & k_FilterRagdollPart);
	const bool r1 = s1 && (s1->getSimulationFilterData().word0 & k_FilterRagdollPart);
	if (!r0 && !r1)
	{
		return; // no ragdoll part involved
	}

	// skip self-collisions between parts of the same ragdoll
	if (r0 && r1 && a0->userData == a1->userData)
	{
		return;
	}

	PxContactPairPoint points[16];
	PxU32 numPoints = pair.extractContacts(points, 16);
	if (numPoints == 0)
	{
		return;
	}

	PxVec3 totalImpulse(0.0f);
	PxVec3 bestPos(0.0f), bestNormal(0.0f);
	float bestMag = -1.0f;
	for (PxU32 p = 0; p < numPoints; p++)
	{
		totalImpulse += points[p].impulse;
		float mag = points[p].impulse.magnitude();
		if (mag > bestMag) {
			bestMag = mag;
			bestPos = points[p].position;
			bestNormal = points[p].normal;
		}
	}

	float force = totalImpulse.magnitude();
	float threshold = CVAR_GET_FLOAT("phys_ragdoll_impactforce");
	if (threshold > 0.0f && force < threshold)
	{
		return; // too soft, ignore (settling / gentle contacts)
	}

	// notify both owners on ragdoll vs ragdoll, part index lives in filter word1
	if (r0)
	{
		int part = (int)s0->getSimulationFilterData().word1;
		m_impl->impactEvents.push_back({ a0, a1, bestPos, bestNormal, force, part });
	}
	if (r1)
	{
		int part = (int)s1->getSimulationFilterData().word1;
		m_impl->impactEvents.push_back({ a1, a0, bestPos, bestNormal, force, part });
	}
}


/*
=================
onTrigger

records water trigger enter/leave pairs, processed after the simulation step
=================
*/
void EventHandler::onTrigger(PxTriggerPair* pairs, PxU32 count)
{
	for (PxU32 i = 0; i < count; i++)
	{
		const PxTriggerPair &pair = pairs[i];
		if (pair.flags & (PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER | PxTriggerPairFlag::eREMOVED_SHAPE_OTHER)) {
			continue; // ignore pairs when shapes have been deleted
		}

		edict_t *triggerEntity = reinterpret_cast<edict_t*>(pair.triggerActor->userData);
		if (triggerEntity && FStrEq(STRING(triggerEntity->v.classname), "func_water")) 
		{
			WaterContactPair contactPair = { pair.triggerActor, pair.otherActor };
			if (pair.status == PxPairFlag::eNOTIFY_TOUCH_FOUND) 
			{
				auto searchIter = std::find(m_impl->waterContactPairs.begin(), m_impl->waterContactPairs.end(), contactPair);
				if (searchIter == std::end(m_impl->waterContactPairs)) {
					m_impl->waterContactPairs.push_back(contactPair);
				}
			}
			else if (pair.status == PxPairFlag::eNOTIFY_TOUCH_LOST) 
			{
 				auto it = std::remove_if(m_impl->waterContactPairs.begin(), m_impl->waterContactPairs.end(), [&contactPair](const auto &p) {
					return p == contactPair;
				});
				m_impl->waterContactPairs.erase(it, m_impl->waterContactPairs.end());
			}
		}
	}
}

/*
=================
onContact

processes contact pairs: fires ragdoll impacts on first contact and queues touch events for solid entities in persisting contact
=================
*/
void EventHandler::onContact(const PxContactPairHeader &pairHeader, const PxContactPair *pairs, PxU32 nbPairs)
{
	for (PxU32 i = 0; i < nbPairs; i++)
	{
		const PxContactPair &pair = pairs[i];

		// ragdoll-vs-world impact fires once, on first contact (not while resting)
		if (FBitSet(pair.events, PxPairFlag::eNOTIFY_TOUCH_FOUND)) {
			queueRagdollImpact(pairHeader, pair);
		}

		if (!FBitSet(pair.events, PxPairFlag::eNOTIFY_TOUCH_PERSISTS)) {
			continue; // was "return", which silently dropped the remaining pairs of the batch
		}

		PxActor *a1 = pairHeader.actors[0];
		PxActor *a2 = pairHeader.actors[1];
		edict_t *e1 = (edict_t*)a1->userData;
		edict_t *e2 = (edict_t*)a2->userData;

		if (!e1 || !e2)
		{
			return;
		}

		if (e1 && e1->v.solid != SOLID_NOT)
		{
			// FIXME: build trace info
			m_impl->touchEventsQueue.push_back({ a1, a2 });
		}

		if (e2 && e2->v.solid != SOLID_NOT)
		{
			// FIXME: build trace info
			m_impl->touchEventsQueue.push_back({ a2, a1 });
		}
	}
}
