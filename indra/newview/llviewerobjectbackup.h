/** 
 * @file llviewerobjectbackup.h
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
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

#ifndef LL_LLVIEWEROBJECTBACKUP_H
#define LL_LLVIEWEROBJECTBACKUP_H

#include <deque>
#include <string>
#include <vector>

#include "boost/unordered_map.hpp"
#include "boost/unordered_set.hpp"

#include "llfloater.h"
#include "statemachine/aifilepicker.h"
#include "lluuid.h"

#include "llviewerinventory.h"
#include "llviewerobject.h"

class LLSelectNode;
class LLViewerObject;

enum export_states {
	EXPORT_INIT,
	EXPORT_CHECK_PERMS,
	EXPORT_FETCH_PHYSICS,
	EXPORT_STRUCTURE,
	EXPORT_TEXTURES,
	EXPORT_LLSD,
	EXPORT_DONE,
	EXPORT_FAILED,
	EXPORT_ABORTED
};

class LLObjectBackup : public LLFloater,
					   public LLFloaterSingleton<LLObjectBackup>
{
	friend class LLUISingleton<LLObjectBackup, VisibilityPolicy<LLFloater> >;

protected:
	LOG_CLASS(LLObjectBackup);

public:
	///////////////////////////////////////////////////////////////////////////
	// LLFloater interface

	/*virtual*/ ~LLObjectBackup();
	/*virtual*/ void onClose(bool app_quitting);

	///////////////////////////////////////////////////////////////////////////
	// Public interface for invoking the object backup feature

	// Import entry point
	void importObject(bool upload = false);
	void importObject_continued(AIFilePicker* filepicker);

	// Export entry point
	void exportObject();
	void exportObject_continued(AIFilePicker* filepicker);

	///////////////////////////////////////////////////////////////////////////
	// Public methods used in callbacks, workers and responders

	// Update map from texture worker
	void updateMap(LLUUID uploaded_asset);

	// Move to next texture upload, called by the agent inventory responder
	void uploadNextAsset();

	// Export idle callback
	static void exportWorker(void *userdata);

	// Prim updated callback, called in llviewerobjectlist.cpp
	static void primUpdate(LLViewerObject* object);

	// New prim call back, called in llviewerobjectlist.cpp
	static void newPrim(LLViewerObject* object);

	// Folder public getter, used by the texture cache responder
	std::string getFolder() { return mFolder; }


	static void setDefaultTextures();

	// Permissions checking
	static bool validatePerms(const LLPermissions* item_permissions);
	static bool validateTexturePerms(const LLUUID& asset_id);
	static bool validateNode(LLSelectNode* node);

private:
	// Open only via the importObject() and exportObject() methods defined
	// above.
	LLObjectBackup(const LLSD&);

	void showFloater(bool exporting);

	static bool confirmCloseCallback(const LLSD& notification,
									 const LLSD& response);

	// Update the floater with status numbers
	void updateImportNumbers();
	void updateExportNumbers();

	// Permissions stuff.
	LLUUID validateTextureID(const LLUUID& asset_id);

	// Convert a selection list of objects to LLSD
	LLSD primsToLLSD(LLViewerObject::child_list_t child_list,
					 bool is_attachment);

	// Start the import process
	void importFirstObject();

	// Move to the next import group
	void importNextObject();

	// Export the next texture in list
	void exportNextTexture();

	// Apply LLSD to object
	void xmlToPrim(LLSD prim_llsd, LLViewerObject* object);

	// Rez a prim at a given position
	void rezAgentOffset(LLVector3 offset);	

	// Get an offset from the agent based on rotation and current pos
	LLVector3 offsetAgent(LLVector3 offset);

public:
	// Public static constants, used in callbacks, workers and responders
	static const U32 TEXTURE_OK = 0x00;
	static const U32 TEXTURE_BAD_PERM = 0x01;
	static const U32 TEXTURE_MISSING = 0x02;
	static const U32 TEXTURE_BAD_ENCODING = 0x04;
	static const U32 TEXTURE_IS_NULL = 0x08;
	static const U32 TEXTURE_SAVED_FAILED = 0x10;

	// Export state machine
	enum export_states mExportState;

	// Export result flags for textures.
	U32 mNonExportedTextures;

	// Set when the region supports the extra physics flags
	bool mGotExtraPhysics;

	// Are we ready to check for next texture?
	bool mCheckNextTexture;

private:
	// Are we active flag
	bool mRunning;

	// True if we need to rebase the assets
	bool mRetexture;

	// Counts of import and export objects and prims
	U32 mObjects;
	U32 mCurObject;
	U32 mPrims;
	U32 mCurPrim;

	// Number of rezzed prims
	U32 mRezCount;

	// Root pos and rotation and central root pos for link set
	LLVector3 mRootPos;
	LLQuaternion mRootRot;
	LLVector3 mRootRootPos;
	LLVector3 mGroupOffset;

	// Agent inital pos and rot when starting import
	LLVector3 mAgentPos;
	LLQuaternion mAgentRot;

	LLUUID mCurrentAsset;
	LLUUID mExpectingUpdate;

	// Working llsd iterators for objects and linksets
	LLSD::map_const_iterator mPrimImportIter;
	LLSD::array_const_iterator mGroupPrimImportIter;

	// File and folder name control
	std::string mFileName;
	std::string mFolder;

	// Export texture list
	typedef uuid_set_t textures_set_t;
	textures_set_t mTexturesList;
	textures_set_t mBadPermsTexturesList;

	// Rebase map
	boost::unordered_map<LLUUID, LLUUID> mAssetMap;

	// Import object tracking
	std::vector<LLViewerObject*> mToSelect;
	std::vector<LLViewerObject*>::iterator mProcessIter;

	// Working LLSD holders
	LLSD mLLSD;
	LLSD mThisGroup;
};

extern LLUUID LL_TEXTURE_PLYWOOD;
extern LLUUID LL_TEXTURE_BLANK;
extern LLUUID LL_TEXTURE_INVISIBLE;
extern LLUUID LL_TEXTURE_TRANSPARENT;
extern LLUUID LL_TEXTURE_MEDIA;

#endif	// LL_LLVIEWEROBJECTBACKUP_H
