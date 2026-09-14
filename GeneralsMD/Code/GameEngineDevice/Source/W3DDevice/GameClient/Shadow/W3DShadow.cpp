/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////


// FILE: W3DShadow.cpp ///////////////////////////////////////////////////////////
//
// Real time shadow representations
//
// Author: Mark Wilczynski, February 2002
//
//

// USER INCLUDES //////////////////////////////////////////////////////////////
#include "dx8wrapper.h"
#include "always.h"
#include "GameClient/View.h"
#include "WW3D2/camera.h"
#include "WW3D2/light.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/hlod.h"
#include "WW3D2/mesh.h"
#include "WW3D2/meshmdl.h"
#include "Lib/BaseType.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "d3dx8math.h"
#include "Common/GlobalData.h"
#include "W3DDevice/GameClient/W3DVolumetricShadow.h"
#include "W3DDevice/GameClient/W3DProjectedShadow.h"
#include "W3DDevice/GameClient/W3DShadow.h"
#include "WW3D2/statistics.h"
#include "Common/Debug.h"
#include "Common/PerfTimer.h"

#define SUN_DISTANCE_FROM_GROUND	10000.0f	//distance of sun (our only light source).

// Global Variables and Functions /////////////////////////////////////////////
W3DShadowManager *TheW3DShadowManager=nullptr;
const FrustumClass *shadowCameraFrustum;

Vector3 LightPosWorld[ MAX_SHADOW_LIGHTS ] =
{

	Vector3( 94.0161f, 50.499f, 200.0f)
};

void PrepareShadows()
{
	if (TheW3DProjectedShadowManager)
		TheW3DProjectedShadowManager->prepareShadows();
}

//DECLARE_PERF_TIMER(shadowsRender)
void DoShadows(RenderInfoClass & rinfo, Bool stencilPass)
{
#ifdef __ANDROID__
	// GeneralsX @feature Codex 13/09/2026 Tag projected AND volume/fill passes.
	extern int GX_XR_ShadowCategory(int);
	const int previous=GX_XR_ShadowCategory(5);
	struct Restore {int category;~Restore(){GX_XR_ShadowCategory(category);}} restore{previous};
#endif
	//USE_PERF_TIMER(shadowsRender)
	shadowCameraFrustum=&rinfo.Camera.Get_Frustum();

	// GeneralsX @bugfix Android port 09/05/2026 projectionCount used to be a
	// LOCAL here, which made W3DVolumetricShadowManager's forceStencilFill
	// argument dead code. DoShadows() is called twice per frame from
	// W3DScene: once with stencilPass=FALSE, which is the only call that
	// computes projectionCount from the projected-shadow manager, and once
	// with stencilPass=TRUE, which is the only call that passes it on. As a
	// local it was re-zeroed by the second call, so renderShadows() always
	// received 0 and its "no shadow volumes to render, but still need to fill
	// the stencil buffer for other effects" branch could never run.
	//
	// That matters beyond the stencil fill itself: both of renderShadows()'
	// branches end with DX8Wrapper::Invalidate_Cached_Render_States(), and
	// with shadow volumes OFF (Medium detail and below) neither branch ran, so
	// nothing flushed that cache all session. DX8Wrapper::Set_DX8_Texture and
	// Set_DX8_Texture_Stage_State are pure redundancy filters -- they skip the
	// real call when they believe the state already matches -- and several
	// subsystems (W3DWater, W3DTreeBuffer, the shadow setup itself) poke the
	// device directly, desyncing that belief. On High the per-frame flush hid
	// it; on a Medium-only session it was never repaired, which is why
	// textures came back black only after a fresh start on Medium and why
	// switching to High "fixed" it permanently.
	static Int projectionCount = 0;

	//Projected shadows render first because they may fill the stencil buffer
	//which will be used by the shadow volumes
	if (stencilPass == FALSE  && TheW3DProjectedShadowManager)
	{
			if (TheW3DShadowManager->isShadowScene())
				projectionCount=TheW3DProjectedShadowManager->renderShadows(rinfo);
	}

	if (stencilPass == TRUE && TheW3DVolumetricShadowManager)
	{

//		TheW3DShadowManager->loadTerrainShadows();

			//This function gets called many times by the W3D renderer
			//so we use this flag to make sure shadows rendered only once per frame.
			if (TheW3DShadowManager->isShadowScene())
				TheW3DVolumetricShadowManager->renderShadows(projectionCount);
	}
	if (TheW3DShadowManager && stencilPass)	//reset so no more shadow processing this frame.
		TheW3DShadowManager->queueShadows(FALSE);

	// GeneralsX @bugfix Android port 09/05/2026 Flush DX8Wrapper's state caches
	// once per frame, unconditionally.
	//
	// This flush already existed, but only INSIDE
	// W3DVolumetricShadowManager::renderShadows(), at the end of both of its
	// branches -- so it ran only when there were shadow volumes to draw, or
	// (after the projectionCount fix above) when there were projected shadows
	// to fill the stencil with. Fixing projectionCount restored it for Medium,
	// where shadow decals are still on, but LOW turns BOTH shadow types off
	// (shadowVolumes=0 shadowDecals=0), so on Low nothing reached the flush and
	// the original bug came straight back. Measured: stage-0 texture collapses
	// in the GLES backend climbing to 14000-16000 per two-second window against
	// 6-549 in a healthy one.
	//
	// Why the flush is needed at all: DX8Wrapper::Set_DX8_Texture and
	// Set_DX8_Texture_Stage_State are redundancy filters that skip the real
	// device call when they believe the state already matches, and several
	// subsystems (W3DWater, W3DTreeBuffer, W3DShaderManager, the shadow setup)
	// drive the device directly and desync that belief. A draw then samples a
	// stage with no texture actually bound and renders as flat vertex colour --
	// black geometry, no GL error, texture still alive. The engine has always
	// depended on this per-frame repair; it just happened to be parked inside a
	// renderer that a low detail level switches off entirely.
	//
	// Tying the repair to DoShadows rather than to any shadow being drawn is
	// deliberate: this function is called every frame from
	// W3DScene::Customized_Render regardless of detail level. It is still a
	// compensation rather than a cure -- the real fix is that those subsystems
	// should not desync the caches in the first place -- so it is placed and
	// commented as such rather than left implicit.
	if (stencilPass)
		DX8Wrapper::Invalidate_Cached_Render_States();

}

W3DShadowManager::W3DShadowManager()
{
	DEBUG_ASSERTCRASH(TheW3DVolumetricShadowManager == nullptr && TheW3DProjectedShadowManager == nullptr,
		("Creating new shadow managers without deleting old ones"));

	m_shadowColor = 0x7fa0a0a0;
	m_isShadowScene = FALSE;
	m_stencilShadowMask = 0;	//all bits can be used for storing shadows.

	Vector3 lightRay(-TheGlobalData->m_terrainLightPos[0].x,
		-TheGlobalData->m_terrainLightPos[0].y, -TheGlobalData->m_terrainLightPos[0].z);
	lightRay.Normalize();

	LightPosWorld[0]=lightRay*SUN_DISTANCE_FROM_GROUND;

	TheW3DVolumetricShadowManager = NEW W3DVolumetricShadowManager;
	TheProjectedShadowManager = TheW3DProjectedShadowManager = NEW W3DProjectedShadowManager;
}

W3DShadowManager::~W3DShadowManager()
{
	delete TheW3DVolumetricShadowManager;
	TheW3DVolumetricShadowManager = nullptr;
	delete TheW3DProjectedShadowManager;
	TheProjectedShadowManager = TheW3DProjectedShadowManager = nullptr;
}

/** Do one-time initilalization of shadow systems that need to be
active for full duration of game*/
Bool W3DShadowManager::init()
{
	Bool result=TRUE;

	if	(TheW3DVolumetricShadowManager && TheW3DVolumetricShadowManager->init())
	{
		if (TheW3DVolumetricShadowManager->ReAcquireResources())
			result = TRUE;
	}
	if ( TheW3DProjectedShadowManager && TheW3DProjectedShadowManager->init())
	{
		if (TheW3DProjectedShadowManager->ReAcquireResources())
			result = TRUE;
	}

	return result;
}

/** Do per-map reset.  This frees up shadows from all objects since
they may not exist on the next map*/
void W3DShadowManager::Reset()
{

	if (TheW3DVolumetricShadowManager)
		TheW3DVolumetricShadowManager->reset();
	if (TheW3DProjectedShadowManager)
		TheW3DProjectedShadowManager->reset();
}

Bool W3DShadowManager::ReAcquireResources()
{
	Bool result = TRUE;

	if (TheW3DVolumetricShadowManager && !TheW3DVolumetricShadowManager->ReAcquireResources())
		result = FALSE;
	if (TheW3DProjectedShadowManager && !TheW3DProjectedShadowManager->ReAcquireResources())
		result = FALSE;

	return result;
}

void W3DShadowManager::ReleaseResources()
{
	if (TheW3DVolumetricShadowManager)
		TheW3DVolumetricShadowManager->ReleaseResources();
	if (TheW3DProjectedShadowManager)
		TheW3DProjectedShadowManager->ReleaseResources();
}

Shadow *W3DShadowManager::addShadow( RenderObjClass *robj, Shadow::ShadowTypeInfo *shadowInfo, Drawable *draw)
{
	ShadowType type = SHADOW_VOLUME;

	if (shadowInfo)
		type = shadowInfo->m_type;

	// GeneralsX @bugfix BenderAI 21/03/2026 ShadowType contains bit flags; route by mask instead of exact enum value.
	if (type & SHADOW_VOLUME)
	{
		if (TheW3DVolumetricShadowManager)
			return (Shadow *)TheW3DVolumetricShadowManager->addShadow(robj, shadowInfo, draw);
	}

	if (type & (SHADOW_PROJECTION | SHADOW_DYNAMIC_PROJECTION | SHADOW_DECAL | SHADOW_ALPHA_DECAL | SHADOW_ADDITIVE_DECAL))
	{
		if (TheW3DProjectedShadowManager)
			return (Shadow *)TheW3DProjectedShadowManager->addShadow(robj, shadowInfo, draw);
	}

	return nullptr;
}

void W3DShadowManager::removeShadow(Shadow *shadow)
{
	shadow->release();
}

void W3DShadowManager::removeAllShadows()
{
	if (TheW3DVolumetricShadowManager)
		TheW3DVolumetricShadowManager->removeAllShadows();
	if (TheW3DProjectedShadowManager)
		TheW3DProjectedShadowManager->removeAllShadows();
}

/**Force update of all shadows even when light source and object have not moved*/
void W3DShadowManager::invalidateCachedLightPositions()
{
	if (TheW3DVolumetricShadowManager)
		TheW3DVolumetricShadowManager->invalidateCachedLightPositions();
	if (TheW3DProjectedShadowManager)
		TheW3DProjectedShadowManager->invalidateCachedLightPositions();
}

Vector3 &W3DShadowManager::getLightPosWorld(Int lightIndex)
{
	return LightPosWorld[lightIndex];
}

void W3DShadowManager::setLightPosition(Int lightIndex, Real x, Real y, Real z)
{
	if (lightIndex != 0)
		return;	///@todo: Add support for multiple lights

	LightPosWorld[lightIndex]=Vector3(x,y,z);
}

void W3DShadowManager::setTimeOfDay(TimeOfDay tod)
{
	//Ray to light source
	const GlobalData::TerrainLighting *ol=&TheGlobalData->m_terrainObjectsLighting[tod][0];

	Vector3 lightRay(-ol->lightPos.x,-ol->lightPos.y,-ol->lightPos.z);

	lightRay.Normalize();
	lightRay *= SUN_DISTANCE_FROM_GROUND;

	setLightPosition(0, lightRay.X, lightRay.Y, lightRay.Z);
}
