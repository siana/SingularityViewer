/* DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 *                   Version 2, December 2004
 *
 * Copyright (C) 2004 Sam Hocevar <sam@hocevar.net>
 *
 * Everyone is permitted to copy and distribute verbatim or modified
 * copies of this license document, and changing it is allowed as long
 * as the name is changed.
 *
 *            DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 *   TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION
 *
 *  0. You just DO WHAT THE FUCK YOU WANT TO.
 */

#include "llviewerprecompiledheaders.h"

#include "NACLantispam.h"
#include "llagent.h"
#include "llavataractions.h"
#include "llcallbacklist.h" // For idle cleaning
#include "llnotificationsutil.h"
#include "lltrans.h"
#include "llviewerobjectlist.h"

#include <time.h>
#include <boost/lexical_cast.hpp>

class NACLAntiSpamQueue
{
	friend class NACLAntiSpamRegistry;
public:
	const U32& getAmount() const { return mAmount; }
	const U32& getTime() const { return mTime; }
protected:
	NACLAntiSpamQueue(const U32& time, const U32& amount) : mTime(time), mAmount(amount) {}
	void setAmount(const U32& amount) { mAmount = amount; }
	void setTime(const U32& time) { mTime = time; }
	void block(const LLUUID& source) { mEntries[source.asString()].block(); }
	void reset() { mEntries.clear(); }
	// Returns 0 if unblocked/disabled, 1 if check results in a new block, 2 if by an existing block
	U8 check(const LLUUID& source, const U32& multiplier)
	{
		const auto key = source.asString();
		auto it = mEntries.find(key);
		if (it != mEntries.end())
			return it->second.blockIfNeeded(mAmount * multiplier, mTime);
		mEntries[key]; // Default construct an Entry
		return 0U;
	}
	void idle()
	{
		// Clean out old unblocked entries
		const auto time_limit = mTime + 1; // One second after time has gone up, the next offense would reset anyway
		for (auto it = mEntries.begin(); it != mEntries.end();)
		{
			const auto& entry = it->second;
			if (entry.getBlocked() || entry.withinBlockTime(time_limit))
				++it;
			else it = mEntries.erase(it);
		}
	}

private:
	class Entry
	{
		friend class NACLAntiSpamQueue;
	protected:
		void reset() { updateTime(); mAmount = 1; mBlocked = false; }
		const U32& getAmount() const { return mAmount; }
		const std::time_t& getTime() const { return mTime; }
		void updateTime() { mTime = time(nullptr); }
		void block() { mBlocked = true; }
		bool withinBlockTime(const U32& time_limit) const { return (time(nullptr) - mTime) <= time_limit; }
		U8 blockIfNeeded(const U32& amount, const U32& time_limit)
		{
			if (mBlocked) return 2U; // Already blocked
			if (withinBlockTime(time_limit))
			{
				if (++mAmount > amount)
				{
					block();
					return 1U;
				}
			}
			else reset(); // Enough time has passed to forgive or not already in list
			return 0U;
		}
		bool getBlocked() const { return mBlocked; }
	private:
		U32 mAmount = 1;
		std::time_t mTime = time(nullptr);
		bool mBlocked = false;
	};
	boost::unordered_map<std::string, Entry> mEntries;
	U32 mAmount, mTime;
};

bool can_block(const LLUUID& id)
{
	if (id.isNull() || gAgentID == id) return false; // Can't block system or self.
	if (const LLViewerObject* obj = gObjectList.findObject(id)) // From an object,
		return !obj->permYouOwner(); // not own object.
	return true;
}

bool is_collision_sound(const std::string& sound)
{
	// The following sounds will be ignored for purposes of spam protection. They have been gathered from wiki documentation of frequent official sounds.
	const std::array<const std::string, 29> COLLISION_SOUNDS = {
		"dce5fdd4-afe4-4ea1-822f-dd52cac46b08",
		"51011582-fbca-4580-ae9e-1a5593f094ec",
		"68d62208-e257-4d0c-bbe2-20c9ea9760bb",
		"75872e8c-bc39-451b-9b0b-042d7ba36cba",
		"6a45ba0b-5775-4ea8-8513-26008a17f873",
		"992a6d1b-8c77-40e0-9495-4098ce539694",
		"2de4da5a-faf8-46be-bac6-c4d74f1e5767",
		"6e3fb0f7-6d9c-42ca-b86b-1122ff562d7d",
		"14209133-4961-4acc-9649-53fc38ee1667",
		"bc4a4348-cfcc-4e5e-908e-8a52a8915fe6",
		"9e5c1297-6eed-40c0-825a-d9bcd86e3193",
		"e534761c-1894-4b61-b20c-658a6fb68157",
		"8761f73f-6cf9-4186-8aaa-0948ed002db1",
		"874a26fd-142f-4173-8c5b-890cd846c74d",
		"0e24a717-b97e-4b77-9c94-b59a5a88b2da",
		"75cf3ade-9a5b-4c4d-bb35-f9799bda7fb2",
		"153c8bf7-fb89-4d89-b263-47e58b1b4774",
		"55c3e0ce-275a-46fa-82ff-e0465f5e8703",
		"24babf58-7156-4841-9a3f-761bdbb8e237",
		"aca261d8-e145-4610-9e20-9eff990f2c12",
		"0642fba6-5dcf-4d62-8e7b-94dbb529d117",
		"25a863e8-dc42-4e8a-a357-e76422ace9b5",
		"9538f37c-456e-4047-81be-6435045608d4",
		"8c0f84c3-9afd-4396-b5f5-9bca2c911c20",
		"be582e5d-b123-41a2-a150-454c39e961c8",
		"c70141d4-ba06-41ea-bcbc-35ea81cb8335",
		"7d1826f4-24c4-4aac-8c2e-eff45df37783",
		"063c97d3-033a-4e9b-98d8-05c8074922cb",
		"00000000-0000-0000-0000-000000000120"
	};

	for (const auto& collision : COLLISION_SOUNDS)
		if (collision == sound)
			return true;
	return false;
}
// NaClAntiSpamRegistry

constexpr std::array<const char*, NACLAntiSpamRegistry::QUEUE_MAX> QUEUE_NAME = {
	"Chat",
	"Inventory",
	"Instant Message",
	"calling card",
	"sound",
	"Sound Preload",
	"Script Dialog",
	"Teleport"
};

NACLAntiSpamRegistry::NACLAntiSpamRegistry()
{
	auto control = gSavedSettings.getControl("_NACL_AntiSpamTime");
	const U32 time = control->get().asInteger();
	mConnections[0] = control->getSignal()->connect(boost::bind(&NACLAntiSpamRegistry::handleNaclAntiSpamTimeChanged, _2));

	control = gSavedSettings.getControl("_NACL_AntiSpamAmount");
	const U32 amount = control->get().asInteger();
	mConnections[1] = control->getSignal()->connect(boost::bind(&NACLAntiSpamRegistry::handleNaclAntiSpamAmountChanged, _2));

	control = gSavedSettings.getControl("_NACL_AntiSpamGlobalQueue");
	mConnections[2] = control->getSignal()->connect(boost::bind(&NACLAntiSpamRegistry::handleNaclAntiSpamGlobalQueueChanged, _2));
	initializeQueues(control->get(), time, amount);
}

void NACLAntiSpamRegistry::initializeQueues(bool global, const U32& time, const U32& amount)
{
	if (global) // If Global, initialize global queue
		mGlobalQueue.reset(new NACLAntiSpamQueue(time, amount));
	else
	{
		mQueues.reset(new std::array<NACLAntiSpamQueue, QUEUE_MAX>{{
			NACLAntiSpamQueue(time, amount), // QUEUE_CHAT
			NACLAntiSpamQueue(time, amount), // QUEUE_INVENTORY
			NACLAntiSpamQueue(time, amount), // QUEUE_IM
			NACLAntiSpamQueue(time, amount), // QUEUE_CALLING_CARD
			NACLAntiSpamQueue(time, amount), // QUEUE_SOUND
			NACLAntiSpamQueue(time, amount), // QUEUE_SOUND_PRELOAD
			NACLAntiSpamQueue(time, amount), // QUEUE_SCRIPT_DIALOG
			NACLAntiSpamQueue(time, amount)  // QUEUE_TELEPORT
		}});
	}
}

constexpr const char* getQueueName(const NACLAntiSpamRegistry::Type& name)
{
	return name >= QUEUE_NAME.size() ? "Unknown" : QUEUE_NAME[name];
}

void NACLAntiSpamRegistry::setAllQueueTimes(U32 time)
{
	if (mGlobalQueue) mGlobalQueue->setTime(time);
	else for(auto& queue : *mQueues) queue.setTime(time);
}

void NACLAntiSpamRegistry::setAllQueueAmounts(U32 amount)
{
	if (mGlobalQueue) mGlobalQueue->setAmount(amount);
	else for (U8 queue = 0U; queue < QUEUE_MAX; ++queue)
	{
		auto& q = (*mQueues)[queue];
		if (queue == QUEUE_SOUND || queue == QUEUE_SOUND_PRELOAD)
			q.setAmount(amount*5);
		else
			q.setAmount(amount);
	}
}

void NACLAntiSpamRegistry::blockOnQueue(const Type& name, const LLUUID& source)
{
	if (name >= QUEUE_MAX)
		LL_ERRS("AntiSpam") << "CODE BUG: Attempting to use a antispam queue that was outside of the reasonable range of queues. Queue: " << getQueueName(name) << LL_ENDL;
	else (mGlobalQueue ? *mGlobalQueue : (*mQueues)[name]).block(source);
}

bool NACLAntiSpamRegistry::checkQueue(const Type& name, const LLUUID& source, const LFIDBearer::Type& type, const U32& multiplier)
//returns true if blocked
{
	if (name >= QUEUE_MAX)
	{
		LL_ERRS("AntiSpam") << "CODE BUG: Attempting to check antispam queue that was outside of the reasonable range of queues. Queue: " << getQueueName(name) << LL_ENDL;
		return false;
	}

	if (!can_block(source)) return false;

	auto& queue = mGlobalQueue ? *mGlobalQueue : (*mQueues)[name];
	const auto result = queue.check(source, multiplier);
	if (!result) return false; // Safe

	if (result != 2 // Not previously blocked
	&& gSavedSettings.getBOOL("AntiSpamNotify")) // and Just blocked!
	{
		const std::string get_slurl_for(const LLUUID& id, const LFIDBearer::Type& type);
		const auto slurl = get_slurl_for(source, type);
		LLSD args;
		args["SOURCE"] = slurl.empty() ? source.asString() : slurl;
		args["TYPE"] = LLTrans::getString(getQueueName(name));
		args["AMOUNT"] = (LLSD::Integer)(multiplier * queue.getAmount());
		args["TIME"] = (LLSD::Integer)queue.getTime();
		LLNotificationsUtil::add("AntiSpamBlock", args);
	}
	return true;
}

void NACLAntiSpamRegistry::idle()
{
	if (mGlobalQueue) mGlobalQueue->idle();
	else for (auto& queue : *mQueues) queue.idle();
}

void NACLAntiSpamRegistry::resetQueues()
{
	if (mGlobalQueue) mGlobalQueue->reset();
	else for (auto& queue : *mQueues) queue.reset();

	LL_INFOS() << "AntiSpam Queues Reset" << LL_ENDL;
}

void NACLAntiSpamRegistry::purgeAllQueues()
{
	// Note: These resets are upon the unique_ptr, not the Queue itself!
	if (mGlobalQueue)
		mGlobalQueue.reset();
	else
		mQueues.reset();
}

// Handlers
// static
void NACLAntiSpamRegistry::startup()
{
	auto onAntiSpamToggle = [](const LLControlVariable*, const LLSD& value) {
		if (value.asBoolean()) instance();
		else deleteSingleton();
	};
	auto control = gSavedSettings.getControl("AntiSpamEnabled");
	control->getSignal()->connect(onAntiSpamToggle);
	onAntiSpamToggle(control, control->get());
}

// static
bool NACLAntiSpamRegistry::handleNaclAntiSpamGlobalQueueChanged(const LLSD& newvalue)
{
	if (instanceExists())
	{
		auto& inst = instance();
		inst.purgeAllQueues();
		inst.initializeQueues(newvalue.asBoolean(), gSavedSettings.getU32("_NACL_AntiSpamTime"), gSavedSettings.getU32("_NACL_AntiSpamAmount"));
	}
    return true;
}
//static
bool NACLAntiSpamRegistry::handleNaclAntiSpamTimeChanged(const LLSD& newvalue)
{
	if (auto inst = getIfExists()) inst->setAllQueueTimes(newvalue.asInteger());
    return true;
}
//static
bool NACLAntiSpamRegistry::handleNaclAntiSpamAmountChanged(const LLSD& newvalue)
{
	if (auto inst = getIfExists()) inst->setAllQueueAmounts(newvalue.asInteger());
    return true;
}