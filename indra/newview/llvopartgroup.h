/** 
 * @file llvopartgroup.h
 * @brief Group of particle systems
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

#ifndef LL_LLVOPARTGROUP_H
#define LL_LLVOPARTGROUP_H

#include "llviewerobject.h"
#include "v3math.h"
#include "v3color.h"
#include "llframetimer.h"
#include "llviewerpartsim.h"
#include "llvertexbuffer.h"

class LLViewerPartGroup;

class LLVOPartGroup : public LLAlphaObject
{
public:

	//vertex buffer for holding all particles
	static LLPointer<LLVertexBuffer> sVB;
	static S32 sVBSlotFree[LL_MAX_PARTICLE_COUNT];
	static S32* sVBSlotCursor;
	//static S32 sVBSlotCursor;

	static void initClass();
	static void restoreGL();
	static void destroyGL();
	static S32 findAvailableVBSlot();
	static void freeVBSlot(S32 idx);

	enum
	{
		VERTEX_DATA_MASK =	LLVertexBuffer::MAP_VERTEX |
							LLVertexBuffer::MAP_NORMAL |
							LLVertexBuffer::MAP_TEXCOORD0 |
							LLVertexBuffer::MAP_COLOR |
							LLVertexBuffer::MAP_EMISSIVE |
							LLVertexBuffer::MAP_TEXTURE_INDEX
	};

	LLVOPartGroup(const LLUUID &id, const LLPCode pcode, LLViewerRegion *regionp);

	/*virtual*/ BOOL    isActive() const; // Whether this object needs to do an idleUpdate.
	void idleUpdate(LLAgent &agent, LLWorld &world, const F64 &time);

	virtual F32 getBinRadius();
	virtual void updateSpatialExtents(LLVector4a& newMin, LLVector4a& newMax);
	virtual U32 getPartitionType() const;
	
	/*virtual*/ BOOL lineSegmentIntersect(const LLVector4a& start, const LLVector4a& end,
										  S32 face,
										  BOOL pick_transparent,
										  BOOL pick_rigged,
										  S32* face_hit,
										  LLVector4a* intersection,
										  LLVector2* tex_coord,
										  LLVector4a* normal,
										  LLVector4a* tangent);

	/*virtual*/ void setPixelAreaAndAngle(LLAgent &agent);
	/*virtual*/ void updateTextures();

	/*virtual*/ LLDrawable* createDrawable(LLPipeline *pipeline);
	/*virtual*/ BOOL        updateGeometry(LLDrawable *drawable);
	void		getGeometry(const LLViewerPart& part,							
								LLStrider<LLVector4a>& verticesp);
				
				void		getGeometry(S32 idx,
								LLStrider<LLVector4a>& verticesp,
								LLStrider<LLVector3>& normalsp, 
								LLStrider<LLVector2>& texcoordsp,
								LLStrider<LLColor4U>& colorsp, 
								LLStrider<LLColor4U>& emissivep,
								LLStrider<U16>& indicesp);

	void updateFaceSize(S32 idx) { }
	F32 getPartSize(S32 idx);
	void getBlendFunc(S32 idx, U32& src, U32& dst);
	LLUUID getPartOwner(S32 idx);
	LLUUID getPartSource(S32 idx);

	void setViewerPartGroup(LLViewerPartGroup *part_groupp)		{ mViewerPartGroupp = part_groupp; }
	LLViewerPartGroup* getViewerPartGroup()	{ return mViewerPartGroupp; }

protected:
	~LLVOPartGroup();

	LLViewerPartGroup *mViewerPartGroupp;

	virtual LLVector3 getCameraPosition() const;

};


class LLVOHUDPartGroup : public LLVOPartGroup
{
public:
	LLVOHUDPartGroup(const LLUUID &id, const LLPCode pcode, LLViewerRegion *regionp) : 
	  LLVOPartGroup(id, pcode, regionp)   
	{
	}
protected:
	LLDrawable* createDrawable(LLPipeline *pipeline);
	U32 getPartitionType() const;
	virtual LLVector3 getCameraPosition() const;
};

#endif // LL_LLVOPARTGROUP_H
