/** 
 * @file llvoclouds.cpp
 * @brief Implementation of LLVOClouds class which is a derivation fo LLViewerObject
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llvoclouds.h"

#include "lldrawpoolalpha.h"

#include "llviewercontrol.h"

#include "llagent.h"		// to get camera position
#include "lldrawable.h"
#include "llface.h"
#include "llprimitive.h"
#include "llsky.h"
#include "llviewercamera.h"
#include "llviewertexturelist.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llvosky.h"
#include "llworld.h"
#include "pipeline.h"
#include "llspatialpartition.h"

LLUUID gCloudTextureID = IMG_CLOUD_POOF;


LLVOClouds::LLVOClouds(const LLUUID &id, const LLPCode pcode, LLViewerRegion *regionp)
:	LLAlphaObject(id, LL_VO_CLOUDS, regionp)
{
	mCloudGroupp = NULL;
	mbCanSelect = FALSE;
	setNumTEs(1);
	LLViewerTexture* image = LLViewerTextureManager::getFetchedTexture(gCloudTextureID);
	image->setBoostLevel(LLViewerTexture::BOOST_CLOUDS);
	setTEImage(0, image);
}


LLVOClouds::~LLVOClouds()
{
}


BOOL LLVOClouds::isActive() const
{
	return TRUE;
}


BOOL LLVOClouds::idleUpdate(LLAgent &agent, LLWorld &world, const F64 &time)
{
	if (!(gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_CLASSIC_CLOUDS)))
	{
		return TRUE;
	}
	
	// Set dirty flag (so renderer will rebuild primitive)
	if (mDrawable)
	{
		gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_VOLUME, TRUE);
	}

	return TRUE;
}


void LLVOClouds::setPixelAreaAndAngle(LLAgent &agent)
{
	mAppAngle = 50;
	mPixelArea = 1500*100;
}

void LLVOClouds::updateTextures()
{
	getTEImage(0)->addTextureStats(mPixelArea);
}

LLDrawable* LLVOClouds::createDrawable(LLPipeline *pipeline)
{
	pipeline->allocDrawable(this);
	mDrawable->setLit(FALSE);
	mDrawable->setRenderType(LLPipeline::RENDER_TYPE_CLASSIC_CLOUDS);

	return mDrawable;
}

BOOL LLVOClouds::updateGeometry(LLDrawable *drawable)
{
	LLFastTimer ftm(LLFastTimer::FTM_UPDATE_CLOUDS);
	if (!(gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_CLASSIC_CLOUDS)))
	{
		return TRUE;
	}
	
	if (drawable->isVisible())
	{
		dirtySpatialGroup(TRUE);
	}

	LLFace *facep;
	
	S32 num_faces = mCloudGroupp->getNumPuffs();

	if (num_faces > drawable->getNumFaces())
	{
		drawable->setNumFacesFast(num_faces, NULL, getTEImage(0));
	}

	mDepth = (getPositionAgent()-LLViewerCamera::getInstance()->getOrigin())*LLViewerCamera::getInstance()->getAtAxis();

	S32 face_indx = 0;
	for ( ;	face_indx < num_faces; face_indx++)
	{
		facep = drawable->getFace(face_indx);
		if (!facep)
		{
			llwarns << "No facep for index " << face_indx << llendl;
			continue;
		}

		facep->setSize(4, 6);
		
		facep->setTEOffset(face_indx);
		facep->setTexture(getTEImage(0));
		const LLCloudPuff &puff = mCloudGroupp->getPuff(face_indx);
		const LLVector3 puff_pos_agent = gAgent.getPosAgentFromGlobal(puff.getPositionGlobal());
		facep->mCenterLocal = puff_pos_agent;
		/// Update cloud color based on sun color.
		LLColor4 float_color(LLColor3(gSky.getSunDiffuseColor() + gSky.getSunAmbientColor()),puff.getAlpha());
		facep->setFaceColor(float_color);
	}
	for ( ; face_indx < drawable->getNumFaces(); face_indx++)
	{
		facep = drawable->getFace(face_indx);
		if (!facep)
		{
			llwarns << "No facep for index " << face_indx << llendl;
			continue;
		}

		facep->setTEOffset(face_indx);
		facep->setSize(0,0);
	}

	drawable->movePartition();

	return TRUE;
}

F32 LLVOClouds::getPartSize(S32 idx)
{
	return (CLOUD_PUFF_HEIGHT+CLOUD_PUFF_WIDTH)*0.5f;
}
void LLVOClouds::getGeometry(S32 idx,
								LLStrider<LLVector4a>& verticesp,
								LLStrider<LLVector3>& normalsp, 
								LLStrider<LLVector2>& texcoordsp,
								LLStrider<LLColor4U>& colorsp, 
								LLStrider<U16>& indicesp)
{

	if (idx >= mCloudGroupp->getNumPuffs())
	{
		return;
	}

	LLDrawable* drawable = mDrawable;
	LLFace *facep = drawable->getFace(idx);

	if (!facep->hasGeometry())
	{
		return;
	}
	

	const LLCloudPuff &puff = mCloudGroupp->getPuff(idx);

	LLColor4 float_color(LLColor3(gSky.getSunDiffuseColor() + gSky.getSunAmbientColor()),puff.getAlpha());
	LLColor4U color;
	color.setVec(float_color);
	facep->setFaceColor(float_color);
	
	U32 vert_offset = facep->getGeomIndex();
		
	LLVector4a part_pos_agent;
	part_pos_agent.load3(facep->mCenterLocal.mV);	
	LLVector4a at;
	at.load3(LLViewerCamera::getInstance()->getAtAxis().mV);
	LLVector4a up(0, 0, 1);
	LLVector4a right;

	right.setCross3(at, up);
	right.normalize3fast();
	up.setCross3(right, at);
	up.normalize3fast();
	right.mul(0.5f*CLOUD_PUFF_WIDTH);
	up.mul(0.5f*CLOUD_PUFF_HEIGHT);
		

	LLVector3 normal(0.f,0.f,-1.f);

	//HACK -- the verticesp->mV[3] = 0.f here are to set the texture index to 0 (particles don't use texture batching, maybe they should)
	// this works because there is actually a 4th float stored after the vertex position which is used as a texture index
	// also, somebody please VECTORIZE THIS

	LLVector4a ppapu;
	LLVector4a ppamu;
	
	ppapu.setAdd(part_pos_agent, up);
	ppamu.setSub(part_pos_agent, up);
	
	verticesp->setSub(ppapu, right);
	(*verticesp++).getF32ptr()[3] = 0.f;
	verticesp->setSub(ppamu, right);
	(*verticesp++).getF32ptr()[3] = 0.f;
	verticesp->setAdd(ppapu, right);
	(*verticesp++).getF32ptr()[3] = 0.f;
	verticesp->setAdd(ppamu, right);
	(*verticesp++).getF32ptr()[3] = 0.f;

	//*verticesp++ = puff_pos_agent - right + up;
	//*verticesp++ = puff_pos_agent - right - up;
	//*verticesp++ = puff_pos_agent + right + up;
	//*verticesp++ = puff_pos_agent + right - up;

	*colorsp++ = color;
	*colorsp++ = color;
	*colorsp++ = color;
	*colorsp++ = color;

	*texcoordsp++ = LLVector2(0.f, 1.f);
	*texcoordsp++ = LLVector2(0.f, 0.f);
	*texcoordsp++ = LLVector2(1.f, 1.f);
	*texcoordsp++ = LLVector2(1.f, 0.f);

	*normalsp++   = normal;
	*normalsp++   = normal;
	*normalsp++   = normal;
	*normalsp++   = normal;

	*indicesp++ = vert_offset + 0;
	*indicesp++ = vert_offset + 1;
	*indicesp++ = vert_offset + 2;

	*indicesp++ = vert_offset + 1;
	*indicesp++ = vert_offset + 3;
	*indicesp++ = vert_offset + 2;
}

U32 LLVOClouds::getPartitionType() const
{
	return LLViewerRegion::PARTITION_CLOUD;
}

// virtual
void LLVOClouds::updateDrawable(BOOL force_damped)
{
	// Force an immediate rebuild on any update
	if (mDrawable.notNull())
	{
		mDrawable->updateXform(TRUE);
		gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_ALL, TRUE);
	}
	clearChanged(SHIFTED);
}

LLCloudPartition::LLCloudPartition()
{
	mDrawableType = LLPipeline::RENDER_TYPE_CLASSIC_CLOUDS;
	mPartitionType = LLViewerRegion::PARTITION_CLOUD;
}

