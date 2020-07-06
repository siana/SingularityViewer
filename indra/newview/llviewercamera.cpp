/** 
 * @file llviewercamera.cpp
 * @brief LLViewerCamera class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llviewercamera.h"
#include "llagent.h"
#include "llagentcamera.h"

#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llvovolume.h"
#include "llworld.h"
#include "lltoolmgr.h"
#include "llviewerjoystick.h"
// [RLVa:KB] - Checked: 2010-04-11 (RLVa-1.2.0e)
#include "rlvhandler.h"
// [/RLVa:KB]

// Linden library includes
#include "lldrawable.h"
#include "llface.h"
#include "llgl.h"
#include "llglheaders.h"
#include "llquaternion.h"
#include "llwindow.h"			// getPixelAspectRatio()

// System includes
#include <iomanip> // for setprecision

LLViewerCamera::eCameraID LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;

LLViewerCamera::LLViewerCamera() : LLCamera()
{
	calcProjection(getFar());
	mCameraFOVDefault = DEFAULT_FIELD_OF_VIEW;
	mSavedFOVDefault = DEFAULT_FIELD_OF_VIEW; // <exodus/>
	mCosHalfCameraFOV = cosf(mCameraFOVDefault * 0.5f);
	mPixelMeterRatio = 0.f;
	mScreenPixelArea = 0;
	mZoomFactor = 1.f;
	mZoomSubregion = 1;
	mAverageSpeed = 0.f;
	mAverageAngularSpeed = 0.f;
	gSavedSettings.getControl("CameraAngle")->getCommitSignal()->connect(boost::bind(&LLViewerCamera::updateCameraAngle, this, _2));
}

void LLViewerCamera::updateCameraLocation(const LLVector3 &center,
											const LLVector3 &up_direction,
											const LLVector3 &point_of_interest)
{
	// do not update if avatar didn't move
	if (!LLViewerJoystick::getInstance()->getCameraNeedsUpdate())
	{
		return;
	}

	LLVector3 last_position;
	LLVector3 last_axis;
	last_position = getOrigin();
	last_axis = getAtAxis();

	mLastPointOfInterest = point_of_interest;

	LLViewerRegion * regp = gAgent.getRegion();
	F32 water_height = (NULL != regp) ? regp->getWaterHeight() : 0.f;

	LLVector3 origin = center;
	if (origin.mV[2] > water_height)
	{
		origin.mV[2] = llmax(origin.mV[2], water_height+0.20f);
	}
	else
	{
		origin.mV[2] = llmin(origin.mV[2], water_height-0.20f);
	}

	setOriginAndLookAt(origin, up_direction, point_of_interest);

	mVelocityDir = center - last_position ; 
	F32 dpos = mVelocityDir.normVec() ;
	LLQuaternion rotation;
	rotation.shortestArc(last_axis, getAtAxis());

	F32 x, y, z;
	F32 drot;
	rotation.getAngleAxis(&drot, &x, &y, &z);

	mVelocityStat.addValue(dpos);
	mAngularVelocityStat.addValue(drot);
	
	mAverageSpeed = mVelocityStat.getMeanPerSec() ;
	mAverageAngularSpeed = mAngularVelocityStat.getMeanPerSec() ;
	mCosHalfCameraFOV = cosf(0.5f * getView() * llmax(1.0f, getAspect()));

	// update pixel meter ratio using default fov, not modified one
	mPixelMeterRatio = getViewHeightInPixels()/ (2.f*tanf(mCameraFOVDefault*0.5));
	// update screen pixel area
	mScreenPixelArea =(S32)((F32)getViewHeightInPixels() * ((F32)getViewHeightInPixels() * getAspect()));
}

const LLMatrix4a &LLViewerCamera::getProjection() const
{
	calcProjection(getFar());
	return mProjectionMatrix;

}

const LLMatrix4a &LLViewerCamera::getModelview() const
{
	LLMatrix4 modelview;
	getMatrixToLocal(modelview);
	LLMatrix4a modelviewa;
	modelviewa.loadu((F32*)modelview.mMatrix);
	mModelviewMatrix.setMul(OGL_TO_CFR_ROTATION,modelviewa);
	return mModelviewMatrix;
}

void LLViewerCamera::calcProjection(const F32 far_distance) const
{
	mProjectionMatrix = gGL.genPersp( getView()*RAD_TO_DEG, getAspect(), getNear(), far_distance );
}

// Sets up opengl state for 3D drawing.  If for selection, also
// sets up a pick matrix.  x and y are ignored if for_selection is false.
// The picking region is centered on x,y and has the specified width and
// height.

//static
void LLViewerCamera::updateFrustumPlanes(LLCamera& camera, BOOL ortho, BOOL zflip, BOOL no_hacks)
{
	LLVector3 frust[8];

	const LLRect& view_port = gGLViewport;

	if (no_hacks)
	{
		gGL.unprojectf(LLVector3(view_port.mLeft,view_port.mBottom,0.f),gGLModelView,gGLProjection,view_port,frust[0]);
		gGL.unprojectf(LLVector3(view_port.mRight,view_port.mBottom,0.f),gGLModelView,gGLProjection,view_port,frust[1]);
		gGL.unprojectf(LLVector3(view_port.mRight,view_port.mTop,0.f),gGLModelView,gGLProjection,view_port,frust[2]);
		gGL.unprojectf(LLVector3(view_port.mLeft,view_port.mTop,0.f),gGLModelView,gGLProjection,view_port,frust[3]);

		gGL.unprojectf(LLVector3(view_port.mLeft,view_port.mBottom,1.f),gGLModelView,gGLProjection,view_port,frust[4]);
		gGL.unprojectf(LLVector3(view_port.mRight,view_port.mBottom,1.f),gGLModelView,gGLProjection,view_port,frust[5]);
		gGL.unprojectf(LLVector3(view_port.mRight,view_port.mTop,1.f),gGLModelView,gGLProjection,view_port,frust[6]);
		gGL.unprojectf(LLVector3(view_port.mLeft,view_port.mTop,1.f),gGLModelView,gGLProjection,view_port,frust[7]);
	}
	else if (zflip)
	{
		gGL.unprojectf(LLVector3(view_port.mLeft,view_port.mTop,0.f),gGLModelView,gGLProjection,view_port,frust[0]);
		gGL.unprojectf(LLVector3(view_port.mRight,view_port.mTop,0.f),gGLModelView,gGLProjection,view_port,frust[1]);
		gGL.unprojectf(LLVector3(view_port.mRight,view_port.mBottom,0.f),gGLModelView,gGLProjection,view_port,frust[2]);
		gGL.unprojectf(LLVector3(view_port.mLeft,view_port.mBottom,0.f),gGLModelView,gGLProjection,view_port,frust[3]);

		gGL.unprojectf(LLVector3(view_port.mLeft,view_port.mTop,1.f),gGLModelView,gGLProjection,view_port,frust[4]);
		gGL.unprojectf(LLVector3(view_port.mRight,view_port.mTop,1.f),gGLModelView,gGLProjection,view_port,frust[5]);
		gGL.unprojectf(LLVector3(view_port.mRight,view_port.mBottom,1.f),gGLModelView,gGLProjection,view_port,frust[6]);
		gGL.unprojectf(LLVector3(view_port.mLeft,view_port.mBottom,1.f),gGLModelView,gGLProjection,view_port,frust[7]);

		for (U32 i = 0; i < 4; i++)
		{
			frust[i+4] = frust[i+4]-frust[i];
			frust[i+4].normVec();
			frust[i+4] = frust[i] + frust[i+4]*camera.getFar();
		}
	}
	else
	{
		gGL.unprojectf(LLVector3(view_port.mLeft,view_port.mBottom,0.f),gGLModelView,gGLProjection,view_port,frust[0]);
		gGL.unprojectf(LLVector3(view_port.mRight,view_port.mBottom,0.f),gGLModelView,gGLProjection,view_port,frust[1]);
		gGL.unprojectf(LLVector3(view_port.mRight,view_port.mTop,0.f),gGLModelView,gGLProjection,view_port,frust[2]);
		gGL.unprojectf(LLVector3(view_port.mLeft,view_port.mTop,0.f),gGLModelView,gGLProjection,view_port,frust[3]);
		
		if (ortho)
		{
			LLVector3 far_shift = camera.getAtAxis()*camera.getFar()*2.f; 
			for (U32 i = 0; i < 4; i++)
			{
				frust[i+4] = frust[i] + far_shift;
			}
		}
		else
		{
			for (U32 i = 0; i < 4; i++)
			{
				LLVector3 vec = frust[i] - camera.getOrigin();
				vec.normVec();
				frust[i+4] = camera.getOrigin() + vec*camera.getFar();
			}
		}
	}

	camera.calcAgentFrustumPlanes(frust);
}

void LLViewerCamera::setPerspective(BOOL for_selection,
									S32 x, S32 y_from_bot, S32 width, S32 height,
									BOOL limit_select_distance,
									F32 z_near, F32 z_far)
{
	F32 fov_y, aspect;
	fov_y = RAD_TO_DEG * getView();
	BOOL z_default_far = FALSE;
	if (z_far <= 0)
	{
		z_default_far = TRUE;
		z_far = getFar();
	}
	if (z_near <= 0)
	{
		z_near = getNear();
	}
	aspect = getAspect();

	// Load camera view matrix
	gGL.matrixMode( LLRender::MM_PROJECTION );
	gGL.loadIdentity();

	LLMatrix4a proj_mat;
	proj_mat.setIdentity();

	if (for_selection)
	{
		// make a tiny little viewport
		// anything drawn into this viewport will be "selected"

		const LLRect& rect = gViewerWindow->getWorldViewRectRaw();
		
		const F32 scale_x = rect.getWidth() / F32(width);
		const F32 scale_y = rect.getHeight() / F32(height);
		const F32 trans_x = scale_x + (2.f * (rect.mLeft - x)) / F32(width) - 1.f;
		const F32 trans_y = scale_y + (2.f * (rect.mBottom - y_from_bot)) / F32(height) - 1.f;

		//Generate a pick matrix
		proj_mat.applyScale_affine(scale_x, scale_y, 1.f);
		proj_mat.setTranslate_affine(LLVector3(trans_x, trans_y, 0.f));

		if (limit_select_distance)
		{
			// ...select distance from control
//			z_far = gSavedSettings.getF32("MaxSelectDistance");
// [RLVa:KB] - Checked: 2010-04-11 (RLVa-1.2.0e) | Added: RLVa-1.2.0e
			z_far = (!gRlvHandler.hasBehaviour(RLV_BHVR_FARTOUCH)) ? gSavedSettings.getF32("MaxSelectDistance") : 1.5;
// [/RLVa:KB]
		}
		else
		{
			z_far = gAgentCamera.mDrawDistance;
		}
	}
	else
	{
		// Only override the far clip if it's not passed in explicitly.
		if (z_default_far)
		{
			z_far = MAX_FAR_CLIP;
		}
		gGLViewport.set(x, y_from_bot + height, x + width, y_from_bot);
		gGL.setViewport(gGLViewport);
	}
	
	if (mZoomFactor > 1.f)
	{
		float offset = mZoomFactor - 1.f;
		int pos_y = mZoomSubregion / llceil(mZoomFactor);
		int pos_x = mZoomSubregion - (pos_y*llceil(mZoomFactor));

		proj_mat.applyTranslation_affine(offset - (F32)pos_x * 2.f, offset - (F32)pos_y * 2.f, 0.f);
		proj_mat.applyScale_affine(mZoomFactor,mZoomFactor,1.f);
	}

	calcProjection(z_far); // Update the projection matrix cache

	proj_mat.mul(gGL.genPersp(fov_y,aspect,z_near,z_far));
	
	gGL.loadMatrix(proj_mat);
	
	gGLProjection = proj_mat;

	gGL.matrixMode(LLRender::MM_MODELVIEW );

	LLMatrix4a ogl_matrix;
	getOpenGLTransform(ogl_matrix.getF32ptr());

	LLMatrix4a modelview;
	modelview.setMul(OGL_TO_CFR_ROTATION, ogl_matrix);
	
	gGL.loadMatrix(modelview);
	
	if (for_selection && (width > 1 || height > 1))
	{
		// NB: as of this writing, i believe the code below is broken (doesn't take into account the world view, assumes entire window)
		// however, it is also unused (the GL matricies are used for selection, (see LLCamera::sphereInFrustum())) and so i'm not
		// comfortable hacking on it.
		calculateFrustumPlanesFromWindow((F32)(x - width / 2) / (F32)gViewerWindow->getWindowWidthScaled() - 0.5f,
								(F32)(y_from_bot - height / 2) / (F32)gViewerWindow->getWindowHeightScaled() - 0.5f,
								(F32)(x + width / 2) / (F32)gViewerWindow->getWindowWidthScaled() - 0.5f,
								(F32)(y_from_bot + height / 2) / (F32)gViewerWindow->getWindowHeightScaled() - 0.5f);

	}

	// if not picking and not doing a snapshot, cache various GL matrices
	if (!for_selection && mZoomFactor == 1.f)
	{
		// Save GL matrices for access elsewhere in code, especially project_world_to_screen
		//glGetDoublev(GL_MODELVIEW_MATRIX, gGLModelView);
		glh_set_current_modelview(modelview);
	}

	updateFrustumPlanes(*this);

	/*if (gSavedSettings.getBOOL("CameraOffset"))
	{
		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.translatef(0,0,-50);
		gGL.rotatef(20.0,1,0,0);
		gGL.matrixMode(LLRender::MM_MODELVIEW);
	}*/
}

// Uses the last GL matrices set in set_perspective to project a point from
// screen coordinates to the agent's region.
void LLViewerCamera::projectScreenToPosAgent(const S32 screen_x, const S32 screen_y, LLVector3* pos_agent) const
{
	gGL.unprojectf(
		LLVector3(screen_x,screen_y,0.f),
		gGLModelView, gGLProjection, gGLViewport,
		*pos_agent );
}

// Uses the last GL matrices set in set_perspective to project a point from
// the agent's region space to screen coordinates.  Returns TRUE if point in within
// the current window.
BOOL LLViewerCamera::projectPosAgentToScreen(const LLVector3 &pos_agent, LLCoordGL &out_point, const BOOL clamp) const
{
	BOOL in_front = TRUE;
	LLVector3 window_coordinates;

	LLVector3 dir_to_point = pos_agent - getOrigin();
	dir_to_point /= dir_to_point.magVec();

	if (dir_to_point * getAtAxis() < 0.f)
	{
		if (clamp)
		{
			return FALSE;
		}
		else
		{
			in_front = FALSE;
		}
	}

	const LLRect& world_view_rect = gViewerWindow->getWorldViewRectRaw();

	if (gGL.projectf(pos_agent, gGLModelView, gGLProjection, world_view_rect, window_coordinates))
	{
		F32 &x = window_coordinates.mV[VX];
		F32 &y = window_coordinates.mV[VY];

		// convert screen coordinates to virtual UI coordinates
		x /= gViewerWindow->getDisplayScale().mV[VX];
		y /= gViewerWindow->getDisplayScale().mV[VY];

		// should now have the x,y coords of grab_point in screen space
		LLRect world_rect = gViewerWindow->getWorldViewRectScaled();

		// ...sanity check
		S32 int_x = lltrunc(x);
		S32 int_y = lltrunc(y);

		BOOL valid = TRUE;

		if (clamp)
		{
			if (int_x < world_rect.mLeft)
			{
				out_point.mX = world_rect.mLeft;
				valid = FALSE;
			}
			else if (int_x > world_rect.mRight)
			{
				out_point.mX = world_rect.mRight;
				valid = FALSE;
			}
			else
			{
				out_point.mX = int_x;
			}

			if (int_y < world_rect.mBottom)
			{
				out_point.mY = world_rect.mBottom;
				valid = FALSE;
			}
			else if (int_y > world_rect.mTop)
			{
				out_point.mY = world_rect.mTop;
				valid = FALSE;
			}
			else
			{
				out_point.mY = int_y;
			}
			return valid;
		}
		else
		{
			out_point.mX = int_x;
			out_point.mY = int_y;

			if (int_x < world_rect.mLeft)
			{
				valid = FALSE;
			}
			else if (int_x > world_rect.mRight)
			{
				valid = FALSE;
			}
			if (int_y < world_rect.mBottom)
			{
				valid = FALSE;
			}
			else if (int_y > world_rect.mTop)
			{
				valid = FALSE;
			}

			return in_front && valid;
		}
	}
	else
	{
		return FALSE;
	}
}

// Uses the last GL matrices set in set_perspective to project a point from
// the agent's region space to the nearest edge in screen coordinates.
// Returns TRUE if projection succeeds.
BOOL LLViewerCamera::projectPosAgentToScreenEdge(const LLVector3 &pos_agent,
												LLCoordGL &out_point) const
{
	LLVector3 dir_to_point = pos_agent - getOrigin();
	dir_to_point /= dir_to_point.magVec();

	BOOL in_front = TRUE;
	if (dir_to_point * getAtAxis() < 0.f)
	{
		in_front = FALSE;
	}

	const LLRect& world_view_rect = gViewerWindow->getWorldViewRectRaw();
	LLVector3 window_coordinates;

	if (gGL.projectf(pos_agent, gGLModelView, gGLProjection, world_view_rect, window_coordinates))
	{
		F32 &x = window_coordinates.mV[VX];
		F32 &y = window_coordinates.mV[VY];

		x /= gViewerWindow->getDisplayScale().mV[VX];
		y /= gViewerWindow->getDisplayScale().mV[VY];
		// should now have the x,y coords of grab_point in screen space
		const LLRect& world_rect = gViewerWindow->getWorldViewRectScaled();

		// ...sanity check
		S32 int_x = lltrunc(x);
		S32 int_y = lltrunc(y);

		// find the center
		GLdouble center_x = (GLdouble)world_rect.getCenterX();
		GLdouble center_y = (GLdouble)world_rect.getCenterY();

		if (x == center_x  &&  y == center_y)
		{
			// can't project to edge from exact center
			return FALSE;
		}

		// find the line from center to local
		GLdouble line_x = x - center_x;
		GLdouble line_y = y - center_y;

		int_x = lltrunc(center_x);
		int_y = lltrunc(center_y);


		if (0.f == line_x)
		{
			// the slope of the line is undefined
			if (line_y > 0.f)
			{
				int_y = world_rect.mTop;
			}
			else
			{
				int_y = world_rect.mBottom;
			}
		}
		else if (0 == world_rect.getWidth())
		{
			// the diagonal slope of the view is undefined
			if (y < world_rect.mBottom)
			{
				int_y = world_rect.mBottom;
			}
			else if ( y > world_rect.mTop)
			{
				int_y = world_rect.mTop;
			}
		}
		else
		{
			F32 line_slope = (F32)(line_y / line_x);
			F32 rect_slope = ((F32)world_rect.getHeight()) / ((F32)world_rect.getWidth());

			if (fabs(line_slope) > rect_slope)
			{
				if (line_y < 0.f)
				{
					// bottom
					int_y = world_rect.mBottom;
				}
				else
				{
					// top
					int_y = world_rect.mTop;
				}
				int_x = lltrunc(((GLdouble)int_y - center_y) / line_slope + center_x);
			}
			else if (fabs(line_slope) < rect_slope)
			{
				if (line_x < 0.f)
				{
					// left
					int_x = world_rect.mLeft;
				}
				else
				{
					// right
					int_x = world_rect.mRight;
				}
				int_y = lltrunc(((GLdouble)int_x - center_x) * line_slope + center_y);
			}
			else
			{
				// exactly parallel ==> push to the corners
				if (line_x > 0.f)
				{
					int_x = world_rect.mRight;
				}
				else
				{
					int_x = world_rect.mLeft;
				}
				if (line_y > 0.0f)
				{
					int_y = world_rect.mTop;
				}
				else
				{
					int_y = world_rect.mBottom;
				}
			}
		}
		if (!in_front)
		{
			int_x = world_rect.mLeft + world_rect.mRight - int_x;
			int_y = world_rect.mBottom + world_rect.mTop - int_y;
		}

		out_point.mX = int_x + world_rect.mLeft;
		out_point.mY = int_y + world_rect.mBottom;
		return TRUE;
	}
	return FALSE;
}


void LLViewerCamera::getPixelVectors(const LLVector3 &pos_agent, LLVector3 &up, LLVector3 &right)
{
	LLVector3 to_vec = pos_agent - getOrigin();

	F32 at_dist = to_vec * getAtAxis();

	F32 height_meters = at_dist* (F32)tan(getView()/2.f);
	F32 height_pixels = getViewHeightInPixels()/2.f;

	F32 pixel_aspect = gViewerWindow->getWindow()->getPixelAspectRatio();

	F32 meters_per_pixel = height_meters / height_pixels;
	up = getUpAxis() * meters_per_pixel * gViewerWindow->getDisplayScale().mV[VY];
	right = -1.f * pixel_aspect * meters_per_pixel * getLeftAxis() * gViewerWindow->getDisplayScale().mV[VX];
}

LLVector3 LLViewerCamera::roundToPixel(const LLVector3 &pos_agent)
{
	F32 dist = (pos_agent - getOrigin()).magVec();
	// Convert to screen space and back, preserving the depth.
	LLCoordGL screen_point;
	if (!projectPosAgentToScreen(pos_agent, screen_point, FALSE))
	{
		// Off the screen, just return the original position.
		return pos_agent;
	}

	LLVector3 ray_dir;

	projectScreenToPosAgent(screen_point.mX, screen_point.mY, &ray_dir);
	ray_dir -= getOrigin();
	ray_dir.normVec();

	LLVector3 pos_agent_rounded = getOrigin() + ray_dir*dist;

	/*
	LLVector3 pixel_x, pixel_y;
	getPixelVectors(pos_agent_rounded, pixel_y, pixel_x);
	pos_agent_rounded += 0.5f*pixel_x, 0.5f*pixel_y;
	*/
	return pos_agent_rounded;
}

BOOL LLViewerCamera::cameraUnderWater() const
{
	if(!gAgent.getRegion())
	{
		return FALSE ;
	}
	return getOrigin().mV[VZ] < gAgent.getRegion()->getWaterHeight();
}

BOOL LLViewerCamera::areVertsVisible(LLViewerObject* volumep, BOOL all_verts)
{
	S32 i, num_faces;
	LLDrawable* drawablep = volumep->mDrawable;

	if (!drawablep)
	{
		return FALSE;
	}

	LLVolume* volume = volumep->getVolume();
	if (!volume)
	{
		return FALSE;
	}

	LLVOVolume* vo_volume = (LLVOVolume*) volumep;

	vo_volume->updateRelativeXform();
	
	LLMatrix4 render_mat(vo_volume->getRenderRotation(), LLVector4(vo_volume->getRenderPosition()));

	LLMatrix4a render_mata;
	render_mata.loadu(render_mat);
	const LLMatrix4a& mata = vo_volume->getRelativeXform();;

	num_faces = volume->getNumVolumeFaces();
	for (i = 0; i < num_faces; i++)
	{
		const LLVolumeFace& face = volume->getVolumeFace(i);
				
		for (U32 v = 0; v < (U32)face.mNumVertices; v++)
		{
			const LLVector4a& src_vec = face.mPositions[v];
			LLVector4a vec;
			mata.affineTransform(src_vec, vec);

			if (drawablep->isActive())
			{
				LLVector4a t = vec;
				render_mata.affineTransform(t, vec);
			}

			BOOL in_frustum = pointInFrustum(LLVector3(vec.getF32ptr())) > 0;

			if (( !in_frustum && all_verts) ||
				 (in_frustum && !all_verts))
			{
				return !all_verts;
			}
		}
	}
	return all_verts;
}

// changes local camera and broadcasts change
/* virtual */ void LLViewerCamera::setView(F32 vertical_fov_rads)
{
	F32 old_fov = getView();

	// cap the FoV
	vertical_fov_rads = llclamp(vertical_fov_rads, getMinView(), getMaxView());

// RLVa:LF - @camzoommax
	if (gRlvHandler.hasBehaviour(RLV_BHVR_CAMZOOMMAX))
		vertical_fov_rads = llmin(vertical_fov_rads, gRlvHandler.camPole(RLV_BHVR_CAMZOOMMAX));
	if (gRlvHandler.hasBehaviour(RLV_BHVR_CAMZOOMMIN))
		vertical_fov_rads = llmax(vertical_fov_rads, gRlvHandler.camPole(RLV_BHVR_CAMZOOMMIN));

	if (vertical_fov_rads == old_fov) return;

	// send the new value to the simulator
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_AgentFOV);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->addU32Fast(_PREHASH_CircuitCode, gMessageSystem->mOurCircuitCode);

	msg->nextBlockFast(_PREHASH_FOVBlock);
	msg->addU32Fast(_PREHASH_GenCounter, 0);
	msg->addF32Fast(_PREHASH_VerticalAngle, vertical_fov_rads);

	gAgent.sendReliableMessage();

	// sync the camera with the new value
	LLCamera::setView(vertical_fov_rads); // call base implementation
}

void LLViewerCamera::setDefaultFOV(F32 vertical_fov_rads) 
{
	vertical_fov_rads = llclamp(vertical_fov_rads, getMinView(), getMaxView());
	setView(vertical_fov_rads);
	mCameraFOVDefault = vertical_fov_rads; 
	mCosHalfCameraFOV = cosf(mCameraFOVDefault * 0.5f);
}

// <exodus>
void LLViewerCamera::loadDefaultFOV()
{
	if (mSavedFOVLoaded) return;
	setView(mSavedFOVDefault);
	mSavedFOVLoaded = true;
	mCameraFOVDefault = mSavedFOVDefault;
	mCosHalfCameraFOV = cosf(mCameraFOVDefault * 0.5f);
}
// </exodus>

// static
void LLViewerCamera::updateCameraAngle( void* user_data, const LLSD& value)
{
	LLViewerCamera* self=(LLViewerCamera*)user_data;
	self->setDefaultFOV(value.asReal());	
}
