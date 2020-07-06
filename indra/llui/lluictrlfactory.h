/** 
 * @file lluictrlfactory.h
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

#ifndef LLUICTRLFACTORY_H
#define LLUICTRLFACTORY_H

#include <iosfwd>
#include <stack>

#include "llcallbackmap.h"
#include "llfloater.h"
#include "llinitparam.h"

class LLView;


extern LLTrace::BlockTimerStatHandle FTM_WIDGET_SETUP;
extern LLTrace::BlockTimerStatHandle FTM_WIDGET_CONSTRUCTION;
extern LLTrace::BlockTimerStatHandle FTM_INIT_FROM_PARAMS;

// Build time optimization, generate this once in .cpp file
#ifndef LLUICTRLFACTORY_CPP
extern template class LLUICtrlFactory* LLSingleton<class LLUICtrlFactory>::getInstance();
#endif

class LLUICtrlFactory : public LLSingleton<LLUICtrlFactory>
{
private:
	friend class LLSingleton<LLUICtrlFactory>;
	LLUICtrlFactory();
	~LLUICtrlFactory();

	// only partial specialization allowed in inner classes, so use extra dummy parameter
	template <typename PARAM_BLOCK, int DUMMY>
	class ParamDefaults : public LLSingleton<ParamDefaults<PARAM_BLOCK, DUMMY> > 
	{
	public:
		ParamDefaults()
		{
			// recursively fill from base class param block
			((typename PARAM_BLOCK::base_block_t&)mPrototype).fillFrom(ParamDefaults<typename PARAM_BLOCK::base_block_t, DUMMY>::instance().get());
		}

		const PARAM_BLOCK& get() { return mPrototype; }

	private:
		PARAM_BLOCK mPrototype;
	};

	// base case for recursion, there are NO base classes of LLInitParam::BaseBlock
	template<int DUMMY>
	class ParamDefaults<LLInitParam::BaseBlock, DUMMY> : public LLSingleton<ParamDefaults<LLInitParam::BaseBlock, DUMMY> >
	{
	public:
		const LLInitParam::BaseBlock& get() { return mBaseBlock; }
	private:
		LLInitParam::BaseBlock mBaseBlock;
	};

public:
	void setupPaths();
	
	void buildFloater(LLFloater* floaterp, const std::string &filename, 
					const LLCallbackMap::map_t* factory_map = NULL, BOOL open = TRUE);
	void buildFloaterFromBuffer(LLFloater *floaterp, const std::string &buffer,
								const LLCallbackMap::map_t *factory_map = NULL, BOOL open = TRUE);
	BOOL buildPanel(LLPanel* panelp, const std::string &filename,
					const LLCallbackMap::map_t* factory_map = NULL);
	BOOL buildPanelFromBuffer(LLPanel *panelp, const std::string &buffer,
							  const LLCallbackMap::map_t* factory_map = NULL);

	LLFloater* getBuiltFloater(const std::string name) const;

	void removePanel(LLPanel* panelp) { mBuiltPanels.erase(panelp->getHandle()); }
	void removeFloater(LLFloater* floaterp) { mBuiltFloaters.erase(floaterp->getHandle()); }
	
	bool builtPanel(LLPanel* panelp) {return mBuiltPanels.find(panelp->getHandle()) != mBuiltPanels.end();}

	class LLMenuGL *buildMenu(const std::string &filename, LLView* parentp);
	class LLContextMenu* buildContextMenu(const std::string& filename, LLView* parentp);

	// Does what you want for LLFloaters and LLPanels
	// Returns 0 on success
	S32 saveToXML(LLView* viewp, const std::string& filename);

	// Rebuilds all currently built panels.
	void rebuild();

	static BOOL getAttributeColor(LLXMLNodePtr node, const std::string& name, LLColor4& color);

	LLPanel* createFactoryPanel(const std::string& name);

	LLView* createCtrlWidget(LLPanel *parent, LLXMLNodePtr node);
	LLView* createWidget(LLPanel *parent, LLXMLNodePtr node);

	template<typename T>
	static T* create(typename T::Params& params, LLView* parent = NULL)
	{
		params.fillFrom(ParamDefaults<typename T::Params, 0>::instance().get());

		T* widget = createWidgetImpl<T>(params, parent);
		if (widget)
		{
			widget->postBuild();
		}

		return widget;
	}

	static bool getLayeredXMLNode(const std::string &filename, LLXMLNodePtr& root);
	static bool getLayeredXMLNodeFromBuffer(const std::string &buffer, LLXMLNodePtr& root);

	static const std::vector<std::string>& getXUIPaths();

private:
	bool getLayeredXMLNodeImpl(const std::string &filename, LLXMLNodePtr& root);


	void buildFloaterInternal(LLFloater *floaterp, LLXMLNodePtr &root, const std::string &filename,
							  const LLCallbackMap::map_t *factory_map, BOOL open);
	BOOL buildPanelInternal(LLPanel* panelp, LLXMLNodePtr &root, const std::string &filename,
							const LLCallbackMap::map_t* factory_map = NULL);

	template<typename T>
	static T* createWidgetImpl(const typename T::Params& params, LLView* parent = NULL)
	{
		T* widget = NULL;

		if (!params.validateBlock())
		{
			LL_WARNS() << /*getInstance()->getCurFileName() <<*/ ": Invalid parameter block for " << typeid(T).name() << LL_ENDL;
			//return NULL;
		}

		{ LL_RECORD_BLOCK_TIME(FTM_WIDGET_CONSTRUCTION);
			widget = new T(params);	
		}
		{ LL_RECORD_BLOCK_TIME(FTM_INIT_FROM_PARAMS);
			widget->initFromParams(params);
		}

		if (parent)
		{
			S32 tab_group = params.tab_group.isProvided() ? params.tab_group() : S32_MAX;
			setCtrlParent(widget, parent, tab_group);
		}
		return widget;
	}

	// this exists to get around dependency on llview
	static void setCtrlParent(LLView* view, LLView* parent, S32 tab_group);

	LLPanel* mDummyPanel;

	typedef std::map<LLHandle<LLPanel>, std::string> built_panel_t;
	built_panel_t mBuiltPanels;

	typedef std::map<LLHandle<LLFloater>, std::string> built_floater_t;
	built_floater_t mBuiltFloaters;

	std::deque<const LLCallbackMap::map_t*> mFactoryStack;

	static std::vector<std::string> sXUIPaths;
};


#endif //LLUICTRLFACTORY_H
