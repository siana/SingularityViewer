/** 
 * @file listener_fmodstudio.cpp
 * @brief implementation of LISTENER class abstracting the audio
 * support as a FMOD 3D implementation (windows only)
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#include "linden_common.h"
#include "llaudioengine.h"
#include "lllistener_fmodstudio.h"
#include "fmod.hpp"

//-----------------------------------------------------------------------
// constructor
//-----------------------------------------------------------------------
LLListener_FMODSTUDIO::LLListener_FMODSTUDIO(FMOD::System *system) 
	: LLListener(),
	mDopplerFactor(1.0f),
	mRolloffFactor(1.0f)
{
	mSystem = system;
}

//-----------------------------------------------------------------------
LLListener_FMODSTUDIO::~LLListener_FMODSTUDIO()
{
}

//-----------------------------------------------------------------------
void LLListener_FMODSTUDIO::translate(LLVector3 offset)
{
	LLListener::translate(offset);

	mSystem->set3DListenerAttributes(0, (FMOD_VECTOR*)mPosition.mV, NULL, NULL, NULL);
}

//-----------------------------------------------------------------------
void LLListener_FMODSTUDIO::setPosition(LLVector3 pos)
{
	LLListener::setPosition(pos);

	mSystem->set3DListenerAttributes(0, (FMOD_VECTOR*)mPosition.mV, NULL, NULL, NULL);
}

//-----------------------------------------------------------------------
void LLListener_FMODSTUDIO::setVelocity(LLVector3 vel)
{
	LLListener::setVelocity(vel);

	mSystem->set3DListenerAttributes(0, NULL, (FMOD_VECTOR*)mVelocity.mV, NULL, NULL);
}

//-----------------------------------------------------------------------
void LLListener_FMODSTUDIO::orient(LLVector3 up, LLVector3 at)
{
	LLListener::orient(up, at);

	mSystem->set3DListenerAttributes(0, NULL, NULL, (FMOD_VECTOR*)at.mV, (FMOD_VECTOR*)up.mV);
}

//-----------------------------------------------------------------------
void LLListener_FMODSTUDIO::commitDeferredChanges()
{
	if(!mSystem)
	{
		return;
	}

	mSystem->update();
}


void LLListener_FMODSTUDIO::setRolloffFactor(F32 factor)
{
	//An internal FMOD Studio optimization skips 3D updates if there have not been changes to the 3D sound environment.
	//Sadly, a change in rolloff is not accounted for, thus we must touch the listener properties as well.
	//In short: Changing the position ticks a dirtyflag inside fmod studio, which makes it not skip 3D processing next update call.
	if(mRolloffFactor != factor)
	{
		LLVector3 tmp_pos = mPosition - LLVector3(0.f,0.f,.1f);
		mSystem->set3DListenerAttributes(0, (FMOD_VECTOR*) tmp_pos.mV, NULL, NULL, NULL);
		mSystem->set3DListenerAttributes(0, (FMOD_VECTOR*) mPosition.mV, NULL, NULL, NULL);
	}
	mRolloffFactor = factor;
	mSystem->set3DSettings(mDopplerFactor, 1.f, mRolloffFactor);
}


F32 LLListener_FMODSTUDIO::getRolloffFactor()
{
	return mRolloffFactor;
}


void LLListener_FMODSTUDIO::setDopplerFactor(F32 factor)
{
	mDopplerFactor = factor;
	mSystem->set3DSettings(mDopplerFactor, 1.f, mRolloffFactor);
}


F32 LLListener_FMODSTUDIO::getDopplerFactor()
{
	return mDopplerFactor;
}


