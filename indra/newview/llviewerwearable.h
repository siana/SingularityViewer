/** 
 * @file llviewerwearable.h
 * @brief LLViewerWearable class header file
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
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

#ifndef LL_VIEWER_WEARABLE_H
#define LL_VIEWER_WEARABLE_H

#include "llwearable.h"
#include "llavatarappearancedefines.h"

class LLVOAvatar;
class LLAPRFile;

class LLViewerWearable : public LLWearable
{
	friend class LLWearableList;

	//--------------------------------------------------------------------
	// Constructors and destructors
	//--------------------------------------------------------------------
private:
	// Private constructors used by LLWearableList
	LLViewerWearable(const LLTransactionID& transactionID);
	LLViewerWearable(const LLAssetID& assetID);
public:
	virtual ~LLViewerWearable();

	//--------------------------------------------------------------------
	// Accessors
	//--------------------------------------------------------------------
public:
	const LLUUID&				getItemID() const { return mItemID; }
	const LLAssetID&			getAssetID() const { return mAssetID; }
	const LLTransactionID&		getTransactionID() const { return mTransactionID; }
	void						setItemID(const LLUUID& item_id);

public:

	BOOL				isDirty() const;
	BOOL				isOldVersion() const;

	/*virtual*/ void	writeToAvatar(LLAvatarAppearance *avatarp);
	void				removeFromAvatar( bool upload_bake = false )	{ LLViewerWearable::removeFromAvatar( mType, upload_bake ); }
	static void			removeFromAvatar( LLWearableType::EType type, bool upload_bake = false); 

	/*virtual*/ EImportResult	importStream( std::istream& input_stream, LLAvatarAppearance* avatarp );

	// Singu extension.
	class AIArchetype getArchetype() const;

	void				setParamsToDefaults();
	void				setTexturesToDefaults();

	/*virtual*/ const LLUUID		getDefaultTextureImageID(LLAvatarAppearanceDefines::ETextureIndex index) const;


	void				saveNewAsset() const;
	static void			onSaveNewAssetComplete( const LLUUID& asset_uuid, void* user_data, S32 status, LLExtStat ext_status );

	void				copyDataFrom(const LLViewerWearable* src);

	friend std::ostream& operator<<(std::ostream &s, const LLViewerWearable &w);

	/*virtual*/ void				revertValues();
	/*virtual*/ void				saveValues();

	// Something happened that requires the wearable's label to be updated (e.g. worn/unworn).
	void				setUpdated() const;

	// the wearable was worn. make sure the name and description of the wearable object matches the LLViewerInventoryItem,
	// not the wearable asset itself.
	void				refreshNameAndDescription();

	// Update the baked texture hash.
	/*virtual*/void		addToBakedTextureHash(LLMD5& hash) const;

protected:
	LLAssetID			mAssetID;
	LLTransactionID		mTransactionID;

	LLUUID				mItemID;  // ID of the inventory item in the agent's inventory	
};


#endif  // LL_VIEWER_WEARABLE_H
