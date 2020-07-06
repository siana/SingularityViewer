/*
 * @file llmediafilter.h
 * @brief Hyperbalistic SLU paranoia controls
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2011, Sione Lomu.
 * Copyright (C) 2014, Cinder Roxley.
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
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"
#include "llmediafilter.h"

#include "llaudioengine.h"
#include "llnotificationsutil.h"
#include "llparcel.h"
#include "llsdserialize.h"
#include "lltrans.h"
#include "llvieweraudio.h"
#include "llviewerparcelmgr.h"
#include "llviewerparcelmedia.h"

const std::string MEDIALIST_XML = "medialist.xml";
bool handle_audio_filter_callback(const LLSD& notification, const LLSD& response, const std::string& url);
bool handle_media_filter_callback(const LLSD& notification, const LLSD& response, LLParcel* parcel);
std::string extractDomain(const std::string& in_url);

LLMediaFilter::LLMediaFilter()
: mMediaCommandQueue(0)
, mCurrentParcel(NULL)
, mMediaQueue(NULL)
, mAlertActive(false)
{
	loadMediaFilterFromDisk();
}

void LLMediaFilter::filterMediaUrl(LLParcel* parcel)
{
	if (!parcel) return;
	const std::string& url = parcel->getMediaURL();
	if (url.empty())
	{
		return;
	}

	const std::string domain = extractDomain(url);
	mCurrentParcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	U32 enabled = gSavedSettings.getU32("MediaFilterEnable");

	if (enabled > 1 && (filter(domain, WHITELIST) || filter(url, WHITELIST)))
	{
		LL_DEBUGS("MediaFilter") << "Media filter: URL allowed by whitelist: " << url << LL_ENDL;
		LLViewerParcelMedia::play(parcel);
		//mAudioState = PLAY;
	}
	else if (enabled && (filter(domain, BLACKLIST) || filter(url, BLACKLIST)))
	{
		LLNotificationsUtil::add("MediaBlocked", LLSD().with("DOMAIN", domain));
		//mAudioState = STOP;
	}
	else if (enabled && isAlertActive())
	{
		mMediaQueue = parcel;
	}
	else if (enabled > 1)
	{
		LL_DEBUGS("MediaFilter") << "Media Filter: Unhanded by lists. Toasting: " << url << LL_ENDL;
		setAlertStatus(true);
		LLSD args, payload;
		args["MEDIA_TYPE"] = LLTrans::getString("media");
		args["URL"] = url;
		args["DOMAIN"] = domain;
		payload = url;
		LLNotificationsUtil::add("MediaAlert", args, payload,
										boost::bind(&handle_media_filter_callback, _1, _2, parcel));
	}
	else
	{
		LL_DEBUGS("MediaFilter") << "Media Filter: Skipping filters and playing " << url << LL_ENDL;
		LLViewerParcelMedia::play(parcel);
		//mAudioState = PLAY;
	}
}

void LLMediaFilter::filterAudioUrl(const std::string& url)
{
	if (url.empty())
	{
		return;
	}
	if (url == mCurrentAudioURL) return;

	const std::string domain = extractDomain(url);
	mCurrentParcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	U32 enabled = gSavedSettings.getU32("MediaFilterEnable");
	
	if (enabled > 1 && (filter(domain, WHITELIST) || filter(url, WHITELIST)))
	{
		LL_DEBUGS("MediaFilter") << "Audio filter: URL allowed by whitelist: " << url << LL_ENDL;
		gAudiop->startInternetStream(url);
	}
	else if (enabled && (filter(domain, BLACKLIST) || filter(url, BLACKLIST)))
	{
		LLNotificationsUtil::add("MediaBlocked", LLSD().with("DOMAIN", domain));
		gAudiop->stopInternetStream();
	}
	else if (enabled && isAlertActive())
	{
		mAudioQueue = url;
	}
	else if (enabled > 1)
	{
		LL_DEBUGS("MediaFilter") << "Audio Filter: Unhanded by lists. Toasting: " << url << LL_ENDL;
		setAlertStatus(true);
		LLSD args, payload;
		args["MEDIA_TYPE"] = LLTrans::getString("audio");
		args["URL"] = url;
		args["DOMAIN"] = domain;
		LLNotificationsUtil::add("MediaAlert", args, LLSD(),
										boost::bind(&handle_audio_filter_callback, _1, _2, url));
	}
	else
	{
		LL_DEBUGS("MediaFilter") << "Audio Filter: Skipping filters and playing: " << url << LL_ENDL;
		gAudiop->startInternetStream(url);
	}
		
}

bool LLMediaFilter::filter(const std::string& url, EMediaList list)
{
	const string_list_t& p_list = (list == WHITELIST) ? mWhiteList : mBlackList;
	string_list_t::const_iterator find_itr = std::find(p_list.begin(), p_list.end(), url);
	return (find_itr != p_list.end());
}

// List bizznizz
void LLMediaFilter::addToMediaList(const std::string& in_url, EMediaList list, bool extract)
{
	const std::string url = extract ? extractDomain(in_url) : in_url;
	if (url.empty())
	{
		LL_INFOS("MediaFilter") << "No url found. Can't add to list." << LL_ENDL;
		return;
	}

	string_list_t& p_list(list == WHITELIST ? mWhiteList : mBlackList);
	// Check for duplicates
	for (const auto& p : p_list)
	{
		if (url == p)
		{
			LL_INFOS("MediaFilter") << "URL " << url << " already in list!" << LL_ENDL;
			return;
		}
	}
	p_list.push_back(url);
	mMediaListUpdate(list);
	saveMediaFilterToDisk();
}

void LLMediaFilter::removeFromMediaList(string_vec_t domains, EMediaList list)
{
	if (domains.empty()) return;
	for (const auto& domain : domains)
		(list == WHITELIST ? mWhiteList : mBlackList).remove(domain);
	mMediaListUpdate(list);
	saveMediaFilterToDisk();
}

void LLMediaFilter::loadMediaFilterFromDisk()
{
	const std::string medialist_filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, MEDIALIST_XML);
	mWhiteList.clear();
	mBlackList.clear();

	LLSD medialist;
	if (LLFile::isfile(medialist_filename))
	{
		llifstream medialist_xml(medialist_filename);
		LLSDSerialize::fromXML(medialist, medialist_xml);
		medialist_xml.close();
	}
	else
		LL_INFOS("MediaFilter") << medialist_filename << " not found." << LL_ENDL;

	for (LLSD::array_const_iterator p_itr = medialist.beginArray();
		 p_itr != medialist.endArray();
		 ++p_itr)
	{
		LLSD itr = (*p_itr);
		/// I hate this string shit more than you could ever imagine,
		/// but I'm retaining it for backwards and cross-compatibility. :|
		if (itr["action"].asString() == "allow")
		{
			LL_DEBUGS("MediaFilter") << "Adding " << itr["domain"].asString() << " to whitelist." << LL_ENDL;
			mWhiteList.push_back(itr["domain"].asString());
		}
		else if (itr["action"].asString() == "deny")
		{
			LL_DEBUGS("MediaFilter") << "Adding " << itr["domain"].asString() << " to blacklist." << LL_ENDL;
			mBlackList.push_back(itr["domain"].asString());
		}
	}
}

void medialist_to_llsd(const LLMediaFilter::string_list_t& list, LLSD& list_llsd, const LLSD& action)
{
	for (const auto& domain : list)
	{
		LLSD item;
		item["domain"] = domain;
		item["action"] = action;
		list_llsd.append(item);
	}
}

void LLMediaFilter::saveMediaFilterToDisk() const
{
	const std::string medialist_filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, MEDIALIST_XML);

	LLSD medialist_llsd;
	medialist_to_llsd(mWhiteList, medialist_llsd, "allow"); // <- $*#@()&%@
	medialist_to_llsd(mBlackList, medialist_llsd, "deny"); // sigh.

	llofstream medialist;
	medialist.open(medialist_filename);
	LLSDSerialize::toPrettyXML(medialist_llsd, medialist);
	medialist.close();
}

void perform_queued_command(LLMediaFilter& inst)
{
	if (U32 command = inst.getQueuedMediaCommand())
	{
		if (command == PARCEL_MEDIA_COMMAND_STOP)
		{
			LLViewerParcelMedia::stop();
		}
		else if (command == PARCEL_MEDIA_COMMAND_PAUSE)
		{
			LLViewerParcelMedia::pause();
		}
		else if (command == PARCEL_MEDIA_COMMAND_UNLOAD)
		{
			LLViewerParcelMedia::stop();
		}
		else if (command == PARCEL_MEDIA_COMMAND_TIME)
		{
			LLViewerParcelMedia::seek(LLViewerParcelMedia::sMediaCommandTime);
		}
		inst.setQueuedMediaCommand(0);
	}
}

bool handle_audio_filter_callback(const LLSD& notification, const LLSD& response, const std::string& url)
{
	LLMediaFilter& inst(LLMediaFilter::instance());
	inst.setAlertStatus(false);
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	const std::string queue = inst.getQueuedAudio();
	switch(option)
	{
		case 3:	// Whitelist domain
			inst.addToMediaList(url, LLMediaFilter::WHITELIST);
		case 0:	// Allow
			if (gAudiop != NULL)
			{
				if (inst.getCurrentParcel() == LLViewerParcelMgr::getInstance()->getAgentParcel())
				{
					gAudiop->startInternetStream(url);
				}
			}
			break;
		case 2:	// Blacklist domain
			inst.addToMediaList(url, LLMediaFilter::BLACKLIST);
		case 1:	// Deny
			if (gAudiop != NULL)
			{
				if (inst.getCurrentParcel() == LLViewerParcelMgr::getInstance()->getAgentParcel())
				{
					gAudiop->stopInternetStream();
				}
			}
			break;
		/*case 4:	//Whitelist url
			inst.addToMediaList(url, LLMediaFilter::WHITELIST, false);
			if (gAudiop != NULL)
			{
				if (inst.getCurrentParcel() == LLViewerParcelMgr::getInstance()->getAgentParcel())
				{
					gAudiop->startInternetStream(url);
				}
			}
			break;
		case 5:	//Blacklist url
			inst.addToMediaList(url, LLMediaFilter::BLACKLIST, false);
			if (gAudiop != NULL)
			{
				if (inst.getCurrentParcel() == LLViewerParcelMgr::getInstance()->getAgentParcel())
				{
					gAudiop->stopInternetStream();
				}
			}
			break;*/
		default:
			// We should never be able to get here.
			llassert(option);
			break;
	}
	if (!queue.empty())
	{
		inst.clearQueuedAudio();
		inst.filterAudioUrl(queue);
	}
	else if (inst.getQueuedMedia())
	{
		inst.clearQueuedMedia();
		LLParcel* queued_media = inst.getQueuedMedia();
		if (queued_media)
			inst.filterMediaUrl(queued_media);
	}
	else
	{
		perform_queued_command(inst);
	}

	return false;
}

bool handle_media_filter_callback(const LLSD& notification, const LLSD& response, LLParcel* parcel)
{
	LLMediaFilter& inst(LLMediaFilter::instance());
	inst.setAlertStatus(false);
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	const std::string& url = notification["payload"].asString();
	LLParcel* queue = inst.getQueuedMedia();
	switch(option)
	{
		case 3:	// Whitelist domain
			inst.addToMediaList(url, LLMediaFilter::WHITELIST);
		case 0:	// Allow
			if (inst.getCurrentParcel() == LLViewerParcelMgr::getInstance()->getAgentParcel())
				LLViewerParcelMedia::play(parcel);
			break;
		case 2:	// Blacklist domain
			inst.addToMediaList(url, LLMediaFilter::BLACKLIST);
		case 1:	// Deny
			break;
		case 4:	//Whitelist url
			inst.addToMediaList(url, LLMediaFilter::WHITELIST, false);
			if (inst.getCurrentParcel() == LLViewerParcelMgr::getInstance()->getAgentParcel())
				LLViewerParcelMedia::play(parcel);
			break;
		case 5:
			inst.addToMediaList(url, LLMediaFilter::BLACKLIST, false);
			break;
		default:
			// We should never be able to get here.
			llassert(option);
			break;
	}
	const std::string audio_queue = inst.getQueuedAudio();
	if (queue)
	{
		inst.clearQueuedMedia();
		inst.filterMediaUrl(queue);
	}
	else if (!audio_queue.empty())
	{
		inst.clearQueuedAudio();
		inst.filterAudioUrl(audio_queue);
	}
	else
	{
		perform_queued_command(inst);
	}

	return false;
}

// Local Functions
std::string extractDomain(const std::string& in_url)
{
	std::string url = in_url;
	// First, find and strip any protocol prefix.
	size_t pos = url.find("//");

	if (pos != std::string::npos)
	{
		size_t count = url.size()-pos+2;
		url = url.substr(pos+2, count);
	}

	// Now, look for a / marking a local part; if there is one,
	//  strip it and anything after.
	pos = url.find("/");

	if (pos != std::string::npos)
	{
		url = url.substr(0, pos);
	}

	// If there's a user{,:password}@ part, remove it,
	pos = url.find("@");

	if (pos != std::string::npos)
	{
		size_t count = url.size()-pos+1;
		url = url.substr(pos+1, count);
	}

	// Finally, find and strip away any port number. This has to be done
	//  after the previous step, or else the extra : for the password,
	//  if supplied, will confuse things.
	pos = url.find(":");

	if (pos != std::string::npos)
	{
		url = url.substr(0, pos);
	}

	// Now map the whole thing to lowercase, since domain names aren't
	//  case sensitive.
	std::transform(url.begin(), url.end(), url.begin(), ::tolower);
	return url;
}
