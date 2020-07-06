/** 
 * @file llmutelist.cpp
 * @author Richard Nelson, James Cook
 * @brief Management of list of muted players
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

/*
 * How should muting work?
 * Mute an avatar
 * Mute a specific object (accidentally spamming)
 *
 * right-click avatar, mute
 * see list of recent chatters, mute
 * type a name to mute?
 *
 * show in list whether chatter is avatar or object
 *
 * need fast lookup by id
 * need lookup by name, doesn't have to be fast
 */

#include "llviewerprecompiledheaders.h"

#include "llmutelist.h"

#include "pipeline.h"

#include <boost/tokenizer.hpp>

#include "lldispatcher.h"
#include "llxfermanager.h"

#include "llagent.h"
#include "llavataractions.h"
#include "llviewergenericmessage.h"	// for gGenericDispatcher
#include "llworld.h" //for particle system banning
#include "llfloaterchat.h"
#include "llimpanel.h"
#include "llimview.h"
#include "llnotifications.h"
#include "llviewerobjectlist.h"
#include "lltrans.h"

namespace 
{
	// This method is used to return an object to mute given an object id.
	// Its used by the LLMute constructor and LLMuteList::isMuted.
	LLViewerObject* get_object_to_mute_from_id(const LLUUID& object_id)
	{
		LLViewerObject *objectp = gObjectList.findObject(object_id);
		if ((objectp) && (!objectp->isAvatar()))
		{
			LLViewerObject *parentp = (LLViewerObject *)objectp->getParent();
			if (parentp && parentp->getID() != gAgent.getID())
			{
				objectp = parentp;
			}
		}
		return objectp;
	}
}

// "emptymutelist"
class LLDispatchEmptyMuteList : public LLDispatchHandler
{
public:
	bool operator()(
		const LLDispatcher* dispatcher,
		const std::string& key,
		const LLUUID& invoice,
		const sparam_t& strings) override
	{
		LLMuteList::getInstance()->setLoaded();
		return true;
	}
};

static LLDispatchEmptyMuteList sDispatchEmptyMuteList;

//-----------------------------------------------------------------------------
// LLMute()
//-----------------------------------------------------------------------------

LLMute::LLMute(const LLUUID& id, const std::string& name, EType type, U32 flags)
  : mID(id),
	mName(name),
	mType(type),
	mFlags(flags)
{
	// muting is done by root objects only - try to find this objects root
	LLViewerObject* mute_object = get_object_to_mute_from_id(id);
	if(mute_object && mute_object->getID() != id)
	{
		mID = mute_object->getID();
		LLNameValue* firstname = mute_object->getNVPair("FirstName");
		LLNameValue* lastname = mute_object->getNVPair("LastName");
		if (firstname && lastname)
		{
			mName = LLCacheName::buildFullName(
				firstname->getString(), lastname->getString());
		}
		mType = mute_object->isAvatar() ? AGENT : OBJECT;
	}

}


std::string LLMute::getDisplayType() const
{
	switch (mType)
	{
		case BY_NAME:
		default:
			return LLTrans::getString("MuteByName");
			break;
		case AGENT:
			return LLTrans::getString("MuteAgent");
			break;
		case OBJECT:
			return LLTrans::getString("MuteObject");
			break;
		case GROUP:
			return LLTrans::getString("MuteGroup");
			break;
		case EXTERNAL:
			return LLTrans::getString("MuteExternal");
			break;
	}
}


/* static */
LLMuteList* LLMuteList::getInstance()
{
	// Register callbacks at the first time that we find that the message system has been created.
	static bool registered = false;
	if( !registered && gMessageSystem)
	{
		registered = true;
		// Register our various callbacks
		gMessageSystem->setHandlerFuncFast(_PREHASH_MuteListUpdate, processMuteListUpdate);
		gMessageSystem->setHandlerFuncFast(_PREHASH_UseCachedMuteList, processUseCachedMuteList);
	}
	return LLSingleton<LLMuteList>::getInstance(); // Call the "base" implementation.
}

//-----------------------------------------------------------------------------
// LLMuteList()
//-----------------------------------------------------------------------------
LLMuteList::LLMuteList() :
	mIsLoaded(FALSE)
{
	gGenericDispatcher.addHandler("emptymutelist", &sDispatchEmptyMuteList);
}

//-----------------------------------------------------------------------------
// ~LLMuteList()
//-----------------------------------------------------------------------------
LLMuteList::~LLMuteList()
{

}

bool LLMuteList::isLinden(const LLUUID& id) const
{
	std::string name;
	gCacheName->getFullName(id, name);
	return isLinden(name);
}

BOOL LLMuteList::isLinden(const std::string& name) const
{
	if (mGodFullNames.find(name) != mGodFullNames.end()) return true;
	if (mGodLastNames.empty()) return false;

	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep(" ");
	tokenizer tokens(name, sep);
	tokenizer::iterator token_iter = tokens.begin();
	
	if (token_iter == tokens.end()) return FALSE;
	++token_iter;
	if (token_iter == tokens.end()) return FALSE;
	
	std::string last_name = *token_iter;
	return mGodLastNames.find(last_name) != mGodLastNames.end();
}

static LLVOAvatar* find_avatar(const LLUUID& id)
{
	LLViewerObject *obj = gObjectList.findObject(id);
	while (obj && obj->isAttachment())
	{
		obj = (LLViewerObject *)obj->getParent();
	}

	if (obj && obj->isAvatar())
	{
		return (LLVOAvatar*)obj;
	}
	else
	{
		return nullptr;
	}
}

BOOL LLMuteList::add(const LLMute& mute, U32 flags)
{
	// Can't mute text from Lindens
	if ((mute.mType == LLMute::AGENT)
		&& isLinden(mute.mName) && (flags & LLMute::flagTextChat || flags == 0))
	{
        LL_WARNS() << "Trying to mute a Linden; ignored" << LL_ENDL;
		LLNotifications::instance().add("MuteLinden", LLSD(), LLSD());
		return FALSE;
	}
	
	// Can't mute self.
	if (mute.mType == LLMute::AGENT
		&& mute.mID == gAgent.getID())
	{
        LL_WARNS() << "Trying to self; ignored" << LL_ENDL;
		return FALSE;
	}
	
	if (mute.mType == LLMute::BY_NAME)
	{		
		// Can't mute empty string by name
		if (mute.mName.empty()) 
		{
			LL_WARNS() << "Trying to mute empty string by-name" << LL_ENDL;
			return FALSE;
		}

		// Null mutes must have uuid null
		if (mute.mID.notNull())
		{
			LL_WARNS() << "Trying to add by-name mute with non-null id" << LL_ENDL;
			return FALSE;
		}

		std::pair<string_set_t::iterator, bool> result = mLegacyMutes.insert(mute.mName);
		if (result.second)
		{
			LL_INFOS() << "Muting by name " << mute.mName << LL_ENDL;
			updateAdd(mute);
			notifyObservers();
			notifyObserversDetailed(mute);
			return TRUE;
		}
		else
		{
			LL_INFOS() << "duplicate mute ignored" << LL_ENDL;
			// was duplicate
			return FALSE;
		}
	}

	// Need a local (non-const) copy to set up flags properly.
	LLMute localmute = mute;
	
	// If an entry for the same entity is already in the list, remove it, saving flags as necessary.
	mute_set_t::iterator it = mMutes.find(localmute);
	bool duplicate = it != mMutes.end();
	if (duplicate)
	{
		// This mute is already in the list.  Save the existing entry's flags if that's warranted.
		localmute.mFlags = it->mFlags;
		
		mMutes.erase(it);
		// Don't need to call notifyObservers() here, since it will happen after the entry has been re-added below.
	}
	else
	{
		// There was no entry in the list previously.  Fake things up by making it look like the previous entry had all properties unmuted.
		localmute.mFlags = LLMute::flagAll;
	}

	if(flags)
	{
		// The user passed some combination of flags.  Make sure those flag bits are turned off (i.e. those properties will be muted).
		localmute.mFlags &= (~flags);
	}
	else
	{
		// The user passed 0.  Make sure all flag bits are turned off (i.e. all properties will be muted).
		localmute.mFlags = 0;
	}
		
	// (re)add the mute entry.
	{			
		auto result = mMutes.insert(localmute);
		if (result.second)
		{
			LL_INFOS() << "Muting " << localmute.mName << " id " << localmute.mID << " flags " << localmute.mFlags << LL_ENDL;
			updateAdd(localmute);
			notifyObservers();
			notifyObserversDetailed(localmute);
			if(!(localmute.mFlags & LLMute::flagParticles))
			{
				//Kill all particle systems owned by muted task
				if(localmute.mType == LLMute::AGENT || localmute.mType == LLMute::OBJECT)
				{
					LLViewerPartSim::getInstance()->clearParticlesByOwnerID(localmute.mID);
				}
			}
			//mute local lights that are attached to the avatar
			LLVOAvatar *avatarp = find_avatar(localmute.mID);
			if (avatarp)
			{
				LLPipeline::removeMutedAVsLights(avatarp);
			}
			return !duplicate;
		}
	}
	
	// If we were going to return success, we'd have done it by now.
	return FALSE;
}

void LLMuteList::updateAdd(const LLMute& mute)
{
	// External mutes (e.g. Avaline callers) are local only, don't send them to the server.
	if (mute.mType == LLMute::EXTERNAL)
	{
		return;
	}

	// Update the database
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_UpdateMuteListEntry);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_MuteData);
	msg->addUUIDFast(_PREHASH_MuteID, mute.mID);
	msg->addStringFast(_PREHASH_MuteName, mute.mName);
	msg->addS32("MuteType", mute.mType);
	msg->addU32("MuteFlags", mute.mFlags);
	gAgent.sendReliableMessage();

	mIsLoaded = TRUE; // why is this here? -MG
}


BOOL LLMuteList::remove(const LLMute& mute, U32 flags)
{
	BOOL found = FALSE;
	
	// First, remove from main list.
	mute_set_t::iterator it = mMutes.find(mute);
	if (it != mMutes.end())
	{
		LLMute localmute = *it;
		bool remove = true;
		if(flags)
		{
			// If the user passed mute flags, we may only want to turn some flags on.
			localmute.mFlags |= flags;
			
			if(localmute.mFlags == LLMute::flagAll)
			{
				// Every currently available mute property has been masked out.
				// Remove the mute entry entirely.
			}
			else
			{
				// Only some of the properties are masked out.  Update the entry.
				remove = false;
			}
		}
		else
		{
			// The caller didn't pass any flags -- just remove the mute entry entirely.
		}
		
		// Always remove the entry from the set -- it will be re-added with new flags if necessary.
		mMutes.erase(it);

		if(remove)
		{
			// The entry was actually removed.  Notify the server.
			updateRemove(localmute);
			LL_INFOS() << "Unmuting " << localmute.mName << " id " << localmute.mID << " flags " << localmute.mFlags << LL_ENDL;
		}
		else
		{
			// Flags were updated, the mute entry needs to be retransmitted to the server and re-added to the list.
			mMutes.insert(localmute);
			updateAdd(localmute);
			LL_INFOS() << "Updating mute entry " << localmute.mName << " id " << localmute.mID << " flags " << localmute.mFlags << LL_ENDL;
		}
		
		// Must be after erase.
		notifyObserversDetailed(localmute);
		setLoaded();  // why is this here? -MG
	}
	else
	{
		// Clean up any legacy mutes
		string_set_t::iterator legacy_it = mLegacyMutes.find(mute.mName);
		if (legacy_it != mLegacyMutes.end())
		{
			// Database representation of legacy mute is UUID null.
			LLMute mute(LLUUID::null, *legacy_it, LLMute::BY_NAME);
			updateRemove(mute);
			mLegacyMutes.erase(legacy_it);
			// Must be after erase.
			notifyObserversDetailed(mute);
			setLoaded(); // why is this here? -MG
		}
	}
	
	return found;
}


void LLMuteList::updateRemove(const LLMute& mute)
{
	// External mutes are not sent to the server anyway, no need to remove them.
	if (mute.mType == LLMute::EXTERNAL)
	{
		return;
	}

	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_RemoveMuteListEntry);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_MuteData);
	msg->addUUIDFast(_PREHASH_MuteID, mute.mID);
	msg->addString("MuteName", mute.mName);
	gAgent.sendReliableMessage();
}

static void notify_automute_callback(const LLUUID& agent_id, const LLMuteList::EAutoReason& reason)
{
	std::string notif_name;
	switch (reason)
	{
	default:
	case LLMuteList::AR_IM:
		notif_name = "AutoUnmuteByIM";
		break;
	case LLMuteList::AR_INVENTORY:
		notif_name = "AutoUnmuteByInventory";
		break;
	case LLMuteList::AR_MONEY:
		notif_name = "AutoUnmuteByMoney";
		break;
	}

	if (auto notif_ptr = LLNotifications::instance().add(notif_name, LLSD().with("NAME", LLAvatarActions::getSLURL(agent_id)), LLSD()))
	{
		std::string message = notif_ptr->getMessage();

		if (reason == LLMuteList::AR_IM)
		{
			if (LLFloaterIMPanel* timp = gIMMgr->findFloaterBySession(agent_id))
			{
				timp->addHistoryLine(message);
			}
		}

		LLFloaterChat::addChat(message, FALSE, FALSE);
	}
}


BOOL LLMuteList::autoRemove(const LLUUID& agent_id, const EAutoReason reason)
{
	BOOL removed = FALSE;

	if (isMuted(agent_id) && !(reason == AR_INVENTORY && gSavedPerAccountSettings.getBOOL("AutoresponseMutedItem")))
	{
		LLMute automute(agent_id, LLStringUtil::null, LLMute::AGENT);
		removed = TRUE;
		remove(automute);

		notify_automute_callback(agent_id, reason);
	}

	return removed;
}


std::vector<LLMute> LLMuteList::getMutes() const
{
	std::vector<LLMute> mutes;
	
	for (const auto& mMute : mMutes)
	{
		mutes.push_back(mMute);
	}
	
	for (const auto& mLegacyMute : mLegacyMutes)
	{
		LLMute legacy(LLUUID::null, mLegacyMute);
		mutes.push_back(legacy);
	}
	
	std::sort(mutes.begin(), mutes.end(), compare_by_name());
	return mutes;
}

//-----------------------------------------------------------------------------
// loadFromFile()
//-----------------------------------------------------------------------------
BOOL LLMuteList::loadFromFile(const std::string& filename)
{
	if(filename.empty())
	{
		LL_WARNS() << "Mute List Filename is Empty!" << LL_ENDL;
		return FALSE;
	}

	LLFILE* fp = LLFile::fopen(filename, "rb");		/*Flawfinder: ignore*/
	if (!fp)
	{
		LL_WARNS() << "Couldn't open mute list " << filename << LL_ENDL;
		return FALSE;
	}

	// *NOTE: Changing the size of these buffers will require changes
	// in the scanf below.
	char id_buffer[MAX_STRING];		/*Flawfinder: ignore*/
	char name_buffer[MAX_STRING];		/*Flawfinder: ignore*/
	char buffer[MAX_STRING];		/*Flawfinder: ignore*/
	while (!feof(fp) 
		   && fgets(buffer, MAX_STRING, fp))
	{
		id_buffer[0] = '\0';
		name_buffer[0] = '\0';
		S32 type = 0;
		U32 flags = 0;
		sscanf(	/* Flawfinder: ignore */
			buffer, " %d %254s %254[^|]| %u\n", &type, id_buffer, name_buffer,
			&flags);
		LLUUID id = LLUUID(id_buffer);
		LLMute mute(id, std::string(name_buffer), (LLMute::EType)type, flags);
		if (mute.mID.isNull()
			|| mute.mType == LLMute::BY_NAME)
		{
			mLegacyMutes.insert(mute.mName);
		}
		else
		{
			mMutes.insert(mute);
		}
	}
	fclose(fp);
	setLoaded();
	return TRUE;
}

//-----------------------------------------------------------------------------
// saveToFile()
//-----------------------------------------------------------------------------
BOOL LLMuteList::saveToFile(const std::string& filename)
{
	if(filename.empty())
	{
		LL_WARNS() << "Mute List Filename is Empty!" << LL_ENDL;
		return FALSE;
	}

	LLFILE* fp = LLFile::fopen(filename, "wb");		/*Flawfinder: ignore*/
	if (!fp)
	{
		LL_WARNS() << "Couldn't open mute list " << filename << LL_ENDL;
		return FALSE;
	}
	// legacy mutes have null uuid
	std::string id_string;
	LLUUID::null.toString(id_string);
	for (const auto& mLegacyMute : mLegacyMutes)
	{
		fprintf(fp, "%d %s %s|\n", (S32)LLMute::BY_NAME, id_string.c_str(), mLegacyMute.c_str());
	}
	for (const auto& mMute : mMutes)
	{
		// Don't save external mutes as they are not sent to the server and probably won't
		//be valid next time anyway.
		if (mMute.mType != LLMute::EXTERNAL)
		{
            mMute.mID.toString(id_string);
			const std::string& name = mMute.mName;
			fprintf(fp, "%d %s %s|%u\n", (S32)mMute.mType, id_string.c_str(), name.c_str(), mMute.mFlags);
		}
	}
	fclose(fp);
	return TRUE;
}


BOOL LLMuteList::isMuted(const LLUUID& id, const std::string& name, U32 flags) const
{
	if (mMutes.empty() && mLegacyMutes.empty())
		return FALSE;

	// for objects, check for muting on their parent prim
	LLViewerObject* mute_object = get_object_to_mute_from_id(id);
	LLUUID id_to_check  = (mute_object) ? mute_object->getID() : id;

	if (id_to_check == gAgentID) return false; // Can't mute self.

	// don't need name or type for lookup
	LLMute mute(id_to_check);
	mute_set_t::const_iterator mute_it = mMutes.find(mute);
	if (mute_it != mMutes.end())
	{
		// If any of the flags the caller passed are set, this item isn't considered muted for this caller.
		if(flags & mute_it->mFlags)
		{
			return FALSE;
		}
		return TRUE;
	}

	// empty names can't be legacy-muted
	bool avatar = mute_object && mute_object->isAvatar();
	if (name.empty() || avatar) return FALSE;

	// Look in legacy pile
	string_set_t::const_iterator legacy_it = mLegacyMutes.find(name);
	return legacy_it != mLegacyMutes.end();
}

//-----------------------------------------------------------------------------
// requestFromServer()
//-----------------------------------------------------------------------------
void LLMuteList::requestFromServer(const LLUUID& agent_id)
{
    std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,
        llformat("%s.cached_mute", agent_id.asString().c_str()));
	LLCRC crc;
	crc.update(filename);

	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_MuteListRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, agent_id);
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_MuteData);
	msg->addU32Fast(_PREHASH_MuteCRC, crc.getCRC());
	gAgent.sendReliableMessage();
}

//-----------------------------------------------------------------------------
// cache()
//-----------------------------------------------------------------------------

void LLMuteList::cache(const LLUUID& agent_id)
{
	// Write to disk even if empty.
	if(mIsLoaded)
	{
		std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,
            llformat("%s.cached_mute", agent_id.asString().c_str()));
		saveToFile(filename);
	}
}

//-----------------------------------------------------------------------------
// Static message handlers
//-----------------------------------------------------------------------------

void LLMuteList::processMuteListUpdate(LLMessageSystem* msg, void**)
{
	LL_INFOS() << "LLMuteList::processMuteListUpdate()" << LL_ENDL;
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_MuteData, _PREHASH_AgentID, agent_id);
	if(agent_id != gAgent.getID())
	{
		LL_WARNS() << "Got an mute list update for the wrong agent." << LL_ENDL;
		return;
	}
	std::string unclean_filename;
	msg->getStringFast(_PREHASH_MuteData, _PREHASH_Filename, unclean_filename);
	std::string filename = LLDir::getScrubbedFileName(unclean_filename);
	
	std::string *local_filename_and_path = new std::string(gDirUtilp->getExpandedFilename( LL_PATH_CACHE, filename ));
	gXferManager->requestFile(*local_filename_and_path,
							  filename,
							  LL_PATH_CACHE,
							  msg->getSender(),
							  TRUE, // make the remote file temporary.
							  onFileMuteList,
							  (void**)local_filename_and_path,
							  LLXferManager::HIGH_PRIORITY);
}

void LLMuteList::processUseCachedMuteList(LLMessageSystem* msg, void**)
{
	LL_INFOS() << "LLMuteList::processUseCachedMuteList()" << LL_ENDL;

    std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,
        llformat("%s.cached_mute", gAgent.getID().asString().c_str()));
	LLMuteList::getInstance()->loadFromFile(filename);
}

void LLMuteList::onFileMuteList(void** user_data, S32 error_code, LLExtStat ext_status)
{
	LL_INFOS() << "LLMuteList::processMuteListFile()" << LL_ENDL;

	std::string* local_filename_and_path = (std::string*)user_data;
	if(local_filename_and_path && !local_filename_and_path->empty() && (error_code == 0))
	{
		LLMuteList::getInstance()->loadFromFile(*local_filename_and_path);
		LLFile::remove(*local_filename_and_path);
	}
	delete local_filename_and_path;
}

void LLMuteList::addObserver(LLMuteListObserver* observer)
{
	mObservers.insert(observer);
}

void LLMuteList::removeObserver(LLMuteListObserver* observer)
{
	mObservers.erase(observer);
}

void LLMuteList::setLoaded()
{
	mIsLoaded = TRUE;
	notifyObservers();
}

void LLMuteList::notifyObservers()
{
	for (observer_set_t::iterator it = mObservers.begin();
		it != mObservers.end();
		)
	{
		LLMuteListObserver* observer = *it;
		observer->onChange();
		// In case onChange() deleted an entry.
		it = mObservers.upper_bound(observer);
	}
}

void LLMuteList::notifyObserversDetailed(const LLMute& mute)
{
	for (observer_set_t::iterator it = mObservers.begin();
		it != mObservers.end();
		)
	{
		LLMuteListObserver* observer = *it;
		observer->onChangeDetailed(mute);
		// In case onChange() deleted an entry.
		it = mObservers.upper_bound(observer);
	}
}
