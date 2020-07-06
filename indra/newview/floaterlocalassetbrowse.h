/**
* @file floaterlocalassetbrowse.h
* @brief Local texture support
*
* $LicenseInfo:firstyear=2009&license=viewergpl$
*
* Copyright (c) 2010, author unknown
*
* Imprudence Viewer Source Code
* The source code in this file ("Source Code") is provided to you
* under the terms of the GNU General Public License, version 2.0
* ("GPL"). Terms of the GPL can be found in doc/GPL-license.txt in
* this distribution, or online at
* http://secondlifegrid.net/programs/open_source/licensing/gplv2
*
* There are special exceptions to the terms and conditions of the GPL as
* it is applied to this Source Code. View the full text of the exception
* in the file doc/FLOSS-exception.txt in this software distribution, or
* online at http://secondlifegrid.net/programs/open_source/licensing/flossexception
*
* By copying, modifying or distributing this software, you acknowledge
* that you have read and understood your obligations described above,
* and agree to abide by those obligations.
*
* ALL SOURCE CODE IS PROVIDED "AS IS." THE AUTHOR MAKES NO
* WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
* COMPLETENESS OR PERFORMANCE.
* $/LicenseInfo$
*/

/* Local Asset Browser: header 

tag: vaa emerald local_asset_browser

*/


#ifndef VAA_LOCALBROWSER
#define VAA_LOCALBROWSER

#include "llfloater.h"
#include "llscrolllistctrl.h"
#include "lltexturectrl.h"
#include "lldrawable.h"
#include "lleventtimer.h"

class LLCheckBoxCtrl;
class LLComboBox;

/*=======================================*/
/*  Global structs / enums / defines     */
/*=======================================*/ 

#define LF_FLOATER_EXPAND_WIDTH 735
#define LF_FLOATER_CONTRACT_WIDTH 415
#define LF_FLOATER_HEIGHT 260

#define LOCAL_USE_MIPMAPS true
#define LOCAL_DISCARD_LEVEL 0
#define NO_IMAGE LLUUID::null

#define TIMER_HEARTBEAT 3.0

#define SLAM_FOR_DEBUG true

enum bitmaplist_cols
{
	BITMAPLIST_COL_NAME,
	BITMAPLIST_COL_ID
};

/* upload & sculpt update related */
struct affected_object
{
	LLViewerObject* object;
	std::vector<LLFace*> face_list;
	bool local_sculptmap;

};

/* texture picker defines */

#define LOCAL_TEXTURE_PICKER_NAME "texture picker"
#define LOCAL_TEXTURE_PICKER_LIST_NAME "local_name_list"
#define LOCAL_TEXTURE_PICKER_RECURSE true
#define LOCAL_TEXTURE_PICKER_CREATEIFMISSING true


/*=======================================*/
/*  LocalBitmap: unit class              */
/*=======================================*/ 
/*
	The basic unit class responsible for
	containing one loaded local texture.
*/

class LocalBitmap
{
	public:
		struct Params : public LLInitParam::Block<Params>
		{
			Mandatory<std::string> fullpath;
			Optional<bool> keep_updating;
			Optional<S32> type;
			Optional<LLUUID> id;
			Params(const std::string& path = LLStringUtil::null);
		};
		LocalBitmap(const Params& p);
		virtual ~LocalBitmap();
		friend class LocalAssetBrowser;

	public: /* [enums, typedefs, etc] */
		enum link_status
		{
			LINK_UNKNOWN, /* default fallback */
			LINK_ON,
			LINK_OFF,
			LINK_BROKEN,
			LINK_UPDATING /* currently redundant, but left in case necessary later. */
		};

		enum extension_type
		{
			IMG_EXTEN_BMP,
			IMG_EXTEN_TGA,
			IMG_EXTEN_JPG,
			IMG_EXTEN_PNG
		};

		enum bitmap_type
		{
			TYPE_TEXTURE = 0,
			TYPE_SCULPT = 1,
			TYPE_LAYER = 2
		};

	public: /* [information query functions] */
		std::string getShortName() const;
		std::string getFileName() const;
		LLUUID      getID() const;
		void        setID(const LLUUID&);
		LLSD        getLastModified() const;
		std::string getLinkStatus() const;
		bool        getUpdateBool() const;
		void        setType( S32 );
		bool        getIfValidBool() const;
		S32		    getType() const;
		void		getDebugInfo() const;
		LLSD		asLLSD() const;


	private: /* [maintenence functions] */
		void updateSelf();
		bool decodeSelf(LLImageRaw* rawimg);
		void setUpdateBool();

		std::vector<LLFace*>		 getFaceUsesThis(LLDrawable*) const;
		std::vector<affected_object> getUsingObjects(bool seek_by_type = true,
												     bool seek_textures = false, bool seek_sculptmaps = false) const;

	protected: /* [basic properties] */
		std::string    shortname;
		std::string    filename;
		extension_type extension;
		LLUUID         id;
		LLSD           last_modified;
		link_status    linkstatus;
		bool           keep_updating;
		bool           valid;
		S32		       bitmap_type;
		bool           sculpt_dirty;
		bool           volume_dirty;
};

/*=======================================*/
/*  LocalAssetBrowser: main class        */
/*=======================================*/ 
/*
	Responsible for internal workings.
	Instantiated at the top of the source file.
	Sits in memory until the viewer is closed.

*/

class AIFilePicker;

class LocalAssetBrowser : public LLSingleton<LocalAssetBrowser>
{
	public:
		const std::string getFileName() const;
		LocalAssetBrowser();
		virtual ~LocalAssetBrowser();
		friend class FloaterLocalAssetBrowser;
		friend class LocalAssetBrowserTimer;
		static void UpdateTextureCtrlList(LLScrollListCtrl*);
		static void setLayerUpdated(bool toggle) { mLayerUpdated = toggle; }
		static void setSculptUpdated(bool toggle) { mSculptUpdated = toggle; }
		static void add(const LocalBitmap& unit) { loaded_bitmaps.push_back(unit); }
		static void AddBitmap();
		static void AddBitmap_continued(AIFilePicker* filepicker);
		static void DelBitmap( std::vector<LLScrollListItem*>, S32 column = BITMAPLIST_COL_ID );

		/* UpdateTextureCtrlList was made public cause texturectrl requests it once on spawn 
		   ( added: when it's own add/remove funcs are used. )
		   i've made it update on spawn instead of on pressing 'local' because the former does it once, 
		   the latter - each time the button's pressed. */

	private:
		static void onChangeHappened();
		static void onUpdateBool(LLUUID);
		static void onSetType(LLUUID, S32);
		static LocalBitmap* GetBitmapUnit(LLUUID);
		static bool IsDoingUpdates();
		static void PingTimer();
		static void PerformTimedActions();
		static void PerformSculptUpdates(LocalBitmap&);

	protected:
		static  std::vector<LocalBitmap> loaded_bitmaps;
		typedef std::vector<LocalBitmap>::iterator local_list_iter;
		static  bool    mLayerUpdated;
		static  bool    mSculptUpdated; 
};

/*==================================================*/
/*  FloaterLocalAssetBrowser : interface class      */
/*==================================================*/ 
/*
	Responsible for talking to the user.
	Instantiated by user request.
	Destroyed when the floater is closed.

*/
class FloaterLocalAssetBrowser : public LLFloater
{
public:
    FloaterLocalAssetBrowser();
    virtual ~FloaterLocalAssetBrowser();
    static void show(void*);
 
private: 
	/* Widget related callbacks */
	// Button callback declarations
	void onClickAdd();
	void onClickDel();
	void onClickUpload();

	// ScrollList callback declarations
	void onChooseBitmapList();

	// Checkbox callback declarations
	void onClickUpdateChkbox();

	// Combobox type select
	void onCommitTypeCombo();

	void onUpdateID(const LLSD& val);

	// Widgets
	LLButton* mAddBtn;
	LLButton* mDelBtn;
	LLButton* mMoreBtn;
	LLButton* mLessBtn;
	LLButton* mUploadBtn;

	LLScrollListCtrl* mBitmapList;
	LLTextureCtrl*    mTextureView;
	LLCheckBoxCtrl*   mUpdateChkBox;

	LLLineEditor* mPathTxt;
	LLLineEditor* mUUIDTxt;
	LLLineEditor* mNameTxt;

	LLTextBox*  mLinkTxt;
	LLTextBox*  mTimeTxt;
	LLComboBox* mTypeComboBox;

	LLTextBox* mCaptionPathTxt;
	LLTextBox* mCaptionUUIDTxt;
	LLTextBox* mCaptionLinkTxt;
	LLTextBox* mCaptionNameTxt;
	LLTextBox* mCaptionTimeTxt;

	// non-widget functions
	void FloaterResize(bool expand);
	void UpdateRightSide();

public:
	void UpdateBitmapScrollList();
};

/*==================================================*/
/*     LocalAssetBrowserTimer : timer class         */
/*==================================================*/ 
/*
	A small, simple timer class inheriting from
	LLEventTimer, responsible for pinging the
	LocalAssetBrowser class to perform it's
	updates / checks / etc.

*/
class LocalAssetBrowserTimer : public LLEventTimer
{
	public:
		LocalAssetBrowserTimer();
		~LocalAssetBrowserTimer();
		BOOL tick() override;
		void		 start();
		void		 stop();
		bool         isRunning() const;
};

#endif

