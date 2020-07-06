/** 
 * @file llexperiencecache.cpp
 * @brief llexperiencecache and related class definitions
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2012, Linden Research, Inc.
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
#include "llexperiencecache.h"

#include "llavatarname.h"
#include "llcororesponder.h"
#include "llsdserialize.h"
#include "lleventfilter.h"
#include "lldir.h"
#include <set>
#include <map>
#include <boost/tokenizer.hpp>
#include <boost/concept_check.hpp>
#include <boost/smart_ptr/make_shared.hpp>

//=========================================================================
namespace LLExperienceCacheImpl
{
	void mapKeys(const LLSD& legacyKeys);
    F64 getErrorRetryDeltaTime(S32 status, const AIHTTPReceivedHeaders& headers);
    bool maxAgeFromCacheControl(const std::string& cache_control, S32 *max_age);

    static const std::string PRIVATE_KEY    = "private_id";
    static const std::string EXPERIENCE_ID  = "public_id";

    static const std::string MAX_AGE("max-age");
    static const boost::char_separator<char> EQUALS_SEPARATOR("=");
    static const boost::char_separator<char> COMMA_SEPARATOR(",");

    // *TODO$: this seems to be tied to mapKeys which is used by bootstrap.... but I don't think that bootstrap is used.
    typedef std::map<LLUUID, LLUUID> KeyMap;
    KeyMap privateToPublicKeyMap;
}

//=========================================================================
const std::string LLExperienceCache::PRIVATE_KEY	= "private_id";
const std::string LLExperienceCache::MISSING       	= "DoesNotExist";

const std::string LLExperienceCache::AGENT_ID      = "agent_id";
const std::string LLExperienceCache::GROUP_ID      = "group_id";
const std::string LLExperienceCache::EXPERIENCE_ID	= "public_id";
const std::string LLExperienceCache::NAME			= "name";
const std::string LLExperienceCache::PROPERTIES		= "properties";
const std::string LLExperienceCache::EXPIRES		= "expiration";  
const std::string LLExperienceCache::DESCRIPTION	= "description";
const std::string LLExperienceCache::QUOTA         	= "quota";
const std::string LLExperienceCache::MATURITY      = "maturity";
const std::string LLExperienceCache::METADATA      = "extended_metadata";
const std::string LLExperienceCache::SLURL         	= "slurl";

// should be in sync with experience-api/experiences/models.py
const int LLExperienceCache::PROPERTY_INVALID		= 1 << 0;
const int LLExperienceCache::PROPERTY_PRIVILEGED	= 1 << 3;
const int LLExperienceCache::PROPERTY_GRID			= 1 << 4;
const int LLExperienceCache::PROPERTY_PRIVATE		= 1 << 5;
const int LLExperienceCache::PROPERTY_DISABLED	= 1 << 6;  
const int LLExperienceCache::PROPERTY_SUSPENDED	= 1 << 7;

// default values
const F64 LLExperienceCache::DEFAULT_EXPIRATION	= 600.0;
const S32 LLExperienceCache::DEFAULT_QUOTA			= 128; // this is megabytes
const int LLExperienceCache::SEARCH_PAGE_SIZE     = 30;

//=========================================================================
LLExperienceCache::LLExperienceCache():
    mShutdown(false)
{
}

LLExperienceCache::~LLExperienceCache()
{

}

void LLExperienceCache::initSingleton()
{
    mCacheFileName = gDirUtilp->getExpandedFilename(LL_PATH_CACHE, "experience_cache.xml");

    LL_INFOS("ExperienceCache") << "Loading " << mCacheFileName << LL_ENDL;
    llifstream cache_stream(mCacheFileName.c_str());

    if (cache_stream.is_open())
    {
        cache_stream >> (*this);
    }
}

void LLExperienceCache::cleanup()
{
    LL_INFOS("ExperienceCache") << "Saving " << mCacheFileName << LL_ENDL;

    llofstream cache_stream(mCacheFileName.c_str());
    if (cache_stream.is_open())
    {
        cache_stream << (*this);
    }
    mShutdown = true;
}

//-------------------------------------------------------------------------
void LLExperienceCache::importFile(std::istream& istr)
{
    LLSD data;
    S32 parse_count = LLSDSerialize::fromXMLDocument(data, istr);
    if (parse_count < 1) return;

    LLSD experiences = data["experiences"];

    LLUUID public_key;
    for (const auto& it : experiences.map())
    {
        public_key.set(it.first);
        mCache[public_key] = it.second;
    }

    LL_DEBUGS("ExperienceCache") << "importFile() loaded " << mCache.size() << LL_ENDL;
}

void LLExperienceCache::exportFile(std::ostream& ostr) const
{
    LLSD experiences;

    cache_t::const_iterator it = mCache.begin();
    for (; it != mCache.end(); ++it)
    {
        if (!it->second.has(EXPERIENCE_ID) || it->second[EXPERIENCE_ID].asUUID().isNull() ||
            it->second.has("DoesNotExist") || (it->second.has(PROPERTIES) && it->second[PROPERTIES].asInteger() & PROPERTY_INVALID))
            continue;

        experiences[it->first.asString()] = it->second;
    }

    LLSD data;
    data["experiences"] = experiences;

    LLSDSerialize::toPrettyXML(data, ostr);
}

// *TODO$: Rider: This method does not seem to be used... it may be useful in testing.
void LLExperienceCache::bootstrap(const LLSD& legacyKeys, int initialExpiration)
{
	LLExperienceCacheImpl::mapKeys(legacyKeys);
    for (auto experience : legacyKeys.array())
    {
        if (experience.has(EXPERIENCE_ID))
        {
            if (!experience.has(EXPIRES))
            {
                experience[EXPIRES] = initialExpiration;
            }
            processExperience(experience[EXPERIENCE_ID].asUUID(), experience);
        }
        else
        {
            LL_WARNS("ExperienceCache")
                << "Skipping bootstrap entry which is missing " << EXPERIENCE_ID
                << LL_ENDL;
        }
    }
}

LLUUID LLExperienceCache::getExperienceId(const LLUUID& private_key, bool null_if_not_found)
{
    if (private_key.isNull())
        return LLUUID::null;

    LLExperienceCacheImpl::KeyMap::const_iterator it = LLExperienceCacheImpl::privateToPublicKeyMap.find(private_key);
    if (it == LLExperienceCacheImpl::privateToPublicKeyMap.end())
    {
        if (null_if_not_found)
        {
            return LLUUID::null;
        }
        return private_key;
    }
    LL_WARNS("LLExperience") << "converted private key " << private_key << " to experience_id " << it->second << LL_ENDL;
    return it->second;
}

//=========================================================================
void LLExperienceCache::processExperience(const LLUUID& public_key, const LLSD& experience)
{
    LL_INFOS("ExperienceCache") << "Processing experience \"" << experience[NAME] << "\" with key " << public_key.asString() << LL_ENDL;

	mCache[public_key]=experience;
	LLSD & row = mCache[public_key];

	if(row.has(EXPIRES))
	{
		row[EXPIRES] = row[EXPIRES].asReal() + LLFrameTimer::getTotalSeconds();
	}

	if(row.has(EXPERIENCE_ID))
	{
		mPendingQueue.erase(row[EXPERIENCE_ID].asUUID());
	}

	//signal
	signal_map_t::iterator sig_it =	mSignalMap.find(public_key);
	if (sig_it != mSignalMap.end())
	{
		signal_ptr signal = sig_it->second;
		(*signal)(experience);

		mSignalMap.erase(public_key);
	}
}

const LLExperienceCache::cache_t& LLExperienceCache::getCached()
{
	return mCache;
}

void LLExperienceCache::requestExperiencesCoro(const LLCoroResponder& responder, RequestQueue_t requests)
{
    //LL_INFOS("requestExperiencesCoro") << "url: " << url << LL_ENDL;

    LLSD result = responder.getContent();
	auto status = responder.getStatus();

    if (!responder.isGoodStatus(status))
    {
        F64 now = LLFrameTimer::getTotalSeconds();

        auto headers = responder.getHeaders();
        // build dummy entries for the failed requests
        for (auto request : requests)
        {
            LLSD exp = get(request);
            //leave the properties alone if we already have a cache entry for this xp
            if (exp.isUndefined())
            {
                exp[PROPERTIES] = PROPERTY_INVALID;
            }
            exp[EXPIRES] = now + LLExperienceCacheImpl::getErrorRetryDeltaTime(status, headers);
            exp[EXPERIENCE_ID] = request;
            exp["key_type"] = EXPERIENCE_ID;
            exp["uuid"] = request;
            exp["error"] = status;
            exp[QUOTA] = DEFAULT_QUOTA;

            processExperience(request, exp);
        }
        return;
    }

    LLSD experiences = result["experience_keys"];
    
    for (const auto& row : experiences.array())
    {
        LLUUID public_key = row[EXPERIENCE_ID].asUUID();

        LL_DEBUGS("ExperienceCache") << "Received result for " << public_key
            << " display '" << row[LLExperienceCache::NAME].asString() << "'" << LL_ENDL;

        processExperience(public_key, row);
    }

    LLSD error_ids = result["error_ids"];
    
    for (const auto& err : error_ids.array())
    {
        LLUUID id = err.asUUID();
        LLSD exp;
        exp[EXPIRES] = DEFAULT_EXPIRATION;
        exp[EXPERIENCE_ID] = id;
        exp[PROPERTIES] = PROPERTY_INVALID;
        exp[MISSING] = true;
        exp[QUOTA] = DEFAULT_QUOTA;

        processExperience(id, exp);
        LL_WARNS("ExperienceCache") << "LLExperienceResponder::result() error result for " << id << LL_ENDL;
    }

}

void LLExperienceCache::requestExperiences()
{
    if (mCapability == nullptr)
    {
        LL_WARNS("ExperienceCache") << "Capability query method not set." << LL_ENDL;
        return;
    }

    std::string urlBase = mCapability("GetExperienceInfo");
    if (urlBase.empty())
    {
        LL_DEBUGS("ExperienceCache") << "No Experience capability." << LL_ENDL;
        return;
    }

    if (*urlBase.rbegin() != '/')
    {
        urlBase += "/";
    }
    urlBase += "id/";


	F64 now = LLFrameTimer::getTotalSeconds();

    const U32 EXP_URL_SEND_THRESHOLD = 3000;
    constexpr U32 EXP_PAGE_SIZE = EXP_URL_SEND_THRESHOLD / UUID_STR_LENGTH;

    std::ostringstream ostr;
    ostr << urlBase << "?page_size=" << EXP_PAGE_SIZE;
    RequestQueue_t  requests;

    while (!mRequestQueue.empty())
    {
        RequestQueue_t::iterator it = mRequestQueue.begin();
        LLUUID key = (*it);
        mRequestQueue.erase(it);
        requests.insert(key);

        ostr << "&" << EXPERIENCE_ID << "=" << key.asString();
        mPendingQueue[key] = now;
        
        if (mRequestQueue.empty() || (ostr.tellp() > EXP_URL_SEND_THRESHOLD))
        {   // request is placed in the coprocedure pool for the ExpCache cache.  Throttling is done by the pool itself.
            LLHTTPClient::get(ostr.str(), new LLCoroResponder(
            	boost::bind(&LLExperienceCache::requestExperiencesCoro, this, _1, requests) ));

            ostr.str(std::string());
            ostr << urlBase << "?page_size=" << EXP_PAGE_SIZE;
            requests.clear();
        }
    }

}


bool LLExperienceCache::isRequestPending(const LLUUID& public_key)
{
	bool isPending = false;
	const F64 PENDING_TIMEOUT_SECS = 5.0 * 60.0;

    PendingQueue_t::const_iterator it = mPendingQueue.find(public_key);

	if(it != mPendingQueue.end())
	{
		F64 expire_time = LLFrameTimer::getTotalSeconds() - PENDING_TIMEOUT_SECS;
		isPending = (it->second > expire_time);
	}

	return isPending;
}

void LLExperienceCache::setCapabilityQuery(LLExperienceCache::CapabilityQuery_t queryfn)
{
    mCapability = queryfn;
}


void LLExperienceCache::idleCoro()
{
    const F32 SECS_BETWEEN_REQUESTS = 0.5f;
    const F32 ERASE_EXPIRED_TIMEOUT = 60.f; // seconds

    {
        static LLFrameTimer sRequestTimer;
        if (!sRequestTimer.checkExpirationAndReset(SECS_BETWEEN_REQUESTS)) return;

        if (mEraseExpiredTimer.checkExpirationAndReset(ERASE_EXPIRED_TIMEOUT))
        {
            eraseExpired();
        }

        if (!mRequestQueue.empty())
        {
            requestExperiences();
        }
    }

    // The coroutine system will likely be shut down by the time we get to this point
    // (or at least no further cycling will occur on it since the user has decided to quit.)
}

void LLExperienceCache::erase(const LLUUID& key)
{
	cache_t::iterator it = mCache.find(key);
				
	if(it != mCache.end())
	{
		mCache.erase(it);
	}
}

void LLExperienceCache::eraseExpired()
{
	F64 now = LLFrameTimer::getTotalSeconds();
	cache_t::iterator it = mCache.begin();
	while (it != mCache.end())
	{
		cache_t::iterator cur = it;
		LLSD& exp = cur->second;
		++it;

        //LL_INFOS("ExperienceCache") << "Testing experience \"" << exp[NAME] << "\" with exp time " << exp[EXPIRES].asReal() << "(now = " << now << ")" << LL_ENDL;

		if(exp.has(EXPIRES) && exp[EXPIRES].asReal() < now)
		{
            if(!exp.has(EXPERIENCE_ID))
			{
                LL_WARNS("ExperienceCache") << "Removing experience with no id " << LL_ENDL ;
                mCache.erase(cur);
			}
            else
            {
                LLUUID id = exp[EXPERIENCE_ID].asUUID();
                LLUUID private_key = exp.has(LLExperienceCache::PRIVATE_KEY) ? exp[LLExperienceCache::PRIVATE_KEY].asUUID():LLUUID::null;
                if(private_key.notNull() || !exp.has("DoesNotExist"))
				{
					fetch(id, true);
				}
				else
				{
                    LL_WARNS("ExperienceCache") << "Removing invalid experience " << id << LL_ENDL ;
					mCache.erase(cur);
				}
			}
		}
	}
}
	
bool LLExperienceCache::fetch(const LLUUID& key, bool refresh/* = true*/)
{
	if(!key.isNull() && !isRequestPending(key) && (refresh || mCache.find(key)==mCache.end()))
	{
		LL_DEBUGS("ExperienceCache") << " queue request for " << EXPERIENCE_ID << " " << key << LL_ENDL;

        mRequestQueue.insert(key);
		return true;
	}
	return false;
}

void LLExperienceCache::insert(const LLSD& experience_data)
{
	if(experience_data.has(EXPERIENCE_ID))
	{
        processExperience(experience_data[EXPERIENCE_ID].asUUID(), experience_data);
	}
	else
	{
		LL_WARNS("ExperienceCache") << ": Ignoring cache insert of experience which is missing " << EXPERIENCE_ID << LL_ENDL;
	}
}

const LLSD& LLExperienceCache::get(const LLUUID& key)
{
	static const LLSD empty;
	
	if(key.isNull()) 
		return empty;
	cache_t::const_iterator it = mCache.find(key);

	if (it != mCache.end())
	{
		return it->second;
	}
	fetch(key);

	return empty;
}

void LLExperienceCache::get(const LLUUID& key, LLExperienceCache::ExperienceGetFn_t slot)
{
	if(key.isNull()) 
		return;

	cache_t::const_iterator it = mCache.find(key);
	if (it != mCache.end())
	{
		// ...name already exists in cache, fire callback now
		callback_signal_t signal;
		signal.connect(slot);
			
		signal(it->second);
		return;
	}

	fetch(key);

	signal_ptr signal = boost::make_shared<callback_signal_t>();
	
	std::pair<signal_map_t::iterator, bool> result = mSignalMap.insert(signal_map_t::value_type(key, signal));
	if (!result.second)
		signal = (*result.first).second;
	signal->connect(slot);
}

//=========================================================================
void LLExperienceCache::fetchAssociatedExperience(const LLUUID& objectId, const LLUUID& itemId, std::string url, ExperienceGetFn_t fn)
{
    if (mCapability == nullptr)
    {
        LL_WARNS("ExperienceCache") << "Capability query method not set." << LL_ENDL;
        return;
    }


    if (url.empty())
    {
        url = mCapability("GetMetadata");

        if (url.empty())
        {
            LL_WARNS("ExperienceCache") << "No Metadata capability." << LL_ENDL;
            return;
        }
    }

    LLSD fields;
    fields.append("experience");
    LLSD data;
    data["object-id"] = objectId;
    data["item-id"] = itemId;
    data["fields"] = fields;

    LLHTTPClient::post(url, data, new LLCoroResponder(
        boost::bind(&LLExperienceCache::fetchAssociatedExperienceCoro, this, _1, fn)));
}

void LLExperienceCache::fetchAssociatedExperienceCoro(const LLCoroResponder& responder, ExperienceGetFn_t fn)
{
    LLSD result = responder.getContent();
    auto status = responder.getStatus();

    if (!responder.isGoodStatus(status) || !result.has("experience"))
    {
        LLSD failure;
        if (!status)
        {
            failure["error"] = status;
            failure["message"] = responder.getReason();
        }
        else 
        {
            failure["error"] = -1;
            failure["message"] = "no experience";
        }
        if (fn != nullptr)
            fn(failure);
        return;
    }

    LLUUID expId = result["experience"].asUUID();
    get(expId, fn);
}

//-------------------------------------------------------------------------
void LLExperienceCache::findExperienceByName(const std::string text, int page, ExperienceGetFn_t fn)
{
    if (mCapability == nullptr)
    {
        LL_WARNS("ExperienceCache") << "Capability query method not set." << LL_ENDL;
        return;
    }

    std::ostringstream url;

    url << mCapability("FindExperienceByName")  << "?page=" << page << "&page_size=" << SEARCH_PAGE_SIZE << "&query=" << LLURI::escape(text);

    LLHTTPClient::get(url.str(), new LLCoroResponder(
        boost::bind(&LLExperienceCache::findExperienceByNameCoro, this, _1, fn)));
}

void LLExperienceCache::findExperienceByNameCoro(const LLCoroResponder& responder, ExperienceGetFn_t fn)
{
    LLSD result = responder.getContent();

    if (!responder.isGoodStatus(responder.getStatus()))
    {
        fn(LLSD());
        return;
    }

    const LLSD& experiences = result["experience_keys"];
    for (const auto& it : experiences.array())
    {
        insert(it);
    }

    fn(result);
}

//-------------------------------------------------------------------------
void LLExperienceCache::getGroupExperiences(const LLUUID &groupId, ExperienceGetFn_t fn)
{
    if (mCapability == nullptr)
    {
        LL_WARNS("ExperienceCache") << "Capability query method not set." << LL_ENDL;
        return;
    }

    // search for experiences owned by the current group
    std::string url = mCapability("GroupExperiences");
    if (url.empty())
    {
        LL_WARNS("ExperienceCache") << "No Group Experiences capability" << LL_ENDL;
        return;
    }

    url += "?" + groupId.asString();

    LLHTTPClient::get(url, new LLCoroResponder(
        boost::bind(&LLExperienceCache::getGroupExperiencesCoro, this, _1, fn)));
}

void LLExperienceCache::getGroupExperiencesCoro(const LLCoroResponder& responder, ExperienceGetFn_t fn)
{
    LLSD result = responder.getContent();

    if (!responder.isGoodStatus(responder.getStatus()))
    {
        fn(LLSD());
        return;
    }

    const LLSD& experienceIds = result["experience_ids"];
    fn(experienceIds);
}

//-------------------------------------------------------------------------
void LLExperienceCache::getRegionExperiences(CapabilityQuery_t regioncaps, ExperienceGetFn_t fn)
{
	regionExperiences(regioncaps, LLSD(), false, fn);
}

void LLExperienceCache::setRegionExperiences(CapabilityQuery_t regioncaps, const LLSD &experiences, ExperienceGetFn_t fn)
{
	regionExperiences(regioncaps, experiences, true, fn);
}

void LLExperienceCache::regionExperiences(CapabilityQuery_t regioncaps, const LLSD &experiences, bool update, ExperienceGetFn_t fn)
{
    // search for experiences owned by the current group
    std::string url = regioncaps("RegionExperiences");
    if (url.empty())
    {
        LL_WARNS("ExperienceCache") << "No Region Experiences capability" << LL_ENDL;
        return;
    }

    auto httpRequest = new LLCoroResponder(
		boost::bind(&LLExperienceCache::regionExperiencesCoro, this, _1, fn));

    LLSD result;
    if (update)
        LLHTTPClient::post(url, experiences, httpRequest);
    else
        LLHTTPClient::get(url, httpRequest);
}

void LLExperienceCache::regionExperiencesCoro(const LLCoroResponder& responder, ExperienceGetFn_t fn)
{
    LLSD result = responder.getContent();

    if (!responder.isGoodStatus(responder.getStatus()))
    {
//      fn(LLSD());
        return;
    }

    fn(result);

}

//-------------------------------------------------------------------------
void LLExperienceCache::getExperiencePermission(const LLUUID &experienceId, ExperienceGetFn_t fn)
{
    if (mCapability == nullptr)
    {
        LL_WARNS("ExperienceCache") << "Capability query method not set." << LL_ENDL;
        return;
    }

    std::string url = mCapability("ExperiencePreferences") + "?" + experienceId.asString();

    LLHTTPClient::get(url, new LLCoroResponder(
        boost::bind(&LLExperienceCache::experiencePermissionCoro, this, _1, fn)));
}

void LLExperienceCache::setExperiencePermission(const LLUUID &experienceId, const std::string &permission, ExperienceGetFn_t fn)
{
    if (mCapability == nullptr)
    {
        LL_WARNS("ExperienceCache") << "Capability query method not set." << LL_ENDL;
        return;
    }

    std::string url = mCapability("ExperiencePreferences");
    if (url.empty())
        return;
    LLSD permData;
    LLSD data;
    permData["permission"] = permission;
    data[experienceId.asString()] = permData;

    LLHTTPClient::put(url, data, new LLCoroResponder(
        boost::bind(&LLExperienceCache::experiencePermissionCoro, this, _1, fn)));
}

void LLExperienceCache::forgetExperiencePermission(const LLUUID &experienceId, ExperienceGetFn_t fn)
{
    if (mCapability == nullptr)
    {
        LL_WARNS("ExperienceCache") << "Capability query method not set." << LL_ENDL;
        return;
    }

    std::string url = mCapability("ExperiencePreferences") + "?" + experienceId.asString();
    LLHTTPClient::del(url, new LLCoroResponder(
        boost::bind(&LLExperienceCache::experiencePermissionCoro, this, _1, fn)));
}

void LLExperienceCache::experiencePermissionCoro(const LLCoroResponder& responder, ExperienceGetFn_t fn)
{
    // search for experiences owned by the current group

    LLSD result = responder.getContent();

    if (responder.isGoodStatus(responder.getStatus()))
    {
        fn(result);
    }
}

//-------------------------------------------------------------------------
void LLExperienceCache::getExperienceAdmin(const LLUUID &experienceId, ExperienceGetFn_t fn)
{
    if (mCapability == nullptr)
    {
        LL_WARNS("ExperienceCache") << "Capability query method not set." << LL_ENDL;
        return;
    }

    std::string url = mCapability("IsExperienceAdmin");
    if (url.empty())
    {
        LL_WARNS("ExperienceCache") << "No Region Experiences capability" << LL_ENDL;
        return;
    }
    url += "?experience_id=" + experienceId.asString();

    LLHTTPClient::get(url, new LLCoroResponder(
        boost::bind(fn, boost::bind(&LLCoroResponder::getContent, _1))));
}

//-------------------------------------------------------------------------
void LLExperienceCache::updateExperience(LLSD updateData, ExperienceGetFn_t fn)
{
    if (mCapability == nullptr)
    {
        LL_WARNS("ExperienceCache") << "Capability query method not set." << LL_ENDL;
        return;
    }

    std::string url = mCapability("UpdateExperience");
    if (url.empty())
    {
        LL_WARNS("ExperienceCache") << "No Region Experiences capability" << LL_ENDL;
        return;
    }

    updateData.erase(LLExperienceCache::QUOTA);
    updateData.erase(LLExperienceCache::EXPIRES);
    updateData.erase(LLExperienceCache::AGENT_ID);

    LLHTTPClient::post(url, updateData, new LLCoroResponder(
        boost::bind(fn, boost::bind(&LLCoroResponder::getContent, _1))));
}

//=========================================================================
void LLExperienceCacheImpl::mapKeys(const LLSD& legacyKeys)
{
    for (const auto& exp : legacyKeys.array())
    {
        if (exp.has(LLExperienceCacheImpl::EXPERIENCE_ID) && exp.has(LLExperienceCacheImpl::PRIVATE_KEY))
        {
            LLExperienceCacheImpl::privateToPublicKeyMap[exp[LLExperienceCacheImpl::PRIVATE_KEY].asUUID()] = 
                exp[LLExperienceCacheImpl::EXPERIENCE_ID].asUUID();
        }
    }
}

// Return time to retry a request that generated an error, based on
// error type and headers.  Return value is seconds-since-epoch.
F64 LLExperienceCacheImpl::getErrorRetryDeltaTime(S32 status, const AIHTTPReceivedHeaders& headers)
{
    // Retry-After takes priority
    std::string retry_afters;
    if (headers.getFirstValue("retry-after", retry_afters))
    {
    	LLSD retry_after(retry_afters);
        // We only support the delta-seconds type
        S32 delta_seconds = retry_after.asInteger();
        if (delta_seconds > 0)
        {
            // ...valid delta-seconds
            return F64(delta_seconds);
        }
    }

    // If no Retry-After, look for Cache-Control max-age
    // Allow the header to override the default
    std::string cache_control;
    if (headers.getFirstValue("cache-control", cache_control))
    {
        S32 max_age = 0;
        if (LLExperienceCacheImpl::maxAgeFromCacheControl(cache_control, &max_age))
        {
            LL_WARNS("ExperienceCache")
                << "got EXPIRES from headers, max_age " << max_age
                << LL_ENDL;
            return (F64)max_age;
        }
    }

    // No information in header, make a guess
    if (status == 503)
    {
        // ...service unavailable, retry soon
        const F64 SERVICE_UNAVAILABLE_DELAY = 600.0; // 10 min
        return SERVICE_UNAVAILABLE_DELAY;
    }
    else if (status == 499)
    {
        // ...we were probably too busy, retry quickly
        const F64 BUSY_DELAY = 10.0; // 10 seconds
        return BUSY_DELAY;

    }
    else
    {
        // ...other unexpected error
        const F64 DEFAULT_DELAY = 3600.0; // 1 hour
        return DEFAULT_DELAY;
    }
}

bool LLExperienceCacheImpl::maxAgeFromCacheControl(const std::string& cache_control, S32 *max_age)
{
	// Split the string on "," to get a list of directives
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	tokenizer directives(cache_control, COMMA_SEPARATOR);
	
	tokenizer::iterator token_it = directives.begin();
	for ( ; token_it != directives.end(); ++token_it)
	{
		// Tokens may have leading or trailing whitespace
		std::string token = *token_it;
		LLStringUtil::trim(token);
		
		if (token.compare(0, MAX_AGE.size(), MAX_AGE) == 0)
		{
			// ...this token starts with max-age, so let's chop it up by "="
			tokenizer subtokens(token, EQUALS_SEPARATOR);
			tokenizer::iterator subtoken_it = subtokens.begin();
			
			// Must have a token
			if (subtoken_it == subtokens.end()) return false;
			std::string subtoken = *subtoken_it;
			
			// Must exactly equal "max-age"
			LLStringUtil::trim(subtoken);
			if (subtoken != MAX_AGE) return false;
			
			// Must have another token
			++subtoken_it;
			if (subtoken_it == subtokens.end()) return false;
			subtoken = *subtoken_it;
			
			// Must be a valid integer
			// *NOTE: atoi() returns 0 for invalid values, so we have to
			// check the string first.
			// *TODO: Do servers ever send "0000" for zero?  We don't handle it
			LLStringUtil::trim(subtoken);
			if (subtoken == "0")
			{
				*max_age = 0;
				return true;
			}
			S32 val = atoi( subtoken.c_str() );
			if (val > 0 && val < S32_MAX)
			{
				*max_age = val;
				return true;
			}
			return false;
		}
	}
	return false;
}




