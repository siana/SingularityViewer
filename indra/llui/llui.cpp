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
		llwarns << "tried to make ui sound for unknown sound name: " << name << llendl;	
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
					llinfos << "ui sound name: " << name << " triggered but silent (null uuid)" << llendl;	
				}				
			}
			else
			{
				llwarns << "ui sound named: " << name << " does not translate to a valid uuid" << llendl;	
			}

		}
		else if (LLUI::sAudioCallback != NULL)
		{
			if (LLUI::sConfigGroup->getBOOL("UISndDebugSpamToggle"))
			{
				llinfos << "ui sound name: " << name << llendl;	
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
		llerrs << "Failure to initialize configuration groups" << llendl;
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
	screen_x = llround((F32)x * getScaleFactor().mV[VX]);
	screen_y = llround((F32)y * getScaleFactor().mV[VY]);
	
	LLView::getWindow()->setCursorPosition(LLCoordGL(screen_x, screen_y).convert());
}

//static 
void LLUI::getMousePositionScreen(S32 *x, S32 *y)
{
	LLCoordWindow cursor_pos_window;
	getWindow()->getCursorPosition(&cursor_pos_window);
	LLCoordGL cursor_pos_gl(cursor_pos_window.convert());
	*x = llround((F32)cursor_pos_gl.mX / getScaleFactor().mV[VX]);
	*y = llround((F32)cursor_pos_gl.mY / getScaleFactor().mV[VX]);
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
	std::string slash = gDirUtilp->getDirDelimiter();
	std::string found_file = filename;
	if (!gDirUtilp->fileExists(found_file))
	{
		found_file = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, filename); // Should be CUSTOM_SKINS?
	}
	if (sConfigGroup && sConfigGroup->controlExists("Language"))
	{
		if (!gDirUtilp->fileExists(found_file))
		{
			std::string localization = getLanguage();
			std::string local_skin = "xui" + slash + localization + slash + filename;
			found_file = gDirUtilp->findSkinnedFilename(local_skin);
		}
	}
	if (!gDirUtilp->fileExists(found_file))
	{
		std::string local_skin = "xui" + slash + "en-us" + slash + filename;
		found_file = gDirUtilp->findSkinnedFilename(local_skin);
	}
	if (!gDirUtilp->fileExists(found_file))
	{
		found_file = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, filename);
	}
	return found_file;
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
	*gl_x = llround((F32)screen_x * getScaleFactor().mV[VX]);
	*gl_y = llround((F32)screen_y * getScaleFactor().mV[VY]);
}

//static
void LLUI::glPointToScreen(S32 gl_x, S32 gl_y, S32 *screen_x, S32 *screen_y)
{
	*screen_x = llround((F32)gl_x / getScaleFactor().mV[VX]);
	*screen_y = llround((F32)gl_y / getScaleFactor().mV[VY]);
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

