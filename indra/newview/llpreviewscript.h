/** 
 * @file llpreviewscript.h
 * @brief LLPreviewScript class definition
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

#ifndef LL_LLPREVIEWSCRIPT_H
#define LL_LLPREVIEWSCRIPT_H

#include "lldarray.h"
#include "llpreview.h"
#include "lltabcontainer.h"
#include "llinventory.h"
#include "llcombobox.h"
#include "lliconctrl.h"
#include "llframetimer.h"

class LLLiveLSLFile;
class LLMessageSystem;
class LLTextEditor;
class LLButton;
class LLCheckBoxCtrl;
class LLScrollListCtrl;
class LLViewerObject;
struct 	LLEntryAndEdCore;
class LLMenuBarGL;
class LLKeywordToken;
class AIFilePicker;
class LLScriptEdContainer;

// Inner, implementation class.  LLPreviewScript and LLLiveLSLEditor each own one of these.
class LLScriptEdCore : public LLPanel
{
	friend class LLPreviewScript;
	friend class LLPreviewLSL;
	friend class LLLiveLSLEditor;
	friend class LLFloaterSearchReplace;
	friend class LLScriptEdContainer;

public:
	static void parseFunctions(const std::string& filename);

protected:
	// Supposed to be invoked only by the container.
	LLScriptEdCore(
		LLScriptEdContainer* container,
		const std::string& sample,
		const LLHandle<LLFloater>& floater_handle,
		void (*load_callback)(void* userdata),
		void (*save_callback)(void* userdata, BOOL close_after_save),
		void (*search_replace_callback)(void* userdata),
		void* userdata,
		S32 bottom_pad = 0);	// pad below bottom row of buttons
public:
	~LLScriptEdCore();
	
	void			initMenu();

	virtual void	draw();
	/*virtual*/	BOOL	postBuild();
	BOOL			canClose();
	void			setEnableEditing(bool enable);

	void            setScriptText(const std::string& text, BOOL is_valid);
	bool			loadScriptText(const std::string& filename);
	bool			writeToFile(const std::string& filename);
	void			sync();

	void			doSave( BOOL close_after_save );

	bool			handleSaveChangesDialog(const LLSD& notification, const LLSD& response);
	bool			handleReloadFromServerDialog(const LLSD& notification, const LLSD& response);

	void			openInExternalEditor();

	static void		onCheckLock(LLUICtrl*, void*);
	static void		onHelpComboCommit(LLUICtrl* ctrl, void* userdata);
	static void		onClickBack(void* userdata);
	static void		onClickForward(void* userdata);
	static void		onBtnInsertSample(void*);
	static void		onBtnInsertFunction(LLUICtrl*, void*);

	// Singu TODO: modernize the menu callbacks and get rid of/update this giant block of static functions
	static BOOL		hasChanged(void* userdata);
	static void		onBtnSave(void*);
	static void		onBtnUndoChanges(void*);
	static void		onSearchMenu(void* userdata);
	
	static void		onUndoMenu(void* userdata);
	static void		onRedoMenu(void* userdata);
	static void		onCutMenu(void* userdata);
	static void		onCopyMenu(void* userdata);
	static void		onPasteMenu(void* userdata);
	static void		onSelectAllMenu(void* userdata);
	static void		onDeselectMenu(void* userdata);

	static BOOL		enableUndoMenu(void* userdata);
	static BOOL		enableRedoMenu(void* userdata);
	static BOOL		enableCutMenu(void* userdata);
	static BOOL		enableCopyMenu(void* userdata);
	static BOOL		enablePasteMenu(void* userdata);
	static BOOL		enableSelectAllMenu(void* userdata);
	static BOOL		enableDeselectMenu(void* userdata);

private:
	static bool		onHelpWebDialog(const LLSD& notification, const LLSD& response);
	static void		onBtnHelp(void* userdata);
	static void		onBtnDynamicHelp(void* userdata);
	void		onBtnUndoChanges();

	bool		hasChanged();

	void selectFirstError();

	virtual BOOL handleKeyHere(KEY key, MASK mask);
	
	void enableSave(BOOL b) {mEnableSave = b;}

protected:
	void deleteBridges();
	void setHelpPage(const std::string& help_string);
	void updateDynamicHelp(BOOL immediate = FALSE);
	void addHelpItemToHistory(const std::string& help_string);
	static void onErrorList(LLUICtrl*, void* user_data);

private:
	std::string		mSampleText;
	LLTextEditor*	mEditor;
	void			(*mLoadCallback)(void* userdata);
	void			(*mSaveCallback)(void* userdata, BOOL close_after_save);
	void			(*mSearchReplaceCallback) (void* userdata);
	void*			mUserdata;
	LLComboBox		*mFunctions;
	BOOL			mForceClose;
	LLPanel*		mCodePanel;
	LLScrollListCtrl* mErrorList;
	LLDynamicArray<LLEntryAndEdCore*> mBridges;
	LLHandle<LLFloater>	mLiveHelpHandle;
	LLKeywordToken* mLastHelpToken;
	LLFrameTimer	mLiveHelpTimer;
	S32				mLiveHelpHistorySize;
	BOOL			mEnableSave;
	BOOL			mHasScriptData;
	LLLiveLSLFile*	mLiveFile;

	LLScriptEdContainer* mContainer; // parent view

	struct LSLFunctionProps
	{
		std::string mName;
		F32 mSleepTime;
		bool mGodOnly;
	};
	static std::vector<LSLFunctionProps> mParsedFunctions;
};

class LLScriptEdContainer : public LLPreview
{
	friend class LLScriptEdCore;
	friend class LLMultiPreview;

public:
	LLScriptEdContainer(const std::string& name, const LLRect& rect, const std::string& title, const LLUUID& item_id, const LLUUID& object_id = LLUUID::null);

protected:
	std::string		getTmpFileName();
	bool			onExternalChange(const std::string& filename);
	virtual void	saveIfNeeded(bool sync = true) = 0;

	LLTextEditor* getEditor() { return mScriptEd->mEditor; }
	/*virtual*/ const char *getTitleName() const { return "Script"; }
	/*virtual*/ void reshape(S32 width, S32 height, BOOL called_from_parent = TRUE);
	// <edit>
	/*virtual*/ BOOL canSaveAs() const;
	/*virtual*/ void saveAs();
	void saveAs_continued(AIFilePicker* filepicker);
	// </edit>

	LLScriptEdCore*		mScriptEd;
};

// Used to view and edit a LSL from your inventory.
class LLPreviewLSL : public LLScriptEdContainer
{
public:
	LLPreviewLSL(const std::string& name, const LLRect& rect, const std::string& title, const LLUUID& item_uuid );
	virtual void callbackLSLCompileSucceeded();
	virtual void callbackLSLCompileFailed(const LLSD& compile_errors);

	/*virtual*/ BOOL postBuild();

protected:
	virtual BOOL canClose();
	void closeIfNeeded();

	virtual void loadAsset();
	/*virtual*/ void saveIfNeeded(bool sync = true);
	void uploadAssetViaCaps(const std::string& url,
							const std::string& filename, 
							const LLUUID& item_id);
#if 0 //Client side compiling disabled.
	void uploadAssetLegacy(const std::string& filename,
							const LLUUID& item_id,
							const LLTransactionID& tid);
#endif

	static void onSearchReplace(void* userdata);
	static void onLoad(void* userdata);
	static void onSave(void* userdata, BOOL close_after_save);
	
	static void onLoadComplete(LLVFS *vfs, const LLUUID& uuid,
							   LLAssetType::EType type,
							   void* user_data, S32 status, LLExtStat ext_status);
#if 0 //Client side compiling disabled.
	static void onSaveComplete(const LLUUID& uuid, void* user_data, S32 status, LLExtStat ext_status);
	static void onSaveBytecodeComplete(const LLUUID& asset_uuid, void* user_data, S32 status, LLExtStat ext_status);
#endif

protected:
	static void* createScriptEdPanel(void* userdata);


protected:

	// Can safely close only after both text and bytecode are uploaded
	S32 mPendingUploads;

};


// Used to view and edit an LSL that is attached to an object.
class LLLiveLSLEditor : public LLScriptEdContainer
{
	friend class LLLiveLSLFile;
public: 
	LLLiveLSLEditor(const std::string& name, const LLRect& rect, const std::string& title, const LLUUID& object_id, const LLUUID& item_id);


	static void processScriptRunningReply(LLMessageSystem* msg, void**);

	virtual void callbackLSLCompileSucceeded(const LLUUID& task_id,
											const LLUUID& item_id,
											bool is_script_running);
	virtual void callbackLSLCompileFailed(const LLSD& compile_errors);

	/*virtual*/ BOOL postBuild();

	void setIsNew() { mIsNew = TRUE; }

private:
	virtual BOOL canClose();
	void closeIfNeeded();
	virtual void draw();

	virtual void loadAsset();
	void loadAsset(BOOL is_new);
	/*virtual*/ void saveIfNeeded(bool sync = true);
	void uploadAssetViaCaps(const std::string& url,
							const std::string& filename, 
							const LLUUID& task_id,
							const LLUUID& item_id,
							BOOL is_running);
#if 0 //Client side compiling disabled.
	void uploadAssetLegacy(const std::string& filename,
						   LLViewerObject* object,
						   const LLTransactionID& tid,
						   BOOL is_running);
#endif
	BOOL monoChecked() const;


	static void onSearchReplace(void* userdata);
	static void onLoad(void* userdata);
	static void onSave(void* userdata, BOOL close_after_save);

	static void onLoadComplete(LLVFS *vfs, const LLUUID& asset_uuid,
							   LLAssetType::EType type,
							   void* user_data, S32 status, LLExtStat ext_status);
#if 0 //Client side compiling disabled.
	static void onSaveTextComplete(const LLUUID& asset_uuid, void* user_data, S32 status, LLExtStat ext_status);
	static void onSaveBytecodeComplete(const LLUUID& asset_uuid, void* user_data, S32 status, LLExtStat ext_status);
#endif
	static void onRunningCheckboxClicked(LLUICtrl*, void* userdata);
	static void onReset(void* userdata);

	void loadScriptText(LLVFS *vfs, const LLUUID &uuid, LLAssetType::EType type);

	static void onErrorList(LLUICtrl*, void* user_data);

	static void* createScriptEdPanel(void* userdata);

	static void	onMonoCheckboxClicked(LLUICtrl*, void* userdata);

private:
	bool mIsNew;
	//LLUUID mTransmitID;
	LLCheckBoxCtrl	*mRunningCheckbox;
	BOOL mAskedForRunningInfo;
	BOOL mHaveRunningInfo;
	LLButton		*mResetButton;
	LLPointer<LLViewerInventoryItem> mItem;
	BOOL mCloseAfterSave;
	// need to save both text and script, so need to decide when done
	S32 mPendingUploads;

	BOOL getIsModifiable() const { return mIsModifiable; } // Evaluated on load assert
	
	LLCheckBoxCtrl*	mMonoCheckbox;
	BOOL mIsModifiable;
};

#endif  // LL_LLPREVIEWSCRIPT_H
