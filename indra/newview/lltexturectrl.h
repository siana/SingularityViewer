/** 
 * @file lltexturectrl.h
 * @author Richard Nelson, James Cook
 * @brief LLTextureCtrl class header file including related functions
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * Second Life Viewer Source Code
 * Copyright (c) 2002-2009, Linden Research, Inc.
 * 
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

#ifndef LL_LLTEXTURECTRL_H
#define LL_LLTEXTURECTRL_H

#include "llcoord.h"
#include "llfloater.h"
#include "llstring.h"
#include "lluictrl.h"
#include "llpermissionsflags.h"

class LLButton;
class LLFloaterTexturePicker;
class LLInventoryItem;
class LLTextBox;
class LLViewBorder;
class LLViewerFetchedTexture;

// used for setting drag & drop callbacks.
typedef boost::function<BOOL (LLUICtrl*, LLInventoryItem*)> drag_n_drop_callback;
typedef boost::function<void (LLInventoryItem*)> texture_selected_callback;


//////////////////////////////////////////////////////////////////////////////////////////
// LLTextureCtrl


class LLTextureCtrl
: public LLUICtrl
{
public:
	typedef enum e_texture_pick_op
	{
		TEXTURE_CHANGE,
		TEXTURE_SELECT,
		TEXTURE_CANCEL
	} ETexturePickOp;

public:
	LLTextureCtrl(
		const std::string& name, const LLRect& rect,
		const std::string& label,
		const LLUUID& image_id,
		const LLUUID& default_image_id, 
		const std::string& default_image_name );
	virtual ~LLTextureCtrl();

	// LLView interface
	virtual LLXMLNodePtr getXML(bool save_children = true) const;
	static LLView* fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory);

	virtual BOOL	handleMouseDown(S32 x, S32 y, MASK mask);
	virtual BOOL	handleDragAndDrop(S32 x, S32 y, MASK mask,
						BOOL drop, EDragAndDropType cargo_type, void *cargo_data,
						EAcceptance *accept,
						std::string& tooltip_msg);
	virtual BOOL	handleHover(S32 x, S32 y, MASK mask);
	virtual BOOL	handleUnicodeCharHere(llwchar uni_char);

	virtual void	draw();
	virtual void	setVisible( BOOL visible );
	virtual void	setEnabled( BOOL enabled );

	virtual BOOL	isDirty() const;
	virtual void	resetDirty();

	void			setValid(BOOL valid);

	// LLUICtrl interface
	virtual void	clear();

	// Takes a UUID, wraps get/setImageAssetID
	virtual void	setValue(const LLSD& value );
	virtual LLSD	getValue() const;

	// LLTextureCtrl interface
	void			showPicker(BOOL take_focus);
	void			setLabel(const std::string& label);
	const std::string&	getLabel() const							{ return mLabel; }

	void			setAllowNoTexture( BOOL b )					{ mAllowNoTexture = b; }
	bool			getAllowNoTexture() const					{ return mAllowNoTexture; }

	void			setAllowInvisibleTexture(BOOL b)			{ mAllowInvisibleTexture = b; }
	bool			getAllowInvisibleTexture() const			{ return mAllowInvisibleTexture; }

	const LLUUID&	getImageItemID() { return mImageItemID; }

	void			setImageAssetID(const LLUUID &image_asset_id);
	const LLUUID&	getImageAssetID() const						{ return mImageAssetID; }

	void			setDefaultImageAssetID( const LLUUID& id )	{ mDefaultImageAssetID = id; }
	const LLUUID&	getDefaultImageAssetID() const { return mDefaultImageAssetID; }

	const std::string&	getDefaultImageName() const					{ return mDefaultImageName; }

	void			setBlankImageAssetID(const LLUUID& id);
	const LLUUID&	getBlankImageAssetID() const;

	void			setFallbackImageName( const std::string& name ) { mFallbackImageName = name; }			
	const std::string& 	getFallbackImageName() const { return mFallbackImageName; }	   

	void			setCaption(const std::string& caption);
	void			setCanApplyImmediately(BOOL b);

	void			setCanApply(bool can_preview, bool can_apply);

	void			setImmediateFilterPermMask(PermissionMask mask)
					{ mImmediateFilterPermMask = mask; }
	void			setDnDFilterPermMask(PermissionMask mask)
						{ mDnDFilterPermMask = mask; }
	void			setNonImmediateFilterPermMask(PermissionMask mask)
					{ mNonImmediateFilterPermMask = mask; }
	PermissionMask	getImmediateFilterPermMask() { return mImmediateFilterPermMask; }
	PermissionMask	getNonImmediateFilterPermMask() { return mNonImmediateFilterPermMask; }

	void			closeFloater();

	void			onFloaterClose();
	void			onFloaterCommit(ETexturePickOp op);
	void			onFloaterCommit(ETexturePickOp op, LLUUID id); // tag: vaa emerald local_asset_browser

	bool			canChange() { return mEnable; } // <edit/>

	// This call is returned when a drag is detected. Your callback
	// should return TRUE if the drag is acceptable.
	void setDragCallback(drag_n_drop_callback cb)	{ mDragCallback = cb; }

	// This callback is called when the drop happens. Return TRUE if
	// the drop happened - resulting in an on commit callback, but not
	// necessariliy any other change.
	void setDropCallback(drag_n_drop_callback cb)	{ mDropCallback = cb; }

	void setOnCancelCallback(commit_callback_t cb)	{ mOnCancelCallback = cb; }
	void setOnCloseCallback(commit_callback_t cb)	{ mOnCloseCallback = cb; }
	void setOnSelectCallback(commit_callback_t cb)	{ mOnSelectCallback = cb; }

	/*
	 * callback for changing texture selection in inventory list of texture floater
	 */
	void setOnTextureSelectedCallback(texture_selected_callback cb);

	void setShowLoadingPlaceholder(BOOL showLoadingPlaceholder);

	LLViewerFetchedTexture* getTexture() { return mTexturep; }

	static void handleClickOpenTexture(void* userdata);
	static void handleClickCopyAssetID(void* userdata);

private:
	BOOL allowDrop(LLInventoryItem* item);
	BOOL doDrop(LLInventoryItem* item);

private:
	drag_n_drop_callback	 mDragCallback;
	drag_n_drop_callback	 mDropCallback;
	commit_callback_t		 mOnCancelCallback;
	commit_callback_t		 mOnSelectCallback;
	commit_callback_t		 	mOnCloseCallback;
	texture_selected_callback	mOnTextureSelectedCallback;
	LLPointer<LLViewerFetchedTexture> mTexturep;
	LLColor4				 mBorderColor;
	LLUUID					 mImageItemID;
	LLUUID					 mImageAssetID;
	LLUUID					 mDefaultImageAssetID;
	std::string				 mFallbackImageName;
	std::string				 mDefaultImageName;
	LLHandle<LLFloater>			 mFloaterHandle;
	LLTextBox*				 mTentativeLabel;
	LLTextBox*				 mCaption;
	std::string				 mLabel;
	BOOL					 mAllowNoTexture; // If true, the user can select "none" as an option
	BOOL					 mAllowInvisibleTexture; // If true, the user can select "Invisible" as an option
	LLCoordGL				 mLastFloaterLeftTop;
	PermissionMask			 mImmediateFilterPermMask;
	PermissionMask				mDnDFilterPermMask;
	PermissionMask			 mNonImmediateFilterPermMask;
	BOOL					 mCanApplyImmediately;
	BOOL					 mNeedsRawImageData;
	LLViewBorder*			 mBorder;
	BOOL					 mValid;
	BOOL					 mDirty;
	BOOL					 mShowLoadingPlaceholder;
	// <edit>
	BOOL					 mEnable;
	// </edit>
	std::string				 mLoadingPlaceholderString;
};

// XUI HACK: When floaters converted, switch this file to lltexturepicker.h/cpp
// and class to LLTexturePicker
#define LLTexturePicker LLTextureCtrl

#endif  // LL_LLTEXTURECTRL_H
