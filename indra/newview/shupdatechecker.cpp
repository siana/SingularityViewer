#include "llviewerprecompiledheaders.h"

#include "llbufferstream.h"
#include "llhttpclient.h"
#include "llnotificationsutil.h"
#include "llversioninfo.h"
#include "llviewerwindow.h"
#include "llsdjson.h"
#include "llweb.h"
#include "llwindow.h"


void onNotifyButtonPress(const LLSD& notification, const LLSD& response, std::string name, std::string url)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (option == 0) // URL
	{
		if (gViewerWindow)
		{
			gViewerWindow->getWindow()->spawnWebBrowser(LLWeb::escapeURL(url), true);
		}
	}
}

void onCompleted(const LLSD& data, bool release)
{
	S32 build(LLVersionInfo::getBuild());
	std::string viewer_version = llformat("%s (%i)", LLVersionInfo::getShortVersion().c_str(), build);

#if LL_WINDOWS
	constexpr auto platform = "windows";
#elif LL_LINUX
	constexpr auto platform = "linux";
#elif LL_DARWIN
	constexpr auto platform = "apple";
#endif
	std::string recommended_version = data["recommended"][platform];
	std::string minimum_version = data["minimum"][platform];
		
	S32 minimum_build, recommended_build;
	sscanf(recommended_version.c_str(), "%*i.%*i.%*i (%i)", &recommended_build);
	sscanf(minimum_version.c_str(), "%*i.%*i.%*i (%i)", &minimum_build);

	LL_INFOS("GetUpdateInfoResponder") << build << LL_ENDL;
	LLSD args;
	args["CURRENT_VER"] = viewer_version;
	args["RECOMMENDED_VER"] = recommended_version;
	args["MINIMUM_VER"] = minimum_version;
	args["URL"] = data["url"].asString();

	static LLCachedControl<S32> lastver(release ? "SinguLastKnownReleaseBuild" : "SinguLastKnownAlphaBuild", 0);

	if (build < minimum_build || build < recommended_build)
	{
		if (lastver.get() < recommended_build)
		{
			lastver = recommended_build;
			LLUI::sIgnoresGroup->setWarning("UrgentUpdateModal", true);
			LLUI::sIgnoresGroup->setWarning("UrgentUpdate", true);
			LLUI::sIgnoresGroup->setWarning("RecommendedUpdate", true);
		}
		const std::string&& notification = build < minimum_build ?
			LLUI::sIgnoresGroup->getWarning("UrgentUpdateModal") ? "UrgentUpdateModal" : "UrgentUpdate" :
			"RecommendedUpdate"; //build < recommended_build
		LLNotificationsUtil::add(notification, args, LLSD(), boost::bind(onNotifyButtonPress, _1, _2, notification, data["url"].asString()));
	}
}

extern AIHTTPTimeoutPolicy getUpdateInfoResponder_timeout;
///////////////////////////////////////////////////////////////////////////////
// GetUpdateInfoResponder
class GetUpdateInfoResponder final : public LLHTTPClient::ResponderWithCompleted
{
	LOG_CLASS(GetUpdateInfoResponder);
public:
	GetUpdateInfoResponder(std::string type) : mType(type) {}
	void completedRaw(LLChannelDescriptors const& channels, buffer_ptr_t const& buffer) override
	{
		if (mStatus != HTTP_OK)
		{
			LL_WARNS() << "Failed to get update info (" << mStatus << ")" << LL_ENDL;
			return;
		}

		LLBufferStream istr(channels, buffer.get());
		std::stringstream strstrm;
		strstrm << istr.rdbuf();
		LLSD data = LlsdFromJsonString(strstrm.str());
		if (data.isUndefined())
		{
			LL_WARNS() << "Failed to parse json string from body." << LL_ENDL;
			// TODO: Should we say something here for the user?
		}
		else onCompleted(data[mType], mType == "release");
	}

protected:
	AIHTTPTimeoutPolicy const& getHTTPTimeoutPolicy() const override { return getUpdateInfoResponder_timeout; }
	char const* getName() const override { return "GetUpdateInfoResponder"; }

private:
	std::string mType;
};

void check_for_updates()
{
	// Hard-code the update url for now.
	std::string url = "http://singularity-viewer.github.io/pages/api/get_update_info.json";//gSavedSettings.getString("SHUpdateCheckURL");
	if (!url.empty())
	{
		std::string type;
		auto& channel = LLVersionInfo::getChannel();
		if (channel == "Singularity")
		{
			type = "release";
		}
		else if (channel == "Singularity Test" || channel == "Singularity Alpha" || channel == "Singularity Beta")
		{
			type = "alpha";
		}
		else return;
		LLHTTPClient::get(url, new GetUpdateInfoResponder(type));
	}
}
