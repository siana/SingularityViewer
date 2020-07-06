/** 
 * @file lluictrlfactory.cpp
 * @brief Factory class for creating UI controls
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 * 
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#define LLUICTRLFACTORY_CPP
#include "lluictrlfactory.h"

#include <fstream>
#include <boost/tokenizer.hpp>

// other library includes
#include "llcontrol.h"
#include "lldir.h"
#include "v4color.h"

// this library includes
#include "llbutton.h"
#include "llcheckboxctrl.h"
//#include "llcolorswatch.h"
#include "llcombobox.h"
#include "llcontrol.h"
#include "lldir.h"
#include "llevent.h"
#include "llfloater.h"
#include "lliconctrl.h"
#include "lllineeditor.h"
#include "llmenugl.h"
#include "llradiogroup.h"
#include "llscrollcontainer.h"
#include "llscrollingpanellist.h"
#include "llscrolllistctrl.h"
#include "llslider.h"
#include "llsliderctrl.h"
#include "llmultislider.h"
#include "llmultisliderctrl.h"
#include "llspinctrl.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "llui.h"
#include "lluiimage.h"
#include "llviewborder.h"

LLTrace::BlockTimerStatHandle FTM_WIDGET_CONSTRUCTION("Widget Construction");
LLTrace::BlockTimerStatHandle FTM_INIT_FROM_PARAMS("Widget InitFromParams");
LLTrace::BlockTimerStatHandle FTM_WIDGET_SETUP("Widget Setup");

const char XML_HEADER[] = "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"yes\" ?>\n";

const S32 HPAD = 4;
const S32 VPAD = 4;
const S32 FLOATER_H_MARGIN = 15;
const S32 MIN_WIDGET_HEIGHT = 10;

std::vector<std::string> LLUICtrlFactory::sXUIPaths;

// UI Ctrl class for padding
class LLUICtrlLocate : public LLUICtrl
{
public:
	LLUICtrlLocate() : LLUICtrl(std::string("locate"), LLRect(0,0,0,0), FALSE) { setTabStop(FALSE); }
	virtual void draw() { }

	virtual LLXMLNodePtr getXML(bool save_children = true) const
	{
		LLXMLNodePtr node = LLUICtrl::getXML();
	
		node->setName(LL_UI_CTRL_LOCATE_TAG);
	
		return node;
	}

	static LLView *fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
	{
		LLUICtrlLocate *new_ctrl = new LLUICtrlLocate();
		new_ctrl->setName("pad");
		new_ctrl->initFromXML(node, parent);
		return new_ctrl;
	}
};

static LLRegisterWidget<LLUICtrlLocate> r1("locate");
static LLRegisterWidget<LLUICtrlLocate> r2("pad");

// Build time optimization, generate this once in .cpp file
template class LLUICtrlFactory* LLSingleton<class LLUICtrlFactory>::getInstance();

//-----------------------------------------------------------------------------
// LLUICtrlFactory()
//-----------------------------------------------------------------------------
LLUICtrlFactory::LLUICtrlFactory()
	: mDummyPanel(NULL)
{
	setupPaths();
}

LLUICtrlFactory::~LLUICtrlFactory()
{
	delete mDummyPanel;
	mDummyPanel = NULL;
}

void LLUICtrlFactory::setupPaths()
{
	std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_SKINS, "paths.xml");

	LLXMLNodePtr root;
	BOOL success  = LLXMLNode::parseFile(filename, root, NULL);
	sXUIPaths.clear();
	
	if (success)
	{
		LLXMLNodePtr path;
	
		for (path = root->getFirstChild(); path.notNull(); path = path->getNextSibling())
		{
			LLUIString path_val_ui(path->getValue());
			std::string language = LLUI::getLanguage();
			path_val_ui.setArg("[LANGUAGE]", language);

			if (std::find(sXUIPaths.begin(), sXUIPaths.end(), path_val_ui.getString()) == sXUIPaths.end())
			{
				sXUIPaths.push_back(path_val_ui.getString());
			}
		}
	}
	else // parsing failed
	{
		std::string slash = gDirUtilp->getDirDelimiter();
		std::string dir = "xui" + slash + "en-us";
		LL_WARNS() << "XUI::config file unable to open: " << filename << LL_ENDL;
		sXUIPaths.push_back(dir);
	}
}

// static
const std::vector<std::string>& LLUICtrlFactory::getXUIPaths()
{
	return sXUIPaths;
}

//-----------------------------------------------------------------------------
// getLayeredXMLNode()
//-----------------------------------------------------------------------------
bool LLUICtrlFactory::getLayeredXMLNode(const std::string &xui_filename, LLXMLNodePtr& root)
{
	std::string full_filename = gDirUtilp->findSkinnedFilenameBaseLang(LLDir::XUI, xui_filename);
	if (full_filename.empty())
	{
		// try filename as passed in since sometimes we load an xml file from a user-supplied path
		if (gDirUtilp->fileExists(xui_filename))
		{
			full_filename = xui_filename;
		}
		else
		{
			LL_WARNS() << "Couldn't find UI description file: " << sXUIPaths.front() + gDirUtilp->getDirDelimiter() + xui_filename << LL_ENDL;
			return false;
		}
	}

	if (!LLXMLNode::parseFile(full_filename, root, NULL))
	{
		LL_WARNS() << "Problem reading UI description file: " << full_filename << LL_ENDL;
		return false;
	}
	
	std::vector<std::string> paths =
	gDirUtilp->findSkinnedFilenames(LLDir::XUI, xui_filename);

	for ( auto& layer_filename : paths )
	{
		LLXMLNodePtr updateRoot;
		if (!LLXMLNode::parseFile(layer_filename, updateRoot, NULL))
		{
			LL_WARNS() << "Problem reading localized UI description file: " << layer_filename << LL_ENDL;
			return false;
		}

		std::string updateName;
		std::string nodeName;
		updateRoot->getAttributeString("name", updateName);
		root->getAttributeString("name", nodeName);

		if (updateName == nodeName)
		{
			LLXMLNode::updateNode(root, updateRoot);
		}
	}

	return true;
}


bool LLUICtrlFactory::getLayeredXMLNodeFromBuffer(const std::string &buffer, LLXMLNodePtr& root)
{
	if (!LLXMLNode::parseBuffer((U8*)buffer.data(), buffer.size(), root, 0)) {
		LL_WARNS() << "Error reading UI description from buffer." << LL_ENDL;
		return false;
	}
	return true;
}


//-----------------------------------------------------------------------------
// buildFloater()
//-----------------------------------------------------------------------------
void LLUICtrlFactory::buildFloater(LLFloater* floaterp, const std::string& filename, 
									const LLCallbackMap::map_t* factory_map, BOOL open) /* Flawfinder: ignore */
{
	LLXMLNodePtr root;

	if (!LLUICtrlFactory::getLayeredXMLNode(filename, root))
	{
		return;
	}
	
	buildFloaterInternal(floaterp, root, filename, factory_map, open);
}

void LLUICtrlFactory::buildFloaterFromBuffer(LLFloater *floaterp, const std::string &buffer,
											 const LLCallbackMap::map_t *factory_map, BOOL open) /* Flawfinder: ignore */
{
	LLXMLNodePtr root;

	if (!LLUICtrlFactory::getLayeredXMLNodeFromBuffer(buffer, root))
		return;
	
	buildFloaterInternal(floaterp, root, "(buffer)", factory_map, open);
}

void LLUICtrlFactory::buildFloaterInternal(LLFloater *floaterp, LLXMLNodePtr &root, const std::string &filename,
										   const LLCallbackMap::map_t *factory_map, BOOL open) /* Flawfinder: ignore */
{
	// root must be called floater
	if( !(root->hasName("floater") || root->hasName("multi_floater") ) )
	{
		LL_WARNS() << "Root node should be named floater in: " << filename << LL_ENDL;
		return;
	}

	if (factory_map)
	{
		mFactoryStack.push_front(factory_map);
	}

	// for local registry callbacks; define in constructor, referenced in XUI or postBuild
	floaterp->getCommitCallbackRegistrar().pushScope();
	floaterp->getEnableCallbackRegistrar().pushScope();
	floaterp->initFloaterXML(root, NULL, this, open);	/* Flawfinder: ignore */
	floaterp->getCommitCallbackRegistrar().popScope();
	floaterp->getEnableCallbackRegistrar().popScope();
	if (LLUI::sShowXUINames)
	{
		floaterp->setToolTip(filename);
	}

	if (factory_map)
	{
		mFactoryStack.pop_front();
	}

	LLHandle<LLFloater> handle = floaterp->getHandle();
	mBuiltFloaters[handle] = filename;
}

//-----------------------------------------------------------------------------
// getBuiltFloater()
//-----------------------------------------------------------------------------
LLFloater* LLUICtrlFactory::getBuiltFloater(const std::string name) const
{
	for (built_floater_t::const_iterator i = mBuiltFloaters.begin(); i != mBuiltFloaters.end(); ++i)
	{
		LLFloater* floater = i->first.get();
		if (floater && floater->getName() == name)
			return floater;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// saveToXML()
//-----------------------------------------------------------------------------
S32 LLUICtrlFactory::saveToXML(LLView* viewp, const std::string& filename)
{
	llofstream out(filename);
	if (!out.good())
	{
		LL_WARNS() << "Unable to open " << filename << " for output." << LL_ENDL;
		return 1;
	}

	out << XML_HEADER;

	LLXMLNodePtr xml_node = viewp->getXML();

	xml_node->writeToOstream(out);

	out.close();
	return 0;
}

//-----------------------------------------------------------------------------
// buildPanel()
//-----------------------------------------------------------------------------
BOOL LLUICtrlFactory::buildPanel(LLPanel* panelp, const std::string& filename,
									const LLCallbackMap::map_t* factory_map)
{
	LLXMLNodePtr root;

	if (!LLUICtrlFactory::getLayeredXMLNode(filename, root))
	{
		return FALSE;
	}
	
	return buildPanelInternal(panelp, root, filename, factory_map);
	}

BOOL LLUICtrlFactory::buildPanelFromBuffer(LLPanel *panelp, const std::string &buffer,
                                           const LLCallbackMap::map_t* factory_map)
{
	LLXMLNodePtr root;

	if (!LLUICtrlFactory::getLayeredXMLNodeFromBuffer(buffer, root))
		return FALSE;

	return buildPanelInternal(panelp, root, "(buffer)", factory_map);
}

BOOL LLUICtrlFactory::buildPanelInternal(LLPanel* panelp, LLXMLNodePtr &root, const std::string &filename,
                                         const LLCallbackMap::map_t* factory_map)
{
	BOOL didPost = FALSE;

	// root must be called panel
	if( !root->hasName("panel" ) )
	{
		LL_WARNS() << "Root node should be named panel in : " << filename << LL_ENDL;
		return didPost;
	}

	if (factory_map)
	{
		mFactoryStack.push_front(factory_map);
	}

	// for local registry callbacks; define in constructor, referenced in XUI or postBuild
	panelp->getCommitCallbackRegistrar().pushScope();
	panelp->getEnableCallbackRegistrar().pushScope();
	didPost = panelp->initPanelXML(root, NULL, this);
	panelp->getCommitCallbackRegistrar().popScope();
	panelp->getEnableCallbackRegistrar().popScope();

	if (LLUI::sShowXUINames)
	{
		panelp->setToolTip(filename);
	}

	LLHandle<LLPanel> handle = panelp->getHandle();
	mBuiltPanels[handle] = filename;

	if (factory_map)
	{
		mFactoryStack.pop_front();
	}

	return didPost;
}

//-----------------------------------------------------------------------------
// buildMenu()
//-----------------------------------------------------------------------------
LLMenuGL *LLUICtrlFactory::buildMenu(const std::string &filename, LLView* parentp)
{
	// TomY TODO: Break this function into buildMenu and buildMenuBar
	LLXMLNodePtr root;
	LLMenuGL*    menu;

	if (!getLayeredXMLNode(filename, root))
	{
		return nullptr;
	}

	// root must be called panel
	if (!root->hasName("menu_bar") && !root->hasName("menu") && !root->hasName("context_menu"))
	{
		LL_WARNS() << "Root node should be named menu bar or menu in: " << filename << LL_ENDL;
		return nullptr;
	}

	if (root->hasName("menu") || root->hasName("context_menu"))
	{
		menu = (LLMenuGL*)LLMenuGL::fromXML(root, parentp, this);
	}
	else
	{
		menu = (LLMenuGL*)LLMenuBarGL::fromXML(root, parentp, this);
	}
	
	if (LLUI::sShowXUINames)
	{
		menu->setToolTip(filename);
	}

    return menu;
}

//-----------------------------------------------------------------------------
// buildMenu()
//-----------------------------------------------------------------------------
LLContextMenu* LLUICtrlFactory::buildContextMenu(const std::string& filename, LLView* parentp)
{
	LLXMLNodePtr root;

	if (!LLUICtrlFactory::getLayeredXMLNode(filename, root))
	{
		return NULL;
	}

	// root must be called panel
	if( !root->hasName( LL_PIE_MENU_TAG ))
	{
		LL_WARNS() << "Root node should be named " << LL_PIE_MENU_TAG << " in : " << filename << LL_ENDL;
		return NULL;
	}

	std::string name("menu");
	root->getAttributeString("name", name);

	static LLUICachedControl<bool> context("LiruUseContextMenus", false);
	LLContextMenu* menu = context ? new LLContextMenu(name) : new LLPieMenu(name);
	parentp->addChild(menu);
	menu->initXML(root, parentp, this, context);

	if (LLUI::sShowXUINames)
	{
		menu->setToolTip(filename);
	}

	return menu;
}

//-----------------------------------------------------------------------------
// rebuild()
//-----------------------------------------------------------------------------
void LLUICtrlFactory::rebuild()
{
	built_panel_t built_panels = mBuiltPanels;
	mBuiltPanels.clear();
	built_panel_t::iterator built_panel_it;
	for (built_panel_it = built_panels.begin();
		built_panel_it != built_panels.end();
		++built_panel_it)
	{
		std::string filename = built_panel_it->second;
		LLPanel* panelp = built_panel_it->first.get();
		if (!panelp)
		{
			continue;
		}
		LL_INFOS() << "Rebuilding UI panel " << panelp->getName() 
			<< " from " << filename
			<< LL_ENDL;
		BOOL visible = panelp->getVisible();
		panelp->setVisible(FALSE);
		panelp->setFocus(FALSE);
		panelp->deleteAllChildren();

		buildPanel(panelp, filename, &panelp->getFactoryMap());
		panelp->setVisible(visible);
	}

	built_floater_t::iterator built_floater_it;
	for (built_floater_it = mBuiltFloaters.begin();
		built_floater_it != mBuiltFloaters.end();
		++built_floater_it)
	{
		LLFloater* floaterp = built_floater_it->first.get();
		if (!floaterp)
		{
			continue;
		}
		std::string filename = built_floater_it->second;
		LL_INFOS() << "Rebuilding UI floater " << floaterp->getName()
			<< " from " << filename
			<< LL_ENDL;
		BOOL visible = floaterp->getVisible();
		floaterp->setVisible(FALSE);
		floaterp->setFocus(FALSE);
		floaterp->deleteAllChildren();

		gFloaterView->removeChild(floaterp);
		buildFloater(floaterp, filename, &floaterp->getFactoryMap());
		floaterp->setVisible(visible);
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

LLView *LLUICtrlFactory::createCtrlWidget(LLPanel *parent, LLXMLNodePtr node)
{
	std::string ctrl_type = node->getName()->mString;
	LLStringUtil::toLower(ctrl_type);
	
	LLWidgetClassRegistry::factory_func_t func = LLWidgetClassRegistry::getInstance()->getCreatorFunc(ctrl_type);

	if (func == NULL)
	{
		LL_WARNS() << "Unknown control type " << ctrl_type << LL_ENDL;
		return NULL;
	}

	if (parent == NULL)
	{
		if (mDummyPanel == NULL)
		{
			mDummyPanel = new LLPanel;
		}
		parent = mDummyPanel;
	}
	LLView *ctrl = func(node, parent, this);

	return ctrl;
}

LLView* LLUICtrlFactory::createWidget(LLPanel *parent, LLXMLNodePtr node)
{
	LLView* view = createCtrlWidget(parent, node);

	S32 tab_group = parent->getLastTabGroup();
	node->getAttributeS32("tab_group", tab_group);

	if (view)
	{
		setCtrlParent(view, parent, tab_group);
	}

	return view;
}

//static
void LLUICtrlFactory::setCtrlParent(LLView* view, LLView* parent, S32 tab_group)
{
	if (tab_group == S32_MAX) tab_group = parent->getLastTabGroup();
	parent->addChild(view, tab_group);
}

//-----------------------------------------------------------------------------
// createFactoryPanel()
//-----------------------------------------------------------------------------
LLPanel* LLUICtrlFactory::createFactoryPanel(const std::string& name)
{
	std::deque<const LLCallbackMap::map_t*>::iterator itor;
	for (itor = mFactoryStack.begin(); itor != mFactoryStack.end(); ++itor)
	{
		const LLCallbackMap::map_t* factory_map = *itor;

		// Look up this panel's name in the map.
		LLCallbackMap::map_const_iter_t iter = factory_map->find( name );
		if (iter != factory_map->end())
		{
			// Use the factory to create the panel, instead of using a default LLPanel.
			LLPanel *ret = (LLPanel*) iter->second.mCallback( iter->second.mData );
			return ret;
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------

//static
BOOL LLUICtrlFactory::getAttributeColor(LLXMLNodePtr node, const std::string& name, LLColor4& color)
{
	std::string colorstring;
	BOOL res = node->getAttributeString(name.c_str(), colorstring);
	if (res && LLUI::sColorsGroup)
	{
		if (LLUI::sColorsGroup->controlExists(colorstring))
		{
			color.setVec(LLUI::sColorsGroup->getColor(colorstring));
		}
		else
		{
			res = FALSE;
		}
	}
	if (!res)
	{
		res = LLColor4::parseColor(colorstring, &color);
	}	
	if (!res)
	{
		res = node->getAttributeColor(name.c_str(), color);
	}
	return res;
}

