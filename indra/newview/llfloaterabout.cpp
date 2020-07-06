/** 
 * @file llfloaterabout.cpp
 * @author James Cook
 * @brief The about box from Help->About
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#include "llfloaterabout.h"

#include "llsys.h"
#include "llgl.h"
#include "llui.h"	// for tr()
#include "v3dmath.h"

#include "llcurl.h"
#include "llimagej2c.h"
#include "llaudioengine.h"

#include "llviewertexteditor.h"
#include "llviewercontrol.h"
#include "llagent.h"
#include "llviewerstats.h"
#include "llviewerregion.h"
#include "llversioninfo.h"
#include "lluictrlfactory.h"
#include "lluri.h"
#include "llweb.h"
#include "llsecondlifeurls.h"
#include "lltrans.h"
#include "llappviewer.h" 
#include "llglheaders.h"
#include "llwindow.h"

#include "hippogridmanager.h"

// [RLVa:KB] - Checked: 2010-04-18 (RLVa-1.4.0)
#include "rlvactions.h"
#include "rlvhelper.h"
// [/RLVa:KB]

#if LL_WINDOWS
#include "lldxhardware.h"
#endif

#include "cef/dullahan.h"
#if VLCPLUGIN
#include "vlc/libvlc_version.h"
#endif // LL_WINDOWS

extern LLMemoryInfo gSysMemory;
extern U32 gPacketsIn;

///----------------------------------------------------------------------------
/// Local function declarations, constants, enums, and typedefs
///----------------------------------------------------------------------------

LLFloaterAbout* LLFloaterAbout::sInstance = NULL;

static std::string get_viewer_release_notes_url();

static void onAboutClickCopyToClipboard(void* user_data)
{
	LLFloater* self = (LLFloater*) user_data;
	LLViewerTextEditor *support_widget = 
		self->getChild<LLViewerTextEditor>("support_editor", true);
	support_widget->selectAll();
	support_widget->copy();
	support_widget->deselect();
}

///----------------------------------------------------------------------------
/// Class LLFloaterAbout
///----------------------------------------------------------------------------

// Default constructor
LLFloaterAbout::LLFloaterAbout() 
:	LLFloater(std::string("floater_about"), std::string("FloaterAboutRect"), LLStringUtil::null)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_about.xml");

	// Support for changing product name.
	std::string title("About ");
	title += LLAppViewer::instance()->getSecondLifeTitle();
	setTitle(title);

	LLViewerTextEditor *support_widget = 
		getChild<LLViewerTextEditor>("support_editor", true);

	LLViewerTextEditor *credits_widget = 
		getChild<LLViewerTextEditor>("credits_editor", true);

	LLViewerTextEditor *licenses_widget =
		getChild<LLViewerTextEditor>("licenses_editor", true);
	
	childSetAction("copy_btn", onAboutClickCopyToClipboard, this);

	if (!support_widget || !credits_widget)
	{
		return;
	}

	// For some reason, adding style doesn't work unless this is true.
	support_widget->setParseHTML(TRUE);

	// Text styles for release notes hyperlinks
	LLStyleSP viewer_link_style(new LLStyle);
	viewer_link_style->setVisible(true);
	viewer_link_style->setFontName(LLStringUtil::null);
	viewer_link_style->setLinkHREF(get_viewer_release_notes_url());
	viewer_link_style->setColor(gSavedSettings.getColor4("HTMLLinkColor"));

	// Version string
	std::string version = std::string(LLAppViewer::instance()->getSecondLifeTitle()
#if defined(_WIN64) || defined(__x86_64__)
		+ " (64 bit)"
#endif
		+ llformat(" %d.%d.%d (%d) %s %s (%s)\n",
		LLVersionInfo::getMajor(), LLVersionInfo::getMinor(), LLVersionInfo::getPatch(), LLVersionInfo::getBuild(),
		__DATE__, __TIME__,
		LLVersionInfo::getChannel().c_str()));
	support_widget->appendColoredText(version, FALSE, FALSE, gColors.getColor("TextFgReadOnlyColor"));
	support_widget->appendText(LLTrans::getString("ReleaseNotes"), false, false, viewer_link_style);

	std::string support;
	support.append("\n\n");
	support.append("Grid: " + gHippoGridManager->getConnectedGrid()->getGridName() + "\n\n");

#if LL_MSVC
    support.append(llformat("Built with MSVC version %d\n\n", _MSC_VER));
#endif

#if LL_CLANG
    support.append(llformat("Built with Clang version %d\n\n", CLANG_VERSION));
#endif

#if LL_INTELC
    support.append(llformat("Built with ICC version %d\n\n", __ICC));
#endif

#if LL_GNUC
    support.append(llformat("Built with GCC version %d\n\n", GCC_VERSION));
#endif

	// Position
	LLViewerRegion* region = gAgent.getRegion();
	if (region)
	{
// [RLVa:KB] - Checked: 2014-02-24 (RLVa-1.4.10)
		if (RlvActions::canShowLocation())
		{
		const LLVector3d &pos = gAgent.getPositionGlobal();
		LLUIString pos_text = getString("you_are_at");
		pos_text.setArg("[POSITION]",
						llformat("%.1f, %.1f, %.1f ", pos.mdV[VX], pos.mdV[VY], pos.mdV[VZ]));
		support.append(pos_text);

		if (const LLViewerRegion* region = gAgent.getRegion())
		{
			const LLVector3d& coords(region->getOriginGlobal());
			std::string region_text = llformat("in %s (%.0f, %.0f) located at ", region->getName().c_str(), coords.mdV[VX]/REGION_WIDTH_METERS, coords.mdV[VY]/REGION_WIDTH_METERS);
			support.append(region_text);

			std::string buffer;
			buffer = region->getHost().getHostName();
			support.append(buffer);
			support.append(" (");
			buffer = region->getHost().getString();
			support.append(buffer);
			support.append(")");
		}
		}
		else
			support.append(RlvStrings::getString(RLV_STRING_HIDDEN_REGION));
// [/RLVa:KN]
		support += '\n';

		support.append(gLastVersionChannel);
		support += '\n';

		support_widget->appendColoredText(support, FALSE, FALSE, gColors.getColor("TextFgReadOnlyColor"));

		const std::string url(region->getCapability("ServerReleaseNotes"));
		if (!url.empty())
		{
			LLStyleSP server_link_style(new LLStyle(*viewer_link_style));
			server_link_style->setLinkHREF(url);
			support_widget->appendText(LLTrans::getString("ReleaseNotes"), false, false, server_link_style);
		}

		support = "\n\n";
	}

	// *NOTE: Do not translate text like GPU, Graphics Card, etc -
	//  Most PC users that know what these mean will be used to the english versions,
	//  and this info sometimes gets sent to support
	
	// CPU
	support.append("CPU: ");
	support.append( gSysCPU.getCPUString() );
	support += '\n';

	/* This is confusing and WRONG.
	support.append("SSE Support:");
	if(gSysCPU.hasSSE())
		support.append(" SSE2\n");
	else if(gSysCPU.hasSSE())
		support.append(" SSE\n");
	else
		support.append(" None\n"); */

	U32Megabytes memory = gSysMemory.getPhysicalMemoryKB();
	// Moved hack adjustment to Windows memory size into llsys.cpp

	std::string mem_text = llformat("Memory: %u MB\n", memory );
	support.append(mem_text);

	support.append("OS Version: ");
	support.append( LLAppViewer::instance()->getOSInfo().getOSString() );
	support += '\n';

	support.append("Graphics Card Vendor: ");
	support.append( (const char*) glGetString(GL_VENDOR) );
	support += '\n';

	support.append("Graphics Card: ");
	support.append( (const char*) glGetString(GL_RENDERER) );
	support += '\n';

#if LL_WINDOWS
    getWindow()->incBusyCount();
    getWindow()->setCursor(UI_CURSOR_ARROW);
    support.append("Windows Graphics Driver Version: ");
    LLSD driver_info = gDXHardware.getDisplayInfo();
    if (driver_info.has("DriverVersion"))
    {
        support.append(driver_info["DriverVersion"]);
    }
    support += '\n';
    getWindow()->decBusyCount();
    getWindow()->setCursor(UI_CURSOR_ARROW);
#endif

	support.append("OpenGL Version: ");
	support.append( (const char*) glGetString(GL_VERSION) );
// [RLVa:KB] - Checked: 2010-04-18 (RLVa-1.2.0)
	support += '\n';
	support.append("RLV Version: " + (RlvActions::isRlvEnabled() ? RlvStrings::getVersionAbout() : "(disabled)"));
// [/RLVa:KB]
	support.append("\n\n");

	support.append("libcurl Version: ");
	support.append( LLCurl::getVersionString() );
	support += '\n';

	support.append("J2C Decoder Version: ");
	support.append( LLImageJ2C::getEngineInfo() );
	support += '\n';

	support.append("Audio Driver Version: ");
	bool want_fullname = true;
	support.append( gAudiop ? gAudiop->getDriverName(want_fullname) : "(none)" );
	support += '\n';

	std::stringstream supportstrm;
	supportstrm << "Dullahan: "
			<< DULLAHAN_VERSION_MAJOR
			<< '.'
			<< DULLAHAN_VERSION_MINOR
			<< '.'
			<< DULLAHAN_VERSION_BUILD

			<< " / CEF: " << CEF_VERSION
			<< " / Chrome: " << CHROME_VERSION_MAJOR
			<< '\n';

#if VLCPLUGIN
	supportstrm << "LibVLC: ";
	supportstrm << LIBVLC_VERSION_MAJOR;
	supportstrm << '.';
	supportstrm << LIBVLC_VERSION_MINOR;
	supportstrm << '.';
	supportstrm << LIBVLC_VERSION_REVISION;
	supportstrm << '\n';
#endif

	support += supportstrm.str();

	if (gPacketsIn > 0)
	{
		std::string packet_loss = llformat("Packets Lost: %.0f/%.0f (%.1f%%)", 
			LLViewerStats::getInstance()->mPacketsLostStat.getCurrent(),
			F32(gPacketsIn),
			100.f*LLViewerStats::getInstance()->mPacketsLostStat.getCurrent() / F32(gPacketsIn) );
		support.append(packet_loss);
		support += '\n';
	}

	support_widget->appendColoredText(support, FALSE, FALSE, gColors.getColor("TextFgReadOnlyColor"));

	// Fix views
	support_widget->setCursorPos(0);
	support_widget->setEnabled(FALSE);
	support_widget->setTakesFocus(TRUE);
	support_widget->setHandleEditKeysDirectly(TRUE);

	credits_widget->setCursorPos(0);
	credits_widget->setEnabled(FALSE);
	credits_widget->setTakesFocus(TRUE);
	credits_widget->setHandleEditKeysDirectly(TRUE);

	// Get the Versions and Copyrights, created at build time
	std::string licenses_path = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "packages-info.txt");
	llifstream licenses_file;
	licenses_file.open(licenses_path);		/* Flawfinder: ignore */
	if (licenses_file.is_open())
	{
		std::string license_line;
		licenses_widget->clear();
		while (std::getline(licenses_file, license_line))
		{
			licenses_widget->appendColoredText(license_line + '\n', FALSE, FALSE, gColors.getColor("TextFgReadOnlyColor"));
		}
		licenses_file.close();
	}
	else
	{
		// this case will use the (out of date) hard coded value from the XUI
		LL_INFOS("AboutInit") << "Could not read licenses file at " << licenses_path << LL_ENDL;
	}
	licenses_widget->setCursorPos(0);
	licenses_widget->setEnabled(FALSE);
	licenses_widget->setTakesFocus(TRUE);
	licenses_widget->setHandleEditKeysDirectly(TRUE);

	center();

	sInstance = this;
}

// Destroys the object
LLFloaterAbout::~LLFloaterAbout()
{
	sInstance = NULL;
}

// static
void LLFloaterAbout::show(void*)
{
	if (!sInstance)
	{
		sInstance = new LLFloaterAbout();
	}

	sInstance->open();	 /*Flawfinder: ignore*/
}


static std::string get_viewer_release_notes_url()
{
	return LLTrans::getString("APP_SITE");
	/*std::ostringstream version;
	version <<  gVersionMajor
		<< "." << gVersionMinor
		<< "." << gVersionPatch
		<< "." << gVersionBuild;
	LLSD query;

	query["channel"] = gVersionChannel;

	query["version"] = version.str();

	std::ostringstream url;
	url << RELEASE_NOTES_BASE_URL << LLURI::mapToQueryString(query);

	return "http://ascent.balseraph.org/index.php/Ascent_" + version.str();// url.str();*/
}

