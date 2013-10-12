/** 
 * @file llfloatercustomize.cpp
 * @brief The customize avatar floater, triggered by "Appearance..."
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

#include "llviewerprecompiledheaders.h"

#include "llfloatercustomize.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llagentwearables.h"
#include "llappearancemgr.h"
#include "llmakeoutfitdialog.h"
#include "llmorphview.h"
#include "llnotificationsutil.h"
#include "lloutfitobserver.h"
#include "llpaneleditwearable.h"
#include "llscrollcontainer.h"
#include "llscrollingpanelparam.h"
#include "lltextbox.h"
#include "lltoolmorph.h"
#include "lluictrlfactory.h"
#include "llviewerinventory.h"
#include "llviewerwearable.h"
#include "llvoavatarself.h"

#include "statemachine/aifilepicker.h"
#include "llxmltree.h"
#include "hippogridmanager.h"

using namespace LLAvatarAppearanceDefines;

// *TODO:translate : The ui xml for this really needs to be integrated with the appearance paramaters

/////////////////////////////////////////////////////////////////////
// LLFloaterCustomizeObserver

class LLFloaterCustomizeObserver : public LLInventoryObserver
{
public:
	LLFloaterCustomizeObserver(LLFloaterCustomize* fc) : mFC(fc) {}
	virtual ~LLFloaterCustomizeObserver() {}
	virtual void changed(U32 mask) { mFC->getCurrentWearablePanel()->updateScrollingPanelUI(); }
protected:
	LLFloaterCustomize* mFC;
};

////////////////////////////////////////////////////////////////////////////

// Local Constants 

BOOL edit_wearable_for_teens(LLWearableType::EType type)
{
	switch(type)
	{
	case LLWearableType::WT_UNDERSHIRT:
	case LLWearableType::WT_UNDERPANTS:
		return FALSE;
	default:
		return TRUE;
	}
}

////////////////////////////////////////////////////////////////////////////

void updateAvatarHeightDisplay()
{
	if (LLFloaterCustomize::instanceExists() && isAgentAvatarValid())
	{
		F32 avatar_size = (gAgentAvatarp->mBodySize.mV[VZ]) + (F32)0.17; //mBodySize is actually quite a bit off.
		LLFloaterCustomize::getInstance()->getChild<LLTextBox>("HeightTextM")->setValue(llformat("%.2f", avatar_size) + "m");
		F32 feet = avatar_size / 0.3048;
		F32 inches = (feet - (F32)((U32)feet)) * 12.0;
		LLFloaterCustomize::getInstance()->getChild<LLTextBox>("HeightTextI")->setValue(llformat("%d'%d\"", (U32)feet, (U32)inches));
	}
 }

/////////////////////////////////////////////////////////////////////
// LLFloaterCustomize

struct WearablePanelData
{
	WearablePanelData(LLFloaterCustomize* floater, LLWearableType::EType type)
		: mFloater(floater), mType(type) {}
	LLFloaterCustomize* mFloater;
	LLWearableType::EType mType;
};

LLFloaterCustomize::LLFloaterCustomize()
:	LLFloater(std::string("customize")),
	mScrollingPanelList( NULL ),
	mInventoryObserver(NULL),
	mCurrentWearableType(LLWearableType::WT_INVALID)
{
	memset(&mWearablePanelList[0],0,sizeof(char*)*LLWearableType::WT_COUNT); //Initialize to 0

	gSavedSettings.setU32("AvatarSex", (gAgentAvatarp->getSex() == SEX_MALE) );

	mResetParams = new LLVisualParamReset();
	
	// create the observer which will watch for matching incoming inventory
	mInventoryObserver = new LLFloaterCustomizeObserver(this);
	gInventory.addObserver(mInventoryObserver);

	LLOutfitObserver& outfit_observer =  LLOutfitObserver::instance();
	outfit_observer.addBOFReplacedCallback(boost::bind(&LLFloaterCustomize::refreshCurrentOutfitName, this, ""));
	outfit_observer.addBOFChangedCallback(boost::bind(&LLFloaterCustomize::refreshCurrentOutfitName, this, ""));
	outfit_observer.addCOFChangedCallback(boost::bind(&LLFloaterCustomize::refreshCurrentOutfitName, this, ""));

	LLCallbackMap::map_t factory_map;
	const std::string &invalid_name = LLWearableType::getTypeName(LLWearableType::WT_INVALID);
	for(U32 type=LLWearableType::WT_SHAPE;type<LLWearableType::WT_INVALID;++type)
	{
		std::string name = LLWearableType::getTypeName((LLWearableType::EType)type);
		if(name != invalid_name)
		{
			name[0] = toupper(name[0]);
			factory_map[name] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, (LLWearableType::EType)type) ) );
		}
	}

	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_customize.xml", &factory_map);

	fetchInventory();	//May as well start requesting now.

	LLFloater::open();

	setVisible(false);
}

LLFloaterCustomize::~LLFloaterCustomize()
{
	llinfos << "Destroying LLFloaterCustomize" << llendl;
	mResetParams = NULL;
	gInventory.removeObserver(mInventoryObserver);
	delete mInventoryObserver;
}

// virtual
BOOL LLFloaterCustomize::postBuild()
{
	getChild<LLUICtrl>("Make Outfit")->setCommitCallback(boost::bind(&LLFloaterCustomize::onBtnMakeOutfit, this));
	getChild<LLUICtrl>("Save Outfit")->setCommitCallback(boost::bind(&LLAppearanceMgr::updateBaseOutfit, LLAppearanceMgr::getInstance()));
	refreshCurrentOutfitName(); // Initialize tooltip for save outfit button
	getChild<LLUICtrl>("Ok")->setCommitCallback(boost::bind(&LLFloaterCustomize::onBtnOk, this));
	getChild<LLUICtrl>("Cancel")->setCommitCallback(boost::bind(&LLFloater::onClickClose, this));

    // reX
	getChild<LLUICtrl>("Import")->setCommitCallback(boost::bind(&LLFloaterCustomize::onBtnImport, this));
	getChild<LLUICtrl>("Export")->setCommitCallback(boost::bind(&LLFloaterCustomize::onBtnExport, this));
	
	// Tab container
	LLTabContainer* tab_container = getChild<LLTabContainer>("customize tab container");
	if(tab_container)
	{
		tab_container->setCommitCallback(boost::bind(&LLFloaterCustomize::onTabChanged, this, _2));
		tab_container->setValidateCallback(boost::bind(&LLFloaterCustomize::onTabPrecommit, this, _1, _2));
	}

	// Remove underwear panels for teens
	if (gAgent.isTeen())
	{
		if (tab_container)
		{
			LLPanel* panel = tab_container->getPanelByName("Undershirt");
			if (panel) tab_container->removeTabPanel(panel);
			panel = tab_container->getPanelByName("Underpants");
			if (panel) tab_container->removeTabPanel(panel);
		}
	}
	
	// Scrolling Panel
	initScrollingPanelList();
	
	return TRUE;
}

void LLFloaterCustomize::refreshCurrentOutfitName(const std::string& name)
{
	LLUICtrl* save_outfit_btn = getChild<LLUICtrl>("Save Outfit");
	// Set current outfit status (wearing/unsaved).
	bool dirty = LLAppearanceMgr::getInstance()->isOutfitDirty();
	//std::string cof_status_str = getString(dirty ? "Unsaved Changes" : "Now Wearing");
	//mOutfitStatus->setText(cof_status_str);
	save_outfit_btn->setEnabled(dirty); // No use saving unless dirty

	if (name == "")
	{
		std::string outfit_name;
		if (LLAppearanceMgr::getInstance()->getBaseOutfitName(outfit_name))
		{
				//mCurrentLookName->setText(outfit_name);
				LLStringUtil::format_map_t args;
				args["[OUTFIT]"] = outfit_name;
				save_outfit_btn->setToolTip(getString("Save changes to", args));
				return;
		}

		std::string string_name = gAgentWearables.isCOFChangeInProgress() ? "Changing outfits" : "No Outfit";
		//mCurrentLookName->setText(getString(string_name));
		save_outfit_btn->setToolTip(getString(string_name));
		//mOpenOutfitBtn->setEnabled(FALSE);
		save_outfit_btn->setEnabled(false); // Can't save right now
	}
	else
	{
		//mCurrentLookName->setText(name);
		LLStringUtil::format_map_t args;
		args["[OUTFIT]"] = name;
		save_outfit_btn->setToolTip(getString("Save changes to", args));
		// Can't just call update verbs since the folder link may not have been created yet.
		//mOpenOutfitBtn->setEnabled(TRUE);
	}
}

//static
void LLFloaterCustomize::editWearable(LLViewerWearable* wearable, bool disable_camera_switch)
{
	if(!wearable)
		return;
	LLFloaterCustomize::getInstance()->setCurrentWearableType(wearable->getType(), disable_camera_switch);
}

//static
void LLFloaterCustomize::show()
{
	if(!LLFloaterCustomize::instanceExists())
	{
		const BOOL disable_camera_switch = LLWearableType::getDisableCameraSwitch(LLWearableType::WT_SHAPE);
		LLFloaterCustomize::getInstance()->setCurrentWearableType(LLWearableType::WT_SHAPE, disable_camera_switch);
	}
	else
		LLFloaterCustomize::getInstance()->setFrontmost(true);
}

// virtual
void LLFloaterCustomize::onClose(bool app_quitting)
{
	// since this window is potentially staying open, push to back to let next window take focus
	gFloaterView->sendChildToBack(this);
	// askToSaveIfDirty will call delayedClose immediately if there's nothing to save.
	askToSaveIfDirty( boost::bind(&LLFloaterCustomize::delayedClose, this, _1, app_quitting) );
}

void LLFloaterCustomize::delayedClose(bool proceed, bool app_quitting)
{
	if(proceed)
	{
		LLVOAvatarSelf::onCustomizeEnd();
		LLFloater::onClose(app_quitting);
		if(gAgentAvatarp)gAgentAvatarp->mSpecialRenderMode = 0;
	}
}

////////////////////////////////////////////////////////////////////////////

void LLFloaterCustomize::setCurrentWearableType( LLWearableType::EType type, bool disable_camera_switch )
{
	if( mCurrentWearableType != type )
	{
		mCurrentWearableType = type; 

		if (!gAgentCamera.cameraCustomizeAvatar())
		{
			LLVOAvatarSelf::onCustomizeStart(disable_camera_switch);
		}
		else if(!gSavedSettings.getBOOL("AppearanceCameraMovement") || disable_camera_switch)	//Break out to free camera.
		{
			gAgentCamera.changeCameraToDefault();
			gAgentCamera.resetView();
		}

		S32 type_int = (S32)type;
		if(mWearablePanelList[type_int])
		{
			std::string panelname = mWearablePanelList[type_int]->getName();
			childShowTab("customize tab container", panelname);
			switchToDefaultSubpart();
		}

		updateVisiblity(disable_camera_switch);
	}
}

// reX: new function
void LLFloaterCustomize::onBtnImport()
{
	AIFilePicker* filepicker = AIFilePicker::create();
	filepicker->open(FFLOAD_XML);
	filepicker->run(boost::bind(&LLFloaterCustomize::onBtnImport_continued, this, filepicker));
}

void LLFloaterCustomize::onBtnImport_continued(AIFilePicker* filepicker)
{
	if (!filepicker->hasFilename())
	{
		// User canceled import.
		return;
	}

	// Find the editted wearable.
	LLPanelEditWearable* panel_edit_wearable = getCurrentWearablePanel();
	LLViewerWearable* edit_wearable = panel_edit_wearable->getWearable();

	std::string const filename = filepicker->getFilename();
	LLSD args(LLSD::emptyMap());
	args["FILE"] = gDirUtilp->getBaseFileName(filename);

	LLXmlTree xml;
	BOOL success = xml.parseFile(filename, FALSE);
	if (!success)
	{
		LLNotificationsUtil::add("AIXMLImportParseError", args);
		return;
	}
	LLXmlTreeNode* root = xml.getRoot();
	if (!root)
	{
		llwarns << "No root node found in wearable import file: " << filename << llendl;
		LLNotificationsUtil::add("AIXMLImportParseError", args);
		return;
	}

	//-------------------------------------------------------------------------
	// <linden_genepool version="1.0" [metaversion="?"]> (root)
	//-------------------------------------------------------------------------
	if (!root->hasName("linden_genepool"))
	{
		llwarns << "Invalid wearable import file (missing linden_genepool header): " << filename << llendl;
		LLNotificationsUtil::add("AIXMLImportRootTypeError", args);
		return;
	}
	static LLStdStringHandle const version_string = LLXmlTree::addAttributeString("version");
	std::string version;
	if (!root->getFastAttributeString(version_string, version) || (version != "1.0"))
	{
		llwarns << "Invalid or incompatible linden_genepool version: " << version << " in file: " << filename << llendl;
		args["TAG"] = "version";
		args["VERSIONMAJOR"] = "1";
		LLNotificationsUtil::add("AIXMLImportRootVersionError", args);
		return;
	}
	static LLStdStringHandle const metaversion_string = LLXmlTree::addAttributeString("metaversion");
	std::string metaversion;
	U32 metaversion_major;
	if (!root->getFastAttributeString(metaversion_string, metaversion))
	{
		llwarns << "Invalid linden_genepool metaversion: " << metaversion << " in file: " << filename << llendl;
		metaversion_major = 0;
	}
	else if (!LLStringUtil::convertToU32(metaversion, metaversion_major) || metaversion_major > 1)
	{
		llwarns << "Invalid or incompatible linden_genepool metaversion: " << metaversion << " in file: " << filename << llendl;
		args["TAG"] = "metaversion";
		args["VERSIONMAJOR"] = "1";
		LLNotificationsUtil::add("AIXMLImportRootVersionError", args);
		return;
	}

	//-------------------------------------------------------------------------
	// <meta gridnick="secondlife" date="2013-07-20T15:27:55.80Z">
	//-------------------------------------------------------------------------
	std::string gridnick;
	LLDate date;
	bool different_grid = false;		// By default assume it was exported on the same grid as we're on now.
	bool mixed_grids = false;			// Set to true if two different grids (might) share UUIDs. Currently only "secondlife" and "secondlife_beta".
	if (metaversion_major >= 1)
	{
		static LLStdStringHandle const gridnick_string = LLXmlTree::addAttributeString("gridnick");
		static LLStdStringHandle const date_string = LLXmlTree::addAttributeString("date");
		std::string date_s;
		bool invalid = true;
		LLXmlTreeNode* meta_node = root->getChildByName("meta");
		if (!meta_node)
		{
			llwarns << "No meta (1) in wearable import file: " << filename << llendl;
		}
		else if (!meta_node->getFastAttributeString(gridnick_string, gridnick))
		{
			llwarns << "meta tag in file: " << filename << " is missing the 'gridnick' parameter." << llendl;
		}
		else if (!meta_node->getFastAttributeString(date_string, date_s) || !date.fromString(date_s))
		{
			llwarns << "meta tag in file: " << filename << " is missing or invalid 'date' parameter." << llendl;
		}
		else
		{
			invalid = false;
			std::string current_gridnick = gHippoGridManager->getConnectedGrid()->getGridNick();
			different_grid = gridnick != current_gridnick;
			mixed_grids = (gridnick == "secondlife" && current_gridnick == "secondlife_beta") ||
			              (gridnick == "secondlife_beta" && current_gridnick == "secondlife");
		}
		if (invalid)
		{
			LLNotificationsUtil::add("AIXMLImportInvalidError", args);
			return;
		}
	}

	static LLStdStringHandle const name_string = LLXmlTree::addAttributeString("name");

	//-------------------------------------------------------------------------
	// <archetype name="???">
	//-------------------------------------------------------------------------
	LLXmlTreeNode* archetype_node = root->getChildByName("archetype");
	if (!archetype_node)
	{
		llwarns << "No archetype in wearable import file: " << filename << llendl;
		LLNotificationsUtil::add("AIXMLImportInvalidError", args);
		return;
	}
	// Legacy that name="" exists. Using it as human (only) readable type label of contents. Don't use it for anything else because it might not be set.
	std::string label = "???";
	if (metaversion_major >= 1)
	{
		if (!archetype_node->getFastAttributeString(name_string, label))
		{
			llwarns << "archetype tag in file: " << filename << " is missing the 'name' parameter." << llendl;
		}
	}

	//-------------------------------------------------------------------------
	// <meta path="Clothing" name="New Shirt27" description="Some description"/>
	//-------------------------------------------------------------------------
	std::string path;
	std::string wearable_name;
	std::string wearable_description;
	if (metaversion_major >= 1)
	{
		static LLStdStringHandle const path_string = LLXmlTree::addAttributeString("path");
		static LLStdStringHandle const description_string = LLXmlTree::addAttributeString("description");
		bool invalid = true;
		LLXmlTreeNode* meta_node = archetype_node->getChildByName("meta");
		if (!meta_node)
		{
			llwarns << "No meta (2) in wearable import file: " << filename << llendl;
		}
		else if (!meta_node->getFastAttributeString(path_string, path))
		{
			llwarns << "meta tag in file: " << filename << " is missing the 'path' parameter." << llendl;
		}
		else if (!meta_node->getFastAttributeString(name_string, wearable_name))
		{
			llwarns << "meta tag in file: " << filename << " is missing the 'name' parameter." << llendl;
		}
		else if (!meta_node->getFastAttributeString(description_string, wearable_description))
		{
			llwarns << "meta tag in file: " << filename << " is missing the 'description' parameter." << llendl;
		}
		else
		{
			invalid = false;
		}
		if (invalid)
		{
			LLNotificationsUtil::add("AIXMLImportInvalidError", args);
			return;
		}
	}

	// Parse the XML content.
	static LLStdStringHandle const id_string = LLXmlTree::addAttributeString("id");
	static LLStdStringHandle const value_string = LLXmlTree::addAttributeString("value");
	static LLStdStringHandle const te_string = LLXmlTree::addAttributeString("te");
	static LLStdStringHandle const uuid_string = LLXmlTree::addAttributeString("uuid");
	bool found_param = false;
	bool found_texture = false;
	for(LLXmlTreeNode* child = archetype_node->getFirstChild(); child; child = archetype_node->getNextChild())
	{
		if (child->hasName("param"))
		{
			std::string id_s;
			U32 id;
			std::string value_s;
			F32 value;
			if (!child->getFastAttributeString(id_string, id_s) || !LLStringUtil::convertToU32(id_s, id) ||
				!child->getFastAttributeString(value_string, value_s) || !LLStringUtil::convertToF32(value_s, value))
			{
				llwarns << "Possible syntax error or corruption for <param id=... value=... /> node in " << filename << llendl;
				continue;
			}
			LLVisualParam* visual_param = edit_wearable->getVisualParam(id);
			if (visual_param)
			{
				found_param = true;
				visual_param->setWeight(value, FALSE);
			}
		}
		else if (child->hasName("texture"))
		{
			std::string te_s;
			S32 te;
			std::string uuid_s;
			LLUUID uuid;
			if (!child->getFastAttributeString(te_string, te_s) || !LLStringUtil::convertToS32(te_s, te) || te < 0 || te >= TEX_NUM_INDICES ||
				!child->getFastAttributeString(uuid_string, uuid_s) || !uuid.set(uuid_s, TRUE))
			{
				llwarns << "Possible syntax error or corruption for <texture te=... uuid=... /> node in " << filename << llendl;
				continue;
			}
			ETextureIndex te_index = (ETextureIndex)te;
			LLWearableType::EType te_wearable_type = LLAvatarAppearanceDictionary::getTEWearableType(te_index);
			if (te_wearable_type == edit_wearable->getType())
			{
				found_texture = true;
				if (!different_grid || mixed_grids)
				{
					panel_edit_wearable->setNewImageID(te_index, uuid);
				}
			}
		}
	}
	if (found_param || found_texture)
	{
		edit_wearable->writeToAvatar(gAgentAvatarp);
		gAgentAvatarp->updateVisualParams();
		panel_edit_wearable->updateScrollingPanelUI();
		if (found_texture && different_grid)
		{
			args["EXPORTGRID"] = gridnick;
			args["CURRENTGRID"] = gHippoGridManager->getConnectedGrid()->getGridNick();
			if (mixed_grids)
			{
				LLNotificationsUtil::add("AIXMLImportMixedGrid", args);
			}
			else
			{
				LLNotificationsUtil::add("AIXMLImportDifferentGrid", args);
			}
		}
	}
	else
	{
		args["TYPE"] = panel_edit_wearable->LLPanel::getLabel();
		args["ARCHETYPENAME"] = label;
		LLNotificationsUtil::add("AIXMLImportWearableTypeMismatch", args);
	}
}

// reX: new function
void LLFloaterCustomize::onBtnExport()
{
	// Find the editted wearable.
	LLPanelEditWearable* panel_edit_wearable = getCurrentWearablePanel();
	LLViewerWearable* edit_wearable = panel_edit_wearable->getWearable();
	U32 edit_index = panel_edit_wearable->getIndex();
	std::string const& name = edit_wearable->getName();

	// Determine if the currently selected wearable is modifiable.
	LLWearableType::EType edit_type = getCurrentWearableType();
	bool is_modifiable = false;
	LLViewerWearable* old_wearable = gAgentWearables.getViewerWearable(edit_type, edit_index);
	if (old_wearable)
	{
		LLViewerInventoryItem* item = gInventory.getItem(old_wearable->getItemID());
		if (item)
		{
			LLPermissions const& perm = item->getPermissions();
			// Modifiable means the user can see the sliders and type them over into a file anyway.
			is_modifiable = perm.allowModifyBy(gAgent.getID(), gAgent.getGroupID());
		}
	}

	if (!is_modifiable)
	{
		// We should never get here, because in that case the Export button is disabled.
		llwarns << "Cannot export current wearable \"" << name << "\" of type " << (int)edit_type << "because user lacks modify permissions." << llendl;
		return;
	}

	std::string file_name = edit_wearable->getName() + "_" + gHippoGridManager->getConnectedGrid()->getGridNick() + "_" + edit_wearable->getTypeName() + "?000.xml";
	std::string default_path = gDirUtilp->getExpandedFilename(LL_PATH_LOGS, "");

	AIFilePicker* filepicker = AIFilePicker::create();
	filepicker->open(file_name, FFSAVE_XML, default_path, "archetype");
	filepicker->run(boost::bind(&LLFloaterCustomize::onBtnExport_continued, edit_wearable, filepicker));
}

//static
void LLFloaterCustomize::onBtnExport_continued(LLViewerWearable* edit_wearable, AIFilePicker* filepicker)
{
	if (!filepicker->hasFilename())
	{
		// User canceled export.
		return;
	}

	std::string filename = filepicker->getFilename();
	LLSD args(LLSD::emptyMap());
	args["FILE"] = filename;

	LLAPRFile outfile;
	outfile.open(filename, LL_APR_WB);
	if (!outfile.getFileHandle())
	{
		llwarns << "Could not open \"" << filename << "\" for writing." << llendl;
		LLNotificationsUtil::add("AIXMLExportWriteError", args);
		return;
	}

	LLVOAvatar::dumpArchetypeXML_header(outfile, edit_wearable->getTypeName());
	edit_wearable->archetypeExport(outfile);
	LLVOAvatar::dumpArchetypeXML_footer(outfile);
}

void LLFloaterCustomize::onBtnOk()
{
	saveCurrentWearables();
	gFloaterView->sendChildToBack(this);
	close(false);
}

void LLFloaterCustomize::onBtnMakeOutfit()
{
	new LLMakeOutfitDialog(true); // LLMakeOutfitDialog deletes itself.
}

////////////////////////////////////////////////////////////////////////////

// static
void* LLFloaterCustomize::createWearablePanel(void* userdata)
{
	WearablePanelData* data = (WearablePanelData*)userdata;
	LLWearableType::EType type = data->mType;
	LLPanelEditWearable* &panel = data->mFloater->mWearablePanelList[type];
	if (!gAgent.isTeen() || edit_wearable_for_teens(type))
		panel = new LLPanelEditWearable( type, data->mFloater );
	else
		panel = NULL;
	delete data;
	return panel;
}

////////////////////////////////////////////////////////////////////////////

void LLFloaterCustomize::switchToDefaultSubpart()
{
	getCurrentWearablePanel()->showDefaultSubpart();
}

void LLFloaterCustomize::draw()
{
	if( isMinimized() )
	{
		LLFloater::draw();
		return;
	}

	// only do this if we are in the customize avatar mode
	// and not transitioning into or out of it
	// *TODO: This is a sort of expensive call, which only needs
	// to be called when the tabs change or an inventory item
	// arrives. Figure out some way to avoid this if possible.
	updateInventoryUI();
	
	updateAvatarHeightDisplay();

	LLScrollingPanelParam::sUpdateDelayFrames = 0;
	
	LLFloater::draw();
}

bool LLFloaterCustomize::isWearableDirty() const
{
	LLWearableType::EType cur = getCurrentWearableType();
	for(U32 i = 0; i < gAgentWearables.getWearableCount(cur); ++i)
	{
		LLViewerWearable* wearable = gAgentWearables.getViewerWearable(cur,i);
		if(wearable && wearable->isDirty())
			return TRUE;
	}
	return FALSE;
}

bool LLFloaterCustomize::onTabPrecommit( LLUICtrl* ctrl, const LLSD& param )
{
	std::string panel_name = param.asString();
	for(U32 type=LLWearableType::WT_SHAPE;type<LLWearableType::WT_INVALID;++type)
	{
		std::string type_name = LLWearableType::getTypeName((LLWearableType::EType)type);
		std::transform(panel_name.begin(), panel_name.end(), panel_name.begin(), tolower); 

		if(type_name == panel_name)
		{
			if(mCurrentWearableType != type)
			{
				askToSaveIfDirty(boost::bind(&LLFloaterCustomize::onCommitChangeTab, this, _1, (LLTabContainer*)ctrl, param.asString(), (LLWearableType::EType)type));
				return false;
			}
		}
	}
	return true;
}


void LLFloaterCustomize::onTabChanged( const LLSD& param )
{
	std::string panel_name = param.asString();
	for(U32 type=LLWearableType::WT_SHAPE;type<LLWearableType::WT_INVALID;++type)
	{
		std::string type_name = LLWearableType::getTypeName((LLWearableType::EType)type);
		std::transform(panel_name.begin(), panel_name.end(), panel_name.begin(), tolower); 

		if(type_name == panel_name)
		{
			const BOOL disable_camera_switch = LLWearableType::getDisableCameraSwitch((LLWearableType::EType)type);
			setCurrentWearableType((LLWearableType::EType)type, disable_camera_switch);
			break;
		}
	}
}

void LLFloaterCustomize::onCommitChangeTab(BOOL proceed, LLTabContainer* ctrl, std::string panel_name, LLWearableType::EType type)
{
	if (!proceed)
	{
		return;
	}

	const BOOL disable_camera_switch = LLWearableType::getDisableCameraSwitch(type);
	setCurrentWearableType(type, disable_camera_switch);
	ctrl->selectTabByName(panel_name);
}



////////////////////////////////////////////////////////////////////////////

const S32 LOWER_BTN_HEIGHT = 18 + 8;

const S32 FLOATER_CUSTOMIZE_BUTTON_WIDTH = 82;
const S32 FLOATER_CUSTOMIZE_BOTTOM_PAD = 30;
const S32 LINE_HEIGHT = 16;
const S32 HEADER_PAD = 8;
const S32 HEADER_HEIGHT = 3 * (LINE_HEIGHT + LLFLOATER_VPAD) + (2 * LLPANEL_BORDER_WIDTH) + HEADER_PAD; 

void LLFloaterCustomize::initScrollingPanelList()
{
	LLScrollContainer* scroll_container =
		getChild<LLScrollContainer>("panel_container");
	// LLScrollingPanelList's do not import correctly 
// 	mScrollingPanelList = LLUICtrlFactory::getScrollingPanelList(this, "panel_list");
	mScrollingPanelList = new LLScrollingPanelList(std::string("panel_list"), LLRect());
	if (scroll_container)
	{
		scroll_container->setScrolledView(mScrollingPanelList);
		scroll_container->addChild(mScrollingPanelList);
	}
}

void LLFloaterCustomize::wearablesChanged(LLWearableType::EType type)
{
	llassert( type < LLWearableType::WT_COUNT );
	gSavedSettings.setU32("AvatarSex", (gAgentAvatarp->getSex() == SEX_MALE) );
	
	LLPanelEditWearable* panel = mWearablePanelList[ type ];
	if( panel )
	{
		panel->wearablesChanged();
	}
}

void LLFloaterCustomize::updateVisiblity(bool force_disable_camera_switch/*=false*/)
{
	if(!getVisible())
	{
		if(force_disable_camera_switch || !gAgentCamera.cameraCustomizeAvatar() || !gAgentCamera.getCameraAnimating() || (gMorphView && gMorphView->getVisible()))
		{
			if (gAgentAvatarp && gSavedSettings.getBOOL("AppearanceSpecialLighting")) gAgentAvatarp->mSpecialRenderMode = 3;
			setVisibleAndFrontmost(TRUE);
		}
	}
}

void LLFloaterCustomize::updateScrollingPanelList()
{
	getCurrentWearablePanel()->updateScrollingPanelList();
}

void LLFloaterCustomize::askToSaveIfDirty( boost::function<void (BOOL)> cb )
{
	if(isWearableDirty())
	{
		// Ask if user wants to save, then continue to next step afterwards
		mNextStepAfterSaveCallback.connect(cb);

		// Bring up view-modal dialog: Save changes? Yes, No, Cancel
		LLNotificationsUtil::add("SaveClothingBodyChanges", LLSD(), LLSD(),
			boost::bind(&LLFloaterCustomize::onSaveDialog, this, _1, _2));
	}
	else
	{
		cb(TRUE);	//just call it immediately.
	}
}

void LLFloaterCustomize::saveCurrentWearables()
{
	LLWearableType::EType cur = getCurrentWearableType();

	for(U32 i = 0;i < gAgentWearables.getWearableCount(cur);++i)
	{
		LLViewerWearable* wearable = gAgentWearables.getViewerWearable(cur,i);
		if(wearable && wearable->isDirty())
		{
			/*
			===============================================================================================
			Copy-pasted some code from LLPanelEditWearable::saveChanges instead of just calling saveChanges, as
			we only have one 'active' panel per wearable type, not per layer. The panels just update when
			layer of focus is changed. Easier just to do it right here manually.
			===============================================================================================
			*/ 
			// Find an existing link to this wearable's inventory item, if any, and its description field.
			if(gAgentAvatarp->isUsingServerBakes())
			{
				LLInventoryItem *link_item = NULL;
				std::string description;
				LLInventoryModel::item_array_t links =
					LLAppearanceMgr::instance().findCOFItemLinks(wearable->getItemID());
				if (links.size()>0)
				{
					link_item = links.get(0).get();
					if (link_item && link_item->getIsLinkType())
					{
						description = link_item->getActualDescription();
					}
				}
				// Make another copy of this link, with the same
				// description.  This is needed to bump the COF
				// version so texture baking service knows appearance has changed.
				if (link_item)
				{
					// Create new link
					link_inventory_item( gAgent.getID(),
										 link_item->getLinkedUUID(),
										 LLAppearanceMgr::instance().getCOF(),
										 link_item->getName(),
										 description,
										 LLAssetType::AT_LINK,
										 NULL);
					// Remove old link
					gInventory.purgeObject(link_item->getUUID());
				}
			}
			gAgentWearables.saveWearable( cur, i );
		}
	}
}

bool LLFloaterCustomize::onSaveDialog(const LLSD& notification, const LLSD& response )
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if(option == 0)
	{
		saveCurrentWearables();
	}
	else
	{
		LLWearableType::EType cur = getCurrentWearableType();
		for(U32 i = 0;i < gAgentWearables.getWearableCount(cur);++i)
		{
			LLViewerWearable* wearable = gAgentWearables.getViewerWearable(cur,i);
			if(wearable && wearable->isDirty())
			{
				gAgentWearables.revertWearable( cur, i );
			}
		}
	}

	mNextStepAfterSaveCallback(option < 2);
	mNextStepAfterSaveCallback.disconnect_all_slots();	//Should this be done?

	return false;
}

// fetch observer
class LLCurrentlyWorn : public LLInventoryFetchItemsObserver
{
public:
	LLCurrentlyWorn(const uuid_vec_t& item_ids) : LLInventoryFetchItemsObserver(item_ids){}
	~LLCurrentlyWorn() {}
	virtual void done() { /* no operation necessary */}
};

void LLFloaterCustomize::fetchInventory()
{
	// Fetch currently worn items
	uuid_vec_t ids;
	LLUUID item_id;
	for(S32 type = (S32)LLWearableType::WT_SHAPE; type < (S32)LLWearableType::WT_COUNT; ++type)
	{
		for(U32 i = 0; i < gAgentWearables.getWearableCount((LLWearableType::EType)type); ++i)
		{
			item_id = gAgentWearables.getWearableItemID((LLWearableType::EType)type, i);
			if(item_id.notNull())
			{
				ids.push_back(item_id);
			}
		}
	}

	// Fire & forget. The mInventoryObserver will catch inventory
	// updates and correct the UI as necessary.
	LLCurrentlyWorn worn(ids);
	worn.startFetch();
}

void LLFloaterCustomize::updateInventoryUI()
{
	BOOL all_complete = TRUE;
	BOOL is_complete = FALSE;
	U32 perm_mask = 0x0;
	LLPanelEditWearable* panel;
	LLViewerInventoryItem* item;
	for(S32 i = 0; i < LLWearableType::WT_COUNT; ++i)
	{
		item = NULL;
		panel = mWearablePanelList[i];
		if(panel)
		{
			LLViewerWearable* wearable = panel->getWearable();
			if(wearable)
				item = gInventory.getItem(wearable->getItemID());
		}
		if(item)
		{
			is_complete = item->isComplete();
			if(!is_complete)
			{
				all_complete = FALSE;
			}
			perm_mask = item->getPermissions().getMaskOwner();
		}
		else
		{
			is_complete = false;
			perm_mask = 0x0;
		}
		if(i == mCurrentWearableType)
		{
			if(panel)
			{
				panel->setUIPermissions(perm_mask, is_complete);
			}
			//BOOL is_vis = panel && item && is_complete && (perm_mask & PERM_MODIFY);
			//childSetVisible("panel_container", is_vis);
		}
	}

	childSetEnabled("Make Outfit", all_complete);
}

