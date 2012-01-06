/**
 * @file llfloatermodeluploadbase.h
 * @brief LLFloaterUploadModelBase class declaration
 *
 * $LicenseInfo:firstyear=2011&license=viewergpl$
 * 
 * Copyright (c) 2011, Linden Research, Inc.
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

#ifndef LL_LLFLOATERMODELUPLOADBASE_H
#define LL_LLFLOATERMODELUPLOADBASE_H

#include "lluploadfloaterobservers.h"

class LLFloaterModelUploadBase
:	public LLFloater,
	public LLUploadPermissionsObserver,
	public LLWholeModelFeeObserver,
	public LLWholeModelUploadObserver
{
public:
	LLFloaterModelUploadBase(const LLSD& key);

	virtual ~LLFloaterModelUploadBase(){};

	virtual void setPermissonsErrorStatus(U32 status, const std::string& reason) = 0;

	virtual void onPermissionsReceived(const LLSD& result) = 0;

	virtual void onModelPhysicsFeeReceived(const LLSD& result, std::string upload_url) = 0;

	virtual void setModelPhysicsFeeErrorStatus(U32 status, const std::string& reason) = 0;

	virtual void onModelUploadSuccess() {};

	virtual void onModelUploadFailure() {};

protected:
	// requests agent's permissions to upload model
	void requestAgentUploadPermissions();

	std::string mUploadModelUrl;
	bool mHasUploadPerm;
};

#endif /* LL_LLFLOATERMODELUPLOADBASE_H */
