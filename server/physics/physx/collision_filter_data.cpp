/*
collision_filter_data.cpp - part of PhysX physics engine implementation
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

#include "collision_filter_data.h"

using namespace physx;

enum ColllisionFlags : PxU32
{
	ConveyorActor = (1 << 0),
};

/*
=================
CollisionFilterData

default constructor; clears the conveyor flag
=================
*/
CollisionFilterData::CollisionFilterData() :
	m_conveyorFlag(false)
{
}

/*
=================
CollisionFilterData

constructs from a native PxFilterData, decoding the conveyor flag from word0
=================
*/
CollisionFilterData::CollisionFilterData(const physx::PxFilterData &data)
{
	m_conveyorFlag = data.word0 & ColllisionFlags::ConveyorActor;
}

/*
=================
HasConveyorFlag

returns whether the conveyor flag is set
=================
*/
bool CollisionFilterData::HasConveyorFlag() const
{
	return m_conveyorFlag;
}

/*
=================
SetConveyorFlag

sets the conveyor flag
=================
*/
void CollisionFilterData::SetConveyorFlag(bool state)
{
	m_conveyorFlag = state;
}

/*
=================
ToNativeType

encodes the current flags into a native PxFilterData
=================
*/
PxFilterData CollisionFilterData::ToNativeType() const
{
	PxFilterData filterData;
	filterData.word0 |= m_conveyorFlag ? ColllisionFlags::ConveyorActor : 0;
	return filterData;
}
