/** 
 * @file llpreviewgesture.cpp
 * @brief Editing UI for inventory-based gestures.
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 * 
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"
#include "llpreviewgesture.h"

#include "llagent.h"
#include "llanimstatelabels.h"
#include "llanimationstates.h"
#include "llappviewer.h"			// gVFS
#include "llassetuploadresponders.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldatapacker.h"
#include "lldelayedgestureerror.h"
#include "llfloatergesture.h" // for some label constants
#include "llgesturemgr.h"
#include "llinventorydefines.h"
#include "llinventoryfunctions.h"
#include "llinventorymodelbackgroundfetch.h"
#include "llkeyboard.h"
#include "lllineeditor.h"
#include "llmultigesture.h"
#include "llnotificationsutil.h"
#include "llradiogroup.h"
#include "llresmgr.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "lltextbox.h"
#include "lltrans.h"
#include "lluictrlfactory.h"
#include "llvfile.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewerstats.h"


// *TODO: Translate?
const std::string NONE_LABEL = "---";
const std::string SHIFT_LABEL = "Shift";
const std::string CTRL_LABEL = "Ctrl";

void dialog_refresh_all();

// used for getting

class LLInventoryGestureAvailable : public LLInventoryCompletionObserver
{
public:
	LLInventoryGestureAvailable() {}

protected:
	virtual void done();
};

void LLInventoryGestureAvailable::done()
{
	for(uuid_vec_t::iterator it = mComplete.begin(); it != mComplete.end(); ++it)
	{
		LLPreview* preview = LLPreview::find((*it));
		if(preview)
		{
			preview->refresh();
		}
	}
	gInventory.removeObserver(this);
	delete this;
}

// Used for sorting
struct SortItemPtrsByName
{
	bool operator()(const LLInventoryItem* i1, const LLInventoryItem* i2)
	{
		return (LLStringUtil::compareDict(i1->getName(), i2->getName()) < 0);
	}
};

// static
LLPreviewGesture* LLPreviewGesture::show(const std::string& title, const LLUUID& item_id, const LLUUID& object_id, BOOL take_focus)
{
	LLPreviewGesture* preview = (LLPreviewGesture*)LLPreview::find(item_id);
	if (preview)
	{
		preview->open();   /*Flawfinder: ignore*/
	}
	else
	{
		preview = new LLPreviewGesture();

		// Finish internal construction
		preview->init(item_id, object_id);

		// Builds and adds to gFloaterView
		LLUICtrlFactory::getInstance()->buildFloater(preview, "floater_preview_gesture.xml");
		preview->setTitle(title);

		// Move window to top-left of screen
		LLMultiFloater* hostp = preview->getHost();
		if (hostp == NULL)
		{
			LLRect r = preview->getRect();
			LLRect screen = gFloaterView->getRect();
			r.setLeftTopAndSize(0, screen.getHeight(), r.getWidth(), r.getHeight());
			preview->setRect(r);
		}
		else
		{
			// re-add to host to update title
			hostp->addFloater(preview, TRUE);
		}

		// Start speculative download of sounds and animations
		const LLUUID animation_folder_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_ANIMATION);
		LLInventoryModelBackgroundFetch::instance().start(animation_folder_id);

		const LLUUID sound_folder_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_SOUND);
		LLInventoryModelBackgroundFetch::instance().start(sound_folder_id);

		// this will call refresh when we have everything.
		LLViewerInventoryItem* item = (LLViewerInventoryItem*)preview->getItem();
		if (item && !item->isFinished())
		{
			LLInventoryGestureAvailable* observer;
			observer = new LLInventoryGestureAvailable();
			observer->watchItem(item_id);
			gInventory.addObserver(observer);
			item->fetchFromServer();
		}
		else
		{
			// not sure this is necessary.
			preview->refresh();
		}
	}

	if (take_focus)
	{
		preview->setFocus(TRUE);
	}

	return preview;
}

void LLPreviewGesture::draw()
{
	// Skip LLPreview::draw() to avoid description update
	LLFloater::draw();
}

// virtual
BOOL LLPreviewGesture::handleKeyHere(KEY key, MASK mask)
{
	if(('S' == key) && (MASK_CONTROL == (mask & MASK_CONTROL)))
	{
		saveIfNeeded();
		return TRUE;
	}

	return LLPreview::handleKeyHere(key, mask);
}


// virtual
BOOL LLPreviewGesture::handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
										 EDragAndDropType cargo_type,
										 void* cargo_data,
										 EAcceptance* accept,
										 std::string& tooltip_msg)
{
	BOOL handled = TRUE;
	switch(cargo_type)
	{
	case DAD_ANIMATION:
	case DAD_SOUND:
		{
			// TODO: Don't allow this if you can't transfer the sound/animation

			// make a script step
			LLInventoryItem* item = (LLInventoryItem*)cargo_data;
			if (item
				&& gInventory.getItem(item->getUUID()))
			{
				LLPermissions perm = item->getPermissions();
				if (!((perm.getMaskBase() & PERM_ITEM_UNRESTRICTED) == PERM_ITEM_UNRESTRICTED))
				{
					*accept = ACCEPT_NO;
					if (tooltip_msg.empty())
					{
						tooltip_msg.assign("Only animations and sounds\n"
											"with unrestricted permissions\n"
											"can be added to a gesture.");
					}
					break;
				}
				else if (drop)
				{
					LLScrollListItem* line = NULL;
					if (cargo_type == DAD_ANIMATION)
					{
						line = addStep( STEP_ANIMATION );
						LLGestureStepAnimation* anim = (LLGestureStepAnimation*)line->getUserdata();
						anim->mAnimAssetID = item->getAssetUUID();
						anim->mAnimName = item->getName();
					}
					else if (cargo_type == DAD_SOUND)
					{
						line = addStep( STEP_SOUND );
						LLGestureStepSound* sound = (LLGestureStepSound*)line->getUserdata();
						sound->mSoundAssetID = item->getAssetUUID();
						sound->mSoundName = item->getName();
					}
					updateLabel(line);
					mDirty = TRUE;
					refresh();
				}
				*accept = ACCEPT_YES_COPY_MULTI;
			}
			else
			{
				// Not in user's inventory means it was in object inventory
				*accept = ACCEPT_NO;
			}
			break;
		}
	default:
		*accept = ACCEPT_NO;
		if (tooltip_msg.empty())
		{
			tooltip_msg.assign("Only animations and sounds\n"
								"can be added to a gesture.");
		}
		break;
	}
	return handled;
}


// virtual
BOOL LLPreviewGesture::canClose()
{

	if(!mDirty || mForceClose)
	{
		return TRUE;
	}
	else
	{
		// Bring up view-modal dialog: Save changes? Yes, No, Cancel
		LLNotificationsUtil::add("SaveChanges", LLSD(), LLSD(),
			boost::bind(&LLPreviewGesture::handleSaveChangesDialog, this, _1, _2) );
		return FALSE;
	}
}

// virtual
void LLPreviewGesture::onClose(bool app_quitting)
{
	LLGestureMgr::instance().stopGesture(mPreviewGesture);
	LLPreview::onClose(app_quitting);
}

// virtual
void LLPreviewGesture::onUpdateSucceeded()
{
	refresh();
}

// virtual
void LLPreviewGesture::setMinimized(BOOL minimize)
{
	if (minimize != isMinimized())
	{
		LLFloater::setMinimized(minimize);

		// We're being restored
		if (!minimize)
		{
			refresh();
		}
	}
}


bool LLPreviewGesture::handleSaveChangesDialog(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	switch(option)
	{
	case 0:  // "Yes"
		LLGestureMgr::instance().stopGesture(mPreviewGesture);
		mCloseAfterSave = TRUE;
		onClickSave();
		break;

	case 1:  // "No"
		LLGestureMgr::instance().stopGesture(mPreviewGesture);
		mDirty = FALSE; // Force the dirty flag because user has clicked NO on confirm save dialog...
		close();
		break;

	case 2: // "Cancel"
	default:
		// If we were quitting, we didn't really mean it.
		LLAppViewer::instance()->abortQuit();
		break;
	}
	return false;
}


LLPreviewGesture::LLPreviewGesture()
:	LLPreview("Gesture Preview"),
	mTriggerEditor(NULL),
	mModifierCombo(NULL),
	mKeyCombo(NULL),
	mLibraryList(NULL),
	mAddBtn(NULL),
	mUpBtn(NULL),
	mDownBtn(NULL),
	mDeleteBtn(NULL),
	mStepList(NULL),
	mOptionsText(NULL),
	mAnimationRadio(NULL),
	mAnimationCombo(NULL),
	mSoundCombo(NULL),
	mChatEditor(NULL),
	mSaveBtn(NULL),
	mPreviewBtn(NULL),
	mPreviewGesture(NULL),
	mDirty(FALSE)
{
}


LLPreviewGesture::~LLPreviewGesture()
{
	// Userdata for all steps is a LLGestureStep we need to clean up
	std::vector<LLScrollListItem*> data_list = mStepList->getAllData();
	std::vector<LLScrollListItem*>::iterator data_itor;
	for (data_itor = data_list.begin(); data_itor != data_list.end(); ++data_itor)
	{
		LLScrollListItem* item = *data_itor;
		LLGestureStep* step = (LLGestureStep*)item->getUserdata();
		delete step;
		step = NULL;
	}
}


BOOL LLPreviewGesture::postBuild()
{
	LLLineEditor* edit;
	LLComboBox* combo;
	LLButton* btn;
	LLScrollListCtrl* list;
	LLTextBox* text;
	LLCheckBoxCtrl* check;

	edit = getChild<LLLineEditor>("trigger_editor");
	edit->setKeystrokeCallback(boost::bind(&onKeystrokeCommit,_1,this));
	edit->setCommitCallback(boost::bind(&LLPreviewGesture::onCommitSetDirty,this));
	edit->setCommitOnFocusLost(TRUE);
	edit->setIgnoreTab(TRUE);
	mTriggerEditor = edit;

	text = getChild<LLTextBox>("replace_text");
	text->setEnabled(FALSE);
	mReplaceText = text;

	edit = getChild<LLLineEditor>("replace_editor");
	edit->setEnabled(FALSE);
	edit->setKeystrokeCallback(boost::bind(&onKeystrokeCommit,_1,this));
	edit->setCommitCallback(boost::bind(&LLPreviewGesture::onCommitSetDirty,this));
	edit->setCommitOnFocusLost(TRUE);
	edit->setIgnoreTab(TRUE);
	mReplaceEditor = edit;

	combo = getChild<LLComboBox>( "modifier_combo");
	combo->setCommitCallback(boost::bind(&LLPreviewGesture::onCommitSetDirty,this));
	mModifierCombo = combo;

	combo = getChild<LLComboBox>( "key_combo");
	combo->setCommitCallback(boost::bind(&LLPreviewGesture::onCommitSetDirty,this));
	mKeyCombo = combo;

	list = getChild<LLScrollListCtrl>("library_list");
	list->setCommitCallback(boost::bind(&LLPreviewGesture::onCommitLibrary,this));
	list->setDoubleClickCallback(boost::bind(&LLPreviewGesture::onClickAdd,this));
	mLibraryList = list;

	btn = getChild<LLButton>( "add_btn");
	btn->setClickedCallback(boost::bind(&LLPreviewGesture::onClickAdd,this));
	btn->setEnabled(FALSE);
	mAddBtn = btn;

	btn = getChild<LLButton>( "up_btn");
	btn->setClickedCallback(boost::bind(&LLPreviewGesture::onClickUp,this));
	btn->setEnabled(FALSE);
	mUpBtn = btn;

	btn = getChild<LLButton>( "down_btn");
	btn->setClickedCallback(boost::bind(&LLPreviewGesture::onClickDown,this));
	btn->setEnabled(FALSE);
	mDownBtn = btn;

	btn = getChild<LLButton>( "delete_btn");
	btn->setClickedCallback(boost::bind(&LLPreviewGesture::onClickDelete,this));
	btn->setEnabled(FALSE);
	mDeleteBtn = btn;

	list = getChild<LLScrollListCtrl>("step_list");
	list->setCommitCallback(boost::bind(&LLPreviewGesture::onCommitStep,this));
	mStepList = list;

	// Options
	text = getChild<LLTextBox>("options_text");
	text->setBorderVisible(TRUE);
	mOptionsText = text;

	combo = getChild<LLComboBox>( "animation_list");
	combo->setVisible(FALSE);
	combo->setCommitCallback(boost::bind(&LLPreviewGesture::onCommitAnimation,this));
	mAnimationCombo = combo;

	LLRadioGroup* group;
	group = getChild<LLRadioGroup>("animation_trigger_type");
	group->setVisible(FALSE);
	group->setCommitCallback(boost::bind(&LLPreviewGesture::onCommitAnimationTrigger,this));
	mAnimationRadio = group;

	combo = getChild<LLComboBox>( "sound_list");
	combo->setVisible(FALSE);
	combo->setCommitCallback(boost::bind(&LLPreviewGesture::onCommitSound,this));
	mSoundCombo = combo;

	edit = getChild<LLLineEditor>("chat_editor");
	edit->setVisible(FALSE);
	edit->setCommitCallback(boost::bind(&LLPreviewGesture::onCommitChat,this));
	//edit->setKeystrokeCallback(onKeystrokeCommit, this);
	edit->setCommitOnFocusLost(TRUE);
	edit->setIgnoreTab(TRUE);
	mChatEditor = edit;

	check = getChild<LLCheckBoxCtrl>( "wait_anim_check");
	check->setVisible(FALSE);
	check->setCommitCallback(boost::bind(&LLPreviewGesture::onCommitWait,this));
	mWaitAnimCheck = check;

	check = getChild<LLCheckBoxCtrl>( "wait_time_check");
	check->setVisible(FALSE);
	check->setCommitCallback(boost::bind(&LLPreviewGesture::onCommitWait,this));
	mWaitTimeCheck = check;

	edit = getChild<LLLineEditor>("wait_time_editor");
	edit->setEnabled(FALSE);
	edit->setVisible(FALSE);
	edit->setPrevalidate(LLLineEditor::prevalidateFloat);
//	edit->setKeystrokeCallback(onKeystrokeCommit, this);
	edit->setCommitOnFocusLost(TRUE);
	edit->setCommitCallback(boost::bind(&LLPreviewGesture::onCommitWaitTime,this));
	edit->setIgnoreTab(TRUE);
	mWaitTimeEditor = edit;

	// Buttons at the bottom
	check = getChild<LLCheckBoxCtrl>( "active_check");
	check->setCommitCallback(boost::bind(&LLPreviewGesture::onCommitActive,this));
	mActiveCheck = check;

	btn = getChild<LLButton>( "save_btn");
	btn->setClickedCallback(boost::bind(&LLPreviewGesture::onClickSave,this));
	mSaveBtn = btn;

	btn = getChild<LLButton>( "preview_btn");
	btn->setClickedCallback(boost::bind(&LLPreviewGesture::onClickPreview,this));
	mPreviewBtn = btn;


	// Populate the comboboxes
	addModifiers();
	addKeys();
	addAnimations();
	addSounds();

	const LLInventoryItem* item = getItem();

	if (item) 
	{
		childSetCommitCallback("desc", LLPreview::onText, this);
		childSetText("desc", item->getDescription());
		getChild<LLLineEditor>("desc")->setPrevalidate(&LLLineEditor::prevalidatePrintableNotPipe);
	}

	return TRUE;
}


void LLPreviewGesture::addModifiers()
{
	LLComboBox* combo = mModifierCombo;

	combo->add( NONE_LABEL,  ADD_BOTTOM );
	combo->add( SHIFT_LABEL, ADD_BOTTOM );
	combo->add( CTRL_LABEL,  ADD_BOTTOM );
	combo->setCurrentByIndex(0);
}


static const std::string valid_key_to_string(KEY key)
{
	std::string skey(1,(char)key);
	std::string strkey = LLKeyboard::stringFromKey(key);
	return ((skey == strkey && key >= ' ' && key <= '~') || (skey != strkey) ) ? strkey : "";
}

void LLPreviewGesture::addKeys()
{
	LLComboBox* combo = mKeyCombo;

	combo->add( NONE_LABEL );
	for (KEY key = ' '; key < KEY_NONE; key++)
	{
		std::string keystr = valid_key_to_string(key);
		if(keystr != "")combo->add( keystr, ADD_BOTTOM );
	}
	combo->setCurrentByIndex(0);
}


// TODO: Sort the legacy and non-legacy together?
void LLPreviewGesture::addAnimations()
{
	LLComboBox* combo = mAnimationCombo;

	combo->removeall();
	
	std::string none_text = getString("none_text");

	combo->add(none_text, LLUUID::null);

	// Add all the default (legacy) animations
	S32 i;
	for (i = 0; i < gUserAnimStatesCount; ++i)
	{
		// Use the user-readable name
		std::string label = LLAnimStateLabels::getStateLabel( gUserAnimStates[i].mName );
		const LLUUID& id = gUserAnimStates[i].mID;
		combo->add(label, id);
	}

	// Get all inventory items that are animations
	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;
	LLIsTypeWithPermissions is_copyable_animation(LLAssetType::AT_ANIMATION,
													PERM_ITEM_UNRESTRICTED,
													gAgent.getID(),
													gAgent.getGroupID());
	gInventory.collectDescendentsIf(gInventory.getRootFolderID(),
									cats,
									items,
									LLInventoryModel::EXCLUDE_TRASH,
									is_copyable_animation);

	// Copy into something we can sort
	std::vector<LLInventoryItem*> animations;

	S32 count = items.count();
	for(i = 0; i < count; ++i)
	{
		animations.push_back( items.get(i) );
	}

	// Do the sort
	std::sort(animations.begin(), animations.end(), SortItemPtrsByName());

	// And load up the combobox
	std::vector<LLInventoryItem*>::iterator it;
	for (it = animations.begin(); it != animations.end(); ++it)
	{
		LLInventoryItem* item = *it;

		combo->add(item->getName(), item->getAssetUUID(), ADD_BOTTOM);
	}
}


void LLPreviewGesture::addSounds()
{
	LLComboBox* combo = mSoundCombo;
	combo->removeall();
	
	std::string none_text = getString("none_text");

	combo->add(none_text, LLUUID::null);

	// Get all inventory items that are sounds
	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;
	LLIsTypeWithPermissions is_copyable_sound(LLAssetType::AT_SOUND,
													PERM_ITEM_UNRESTRICTED,
													gAgent.getID(),
													gAgent.getGroupID());
	gInventory.collectDescendentsIf(gInventory.getRootFolderID(),
									cats,
									items,
									LLInventoryModel::EXCLUDE_TRASH,
									is_copyable_sound);

	// Copy sounds into something we can sort
	std::vector<LLInventoryItem*> sounds;

	S32 i;
	S32 count = items.count();
	for(i = 0; i < count; ++i)
	{
		sounds.push_back( items.get(i) );
	}

	// Do the sort
	std::sort(sounds.begin(), sounds.end(), SortItemPtrsByName());

	// And load up the combobox
	std::vector<LLInventoryItem*>::iterator it;
	for (it = sounds.begin(); it != sounds.end(); ++it)
	{
		LLInventoryItem* item = *it;

		combo->add(item->getName(), item->getAssetUUID(), ADD_BOTTOM);
	}
}


void LLPreviewGesture::init(const LLUUID& item_id, const LLUUID& object_id)
{
	// Sets ID and adds to instance list
	setItemID(item_id);
	setObjectID(object_id);
}


void LLPreviewGesture::refresh()
{
	// If previewing or item is incomplete, all controls are disabled
	LLViewerInventoryItem* item = (LLViewerInventoryItem*)getItem();
	bool is_complete = (item && item->isFinished()) ? true : false;
	if (mPreviewGesture || !is_complete)
	{
		
		getChildView("desc")->setEnabled(FALSE);
		//mDescEditor->setEnabled(FALSE);
		mTriggerEditor->setEnabled(FALSE);
		mReplaceText->setEnabled(FALSE);
		mReplaceEditor->setEnabled(FALSE);
		mModifierCombo->setEnabled(FALSE);
		mKeyCombo->setEnabled(FALSE);
		mLibraryList->setEnabled(FALSE);
		mAddBtn->setEnabled(FALSE);
		mUpBtn->setEnabled(FALSE);
		mDownBtn->setEnabled(FALSE);
		mDeleteBtn->setEnabled(FALSE);
		mStepList->setEnabled(FALSE);
		mOptionsText->setEnabled(FALSE);
		mAnimationCombo->setEnabled(FALSE);
		mAnimationRadio->setEnabled(FALSE);
		mSoundCombo->setEnabled(FALSE);
		mChatEditor->setEnabled(FALSE);
		mWaitAnimCheck->setEnabled(FALSE);
		mWaitTimeCheck->setEnabled(FALSE);
		mWaitTimeEditor->setEnabled(FALSE);
		mActiveCheck->setEnabled(FALSE);
		mSaveBtn->setEnabled(FALSE);
		// <edit>
		mStepList->setEnabled(TRUE);
		// </edit>

		// Make sure preview button is enabled, so we can stop it
		mPreviewBtn->setEnabled(TRUE);
		return;
	}

	BOOL modifiable = item->getPermissions().allowModifyBy(gAgent.getID());

	getChildView("desc")->setEnabled(modifiable);
	mTriggerEditor->setEnabled(TRUE);
	mLibraryList->setEnabled(modifiable);
	mStepList->setEnabled(modifiable);
	mOptionsText->setEnabled(modifiable);
	mAnimationCombo->setEnabled(modifiable);
	mAnimationRadio->setEnabled(modifiable);
	mSoundCombo->setEnabled(modifiable);
	mChatEditor->setEnabled(modifiable);
	mWaitAnimCheck->setEnabled(modifiable);
	mWaitTimeCheck->setEnabled(modifiable);
	mWaitTimeEditor->setEnabled(modifiable);
	mActiveCheck->setEnabled(TRUE);

	const std::string& trigger = mTriggerEditor->getText();
	BOOL have_trigger = !trigger.empty();

	const std::string& replace = mReplaceEditor->getText();
	BOOL have_replace = !replace.empty();

	LLScrollListItem* library_item = mLibraryList->getFirstSelected();
	BOOL have_library = (library_item != NULL);

	LLScrollListItem* step_item = mStepList->getFirstSelected();
	S32 step_index = mStepList->getFirstSelectedIndex();
	S32 step_count = mStepList->getItemCount();
	BOOL have_step = (step_item != NULL);

	mReplaceText->setEnabled(have_trigger || have_replace);
	mReplaceEditor->setEnabled(have_trigger || have_replace);

	mModifierCombo->setEnabled(TRUE);
	mKeyCombo->setEnabled(TRUE);

	mAddBtn->setEnabled(modifiable && have_library);
	mUpBtn->setEnabled(modifiable && have_step && step_index > 0);
	mDownBtn->setEnabled(modifiable && have_step && step_index < step_count-1);
	mDeleteBtn->setEnabled(modifiable && have_step);

	// Assume all not visible
	mAnimationCombo->setVisible(FALSE);
	mAnimationRadio->setVisible(FALSE);
	mSoundCombo->setVisible(FALSE);
	mChatEditor->setVisible(FALSE);
	mWaitAnimCheck->setVisible(FALSE);
	mWaitTimeCheck->setVisible(FALSE);
	mWaitTimeEditor->setVisible(FALSE);

	std::string optionstext;
	
	if (have_step)
	{
		// figure out the type, show proper options, update text
		LLGestureStep* step = (LLGestureStep*)step_item->getUserdata();
		EStepType type = step->getType();

		switch(type)
		{
		case STEP_ANIMATION:
			{
				LLGestureStepAnimation* anim_step = (LLGestureStepAnimation*)step;
				optionstext = getString("step_anim");
				mAnimationCombo->setVisible(TRUE);
				mAnimationRadio->setVisible(TRUE);
				mAnimationRadio->setSelectedIndex((anim_step->mFlags & ANIM_FLAG_STOP) ? 1 : 0);
				mAnimationCombo->setCurrentByID(anim_step->mAnimAssetID);
				break;
			}
		case STEP_SOUND:
			{
				LLGestureStepSound* sound_step = (LLGestureStepSound*)step;
				optionstext = getString("step_sound");
				mSoundCombo->setVisible(TRUE);
				mSoundCombo->setCurrentByID(sound_step->mSoundAssetID);
				break;
			}
		case STEP_CHAT:
			{
				LLGestureStepChat* chat_step = (LLGestureStepChat*)step;
				optionstext = getString("step_chat");
				mChatEditor->setVisible(TRUE);
				mChatEditor->setText(chat_step->mChatText);
				break;
			}
		case STEP_WAIT:
			{
				LLGestureStepWait* wait_step = (LLGestureStepWait*)step;
				optionstext = getString("step_wait");
				mWaitAnimCheck->setVisible(TRUE);
				mWaitAnimCheck->set(wait_step->mFlags & WAIT_FLAG_ALL_ANIM);
				mWaitTimeCheck->setVisible(TRUE);
				mWaitTimeCheck->set(wait_step->mFlags & WAIT_FLAG_TIME);
				mWaitTimeEditor->setVisible(TRUE);
				std::string buffer = llformat("%.1f", (double)wait_step->mWaitSeconds);
				mWaitTimeEditor->setText(buffer);
				break;
			}
		default:
			break;
		}
	}
	
	mOptionsText->setText(optionstext);

	BOOL active = LLGestureMgr::instance().isGestureActive(mItemUUID);
	mActiveCheck->set(active);

	// Can only preview if there are steps
	mPreviewBtn->setEnabled(step_count > 0);

	// And can only save if changes have been made
	mSaveBtn->setEnabled(mDirty);
	addAnimations();
	addSounds();
}


void LLPreviewGesture::initDefaultGesture()
{
	LLScrollListItem* item;
	item = addStep( STEP_ANIMATION );
	LLGestureStepAnimation* anim = (LLGestureStepAnimation*)item->getUserdata();
	anim->mAnimAssetID = ANIM_AGENT_HELLO;
	anim->mAnimName = LLTrans::getString("Wave");
	updateLabel(item);

	item = addStep( STEP_WAIT );
	LLGestureStepWait* wait = (LLGestureStepWait*)item->getUserdata();
	wait->mFlags = WAIT_FLAG_ALL_ANIM;
	updateLabel(item);

	item = addStep( STEP_CHAT );
	LLGestureStepChat* chat_step = (LLGestureStepChat*)item->getUserdata();
	chat_step->mChatText =  LLTrans::getString("HelloAvatar");
	updateLabel(item);

	// Start with item list selected
	mStepList->selectFirstItem();

	// this is *new* content, so we are dirty
	mDirty = TRUE;
}


void LLPreviewGesture::loadAsset()
{
	const LLInventoryItem* item = getItem();
	if (!item) 
	{
		// Don't set asset status here; we may not have set the item id yet
		// (e.g. when this gets called initially)
		//mAssetStatus = PREVIEW_ASSET_ERROR;
		return;
	}

	LLUUID asset_id = item->getAssetUUID();
	if (asset_id.isNull())
	{
		// Freshly created gesture, don't need to load asset.
		// Blank gesture will be fine.
		initDefaultGesture();
		refresh();
		mAssetStatus = PREVIEW_ASSET_LOADED;
		return;
	}

	// TODO: Based on item->getPermissions().allow*
	// could enable/disable UI.

	// Copy the UUID, because the user might close the preview
	// window if the download gets stalled.
	LLUUID* item_idp = new LLUUID(mItemUUID);

	const BOOL high_priority = TRUE;
	gAssetStorage->getAssetData(asset_id,
								LLAssetType::AT_GESTURE,
								onLoadComplete,
								(void**)item_idp,
								high_priority);
	mAssetStatus = PREVIEW_ASSET_LOADING;
}


// static
void LLPreviewGesture::onLoadComplete(LLVFS *vfs,
									   const LLUUID& asset_uuid,
									   LLAssetType::EType type,
									   void* user_data, S32 status, LLExtStat ext_status)
{
	LLUUID* item_idp = (LLUUID*)user_data;
	LLPreview* preview = LLPreview::find(*item_idp);
	if (preview)
	{
		LLPreviewGesture* self = (LLPreviewGesture*)preview;

		if (0 == status)
		{
			LLVFile file(vfs, asset_uuid, type, LLVFile::READ);
			S32 size = file.getSize();

			std::vector<char> buffer(size+1);
			file.read((U8*)&buffer[0], size);
			buffer[size] = '\0';

			LLMultiGesture* gesture = new LLMultiGesture();

			LLDataPackerAsciiBuffer dp(&buffer[0], size+1);
			BOOL ok = gesture->deserialize(dp);

			if (ok)
			{
				// Everything has been successful.  Load up the UI.
				self->loadUIFromGesture(gesture);

				self->mStepList->selectFirstItem();

				self->mDirty = FALSE;
				self->refresh();
				self->refreshFromItem(self->getItem()); // to update description and title
			}
			else
			{
				llwarns << "Unable to load gesture" << llendl;
			}

			delete gesture;
			gesture = NULL;

			self->mAssetStatus = PREVIEW_ASSET_LOADED;
		}
		else
		{
			LLViewerStats::getInstance()->incStat( LLViewerStats::ST_DOWNLOAD_FAILED );

			if( LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE == status ||
				LL_ERR_FILE_EMPTY == status)
			{
				LLDelayedGestureError::gestureMissing( *item_idp );
			}
			else
			{
				LLDelayedGestureError::gestureFailedToLoad( *item_idp );
			}

			llwarns << "Problem loading gesture: " << status << llendl;
			self->mAssetStatus = PREVIEW_ASSET_ERROR;
		}
	}
	delete item_idp;
	item_idp = NULL;
}


void LLPreviewGesture::loadUIFromGesture(LLMultiGesture* gesture)
{
	/*LLInventoryItem* item = getItem();


	
	if (item)
	{
		LLLineEditor* descEditor = getChild<LLLineEditor>("desc");
		descEditor->setText(item->getDescription());
	}*/

	mTriggerEditor->setText(gesture->mTrigger);

	mReplaceEditor->setText(gesture->mReplaceText);

	switch (gesture->mMask)
	{
	default:
	  case MASK_NONE:
		mModifierCombo->setSimple( NONE_LABEL );
		break;
	  case MASK_SHIFT:
		mModifierCombo->setSimple( SHIFT_LABEL );
		break;
	  case MASK_CONTROL:
		mModifierCombo->setSimple( CTRL_LABEL );
		break;
	}

	mKeyCombo->setCurrentByIndex(0);
	if (gesture->mKey != KEY_NONE)
	{
		mKeyCombo->setSimple(LLKeyboard::stringFromKey(gesture->mKey));
	}

	// Make UI steps for each gesture step
	S32 i;
	S32 count = gesture->mSteps.size();
	for (i = 0; i < count; ++i)
	{
		LLGestureStep* step = gesture->mSteps[i];

		LLGestureStep* new_step = NULL;

		switch(step->getType())
		{
		case STEP_ANIMATION:
			{
				LLGestureStepAnimation* anim_step = (LLGestureStepAnimation*)step;
				LLGestureStepAnimation* new_anim_step =
					new LLGestureStepAnimation(*anim_step);
				new_step = new_anim_step;
				break;
			}
		case STEP_SOUND:
			{
				LLGestureStepSound* sound_step = (LLGestureStepSound*)step;
				LLGestureStepSound* new_sound_step =
					new LLGestureStepSound(*sound_step);
				new_step = new_sound_step;
				break;
			}
		case STEP_CHAT:
			{
				LLGestureStepChat* chat_step = (LLGestureStepChat*)step;
				LLGestureStepChat* new_chat_step =
					new LLGestureStepChat(*chat_step);
				new_step = new_chat_step;
				break;
			}
		case STEP_WAIT:
			{
				LLGestureStepWait* wait_step = (LLGestureStepWait*)step;
				LLGestureStepWait* new_wait_step =
					new LLGestureStepWait(*wait_step);
				new_step = new_wait_step;
				break;
			}
		default:
			{
				break;
			}
		}

		if (!new_step) continue;

		// Create an enabled item with this step
		LLSD row;
		row["columns"][0]["value"] = getLabel( new_step->getLabel());
		row["columns"][0]["font"] = "SANSSERIF_SMALL";
		LLScrollListItem* item = mStepList->addElement(row);
		item->setUserdata(new_step);
	}
}

// Helpful structure so we can look up the inventory item
// after the save finishes.
struct LLSaveInfo
{
	LLSaveInfo(const LLUUID& item_id, const LLUUID& object_id, const std::string& desc,
				const LLTransactionID tid)
		: mItemUUID(item_id), mObjectUUID(object_id), mDesc(desc), mTransactionID(tid)
	{
	}

	LLUUID mItemUUID;
	LLUUID mObjectUUID;
	std::string mDesc;
	LLTransactionID mTransactionID;
};


void LLPreviewGesture::saveIfNeeded()
{
	if (!gAssetStorage)
	{
		llwarns << "Can't save gesture, no asset storage system." << llendl;
		return;
	}

	if (!mDirty)
	{
		return;
	}

	// Copy the UI into a gesture
	LLMultiGesture* gesture = createGesture();

	// Serialize the gesture
	S32 max_size = gesture->getMaxSerialSize();
	char* buffer = new char[max_size];

	LLDataPackerAsciiBuffer dp(buffer, max_size);

	BOOL ok = gesture->serialize(dp);

	// <edit>
	//if (dp.getCurrentSize() > 1000)
	if(0)
	// </edit>
	{
		LLNotificationsUtil::add("GestureSaveFailedTooManySteps");

		delete gesture;
		gesture = NULL;
	}
	else if (!ok)
	{
		LLNotificationsUtil::add("GestureSaveFailedTryAgain");
		delete gesture;
		gesture = NULL;
	}
	else
	{
		// Every save gets a new UUID.  Yup.
		LLTransactionID tid;
		LLAssetID asset_id;
		tid.generate();
		asset_id = tid.makeAssetID(gAgent.getSecureSessionID());

		LLVFile file(gVFS, asset_id, LLAssetType::AT_GESTURE, LLVFile::APPEND);

		S32 size = dp.getCurrentSize();
		file.setMaxSize(size);
		file.write((U8*)buffer, size);

		BOOL delayedUpload = FALSE;

		// Upload that asset to the database
		LLViewerInventoryItem* item = (LLViewerInventoryItem*) getItem();
		if (item)
		{
			std::string agent_url = gAgent.getRegion()->getCapability("UpdateGestureAgentInventory");
			std::string task_url = gAgent.getRegion()->getCapability("UpdateGestureTaskInventory");
			if (mObjectUUID.isNull() && !agent_url.empty())
			{
				//need to disable the preview floater so item
				//isn't re-saved before new asset arrives
				//fake out refresh.
				item->setComplete(FALSE);
				refresh();				
				item->setComplete(TRUE);

				// Saving into agent inventory
				LLSD body;
				body["item_id"] = mItemUUID;
				LLHTTPClient::post(agent_url, body,
					new LLUpdateAgentInventoryResponder(body, asset_id, LLAssetType::AT_GESTURE));
				delayedUpload = TRUE;
			}
			else if (!mObjectUUID.isNull() && !task_url.empty())
			{
				// Saving into task inventory
				LLSD body;
				body["task_id"] = mObjectUUID;
				body["item_id"] = mItemUUID;
				LLHTTPClient::post(task_url, body,
					new LLUpdateTaskInventoryResponder(body, asset_id, LLAssetType::AT_GESTURE));
			}
			else if (gAssetStorage)
			{
				LLLineEditor* descEditor = getChild<LLLineEditor>("desc");
				LLSaveInfo* info = new LLSaveInfo(mItemUUID, mObjectUUID, descEditor->getText(), tid);
				gAssetStorage->storeAssetData(tid, LLAssetType::AT_GESTURE, onSaveComplete, info, FALSE);
			}
		}

		// If this gesture is active, then we need to update the in-memory
		// active map with the new pointer.
		if (!delayedUpload && LLGestureMgr::instance().isGestureActive(mItemUUID))
		{
			// gesture manager now owns the pointer
			LLGestureMgr::instance().replaceGesture(mItemUUID, gesture, asset_id);

			// replaceGesture may deactivate other gestures so let the
			// inventory know.
			gInventory.notifyObservers();
		}
		else
		{
			// we're done with this gesture
			delete gesture;
			gesture = NULL;
		}

		mDirty = FALSE;
		// refresh will be called when callback
		// if triggered when delayedUpload
		if(!delayedUpload)
		{
			refresh();
		}
	}

	delete [] buffer;
	buffer = NULL;
}


// TODO: This is very similar to LLPreviewNotecard::onSaveComplete.
// Could merge code.
// static
void LLPreviewGesture::onSaveComplete(const LLUUID& asset_uuid, void* user_data, S32 status, LLExtStat ext_status) // StoreAssetData callback (fixed)
{
	LLSaveInfo* info = (LLSaveInfo*)user_data;
	if (info && (status == 0))
	{
		if(info->mObjectUUID.isNull())
		{
			// Saving into user inventory
			LLViewerInventoryItem* item;
			item = (LLViewerInventoryItem*)gInventory.getItem(info->mItemUUID);
			if(item)
			{
				LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
				new_item->setDescription(info->mDesc);
				new_item->setTransactionID(info->mTransactionID);
				new_item->setAssetUUID(asset_uuid);
				new_item->updateServer(FALSE);
				gInventory.updateItem(new_item);
				gInventory.notifyObservers();
			}
			else
			{
				llwarns << "Inventory item for gesture " << info->mItemUUID
						<< " is no longer in agent inventory." << llendl;
			}
		}
		else
		{
			// Saving into in-world object inventory
			LLViewerObject* object = gObjectList.findObject(info->mObjectUUID);
			LLViewerInventoryItem* item = NULL;
			if(object)
			{
				item = (LLViewerInventoryItem*)object->getInventoryObject(info->mItemUUID);
			}
			if(object && item)
			{
				item->setDescription(info->mDesc);
				item->setAssetUUID(asset_uuid);
				item->setTransactionID(info->mTransactionID);
				object->updateInventory(item, TASK_INVENTORY_ITEM_KEY, false);
				dialog_refresh_all();
			}
			else
			{
				LLNotificationsUtil::add("GestureSaveFailedObjectNotFound");
			}
		}

		// Find our window and close it if requested.
		LLPreviewGesture* previewp = (LLPreviewGesture*)LLPreview::find(info->mItemUUID);
		if (previewp && previewp->mCloseAfterSave)
		{
			previewp->close();
		}
	}
	else
	{
		llwarns << "Problem saving gesture: " << status << llendl;
		LLSD args;
		args["REASON"] = std::string(LLAssetStorage::getErrorString(status));
		LLNotificationsUtil::add("GestureSaveFailedReason", args);
	}
	delete info;
	info = NULL;
}


LLMultiGesture* LLPreviewGesture::createGesture()
{
	LLMultiGesture* gesture = new LLMultiGesture();

	gesture->mTrigger = mTriggerEditor->getText();
	gesture->mReplaceText = mReplaceEditor->getText();

	const std::string& modifier = mModifierCombo->getSimple();
	if (modifier == CTRL_LABEL)
	{
		gesture->mMask = MASK_CONTROL;
	}
	else if (modifier == SHIFT_LABEL)
	{
		gesture->mMask = MASK_SHIFT;
	}
	else
	{
		gesture->mMask = MASK_NONE;
	}

	if (mKeyCombo->getCurrentIndex() == 0)
	{
		gesture->mKey = KEY_NONE;
	}
	else
	{
		const std::string& key_string = mKeyCombo->getSimple();
		LLKeyboard::keyFromString(key_string, &(gesture->mKey));
	}

	std::vector<LLScrollListItem*> data_list = mStepList->getAllData();
	std::vector<LLScrollListItem*>::iterator data_itor;
	for (data_itor = data_list.begin(); data_itor != data_list.end(); ++data_itor)
	{
		LLScrollListItem* item = *data_itor;
		LLGestureStep* step = (LLGestureStep*)item->getUserdata();

		switch(step->getType())
		{
		case STEP_ANIMATION:
			{
				// Copy UI-generated step into actual gesture step
				LLGestureStepAnimation* anim_step = (LLGestureStepAnimation*)step;
				LLGestureStepAnimation* new_anim_step =
					new LLGestureStepAnimation(*anim_step);
				gesture->mSteps.push_back(new_anim_step);
				break;
			}
		case STEP_SOUND:
			{
				// Copy UI-generated step into actual gesture step
				LLGestureStepSound* sound_step = (LLGestureStepSound*)step;
				LLGestureStepSound* new_sound_step =
					new LLGestureStepSound(*sound_step);
				gesture->mSteps.push_back(new_sound_step);
				break;
			}
		case STEP_CHAT:
			{
				// Copy UI-generated step into actual gesture step
				LLGestureStepChat* chat_step = (LLGestureStepChat*)step;
				LLGestureStepChat* new_chat_step =
					new LLGestureStepChat(*chat_step);
				gesture->mSteps.push_back(new_chat_step);
				break;
			}
		case STEP_WAIT:
			{
				// Copy UI-generated step into actual gesture step
				LLGestureStepWait* wait_step = (LLGestureStepWait*)step;
				LLGestureStepWait* new_wait_step =
					new LLGestureStepWait(*wait_step);
				gesture->mSteps.push_back(new_wait_step);
				break;
			}
		default:
			{
				break;
			}
		}
	}

	return gesture;
}


// static
void LLPreviewGesture::updateLabel(LLScrollListItem* item)
{
	LLGestureStep* step = (LLGestureStep*)item->getUserdata();

	LLScrollListCell* cell = item->getColumn(0);
	LLScrollListText* text_cell = (LLScrollListText*)cell;
	std::string label = getLabel( step->getLabel());
	text_cell->setText(label);
}

void LLPreviewGesture::onCommitSetDirty()
{
	mDirty = TRUE;
	refresh();
}

void LLPreviewGesture::onCommitLibrary()
{
	LLScrollListItem* library_item = mLibraryList->getFirstSelected();
	if (library_item)
	{
		mStepList->deselectAllItems();
		refresh();
	}
}


void LLPreviewGesture::onCommitStep()
{
	LLScrollListItem* step_item = mStepList->getFirstSelected();
	if (!step_item) return;

	mLibraryList->deselectAllItems();
	refresh();
}

void LLPreviewGesture::onCommitAnimation()
{
	LLScrollListItem* step_item = mStepList->getFirstSelected();
	if (step_item)
	{
		LLGestureStep* step = (LLGestureStep*)step_item->getUserdata();
		if (step->getType() == STEP_ANIMATION)
		{
			// Assign the animation name
			LLGestureStepAnimation* anim_step = (LLGestureStepAnimation*)step;
			if (mAnimationCombo->getCurrentIndex() == 0)
			{
				anim_step->mAnimName.clear();
				anim_step->mAnimAssetID.setNull();
			}
			else
			{
				anim_step->mAnimName = mAnimationCombo->getSimple();
				anim_step->mAnimAssetID = mAnimationCombo->getCurrentID();
			}
			//anim_step->mFlags = 0x0;

			// Update the UI label in the list
			updateLabel(step_item);

			mDirty = TRUE;
			refresh();
		}
	}
}

void LLPreviewGesture::onCommitAnimationTrigger()
{
	LLScrollListItem* step_item = mStepList->getFirstSelected();
	if (step_item)
	{
		LLGestureStep* step = (LLGestureStep*)step_item->getUserdata();
		if (step->getType() == STEP_ANIMATION)
		{
			LLGestureStepAnimation* anim_step = (LLGestureStepAnimation*)step;
			if (mAnimationRadio->getSelectedIndex() == 0)
			{
				// start
				anim_step->mFlags &= ~ANIM_FLAG_STOP;
			}
			else
			{
				// stop
				anim_step->mFlags |= ANIM_FLAG_STOP;
			}
			// Update the UI label in the list
			updateLabel(step_item);

			mDirty = TRUE;
			refresh();
		}
	}
}

void LLPreviewGesture::onCommitSound()
{
	LLScrollListItem* step_item = mStepList->getFirstSelected();
	if (step_item)
	{
		LLGestureStep* step = (LLGestureStep*)step_item->getUserdata();
		if (step->getType() == STEP_SOUND)
		{
			// Assign the sound name
			LLGestureStepSound* sound_step = (LLGestureStepSound*)step;
			sound_step->mSoundName = mSoundCombo->getSimple();
			sound_step->mSoundAssetID = mSoundCombo->getCurrentID();
			sound_step->mFlags = 0x0;

			// Update the UI label in the list
			updateLabel(step_item);

			mDirty = TRUE;
			refresh();
		}
	}
}

void LLPreviewGesture::onCommitChat()
{
	LLScrollListItem* step_item = mStepList->getFirstSelected();
	if (!step_item) return;

	LLGestureStep* step = (LLGestureStep*)step_item->getUserdata();
	if (step->getType() != STEP_CHAT) return;

	LLGestureStepChat* chat_step = (LLGestureStepChat*)step;
	chat_step->mChatText = mChatEditor->getText();
	chat_step->mFlags = 0x0;

	// Update the UI label in the list
	updateLabel(step_item);

	mDirty = TRUE;
	refresh();
}

void LLPreviewGesture::onCommitWait()
{
	LLScrollListItem* step_item = mStepList->getFirstSelected();
	if (!step_item) return;

	LLGestureStep* step = (LLGestureStep*)step_item->getUserdata();
	if (step->getType() != STEP_WAIT) return;

	LLGestureStepWait* wait_step = (LLGestureStepWait*)step;
	U32 flags = 0x0;
	if (mWaitAnimCheck->get()) flags |= WAIT_FLAG_ALL_ANIM;
	if (mWaitTimeCheck->get()) flags |= WAIT_FLAG_TIME;
	wait_step->mFlags = flags;

	{
		LLLocale locale(LLLocale::USER_LOCALE);

		F32 wait_seconds = (F32)atof(mWaitTimeEditor->getText().c_str());
		if (wait_seconds < 0.f) wait_seconds = 0.f;
		if (wait_seconds > 3600.f) wait_seconds = 3600.f;
		wait_step->mWaitSeconds = wait_seconds;
	}

	// Enable the input area if necessary
	mWaitTimeEditor->setEnabled(mWaitTimeCheck->get());

	// Update the UI label in the list
	updateLabel(step_item);

	mDirty = TRUE;
	refresh();
}

void LLPreviewGesture::onCommitWaitTime()
{
	LLScrollListItem* step_item = mStepList->getFirstSelected();
	if (!step_item) return;

	LLGestureStep* step = (LLGestureStep*)step_item->getUserdata();
	if (step->getType() != STEP_WAIT) return;

	mWaitTimeCheck->set(TRUE);
	onCommitWait();
}


// static
void LLPreviewGesture::onKeystrokeCommit(LLLineEditor* caller,
										 void* data)
{
	// Just commit every keystroke
	((LLPreviewGesture*)data)->onCommitSetDirty();
}

void LLPreviewGesture::onClickAdd()
{
	LLScrollListItem* library_item = mLibraryList->getFirstSelected();
	if (!library_item) return;

	S32 library_item_index = mLibraryList->getFirstSelectedIndex();

	const LLScrollListCell* library_cell = library_item->getColumn(0);
	const std::string& library_text = library_cell->getValue().asString();

	if( library_item_index >= STEP_EOF )
	{
		llerrs << "Unknown step type: " << library_text << llendl;
		return;
	}

	addStep( (EStepType)library_item_index );
	mDirty = TRUE;
	refresh();
}

LLScrollListItem* LLPreviewGesture::addStep( const EStepType step_type )
{
	// Order of enum EStepType MUST match the library_list element in floater_preview_gesture.xml

	LLGestureStep* step = NULL;
	switch( step_type)
	{
		case STEP_ANIMATION:
			step = new LLGestureStepAnimation();
			break;
		case STEP_SOUND:
			step = new LLGestureStepSound();
			break;
		case STEP_CHAT:
			step = new LLGestureStepChat();
			break;
		case STEP_WAIT:
			step = new LLGestureStepWait();
			break;
		default:
			llerrs << "Unknown step type: " << (S32)step_type << llendl;
			return NULL;
	}

	// Create an enabled item with this step
	LLSD row;
	row["columns"][0]["value"] = getLabel(step->getLabel());
	row["columns"][0]["font"] = "SANSSERIF_SMALL";
	LLScrollListItem* step_item = mStepList->addElement(row);
	step_item->setUserdata(step);

	// And move selection to the list on the right
	mLibraryList->deselectAllItems();
	mStepList->deselectAllItems();

	step_item->setSelected(TRUE);

	return step_item;
}

// static
std::string LLPreviewGesture::getLabel(std::vector<std::string> labels)
{
	std::vector<std::string> v_labels = labels ;
	std::string result("");
	
	if( v_labels.size() != 2)
	{
		return result;
	}
	
	if(v_labels[0]=="Chat")
	{
		result=LLTrans::getString("Chat Message");
	}
    else if(v_labels[0]=="Sound")	
	{
		result=LLTrans::getString("Sound");
	}
	else if(v_labels[0]=="Wait")
	{
		result=LLTrans::getString("Wait");
	}
	else if(v_labels[0]=="AnimFlagStop")
	{
		result=LLTrans::getString("AnimFlagStop");
	}
	else if(v_labels[0]=="AnimFlagStart")
	{
		result=LLTrans::getString("AnimFlagStart");
	}

	// lets localize action value
	std::string action = v_labels[1];
	if ("None" == action)
	{
		action = LLTrans::getString("GestureActionNone");
	}
	else if ("until animations are done" == action)
	{
		//action = LLFloaterReg::getInstance("preview_gesture")->getChild<LLCheckBoxCtrl>("wait_anim_check")->getLabel();
		//Worst. Thing. Ever. We are in a static function. Find any existing gesture preview and grab the label from its 'wait_anim_check' element.
		for(preview_map_t::iterator it = LLPreview::sInstances.begin(); it != LLPreview::sInstances.end();++it)
		{
			LLPreviewGesture* pPreview = dynamic_cast<LLPreviewGesture*>(it->second);
			if(pPreview)
			{
				pPreview->getChild<LLCheckBoxCtrl>("wait_anim_check")->getLabel();
				break;
			}
		}
		
	}
	result.append(action);
	return result;
	
}

void LLPreviewGesture::onClickUp()
{
	S32 selected_index = mStepList->getFirstSelectedIndex();
	if (selected_index > 0)
	{
		mStepList->swapWithPrevious(selected_index);
		mDirty = TRUE;
		refresh();
	}
}

void LLPreviewGesture::onClickDown()
{
	S32 selected_index = mStepList->getFirstSelectedIndex();
	if (selected_index < 0) return;

	S32 count = mStepList->getItemCount();
	if (selected_index < count-1)
	{
		mStepList->swapWithNext(selected_index);
		mDirty = TRUE;
		refresh();
	}
}

void LLPreviewGesture::onClickDelete()
{
	LLScrollListItem* item = mStepList->getFirstSelected();
	S32 selected_index = mStepList->getFirstSelectedIndex();
	if (item && selected_index >= 0)
	{
		LLGestureStep* step = (LLGestureStep*)item->getUserdata();
		delete step;

		mStepList->deleteSingleItem(selected_index);

		mDirty = TRUE;
		refresh();
	}
}

void LLPreviewGesture::onCommitActive()
{
	if (!LLGestureMgr::instance().isGestureActive(mItemUUID))
	{
		LLGestureMgr::instance().activateGesture(mItemUUID);
	}
	else
	{
		LLGestureMgr::instance().deactivateGesture(mItemUUID);
	}

	// Make sure the (active) label in the inventory gets updated.
	LLViewerInventoryItem* item = gInventory.getItem(mItemUUID);
	if (item)
	{
		gInventory.updateItem(item);
		gInventory.notifyObservers();
	}

	refresh();
}

void LLPreviewGesture::onClickSave()
{
	saveIfNeeded();
}

void LLPreviewGesture::onClickPreview()
{
	if (!mPreviewGesture)
	{
		// make temporary gesture
		mPreviewGesture = createGesture();

		// add a callback
		mPreviewGesture->mDoneCallback = boost::bind(&LLPreviewGesture::onDonePreview,this,_1);

		// set the button title
		mPreviewBtn->setLabel(getString("stop_txt"));

		// play it, and delete when done
		LLGestureMgr::instance().playGesture(mPreviewGesture);

		refresh();
	}
	else
	{
		// Will call onDonePreview() below
		LLGestureMgr::instance().stopGesture(mPreviewGesture);

		refresh();
	}
}


// static
void LLPreviewGesture::onDonePreview(LLMultiGesture* gesture)
{
	mPreviewBtn->setLabel(getString("preview_txt"));

	delete mPreviewGesture;
	mPreviewGesture = NULL;

	refresh();
}
