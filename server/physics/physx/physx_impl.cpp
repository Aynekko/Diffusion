/*
physx_impl.cpp - part of PhysX physics engine implementation
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

#include "extdll.h"
#include "physic.h"

#ifdef USE_PHYSICS_ENGINE
#include <PxConfig.h>
#include "physx_impl.h"
#include "game/saverestore.h"
#include "game/client.h"
#include "bspfile.h"
#include "triangleapi.h"
#include "studio.h"
#include "pm_defs.h"
#include "pm_movevars.h"
#include "animation.h"
#include "game/game.h"
#include "build.h"
#include "mathlib.h"
#include "physcallback.h"
#include "error_stream.h"
#include "event_handler.h"
#include "holdable_item_controller.h"
#include "assert_handler.h"
#include "contact_modify_callback.h"
#include "collision_filter_data.h"
#include "decomposed_shape.h"
#include "meshdesc.h"
#include "tracemesh.h"
#include "game/user_messages.h"
#include <foundation/PxMat33.h>
#include <foundation/PxMat44.h>
#include <vector>
#include <algorithm>
#include <thread>

using namespace physx;

// hull-0 world/submodel collision reconstruction (defined later in this file)
namespace { void PW_BuildWorld( mnode_t *root, std::vector<PxVec3> &verts, std::vector<PxU32> &tris ); }

// k_FilterRagdollPart / k_FilterCharacter word0 bits live in collision_filter_data.h;
// ragdolls are debris, the filter shader keeps them out of character hulls

// fixed physics step, 100Hz
static const double k_SimulationStepSize = 1.0 / 100.0;

CPhysicPhysX	g_physicPhysX;
IPhysicLayer	*WorldPhysic = &g_physicPhysX;

/*
=================
CPhysicPhysX

zeroes all engine handles and sets the default world bounds
=================
*/
CPhysicPhysX::CPhysicPhysX() : 
	m_pPhysics(nullptr),
	m_pFoundation(nullptr),
	m_pDispatcher(nullptr),
	m_pScene(nullptr),
	m_pWorldModel(nullptr),
	m_pSceneMesh(nullptr),
	m_pSceneActor(nullptr),
	m_pDefaultMaterial(nullptr),
	m_pConveyorMaterial(nullptr),
	m_pCooking(nullptr),
	m_pVisualDebugger(nullptr),
	m_fLoaded(false),
	m_fDisableWarning(false),
	m_fWorldChanged(false),
	m_traceStateChanges(false),
	m_flAccumulator(0.0)
{
	m_szMapName[0] = '\0';
	p_speeds_msg[0] = '\0';
	m_flRagdollUpdateTime = 0.0f;
	m_numRagdolls = 0;
	m_numPendingRagdolls = 0;
	m_numRagdollConfigs = 0;
	m_worldBounds.minimum = PxVec3(-32768, -32768, -32768);
	m_worldBounds.maximum = PxVec3(32768, 32768, 32768);
}

/*
=================
InitPhysic

creates the PhysX foundation, physics, cooking, scene and materials; falls back to null physics when disabled by the user or on failure
=================
*/
void CPhysicPhysX :: InitPhysic( void )
{
	if( m_pPhysics )
	{
		ALERT( at_error, "InitPhysic: physics already initalized\n" );
		return;
	}

	if( g_allow_physx != NULL && g_allow_physx->value == 0.0f )
	{
		ALERT( at_console, "InitPhysic: PhysX support is disabled by user.\n" );
		GameInitNullPhysics ();
		return;
	}

	m_pFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, m_Allocator, m_errorCallback);
	if (!m_pFoundation)
	{
		ALERT(at_error, "InitPhysic: failed to create foundation\n");
		GameInitNullPhysics();
		return;
	}

	PxTolerancesScale scale;
	scale.length = 1.0f / METERS_PER_INCH;   // typical length of an object
	scale.speed = 800.0;   // typical speed of an object, gravity*1s is a reasonable choice
	
	// log -physxdebug once so a missing PVD view can be explained
	bool debugEnabled = DebugEnabled();
	ALERT( at_console, "PhysX: -physxdebug %s\n", debugEnabled ?
		"detected - PVD connection enabled" :
		"not passed - PVD will not connect this session" );

	if (debugEnabled)
	{
		m_pVisualDebugger = PxCreatePvd(*m_pFoundation);
		PxPvdTransport *transport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 300);
		// connect() fails silently when PVD isn't listening yet, report it
		bool pvdConnected = m_pVisualDebugger->connect(*transport, PxPvdInstrumentationFlag::eALL);
		ALERT( at_console, "PhysX: PVD connect to 127.0.0.1:5425 %s\n",
			pvdConnected ? "OK" : "FAILED - start PVD and connect it BEFORE loading the map, not after" );
	}

	m_pPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_pFoundation, scale, false, m_pVisualDebugger);
	m_pDispatcher = PxDefaultCpuDispatcherCreate(std::thread::hardware_concurrency());
	if (!m_pPhysics)
	{
		ALERT( at_error, "InitPhysic: failed to initalize physics engine\n" );
		GameInitNullPhysics();
		return;
	}

	m_pCooking = PxCreateCooking(PX_PHYSICS_VERSION, *m_pFoundation, PxCookingParams(scale));
	if (!m_pCooking)
	{
		ALERT( at_warning, "InitPhysic: failed to initalize cooking library\n" );
	}

	PxSetAssertHandler(m_assertHandler);
	if (!PxInitExtensions(*m_pPhysics, m_pVisualDebugger)) {
		ALERT( at_warning, "InitPhysic: failed to initalize extensions\n" );
	}

	// create objects needed for scene creation
	m_eventHandler = std::make_unique<EventHandler>();
	m_contactModifyCallback = std::make_unique<ContactModifyCallback>();
	m_holdableController = std::make_unique<HoldableItemController>();

	// create a scene
	PxSceneDesc sceneDesc(scale);
	sceneDesc.simulationEventCallback = m_eventHandler.get();
	sceneDesc.contactModifyCallback = m_contactModifyCallback.get();
	sceneDesc.gravity = PxVec3(0.0f, 0.0f, -800.0f);
	// stabilization keeps interpenetrating stacked bodies from jittering forever
	sceneDesc.flags = PxSceneFlag::eENABLE_CCD | PxSceneFlag::eENABLE_STABILIZATION;
	sceneDesc.cpuDispatcher = m_pDispatcher;
	sceneDesc.sanityBounds = m_worldBounds;
	sceneDesc.filterShader = [](
		PxFilterObjectAttributes attributes0, PxFilterData filterData0,
		PxFilterObjectAttributes attributes1, PxFilterData filterData1,
		PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize) -> PxFilterFlags {
			// ragdolls are debris: never collide with player/NPC character hulls
			if (((filterData0.word0 & k_FilterRagdollPart) && (filterData1.word0 & k_FilterCharacter)) ||
				((filterData1.word0 & k_FilterRagdollPart) && (filterData0.word0 & k_FilterCharacter)))
			{
				return PxFilterFlag::eSUPPRESS;
			}

		    if (filterData0.word2 != 0 && filterData0.word2 == filterData1.word2)
			{
				if ((filterData0.word3 & (1u << filterData1.word1)) || (filterData1.word3 & (1u << filterData0.word1)))
				{
					return PxFilterFlag::eSUPPRESS;
				}
			}

		    if (PxFilterObjectIsTrigger(attributes0) || PxFilterObjectIsTrigger(attributes1))
			{
				pairFlags = PxPairFlag::eTRIGGER_DEFAULT;
				return PxFilterFlag::eDEFAULT;
			}

			pairFlags = PxPairFlag::eCONTACT_DEFAULT
					  | PxPairFlag::eDETECT_CCD_CONTACT
					  | PxPairFlag::eNOTIFY_TOUCH_CCD
					  |	PxPairFlag::eNOTIFY_TOUCH_FOUND
					  |	PxPairFlag::eNOTIFY_TOUCH_PERSISTS
					  | PxPairFlag::eNOTIFY_CONTACT_POINTS
					  | PxPairFlag::eCONTACT_EVENT_POSE;

			CollisionFilterData fd1(filterData0);
			CollisionFilterData fd2(filterData1);
			if (fd1.HasConveyorFlag() || fd2.HasConveyorFlag()) {
				pairFlags |= PxPairFlag::eMODIFY_CONTACTS;
			}

			return PxFilterFlag::eDEFAULT;
		};
	
	m_pScene = m_pPhysics->createScene(sceneDesc);

	if (debugEnabled)
	{
		PxPvdSceneClient *pvdClient = m_pScene->getScenePvdClient();

		if (pvdClient)
		{
			pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
			pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
			pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
			ALERT( at_console, "PhysX: PVD scene client attached, contacts/constraints/queries will be transmitted\n" );
		}
		else
		{
			// connect() above failed, nothing will ever be transmitted
			ALERT( at_console, "PhysX: no PVD scene client - PVD was not connected when the scene was created, so nothing will be transmitted\n" );
		}
	}

	m_pDefaultMaterial = m_pPhysics->createMaterial(0.5f, 0.5f, 0.0f);
	m_pConveyorMaterial = m_pPhysics->createMaterial(1.0f, 1.0f, 0.0f);
}

/*
=================
FreePhysic

releases the scene, cooking, dispatcher, physics and foundation and clears all handles
=================
*/
void CPhysicPhysX :: FreePhysic( void )
{
	if (m_pScene) { m_pScene->release(); }
	if (m_pCooking) { m_pCooking->release(); }
	if (m_pDispatcher) { m_pDispatcher->release(); }
	if (m_pPhysics) { m_pPhysics->release(); }
	if (m_pVisualDebugger) 
	{
		PxPvdTransport *transport = m_pVisualDebugger->getTransport();
		m_pVisualDebugger->release();
		if (transport) {
			transport->release();
		}
	}

	PxCloseExtensions();
	if (m_pFoundation) { m_pFoundation->release(); }

	m_eventHandler.reset();
	m_contactModifyCallback.reset();
	m_holdableController.reset();

	m_pScene = nullptr;
	m_pCooking = nullptr;
	m_pPhysics = nullptr;
	m_pVisualDebugger = nullptr;
	m_pFoundation = nullptr;
}

/*
=================
Update

steps the simulation at a fixed rate, clamping gravity and applying holdable and water forces on each substep
=================
*/
void CPhysicPhysX :: Update( float flTimeDelta )
{
	if( !m_pScene || GET_SERVER_STATE() != SERVER_ACTIVE )
	{
		return;
	}

	if( g_psv_gravity )
	{
		// clamp gravity
		if( g_psv_gravity->value < 0.0f )
		{
			CVAR_SET_FLOAT( "sv_gravity", 0.0f );
		}
		if( g_psv_gravity->value > 800.0f )
		{
			CVAR_SET_FLOAT( "sv_gravity", 800.0f );
		}

		PxVec3 gravity = m_pScene->getGravity();
		if( gravity.z != -( g_psv_gravity->value ))
		{
			ALERT( at_aiconsole, "gravity changed from %g to %g\n", gravity.z, -(g_psv_gravity->value));
			gravity.z = -(g_psv_gravity->value);
			m_pScene->setGravity( gravity );
		}
	}

	HandleEvents();

	m_flAccumulator += flTimeDelta;
	while (m_flAccumulator > k_SimulationStepSize)
	{
		m_flAccumulator -= k_SimulationStepSize;
		m_holdableController->ApplyForces();
		RagdollApplyWaterForces();
		m_pScene->simulate(k_SimulationStepSize);
		m_pScene->fetchResults(true);
	}
}

/*
=================
EndFrame

updates ragdolls and fills the p_speeds statistics string after simulation
=================
*/
void CPhysicPhysX :: EndFrame( void )
{
	if( !m_pScene || GET_SERVER_STATE() != SERVER_ACTIVE )
	{
		return;
	}

	UpdateRagdolls();

	// fill physics stats
	if( !p_speeds || p_speeds->value <= 0.0f )
	{
		return;
	}

	PxSimulationStatistics stats;
	m_pScene->getSimulationStatistics( stats );

	switch( (int)p_speeds->value )
	{
	case 1:
		Q_snprintf(p_speeds_msg, sizeof(p_speeds_msg), 
			"%3i active dynamic bodies\n%3i static bodies\n%3i dynamic bodies",
			stats.nbActiveDynamicBodies, 
			stats.nbStaticBodies, 
			stats.nbDynamicBodies
		);
		break;		
	}
}

/*
=================
RemoveBody

releases the actor (and any ragdoll) backing an edict and clears its user data
=================
*/
void CPhysicPhysX :: RemoveBody( edict_t *pEdict )
{
	if( !m_pScene || !pEdict || pEdict->free )
	{
		return; // scene purge all the objects automatically
	}

	int ragdollIndex = FindRagdoll( ENTINDEX( pEdict ));
	if( ragdollIndex != -1 )
	{
		ReleaseRagdoll( ragdollIndex, true );
	}

	CBaseEntity *pEntity = CBaseEntity::Instance( pEdict );
	PxActor *pActor = ActorFromEntity( pEntity );

	if (pActor)
	{
		// forget queued events referencing the actor before it dies
		m_eventHandler->purgeActor(pActor);
		m_pScene->removeActor(*pActor);
		pActor->release();
	}
	pEntity->m_pUserData = nullptr;
}

/*
=================
ConvexMeshFromBmodel

cooks a convex hull from a brush model's render-face vertices
=================
*/
PxConvexMesh *CPhysicPhysX :: ConvexMeshFromBmodel( entvars_t *pev, int modelindex )
{
	if( !m_pCooking )
	{
		return NULL; // don't spam console about missed NxCooking.dll
	}

	if( modelindex == 1 )
	{
		ALERT( at_error, "ConvexMeshFromBmodel: can't create convex hull from worldmodel\n" );
		return NULL; // don't create rigidbody from world
          }

	model_t *bmodel;

	// get a world struct
	if(( bmodel = (model_t *)MODEL_HANDLE( modelindex )) == NULL )
	{
		ALERT( at_error, "ConvexMeshFromBmodel: unbale to fetch model pointer %i\n", modelindex );
		return NULL;
	}

	if( bmodel->nummodelsurfaces <= 0 )
	{
		ALERT( at_aiconsole, "ConvexMeshFromBmodel: %s has no visible surfaces\n", bmodel->name );
		m_fDisableWarning = TRUE;
		return NULL;
	}

	int numVerts = 0, totalVerts = 0;
	PxConvexMesh *pHull = NULL;
	msurface_t *psurf;
	Vector *verts;
	int i, j;

	// compute vertexes count
	psurf = &bmodel->surfaces[bmodel->firstmodelsurface];
	for( i = 0; i < bmodel->nummodelsurfaces; i++, psurf++ )
	{
		totalVerts += psurf->numedges;
	}

	// create a temp vertices array
	verts = new Vector[totalVerts];

	psurf = &bmodel->surfaces[bmodel->firstmodelsurface];
	for( i = 0; i < bmodel->nummodelsurfaces; i++, psurf++ )
	{
		for( j = 0; j < psurf->numedges; j++ )
		{
			int e = bmodel->surfedges[psurf->firstedge+j];
			int v = (e > 0) ? bmodel->edges[e].v[0] : bmodel->edges[-e].v[1];
			verts[numVerts++] = bmodel->vertexes[v].position;
		}
	}

	PxConvexMeshDesc meshDesc;
	meshDesc.points.data = verts;
	meshDesc.points.stride = sizeof(Vector);
	meshDesc.points.count = numVerts;
	meshDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

	MemoryWriteBuffer outputBuffer;
	bool status = m_pCooking->cookConvexMesh(meshDesc, outputBuffer);
	delete [] verts;

	if( !status )
	{
		ALERT( at_error, "failed to create convex mesh from %s\n", bmodel->name );
		return NULL;
	}

	MemoryReadBuffer inputBuffer(outputBuffer.getData(), outputBuffer.getSize());
	pHull = m_pPhysics->createConvexMesh(inputBuffer);
	if( !pHull ) { ALERT( at_error, "failed to create convex mesh from %s\n", bmodel->name ); }

	return pHull;
}

/*
=================
TriangleMeshFromBmodel

cooks a triangle mesh for a brush model, rebuilt from its hull-0 solid boundary or, as a fallback, its render faces
=================
*/
PxTriangleMesh *CPhysicPhysX :: TriangleMeshFromBmodel( entvars_t *pev, int modelindex )
{
	if( !m_pCooking )
	{
		ALERT( at_error, "TriangleMeshFromBmodel: cooking library unavailable, no collision for model %i\n", modelindex );
		return NULL;
	}

	if( modelindex == 1 )
	{
		ALERT( at_error, "TriangleMeshFromBmodel: can't create triangle mesh from worldmodel\n" );
		return NULL; // don't create rigidbody from world
          }

	model_t *bmodel;

	// get a world struct
	if(( bmodel = (model_t *)MODEL_HANDLE( modelindex )) == NULL )
	{
		ALERT( at_error, "TriangleMeshFromBmodel: unable to fetch model pointer %i\n", modelindex );
		return NULL;
	}

	if( bmodel->nummodelsurfaces <= 0 )
	{
		ALERT( at_aiconsole, "TriangleMeshFromBmodel: %s has no visible surfaces\n", bmodel->name );
		m_fDisableWarning = TRUE;
		return NULL;
	}

	// don't build hulls for water
	if( FBitSet( bmodel->flags, MODEL_LIQUID ))
	{
		m_fDisableWarning = TRUE;
		return NULL;
	}

	PxTriangleMesh *pMesh = NULL;

	// rebuild collision from the submodel's hull 0 like the world, so invisible but solid faces (null textures) are included
	std::vector<PxVec3> subVerts;
	std::vector<PxU32> subTris;
	int headnode = bmodel->hulls[0].firstclipnode;
	if( bmodel->nodes && headnode >= 0 && headnode < bmodel->numnodes )
	{
		PW_BuildWorld( bmodel->nodes + headnode, subVerts, subTris );
	}

	PxTriangleMeshDesc meshDesc;
	PxU32 *indices = NULL;

	if( !subTris.empty() )
	{
		meshDesc.points.count = (PxU32)subVerts.size();
		meshDesc.points.stride = sizeof( PxVec3 );
		meshDesc.points.data = subVerts.data();
		meshDesc.triangles.count = (PxU32)( subTris.size() / 3 );
		meshDesc.triangles.stride = 3 * sizeof( PxU32 );
		meshDesc.triangles.data = subTris.data();
		meshDesc.flags = (PxMeshFlags)0;
	}
	else
	{
		// fallback: render faces (misses null faces)
		int numElems = 0, totalElems = 0;
		for( int i = 0; i < bmodel->nummodelsurfaces; i++ )
		{
			totalElems += ( bmodel->surfaces[bmodel->firstmodelsurface + i].numedges - 2 );
		}

		indices = new PxU32[totalElems * 3];
		for( int i = 0; i < bmodel->nummodelsurfaces; i++ )
		{
			msurface_t *face = &bmodel->surfaces[bmodel->firstmodelsurface + i];
			int k = face->firstedge;
			for( int j = 0; j < face->numedges - 2; j++ )
			{
				indices[numElems*3+0] = ConvertEdgeToIndex( bmodel, k );
				indices[numElems*3+1] = ConvertEdgeToIndex( bmodel, k + j + 2 );
				indices[numElems*3+2] = ConvertEdgeToIndex( bmodel, k + j + 1 );
				numElems++;
			}
		}

		meshDesc.points.count = bmodel->numvertexes;
		meshDesc.points.stride = sizeof( mvertex_t );
		meshDesc.points.data = (const PxVec3 *)&( bmodel->vertexes[0].position );
		meshDesc.triangles.count = numElems;
		meshDesc.triangles.stride = 3 * sizeof( PxU32 );
		meshDesc.triangles.data = indices;
		meshDesc.flags = (PxMeshFlags)0;
	}

#ifdef _DEBUG
	// mesh should be validated before cooked without the mesh cleaning
	bool res = m_pCooking->validateTriangleMesh(meshDesc);
	PX_ASSERT(res);
#endif

	MemoryWriteBuffer outputBuffer;
	bool status = m_pCooking->cookTriangleMesh(meshDesc, outputBuffer);
	delete [] indices;

	if( !status )
	{
		ALERT( at_error, "failed to create triangle mesh from %s\n", bmodel->name );
		return NULL;
	}

	MemoryReadBuffer inputBuffer(outputBuffer.getData(), outputBuffer.getSize());
	pMesh = m_pPhysics->createTriangleMesh(inputBuffer);
	if( !pMesh ) 
	{
		ALERT( at_error, "failed to create triangle mesh from %s\n", bmodel->name );
	}

	return pMesh;
}

/*
=================
StudioCalcBoneQuaterion

computes a bone's rotation quaternion from its default pose and animation data
=================
*/
void CPhysicPhysX :: StudioCalcBoneQuaterion( mstudiobone_t *pbone, mstudioanim_t *panim, Vector4D &q )
{
	mstudioanimvalue_t *panimvalue;
	Radian angle;

	for( int j = 0; j < 3; j++ )
	{
		if( !panim || panim->offset[j+3] == 0 )
		{
			angle[j] = pbone->value[j+3]; // default;
		}
		else
		{
			panimvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[j+3]);
			angle[j] = panimvalue[1].value;
			angle[j] = pbone->value[j+3] + angle[j] * pbone->scale[j+3];
		}
	}

	AngleQuaternion( angle, q );
}

/*
=================
StudioCalcBonePosition

computes a bone's position from its default pose and animation data
=================
*/
void CPhysicPhysX :: StudioCalcBonePosition( mstudiobone_t *pbone, mstudioanim_t *panim, Vector &pos )
{
	mstudioanimvalue_t *panimvalue;

	for( int j = 0; j < 3; j++ )
	{
		pos[j] = pbone->value[j]; // default;

		if( panim && panim->offset[j] != 0 )
		{
			panimvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[j]);
			pos[j] += panimvalue[1].value * pbone->scale[j];
		}
	}
}

/*
=================
ConvexMeshFromStudio

cooks (or loads from cache) a convex hull built from a studio model's default pose
=================
*/
PxConvexMesh *CPhysicPhysX :: ConvexMeshFromStudio( entvars_t *pev, int modelindex, int32_t body, int32_t skin )
{
	if( UTIL_GetModelType( modelindex ) != mod_studio )
	{
		ALERT( at_error, "CollisionFromStudio: not a studio model\n" );
		return NULL;
    }

	model_t *smodel = (model_t *)MODEL_HANDLE( modelindex );
	studiohdr_t *phdr = (studiohdr_t *)smodel->cache.data;

	if( !phdr || phdr->numbones < 1 )
	{
		ALERT( at_error, "CollisionFromStudio: bad model header\n" );
		return NULL;
	}

	PxConvexMesh *pHull = NULL;
	char cacheFileName[MAX_PATH];
	uint32_t modelStateHash = GetHashForModelState(smodel, body, skin);
	CacheNameForModel(smodel, cacheFileName, sizeof(cacheFileName), modelStateHash, ".hull");

	if (CheckFileTimes(smodel->name, cacheFileName))
	{
		// hull is never than studiomodel. Trying to load it
		UserStream cacheFileStream(cacheFileName, true);
		pHull = m_pPhysics->createConvexMesh(cacheFileStream);

		if( !pHull )
		{
			// we failed to loading existed hull and can't cooking new :(
			if( m_pCooking == NULL )
			{
				return NULL; // don't spam console about missed nxCooking.dll
			}

			// trying to rebuild hull
			ALERT( at_error, "Convex mesh for %s is corrupted. Rebuilding...\n", smodel->name );
		}
		else
		{
			// all is ok
			return pHull;
		}
	}
	else
	{
		// can't cooking new hull because nxCooking.dll is missed
		if( m_pCooking == NULL )
		{
			return NULL; // don't spam console about missed nxCooking.dll
		}

		// trying to rebuild hull
		ALERT( at_console, "Convex mesh for %s is out of Date. Rebuilding...\n", smodel->name );
	}

	// at this point nxCooking instance is always valid

	// compute default pose for building mesh from
	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);
	mstudioseqgroup_t *pseqgroup = (mstudioseqgroup_t *)((byte *)phdr + phdr->seqgroupindex) + pseqdesc->seqgroup;

	// sanity check
	if( pseqdesc->seqgroup != 0 )
	{
		ALERT( at_error, "CollisionFromStudio: bad sequence group (must be 0)\n" );
		return NULL;
	}

	mstudioanim_t *panim = (mstudioanim_t *)((byte *)phdr + pseqgroup->data + pseqdesc->animindex);
	mstudiobone_t *pbone = (mstudiobone_t *)((byte *)phdr + phdr->boneindex);
	static Vector pos[MAXSTUDIOBONES];
	static Vector4D q[MAXSTUDIOBONES];

	for( int i = 0; i < phdr->numbones; i++, pbone++, panim++ ) 
	{
		StudioCalcBoneQuaterion( pbone, panim, q[i] );
		StudioCalcBonePosition( pbone, panim, pos[i] );
	}

	pbone = (mstudiobone_t *)((byte *)phdr + phdr->boneindex);
	matrix4x4	transform, bonematrix, bonetransform[MAXSTUDIOBONES];
	transform.Identity();

	// compute bones for default anim
	for( int i = 0; i < phdr->numbones; i++ ) 
	{
		// initialize bonematrix
		bonematrix = matrix3x4( pos[i], q[i] );

		if( pbone[i].parent == -1 ) 
		{
			bonetransform[i] = transform.ConcatTransforms( bonematrix );
		}
		else { bonetransform[i] = bonetransform[pbone[i].parent].ConcatTransforms( bonematrix ); }
	}

	mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((byte *)phdr + phdr->bodypartindex);
	mstudiomodel_t *psubmodel = (mstudiomodel_t *)((byte *)phdr + pbodypart->modelindex);
	Vector *pstudioverts = (Vector *)((byte *)phdr + psubmodel->vertindex);
	Vector *m_verts = new Vector[psubmodel->numverts];
	byte *pvertbone = ((byte *)phdr + psubmodel->vertinfoindex);
	Vector *verts = new Vector[psubmodel->numverts * 8];	// allocate temporary vertices array
	PxU32 *indices = new PxU32[psubmodel->numverts * 24];
	int numVerts = 0, numElems = 0;
	Vector tmp;

	// setup all the vertices
	for( int i = 0; i < psubmodel->numverts; i++ )
	{
		m_verts[i] = bonetransform[pvertbone[i]].VectorTransform( pstudioverts[i] );
	}

	for( int j = 0; j < psubmodel->nummesh; j++ ) 
	{
		int i;
		mstudiomesh_t *pmesh = (mstudiomesh_t *)((byte *)phdr + psubmodel->meshindex) + j;
		short *ptricmds = (short *)((byte *)phdr + pmesh->triindex);

		while( i = *( ptricmds++ ))
		{
			int	vertexState = 0;
			qboolean	tri_strip;

			if( i < 0 )
			{
				tri_strip = false;
				i = -i;
			}
			else
			{
				tri_strip = true;
			}

			for( ; i > 0; i--, ptricmds += 4 )
			{
				// build in indices
				if( vertexState++ < 3 )
				{
					indices[numElems++] = numVerts;
				}
				else if( tri_strip )
				{
					// flip triangles between clockwise and counter clockwise
					if( vertexState & 1 )
					{
						// draw triangle [n-2 n-1 n]
						indices[numElems++] = numVerts - 2;
						indices[numElems++] = numVerts - 1;
						indices[numElems++] = numVerts;
					}
					else
					{
						// draw triangle [n-1 n-2 n]
						indices[numElems++] = numVerts - 1;
						indices[numElems++] = numVerts - 2;
						indices[numElems++] = numVerts;
					}
				}
				else
				{
					// draw triangle fan [0 n-1 n]
					indices[numElems++] = numVerts - ( vertexState - 1 );
					indices[numElems++] = numVerts - 1;
					indices[numElems++] = numVerts;
				}

				verts[numVerts++] = m_verts[ptricmds[0]];
			}
		}
	}

	PxConvexMeshDesc meshDesc;
	meshDesc.indices.count = numElems / 3;
	meshDesc.indices.data = indices;
	meshDesc.indices.stride = 3 * sizeof( PxU32 );
	meshDesc.points.count = numVerts;
	meshDesc.points.data = verts;
	meshDesc.points.stride = sizeof(Vector);
	meshDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

	UserStream outputFileStream(cacheFileName, false);
	bool status = m_pCooking->cookConvexMesh(meshDesc, outputFileStream);

	delete [] verts;
	delete [] m_verts;
	delete [] indices;

	if( !status )
	{
		ALERT( at_error, "failed to create convex mesh from %s\n", smodel->name );
		return NULL;
	}

	UserStream inputFileStream(cacheFileName, true);
	pHull = m_pPhysics->createConvexMesh(inputFileStream);
	if( !pHull ) { ALERT( at_error, "failed to create convex mesh from %s\n", smodel->name ); }

	return pHull;
}

/*
=================
TriangleMeshFromStudio

cooks (or loads from cache) a triangle mesh from a studio model's solid meshes in its default pose
=================
*/
PxTriangleMesh *CPhysicPhysX::TriangleMeshFromStudio(entvars_t *pev, int modelindex, int32_t body, int32_t skin)
{
	if (UTIL_GetModelType(modelindex) != mod_studio)
	{
		ALERT(at_error, "TriangleMeshFromStudio: not a studio model\n");
		return NULL;
	}

	model_t *smodel = (model_t *)MODEL_HANDLE(modelindex);
	studiohdr_t *phdr = (studiohdr_t *)smodel->cache.data;
	int solidMeshes = 0;

	if (!phdr || phdr->numbones < 1)
	{
		ALERT(at_error, "TriangleMeshFromStudio: bad model header\n");
		return NULL;
	}

	mstudiotexture_t *ptexture = (mstudiotexture_t *)((byte *)phdr + phdr->textureindex);

	for (int i = 0; i < phdr->numtextures; i++)
	{
		// skip this mesh it's probably foliage or somewhat
		if (FBitSet(ptexture[i].flags, STUDIO_NF_MASKED) && !FBitSet(ptexture[i].flags, STUDIO_NF_SOLIDGEOM))
		{
			continue;
		}

		solidMeshes++;
	}

	// model is non-solid
	if (!solidMeshes)
	{
		m_fDisableWarning = TRUE;
		return NULL;
	}

	PxTriangleMesh *pMesh = NULL;
	char cacheFilePath[MAX_PATH];
	uint32_t modelStateHash = GetHashForModelState(smodel, body, skin);
	CacheNameForModel(smodel, cacheFilePath, sizeof(cacheFilePath), modelStateHash, ".mesh");

	if (CheckFileTimes(smodel->name, cacheFilePath) && !m_fWorldChanged)
	{
		// hull is never than studiomodel. Trying to load it
		UserStream cacheFileStream(cacheFilePath, true);
		pMesh = m_pPhysics->createTriangleMesh(cacheFileStream);

		if (!pMesh)
		{
			// we failed to loading existed hull and can't cooking new :(
			if (m_pCooking == NULL)
			{
				return NULL; // don't spam console about missed nxCooking.dll
			}

			// trying to rebuild hull
			ALERT(at_error, "Triangle mesh for %s is corrupted. Rebuilding...\n", smodel->name);
		}
		else
		{
			// all is ok
			return pMesh;
		}
	}
	else
	{
		// can't cooking new hull because nxCooking.dll is missed
		if (m_pCooking == NULL)
		{
			return NULL; // don't spam console about missed nxCooking.dll
		}

		// trying to rebuild hull
		ALERT(at_console, "Triangle mesh for %s is out of Date. Rebuilding...\n", smodel->name);
	}

	// at this point nxCooking instance is always valid
	// compute default pose for building mesh from
	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);
	mstudioseqgroup_t *pseqgroup = (mstudioseqgroup_t *)((byte *)phdr + phdr->seqgroupindex) + pseqdesc->seqgroup;

	// sanity check
	if (pseqdesc->seqgroup != 0)
	{
		ALERT(at_error, "TriangleMeshFromStudio: bad sequence group (must be 0)\n");
		return NULL;
	}

	mstudioanim_t *panim = (mstudioanim_t *)((byte *)phdr + pseqgroup->data + pseqdesc->animindex);
	mstudiobone_t *pbone = (mstudiobone_t *)((byte *)phdr + phdr->boneindex);
	static Vector pos[MAXSTUDIOBONES];
	static Vector4D q[MAXSTUDIOBONES];

	for (int i = 0; i < phdr->numbones; i++, pbone++, panim++)
	{
		StudioCalcBoneQuaterion(pbone, panim, q[i]);
		StudioCalcBonePosition(pbone, panim, pos[i]);
	}

	pbone = (mstudiobone_t *)((byte *)phdr + phdr->boneindex);
	matrix4x4 transform, bonematrix, bonetransform[MAXSTUDIOBONES];
	transform.Identity();

	// compute bones for default anim
	for (int i = 0; i < phdr->numbones; i++)
	{
		// initialize bonematrix
		bonematrix = matrix3x4(pos[i], q[i]);

		if (pbone[i].parent == -1)
		{
			bonetransform[i] = transform.ConcatTransforms(bonematrix);
		}
		else { bonetransform[i] = bonetransform[pbone[i].parent].ConcatTransforms(bonematrix); }
	}

	int colliderBodygroup = pev->body;
	int totalVertSize = 0;
	for (int i = 0; i < phdr->numbodyparts; i++)
	{
		mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((byte *)phdr + phdr->bodypartindex) + i;
		int32_t index = (pev->body / pbodypart->base) % pbodypart->nummodels;
		mstudiomodel_t *psubmodel = (mstudiomodel_t *)((byte *)phdr + pbodypart->modelindex) + index;
		totalVertSize += psubmodel->numverts;
	}

	Vector *verts = new Vector[totalVertSize * 8]; // allocate temporary vertices array
	PxU32 *indices = new PxU32[totalVertSize * 24];
	int32_t skinNumber = bound( 0, pev->skin, phdr->numskinfamilies );
	int numVerts = 0, numElems = 0;
	Vector tmp;

	for (int k = 0; k < phdr->numbodyparts; k++)
	{
		int i;
		mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((byte *)phdr + phdr->bodypartindex) + k;
		int32_t index = (pev->body / pbodypart->base) % pbodypart->nummodels;
		mstudiomodel_t *psubmodel = (mstudiomodel_t *)((byte *)phdr + pbodypart->modelindex) + index;
		Vector *pstudioverts = (Vector *)((byte *)phdr + psubmodel->vertindex);
		Vector *m_verts = new Vector[psubmodel->numverts];
		byte *pvertbone = ((byte *)phdr + psubmodel->vertinfoindex);

		// setup all the vertices
		for (i = 0; i < psubmodel->numverts; i++) {
			m_verts[i] = bonetransform[pvertbone[i]].VectorTransform(pstudioverts[i]);
		}

		ptexture = (mstudiotexture_t *)((byte *)phdr + phdr->textureindex);
		short *pskinref = (short *)((byte *)phdr + phdr->skinindex);
		if (skinNumber != 0 && skinNumber < phdr->numskinfamilies)
		{
			pskinref += (skinNumber * phdr->numskinref);
		}

		for (int j = 0; j < psubmodel->nummesh; j++)
		{
			mstudiomesh_t *pmesh = (mstudiomesh_t *)((byte *)phdr + psubmodel->meshindex) + j;
			short *ptricmds = (short *)((byte *)phdr + pmesh->triindex);

			if (phdr->numtextures != 0 && phdr->textureindex != 0)
			{
				// skip this mesh it's probably foliage or somewhat
				const int flags = ptexture[pskinref[pmesh->skinref]].flags;
				if (FBitSet(flags, STUDIO_NF_MASKED) && !FBitSet(flags, STUDIO_NF_SOLIDGEOM))
				{
					continue;
				}
			}

			while (i = *(ptricmds++))
			{
				int	vertexState = 0;
				bool tri_strip;

				if (i < 0)
				{
					tri_strip = false;
					i = -i;
				}
				else
				{
					tri_strip = true;
				}

				for (; i > 0; i--, ptricmds += 4)
				{
					// build in indices
					if (vertexState++ < 3)
					{
						indices[numElems++] = numVerts;
					}
					else if (tri_strip)
					{
						// flip triangles between clockwise and counter clockwise
						if (vertexState & 1)
						{
							// draw triangle [n-2 n-1 n]
							indices[numElems++] = numVerts - 2;
							indices[numElems++] = numVerts - 1;
							indices[numElems++] = numVerts;
						}
						else
						{
							// draw triangle [n-1 n-2 n]
							indices[numElems++] = numVerts - 1;
							indices[numElems++] = numVerts - 2;
							indices[numElems++] = numVerts;
						}
					}
					else
					{
						// draw triangle fan [0 n-1 n]
						indices[numElems++] = numVerts - (vertexState - 1);
						indices[numElems++] = numVerts - 1;
						indices[numElems++] = numVerts;
					}

					//	verts[numVerts++] = m_verts[ptricmds[0]];
					verts[numVerts] = m_verts[ptricmds[0]];
					numVerts++;
				}
			}
		}

		delete[] m_verts;
	}

	PxTriangleMeshDesc meshDesc;
	meshDesc.triangles.data = indices;
	meshDesc.triangles.count = numElems / 3;
	meshDesc.triangles.stride = 3 * sizeof(PxU32);
	meshDesc.points.data = verts;
	meshDesc.points.count = numVerts;
	meshDesc.points.stride = sizeof(Vector);
	meshDesc.flags = PxMeshFlag::eFLIPNORMALS;

#ifdef _DEBUG
	// mesh should be validated before cooked without the mesh cleaning
	bool res = m_pCooking->validateTriangleMesh(meshDesc);
	PX_ASSERT(res);
#endif

	UserStream outputFileStream(cacheFilePath, false);
	bool status = m_pCooking->cookTriangleMesh(meshDesc, outputFileStream);
	delete[] verts;
	delete[] indices;

	if (!status)
	{
		ALERT(at_error, "failed to create triangle mesh from %s\n", smodel->name);
		return NULL;
	}

	UserStream inputFileStream(cacheFilePath, true);
	pMesh = m_pPhysics->createTriangleMesh(inputFileStream);
	if (!pMesh) { ALERT(at_error, "failed to create triangle mesh from %s\n", smodel->name); }

	return pMesh;
}

/*
=================
ConvexMeshFromEntity

dispatches to the brush or studio convex-hull builder for an entity's model
=================
*/
PxConvexMesh *CPhysicPhysX :: ConvexMeshFromEntity( CBaseEntity *pObject )
{
	if( !pObject || !m_pPhysics )
	{
		return NULL;
	}

	// check for bspmodel
	model_t *model = (model_t *)MODEL_HANDLE( pObject->pev->modelindex );

	if( !model || model->type == mod_bad )
	{
		ALERT( at_aiconsole, "ConvexMeshFromEntity: entity %s has NULL model\n", pObject->GetClassname( )); 
		return NULL;
	}

	PxConvexMesh *pCollision = NULL;

	// call the apropriate loader
	switch( model->type )
	{
	case mod_brush:
		pCollision = ConvexMeshFromBmodel( pObject->pev, pObject->pev->modelindex );	
		break;
	case mod_studio:
		pCollision = ConvexMeshFromStudio( pObject->pev, pObject->pev->modelindex, pObject->pev->body, pObject->pev->skin );	
		break;
	}

	if( !pCollision && !m_fDisableWarning )
	{
		ALERT( at_warning, "ConvexMeshFromEntity: %i has missing collision\n", pObject->pev->modelindex ); 
	}
	m_fDisableWarning = FALSE;

	return pCollision;
}

/*
=================
TriangleMeshFromEntity

dispatches to the brush or studio triangle-mesh builder for an entity's model
=================
*/
PxTriangleMesh *CPhysicPhysX :: TriangleMeshFromEntity( CBaseEntity *pObject )
{
	if( !pObject )
	{
		return NULL;
	}

	if( !m_pPhysics )
	{
		ALERT( at_error, "TriangleMeshFromEntity: %s spawned before physics initialization\n", pObject->GetClassname( ));
		return NULL;
	}

	// check for bspmodel
	model_t *model = (model_t *)MODEL_HANDLE( pObject->pev->modelindex );

	if( !model || model->type == mod_bad )
	{
		ALERT( at_error, "TriangleMeshFromEntity: entity %s has NULL model (modelindex %i)\n", pObject->GetClassname(), pObject->pev->modelindex );
		return NULL;
	}

	PxTriangleMesh *pCollision = NULL;

	// call the apropriate loader
	switch( model->type )
	{
	case mod_brush:
		pCollision = TriangleMeshFromBmodel( pObject->pev, pObject->pev->modelindex );	
		break;
	case mod_studio:
		pCollision = TriangleMeshFromStudio( pObject->pev, pObject->pev->modelindex, pObject->pev->body, pObject->pev->skin );	
		break;
	}

	if( !pCollision && !m_fDisableWarning )
	{
		ALERT( at_warning, "TriangleMeshFromEntity: %s has missing collision\n", pObject->GetClassname( )); 
	}
	m_fDisableWarning = FALSE;

	return pCollision;
}

/*
=================
ActorFromEntity

returns the PhysX actor backing an entity, unwrapping the vehicle actor if present
=================
*/
PxActor *CPhysicPhysX :: ActorFromEntity( CBaseEntity *pObject )
{
	if( FNullEnt( pObject ) || !pObject->m_pUserData )
	{
		return NULL;
	}
	return (PxActor *)pObject->m_pUserData;
}

/*
=================
GetIntersectionBounds

returns the overlap of two bounds, or an empty box when they don't intersect
=================
*/
physx::PxBounds3 CPhysicPhysX::GetIntersectionBounds(const physx::PxBounds3 &a, const physx::PxBounds3 &b)
{
	if (!a.intersects(b)) {
		return PxBounds3::empty();
	}

	PxBounds3 result;
	result.minimum.x = std::max(a.minimum.x, b.minimum.x);
	result.minimum.y = std::max(a.minimum.y, b.minimum.y);
	result.minimum.z = std::max(a.minimum.z, b.minimum.z);
	result.maximum.x = std::min(a.maximum.x, b.maximum.x);
	result.maximum.y = std::min(a.maximum.y, b.maximum.y);
	result.maximum.z = std::min(a.maximum.z, b.maximum.z);
	return result;
}

/*
=================
EntityFromActor

returns the entity that owns a PhysX actor
=================
*/
CBaseEntity *CPhysicPhysX :: EntityFromActor( PxActor *pObject )
{
	if( !pObject || !pObject->userData )
	{
		return NULL;
	}

	return CBaseEntity::Instance( (edict_t *)pObject->userData );
}

/*
=================
CheckCollision

returns true if any of the actor's shapes participate in simulation
=================
*/
bool CPhysicPhysX::CheckCollision(physx::PxRigidActor *pActor)
{
	std::vector<PxShape*> shapes;
	if (pActor->getNbShapes() > 0)
	{
		shapes.resize(pActor->getNbShapes());
		pActor->getShapes(&shapes[0], shapes.size());
		for (PxShape *shape : shapes) 
		{
			if (shape->getFlags() & PxShapeFlag::eSIMULATION_SHAPE) {
				return true;
			}
		}
	}
	return false;
}

/*
=================
ToggleCollision

enables or disables simulation and scene-query participation on all of an actor's shapes
=================
*/
void CPhysicPhysX::ToggleCollision(physx::PxRigidActor *pActor, bool enabled)
{
	std::vector<PxShape*> shapes;
	shapes.resize(pActor->getNbShapes());
	pActor->getShapes(&shapes[0], shapes.size());

	if (TracingStateChanges(pActor)) {
		ALERT(at_console, "PhysX: ToggleCollision( actor = %x, state = %s )\n", pActor, enabled ? "true" : "false");
	}

	for (PxShape *shape : shapes) 
	{
		bool collisionState = shape->getFlags() & PxShapeFlag::eSIMULATION_SHAPE;
		if (collisionState != enabled) 
		{
			// change state only if it's different, to avoid useless write access to API
			shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, enabled);
			shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, enabled);
		}
	}
}

/*
=================
CreateBodyFromEntity

creates a dynamic rigid body with a convex-hull shape for an entity
=================
*/
void *CPhysicPhysX :: CreateBodyFromEntity( CBaseEntity *pObject )
{
	PxConvexMesh *pCollision = ConvexMeshFromEntity(pObject);
	if (!pCollision)
	{
		return NULL;
	}

	PxRigidDynamic *pActor = m_pPhysics->createRigidDynamic(PxTransform(PxIdentity));
	PxMeshScale scale(pObject->GetScale());
	PxShape *pShape = PxRigidActorExt::createExclusiveShape(*pActor, PxConvexMeshGeometry(pCollision, scale), *m_pDefaultMaterial);

	// shape holds its own reference now
	pCollision->release();

	if (!pActor)
	{
		ALERT(at_error, "failed to create rigidbody from entity %s\n", pObject->GetClassname());
		return NULL;
	}

	float mat[16];
	float maxVelocity = CVAR_GET_FLOAT("sv_maxvelocity");
	matrix4x4(pObject->GetAbsOrigin(), pObject->GetAbsAngles(), 1.0f).CopyToArray(mat);
	PxTransform pose = PxTransform(PxMat44(mat));

	pActor->setGlobalPose(pose);
	pActor->setName(pObject->GetClassname());
	pActor->setSolverIterationCounts((PxU32)CVAR_GET_FLOAT("phys_solveriterations"));
	// speculative, not swept: swept CCD drops contacts against a moving mover once a pair separates, so props resting on a platform fall through it
	pActor->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_SPECULATIVE_CCD, CVAR_GET_FLOAT("phys_ccd") != 0.0f);
	pActor->setLinearVelocity(pObject->GetLocalVelocity());
	pActor->setAngularVelocity(pObject->GetLocalAvelocity());
	pActor->setMaxLinearVelocity(maxVelocity);
	pActor->setMaxAngularVelocity(720.0);
	pActor->userData = pObject->edict();

	float density = pObject->GetDensity();
	if (density <= 0.0f)
	{
		density = MetricDensityToEngine(CVAR_GET_FLOAT("phys_density_default"));
	}

	PxRigidBodyExt::updateMassAndInertia(*pActor, density);

	m_pScene->addActor(*pActor);
	pObject->m_iActorType = ACTOR_DYNAMIC;
	pObject->m_pUserData = pActor;
	return pActor;
}

/*
=================
CreateBoxFromEntity

used for characters: clients and monsters
=================
*/
void *CPhysicPhysX :: CreateBoxFromEntity( CBaseEntity *pObject )
{
	PxBoxGeometry boxGeometry;
	boxGeometry.halfExtents = pObject->pev->size * CVAR_GET_FLOAT("phys_character_padding");

	PxRigidDynamic *pActor = m_pPhysics->createRigidDynamic(PxTransform(PxIdentity));
	PxShape *pShape = PxRigidActorExt::createExclusiveShape(*pActor, boxGeometry, *m_pDefaultMaterial);

	if (!pActor)
	{
		ALERT( at_error, "failed to create rigidbody from entity %s\n", pObject->GetClassname());
		return NULL;
	}

	if (pShape)
	{
		PxFilterData filterData;
		filterData.word0 = k_FilterCharacter;
		pShape->setSimulationFilterData(filterData);
	}

	Vector origin = (pObject->IsMonster()) ? Vector( 0, 0, pObject->pev->maxs.z / 2.0f ) : g_vecZero;
	origin += pObject->GetAbsOrigin();
	PxTransform pose = PxTransform(origin);

	pActor->setName(pObject->GetClassname());
	pActor->setGlobalPose(pose);
	pActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
	pActor->userData = pObject->edict();
	m_pScene->addActor(*pActor);

	pObject->m_iActorType = ACTOR_CHARACTER;
	pObject->m_pUserData = pActor;

	return pActor;
}

// part names as they appear in the model's ragdoll config file
static const char *k_RagdollPartNames[RAGDOLL_PARTS] =
{
	"head",
	"torso",
	"upper_arm_right",
	"lower_arm_right",
	"upper_arm_left",
	"lower_arm_left",
	"upper_leg_right",
	"lower_leg_right",
	"upper_leg_left",
	"lower_leg_left",
};

struct RagdollJointDesc
{
	int child;
	int parent;
	float yMin, yMax;	// swing range around the joint frame Y, degrees
	float zMin, zMax;	// swing range around the joint frame Z, degrees
	float tMin, tMax;	// twist (roll) range around the limb axis, degrees
};

// fixed skeleton wiring: every part hangs off the torso (1) directly or through its upper limb
static const RagdollJointDesc k_RagdollJointTable[RAGDOLL_JOINTS] =
{
	{ 0, 1, -30.0f, 30.0f, -35.0f, 35.0f, -20.0f, 20.0f },	// neck
	{ 2, 1, -75.0f, 75.0f, -95.0f, 95.0f, -45.0f, 45.0f },	// shoulders
	{ 3, 2, -8.0f, 8.0f, -145.0f, 5.0f, -10.0f, 10.0f },	// elbows
	{ 4, 1, -75.0f, 75.0f, -95.0f, 95.0f, -45.0f, 45.0f },
	{ 5, 4, -8.0f, 8.0f, -145.0f, 5.0f, -10.0f, 10.0f },
	{ 6, 1, -65.0f, 65.0f, -85.0f, 45.0f, -35.0f, 35.0f },	// hips
	{ 7, 6, -8.0f, 8.0f, -145.0f, 5.0f, -8.0f, 8.0f },	// knees
	{ 8, 1, -65.0f, 65.0f, -85.0f, 45.0f, -35.0f, 35.0f },
	{ 9, 8, -8.0f, 8.0f, -145.0f, 5.0f, -8.0f, 8.0f },
};

// relative part masses of a humanoid (heavy torso, lighter limbs and head)
static const float k_RagdollMassRatio[RAGDOLL_PARTS] =
{
	1.25f, 6.25f, 1.25f, 1.25f, 1.25f, 1.25f, 1.875f, 1.875f, 1.875f, 1.875f
};

/*
=================
RagdollLimbTipBone

finds the bone at the far end of a part's limb: the next configured part down the chain, or the best aligned child bone
=================
*/
static int RagdollLimbTipBone( studiohdr_t *phdr, mstudiobone_t *pbones, const int bones[], int part, const matrix4x4 refWorld[] )
{
	static const int nextPart[RAGDOLL_PARTS] = { -1, -1, 3, -1, 5, -1, 7, -1, 9, -1 };

	if( nextPart[part] != -1 )
	{
		return bones[nextPart[part]];
	}

	Vector axis = refWorld[bones[part]].GetForward();
	Vector org = refWorld[bones[part]].GetOrigin();
	float bestDot = 0.25f;	// reject children pointing nowhere near the bone axis
	int best = -1;

	for( int j = 0; j < phdr->numbones; j++ )
	{
		if( pbones[j].parent != bones[part] )
		{
			continue;
		}

		Vector dir = refWorld[j].GetOrigin() - org;

		if( dir.Length() < 0.1f )
		{
			continue;
		}

		float dot = DotProduct( dir.Normalize(), axis );

		if( dot > bestDot )
		{
			best = j;
			bestDot = dot;
		}
	}

	return best;
}

/*
=================
RagdollPxTransform

converts an engine bone matrix into a PhysX transform
=================
*/
static PxTransform RagdollPxTransform( const matrix4x4 &m )
{
	float mat[16];
	matrix4x4( m ).CopyToArray( mat );
	return PxTransform( PxMat44( mat ));
}

/*
=================
FindRagdoll

returns the live ragdoll slot for an entity index, or -1
=================
*/
int CPhysicPhysX::FindRagdoll( int entindex ) const
{
	for( int i = 0; i < m_numRagdolls; i++ )
	{
		if( m_ragdolls[i].entindex == entindex )
		{
			return i;
		}
	}
	return -1;
}

/*
=================
ReleaseRagdoll

releases a ragdoll's joints (and optionally its bodies) and compacts the live list
=================
*/
void CPhysicPhysX::ReleaseRagdoll( size_t index, bool releaseBodies )
{
	RagdollDesc &rag = m_ragdolls[index];

	StopRagdollScrapeSound( rag );

	// joints must go before the bodies they connect
	for( int i = 0; i < RAGDOLL_JOINTS; i++ )
	{
		if( rag.joints[i] )
		{
			rag.joints[i]->release();
		}
		rag.joints[i] = nullptr;
	}

	if( releaseBodies )
	{
		for( int i = 0; i < RAGDOLL_PARTS; i++ )
		{
			if( rag.bodies[i] )
			{
				// forget queued events referencing the body before it dies
				m_eventHandler->purgeActor( rag.bodies[i] );
				m_pScene->removeActor( *rag.bodies[i] );
				rag.bodies[i]->release();
			}
			rag.bodies[i] = nullptr;
		}
	}

	// move the last one into the freed slot
	if( (int)index < m_numRagdolls - 1 )
	{
		m_ragdolls[index] = m_ragdolls[m_numRagdolls - 1];
	}
	m_numRagdolls--;
}

/*
=================
GetRagdollConfig

loads and caches a model's ragdoll config (bone names, joint limits, mass ratios, bone adjustments) parsed from the .txt beside the model
=================
*/
const RagdollConfig *CPhysicPhysX::GetRagdollConfig( const char *szModelName )
{
	if( !szModelName || !szModelName[0] )
	{
		return NULL;
	}

	// parsed configs are cached per model, negative results included
	for( int i = 0; i < m_numRagdollConfigs; i++ )
	{
		if( !Q_stricmp( m_ragdollConfigs[i].modelName, szModelName ))
		{
			return &m_ragdollConfigs[i].config;
		}
	}

	RagdollConfig config;
	config.valid = false;
	for( int i = 0; i < RAGDOLL_PARTS; i++ )
	{
		config.boneNames[i][0] = '\0';
		config.numLimits[i] = 0;
		for( int k = 0; k < 6; k++ )
		{
			config.limits[i][k] = 0.0f;
		}
		config.hasAxes[i] = false;
		config.hasRatio[i] = false;
		config.massRatio[i] = 0.0f;
		for( int k = 0; k < 3; k++ )
		{
			config.swingAxisY[i][k] = 0.0f;
			config.swingAxisZ[i][k] = 0.0f;
		}
	}
	config.numAdjustments = 0;
	config.numImpactSoft = 0;
	config.numImpactHard = 0;
	config.impactForce = 0.0f;
	config.scrapeRough[0] = '\0';
	config.scrapeSmooth[0] = '\0';

	// the config lives next to the model: models/foo.mdl -> models/foo.txt
	char szConfigPath[MAX_PATH];
	Q_strncpy( szConfigPath, szModelName, sizeof( szConfigPath ));
	COM_StripExtension( szConfigPath );
	Q_strncat( szConfigPath, ".txt", sizeof( szConfigPath ));

	int length = 0;
	char *pfile = (char *)LOAD_FILE( szConfigPath, &length );

	if( pfile )
	{
		char token[256];
		char *pdata = pfile;

		// each line pairs a part name with a quoted bone name: head "Bip01 Head"
		while(( pdata = COM_ParseFile( pdata, token )) != NULL )
		{
			// 'adjustment "bone" x y z': a bone (hand, finger) to reset to its reference pose plus an euler
			if( !Q_stricmp( token, "adjustment" ))
			{
				if(( pdata = COM_ParseFile( pdata, token )) == NULL )
				{
					break;
				}

				char adjBone[32];
				Q_strncpy( adjBone, token, sizeof( adjBone ));

				float e[3] = { 0.0f, 0.0f, 0.0f };
				int got = 0;
				for( ; got < 3; got++ )
				{
					char *ppeek = COM_ParseFile( pdata, token );
					if( !ppeek || ( !isdigit( token[0] ) && token[0] != '-' && token[0] != '+' && token[0] != '.' ))
					{
						break;
					}
					e[got] = Q_atof( token );
					pdata = ppeek;
				}

				if( got != 3 )
				{
					ALERT( at_warning, "GetRagdollConfig: %s adjustment \"%s\" needs 3 euler values, ignored\n", szConfigPath, adjBone );
				}
				else if( config.numAdjustments >= RAGDOLL_ADJUSTMENTS )
				{
					ALERT( at_warning, "GetRagdollConfig: %s too many adjustments, \"%s\" ignored\n", szConfigPath, adjBone );
				}
				else
				{
					Q_strncpy( config.adjustBoneNames[config.numAdjustments], adjBone, 32 );
					config.adjustEuler[config.numAdjustments][0] = e[0];
					config.adjustEuler[config.numAdjustments][1] = e[1];
					config.adjustEuler[config.numAdjustments][2] = e[2];
					config.numAdjustments++;
				}
				continue;
			}

			// 'impact_force N': contact impulse that flips the impact sound from soft to hard
			if( !Q_stricmp( token, "impact_force" ))
			{
				if(( pdata = COM_ParseFile( pdata, token )) == NULL )
				{
					break;
				}
				config.impactForce = Q_atof( token );
				continue;
			}

			// 'impact_soft/impact_hard "sound.wav"': one sound per line, repeat to add variants
			if( !Q_stricmp( token, "impact_soft" ) || !Q_stricmp( token, "impact_hard" ))
			{
				const bool hard = !Q_stricmp( token, "impact_hard" );

				if(( pdata = COM_ParseFile( pdata, token )) == NULL )
				{
					break;
				}

				int &count = hard ? config.numImpactHard : config.numImpactSoft;
				if( count >= RAGDOLL_IMPACT_SOUNDS )
				{
					ALERT( at_warning, "GetRagdollConfig: %s too many impact sounds, \"%s\" ignored\n", szConfigPath, token );
				}
				else
				{
					Q_strncpy( hard ? config.impactHard[count] : config.impactSoft[count], token, 64 );
					count++;
				}
				continue;
			}

			if( !Q_stricmp( token, "scrape_rough" ) || !Q_stricmp( token, "scrape_smooth" ))
			{
				const bool smooth = !Q_stricmp( token, "scrape_smooth" );

				if(( pdata = COM_ParseFile( pdata, token )) == NULL )
				{
					break;
				}

				Q_strncpy( smooth ? config.scrapeSmooth : config.scrapeRough, token, 64 );
				continue;
			}

			int part = -1;

			for( int i = 0; i < RAGDOLL_PARTS; i++ )
			{
				if( !Q_stricmp( token, k_RagdollPartNames[i] ))
				{
					part = i;
					break;
				}
			}

			if( part == -1 )
			{
				continue;
			}

			// the token after the part name is the quoted bone name
			if(( pdata = COM_ParseFile( pdata, token )) == NULL )
			{
				break;
			}

			Q_strncpy( config.boneNames[part], token, sizeof( config.boneNames[part] ));

			// two line forms: bone name alone (all defaults) or the full 13 numbers
			// gather every trailing numeric token, then split by count
			float nums[16];
			int numCount = 0;
			for( ; numCount < 16; numCount++ )
			{
				char *ppeek = COM_ParseFile( pdata, token );

				if( !ppeek || ( !isdigit( token[0] ) && token[0] != '-' && token[0] != '+' && token[0] != '.' ))
				{
					break;
				}

				nums[numCount] = Q_atof( token );
				pdata = ppeek;
			}

			if( numCount == 0 )
			{
				// bone-name-only form: leave every default in place
			}
			else if( numCount == 13 )
			{
				config.swingAxisY[part][0] = nums[0];
				config.swingAxisY[part][1] = nums[1];
				config.swingAxisY[part][2] = nums[2];
				config.swingAxisZ[part][0] = nums[3];
				config.swingAxisZ[part][1] = nums[4];
				config.swingAxisZ[part][2] = nums[5];
				config.hasAxes[part] = true;

				for( int k = 0; k < 6; k++ )
				{
					config.limits[part][k] = nums[6 + k];
				}
				config.numLimits[part] = 6;

				config.massRatio[part] = nums[12];
				config.hasRatio[part] = true;
			}
			else
			{
				ALERT( at_warning, "GetRagdollConfig: %s '%s' has %i values, expected 0 or 13\n",
					szConfigPath, k_RagdollPartNames[part], numCount );
			}
		}

		FREE_FILE( pfile );

		// a config is only usable if all ten parts got a bone
		config.valid = true;
		for( int i = 0; i < RAGDOLL_PARTS; i++ )
		{
			if( !config.boneNames[i][0] )
			{
				ALERT( at_warning, "GetRagdollConfig: %s is missing '%s', ragdoll disabled\n", szConfigPath, k_RagdollPartNames[i] );
				config.valid = false;
				break;
			}
		}
	}

	if( m_numRagdollConfigs < MAX_RAGDOLL_CONFIGS )
	{
		RagdollConfigEntry &entry = m_ragdollConfigs[m_numRagdollConfigs++];
		Q_strncpy( entry.modelName, szModelName, sizeof( entry.modelName ));
		entry.config = config;
		return &entry.config;
	}

	// cache full: serve this config uncached from a static slot
	ALERT( at_warning, "GetRagdollConfig: config cache is full, %s not cached\n", szModelName );
	static RagdollConfig s_overflowConfig;
	s_overflowConfig = config;
	return &s_overflowConfig;
}

/*
=================
PrecacheRagdoll

parses a model's ragdoll config at level load so the file i/o never happens at kill time
=================
*/
void CPhysicPhysX::PrecacheRagdoll( const char *szModelName )
{
	const RagdollConfig *config = GetRagdollConfig( szModelName );

	if( !config )
	{
		return;
	}

	for( int i = 0; i < config->numImpactSoft; i++ )
	{
		PRECACHE_SOUND( (char *)config->impactSoft[i] );
	}

	for( int i = 0; i < config->numImpactHard; i++ )
	{
		PRECACHE_SOUND( (char *)config->impactHard[i] );
	}

	if( config->scrapeRough[0] )
	{
		PRECACHE_SOUND( (char *)config->scrapeRough );
	}

	if( config->scrapeSmooth[0] )
	{
		PRECACHE_SOUND( (char *)config->scrapeSmooth );
	}
}

/*
=================
PrecachePlayerRagdolls

=================
*/
void CPhysicPhysX::PrecachePlayerRagdolls( void )
{
	// the biped the server always simulates plus the fallback config
	PrecacheRagdoll( "models/player.mdl" );

	// every selectable model, models/player/<name>/<name>.mdl
	int numFiles = 0;
	char **files = GET_FILES_LIST( "models/player/*/*.mdl", &numFiles, TRUE );

	for( int i = 0; i < numFiles; i++ )
	{
		PrecacheRagdoll( files[i] );
	}
}

/*
=================
GetRagdollImpactSound

picks a random impact sound for the model, hard tier when the contact impulse
beats the config threshold; also derives a volume from the impulse
=================
*/
const char *CPhysicPhysX::GetRagdollImpactSound( const char *szModelName, float flForce, float *flVolume )
{
	const RagdollConfig *config = GetRagdollConfig( szModelName );

	if( !config || ( config->numImpactSoft <= 0 && config->numImpactHard <= 0 ))
	{
		return NULL;
	}

	// hard tier needs hard sounds and enough impulse; either list covers for a missing one
	bool hard = ( config->numImpactHard > 0 );
	if( hard && config->numImpactSoft > 0 && flForce < config->impactForce )
	{
		hard = false;
	}

	if( flVolume )
	{
		// full volume at twice the tier threshold, quieter below it
		if( config->impactForce > 0.0f )
		{
			float frac = flForce / ( config->impactForce * 2.0f );
			*flVolume = bound( 0.4f, frac * frac, 1.0f );
		}
		else
		{
			*flVolume = 1.0f;
		}
	}

	if( hard )
	{
		return config->impactHard[RANDOM_LONG( 0, config->numImpactHard - 1 )];
	}

	return config->impactSoft[RANDOM_LONG( 0, config->numImpactSoft - 1 )];
}

void CPhysicPhysX::UpdateRagdollScrapeSound( RagdollDesc &rag, edict_t *pEdict )
{
	const RagdollConfig *config = GetRagdollConfig( STRING( pEdict->v.model ));
	if( !config )
	{
		return;
	}

	const char *sample = config->scrapeRough[0] ? config->scrapeRough : config->scrapeSmooth;
	if( !sample[0] )
	{
		return;
	}

	bool active = ( CVAR_GET_FLOAT( "phys_ragdoll_scrape" ) > 0.0f )
		&& ( gpGlobals->time - rag.lastScrapeTime < 0.15f )
		&& ( rag.waterFrac[1] < 0.5f );

	if( active )
	{
		float vol = bound( 0.25f, rag.scrapeSpeed * ( 1.0f / 250.0f ), 1.0f );
		int pitch = 95 + (int)( vol * 15.0f );

		if( !rag.scrapePlaying )
		{
			EMIT_SOUND_DYN( pEdict, CHAN_BODY, sample, vol, ATTN_NORM, 0, pitch );
			rag.scrapePlaying = true;
			rag.scrapeSoundTime = gpGlobals->time + 0.2f;
		}
		else if( gpGlobals->time > rag.scrapeSoundTime )
		{
			EMIT_SOUND_DYN( pEdict, CHAN_BODY, sample, vol, ATTN_NORM, SND_CHANGE_VOL | SND_CHANGE_PITCH, pitch );
			rag.scrapeSoundTime = gpGlobals->time + 0.2f;
		}

		rag.scrapeSpeed = 0.0f;
	}
	else if( rag.scrapePlaying )
	{
		STOP_SOUND( pEdict, CHAN_BODY, sample );
		rag.scrapePlaying = false;
		rag.scrapeSpeed = 0.0f;
	}
}

void CPhysicPhysX::StopRagdollScrapeSound( RagdollDesc &rag )
{
	if( !rag.scrapePlaying )
	{
		return;
	}

	rag.scrapePlaying = false;

	edict_t *pEdict = INDEXENT( rag.entindex );
	if( !pEdict || pEdict->free || pEdict->serialnumber != rag.serialnumber || !pEdict->v.model )
	{
		return;
	}

	const RagdollConfig *config = GetRagdollConfig( STRING( pEdict->v.model ));
	if( !config )
	{
		return;
	}

	const char *sample = config->scrapeRough[0] ? config->scrapeRough : config->scrapeSmooth;
	if( sample[0] )
	{
		STOP_SOUND( pEdict, CHAN_BODY, sample );
	}
}

/*
=================
ReloadRagdollConfigs

drops the config cache so edited .txt files are re-read on the next ragdoll spawn; backs the ragdoll_reload console command
=================
*/
void CPhysicPhysX::ReloadRagdollConfigs( void )
{
	int count = m_numRagdollConfigs;
	m_numRagdollConfigs = 0;
	ALERT( at_console, "ragdoll_reload: %i configs dropped\n", count );
}

/*
=================
RagdollComputeReferencePose

builds world bone matrices from the skeleton's bind pose, the neutral stance the ragdoll geometry and joint rest orientations are derived from
=================
*/
bool CPhysicPhysX::RagdollComputeReferencePose( studiohdr_t *phdr, matrix4x4 refWorld[] )
{
	mstudiobone_t *pbone = (mstudiobone_t *)((byte *)phdr + phdr->boneindex);
	matrix4x4 transform, bonematrix;
	transform.Identity();

	for( int i = 0; i < phdr->numbones; i++ )
	{
		Vector pos;
		Radian angle;
		Vector4D q;

		for( int j = 0; j < 3; j++ )
		{
			pos[j] = pbone[i].value[j];
			angle[j] = pbone[i].value[j+3];
		}

		AngleQuaternion( angle, q );
		bonematrix = matrix3x4( pos, q );

		if( pbone[i].parent == -1 )
		{
			refWorld[i] = transform.ConcatTransforms( bonematrix );
		}
		else { refWorld[i] = refWorld[pbone[i].parent].ConcatTransforms( bonematrix ); }
	}

	return true;
}

/*
=================
CreateRagdollEntity

queues a ragdoll for a freshly killed corpse, snapshotting its current bone pose now; the bodies are built next frame in SpawnRagdoll
=================
*/
void *CPhysicPhysX::CreateRagdollEntity( CBaseEntity *pObject )
{
	if( !pObject || !pObject->pev->model )
	{
		return NULL;
	}

	// no config, no ragdoll: a NULL return lets callers fall back cleanly
	if( !GetRagdollConfig( STRING( pObject->pev->model )))
	{
		return NULL;
	}

	// guard against double-queueing the same entity
	int entindex = pObject->entindex();
	if( FindRagdoll( entindex ) != -1 )
	{
		return NULL;
	}

	for( int i = 0; i < m_numPendingRagdolls; i++ )
	{
		if( m_pendingRagdolls[i].entindex == entindex )
		{
			return NULL;
		}
	}

	// queue full, these corpses keep their death animation
	if( m_numPendingRagdolls >= MAX_RAGDOLLS )
	{
		ALERT( at_warning, "CreateRagdollEntity: too many pending ragdolls\n" );
		return NULL;
	}

	// spawn deferred, restore paths call this before the world is set up
	PendingRagdoll &pending = m_pendingRagdolls[m_numPendingRagdolls];
	pending.entindex = entindex;
	pending.numBones = 0;

	// capture the pose now, the death sequence replaces the live animation before the deferred spawn runs
	if( UTIL_GetModelType( pObject->pev->modelindex ) == mod_studio )
	{
		studiohdr_t *phdr = (studiohdr_t *)CBaseAnimating::GetModelPtr( pObject->pev->modelindex );

		if( phdr && phdr->numbones >= 1 && phdr->numbones <= MAXSTUDIOBONES )
		{
			CBaseAnimating *pAnim = (CBaseAnimating *)pObject;

			for( int j = 0; j < phdr->numbones; j++ )
			{
				Vector org, ang;
				pAnim->GetBonePosition( j, org, ang );
				pending.boneWorld[j] = matrix4x4( org, ang, 1.0f );
			}
			pending.numBones = phdr->numbones;
		}
	}

	m_numPendingRagdolls++;

	return (void *)1;	// queued, the bodies are built next frame
}

/*
=================
SpawnRagdoll

builds the part bodies and joints for a queued ragdoll and registers it, spawning on the captured pose when supplied or the model's current bones
=================
*/
void *CPhysicPhysX::SpawnRagdoll( CBaseEntity *pObject, const PendingRagdoll *pPending )
{
	if( !pObject || !pObject->pev->model )
	{
		return NULL;
	}

	if( !m_pScene )
	{
		return NULL;
	}

	if( UTIL_GetModelType( pObject->pev->modelindex ) != mod_studio )
	{
		return NULL;
	}

	int entindex = pObject->entindex();
	if( FindRagdoll( entindex ) != -1 )
	{
		return NULL;
	}

	studiohdr_t *phdr = (studiohdr_t *)CBaseAnimating::GetModelPtr( pObject->pev->modelindex );
	if( !phdr || phdr->numbones < 1 || phdr->numbones > MAXSTUDIOBONES )
	{
		return NULL;
	}

	const RagdollConfig *pConfig = GetRagdollConfig( STRING( pObject->pev->model ));
	if( !pConfig || !pConfig->valid )
	{
		return NULL; // model has no ragdoll config
	}

	// resolve the config bone names against the model's bone table
	mstudiobone_t *pStudioBones = (mstudiobone_t *)((byte *)phdr + phdr->boneindex);
	int bones[RAGDOLL_PARTS];

	for( int i = 0; i < RAGDOLL_PARTS; i++ )
	{
		bones[i] = -1;

		for( int j = 0; j < phdr->numbones; j++ )
		{
			if( !Q_stricmp( pConfig->boneNames[i], pStudioBones[j].name ))
			{
				bones[i] = j;
				break;
			}
		}

		if( bones[i] == -1 )
		{
			ALERT( at_error, "CreateRagdollEntity: %s has no bone named \"%s\" for '%s'\n", STRING( pObject->pev->model ), pConfig->boneNames[i], k_RagdollPartNames[i] );
			return NULL;
		}
	}

	CBaseAnimating *pAnim = (CBaseAnimating *)pObject;

	// spawn on the pose captured at queue time, sample current bones as fallback
	static matrix4x4 boneWorld[MAXSTUDIOBONES];

	if( pPending && pPending->numBones == phdr->numbones )
	{
		for( int j = 0; j < phdr->numbones; j++ )
		{
			boneWorld[j] = pPending->boneWorld[j];
		}
	}
	else
	{
		for( int j = 0; j < phdr->numbones; j++ )
		{
			Vector org, ang;
			pAnim->GetBonePosition( j, org, ang );
			boneWorld[j] = matrix4x4( org, ang, 1.0f );
		}
	}

	RagdollDesc rag;
	rag.entindex = entindex;
	rag.serialnumber = pObject->edict()->serialnumber;
	rag.numBones = phdr->numbones;
	rag.asleep = false;
	rag.lastSendTime = 0.0f;
	rag.impactGraceUntil = 0.0f;
	rag.fadeStartTime = 0.0f;
	rag.lastScrapeTime = 0.0f;
	rag.scrapeSpeed = 0.0f;
	rag.scrapeSoundTime = 0.0f;
	rag.scrapePlaying = false;

	for( int i = 0; i < RAGDOLL_PARTS; i++ )
	{
		rag.waterFrac[i] = 0.0f;
		rag.prevWaterFrac[i] = 0.0f;
	}

	// assign every bone to a part by walking up its parents until a part bone is found; bones outside any chain (tails, weapons...) fall to the torso
	mstudiobone_t *pbone = (mstudiobone_t *)((byte *)phdr + phdr->boneindex);

	for( int j = 0; j < phdr->numbones; j++ )
	{
		int part = -1;
		int b = j;

		while( b != -1 && part == -1 )
		{
			for( int i = 0; i < RAGDOLL_PARTS; i++ )
			{
				if( bones[i] == b )
				{
					part = i;
					break;
				}
			}
			b = pbone[b].parent;
		}
		rag.bonePart[j] = ( part != -1 ) ? part : 1;
		rag.studioParent[j] = pbone[j].parent;
		rag.boneAdjusted[j] = false;
	}

	// derive geometry, collision masks and joint frames from the model reference pose
	static matrix4x4 refWorld[MAXSTUDIOBONES];
	if( !RagdollComputeReferencePose( phdr, refWorld ))
	{
		for( int j = 0; j < phdr->numbones; j++ )
		{
			refWorld[j] = boneWorld[j];
		}
	}

	// precompute the parent-relative transform each adjustment bone is pinned to
	for( int a = 0; a < pConfig->numAdjustments; a++ )
	{
		int adjBone = -1;
		for( int j = 0; j < phdr->numbones; j++ )
		{
			if( !Q_stricmp( pConfig->adjustBoneNames[a], pStudioBones[j].name ))
			{
				adjBone = j;
				break;
			}
		}

		if( adjBone == -1 )
		{
			ALERT( at_warning, "SpawnRagdoll: %s adjustment bone \"%s\" not found\n", STRING( pObject->pev->model ), pConfig->adjustBoneNames[a] );
			continue;
		}

		int adjParent = rag.studioParent[adjBone];
		if( adjParent == -1 )
		{
			ALERT( at_warning, "SpawnRagdoll: %s adjustment bone \"%s\" is a root bone, ignored\n", STRING( pObject->pev->model ), pConfig->adjustBoneNames[a] );
			continue;
		}

		matrix4x4 bindLocal = refWorld[adjParent].Invert().ConcatTransforms( refWorld[adjBone] );
		matrix4x4 euler( g_vecZero, Vector( pConfig->adjustEuler[a][0], pConfig->adjustEuler[a][1], pConfig->adjustEuler[a][2] ), 1.0f );

		rag.boneAdjustLocal[adjBone] = bindLocal.ConcatTransforms( euler );
		rag.boneAdjusted[adjBone] = true;
	}

	Vector velocity = pObject->GetAbsVelocity();

	// the collision geometry comes straight from the model hitboxes
	mstudiobbox_t *phitbox = (mstudiobbox_t *)((byte *)phdr + phdr->hitboxindex);
	int partHitboxes[RAGDOLL_PARTS] = { 0 };

	for( int h = 0; h < phdr->numhitboxes; h++ )
	{
		int b = phitbox[h].bone;
		if( b >= 0 && b < phdr->numbones )
		{
			partHitboxes[rag.bonePart[b]]++;
		}
	}

	for( int i = 0; i < RAGDOLL_PARTS; i++ )
	{
		if( partHitboxes[i] == 0 )
		{
			ALERT( at_warning, "CreateRagdollEntity: %s has no hitboxes for '%s', ragdoll disabled\n", STRING( pObject->pev->model ), k_RagdollPartNames[i] );
			return NULL;
		}
	}

	// build per-part masks of which other parts it must NOT collide with, starting with the jointed neighbours
	unsigned int noCollide[RAGDOLL_PARTS] = { 0 };

	for( int n = 0; n < RAGDOLL_JOINTS; n++ )
	{
		noCollide[k_RagdollJointTable[n].parent] |= ( 1u << k_RagdollJointTable[n].child );
		noCollide[k_RagdollJointTable[n].child] |= ( 1u << k_RagdollJointTable[n].parent );
	}

	// body tuning, all configurable; the defaults keep the motion floppy but stable
	float radiusScale = CVAR_GET_FLOAT( "phys_ragdoll_radiusscale" );
	if( radiusScale <= 0.0f )
	{
		radiusScale = 1.0f;
	}
	float linearDamping = CVAR_GET_FLOAT( "phys_ragdoll_lineardamping" );
	float angularDamping = CVAR_GET_FLOAT( "phys_ragdoll_angulardamping" );
	float maxAngVelocity = CVAR_GET_FLOAT( "phys_ragdoll_maxangvelocity" );
	float sleepThreshold = CVAR_GET_FLOAT( "phys_ragdoll_sleepthreshold" );
	float maxDepenetration = CVAR_GET_FLOAT( "phys_ragdoll_maxdepenetration" );
	int solverIterations = bound( 1, (int)CVAR_GET_FLOAT( "phys_solveriterations" ), 255 );
	int solverVelIterations = bound( 1, (int)CVAR_GET_FLOAT( "phys_ragdoll_solverveliterations" ), 255 );
	bool enableCCD = CVAR_GET_FLOAT( "phys_ccd" ) != 0.0f;

	// one dynamic body per part, spawned at the death pose of its bone
	for( int i = 0; i < RAGDOLL_PARTS; i++ )
	{
		PxTransform pose = RagdollPxTransform( boneWorld[bones[i]] );
		PxRigidDynamic *pBody = m_pPhysics->createRigidDynamic( pose );

		// word1/word2 identify the part and owning ragdoll, word3 is the no-collide mask, the word0 debris bit keeps ragdolls out of character hulls
		PxFilterData filterData;
		filterData.word0 = k_FilterRagdollPart;
		filterData.word1 = i;
		filterData.word2 = entindex + 1;
		filterData.word3 = noCollide[i];

		// attach a box shape for every hitbox that belongs to this part
		for( int h = 0; h < phdr->numhitboxes; h++ )
		{
			int b = phitbox[h].bone;
			if( b < 0 || b >= phdr->numbones || rag.bonePart[b] != i )
			{
				continue;
			}

			Vector center = ( phitbox[h].bbmin + phitbox[h].bbmax ) * 0.5f;
			Vector half = ( phitbox[h].bbmax - phitbox[h].bbmin ) * 0.5f;

			// hitboxes live in bone space with X along the bone
			half.y *= radiusScale;
			half.z *= radiusScale;

			PxVec3 halfExtents( Q_max( half.x, 0.5f ), Q_max( half.y, 0.5f ), Q_max( half.z, 0.5f ));

			PxShape *pBoxShape = PxRigidActorExt::createExclusiveShape( *pBody, PxBoxGeometry( halfExtents ), *m_pDefaultMaterial );
			if( !pBoxShape )
			{
				continue;
			}

			// place the box relative to the part body
			matrix4x4 rel = boneWorld[bones[i]].Invert().ConcatTransforms( boneWorld[b] );
			PxTransform boneLocal = RagdollPxTransform( rel );
			pBoxShape->setLocalPose( boneLocal.transform( PxTransform( PxVec3( center.x, center.y, center.z ))));
			pBoxShape->setSimulationFilterData( filterData );
		}

		pBody->setLinearDamping( linearDamping );
		pBody->setAngularDamping( angularDamping );
		pBody->setSolverIterationCounts( solverIterations, solverVelIterations );
		// speculative, not swept: see the comment in CreateBodyFromEntity
		pBody->setRigidBodyFlag( PxRigidBodyFlag::eENABLE_SPECULATIVE_CCD, enableCCD );
		pBody->setMaxAngularVelocity( maxAngVelocity );
		pBody->setMaxDepenetrationVelocity( maxDepenetration );
		pBody->setSleepThreshold( sleepThreshold );
		PxRigidBodyExt::updateMassAndInertia( *pBody, MetricDensityToEngine( CVAR_GET_FLOAT( "phys_density_default" )));
		pBody->setLinearVelocity( velocity );
		pBody->setName( "ragdoll" );
		pBody->userData = pObject->edict();
		m_pScene->addActor( *pBody );

		rag.bodies[i] = pBody;
	}

	// redistribute the total mass with humanoid ratios
	float partRatio[RAGDOLL_PARTS];
	float totalMass = 0.0f;
	float totalRatio = 0.0f;

	for( int i = 0; i < RAGDOLL_PARTS; i++ )
	{
		partRatio[i] = pConfig->hasRatio[i] ? pConfig->massRatio[i] : k_RagdollMassRatio[i];
		if( partRatio[i] <= 0.0f )
		{
			partRatio[i] = k_RagdollMassRatio[i];	// guard bad/zero authored values
		}

		totalMass += rag.bodies[i]->getMass();
		totalRatio += partRatio[i];
	}

	for( int i = 0; i < RAGDOLL_PARTS; i++ )
	{
		PxRigidBodyExt::setMassAndUpdateInertia( *rag.bodies[i], totalMass * partRatio[i] / totalRatio );
	}

	// seed each part with its bone's animation velocity, sampled by rewinding the sequence
	float animVelScale = CVAR_GET_FLOAT( "phys_ragdoll_animvelocity" );

	if( animVelScale > 0.0f && pAnim->m_flFrameRate > 0.0f )
	{
		const float animDt = 0.1f;
		float savedFrame = pObject->pev->frame;
		float prevFrame = savedFrame - animDt * pAnim->m_flFrameRate * pObject->pev->framerate;

		if( pAnim->m_fSequenceLoops )
		{
			while( prevFrame < 0.0f )
			{
				prevFrame += 256.0f;
			}
		}
		else { prevFrame = Q_max( prevFrame, 0.0f ); }

		if( fabs( prevFrame - savedFrame ) > 0.01f )
		{
			pObject->pev->frame = prevFrame;

			for( int i = 0; i < RAGDOLL_PARTS; i++ )
			{
				Vector org, ang;
				pAnim->GetBonePosition( bones[i], org, ang );

				Vector linVel = velocity + ( boneWorld[bones[i]].GetOrigin() - org ) * ( animVelScale / animDt );
				rag.bodies[i]->setLinearVelocity( PxVec3( linVel.x, linVel.y, linVel.z ));

				PxQuat qPrev = RagdollPxTransform( matrix4x4( org, ang, 1.0f )).q;
				PxQuat qCur = RagdollPxTransform( boneWorld[bones[i]] ).q;
				PxQuat qDelta = qCur * qPrev.getConjugate();
				if( qDelta.w < 0.0f )
				{
					qDelta = PxQuat( -qDelta.x, -qDelta.y, -qDelta.z, -qDelta.w );
				}

				float angle;
				PxVec3 axis;
				qDelta.toRadiansAndUnitAxis( angle, axis );

				PxVec3 angVel = axis * ( angle * animVelScale / animDt );
				float magnitude = angVel.magnitude();
				if( magnitude > maxAngVelocity )
				{
					angVel *= maxAngVelocity / magnitude;
				}

				rag.bodies[i]->setAngularVelocity( angVel );
			}

			pObject->pev->frame = savedFrame;
		}
	}

	// kick the part that took the killing blow at the actual hit position
	Vector hitPos, hitDir;
	float hitDamage;
	int hitGroup;
	float pushScale = CVAR_GET_FLOAT( "phys_ragdoll_push" );

	if( pushScale > 0.0f && pObject->GetLastHitInfo( hitPos, hitDir, hitDamage, hitGroup ))
	{
		int part = -1;

		// the engine already resolved which hitbox was struck, map its group to a part
		for( int h = 0; h < phdr->numhitboxes; h++ )
		{
			if( hitGroup != 0 && phitbox[h].group == hitGroup )
			{
				int b = phitbox[h].bone;
				if( b >= 0 && b < phdr->numbones )
				{
					part = rag.bonePart[b];
					break;
				}
			}
		}

		// generic hits (explosions etc): pick the part closest to the hit point
		if( part == -1 )
		{
			float bestDist = 999999.0f;

			for( int i = 0; i < RAGDOLL_PARTS; i++ )
			{
				float dist = ( boneWorld[bones[i]].GetOrigin() - hitPos ).Length();
				if( dist < bestDist )
				{
					bestDist = dist;
					part = i;
				}
			}
		}

		// push speed is damage scaled by the push cvar and the entity's knockback multiplier, clamped to the configured range (0 disables either bound)
		float pushSpeed = pushScale * pObject->GetRagdollImpulseMultiplier(hitDamage);
		float pushMin = CVAR_GET_FLOAT( "phys_ragdoll_push_min" );
		float pushMax = CVAR_GET_FLOAT( "phys_ragdoll_push_max" );

		if( pushMin > 0.0f && pushSpeed < pushMin )
		{
			pushSpeed = pushMin;
		}
		if( pushMax > 0.0f && pushSpeed > pushMax )
		{
			pushSpeed = pushMax;
		}

		PxRigidDynamic *pHitBody = rag.bodies[part];
		PxVec3 pxHitPos( hitPos.x, hitPos.y, hitPos.z );
		PxVec3 impulse = PxVec3( hitDir.x, hitDir.y, hitDir.z ) * ( pushSpeed * pHitBody->getMass( ));
		PxRigidBodyExt::addForceAtPos( *pHitBody, impulse, pxHitPos, PxForceMode::eIMPULSE );

		// the rest of the body gets a mass-weighted share of the same blow
		for( int i = 0; i < RAGDOLL_PARTS; i++ )
		{
			if( i == part )
			{
				continue;
			}

			PxRigidBodyExt::addForceAtPos( *rag.bodies[i], impulse * ( rag.bodies[i]->getMass() / totalMass ), pxHitPos, PxForceMode::eIMPULSE );
		}
	}

	// joint limit tuning: a global range scale, and optional soft limits
	float limitScale = CVAR_GET_FLOAT( "phys_ragdoll_limitscale" );
	if( limitScale <= 0.0f )
	{
		limitScale = 1.0f;
	}
	float limitSpring = CVAR_GET_FLOAT( "phys_ragdoll_limitspring" );
	float limitDamping = CVAR_GET_FLOAT( "phys_ragdoll_limitdamping" );
	float limitBlend = CVAR_GET_FLOAT( "phys_ragdoll_limitblend" );

	rag.spawnTime = gpGlobals->time;
	rag.limitBlendDuration = limitBlend;
	rag.limitBlendDone = ( limitBlend <= 0.0f );
	rag.limitSpring = limitSpring;
	rag.limitDamping = limitDamping;

	// connect the parts with D6 joints anchored at the child bone, with the reference pose as the zero (rest) orientation
	for( int n = 0; n < RAGDOLL_JOINTS; n++ )
	{
		const RagdollJointDesc &desc = k_RagdollJointTable[n];
		matrix4x4 parentPose = refWorld[bones[desc.parent]];
		matrix4x4 childPose = refWorld[bones[desc.child]];

		// the joint frame sits at the child bone. its X axis is the twist/rest axis, Y is swing1 (swingY), Z is swing2 (swingZ)
		matrix4x4 jointFrame = childPose;

		if( pConfig->hasAxes[desc.child] )
		{
			// config axes, world space at the reference pose. twist = swingY x swingZ, swingZ re-orthogonalized
			Vector axisY( pConfig->swingAxisY[desc.child][0], pConfig->swingAxisY[desc.child][1], pConfig->swingAxisY[desc.child][2] );
			Vector axisZ( pConfig->swingAxisZ[desc.child][0], pConfig->swingAxisZ[desc.child][1], pConfig->swingAxisZ[desc.child][2] );

			if( axisY.Length() > 0.001f && axisZ.Length() > 0.001f )
			{
				axisY = axisY.Normalize();
				Vector axisX = CrossProduct( axisY, axisZ );	// twist = swingY x swingZ

				if( axisX.Length() > 0.001f )
				{
					axisX = axisX.Normalize();
					axisZ = CrossProduct( axisX, axisY );	// re-orthogonalize swingZ

					matrix3x4 jf;
					jf.SetForward( axisX );
					jf.SetRight( axisY );
					jf.SetUp( axisZ );
					jf.SetOrigin( childPose.GetOrigin( ));
					jointFrame = matrix4x4( jf );
				}
			}
		}
		else
		{
			// no config axes: X points down the limb, only the bend axis roll is taken from the bone, reprojected perpendicular to the limb
			int tip = RagdollLimbTipBone( phdr, pStudioBones, bones, desc.child, refWorld );

			if( tip != -1 )
			{
				Vector axisX = refWorld[tip].GetOrigin() - childPose.GetOrigin();

				if( axisX.Length() > 0.1f )
				{
					axisX = axisX.Normalize();

					Vector axisY = childPose.GetRight();
					axisY = axisY - axisX * DotProduct( axisY, axisX );

					if( axisY.Length() < 0.001f ) // bone Y lies along the limb, take the roll from Z
					{
						axisY = childPose.GetUp() - axisX * DotProduct( childPose.GetUp(), axisX );
					}

					axisY = axisY.Normalize();

					matrix3x4 jf;
					jf.SetForward( axisX );
					jf.SetRight( axisY );
					jf.SetUp( CrossProduct( axisX, axisY ));
					jf.SetOrigin( childPose.GetOrigin( ));
					jointFrame = matrix4x4( jf );
				}
			}
		}

		matrix4x4 parentLocal = parentPose.Invert().ConcatTransforms( jointFrame );
		matrix4x4 childLocal = childPose.Invert().ConcatTransforms( jointFrame );

		// sets the rest orientation stays reference-pose
		parentLocal.SetOrigin( boneWorld[bones[desc.parent]].VectorITransform( boneWorld[bones[desc.child]].GetOrigin( )));

		PxD6Joint *pJoint = PxD6JointCreate( *m_pPhysics,
			rag.bodies[desc.parent], RagdollPxTransform( parentLocal ),
			rag.bodies[desc.child], RagdollPxTransform( childLocal ));

		if( !pJoint )
		{
			rag.joints[n] = nullptr;
			continue;
		}

		pJoint->setMotion( PxD6Axis::eTWIST, PxD6Motion::eLIMITED );
		pJoint->setMotion( PxD6Axis::eSWING1, PxD6Motion::eLIMITED );
		pJoint->setMotion( PxD6Axis::eSWING2, PxD6Motion::eLIMITED );

		// limit ranges, degrees: config values override the scaled defaults, up to three are symmetric ranges (0 = default), six the asymmetric min/max form
		const float *cfgLim = pConfig->limits[desc.child];
		int numLim = pConfig->numLimits[desc.child];

		float yMin = desc.yMin * limitScale, yMax = desc.yMax * limitScale;
		float zMin = desc.zMin * limitScale, zMax = desc.zMax * limitScale;
		float tMin = desc.tMin * limitScale, tMax = desc.tMax * limitScale;

		if( numLim >= 4 )
		{
			yMin = cfgLim[0]; yMax = cfgLim[1];
			zMin = cfgLim[2]; zMax = cfgLim[3];
			tMin = cfgLim[4]; tMax = cfgLim[5];
		}
		else
		{
			if( cfgLim[0] > 0.0f ) { yMax = cfgLim[0]; yMin = -yMax; }
			if( cfgLim[1] > 0.0f ) { zMax = cfgLim[1]; zMin = -zMax; }
			if( cfgLim[2] > 0.0f ) { tMax = cfgLim[2]; tMin = -tMax; }
		}

		// degrees -> radians, clamped to what PhysX accepts, min never above max
		yMin = bound( -3.05f, DEG2RAD( yMin ), 3.05f ); yMax = bound( yMin, DEG2RAD( yMax ), 3.05f );
		zMin = bound( -3.05f, DEG2RAD( zMin ), 3.05f ); zMax = bound( zMin, DEG2RAD( zMax ), 3.05f );
		tMin = bound( -3.05f, DEG2RAD( tMin ), 3.05f ); tMax = bound( tMin, DEG2RAD( tMax ), 3.05f );

		// the authored ranges are the target the joint blends to
		rag.jointLimitEnd[n][0] = yMin; rag.jointLimitEnd[n][1] = yMax;
		rag.jointLimitEnd[n][2] = zMin; rag.jointLimitEnd[n][3] = zMax;
		rag.jointLimitEnd[n][4] = tMin; rag.jointLimitEnd[n][5] = tMax;

		// widen the spawn ranges just enough to contain the death pose
		if( !rag.limitBlendDone )
		{
			matrix4x4 jointA = boneWorld[bones[desc.parent]].ConcatTransforms( parentLocal );
			matrix4x4 jointB = boneWorld[bones[desc.child]].ConcatTransforms( childLocal );
			matrix4x4 relCur = jointA.Invert().ConcatTransforms( jointB );

			Vector dir = relCur.GetForward();
			float swing = acosf( bound( -1.0f, dir.x, 1.0f ));

			if( swing > 0.001f )
			{
				float w = swing / sinf( swing );
				float ay = -dir.z * w;
				float az = dir.y * w;

				yMin = Q_min( yMin, bound( -3.05f, ay - 0.15f, 3.05f ));
				yMax = Q_max( yMax, bound( -3.05f, ay + 0.15f, 3.05f ));
				zMin = Q_min( zMin, bound( -3.05f, az - 0.15f, 3.05f ));
				zMax = Q_max( zMax, bound( -3.05f, az + 0.15f, 3.05f ));
			}

			Vector4D q = relCur.GetQuaternion();
			float phi = 2.0f * atan2f( q.x, q.w );

			if( phi > M_PI ) { phi -= M_PI * 2.0f; }
			if( phi < -M_PI ) { phi += M_PI * 2.0f; }

			tMin = Q_min( tMin, bound( -3.05f, phi - 0.15f, 3.05f ));
			tMax = Q_max( tMax, bound( -3.05f, phi + 0.15f, 3.05f ));
		}

		rag.jointLimitStart[n][0] = yMin; rag.jointLimitStart[n][1] = yMax;
		rag.jointLimitStart[n][2] = zMin; rag.jointLimitStart[n][3] = zMax;
		rag.jointLimitStart[n][4] = tMin; rag.jointLimitStart[n][5] = tMax;

		if( limitSpring > 0.0f )
		{
			pJoint->setPyramidSwingLimit( PxJointLimitPyramid( yMin, yMax, zMin, zMax, PxSpring( limitSpring, limitDamping )));
			pJoint->setTwistLimit( PxJointAngularLimitPair( tMin, tMax, PxSpring( limitSpring, limitDamping )));
		}
		else
		{
			pJoint->setPyramidSwingLimit( PxJointLimitPyramid( yMin, yMax, zMin, zMax ));
			pJoint->setTwistLimit( PxJointAngularLimitPair( tMin, tMax ));
		}

		// a friction drive dissipates pendulum energy
		float frictionDamping = CVAR_GET_FLOAT( "phys_ragdoll_jointfriction" );
		if( frictionDamping > 0.0f )
		{
			PxD6JointDrive jointFriction( 0.0f, frictionDamping, PX_MAX_F32, true );
			pJoint->setDrive( PxD6Drive::eSWING, jointFriction );
			pJoint->setDrive( PxD6Drive::eTWIST, jointFriction );
		}

		// preprocessing is disabled to survive degenerate starts
		pJoint->setConstraintFlag( PxConstraintFlag::eDISABLE_PREPROCESSING, true );

		// projection snaps drifted bodies back onto the joint after solver overload
		pJoint->setProjectionLinearTolerance( 2.0f );
		pJoint->setProjectionAngularTolerance( 0.4f );
		pJoint->setConstraintFlag( PxConstraintFlag::ePROJECTION, true );

		rag.joints[n] = pJoint;
	}

	// remember where every bone sits relative to its part body
	for( int j = 0; j < phdr->numbones; j++ )
	{
		rag.boneOffset[j] = boneWorld[bones[rag.bonePart[j]]].Invert().ConcatTransforms( boneWorld[j] );
	}

	// drop the old physics shadow so the corpse does not leave a kinematic box behind
	if( pObject->m_pUserData )
	{
		PxActor *pOldActor = ActorFromEntity( pObject );
		if( pOldActor )
		{
			m_eventHandler->purgeActor( pOldActor );
			m_pScene->removeActor( *pOldActor );
			pOldActor->release();
		}
		pObject->m_pUserData = nullptr;
	}

	// the engine must stop moving/colliding the corpse, physics owns it now
	pObject->pev->movetype = MOVETYPE_NONE;
	pObject->pev->solid = SOLID_NOT;
	pObject->pev->velocity = g_vecZero;
	pObject->pev->avelocity = g_vecZero;
	pObject->m_iActorType = ACTOR_INVALID;

	// mark the entity as ragdolled for the client (iuser2 is networked)
	pObject->pev->iuser2 = -677;

	// clear the mover rider link, the engine would drag the corpse entity
	pObject->pev->flags &= ~FL_ONGROUND;
	pObject->pev->groundentity = NULL;

	// death animation events (weapon drops etc) never play for ragdolls
	pObject->OnRagdoll();

	// save restore: drop the bodies onto the saved pose, asleep, no visible settle
	if( pObject->m_fHasRagdollPose )
	{
		const float *pose = pObject->m_flRagdollPose;
		for( int i = 0; i < RAGDOLL_PARTS; i++, pose += 7 )
		{
			if( !rag.bodies[i] )
			{
				continue;
			}
			PxTransform xform( PxVec3( pose[0], pose[1], pose[2] ),
				PxQuat( pose[3], pose[4], pose[5], pose[6] ));
			rag.bodies[i]->setGlobalPose( xform );
			rag.bodies[i]->setLinearVelocity( PxVec3( 0.0f ));
			rag.bodies[i]->setAngularVelocity( PxVec3( 0.0f ));
			rag.bodies[i]->putToSleep();
		}
		rag.asleep = true;
		rag.impactGraceUntil = gpGlobals->time + CVAR_GET_FLOAT( "phys_ragdoll_restoregrace" );
		// consume it: a later kill/re-save of this entity starts fresh
		pObject->m_fHasRagdollPose = false;
	}

	// at the cap, evict the oldest corpse
	if( m_numRagdolls >= MAX_RAGDOLLS )
	{
		int oldest = 0;
		for( int i = 1; i < m_numRagdolls; i++ )
		{
			if( m_ragdolls[i].spawnTime < m_ragdolls[oldest].spawnTime )
			{
				oldest = i;
			}
		}

		edict_t *pOldEdict = INDEXENT( m_ragdolls[oldest].entindex );
		bool validEdict = ( pOldEdict && !pOldEdict->free && pOldEdict->serialnumber == m_ragdolls[oldest].serialnumber );

		// release first, UTIL_Remove ends up in RemoveBody and would release this slot again
		ReleaseRagdoll( oldest, true );

		if( validEdict )
		{
			CBaseEntity *pOldEntity = CBaseEntity::Instance( pOldEdict );
			if( pOldEntity )
			{
				UTIL_Remove( pOldEntity );
			}
		}
	}

	m_ragdolls[m_numRagdolls++] = rag;
	m_flRagdollUpdateTime = 0.0f;

	return rag.bodies[1];
}

/*
=================
RagdollSendBones

rebuilds the full skeleton from the part bodies and sends it as one RagdollBones message; returns the entity-local bone bounds in mins/maxs
=================
*/
void CPhysicPhysX::RagdollSendBones( RagdollDesc &rag, CBaseEntity *pEntity, int msgDest, edict_t *pTarget, Vector &mins, Vector &maxs )
{
	matrix4x4 partWorld[RAGDOLL_PARTS];

	for( int i = 0; i < RAGDOLL_PARTS; i++ )
	{
		PxMat44 poseMat( rag.bodies[i]->getGlobalPose( ));
		partWorld[i] = matrix4x4( const_cast<float *>( poseMat.front( )));
	}

	// build the transform from origin+angles like the client decodes it, the cached
	// entity transform loses its rotation on save restore and never recomputes while asleep
	matrix4x4 localSpace = matrix4x4( pEntity->pev->origin, pEntity->pev->angles, 1.0f ).Invert();
	static matrix4x4 boneWorld[MAXSTUDIOBONES];
	Vector pos;
	Radian ang;

	for( int j = 0; j < rag.numBones; j++ )
	{
		boneWorld[j] = partWorld[rag.bonePart[j]].ConcatTransforms( rag.boneOffset[j] );
	}

	ClearBounds( mins, maxs );

	MESSAGE_BEGIN( msgDest, gmsgRagdollBones, NULL, pTarget );
		WRITE_ENTITY( (short)rag.entindex );
		WRITE_LONG( (int)( gpGlobals->time * 1000.0f ));
		WRITE_COORD( pEntity->pev->origin.x );
		WRITE_COORD( pEntity->pev->origin.y );
		WRITE_COORD( pEntity->pev->origin.z );
		WRITE_BYTE( rag.numBones );

		for( int j = 0; j < rag.numBones; j++ )
		{
			int parent = rag.studioParent[j];
			matrix4x4 entityLocal = localSpace.ConcatTransforms( boneWorld[j] );

			matrix4x4 local;
			if( parent == -1 )
			{
				local = entityLocal;
			}
			else if( rag.boneAdjusted[j] )
			{
				local = rag.boneAdjustLocal[j];	// config 'adjustment': pin to the reset pose
			}
			else { local = boneWorld[parent].Invert().ConcatTransforms( boneWorld[j] ); }

			local.GetStudioTransform( pos, ang );
			AddPointToBounds( entityLocal.GetOrigin(), mins, maxs );
			WRITE_SHORT( pos.x * 128 );
			WRITE_SHORT( pos.y * 128 );
			WRITE_SHORT( pos.z * 128 );
			WRITE_SHORT( ang.x * 512 );
			WRITE_SHORT( ang.y * 512 );
			WRITE_SHORT( ang.z * 512 );
		}
	MESSAGE_END();
}

/*
=================
SendRagdollPose

sends one ragdoll's pose to a client answering ragdoll_request, lets late joiners recover a sleeping network-silent ragdoll
=================
*/
void CPhysicPhysX::SendRagdollPose( CBaseEntity *pPlayer, int entindex )
{
	if( !pPlayer )
	{
		return;
	}

	int slot = FindRagdoll( entindex );
	if( slot == -1 )
	{
		return;
	}

	edict_t *pEdict = INDEXENT( entindex );
	RagdollDesc &rag = m_ragdolls[slot];

	if( !pEdict || pEdict->free || pEdict->serialnumber != rag.serialnumber )
	{
		return;
	}

	CBaseEntity *pEntity = CBaseEntity::Instance( pEdict );
	if( !pEntity )
	{
		return;
	}

	Vector mins, maxs;
	RagdollSendBones( rag, pEntity, MSG_ONE, pPlayer->edict(), mins, maxs );
}

/*
=================
RagdollApplyWaterForces

applies depth-scaled buoyancy and drag to submerged ragdoll parts every simulation step so corpses settle partially submerged instead of bobbing
=================
*/
void CPhysicPhysX::RagdollApplyWaterForces( void )
{
	if( !m_numRagdolls )
	{
		return;
	}

	float buoyancy = CVAR_GET_FLOAT( "phys_ragdoll_buoyancy" );
	if( buoyancy <= 0.0f )
	{
		return;
	}

	float drag = CVAR_GET_FLOAT( "phys_ragdoll_waterdrag" );
	float gravity = g_psv_gravity ? g_psv_gravity->value : 800.0f;

	for( int n = 0; n < m_numRagdolls; n++ )
	{
		RagdollDesc &rag = m_ragdolls[n];

		for( int i = 0; i < RAGDOLL_PARTS; i++ )
		{
			float frac = rag.waterFrac[i];

			if( frac <= 0.0f || !rag.bodies[i] )
			{
				continue;
			}

			PxRigidDynamic *pBody = rag.bodies[i];
			pBody->addForce( PxVec3( 0.0f, 0.0f, gravity * buoyancy * frac ), PxForceMode::eACCELERATION );

			// water drag, also depth-proportional, kills the residual bobbing
			if( drag > 0.0f )
			{
				pBody->addForce( -pBody->getLinearVelocity() * drag * frac, PxForceMode::eACCELERATION );
				pBody->addTorque( -pBody->getAngularVelocity() * drag * frac, PxForceMode::eACCELERATION );
			}
		}
	}
}

/*
=================
UpdateRagdolls

per-frame ragdoll maintenance: spawns queued ragdolls, expires/fades old ones, blends joint limits, samples water, and streams each awake ragdoll's pose out
=================
*/
void CPhysicPhysX::UpdateRagdolls( void )
{
	// spawn queued ragdolls (kills, save restores)
	for( int p = 0; p < m_numPendingRagdolls; p++ )
	{
		const PendingRagdoll &pending = m_pendingRagdolls[p];

		edict_t *pEdict = INDEXENT( pending.entindex );
		if( !pEdict || pEdict->free )
		{
			continue;
		}

		// no deadflag check, monsters flip it on a later think
		CBaseEntity *pEntity = CBaseEntity::Instance( pEdict );
		if( !pEntity )
		{
			continue;
		}

		SpawnRagdoll( pEntity, &pending );
	}
	m_numPendingRagdolls = 0;

	if( !m_numRagdolls )
	{
		return;
	}

	// bone snapshots go out at 25Hz regardless of the server framerate
	if( m_flRagdollUpdateTime > gpGlobals->time )
	{
		return;
	}

	m_flRagdollUpdateTime = gpGlobals->time + 0.04f;

	// optional corpse expiry: 0 keeps ragdolls until the level unloads
	float lifetime = CVAR_GET_FLOAT( "phys_ragdoll_lifetime" );

	for( int n = 0; n < m_numRagdolls; )
	{
		RagdollDesc &rag = m_ragdolls[n];
		edict_t *pEdict = INDEXENT( rag.entindex );

		// tear the ragdoll down when its corpse entity is gone
		if( !pEdict || pEdict->free )
		{
			ReleaseRagdoll( n, true );
			continue;
		}

		CBaseEntity *pEntity = CBaseEntity::Instance( pEdict );
		if( !pEntity )
		{
			ReleaseRagdoll( n, true );
			continue;
		}

		// edict slot recycled for a new entity, drop the stale descriptor
		if( pEdict->serialnumber != rag.serialnumber )
		{
			ReleaseRagdoll( n, true );
			continue;
		}

		// lifetime expired: fade out, then remove entity and ragdoll together
		if( rag.fadeStartTime > 0.0f )
		{
			float fadeTime = Q_max( CVAR_GET_FLOAT( "phys_ragdoll_fadetime" ), 0.001f );
			float frac = ( gpGlobals->time - rag.fadeStartTime ) / fadeTime;

			if( frac >= 1.0f )
			{
				// release first, UTIL_Remove ends up in RemoveBody and would release this slot again
				ReleaseRagdoll( n, true );
				UTIL_Remove( pEntity );
				continue;
			}

			pEntity->pev->renderamt = bound( 0.0f, 255.0f * ( 1.0f - frac ), 255.0f );
		}
		else if( lifetime > 0.0f && gpGlobals->time - rag.spawnTime > lifetime )
		{
			rag.fadeStartTime = gpGlobals->time;
			pEntity->pev->rendermode = kRenderTransTexture;
			pEntity->pev->renderamt = 255.0f;
		}

		// sample how deep each part sits in water, before the sleep skip
		bool splash = ( CVAR_GET_FLOAT( "phys_ragdoll_splash" ) > 0.0f );
		bool checkWater = ( CVAR_GET_FLOAT( "phys_ragdoll_buoyancy" ) > 0.0f ) || splash;
		float splashSpeed = CVAR_GET_FLOAT( "phys_ragdoll_splash_speed" );

		for( int i = 0; i < RAGDOLL_PARTS; i++ )
		{
			rag.waterFrac[i] = 0.0f;

			if( !checkWater || !rag.bodies[i] )
			{
				continue;
			}

			PxBounds3 bounds = rag.bodies[i]->getWorldBounds();
			Vector org( bounds.getCenter().x, bounds.getCenter().y, 0.0f );
			float bottom = bounds.minimum.z;
			float height = Q_max( bounds.getDimensions().z, 1.0f );

			// count sample points up the part's height that sit in water
			const int k_WaterSamples = 8;
			int submerged = 0;
			for( int s = 0; s < k_WaterSamples; s++ )
			{
				Vector probe = org;
				probe.z = bottom + height * ( s + 0.5f ) / (float)k_WaterSamples;
				if( UTIL_PointContents( probe ) == CONTENTS_WATER )
				{
					submerged++;
				}
			}
			rag.waterFrac[i] = (float)submerged / (float)k_WaterSamples;

			// a part that was dry and just dipped in while moving down makes a splash;
			// reuse the same water event the player entry uses (sound + ring + splash + drips)
			if( splash && rag.prevWaterFrac[i] < 0.05f && rag.waterFrac[i] >= 0.125f )
			{
				float vz = rag.bodies[i]->getLinearVelocity().z;
				if( vz < -splashSpeed )
				{
					Vector below( org.x, org.y, bottom );			// under the surface
					Vector above( org.x, org.y, bottom + height + 32.0f );	// clear of it
					int type = ( -vz > splashSpeed * 3.0f ) ? 1 : 0;	// bigger splash for a hard entry
					pEntity->MakeWaterSplash( above, below, type );
				}
			}

			rag.prevWaterFrac[i] = rag.waterFrac[i];
		}

		UpdateRagdollScrapeSound( rag, pEdict );

		// blend the joint limits from the widened spawn ranges back to the authored ones
		if( !rag.limitBlendDone )
		{
			float frac = 1.0f;

			if( rag.limitBlendDuration > 0.0f )
			{
				frac = bound( 0.0f, ( gpGlobals->time - rag.spawnTime ) / rag.limitBlendDuration, 1.0f );
			}

			for( int i = 0; i < RAGDOLL_JOINTS; i++ )
			{
				if( !rag.joints[i] )
				{
					continue;
				}

				PxD6Joint *pJoint = static_cast<PxD6Joint *>( rag.joints[i] );
				float lim[6];

				for( int k = 0; k < 6; k++ )
				{
					lim[k] = rag.jointLimitStart[i][k] + ( rag.jointLimitEnd[i][k] - rag.jointLimitStart[i][k] ) * frac;
				}

				if( rag.limitSpring > 0.0f )
				{
					pJoint->setPyramidSwingLimit( PxJointLimitPyramid( lim[0], lim[1], lim[2], lim[3], PxSpring( rag.limitSpring, rag.limitDamping )));
					pJoint->setTwistLimit( PxJointAngularLimitPair( lim[4], lim[5], PxSpring( rag.limitSpring, rag.limitDamping )));
				}
				else
				{
					pJoint->setPyramidSwingLimit( PxJointLimitPyramid( lim[0], lim[1], lim[2], lim[3] ));
					pJoint->setTwistLimit( PxJointAngularLimitPair( lim[4], lim[5] ));
				}
			}

			if( frac >= 1.0f )
			{
				rag.limitBlendDone = true;
			}
		}

		bool asleep = true;
		for( int i = 0; i < RAGDOLL_PARTS; i++ )
		{
			if( rag.bodies[i] && !rag.bodies[i]->isSleeping( ))
			{
				asleep = false;
				break;
			}
		}

		// a resting ragdoll is network-silent after one final reliable snapshot
		if( asleep && rag.asleep )
		{
			n++;
			continue;
		}

		bool fellAsleep = ( asleep && !rag.asleep );
		rag.asleep = asleep;
		rag.lastSendTime = gpGlobals->time;

		// the entity origin follows the torso so bounds, PVS and lighting stay correct
		PxVec3 torsoPos = rag.bodies[1]->getGlobalPose().p;
		Vector torsoOrigin( torsoPos.x, torsoPos.y, torsoPos.z );
		Vector vecPrevOrigin = pEntity->GetAbsOrigin();
		pEntity->SetLocalOrigin( torsoOrigin );
		pEntity->RelinkEntity( TRUE, &vecPrevOrigin );

		// broadcast unconditionally while awake, PVS routing can land in a solid leaf and drop every message
		Vector mins, maxs;
		RagdollSendBones( rag, pEntity, fellAsleep ? MSG_ALL : MSG_BROADCAST, NULL, mins, maxs );

		// grow the entity bbox to cover the sprawled pose so it stays in the PVS
		ExpandBounds( mins, maxs, 2.0f );
		UTIL_SetSize( pEntity->pev, mins, maxs );
		n++;
	}
}

/*
=================
CreateKinematicBodyFromEntity

creates a kinematic rigid body with a triangle-mesh shape for an entity
=================
*/
void *CPhysicPhysX::CreateKinematicBodyFromEntity(CBaseEntity *pObject)
{
	PxTriangleMesh *pCollision = TriangleMeshFromEntity(pObject);
	if (!pCollision)
	{
		return NULL;
	}

	PxRigidDynamic *pActor = m_pPhysics->createRigidDynamic(PxTransform(PxIdentity));
	if (!pActor)
	{
		ALERT(at_error, "failed to create kinematic from entity %s\n", pObject->GetClassname());
		return NULL;
	}

	if (!UTIL_CanRotate(pObject))
	{
		// entity missed origin-brush
		pActor->setRigidDynamicLockFlags(
			PxRigidDynamicLockFlag::eLOCK_ANGULAR_X |
			PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y |
			PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z);
	}

	float mat[16];
	matrix4x4(pObject->GetAbsOrigin(), pObject->GetAbsAngles(), 1.0f).CopyToArray(mat);

	PxTransform pose = PxTransform(PxMat44(mat));
	pActor->setName(pObject->GetClassname());
	pActor->setGlobalPose(pose);
	pActor->setSolverIterationCounts((PxU32)CVAR_GET_FLOAT("phys_solveriterations"));
	pActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
	// speculative, not swept: see the comment in CreateBodyFromEntity
	pActor->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_SPECULATIVE_CCD, CVAR_GET_FLOAT("phys_ccd") != 0.0f);
	pActor->userData = pObject->edict();

	PxMeshScale scale(pObject->GetScale());
	PxShape *pShape = PxRigidActorExt::createExclusiveShape(*pActor, PxTriangleMeshGeometry(pCollision, scale), *m_pDefaultMaterial);

	// shape holds its own reference now
	pCollision->release();

	if (!pShape)
	{
		ALERT(at_error, "CreateKinematicBodyFromEntity: failed to create shape for %s\n", pObject->GetClassname());
		pActor->release();
		return NULL;
	}

	m_pScene->addActor(*pActor);
	pObject->m_iActorType = ACTOR_KINEMATIC;
	pObject->m_pUserData = pActor;
	return pActor;
}

/*
=================
CreateStaticBodyFromEntity

creates a static rigid body with a triangle-mesh shape for an entity, using the conveyor material when the entity is flagged as one
=================
*/
void *CPhysicPhysX :: CreateStaticBodyFromEntity( CBaseEntity *pObject )
{
	PxTriangleMesh *pCollision = TriangleMeshFromEntity( pObject );
	if( !pCollision ) 
	{
		return NULL;
	}

	float mat[16];
	matrix4x4(pObject->GetAbsOrigin(), pObject->GetAbsAngles(), 1.0f).CopyToArray(mat);

	bool conveyorEntity = FBitSet(pObject->pev->flags, FL_CONVEYOR);
	PxTransform pose = PxTransform(PxMat44(mat));
	PxMeshScale scale(pObject->GetScale());
	PxMaterial *material = conveyorEntity ? m_pConveyorMaterial : m_pDefaultMaterial;
	PxRigidStatic *pActor = m_pPhysics->createRigidStatic(pose);

	if( !pActor )
	{
		ALERT( at_error, "failed to create static from entity %s\n", pObject->GetClassname( ));
		return NULL;
	}

	PxShape *pShape = PxRigidActorExt::createExclusiveShape(*pActor, PxTriangleMeshGeometry(pCollision, scale), *material);

	// shape holds its own reference now
	pCollision->release();

	if( !pShape )
	{
		ALERT( at_error, "CreateStaticBodyFromEntity: failed to create shape for %s\n", pObject->GetClassname( ));
		pActor->release();
		return NULL;
	}

	if (conveyorEntity) 
	{
		CollisionFilterData filterData;
		filterData.SetConveyorFlag(true);
		pShape->setSimulationFilterData(filterData.ToNativeType());
	}

	pActor->setName( pObject->GetClassname( ));
	pActor->userData = pObject->edict();
	m_pScene->addActor(*pActor);

	pObject->m_iActorType = ACTOR_STATIC;
	pObject->m_pUserData = pActor;

	return pActor;
}

/*
=================
CreateTriggerFromEntity

creates a gravity-free trigger box actor for an entity
=================
*/
void *CPhysicPhysX::CreateTriggerFromEntity(CBaseEntity *pEntity)
{
	PxBoxGeometry boxGeometry;
	boxGeometry.halfExtents = pEntity->pev->size / 2.0f;
	Vector centerOrigin = pEntity->pev->mins + pEntity->pev->size / 2.0f;
	PxRigidDynamic *pActor = m_pPhysics->createRigidDynamic(PxTransform(centerOrigin));
	PxShape *pShape = PxRigidActorExt::createExclusiveShape(*pActor, boxGeometry, *m_pDefaultMaterial);

	if (!pActor)
	{
		ALERT( at_error, "failed to create trigger actor from entity %s\n", pEntity->GetClassname());
		return NULL;
	}

	pShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
	pShape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
	pActor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, true);
	pActor->setName(pEntity->GetClassname());
	pActor->userData = pEntity->edict();
	m_pScene->addActor(*pActor);

	pEntity->m_iActorType = ACTOR_TRIGGER;
	pEntity->m_pUserData = pActor;

	return pActor;
}

/*
=================
UpdateEntityTransform

copies an awake dynamic actor's pose and velocities back onto its entity
=================
*/
bool CPhysicPhysX::UpdateEntityTransform( CBaseEntity *pEntity )
{
	PxActor *pActor = ActorFromEntity(pEntity);
	if (!pActor)
	{
		return false;
	}

	PxRigidDynamic *pDynamicActor = pActor->is<PxRigidDynamic>();
	if (!pDynamicActor || pDynamicActor->isSleeping())
	{
		return false;
	}

	PxTransform pose = pDynamicActor->getGlobalPose();
	PxMat44 poseMat(pose);
	matrix4x4 m( const_cast<float *>( poseMat.front() ));

	Vector angles = m.GetAngles();
	Vector origin = m.GetOrigin();

	// store actor velocities too
	pEntity->SetLocalVelocity( pDynamicActor->getLinearVelocity() );
	pEntity->SetLocalAvelocity( pDynamicActor->getAngularVelocity() );
	Vector vecPrevOrigin = pEntity->GetAbsOrigin();

	pEntity->SetLocalAngles( angles );
	pEntity->SetLocalOrigin( origin );
	pEntity->RelinkEntity( TRUE, &vecPrevOrigin );

	return true;
}

/*
=================
UpdateActorPos

pushes an entity's origin, angles and velocities onto its actor
=================
*/
bool CPhysicPhysX :: UpdateActorPos( CBaseEntity *pEntity )
{
	PxActor *pActor = ActorFromEntity(pEntity);
	if (!pActor)
	{
		return false;
	}

	PxRigidActor *pRigidActor = pActor->is<PxRigidActor>();
	if (!pRigidActor)
	{
		return false;
	}

	float mat[16];
	matrix4x4 m( pEntity->GetAbsOrigin(), pEntity->GetAbsAngles(), 1.0f );
	m.CopyToArray( mat );

	PxTransform pose = PxTransform(PxMat44(mat));
	pRigidActor->setGlobalPose( pose );

	PxRigidDynamic *pRigidDynamic = pActor->is<PxRigidDynamic>();
	if (!(pRigidDynamic->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC))
	{
		pRigidDynamic->setLinearVelocity( pEntity->GetLocalVelocity() );
		pRigidDynamic->setAngularVelocity( pEntity->GetLocalAvelocity() );
	}

	return true;
}

/*
=================
IsBodySleeping

returns true if the entity's dynamic body is asleep
=================
*/
bool CPhysicPhysX :: IsBodySleeping( CBaseEntity *pEntity )
{
	PxActor *pActor = ActorFromEntity(pEntity);
	if (!pActor)
	{
		return false;
	}

	PxRigidDynamic *pDynamicActor = pActor->is<PxRigidDynamic>();
	if (!pDynamicActor)
	{
		return false;
	}

	return pDynamicActor->isSleeping();
}

/*
=================
UpdateEntityAABB

recomputes an entity's bounding box from its actor's world bounds
=================
*/
void CPhysicPhysX :: UpdateEntityAABB( CBaseEntity *pEntity )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if (!pActor)
	{
		return;
	}

	PxRigidActor *pRigidActor = pActor->is<PxRigidActor>();
	if (!pRigidActor || pRigidActor->getNbShapes() <= 0)
	{
		return;
	}

	PxBounds3 actorBounds = pRigidActor->getWorldBounds();
	ClearBounds( pEntity->pev->absmin, pEntity->pev->absmax );
	AddPointToBounds( actorBounds.minimum, pEntity->pev->absmin, pEntity->pev->absmax );
	AddPointToBounds( actorBounds.maximum, pEntity->pev->absmin, pEntity->pev->absmax );

	pEntity->pev->mins = pEntity->pev->absmin - pEntity->pev->origin;
	pEntity->pev->maxs = pEntity->pev->absmax - pEntity->pev->origin;
	pEntity->pev->size = pEntity->pev->maxs - pEntity->pev->mins;
}

/*
===============

PHYSICS SAVE\RESTORE SYSTEM

===============
*/

// list of variables that needs to be saved	how it will be saved

/*
// actor description
NxMat34		globalPose;		pev->origin, pev->angles 
const NxBodyDesc*	body;			not needs
NxReal		density;			constant (???)
NxU32		flags;			m_iActorFlags;
NxActorGroup	group;			pev->groupinfo (???).
NxU32		contactReportFlags; 	not needs (already set by scene rules)
NxU16		forceFieldMaterial;		not needs (not used)
void*		userData;			not needs (automatically restored)
const char*	name;			not needs (automatically restored)
NxCompartment	*compartment;		not needs (not used)
NxActorDescType	type;			not needs (automatically restored) 
// body description
NxMat34		massLocalPose;		not needs (probably automatically recomputed on apply shape)
NxVec3		massSpaceInertia;		not needs (probably automatically recomputed on apply shape)
NxReal		mass;			not needs (probably automatically recomputed on apply shape)
NxVec3		linearVelocity;		pev->velocity
NxVec3		angularVelocity;		pev->avelocity
NxReal		wakeUpCounter;		not needs (use default)
NxReal		linearDamping;		not needs (use default)
NxReal		angularDamping;		not needs (use default)
NxReal		maxAngularVelocity; 	not needs (already set by scene rules)
NxReal		CCDMotionThreshold;		not needs (use default)
NxU32		flags;			m_iBodyFlags;
NxReal		sleepLinearVelocity; 	not needs (already set by scene rules)
NxReal		sleepAngularVelocity; 	not needs (already set by scene rules)
NxU32		solverIterationCount;	not needs (use default)
NxReal		sleepEnergyThreshold;	not needs (use default)
NxReal		sleepDamping;		not needs (use default)
NxReal		contactReportThreshold;	not needs (use default)
*/

/*
===============
SaveBody

collect all the info from generic actor
===============
*/
void CPhysicPhysX :: SaveBody( CBaseEntity *pEntity )
{
	PxActor *pActor = ActorFromEntity(pEntity);
	if (!pActor)
	{
		ALERT(at_warning, "SaveBody: physic entity %i missed actor!\n", pEntity->m_iActorType);
		return;
	}

	PxRigidActor *pRigidActor = pActor->is<PxRigidActor>();
	if (!pRigidActor)
	{
		ALERT(at_warning, "SaveBody: physic entity %i missed rigid actor!\n", pEntity->m_iActorType);
		return;
	}

	PxShape *shape;
	PxFilterData filterData;
	PxRigidDynamic *pRigidBody = pRigidActor->is<PxRigidDynamic>();

	if (pRigidActor->getShapes(&shape, 1)) {
		filterData = shape->getSimulationFilterData();
	}

	// filter data get and stored only for one shape, but it can be more than one assumed that all attached shapes have same filter data
	pEntity->m_iFilterData[0] = filterData.word0;
	pEntity->m_iFilterData[1] = filterData.word1;
	pEntity->m_iFilterData[2] = filterData.word2;
	pEntity->m_iFilterData[3] = filterData.word3;

	pEntity->m_iActorFlags = static_cast<uint8_t>( pRigidActor->getActorFlags() );
	if (pRigidBody) {
		pEntity->m_iBodyFlags = static_cast<uint8_t>( pRigidBody->getRigidBodyFlags() );
		pEntity->m_flBodyMass = pRigidBody->getMass();
		pEntity->m_fFreezed = pRigidBody->isSleeping();
	}
	else {
		pEntity->m_iBodyFlags = 0;
		pEntity->m_flBodyMass = 0.0f;
		pEntity->m_fFreezed = false;
	}

	if (pEntity->m_iActorType == ACTOR_DYNAMIC)
	{
		// update movement variables
		UpdateEntityTransform(pEntity);
	}
}

/*
===============
SaveRagdoll

captures every part's world pose so the restore drops the ragdoll settled
in place, clears the flag when the entity has no live ragdoll
===============
*/
void CPhysicPhysX :: SaveRagdoll( CBaseEntity *pObject )
{
	static_assert( RAGDOLL_PARTS * 7 == RAGDOLL_SAVE_POSE_FLOATS,
		"m_flRagdollPose size (cbase.h) must match RAGDOLL_PARTS * 7" );

	if (!pObject)
	{
		return;
	}

	int idx = FindRagdoll( pObject->entindex() );
	if (idx < 0)
	{
		pObject->m_fHasRagdollPose = false;
		return;
	}

	RagdollDesc &rag = m_ragdolls[idx];
	float *pose = pObject->m_flRagdollPose;
	for (int i = 0; i < RAGDOLL_PARTS; i++, pose += 7)
	{
		if (!rag.bodies[i])
		{
			// no body for this part; store identity (restore skips it the same way)
			pose[0] = pose[1] = pose[2] = 0.0f;
			pose[3] = pose[4] = pose[5] = 0.0f;
			pose[6] = 1.0f;
			continue;
		}

		PxTransform xform = rag.bodies[i]->getGlobalPose();
		pose[0] = xform.p.x; pose[1] = xform.p.y; pose[2] = xform.p.z;
		pose[3] = xform.q.x; pose[4] = xform.q.y; pose[5] = xform.q.z; pose[6] = xform.q.w;
	}
	pObject->m_fHasRagdollPose = true;
}

/*
===============
RestoreBody

re-create shape, apply physic params
===============
*/						
void *CPhysicPhysX :: RestoreBody( CBaseEntity *pEntity )
{
	// physics not initialized?
	if (!m_pScene)
	{
		return NULL;
	}

	PxShape *pShape;
	PxRigidActor *pActor;
	PxTransform pose;
	Vector angles = pEntity->GetAbsAngles();
	PxMeshScale scale(pEntity->GetScale());

	if (pEntity->m_iActorType == ACTOR_CHARACTER) {
		angles = g_vecZero;	// no angles for NPC and client
	}

	float mat[16];
	matrix4x4 m(pEntity->GetAbsOrigin(), angles, 1.0f);
	m.CopyToArray(mat);
	pose = PxTransform(PxMat44(mat));

	if (pEntity->m_iActorType == ACTOR_STATIC) {
		pActor = m_pPhysics->createRigidStatic(pose);
	}
	else {
		pActor = m_pPhysics->createRigidDynamic(pose);
	}

	if (!pActor)
	{
		ALERT(at_error, "RestoreBody: unbale to create actor with type (%i)\n", pEntity->m_iActorType);
		return NULL;
	}

	PxRigidDynamic *pRigidBody = pActor->is<PxRigidDynamic>();
	if (pEntity->m_iActorType == ACTOR_KINEMATIC) 
	{
		// set kinematic flag before shape creating, this is required by design
		pRigidBody->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
	}

	switch (pEntity->m_iActorType)
	{
		case ACTOR_DYNAMIC:
		{
			PxConvexMesh *convexMesh = ConvexMeshFromEntity(pEntity);
			if (!convexMesh) {
				ALERT(at_error, "RestoreBody: failed to create convex mesh\n");
				return NULL;
			}

			pShape = PxRigidActorExt::createExclusiveShape(*pActor, PxConvexMeshGeometry(convexMesh, scale), *m_pDefaultMaterial);
			convexMesh->release(); // shape holds its own reference now
			break;
		}
		case ACTOR_CHARACTER:
		{
			PxBoxGeometry box;
			box.halfExtents = pEntity->pev->size * CVAR_GET_FLOAT("phys_character_padding");
			pShape = PxRigidActorExt::createExclusiveShape(*pActor, box, *m_pDefaultMaterial);
			break;
		}
		case ACTOR_KINEMATIC:
		case ACTOR_STATIC:
		{
			PxTriangleMesh *triangleMesh = TriangleMeshFromEntity(pEntity);
			if (!triangleMesh) {
				ALERT(at_error, "RestoreBody: failed to create triangle mesh\n");
				return NULL;
			}

			PxMaterial *pMaterial = m_pDefaultMaterial;
			if (pEntity->pev->flags & FL_CONVEYOR) {
				pMaterial = m_pConveyorMaterial;
			}
			pShape = PxRigidActorExt::createExclusiveShape(*pActor, PxTriangleMeshGeometry(triangleMesh, scale), *pMaterial);
			triangleMesh->release(); // shape holds its own reference now
			break;
		}
		default:
		{
			ALERT(at_error, "RestoreBody: invalid actor type %i\n", pEntity->m_iActorType);
			return NULL;
		}
	}

	if (!pShape) {
		ALERT(at_error, "RestoreBody: failed to create shape\n");
		return NULL;
	}

	PxFilterData filterData;
	filterData.word0 = pEntity->m_iFilterData[0];
	filterData.word1 = pEntity->m_iFilterData[1];
	filterData.word2 = pEntity->m_iFilterData[2];
	filterData.word3 = pEntity->m_iFilterData[3];

	// old saves carry no filter data, restamp the character bit
	if (pEntity->m_iActorType == ACTOR_CHARACTER)
	{
		filterData.word0 |= k_FilterCharacter;
	}

	pShape->setSimulationFilterData(filterData);

	// fill in actor description
	if (pEntity->m_iActorType != ACTOR_STATIC)
	{
		pRigidBody->setRigidBodyFlags(static_cast<PxRigidBodyFlags>(pEntity->m_iBodyFlags));
		pRigidBody->setMass(pEntity->m_flBodyMass);
		pRigidBody->setSolverIterationCounts((PxU32)CVAR_GET_FLOAT("phys_solveriterations"));

		// old saves can carry the swept eENABLE_CCD bit, reapply CCD per the current cvar and never let swept CCD survive a restore
		if (pEntity->m_iActorType == ACTOR_DYNAMIC || pEntity->m_iActorType == ACTOR_KINEMATIC)
		{
			pRigidBody->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, false);
			pRigidBody->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_SPECULATIVE_CCD, CVAR_GET_FLOAT("phys_ccd") != 0.0f);
		}

		if (pEntity->m_iActorType != ACTOR_KINEMATIC)
		{
			pRigidBody->setLinearVelocity(pEntity->GetAbsVelocity());
			pRigidBody->setAngularVelocity(pEntity->GetAbsAvelocity());
		}

		float density = pEntity->GetDensity();
		if (density <= 0.0f)
		{
			density = MetricDensityToEngine(CVAR_GET_FLOAT("phys_density_default"));
		}

		PxRigidBodyExt::updateMassAndInertia(*pRigidBody, density);
		if (pEntity->m_fFreezed && pEntity->m_iActorType == ACTOR_DYNAMIC) {
			pRigidBody->putToSleep();
		}
	}

	pActor->userData = pEntity->edict();
	pActor->setActorFlags(static_cast<PxActorFlags>(pEntity->m_iActorFlags));
	pActor->setName(pEntity->GetClassname());
	pEntity->m_pUserData = pActor;
	m_pScene->addActor(*pActor);
	return pActor;
}

/*
=================
SetAngles

sets a dynamic actor's orientation from Euler angles
=================
*/
void CPhysicPhysX :: SetAngles( CBaseEntity *pEntity, const Vector &angles )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor ) 
	{
		return;
	}

	if (TracingStateChanges(pActor)) 
	{
		ALERT(at_console, "PhysX: SetAngles( entity = %x, angles = (%.2f, %.2f, %.2f) )\n", 
			pEntity, angles.x, angles.y, angles.z);
	}

	matrix3x3 m(angles);
	PxRigidDynamic *pRigidDynamic = pActor->is<PxRigidDynamic>();
	PxTransform transform = pRigidDynamic->getGlobalPose();
	transform.q = PxQuat(PxMat33( m ));
	pRigidDynamic->setGlobalPose(transform);
}

/*
=================
SetOrigin

sets a dynamic actor's position
=================
*/
void CPhysicPhysX :: SetOrigin( CBaseEntity *pEntity, const Vector &origin )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor ) 
	{
		return;
	}

	if (TracingStateChanges(pActor)) 
	{
		ALERT(at_console, "PhysX: SetOrigin( entity = %x, origin = (%.2f, %.2f, %.2f) )\n", 
			pEntity, origin.x, origin.y, origin.z);
	}

	PxRigidDynamic *pRigidDynamic = pActor->is<PxRigidDynamic>();
	PxTransform transform = pRigidDynamic->getGlobalPose();
	transform.p = origin;
	pRigidDynamic->setGlobalPose(transform);
}

/*
=================
SetVelocity

sets a dynamic actor's linear velocity
=================
*/
void CPhysicPhysX :: SetVelocity( CBaseEntity *pEntity, const Vector &velocity )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor ) 
	{
		return;
	}

	if (TracingStateChanges(pActor)) 
	{
		ALERT(at_console, "PhysX: SetVelocity( entity = %x, velocity = (%.2f, %.2f, %.2f) )\n", 
			pEntity, velocity.x, velocity.y, velocity.z);
	}

	PxRigidDynamic *pRigidDynamic = pActor->is<PxRigidDynamic>();
	pRigidDynamic->setLinearVelocity( velocity );
}

/*
=================
SetAvelocity

sets a dynamic actor's angular velocity
=================
*/
void CPhysicPhysX :: SetAvelocity( CBaseEntity *pEntity, const Vector &velocity )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor ) 
	{
		return;
	}

	if (TracingStateChanges(pActor)) 
	{
		ALERT(at_console, "PhysX: SetAvelocity( entity = %x, velocity = (%.2f, %.2f, %.2f) )\n", 
			pEntity, velocity.x, velocity.y, velocity.z);
	}

	PxRigidDynamic *pRigidDynamic = pActor->is<PxRigidDynamic>();
	pRigidDynamic->setAngularVelocity( velocity );
}

/*
=================
MoveObject

sets a kinematic actor's target position
=================
*/
void CPhysicPhysX :: MoveObject( CBaseEntity *pEntity, const Vector &finalPos )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor ) 
	{
		return;
	}

	if (TracingStateChanges(pActor)) 
	{
		ALERT(at_console, "PhysX: MoveObject( entity = %x, finalPos = (%.2f, %.2f, %.2f) )\n", 
			pEntity, finalPos.x, finalPos.y, finalPos.z);
	}

	PxRigidDynamic *pRigidDynamic = pActor->is<PxRigidDynamic>();
	PxTransform pose = pRigidDynamic->getGlobalPose();
	pose.p = finalPos;
	pRigidDynamic->setKinematicTarget(pose);
}

/*
=================
RotateObject

sets a kinematic actor's target orientation
=================
*/
void CPhysicPhysX :: RotateObject( CBaseEntity *pEntity, const Vector &finalAngle )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor ) 
	{
		return;
	}

	if (TracingStateChanges(pActor)) 
	{
		ALERT(at_console, "PhysX: RotateObject( entity = %x, finalAngle = (%.2f, %.2f, %.2f) )\n", 
			pEntity, finalAngle.x, finalAngle.y, finalAngle.z);
	}

	PxRigidDynamic *pRigidDynamic = pActor->is<PxRigidDynamic>();
	PxTransform pose = pRigidDynamic->getGlobalPose();
	matrix3x3 m(finalAngle);
	pose.q = PxQuat(PxMat33(m));
	pRigidDynamic->setKinematicTarget(pose);
}

/*
=================
SetLinearMomentum

applies the impulse needed to reach a target linear momentum
=================
*/
void CPhysicPhysX :: SetLinearMomentum( CBaseEntity *pEntity, const Vector &momentum )
{
	PxActor *pActor = ActorFromEntity(pEntity);
	if (!pActor)
	{
		return;
	}

	PxRigidBody *pRigidBody = pActor->is<PxRigidBody>();
	if (!pRigidBody)
	{
		return;
	}

	PxVec3 targetMomentum( momentum );
	PxVec3 currentMomentum = pRigidBody->getLinearVelocity() * pRigidBody->getMass();
	pRigidBody->addForce( targetMomentum - currentMomentum, PxForceMode::eIMPULSE );
}

/*
=================
AddImpulse

applies an impulse at a world position, scaled by a factor
=================
*/
void CPhysicPhysX :: AddImpulse( CBaseEntity *pEntity, const Vector &impulse, const Vector &position, float factor )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if (!pActor) 
	{
		return;
	}

	PxRigidBody *pRigidBody = pActor->is<PxRigidBody>();
	if (!pRigidBody)
	{
		return;
	}

	PxRigidBodyExt::addForceAtPos(*pRigidBody, PxVec3(impulse * factor), position, PxForceMode::eIMPULSE);
}

/*
=================
AddForce

applies a force to a body in the given force mode
=================
*/
void CPhysicPhysX :: AddForce( CBaseEntity *pEntity, const Vector &force, ForceMode mode )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor ) 
	{
		return;
	}

	PxRigidBody *pRigidBody = pActor->is<PxRigidBody>();
	if( !pRigidBody ) 
	{
		return;
	}

	switch( mode )
	{
		case ForceMode::Impulse: pRigidBody->addForce( force, PxForceMode::eIMPULSE ); break;
		case ForceMode::VelocityChange: pRigidBody->addForce( force, PxForceMode::eVELOCITY_CHANGE ); break;
		case ForceMode::Acceleration: pRigidBody->addForce( force, PxForceMode::eACCELERATION ); break;
		default: pRigidBody->addForce( force, PxForceMode::eFORCE ); break;
	}
}

/*
=================
AddTorque

applies a torque to a body in the given force mode
=================
*/
void CPhysicPhysX :: AddTorque( CBaseEntity *pEntity, const Vector &torque, ForceMode mode )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor ) 
	{
		return;
	}

	PxRigidBody *pRigidBody = pActor->is<PxRigidBody>();
	if( !pRigidBody ) 
	{
		return;
	}

	switch( mode )
	{
		case ForceMode::Impulse: pRigidBody->addTorque( torque, PxForceMode::eIMPULSE ); break;
		case ForceMode::VelocityChange: pRigidBody->addTorque( torque, PxForceMode::eVELOCITY_CHANGE ); break;
		case ForceMode::Acceleration: pRigidBody->addTorque( torque, PxForceMode::eACCELERATION ); break;
		default: pRigidBody->addTorque( torque, PxForceMode::eFORCE ); break;
	}
}

/*
=================
SetHoldableTarget

sets the held-item controller's target pose for an entity
=================
*/
void CPhysicPhysX :: SetHoldableTarget( CBaseEntity *pEntity, const Vector &targetOrigin, const Vector4D &targetQuat )
{
	m_holdableController->SetTarget( pEntity, targetOrigin, targetQuat );
}

/*
=================
ClearHoldableTarget

clears the held-item controller's target for an entity
=================
*/
void CPhysicPhysX :: ClearHoldableTarget( CBaseEntity *pEntity )
{
	m_holdableController->ClearTarget( pEntity );
}

/*
=================
GetTransform

returns a body's world transform as a matrix
=================
*/
void CPhysicPhysX :: GetTransform( CBaseEntity *pEntity, matrix4x4 &out )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor )
	{
		return;
	}

	PxRigidBody *pRigidBody = pActor->is<PxRigidBody>();
	if( !pRigidBody )
	{
		return;
	}

	PxTransform pose = pRigidBody->getGlobalPose();
	PxMat44 poseMat(pose);
	matrix4x4 m( const_cast<float *>( poseMat.front() ));
	out = m;
}

/*
=================
FLoadTree

loads the cached world collision mesh for a map, reloading only when the map changes
=================
*/
int CPhysicPhysX :: FLoadTree( char *szMapName )
{
	if( !szMapName || !*szMapName || !m_pPhysics )
	{
		return 0;
	}

	if( m_fLoaded )
	{
		if( !Q_stricmp( szMapName, m_szMapName ))
		{
			// stay on same map - no reload
			return 1;
		}

		if (m_pSceneActor)
		{
			// the world actor can be the "other" side of queued ragdoll impacts
			m_eventHandler->purgeActor(m_pSceneActor);
			m_pSceneActor->release();
		}
		m_pSceneActor = NULL;

		if (m_pSceneMesh)
		{
			m_pSceneMesh->release();
		}
		m_pSceneMesh = NULL;

		m_fLoaded = FALSE; // trying to load new collision tree
	}

	// save off mapname
	strcpy(m_szMapName, szMapName);

	char szHullFilename[MAX_PATH];

	// versioned cache name ("_hull3"), older caches get rebuilt
	Q_snprintf( szHullFilename, sizeof( szHullFilename ), "cache/maps/%s_hull3.bin", szMapName );
	UserStream cacheFileStream(szHullFilename, true);
	m_pSceneMesh = m_pPhysics->createTriangleMesh(cacheFileStream);
	m_fWorldChanged = FALSE;

	return (m_pSceneMesh != NULL) ? TRUE : FALSE;
}

/*
=================
CheckBINFile

returns true if the cached world collision file is up to date with the map's BSP
=================
*/
int CPhysicPhysX :: CheckBINFile( char *szMapName )
{
	if( !szMapName || !*szMapName || !m_pPhysics )
	{
		return FALSE;
	}

	char szBspFilename[MAX_PATH];
	char szHullFilename[MAX_PATH];

	Q_snprintf( szBspFilename, sizeof( szBspFilename ), "maps/%s.bsp", szMapName );
	Q_snprintf( szHullFilename, sizeof( szHullFilename ), "cache/maps/%s_hull3.bin", szMapName ); // versioned cache ("_hull3"), see FLoadTree

	BOOL retValue = TRUE;

	int iCompare;
	if ( COMPARE_FILE_TIME( szBspFilename, szHullFilename, &iCompare ))
	{
		if( iCompare > 0 )
		{
			// BSP file is newer.
			ALERT ( at_console, ".BIN file will be updated\n\n" );
			retValue = FALSE;
		}
	}
	else
	{
		retValue = FALSE;
	}

	return retValue;
}

/*
=================
CacheNameForModel

builds a cache file path for a model from its name, state hash and suffix
=================
*/
void CPhysicPhysX::CacheNameForModel( model_t *model, char *hullfile, size_t size, uint32_t hash, const char *suffix )
{
	char szModelPath[MAX_PATH];

	hullfile[0] = '\0';
	if (!model->name[0])
	{
		return;
	}

	Q_strncpy(szModelPath, model->name, sizeof(szModelPath));

	char *pszName = szModelPath;
	char *pszSlash = strpbrk(pszName, "/\\");
	if (pszSlash)
	{
		pszName = pszSlash + 1;
	}

	COM_StripExtension(pszName);
	Q_snprintf(hullfile, size, "cache/%s_%08x%s", pszName, hash, suffix);
}

/*
=================
GetHashForModelState

returns a CRC of a model's body, skin and content that keys its collision cache
=================
*/
uint32_t CPhysicPhysX::GetHashForModelState( model_t *model, int32_t body, int32_t skin )
{
	CRC32_t hash;
	CRC32_INIT(&hash);
	CRC32_PROCESS_BUFFER(&hash, &body, sizeof(body));
	CRC32_PROCESS_BUFFER(&hash, &skin, sizeof(skin));
	CRC32_PROCESS_BUFFER(&hash, &model->modelCRC, sizeof(model->modelCRC));
	return CRC32_FINAL(hash);
}

/*
=================
CheckFileTimes

returns true if the cached hull/mesh file is up to date relative to its source model
=================
*/
int CPhysicPhysX :: CheckFileTimes( const char *szFile1, const char *szFile2 )
{
	if( !szFile1 || !*szFile1 || !szFile2 || !*szFile2 )
	{
		return FALSE;
	}

	BOOL retValue = TRUE;

	int iCompare;
	if ( COMPARE_FILE_TIME( (char *)szFile1, (char *)szFile2, &iCompare ))
	{
		if( iCompare > 0 )
		{
			// MDL file is newer.
			retValue = FALSE;
		}
	}
	else
	{
		retValue = FALSE;
	}

	return retValue;
}

/*
=================
DebugEnabled

returns whether the -physxdebug command-line flag was passed
=================
*/
bool CPhysicPhysX::DebugEnabled() const
{
	bool enabled = g_engfuncs.CheckParm((char *)"-physxdebug", nullptr) != 0;
	return enabled;
}

/*
=================
TracingStateChanges

returns whether state-change logging is enabled for a physbox actor
=================
*/
bool CPhysicPhysX::TracingStateChanges(PxActor *actor) const
{
	if (DebugEnabled() && m_traceStateChanges) {
		return strcmp(actor->getName(), "func_physbox") == 0 || strcmp(actor->getName(), "env_physbox") == 0;
	}
	return false;
}

/*
=================
HandleEvents

drains the touch, ragdoll-impact and water-contact queues filled by the simulation callbacks last step and dispatches each to game code
=================
*/
void CPhysicPhysX::HandleEvents()
{
	// consume before dispatching: the dispatch can release actors and purge the rest of the queue
	auto &touchEventsQueue = m_eventHandler->getTouchEventsQueue();
	while (!touchEventsQueue.empty())
	{
		auto touchEvent = touchEventsQueue.front();
		touchEventsQueue.erase(touchEventsQueue.begin());

		edict_t *e1 = reinterpret_cast<edict_t*>(touchEvent.first->userData);
		edict_t *e2 = reinterpret_cast<edict_t*>(touchEvent.second->userData);
		DispatchTouch(e1, e2);
	}

	// ragdoll parts that struck something this step -> owning entity
	auto &impactEvents = m_eventHandler->getImpactEvents();
	while (!impactEvents.empty())
	{
		auto ev = impactEvents.front();
		impactEvents.erase(impactEvents.begin());

		edict_t *entity = reinterpret_cast<edict_t*>(ev.ragdollActor->userData);

		// restored ragdolls get a grace window, resting contacts aren't real hits
		int idx = FindRagdoll( ENTINDEX( entity ));
		if (idx >= 0 && gpGlobals->time < m_ragdolls[idx].impactGraceUntil)
		{
			continue;
		}

		edict_t *other = ev.otherActor ? reinterpret_cast<edict_t*>(ev.otherActor->userData) : nullptr;
		Vector position( ev.position.x, ev.position.y, ev.position.z );
		Vector normal( ev.normal.x, ev.normal.y, ev.normal.z );
		DispatchRagdollImpact( entity, other, position, normal, ev.force, ev.part );
	}

	auto &scrapeEvents = m_eventHandler->getScrapeEvents();
	while (!scrapeEvents.empty())
	{
		auto ev = scrapeEvents.front();
		scrapeEvents.erase(scrapeEvents.begin());

		edict_t *entity = reinterpret_cast<edict_t*>(ev.ragdollActor->userData);
		if (!entity)
		{
			continue;
		}

		int idx = FindRagdoll( ENTINDEX( entity ));
		if (idx < 0 || gpGlobals->time < m_ragdolls[idx].impactGraceUntil)
		{
			continue;
		}

		m_ragdolls[idx].lastScrapeTime = gpGlobals->time;
		m_ragdolls[idx].scrapeSpeed = Q_max( m_ragdolls[idx].scrapeSpeed, ev.speed );
	}

	auto &waterContactPairs = m_eventHandler->getWaterContactPairs();
	for (const auto &pair : waterContactPairs)
	{
		edict_t *entity = reinterpret_cast<edict_t*>(pair.objectActor->userData);
		physx::PxRigidDynamic *dynamicActor = pair.objectActor->is<PxRigidDynamic>();
		physx::PxRigidDynamic *triggerActor = pair.waterTriggerActor->is<PxRigidDynamic>();

		if (!entity || !dynamicActor || !triggerActor)
		{
			continue;
		}

		if (!FBitSet(dynamicActor->getRigidBodyFlags(), PxRigidBodyFlag::eKINEMATIC))
		{
			PxBounds3 objectBounds = dynamicActor->getWorldBounds();
			PxBounds3 waterBounds = triggerActor->getWorldBounds();
			PxBounds3 intersectionBounds = GetIntersectionBounds(objectBounds, waterBounds);
			if (!intersectionBounds.isEmpty())
			{
				float waterDensity = MetricDensityToEngine(CVAR_GET_FLOAT("phys_density_water"));
				PxVec3 intersectionSizes = intersectionBounds.getDimensions();
				float displacedVolume = intersectionSizes.x * intersectionSizes.y * intersectionSizes.z;
				PxVec3 byoyancyForce = displacedVolume * waterDensity * -PxVec3(0.f, 0.f, -g_psv_gravity->value);
				dynamicActor->addForce(byoyancyForce);

				// in practice this velocity should be relative to water shape, since water can be moved
				PxVec3 velocityDir = dynamicActor->getLinearVelocity();// -triggerActor->getLinearVelocity();
				float velocityMag = velocityDir.normalizeSafe();
				PxVec3 linearDragForce = CVAR_GET_FLOAT("phys_water_lineardrag") * waterDensity * velocityMag * velocityMag * -velocityDir;
				dynamicActor->addForce(linearDragForce);

				PxVec3 angularDragForce = CVAR_GET_FLOAT("phys_water_angulardrag") * displacedVolume * -dynamicActor->getAngularVelocity();
				dynamicActor->addTorque(angularDragForce);
			}
		}
	}
}

/*
=================
ConvertEdgeToIndex

resolves a surface edge to its vertex index in a brush model
=================
*/
int CPhysicPhysX :: ConvertEdgeToIndex( model_t *model, int edge )
{
	int e = model->surfedges[edge];
	return (e > 0) ? model->edges[e].v[0] : model->edges[-e].v[1];
}

//-----------------------------------------------------------------------------
// reconstruct the solid world collision mesh from the BSP node tree (hull 0) instead of render faces, which miss invisible but solid brushes (null textures)
//-----------------------------------------------------------------------------
namespace
{
	const float HULL_BOGUS_RANGE = 131072.0f;	// larger than any map extent

	typedef std::vector<PxVec3> hullWinding_t;

	/*
	=================
	HullContentsSolid

	returns whether a BSP contents value is solid clip (solid or sky)
	=================
	*/
	inline bool HullContentsSolid( int contents )
	{
		// sky brushes are solid clip too; water/slime/lava/ladders/etc. are passable
		return ( contents == CONTENTS_SOLID || contents == CONTENTS_SKY );
	}

	/*
	=================
	HullNodeIsLeaf

	returns whether a hull node is a leaf
	=================
	*/
	inline bool HullNodeIsLeaf( mnode_t *node )
	{
		return ( node->contents < 0 );
	}

	/*
	=================
	HullPointContents

	descends the hull-0 node tree to classify a point's contents
	=================
	*/
	int HullPointContents( mnode_t *node, const PxVec3 &p )
	{
		while( node && !HullNodeIsLeaf( node ))
		{
			const mplane_t *pl = node->plane;
			float d = PxVec3( pl->normal[0], pl->normal[1], pl->normal[2] ).dot( p ) - pl->dist;
			node = node->children[( d >= 0.0f ) ? 0 : 1];
		}
		return node ? node->contents : CONTENTS_EMPTY;
	}

	/*
	=================
	HullWindingNormal

	computes a polygon's normal using Newell's method
	=================
	*/
	PxVec3 HullWindingNormal( const hullWinding_t &w )
	{
		PxVec3 n( 0.0f, 0.0f, 0.0f );
		int cnt = (int)w.size();
		for( int i = 0; i < cnt; i++ )
		{
			const PxVec3 &a = w[i];
			const PxVec3 &b = w[( i + 1 ) % cnt];
			n.x += ( a.y - b.y ) * ( a.z + b.z );
			n.y += ( a.z - b.z ) * ( a.x + b.x );
			n.z += ( a.x - b.x ) * ( a.y + b.y );
		}
		n.normalize();
		return n;
	}

	// ------------------------------------------------------------------------
	// port of the engine's hull->polygon extractor (mod_dbghulls.c), the exact code r_showhull uses
	// ------------------------------------------------------------------------
	#define PW_MAX_DEPTH	256	// clipnode stack depth, matches the engine

	struct pwinding_t
	{
		pwinding_t		*prev, *next;	// intrusive chain links
		pwinding_t		*pair;
		const mplane_t		*plane;
		std::vector<PxVec3>	p;
	};

	/*
	=================
	PW_ListInit

	initializes an empty winding list
	=================
	*/
	inline void PW_ListInit( pwinding_t *h ) { h->next = h->prev = h; }
	/*
	=================
	PW_ListAdd

	links a winding into a list after a node
	=================
	*/
	inline void PW_ListAdd( pwinding_t *n, pwinding_t *after )
	{
		n->next = after->next;
		n->prev = after;
		after->next->prev = n;
		after->next = n;
	}
	/*
	=================
	PW_ListDel

	unlinks a winding from its list
	=================
	*/
	inline void PW_ListDel( pwinding_t *n )
	{
		n->next->prev = n->prev;
		n->prev->next = n->next;
	}

	/*
	=================
	PW_Alloc

	allocates a cleared winding
	=================
	*/
	inline pwinding_t *PW_Alloc()
	{
		pwinding_t *w = new pwinding_t();
		w->prev = w->next = w->pair = NULL;
		w->plane = NULL;
		return w;
	}
	/*
	=================
	PW_Free

	frees a winding
	=================
	*/
	inline void PW_Free( pwinding_t *w ) { delete w; }

	/*
	=================
	PW_Copy

	duplicates a winding's plane and points
	=================
	*/
	pwinding_t *PW_Copy( const pwinding_t *src )
	{
		pwinding_t *w = PW_Alloc();
		w->plane = src->plane;
		w->p = src->p;
		return w;
	}

	/*
	=================
	PW_ForPlane

	builds a large square winding lying on a plane
	=================
	*/
	pwinding_t *PW_ForPlane( const mplane_t *pl )
	{
		PxVec3 normal( pl->normal[0], pl->normal[1], pl->normal[2] );
		int axis = 0;
		float max = -1.0f;
		for( int i = 0; i < 3; i++ )
		{
			float v = (float)fabs( normal[i] );
			if( v > max ) { max = v; axis = i; }
		}

		PxVec3 vup( 0.0f, 0.0f, 0.0f );
		if( axis == 2 ) { vup.x = 1.0f; } else { vup.z = 1.0f; }
		vup -= normal * normal.dot( vup );
		vup.normalize();

		PxVec3 org = normal * pl->dist;
		PxVec3 vright = vup.cross( normal );
		vup *= HULL_BOGUS_RANGE;
		vright *= HULL_BOGUS_RANGE;

		pwinding_t *w = PW_Alloc();
		w->plane = pl;
		w->p.resize( 4 );
		w->p[0] = org - vright + vup;
		w->p[1] = org + vright + vup;
		w->p[2] = org + vright - vup;
		w->p[3] = org - vright - vup;
		return w;
	}

	/*
	=================
	PW_CalcSides

	classifies each winding point against a split plane as front, back or on-plane
	=================
	*/
	void PW_CalcSides( const pwinding_t *in, const mplane_t *split, std::vector<int> &sides,
		std::vector<float> &dists, int counts[3], float epsilon )
	{
		int n = (int)in->p.size();
		sides.resize( n + 1 );
		dists.resize( n + 1 );
		counts[0] = counts[1] = counts[2] = 0;
		PxVec3 sn( split->normal[0], split->normal[1], split->normal[2] );

		for( int i = 0; i < n; i++ )
		{
			float dot = ( split->type < 3 ) ? ( in->p[i][split->type] - split->dist )
				: ( sn.dot( in->p[i] ) - split->dist );
			dists[i] = dot;
			if( dot > epsilon ) { sides[i] = 0; }
			else if( dot < -epsilon ) { sides[i] = 1; }
			else { sides[i] = 2; }
			counts[sides[i]]++;
		}
		sides[n] = sides[0];
		dists[n] = dists[0];
	}

	/*
	=================
	PW_PushToPlaneAxis

	snaps a point exactly onto an axial plane along its dominant axis
	=================
	*/
	void PW_PushToPlaneAxis( PxVec3 &v, const mplane_t *p )
	{
		int t = p->type % 3, a = ( t + 1 ) % 3, b = ( t + 2 ) % 3;
		v[t] = ( p->dist - p->normal[a] * v[a] - p->normal[b] * v[b] ) / p->normal[t];
	}

	/*
	=================
	PW_SplitPoint

	interpolates an on-plane split point, snapping to axial planes for precision
	=================
	*/
	PxVec3 PW_SplitPoint( const pwinding_t *in, const mplane_t *split, const PxVec3 &p1,
		const PxVec3 &p2, float dot )
	{
		PxVec3 mid;
		for( int j = 0; j < 3; j++ )
		{
			if( in->plane->normal[j] == 1.0f ) { mid[j] = in->plane->dist; }
			else if( in->plane->normal[j] == -1.0f ) { mid[j] = -in->plane->dist; }
			else if( split->normal[j] == 1.0f ) { mid[j] = split->dist; }
			else if( split->normal[j] == -1.0f ) { mid[j] = -split->dist; }
			else { mid[j] = p1[j] + dot * ( p2[j] - p1[j] ); }
		}
		if( in->plane->type < 3 )
		{
			PW_PushToPlaneAxis( mid, in->plane );
		}
		return mid;
	}

	/*
	=================
	PW_Split

	splits a winding into front and back halves without freeing the input
	=================
	*/
	void PW_Split( pwinding_t *in, const mplane_t *split, pwinding_t **pfront, pwinding_t **pback )
	{
		std::vector<int> sides;
		std::vector<float> dists;
		int counts[3];
		PW_CalcSides( in, split, sides, dists, counts, 0.04f );

		if( !counts[0] && !counts[1] ) { *pfront = PW_Copy( in ); *pback = PW_Copy( in ); return; }
		if( !counts[0] ) { *pfront = NULL; *pback = in; return; }
		if( !counts[1] ) { *pfront = in; *pback = NULL; return; }

		pwinding_t *front = PW_Alloc(); front->plane = in->plane;
		pwinding_t *back = PW_Alloc(); back->plane = in->plane;
		int n = (int)in->p.size();

		for( int i = 0; i < n; i++ )
		{
			PxVec3 p1 = in->p[i];
			if( sides[i] == 2 ) { front->p.push_back( p1 ); back->p.push_back( p1 ); continue; }
			if( sides[i] == 0 ) { front->p.push_back( p1 ); }
			else { back->p.push_back( p1 ); }

			if( sides[i + 1] == 2 || sides[i + 1] == sides[i] )
			{
				continue;
			}

			PxVec3 mid = PW_SplitPoint( in, split, p1, in->p[( i + 1 ) % n],
				dists[i] / ( dists[i] - dists[i + 1] ));
			front->p.push_back( mid );
			back->p.push_back( mid );
		}
		*pfront = front;
		*pback = back;
	}

	/*
	=================
	PW_Clip

	clips a winding to one side of a plane, freeing the input
	=================
	*/
	pwinding_t *PW_Clip( pwinding_t *in, const mplane_t *split, int side, float epsilon )
	{
		std::vector<int> sides;
		std::vector<float> dists;
		int counts[3];
		PW_CalcSides( in, split, sides, dists, counts, epsilon );

		if( !counts[side] ) { PW_Free( in ); return NULL; }
		if( !counts[side ^ 1] ) { return in; }

		pwinding_t *neww = PW_Alloc();
		neww->plane = in->plane;
		int n = (int)in->p.size();

		for( int i = 0; i < n; i++ )
		{
			PxVec3 p1 = in->p[i];
			if( sides[i] == 2 ) { neww->p.push_back( p1 ); continue; }
			if( sides[i] == side ) { neww->p.push_back( p1 ); }

			if( sides[i + 1] == 2 || sides[i + 1] == sides[i] )
			{
				continue;
			}

			neww->p.push_back( PW_SplitPoint( in, split, p1, in->p[( i + 1 ) % n],
				dists[i] / ( dists[i] - dists[i + 1] )));
		}
		PW_Free( in );
		return neww;
	}

	static mnode_t	*pw_node_stack[PW_MAX_DEPTH];
	static int	pw_side_stack[PW_MAX_DEPTH];
	static int	pw_depth;

	void PW_HullWindingsR( mnode_t *node, pwinding_t *polys, pwinding_t *out );

	/*
	=================
	PW_DoRecursion

hands a list of windings to a child: keeps them at an empty leaf, discards them at a solid leaf, or recurses when the child is another node
	=================
	*/
	void PW_DoRecursion( mnode_t *node, int side, pwinding_t *polys, pwinding_t *out )
	{
		mnode_t *child = node->children[side];
		pwinding_t *w, *next;

		if( !HullNodeIsLeaf( child ))
		{
			if( pw_depth < PW_MAX_DEPTH )
			{
				pw_node_stack[pw_depth] = node;
				pw_side_stack[pw_depth] = side;
				pw_depth++;
				PW_HullWindingsR( child, polys, out );
				pw_depth--;
			}
			else
			{
				// too deep, drop these windings rather than overflow
				for( w = polys->next; w != polys; w = next )
				{
					next = w->next;
					if( w->pair ) { w->pair->pair = NULL; }
					PW_ListDel( w );
					PW_Free( w );
				}
			}
			return;
		}

		if( !HullContentsSolid( child->contents ))
		{
			// empty-ish leaf: these windings border empty space, keep them
			for( w = polys->next; w != polys; w = next )
			{
				next = w->next;
				PW_ListDel( w );
				PW_ListAdd( w, out );
			}
		}
		else
		{
			// solid/sky leaf: discard, and break the pairing of the survivors
			for( w = polys->next; w != polys; w = next )
			{
				next = w->next;
				if( w->pair ) { w->pair->pair = NULL; }
				PW_ListDel( w );
				PW_Free( w );
			}
		}
	}

	/*
	=================
	PW_HullWindingsR

recursively splits inherited windings by each node's plane, then emits this node's own plane winding filtered through both subtrees
	=================
	*/
	void PW_HullWindingsR( mnode_t *node, pwinding_t *polys, pwinding_t *out )
	{
		const mplane_t *plane = node->plane;
		pwinding_t frontlist, backlist;
		PW_ListInit( &frontlist );
		PW_ListInit( &backlist );
		pwinding_t *w, *next, *front, *back;

		// split inherited windings by this plane, keeping pairs consistent
		for( w = polys->next; w != polys; w = next )
		{
			next = w->next;
			PW_ListDel( w );
			PW_Split( w, plane, &front, &back );
			if( front ) { PW_ListAdd( front, &frontlist ); }
			if( back ) { PW_ListAdd( back, &backlist ); }

			if( front && back )
			{
				pwinding_t *front2 = NULL, *back2 = NULL;
				if( w->pair )
				{
					PW_Split( w->pair, plane, &front2, &back2 );
				}

				if( front2 && back2 )
				{
					// the pair straddled too: keep all four halves paired up
					front2->pair = front; front->pair = front2;
					back2->pair = back; back->pair = back2;
					PW_ListAdd( front2, w->pair );
					PW_ListAdd( back2, w->pair );
					PW_ListDel( w->pair );
					PW_Free( w->pair );
				}
				else
				{
					// no pair, or grazing left the pair entirely on one side, just break the pairing
					front->pair = NULL;
					back->pair = NULL;
					if( front2 ) { front2->pair = NULL; }
					if( back2 ) { back2->pair = NULL; }
				}
				PW_Free( w );
			}
		}

		// this node's own plane winding, clipped to the cell by the ancestor stack
		w = PW_ForPlane( plane );
		for( int i = 0; w && i < pw_depth; i++ )
		{
			w = PW_Clip( w, pw_node_stack[i]->plane, pw_side_stack[i], 0.00001f );
		}

		if( w )
		{
			pwinding_t *tmp = PW_Copy( w );
			std::reverse( tmp->p.begin(), tmp->p.end() );
			w->pair = tmp;
			tmp->pair = w;
			PW_ListAdd( w, &frontlist );
			PW_ListAdd( tmp, &backlist );
		}

		PW_DoRecursion( node, 0, &frontlist, out );
		PW_DoRecursion( node, 1, &backlist, out );
	}

	/*
	=================
	PW_Emit

	triangulates a boundary winding as a fan, orienting it toward the empty side
	=================
	*/
	void PW_Emit( const pwinding_t *w, mnode_t *root, std::vector<PxVec3> &verts, std::vector<PxU32> &tris )
	{
		int n = (int)w->p.size();
		if( n < 3 )
		{
			return;
		}

		hullWinding_t hw( w->p.begin(), w->p.end() );
		PxVec3 wn = HullWindingNormal( hw );

		PxVec3 c( 0.0f, 0.0f, 0.0f );
		for( int i = 0; i < n; i++ ) { c += w->p[i]; }
		c *= ( 1.0f / (float)n );

		// flip so the surface normal points away from solid
		bool flip = HullContentsSolid( HullPointContents( root, c + wn * 1.0f ))
			&& !HullContentsSolid( HullPointContents( root, c - wn * 1.0f ));

		PxU32 base = (PxU32)verts.size();
		for( int i = 0; i < n; i++ )
		{
			verts.push_back( w->p[i] );
		}

		for( int i = 1; i < n - 1; i++ )
		{
			tris.push_back( base );
			if( !flip ) { tris.push_back( base + i ); tris.push_back( base + i + 1 ); }
			else { tris.push_back( base + i + 1 ); tris.push_back( base + i ); }
		}
	}

	/*
	=================
	PW_BuildWorld

	reconstructs the world collision boundary triangles from the hull-0 node tree
	=================
	*/
	void PW_BuildWorld( mnode_t *root, std::vector<PxVec3> &verts, std::vector<PxU32> &tris )
	{
		if( HullNodeIsLeaf( root ))
		{
			return;
		}

		pwinding_t polysHead, outHead;
		PW_ListInit( &polysHead );
		PW_ListInit( &outHead );
		pw_depth = 0;

		PW_HullWindingsR( root, &polysHead, &outHead );

		pwinding_t *w, *next;
		// remove interior faces (both sides empty - still paired)
		for( w = outHead.next; w != &outHead; w = next )
		{
			next = w->next;
			if( w->pair ) { PW_ListDel( w ); PW_Free( w ); }
		}
		// triangulate and free the survivors
		for( w = outHead.next; w != &outHead; w = next )
		{
			next = w->next;
			PW_Emit( w, root, verts, tris );
			PW_ListDel( w );
			PW_Free( w );
		}
	}

}

/*
=================
BuildCollisionTree

rebuilds and caches the world collision mesh from the BSP hull-0 solid boundary
=================
*/
int CPhysicPhysX :: BuildCollisionTree( char *szMapName )
{
	if( !m_pPhysics )
	{
		return FALSE;
	}
	 
	// get a world struct
	if(( m_pWorldModel = (model_t *)MODEL_HANDLE( 1 )) == NULL )
	{
		ALERT( at_error, "BuildCollisionTree: unbale to fetch world pointer %s\n", szMapName );
		return FALSE;
	}

	if (m_pSceneActor)
	{
		m_eventHandler->purgeActor(m_pSceneActor);
		m_pSceneActor->release();
	}
	m_pSceneActor = NULL;

	if (m_pSceneMesh)
	{
		m_pSceneMesh->release();
	}
	m_pSceneMesh = NULL;

	// save off mapname
	Q_strcpy( m_szMapName, szMapName );

	ALERT( at_console, "Tree Collision out of Date. Rebuilding...\n" );

	// reconstruct the solid boundary from the BSP node tree (hull 0) so invisible but solid brushes get collision too
	std::vector<PxVec3> hullVerts;
	std::vector<PxU32> hullTris;

	if( m_pWorldModel->nodes && m_pWorldModel->numnodes > 0 )
	{
		PW_BuildWorld( m_pWorldModel->nodes, hullVerts, hullTris );
	}

	if( hullTris.empty() )
	{
		ALERT( at_error, "BuildCollisionTree: no solid geometry reconstructed for %s\n", szMapName );
		return FALSE;
	}

	ALERT( at_console, "Cooked world collision: %i tris from BSP hull\n", (int)( hullTris.size() / 3 ));

	// build physical model
	PxTriangleMeshDesc levelDesc;
	levelDesc.points.count = (PxU32)hullVerts.size();
	levelDesc.points.data = hullVerts.data();
	levelDesc.points.stride = sizeof( PxVec3 );
	levelDesc.triangles.count = (PxU32)( hullTris.size() / 3 );
	levelDesc.triangles.data = hullTris.data();
	levelDesc.triangles.stride = 3 * sizeof( PxU32 );
	levelDesc.flags = (PxMeshFlags)0;

	char szHullFilename[MAX_PATH];
	Q_snprintf( szHullFilename, sizeof( szHullFilename ), "cache/maps/%s_hull3.bin", szMapName ); // versioned cache ("_hull3"), see FLoadTree

	if (m_pCooking)
	{
		UserStream outputFileStream(szHullFilename, false);
		bool status = m_pCooking->cookTriangleMesh(levelDesc, outputFileStream);
	}

	UserStream inputFileStream(szHullFilename, true);
	m_pSceneMesh = m_pPhysics->createTriangleMesh(inputFileStream);
	m_fWorldChanged = TRUE;

	return (m_pSceneMesh != NULL) ? TRUE : FALSE;
}

/*
=================
SetupWorld

creates the static world actor from the cooked collision mesh and marks physics loaded
=================
*/
void CPhysicPhysX::SetupWorld(void)
{
	if (m_pSceneActor)
	{
		return;	// already loaded
	}

	if (!m_pSceneMesh)
	{
		ALERT(at_error, "*collision tree not ready!\n");
		return;
	}

	// get a world struct
	if ((m_pWorldModel = (model_t *)MODEL_HANDLE(1)) == NULL)
	{
		ALERT(at_error, "SetupWorld: unbale to fetch world pointer %s\n", m_szMapName);
		return;
	}

	PxRigidStatic *pActor = m_pPhysics->createRigidStatic(PxTransform(PxVec3(PxZero), PxQuat(PxIdentity)));
	PxShape *pShape = PxRigidActorExt::createExclusiveShape(*pActor, PxTriangleMeshGeometry(m_pSceneMesh), *m_pDefaultMaterial);

	pActor->setName(g_pWorld->GetClassname());
	pActor->userData = g_pWorld->edict();

	m_pScene->addActor(*pActor);
	m_pSceneActor = pActor;
	m_worldBounds = pActor->getWorldBounds();
	m_eventHandler->onWorldInit();
	m_fLoaded = true;
}

/*
=================
P_SpeedsMessage

copies the current physics stats string out when p_speeds is enabled
=================
*/
bool CPhysicPhysX :: P_SpeedsMessage( char *out, size_t size )
{
	if( !p_speeds || p_speeds->value <= 0.0f )
	{
		return false;
	}

	if( !out || !size ) { return false; }
	Q_strncpy( out, p_speeds_msg, size );

	return true;
}

/*
=================
DrawPSpeeds

draws the physics stats string on screen
=================
*/
void CPhysicPhysX :: DrawPSpeeds( void )
{
	char	msg[1024];
	int	iScrWidth = CVAR_GET_FLOAT( "width" );

	if( P_SpeedsMessage( msg, sizeof( msg )))
	{
		int	x, y, height;
		char	*p, *start, *end;

		x = iScrWidth - 320;
		y = 128;

		DrawConsoleStringLen( NULL, NULL, &height );
		DrawSetTextColor( 1.0f, 1.0f, 1.0f );

		p = start = msg;
		do
		{
			end = Q_strchr( p, '\n' );
			if( end ) { msg[end-start] = '\0'; }

			DrawConsoleString( x, y, p );
			y += height;

			if( end )
			{
				p = end + 1;
			}
			else
			{
				break;
			}
		} while( 1 );
	}
}

/*
=================
FreeWorld

tears down all ragdolls, bodies and world actors when a level ends
=================
*/
void CPhysicPhysX :: FreeWorld()
{
	if( !m_pScene )
	{
		return;
	}

	for( int n = 0; n < m_numRagdolls; n++ )
	{
		for( int i = 0; i < RAGDOLL_JOINTS; i++ )
		{
			if( m_ragdolls[n].joints[i] )
			{
				m_ragdolls[n].joints[i]->release();
			}
		}
	}
	m_numRagdolls = 0;
	m_numPendingRagdolls = 0;
	m_numRagdollConfigs = 0;
	m_flRagdollUpdateTime = 0.0f;

	PxActorTypeFlags actorFlags = (
		PxActorTypeFlag::eRIGID_STATIC |
		PxActorTypeFlag::eRIGID_DYNAMIC
	);

	std::vector<PxActor*> actors;
	actors.assign(m_pScene->getNbActors(actorFlags), nullptr);
	m_pScene->getActors(actorFlags, actors.data(), actors.size() * sizeof(actors[0]));

	// throw all bodies
	for (PxActor *actor : actors)
	{
		CBaseEntity *entity = EntityFromActor( actor );
		if (entity) {
			entity->m_pUserData = nullptr;
		}
		m_pScene->removeActor(*actor);
		actor->release();
	}

	m_holdableController->ClearAllTargets();
	m_eventHandler->onWorldShutdown();
	m_pSceneActor = nullptr;
}

/*
=================
TeleportCharacter

moves a character's box actor to the entity origin, resizing it to the current bounds
=================
*/
void CPhysicPhysX :: TeleportCharacter( CBaseEntity *pEntity )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor )
	{
		return;
	}

	PxRigidBody *pRigidBody = pActor->is<PxRigidBody>();
	if (pRigidBody->getNbShapes() <= 0)
	{
		return;
	}

	PxShape *pShape;
	pRigidBody->getShapes(&pShape, 1); // get only first shape, but it can be several
	Vector vecOffset = (pEntity->IsMonster()) ? Vector( 0, 0, pEntity->pev->maxs.z / 2.0f ) : g_vecZero;

	if (pShape->getGeometryType() == PxGeometryType::eBOX)
	{
		PxBoxGeometry &box = pShape->getGeometry().box();
		PxTransform pose = pRigidBody->getGlobalPose();
		box.halfExtents = pEntity->pev->size * CVAR_GET_FLOAT("phys_character_padding");
		pose.p = (pEntity->GetAbsOrigin() + vecOffset);
		pRigidBody->setGlobalPose(pose);
	}
	else {
		ALERT(at_error, "TeleportCharacter: shape geometry type is not a box\n");
	}
}

/*
=================
TeleportActor

snaps a body's world pose to the entity's origin and angles
=================
*/
void CPhysicPhysX :: TeleportActor( CBaseEntity *pEntity )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if (!pActor)
	{
		return;
	}

	if (TracingStateChanges(pActor)) {
		ALERT(at_console, "PhysX: TeleportActor( entity = %x )\n", pEntity);
	}

	PxRigidBody *pRigidBody = pActor->is<PxRigidBody>();
	matrix4x4 m(pEntity->GetAbsOrigin(), pEntity->GetAbsAngles(), 1.0f);
	PxTransform pose = PxTransform(PxMat44(m));
	pRigidBody->setGlobalPose( pose );
}

/*
=================
MoveCharacter

sets a moved character's kinematic target, toggling collision for noclip/fly and resizing its bounds
=================
*/
void CPhysicPhysX :: MoveCharacter( CBaseEntity *pEntity )
{
	if (!pEntity || pEntity->m_vecOldPosition == pEntity->pev->origin)
	{
		return;
	}

	PxActor *pActor = ActorFromEntity(pEntity);
	if (!pActor)
	{
		return;
	}

	PxRigidDynamic *pRigidBody = pActor->is<PxRigidDynamic>();
	if (pRigidBody->getNbShapes() <= 0)
	{
		return;
	}

	if (TracingStateChanges(pActor)) 
	{
		ALERT(at_console, 
			"PhysX: MoveCharacter( entity = %x, from = (%.2f, %.2f, %.2f), to = (%.2f, %.2f, %.2f) )\n", 
			pEntity, 
			pEntity->m_vecOldPosition.x,
			pEntity->m_vecOldPosition.y,
			pEntity->m_vecOldPosition.z,
			pEntity->pev->origin.x,
			pEntity->pev->origin.y,
			pEntity->pev->origin.z
		);
	}

	PxShape *pShape;
	pRigidBody->getShapes(&pShape, 1); // get only first shape, but it can be several

	if (pEntity->IsPlayer())
	{
		// if we're in NOCLIP or FLY (ladder climbing) mode - disable collisions
		if (pEntity->pev->movetype != MOVETYPE_WALK)
		{
			ToggleCollision(pRigidBody, false);
		}
		else
		{
			ToggleCollision(pRigidBody, true);
		}
	}

	UpdateCharacterBounds(pEntity, pShape);
	Vector vecOffset = (pEntity->IsMonster()) ? Vector(0, 0, pEntity->pev->maxs.z / 2.0f) : g_vecZero;
	PxTransform pose = pRigidBody->getGlobalPose();
	pose.p = (pEntity->GetAbsOrigin() + vecOffset);
	pRigidBody->setKinematicTarget(pose);
	pEntity->m_vecOldPosition = pEntity->GetAbsOrigin(); // update old position
}

/*
=================
UpdateCharacterBounds

resizes a character's box shape when its entity bounds change
=================
*/
void CPhysicPhysX::UpdateCharacterBounds(CBaseEntity *pEntity, PxShape *pShape)
{
	if (pEntity->pev->size == pEntity->m_vecOldBounds)
	{
		return;
	}

	PxBoxGeometry box;
	box.halfExtents = pEntity->pev->size * CVAR_GET_FLOAT("phys_character_padding");
	pShape->setGeometry(box);
	pEntity->m_vecOldBounds = pEntity->pev->size;
}

/*
=================
MoveKinematic

sets a mover entity's kinematic target and toggles collision by its solid state
=================
*/
void CPhysicPhysX :: MoveKinematic( CBaseEntity *pEntity )
{
	if( !pEntity || ( pEntity->pev->movetype != MOVETYPE_PUSH && pEntity->pev->movetype != MOVETYPE_PUSHSTEP ))
	{
		return;	// probably not a mover
	}

	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor )
	{
		return;
	}

	PxRigidDynamic *pRigidBody = pActor->is<PxRigidDynamic>();
	if (pRigidBody->getNbShapes() <= 0)
	{
		return;
	}

	if( pEntity->pev->solid == SOLID_NOT || pEntity->pev->solid == SOLID_TRIGGER )
	{
		ToggleCollision(pRigidBody, false);
	}
	else 
	{
		ToggleCollision(pRigidBody, true);
	}

	PxTransform pose;
	matrix4x4 m( pEntity->GetAbsOrigin(), pEntity->GetAbsAngles( ), 1.0f );

	// complex move for kinematic entities
	pose = PxTransform(PxMat44(m));
	pRigidBody->setKinematicTarget( pose );
}

/*
=================
EnableCollision

toggles collision and visualization on a body
=================
*/
void CPhysicPhysX :: EnableCollision( CBaseEntity *pEntity, int fEnable )
{
	PxActor *pActor = ActorFromEntity(pEntity);
	if (!pActor)
	{
		return;
	}

	PxRigidDynamic *pRigidBody = pActor->is<PxRigidDynamic>();
	if (pRigidBody->getNbShapes() <= 0)
	{
		return;
	}

	if (TracingStateChanges(pActor)) {
		ALERT(at_console, "PhysX: EnableCollision( entity = %x, state = %s )\n", pEntity, fEnable ? "true" : "false");
	}

	if (fEnable)
	{
		ToggleCollision(pRigidBody, true);
		pActor->setActorFlag(PxActorFlag::eVISUALIZATION, true);
	}
	else
	{
		ToggleCollision(pRigidBody, false);
		pActor->setActorFlag(PxActorFlag::eVISUALIZATION, false);
	}
}

/*
=================
MakeKinematic

sets or clears a body's kinematic flag
=================
*/
void CPhysicPhysX :: MakeKinematic( CBaseEntity *pEntity, int fEnable )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if (!pActor)
	{
		return;
	}

	PxRigidBody *pRigidBody = pActor->is<PxRigidBody>();
	if (!pRigidBody || pRigidBody->getNbShapes() <= 0)
	{
		return;
	}

	if (TracingStateChanges(pActor)) {
		ALERT(at_console, "PhysX: MakeKinematic( entity = %x, state = %s )\n", pEntity, fEnable ? "true" : "false");
	}

	if (fEnable)
	{
		pRigidBody->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
	}
	else
	{
		pRigidBody->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, false);
	}
}

/*
=================
SweepTest

traces a moving player box against an entity's decomposed collision mesh, building and caching that mesh on first use
=================
*/
void CPhysicPhysX :: SweepTest( CBaseEntity *pTouch, const Vector &start, const Vector &mins, const Vector &maxs, const Vector &end, trace_t *tr )
{
	PxActor *pActor = ActorFromEntity( pTouch );
	if (!pActor)
	{
		// bad actor?
		tr->allsolid = false;
		return;
	}

	PxRigidActor *pRigidActor = pActor->is<PxRigidActor>();
	if (!pRigidActor || pRigidActor->getNbShapes() <= 0 || !CheckCollision(pRigidActor))
	{
		// bad actor?
		tr->allsolid = false;
		return;
	}

	Vector trace_mins, trace_maxs;
	UTIL_MoveBounds(start, mins, maxs, end, trace_mins, trace_maxs);

	// NOTE: pmove code completely ignore a bounds checking. So we need to do it here
	if (!BoundsIntersect(trace_mins, trace_maxs, pTouch->pev->absmin, pTouch->pev->absmax))
	{
		tr->allsolid = false;
		return;
	}

	mmesh_t *pMesh = pTouch->m_BodyMesh.CheckMesh( pTouch->GetAbsOrigin(), pTouch->GetAbsAngles( ));
	areanode_t *pHeadNode = pTouch->m_BodyMesh.GetHeadNode();

	if( !pMesh )
	{
		DecomposedShape shape;
		if( !shape.Triangulate( pRigidActor ))
		{
			// failed to triangulate, unsupported mesh type, so skip them
			tr->allsolid = false;
			return;
		}

		PxTransform globalPose = pRigidActor->getGlobalPose();
		pTouch->m_BodyMesh.InitMeshBuild( pTouch->GetModel(), shape.GetTrianglesCount( ));

		Vector triangle[3];
		const auto &indexBuffer = shape.GetIndexBuffer();
		const auto &vertexBuffer = shape.GetVertexBuffer();

		// NOTE: we compute triangles in abs coords because player AABB can't be transformed as done for not axial cases
		// FIXME: store all meshes as local and use capsule instead of bbox
		for( size_t i = 0; i < shape.GetTrianglesCount(); i++ )
		{
			uint32_t i0 = indexBuffer[3 * i];
			uint32_t i1 = indexBuffer[3 * i + 1];
			uint32_t i2 = indexBuffer[3 * i + 2];

			triangle[0] = globalPose.transform( vertexBuffer[i0] );
			triangle[1] = globalPose.transform( vertexBuffer[i1] );
			triangle[2] = globalPose.transform( vertexBuffer[i2] );

			pTouch->m_BodyMesh.AddMeshTriangle( triangle );
		}

		if( !pTouch->m_BodyMesh.FinishMeshBuild( ))
		{
			ALERT( at_error, "failed to build mesh from %s\n", pTouch->GetModel() );
			tr->allsolid = false;
			return;
		}

		pMesh = pTouch->m_BodyMesh.GetMesh();
		pHeadNode = pTouch->m_BodyMesh.GetHeadNode();
	}

	TraceMesh trm;

	trm.SetTraceMesh( pMesh, pHeadNode );
	trm.SetupTrace( start, mins, maxs, end, tr );

	if( trm.DoTrace( ))
	{
		if( tr->fraction < 1.0f || tr->startsolid )
		{
			tr->ent = pTouch->edict();
		}
	}
}

/*
=================
SweepEntity

sweeps an entity's box through the scene and returns the first hit, or tests for being stuck when start equals end
=================
*/
void CPhysicPhysX :: SweepEntity( CBaseEntity *pEntity, const Vector &start, const Vector &end, TraceResult *tr )
{
	// make trace default
	memset( tr, 0, sizeof( *tr ));
	tr->flFraction = 1.0f;
	tr->vecEndPos = end;

	PxActor *pActor = ActorFromEntity( pEntity );
	if (!pActor)
	{
		return;
	}

	PxRigidActor *pRigidActor = pActor->is<PxRigidActor>();
	if (!pRigidActor || pRigidActor->getNbShapes() <= 0 || pEntity->pev->solid == SOLID_NOT)
	{
		return; // only dynamic solid objects can be traced
	}

	// test for stuck entity into another
	if (start == end)
	{
		Vector triangle[3], dirs[3];
		PxTransform globalPose = pRigidActor->getGlobalPose();
		DecomposedShape shape;
		if (!shape.Triangulate(pRigidActor)) {
			return; // failed to triangulate
		}

		const auto &indexBuffer = shape.GetIndexBuffer();
		const auto &vertexBuffer = shape.GetVertexBuffer();
		for (size_t i = 0; i < shape.GetTrianglesCount(); i++)
		{
			uint32_t i0 = indexBuffer[3 * i];
			uint32_t i1 = indexBuffer[3 * i + 1];
			uint32_t i2 = indexBuffer[3 * i + 2];
			vec3_t v0 = vertexBuffer[i0];
			vec3_t v1 = vertexBuffer[i1];
			vec3_t v2 = vertexBuffer[i2];

			// transform triangles from local to world space
			triangle[0] = globalPose.transform(v0);
			triangle[1] = globalPose.transform(v1);
			triangle[2] = globalPose.transform(v2);

			for (size_t j = 0; j < 3; j++)
			{
				dirs[j] = globalPose.p - triangle[j];
				triangle[j] += dirs[j] * -2.0f;
				UTIL_TraceLine(triangle[j], triangle[j], ignore_monsters, pEntity->edict(), tr);
				if (tr->fStartSolid) 
				{
					return;	// one of points in solid
				}
			}
		}
		return;
	}

	// compute motion
	Vector vecDir = end - start;
	float flLength = vecDir.Length();
	vecDir = vecDir.Normalize();

	// setup trace box
	PxBoxGeometry testBox;
	PxTransform initialPose = pRigidActor->getGlobalPose();
	initialPose.p = pEntity->Center(); // does we really need to do this?
	testBox.halfExtents = pEntity->pev->size * CVAR_GET_FLOAT("phys_character_padding");

	// make a linear sweep through the world
	// we need to disable collision here to avoid touching same actor as we trying to sweep
	PxSweepBuffer sweepResult;
	bool hitOccured = m_pScene->sweep(testBox, initialPose, vecDir, flLength, sweepResult, PxHitFlag::eNORMAL);

	if (!hitOccured || sweepResult.getNbAnyHits() < 1)
	{
		return; // no intersection
	}

	const PxSweepHit &hit = sweepResult.getAnyHit(0);
	if (hit.distance > flLength || !hit.actor)
	{
		return; // hit missed
	}

	// compute fraction
	tr->flFraction = (hit.distance / flLength);
	tr->flFraction = bound( 0.0f, tr->flFraction, 1.0f );
	VectorLerp( start, tr->flFraction, end, tr->vecEndPos );

	CBaseEntity *pHit = EntityFromActor( hit.actor );
	if (pHit) {
		tr->pHit = pHit->edict();
	}

	tr->vecPlaneNormal = hit.normal;
	tr->flPlaneDist = DotProduct( tr->vecEndPos, tr->vecPlaneNormal );
	float flDot = DotProduct( vecDir, tr->vecPlaneNormal );
	float moveDot = Q_round( flDot, 0.1f );

	// FIXME: this is incorrect. Find a better method?
	if(( tr->flFraction < 0.1f ) && ( moveDot < 0.0f ))
	{
		tr->fAllSolid = true;
	}
}

#endif // USE_PHYSICS_ENGINE
