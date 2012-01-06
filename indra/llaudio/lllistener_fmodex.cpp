/** 
 * @file listener_fmod.cpp
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
#include "lllistener_fmodex.h"
#include "fmod.hpp"

//-----------------------------------------------------------------------
// constructor
//-----------------------------------------------------------------------
LLListener_FMODEX::LLListener_FMODEX(FMOD::System *system)
{
	mSystem = system;
	init();
}

//-----------------------------------------------------------------------
LLListener_FMODEX::~LLListener_FMODEX()
{
}

//-----------------------------------------------------------------------
void LLListener_FMODEX::init(void)
{
	// do inherited
	LLListener::init();
	mDopplerFactor = 1.0f;
	mRolloffFactor = 1.0f;
}

//-----------------------------------------------------------------------
void LLListener_FMODEX::translate(LLVector3 offset)
{
	LLListener::translate(offset);

	mSystem->set3DListenerAttributes(0, (FMOD_VECTOR*)mPosition.mV, NULL, (FMOD_VECTOR*)mListenAt.mV, (FMOD_VECTOR*)mListenUp.mV);
}

//-----------------------------------------------------------------------
void LLListener_FMODEX::setPosition(LLVector3 pos)
{
	LLListener::setPosition(pos);

	mSystem->set3DListenerAttributes(0, (FMOD_VECTOR*)mPosition.mV, NULL, (FMOD_VECTOR*)mListenAt.mV, (FMOD_VECTOR*)mListenUp.mV);
}

//-----------------------------------------------------------------------
void LLListener_FMODEX::setVelocity(LLVector3 vel)
{
	LLListener::setVelocity(vel);

	mSystem->set3DListenerAttributes(0, NULL, (FMOD_VECTOR*)mVelocity.mV, (FMOD_VECTOR*)mListenAt.mV, (FMOD_VECTOR*)mListenUp.mV);
}

//-----------------------------------------------------------------------
void LLListener_FMODEX::orient(LLVector3 up, LLVector3 at)
{
	LLListener::orient(up, at);

	// Welcome to the transition between right and left
	// (coordinate systems, that is)
	// Leaving the at vector alone results in a L/R reversal
	// since DX is left-handed and we (LL, OpenGL, OpenAL) are right-handed
	at = -at;

	mSystem->set3DListenerAttributes(0, NULL, NULL, (FMOD_VECTOR*)at.mV, (FMOD_VECTOR*)up.mV);
}

//-----------------------------------------------------------------------
void LLListener_FMODEX::commitDeferredChanges()
{
	mSystem->update();
}


void LLListener_FMODEX::setRolloffFactor(F32 factor)
{
	mRolloffFactor = factor;
	mSystem->set3DSettings(mDopplerFactor, 1.f, mRolloffFactor);
}


F32 LLListener_FMODEX::getRolloffFactor()
{
	return mRolloffFactor;
}


void LLListener_FMODEX::setDopplerFactor(F32 factor)
{
	mDopplerFactor = factor;
	mSystem->set3DSettings(mDopplerFactor, 1.f, mRolloffFactor);
}


F32 LLListener_FMODEX::getDopplerFactor()
{
	return mDopplerFactor;
}


