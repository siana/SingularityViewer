// Ported to Imprudence from the Hippo OpenSim Viewer by Jacek Antonelli

#include "llviewerprecompiledheaders.h"

#include "hippogridmanager.h"

#include <cctype>

#include <stdtypes.h>
#include <lldir.h>
#include <llerror.h>
#include <llfile.h>
#include <llhttpclient.h>
#include <llsdserialize.h>
#include "lltrans.h"
#include "llviewercontrol.h"
#include "llweb.h"
#include "aialert.h"

// ********************************************************************
// Global Variables

HippoGridManager* gHippoGridManager = 0;

HippoGridInfo HippoGridInfo::FALLBACK_GRIDINFO("");



// ********************************************************************
// ********************************************************************
// HippoGridInfo
// ********************************************************************
// ********************************************************************

const S32 MINIMUM_PRICE_FOR_LISTING = 50;	// L$

// ********************************************************************
// Initialize

HippoGridInfo::HippoGridInfo(const std::string& gridName) :
	mPlatform(PLATFORM_OPENSIM),
	mGridName(gridName),
	mGridNick(LLStringUtil::null),
	mLoginUri(LLStringUtil::null),
	mLoginPage(LLStringUtil::null),
	mHelperUri(LLStringUtil::null),
	mWebSite(LLStringUtil::null),
	mSupportUrl(LLStringUtil::null),
	mRegisterUrl(LLStringUtil::null),
	mPartnerUrl(LLStringUtil::null),
	mPasswordUrl(LLStringUtil::null),
	mSearchUrl(LLStringUtil::null),
	mGridMessage(""),
	mXmlState(XML_VOID),
	mVoiceConnector("SLVoice"),
	mIsInProductionGrid(false),
	mIsInAvination(false),
	mRenderCompat(true),
	mAutoUpdate(false),
	mLocked(false),
	mCurrencySymbol("OS$"),
	mCurrencyText("OS Dollars"),
	mRealCurrencySymbol("US$"),
	mClassifiedFee(MINIMUM_PRICE_FOR_LISTING),
	mDirectoryFee(30),
	mUPCSupported(false)
{
}


// ********************************************************************
// Getters

const std::string& HippoGridInfo::getGridOwner() const
{
	if(isSecondLife())
	{
		static const std::string ll = "Linden Lab";
		return ll;
	}
	else
	{
		return getGridName();
	}	
}

// ********************************************************************
// Setters

void HippoGridInfo::setPlatform(Platform platform)
{
	mPlatform = platform;
	if (mPlatform == PLATFORM_SECONDLIFE)
	{
		mCurrencySymbol = "L$";
		mCurrencyText = "Linden Dollars";
	}
}


void HippoGridInfo::setPlatform(const std::string& platform)
{
	std::string tmp = platform;
	LLStringUtil::toLower(tmp);

	if (tmp == "aurora" || tmp == "whitecore")
	{
		setPlatform(PLATFORM_WHITECORE);
	} 
	else if (tmp == "opensim" || tmp == "halcyon")
	{
		setPlatform(PLATFORM_OPENSIM);
	} 
	else if (tmp == "secondlife") 
	{
		setPlatform(PLATFORM_SECONDLIFE);
	} 
	else 
	{
		setPlatform(PLATFORM_OTHER);
		LL_WARNS() << "Unknown platform '" << platform << "' for " << mGridName << '.' << LL_ENDL;
	}
}

void HippoGridInfo::setGridName(const std::string& gridName)
{
	HippoGridManager::GridIterator it;
	for(it = gHippoGridManager->beginGrid(); it != gHippoGridManager->endGrid(); ++it)
	{
		if (it->second == this)
		{
			gHippoGridManager->mGridInfo.erase(it);
			gHippoGridManager->mGridInfo[gridName] = this;
			break;
		}
	}
	mGridName = gridName;
	/*if(mGridNick.empty() && !gridName.empty())
	{
		setGridNick(gridName);
	}*/
}

void HippoGridInfo::setGridNick(std::string gridNick)
{
	mGridNick = sanitizeGridNick(gridNick);
	if(mGridName.empty() && !gridNick.empty())
	{
		setGridName(gridNick);
	}
	if(gridNick == "secondlife")
	{
		mIsInProductionGrid = true;
	}
	else if(gridNick == "avination")
	{
		mIsInAvination = true;
	}
}

void HippoGridInfo::useHttps()
{
	// If the Login URI starts with "http:", replace that with "https:".
	if (mLoginUri.substr(0, 5) == "http:")
	{
		mLoginUri = "https:" + mLoginUri.substr(5);
	}
}

void HippoGridInfo::setLoginUri(const std::string& loginUri)
{
	mLoginUri = sanitizeUri(loginUri);
	if (utf8str_tolower(LLURI(mLoginUri).hostName()) == "login.agni.lindenlab.com")
	{
		mIsInProductionGrid = true;
		useHttps();
		setPlatform(PLATFORM_SECONDLIFE);
	}
	else if (utf8str_tolower(LLURI(mLoginUri).hostName()) == "login.aditi.lindenlab.com")
	{
		useHttps();
		setPlatform(PLATFORM_SECONDLIFE);
	}
	else if (utf8str_tolower(LLURI(mLoginUri).hostName()) == "login.avination.com" ||
		utf8str_tolower(LLURI(mLoginUri).hostName()) == "login.avination.net")
	{
		mIsInAvination = true;
		useHttps();
	}
}

void HippoGridInfo::setLoginPage(const std::string& loginPage)
{
	mLoginPage = loginPage;
}

void HippoGridInfo::setHelperUri(const std::string& helperUri)
{
	mHelperUri = sanitizeUri(helperUri);
}

void HippoGridInfo::setWebSite(const std::string& website)
{
	mWebSite = website;
}

void HippoGridInfo::setSupportUrl(const std::string& url)
{
	mSupportUrl = url;
}

void HippoGridInfo::setRegisterUrl(const std::string& url)
{
	mRegisterUrl = url;
}

void HippoGridInfo::setPartnerUrl(const std::string& url)
{
	mPartnerUrl = url;
}

void HippoGridInfo::setPasswordUrl(const std::string& url)
{
	mPasswordUrl = url;
}

void HippoGridInfo::setSearchUrl(const std::string& url)
{
	mSearchUrl = url;
}

void HippoGridInfo::setGridMessage(const std::string& message)
{
	mGridMessage = message;
}

void HippoGridInfo::setRenderCompat(bool compat)
{
	mRenderCompat = compat;
}

void HippoGridInfo::setCurrencySymbol(const std::string& sym)
{
	mCurrencySymbol = sym;
}

void HippoGridInfo::setCurrencyText(const std::string& text)
{
	mCurrencyText = text;
}

void HippoGridInfo::setRealCurrencySymbol(const std::string& sym)
{
	mRealCurrencySymbol = sym.substr(0, 3);
}



// ********************************************************************
// Grid Info

//static
void HippoGridInfo::onXmlElementStart(void* userData, const XML_Char* name, const XML_Char** atts)
{
	HippoGridInfo* self = (HippoGridInfo*)userData;
	if (strcasecmp(name, "gridnick") == 0)
		self->mXmlState = XML_GRIDNICK;
	else if (strcasecmp(name, "gridname") == 0)
		self->mXmlState = XML_GRIDNAME;
	else if (strcasecmp(name, "platform") == 0)
		self->mXmlState = XML_PLATFORM;
	else if ((strcasecmp(name, "login") == 0) || (strcasecmp(name, "loginuri") == 0))
		self->mXmlState = XML_LOGINURI;
	else if ((strcasecmp(name, "welcome") == 0) || (strcasecmp(name, "loginpage") == 0))
		self->mXmlState = XML_LOGINPAGE;
	else if ((strcasecmp(name, "economy") == 0) || (strcasecmp(name, "helperuri") == 0))
		self->mXmlState = XML_HELPERURI;
	else if ((strcasecmp(name, "about") == 0) || (strcasecmp(name, "website") == 0))
		self->mXmlState = XML_WEBSITE;
	else if ((strcasecmp(name, "help") == 0) || (strcasecmp(name, "support") == 0))
		self->mXmlState = XML_SUPPORT;
	else if ((strcasecmp(name, "register") == 0) || (strcasecmp(name, "account") == 0))
		self->mXmlState = XML_REGISTER;
	else if (strcasecmp(name, "partner") == 0)
		self->mXmlState = XML_PARTNER;
	else if (strcasecmp(name, "password") == 0)
		self->mXmlState = XML_PASSWORD;
	else if (strcasecmp(name, "search") == 0)
		self->mXmlState = XML_SEARCH;
	else if (strcasecmp(name, "message") == 0)
		self->mXmlState = XML_MESSAGE;
}

//static
void HippoGridInfo::onXmlElementEnd(void* userData, const XML_Char* name)
{
	HippoGridInfo* self = (HippoGridInfo*)userData;
	self->mXmlState = XML_VOID;
}

//static
void HippoGridInfo::onXmlCharacterData(void* userData, const XML_Char* s, int len)
{
	HippoGridInfo* self = (HippoGridInfo*)userData;
	switch (self->mXmlState) 
	{
		case XML_GRIDNICK:
		{
			if (self->mGridNick == "")
			{
			  self->mGridNick.assign(s, len);
			  self->mGridNick = sanitizeGridNick(self->mGridNick);
			}
			break;
		}

		case XML_PLATFORM:
		{
			std::string platform(s, len);
			self->setPlatform(platform); 
			break;
		}	

		case XML_LOGINURI:
		{
			self->setLoginUri(std::string(s, len));
			break;
		}

		case XML_HELPERURI:
		{
			self->setHelperUri(std::string(s, len));
			break;
		}

		case XML_SEARCH:
		{
			self->mSearchUrl.assign(s, len);
			//sanitizeQueryUrl(mSearchUrl);
			break;
		}

		case XML_GRIDNAME:
		{
		  if (self->mGridName.empty())
		  {
			self->mGridName.assign(s, len);
		  }
		  break;
		}

		case XML_LOGINPAGE: self->mLoginPage.assign(s, len); break;
		case XML_WEBSITE: self->mWebSite.assign(s, len); break;
		case XML_SUPPORT: self->mSupportUrl.assign(s, len); break;
		case XML_REGISTER: self->mRegisterUrl.assign(s, len); break;
		case XML_PARTNER: self->mPartnerUrl.assign(s, len); break;
		case XML_PASSWORD: self->mPasswordUrl.assign(s, len); break;
		case XML_MESSAGE: self->mGridMessage.assign(s, len); break;

		case XML_VOID: break;
	}
}

// Throws AIAlert::ErrorCode with the http status as 'code' (HTTP_OK on XML parse error).
void HippoGridInfo::getGridInfo()
{
	if (mLoginUri.empty())
	{
		// By passing 0 we automatically get GridInfoErrorInstruction appended.
		THROW_ALERTC(0, "GridInfoErrorNoLoginURI");
	}

	// Make sure the uri ends on a '/'.
	std::string uri = mLoginUri;
	if (uri.back() != '/')
	{
		uri += '/';
	}

	std::string reply;
	int result = LLHTTPClient::blockingGetRaw(uri + "get_grid_info", reply);
	if (result != HTTP_OK)
	{
		char const* xml_desc;
		switch (result)
		{
			case HTTP_NOT_FOUND:
				xml_desc = "GridInfoErrorNotFound";
				break;
			case HTTP_METHOD_NOT_ALLOWED:
				xml_desc = "GridInfoErrorNotAllowed";
				break;
			default:
				xml_desc = "AIError";
				break;
		}
		// LLHTTPClient::blockingGetRaw puts any error message in the reply.
		THROW_ALERTC(result, xml_desc, AIArgs("[ERROR]", reply));
	}

	LL_INFOS() << "Received: " << reply << LL_ENDL;

	XML_Parser parser = XML_ParserCreate(0);
	XML_SetUserData(parser, this);
	XML_SetElementHandler(parser, onXmlElementStart, onXmlElementEnd);
	XML_SetCharacterDataHandler(parser, onXmlCharacterData);
	mXmlState = XML_VOID;
	if (!XML_Parse(parser, reply.data(), reply.size(), TRUE)) 
	{
		THROW_ALERTC(HTTP_OK, "GridInfoParseError", AIArgs("[XML_ERROR]", XML_ErrorString(XML_GetErrorCode(parser))));
	}
	XML_ParserFree(parser);
}

std::string HippoGridInfo::getDirectoryFee() const
{
	std::string fee = formatFee(mDirectoryFee, true);
	if (mDirectoryFee != 0) fee += "/" + LLTrans::getString("hippo_label_week");
	return fee;
}

std::string HippoGridInfo::formatFee(int cost, bool showFree) const
{
	if (showFree && (cost < 0)) 
	{
		return LLTrans::getString("hippo_label_free");
	} 
	else 
	{
		return llformat("%s%d", getCurrencySymbol().c_str(), cost);
	}
}

//static
std::string HippoGridInfo::sanitizeGridNick(const std::string &gridnick)
{
	std::string tmp;
	int size = gridnick.size();
	for (int i=0; i<size; i++)
	{
		char c = gridnick[i];
		if ((c == '_') || isalnum(c))
		{
			tmp += tolower(c);
		}
		else if (isspace(c))
		{
			tmp += "_";
		}
	}
	if(tmp.length() > 16) {
		tmp.resize(16);
	}
	return tmp;
}


std::string HippoGridInfo::getGridNick() const
{
	if(!mGridNick.empty())
	{
		return mGridNick;
	}
	else
	{
		return sanitizeGridNick(mGridName);
	}
}

// ********************************************************************
// Static Helpers

// static
const char* HippoGridInfo::getPlatformString(Platform platform)
{
	constexpr const char* platformStrings[PLATFORM_LAST] =
	{
		"Other", "WhiteCore", "OpenSim", "SecondLife"
	};

	if ((platform < PLATFORM_OTHER) || (platform >= PLATFORM_LAST))
		platform = PLATFORM_OTHER;
	return platformStrings[platform];
}

// static
std::string HippoGridInfo::sanitizeUri(std::string const& uri_in)
{
	std::string uri = uri_in;

	// Strip any leading and trailing spaces.
	LLStringUtil::trim(uri);

	// Only use https when it was entered.
	bool use_https = uri.substr(0, 6) == "https:";

	// Strip off attempts to use some prefix that is just wrong.
	// We accept the following:
	// "" (nothing)
	// "http:" or "https:", optionally followed by one or more '/'.
	std::string::size_type pos = uri.find_first_not_of("htps");
	if (pos != std::string::npos && pos < 6 && uri[pos] == ':')
	{
		do { ++pos; } while(uri[pos] == '/');
		uri = uri.substr(pos);
	}

	// Add (back) the prefix.
	if (use_https)
	{
		uri = "https://" + uri;
	}
	else
	{
		uri = "http://" + uri;
	}

	return uri;
}

void HippoGridInfo::initFallback()
{
	FALLBACK_GRIDINFO.setPlatform(PLATFORM_OPENSIM);
	FALLBACK_GRIDINFO.setGridName("Local Host");
	FALLBACK_GRIDINFO.setLoginUri("http://127.0.0.1:9000/");
	FALLBACK_GRIDINFO.setHelperUri("http://127.0.0.1:9000/");
}

bool HippoGridInfo::getAutoUpdate()
{
	if(isSecondLife())
		return false;
	else
		return mAutoUpdate;
}

bool HippoGridInfo::getUPCSupported()
{
	if(isSecondLife())
		return false;
	else
		return mUPCSupported;
}

void HippoGridInfo::setUPCSupported(bool b)
{
	mUPCSupported = b;
}

// ********************************************************************
// ********************************************************************
// HippoGridManager
// ********************************************************************
// ********************************************************************


// ********************************************************************
// Initialize

HippoGridManager::HippoGridManager() :
	mConnectedGrid(0),
	mDefaultGridsVersion(0),
	mCurrentGrid("Local Host"),
	mDefaultGrid("Local Host"),
	mCurrentGridChangeSignal(NULL)
{
}

HippoGridManager::~HippoGridManager()
{
	cleanup();
	if(mCurrentGridChangeSignal)
		delete mCurrentGridChangeSignal;
}


void HippoGridManager::cleanup()
{
	std::map<std::string, HippoGridInfo*>::iterator it, end = mGridInfo.end();
	for (it=mGridInfo.begin(); it != end; ++it) 
	{
		delete it->second;
	}
	mGridInfo.clear();
}


void HippoGridManager::init()
{
	HippoGridInfo::initFallback();
	loadFromFile();

	// !!!### gSavedSettings.getControl("CmdLineLoginURI");
	// !!!### gSavedSettings.getString("CmdLineLoginPage");
	// !!!### gSavedSettings.getString("CmdLineHelperURI");
	// !!!### LLString::compareInsensitive(gGridInfo[grid_index].mLabel, grid_name.c_str()))
}


void HippoGridManager::discardAndReload()
{
	cleanup();
	loadFromFile();
}


// ********************************************************************
// Public Access

HippoGridInfo* HippoGridManager::getGrid(const std::string& grid) const
{
	if(grid.empty())
		return NULL;

	std::map<std::string, HippoGridInfo*>::const_iterator it;
	it = mGridInfo.find(grid);

	//The grids are keyed by 'name' which equates to something like "Second Life"
	//Try to match such first.
	if (it != mGridInfo.end()) 
	{
		return it->second;
	} 
	else //Fall back to nick short names. (so something like "secondlife" will work)
	{
		for(it = mGridInfo.begin(); it != mGridInfo.end(); ++it)
		{
			if(it->second && LLStringUtil::compareInsensitive(it->second->getGridNick(), grid)==0)
				return it->second;
		}
	}
	return NULL;
}

HippoGridInfo* HippoGridManager::getCurrentGrid() const
{
	HippoGridInfo* grid = getGrid(mCurrentGrid);
	if(!grid) 
	{
		grid = getGrid(mDefaultGrid);
	}
	return grid ? grid : &HippoGridInfo::FALLBACK_GRIDINFO;
}

std::string HippoGridManager::getDefaultGridNick() const
{
	HippoGridInfo* grid = getGrid(mDefaultGrid);
	return grid ? grid->getGridNick() : HippoGridInfo::FALLBACK_GRIDINFO.getGridNick();
}

std::string HippoGridManager::getCurrentGridNick() const
{
	return getCurrentGrid()->getGridNick();
}

const std::string& HippoGridManager::getDefaultGridName() const
{
	return mDefaultGrid;
}

const std::string& HippoGridManager::getCurrentGridName() const
{
	return mCurrentGrid;
}

void HippoGridManager::setCurrentGridAsConnected()
{
	mConnectedGrid = getCurrentGrid();
}


void HippoGridManager::addGrid(HippoGridInfo* grid)
{
	if (!grid) return;
	const std::string& nick = grid->getGridName();
	if (nick == "") 
	{
		LL_WARNS() << "Ignoring to try adding grid with empty nick." << LL_ENDL;
		delete grid;
		return;
	}
	if (mGridInfo.find(nick) != mGridInfo.end()) 
	{
		LL_WARNS() << "Ignoring to try adding existing grid " << nick << '.' << LL_ENDL;
		delete grid;
		return;
	}
	mGridInfo[nick] = grid;
}


void HippoGridManager::deleteGrid(const std::string& grid)
{
	GridIterator it = mGridInfo.find(grid);
	if (it == mGridInfo.end()) {
		LL_WARNS() << "Trying to delete non-existing grid " << grid << '.' << LL_ENDL;
		return;
	}
	mGridInfo.erase(it);
	LL_INFOS() << "Number of grids now: " << mGridInfo.size() << LL_ENDL;
	if (mGridInfo.empty()) LL_INFOS() << "Grid info map is empty." << LL_ENDL;
	if (grid == mDefaultGrid)
		setDefaultGrid(LLStringUtil::null);  // sets first grid, if map not empty
	if (grid == mCurrentGrid)
		mCurrentGrid = mDefaultGrid;
}


void HippoGridManager::setDefaultGrid(const std::string& grid)
{
	GridIterator it = mGridInfo.find(grid);
	if (it != mGridInfo.end()) 
	{
		mDefaultGrid = grid;
	} 
	else if (mGridInfo.find("Second life") != mGridInfo.end()) 
	{
		mDefaultGrid = "Second Life";
	} 
	else if (!mGridInfo.empty()) 
	{
	    mDefaultGrid = mGridInfo.begin()->first;
	} 
	else 
	{
		mDefaultGrid = "";
	}
}


void HippoGridManager::setCurrentGrid(const std::string& grid)
{
	HippoGridInfo* prevGrid = getGrid(mCurrentGrid);
	GridIterator it = mGridInfo.find(grid);
	if (it != mGridInfo.end()) 
	{
		mCurrentGrid = grid;
	} 
	else if (!mGridInfo.empty()) 
	{
		LL_WARNS() << "Unknown grid '" << grid << "'. Setting to default grid." << LL_ENDL;
	    mCurrentGrid = mDefaultGrid;
	}
	if(mCurrentGridChangeSignal)
		(*mCurrentGridChangeSignal)(getGrid(mCurrentGrid),prevGrid);
}


// ********************************************************************
// Persistent Store

void HippoGridManager::loadFromFile()
{
	mDefaultGridsVersion = 0;
	// load user grid info
	parseFile(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "grids_sg1.xml"), false);
	// merge default grid info, if newer. Force load, if list of grids is empty.
	parseFile(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "default_grids.xml"), !mGridInfo.empty());

	std::string last_grid = gSavedSettings.getString("LastSelectedGrid");
	if (last_grid.empty()) last_grid = gSavedSettings.getString("DefaultGrid");
	setDefaultGrid(last_grid);
	setCurrentGrid(last_grid);
}

void HippoGridManager::parseUrl()
{
	const std::string& url(gSavedSettings.getString("GridUpdateList"));
	if (url.empty()) return;

	LL_INFOS() << "Loading grid info from '" << url << "'." << LL_ENDL;

	// query update server
	std::string escaped_url = LLWeb::escapeURL(url);
	LLSD response = LLHTTPClient::blockingGet(url);

	// check response, return on error
	S32 status = response["status"].asInteger();
	if ((status != 200) || !response["body"].isArray()) 
	{
		LL_INFOS() << "GridInfo Update failed (" << status << "): "
			<< (response["body"].isString()? response["body"].asString(): "<unknown error>")
			<< LL_ENDL;
		return;
	}

	// Force load, if list of grids is empty.
	parseData(response["body"], !mGridInfo.empty());
}

void HippoGridManager::parseFile(const std::string& fileName, bool mergeIfNewer)
{
	llifstream infile;
	infile.open(fileName.c_str());
	if (!infile.is_open()) 
	{
		LL_WARNS() << "Cannot find grid info file " << fileName << " to load." << LL_ENDL;
		return;
	}

	LLSD gridInfo;
	if (LLSDSerialize::fromXML(gridInfo, infile) <= 0) 
	{
		LL_WARNS() << "Unable to parse grid info file " << fileName << '.' << LL_ENDL;		
		return;
	}

	LL_INFOS() << "Loading grid info file " << fileName << '.' << LL_ENDL;
	parseData(gridInfo, mergeIfNewer);
}


void HippoGridManager::parseData(LLSD &gridInfo, bool mergeIfNewer)
{
	if (mergeIfNewer) 
	{
		LLSD::array_const_iterator it, end = gridInfo.endArray();
		for (it = gridInfo.beginArray(); it != end; ++it) 
		{
			LLSD gridMap = *it;
			if (gridMap.has("default_grids_version")) 
			{
				int version = gridMap["default_grids_version"];
				if (version <= mDefaultGridsVersion) return;
				else break;
			}
		}
		if (it == end) 
		{
			LL_WARNS() << "Grid data has no version number." << LL_ENDL;
			return;
		}
	}

	LL_INFOS() << "Loading grid data." << LL_ENDL;

	LLSD::array_const_iterator it, end = gridInfo.endArray();
	for (it = gridInfo.beginArray(); it != end; ++it) 
	{
		LLSD gridMap = *it;
		if (gridMap.has("default_grids_version")) 
		{
			mDefaultGridsVersion = gridMap["default_grids_version"];
		} 
		else if ((gridMap.has("gridnick")  || gridMap.has("gridname")) && gridMap.has("loginuri")) 
		{
			std::string gridnick = gridMap["gridnick"];
			std::string gridname = gridMap["gridname"];
			
			HippoGridInfo* grid;
			GridIterator it = mGridInfo.end();
			for (it = mGridInfo.begin(); it != mGridInfo.end(); ++it)
			{
				if(!gridnick.empty() && (it->second->getGridNick() == gridnick))
				{
					break;
				}
				if(gridnick.empty() && !gridname.empty() && (it->first == gridname))
				{
					break;
				}
			}
				
			bool newGrid = (it == mGridInfo.end());
			if (newGrid || !it->second)
			{
				if(gridname.empty())
				{
					gridname = gridnick;
				}				
				// create new grid info
				grid = new HippoGridInfo(gridname);
			} 
			else 
			{
				// update existing grid info
				grid = it->second;
			}
			grid->setLoginUri(gridMap["loginuri"]);
			if (gridMap.has("platform")) grid->setPlatform(gridMap["platform"]);
			if (gridMap.has("gridname")) grid->setGridName(gridMap["gridname"]);
			if (gridMap.has("gridnick")) grid->setGridNick(gridMap["gridnick"]);
			if (gridMap.has("loginpage")) grid->setLoginPage(gridMap["loginpage"]);
			if (gridMap.has("helperuri")) grid->setHelperUri(gridMap["helperuri"]);
			if (gridMap.has("website")) grid->setWebSite(gridMap["website"]);
			if (gridMap.has("support")) grid->setSupportUrl(gridMap["support"]);
			if (gridMap.has("register")) grid->setRegisterUrl(gridMap["register"]);
			if (gridMap.has("partner")) grid->setPartnerUrl(gridMap["partner"]);
			if (gridMap.has("password")) grid->setPasswordUrl(gridMap["password"]);
			if (gridMap.has("search")) grid->setSearchUrl(gridMap["search"]);
			if (gridMap.has("render_compat")) grid->setRenderCompat(gridMap["render_compat"]);
			if (gridMap.has("auto_update")) grid->mAutoUpdate = gridMap["auto_update"];
			if (gridMap.has("locked")) grid->mLocked = gridMap["locked"];
			if (newGrid) addGrid(grid);
		}
	}
}


void HippoGridManager::saveFile()
{
	// save default grid to client settings
	gSavedSettings.setString("DefaultGrid", mDefaultGrid);

	// build LLSD
	LLSD gridInfo;
	gridInfo[0]["default_grids_version"] = mDefaultGridsVersion;

	// add grids
	S32 i = 1;
	GridIterator it, end = mGridInfo.end();
	for (it = mGridInfo.begin(); it != end; ++it, i++) 
	{
		HippoGridInfo* grid = it->second;
		gridInfo[i]["platform"] = HippoGridInfo::getPlatformString(grid->getPlatform());
		gridInfo[i]["gridnick"] = grid->getGridNick();
		gridInfo[i]["gridname"] = grid->getGridName();
		gridInfo[i]["loginuri"] = grid->getLoginUri();
		gridInfo[i]["loginpage"] = grid->getLoginPage();
		gridInfo[i]["helperuri"] = grid->getHelperUri();
		gridInfo[i]["website"] = grid->getWebSite();
		gridInfo[i]["support"] = grid->getSupportUrl();
		gridInfo[i]["register"] = grid->getRegisterUrl();
		gridInfo[i]["partner"] = grid->getPartnerUrl();
		gridInfo[i]["password"] = grid->getPasswordUrl();
		
		gridInfo[i]["search"] = grid->getSearchUrl();
		gridInfo[i]["render_compat"] = grid->isRenderCompat();
		gridInfo[i]["auto_update"] = grid->getAutoUpdate();
		gridInfo[i]["locked"] = grid->getLocked();
	}

	// write client grid info file
	std::string fileName = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "grids_sg1.xml");
	llofstream file;
	file.open(fileName.c_str());
	if (file.is_open()) 
	{
		LLSDSerialize::toPrettyXML(gridInfo, file);
		file.close();
		LL_INFOS() << "Saved grids to " << fileName << LL_ENDL;
	} 
	else 
	{
		LL_WARNS() << "Unable to open grid info file for save: " << fileName << LL_ENDL;
	}
}
