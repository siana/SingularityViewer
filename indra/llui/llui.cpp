/** 
 * @file llui.cpp
 * @brief UI implementation
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

// Utilities functions the user interface needs

#include "linden_common.h"

#include <string>
#include <map>

// Linden library includes
#include "v2math.h"
#include "m3math.h"
#include "v4color.h"
#include "llrender.h"
#include "llrect.h"
#include "lldir.h"
#include "llgl.h"
#include "llfontgl.h"

// Project includes
#include "llcontrol.h"
#include "llui.h"
#include "llview.h"
#include "lllineeditor.h"
#include "llwindow.h"

//
// Globals
//


// Used to hide the flashing text cursor when window doesn't have focus.
BOOL gShowTextEditCursor = TRUE;

// Language for UI construction
std::map<std::string, std::string> gTranslation;
std::list<std::string> gUntranslated;

/*static*/ LLControlGroup* LLUI::sConfigGroup = NULL;
/*static*/ LLControlGroup* LLUI::sAccountGroup = NULL;
/*static*/ LLControlGroup* LLUI::sIgnoresGroup = NULL;
/*static*/ LLControlGroup* LLUI::sColorsGroup = NULL;
/*static*/ LLUIAudioCallback LLUI::sAudioCallback = NULL;
/*static*/ LLWindow*		LLUI::sWindow = NULL;
/*static*/ LLView*			LLUI::sRootView = NULL;
/*static*/ LLHtmlHelp*		LLUI::sHtmlHelp = NULL;
/*static*/ BOOL            LLUI::sShowXUINames = FALSE;
/*static*/ BOOL            LLUI::sQAMode = FALSE;

//
// Functions
//
void make_ui_sound(const char* namep)
{
	std::string name = ll_safe_string(namep);
	if (!LLUI::sConfigGroup->controlExists(name))
	{
		LL_WARNS() << "tried to make ui sound for unknown sound name: " << name << LL_ENDL;	
	}
	else
	{
		LLUUID uuid(LLUI::sConfigGroup->getString(name));		
		if (uuid.isNull())
		{
			if (LLUI::sConfigGroup->getString(name) == LLUUID::null.asString())
			{
				if (LLUI::sConfigGroup->getBOOL("UISndDebugSpamToggle"))
				{
					LL_INFOS() << "ui sound name: " << name << " triggered but silent (null uuid)" << LL_ENDL;	
				}				
			}
			else
			{
				LL_WARNS() << "ui sound named: " << name << " does not translate to a valid uuid" << LL_ENDL;	
			}

		}
		else if (LLUI::sAudioCallback != NULL)
		{
			if (LLUI::sConfigGroup->getBOOL("UISndDebugSpamToggle"))
			{
				LL_INFOS() << "ui sound name: " << name << LL_ENDL;	
			}
			LLUI::sAudioCallback(uuid);
		}
	}
}

bool handleShowXUINamesChanged(const LLSD& newvalue)
{
	LLUI::sShowXUINames = newvalue.asBoolean();
	return true;
}

void LLUI::initClass(LLControlGroup* config, 
					 LLControlGroup* account, 
					 LLControlGroup* ignores, 
					 LLControlGroup* colors, 
					 LLImageProviderInterface* image_provider,
					 LLUIAudioCallback audio_callback,
					 const LLVector2* scale_factor,
					 const std::string& language
					 )
{
	LLRender2D::initClass(image_provider, scale_factor);
	sConfigGroup = config;
	sAccountGroup = account;
	sIgnoresGroup = ignores;
	sColorsGroup = colors;

	if (sConfigGroup == NULL
		|| sAccountGroup == NULL
		|| sIgnoresGroup == NULL
		|| sColorsGroup == NULL)
	{
		LL_ERRS() << "Failure to initialize configuration groups" << LL_ENDL;
	}

	sAudioCallback = audio_callback;
	sWindow = NULL; // set later in startup
	LLFontGL::sShadowColor = colors->getColor("ColorDropShadow");

	LLUI::sShowXUINames = LLUI::sConfigGroup->getBOOL("ShowXUINames");
	LLUI::sConfigGroup->getControl("ShowXUINames")->getSignal()->connect(boost::bind(&handleShowXUINamesChanged,_2));
}

void LLUI::cleanupClass()
{
	LLRender2D::cleanupClass();
	LLLineEditor::cleanupLineEditor();
}

//static 
void LLUI::setMousePositionScreen(S32 x, S32 y)
{
	S32 screen_x, screen_y;
	screen_x = ll_pos_round((F32)x * getScaleFactor().mV[VX]);
	screen_y = ll_pos_round((F32)y * getScaleFactor().mV[VY]);
	
	LLView::getWindow()->setCursorPosition(LLCoordGL(screen_x, screen_y).convert());
}

//static 
void LLUI::getMousePositionScreen(S32 *x, S32 *y)
{
	LLCoordWindow cursor_pos_window;
	getWindow()->getCursorPosition(&cursor_pos_window);
	LLCoordGL cursor_pos_gl(cursor_pos_window.convert());
	*x = ll_pos_round((F32)cursor_pos_gl.mX / getScaleFactor().mV[VX]);
	*y = ll_pos_round((F32)cursor_pos_gl.mY / getScaleFactor().mV[VX]);
}

//static 
void LLUI::setMousePositionLocal(const LLView* viewp, S32 x, S32 y)
{
	S32 screen_x, screen_y;
	viewp->localPointToScreen(x, y, &screen_x, &screen_y);

	setMousePositionScreen(screen_x, screen_y);
}

//static 
void LLUI::getMousePositionLocal(const LLView* viewp, S32 *x, S32 *y)
{
	S32 screen_x, screen_y;
	getMousePositionScreen(&screen_x, &screen_y);
	viewp->screenPointToLocal(screen_x, screen_y, x, y);
}


// On Windows, the user typically sets the language when they install the
// app (by running it with a shortcut that sets InstallLanguage).  On Mac,
// or on Windows if the SecondLife.exe executable is run directly, the 
// language follows the OS language.  In all cases the user can override
// the language manually in preferences. JC
// static
std::string LLUI::getLanguage()
{
	std::string language = "en-us";
	if (sConfigGroup)
	{
		language = sConfigGroup->getString("Language");
		if (language.empty() || language == "default")
		{
			language = sConfigGroup->getString("InstallLanguage");
		}
		if (language.empty() || language == "default")
		{
			language = sConfigGroup->getString("SystemLanguage");
		}
		if (language.empty() || language == "default")
		{
			language = "en-us";
		}
	}
	return language;
}

//static
std::string LLUI::locateSkin(const std::string& filename)
{
	std::string found_file = filename;
	if (gDirUtilp->fileExists(found_file))
	{
		return found_file;
	}

	found_file = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, filename); // Should be CUSTOM_SKINS?
	if (gDirUtilp->fileExists(found_file))
	{
		return found_file;
	}

	found_file = gDirUtilp->findSkinnedFilename(LLDir::XUI, filename);
	if (! found_file.empty())
	{
		return found_file;
	}

	found_file = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, filename);
	if (gDirUtilp->fileExists(found_file))
	{
		return found_file;
	}
	LL_WARNS("LLUI") << "Can't find '" << filename
					 << "' in user settings, any skin directory or app_settings" << LL_ENDL;
	return "";
}

//static
LLVector2 LLUI::getWindowSize()
{
	LLCoordWindow window_rect;
	sWindow->getSize(&window_rect);

	return LLVector2(window_rect.mX / getScaleFactor().mV[VX], window_rect.mY / getScaleFactor().mV[VY]);
}

//static
void LLUI::screenPointToGL(S32 screen_x, S32 screen_y, S32 *gl_x, S32 *gl_y)
{
	*gl_x = ll_round((F32)screen_x * getScaleFactor().mV[VX]);
	*gl_y = ll_round((F32)screen_y * getScaleFactor().mV[VY]);
}

//static
void LLUI::glPointToScreen(S32 gl_x, S32 gl_y, S32 *screen_x, S32 *screen_y)
{
	*screen_x = ll_round((F32)gl_x / getScaleFactor().mV[VX]);
	*screen_y = ll_round((F32)gl_y / getScaleFactor().mV[VY]);
}

//static
void LLUI::screenRectToGL(const LLRect& screen, LLRect *gl)
{
	screenPointToGL(screen.mLeft, screen.mTop, &gl->mLeft, &gl->mTop);
	screenPointToGL(screen.mRight, screen.mBottom, &gl->mRight, &gl->mBottom);
}

//static
void LLUI::glRectToScreen(const LLRect& gl, LLRect *screen)
{
	glPointToScreen(gl.mLeft, gl.mTop, &screen->mLeft, &screen->mTop);
	glPointToScreen(gl.mRight, gl.mBottom, &screen->mRight, &screen->mBottom);
}


LLControlGroup& LLUI::getControlControlGroup (const std::string& controlname)
{
	if(sConfigGroup->controlExists(controlname))
		return *sConfigGroup;
	if(sAccountGroup->controlExists(controlname))
		return *sAccountGroup;
	//if(sIgnoresGroup->controlExists(controlname)) //Identical to sConfigGroup currently.
	//	return *sIgnoresGroup;
	if(sColorsGroup->controlExists(controlname))
		return *sColorsGroup;
	return *sConfigGroup;
}

// static 
void LLUI::setHtmlHelp(LLHtmlHelp* html_help)
{
	LLUI::sHtmlHelp = html_help;
}

//static 
void LLUI::setQAMode(BOOL b)
{
	LLUI::sQAMode = b;
}

namespace LLInitParam
{
	ParamValue<LLUIColor>::ParamValue(const LLUIColor& color)
	:	super_t(color),
		red("red"),
		green("green"),
		blue("blue"),
		alpha("alpha"),
		control("")
	{
		updateBlockFromValue(false);
	}

	void ParamValue<LLUIColor>::updateValueFromBlock()
	{
		if (control.isProvided() && !control().empty())
		{
			updateValue(LLUI::sColorsGroup->controlExists(control) ? LLUI::sColorsGroup->getColor(control) : LLUI::sConfigGroup->getColor(control)); // Singu Note: Most of our colors will be in sColorsGroup (skin), but some may be moved to settings for users.
		}
		else
		{
			updateValue(LLColor4(red, green, blue, alpha));
		}
	}

	void ParamValue<LLUIColor>::updateBlockFromValue(bool make_block_authoritative)
	{
		LLColor4 color = getValue();
		red.set(color.mV[VRED], make_block_authoritative);
		green.set(color.mV[VGREEN], make_block_authoritative);
		blue.set(color.mV[VBLUE], make_block_authoritative);
		alpha.set(color.mV[VALPHA], make_block_authoritative);
		control.set("", make_block_authoritative);
	}

	bool ParamCompare<const LLFontGL*, false>::equals(const LLFontGL* a, const LLFontGL* b)
	{
		return !(a->getFontDesc() < b->getFontDesc())
			&& !(b->getFontDesc() < a->getFontDesc());
	}

	ParamValue<const LLFontGL*>::ParamValue(const LLFontGL* fontp)
	:	super_t(fontp),
		name("name"),
		size("size"),
		style("style")
	{
		if (!fontp)
		{
			updateValue(LLFontGL::getFontDefault());
		}
		addSynonym(name, "");
		updateBlockFromValue(false);
	}

	void ParamValue<const LLFontGL*>::updateValueFromBlock()
	{
		const LLFontGL* res_fontp = LLFontGL::getFontByName(name);
		if (res_fontp)
		{
			updateValue(res_fontp);
			return;
		}

		U8 fontstyle = 0;
		fontstyle = LLFontGL::getStyleFromString(style());
		LLFontDescriptor desc(name(), size(), fontstyle);
		const LLFontGL* fontp = LLFontGL::getFont(desc);
		if (fontp)
		{
			updateValue(fontp);
		}
		else
		{
			updateValue(LLFontGL::getFontDefault());
		}
	}

	void ParamValue<const LLFontGL*>::updateBlockFromValue(bool make_block_authoritative)
	{
		if (getValue())
		{
			name.set(LLFontGL::nameFromFont(getValue()), make_block_authoritative);
			size.set(LLFontGL::sizeFromFont(getValue()), make_block_authoritative);
			style.set(LLFontGL::getStringFromStyle(getValue()->getFontDesc().getStyle()), make_block_authoritative);
		}
	}

	ParamValue<LLRect>::ParamValue(const LLRect& rect)
	:	super_t(rect),
		left("left"),
		top("top"),
		right("right"),
		bottom("bottom"),
		width("width"),
		height("height")
	{
		updateBlockFromValue(false);
	}

	void ParamValue<LLRect>::updateValueFromBlock()
	{
		LLRect rect;

		//calculate from params
		// prefer explicit left and right
		if (left.isProvided() && right.isProvided())
		{
			rect.mLeft = left;
			rect.mRight = right;
		}
		// otherwise use width along with specified side, if any
		else if (width.isProvided())
		{
			// only right + width provided
			if (right.isProvided())
			{
				rect.mRight = right;
				rect.mLeft = right - width;
			}
			else // left + width, or just width
			{
				rect.mLeft = left;
				rect.mRight = left + width;
			}
		}
		// just left, just right, or none
		else
		{
			rect.mLeft = left;
			rect.mRight = right;
		}

		// prefer explicit bottom and top
		if (bottom.isProvided() && top.isProvided())
		{
			rect.mBottom = bottom;
			rect.mTop = top;
		}
		// otherwise height along with specified side, if any
		else if (height.isProvided())
		{
			// top + height provided
			if (top.isProvided())
			{
				rect.mTop = top;
				rect.mBottom = top - height;
			}
			// bottom + height or just height
			else
			{
				rect.mBottom = bottom;
				rect.mTop = bottom + height;
			}
		}
		// just bottom, just top, or none
		else
		{
			rect.mBottom = bottom;
			rect.mTop = top;
		}
		updateValue(rect);
	}

	void ParamValue<LLRect>::updateBlockFromValue(bool make_block_authoritative)
	{
		// because of the ambiguity in specifying a rect by position and/or dimensions
		// we use the lowest priority pairing so that any valid pairing in xui
		// will override those calculated from the rect object
		// in this case, that is left+width and bottom+height
		LLRect& value = getValue();

		right.set(value.mRight, false);
		left.set(value.mLeft, make_block_authoritative);
		width.set(value.getWidth(), make_block_authoritative);

		top.set(value.mTop, false);
		bottom.set(value.mBottom, make_block_authoritative);
		height.set(value.getHeight(), make_block_authoritative);
	}

	ParamValue<LLCoordGL>::ParamValue(const LLCoordGL& coord)
	:	super_t(coord),
		x("x"),
		y("y")
	{
		updateBlockFromValue(false);
	}

	void ParamValue<LLCoordGL>::updateValueFromBlock()
	{
		updateValue(LLCoordGL(x, y));
	}

	void ParamValue<LLCoordGL>::updateBlockFromValue(bool make_block_authoritative)
	{
		x.set(getValue().mX, make_block_authoritative);
		y.set(getValue().mY, make_block_authoritative);
	}


	void TypeValues<LLFontGL::HAlign>::declareValues()
	{
		declare("left", LLFontGL::LEFT);
		declare("right", LLFontGL::RIGHT);
		declare("center", LLFontGL::HCENTER);
	}

	void TypeValues<LLFontGL::VAlign>::declareValues()
	{
		declare("top", LLFontGL::TOP);
		declare("center", LLFontGL::VCENTER);
		declare("baseline", LLFontGL::BASELINE);
		declare("bottom", LLFontGL::BOTTOM);
	}

	void TypeValues<LLFontGL::ShadowType>::declareValues()
	{
		declare("none", LLFontGL::NO_SHADOW);
		declare("hard", LLFontGL::DROP_SHADOW);
		declare("soft", LLFontGL::DROP_SHADOW_SOFT);
	}
}

