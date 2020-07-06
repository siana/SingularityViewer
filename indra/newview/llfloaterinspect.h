/** 
* @file llfloaterinspect.h
* @author Cube
* @date 2006-12-16
* @brief Declaration of class for displaying object attributes
*
* $LicenseInfo:firstyear=2006&license=viewergpl$
* 
* Copyright (c) 2006-2009, Linden Research, Inc.
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

#ifndef LL_LLFLOATERINSPECT_H
#define LL_LLFLOATERINSPECT_H

#include "llavatarname.h"
#include "llfloater.h"
#include "llvoinventorylistener.h"

//class LLTool;
class LLObjectSelection;
class LLScrollListCtrl;
class LLUICtrl;

class LLFloaterInspect : public LLFloater, public LLFloaterSingleton<LLFloaterInspect>, public LLVOInventoryListener
{
	friend class LLUISingleton<LLFloaterInspect, VisibilityPolicy<LLFloater> >;
public:

//	static void show(void* ignored = NULL);
	void onOpen();
	virtual BOOL postBuild();
	void dirty();
	LLUUID getSelectedUUID();
	virtual void draw();
	virtual void refresh();
//	static BOOL isVisible();
	virtual void onFocusReceived();
	void onClickCreatorProfile();
	void onClickOwnerProfile();
	void onClickLastOwnerProfile();
	void onSelectObject();

	LLScrollListCtrl* mObjectList;
protected:
	// protected members
	void setDirty() { mDirty = TRUE; }
	bool mDirty;
	// <edit>
	/*virtual*/ void inventoryChanged(LLViewerObject* obj, LLInventoryObject::object_list_t* inv, S32, void* queue);
	// </edit>

private:
	void onGetOwnerNameCallback(const LLUUID& id);
	void onGetLastOwnerNameCallback(const LLUUID& id); // <edit/>
	void onGetCreatorNameCallback(const LLUUID& id);

	LLFloaterInspect(const LLSD& key);
	virtual ~LLFloaterInspect(void);

	LLSafeHandle<LLObjectSelection> mObjectSelection;
	// <edit>
	std::map<LLUUID,std::pair<U32,U32> > mInventoryNums; //<scripts,total>
	uuid_vec_t mQueue;
	// </edit>
	std::map<LLUUID, boost::signals2::scoped_connection> mOwnerNameCacheConnection;
	std::map<LLUUID, boost::signals2::scoped_connection> mLastOwnerNameCacheConnection; // <edit/>
	std::map<LLUUID, boost::signals2::scoped_connection> mCreatorNameCacheConnection;
};

#endif //LL_LLFLOATERINSPECT_H
