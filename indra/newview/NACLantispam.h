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

#pragma once
#include <boost/signals2/connection.hpp>
#include <boost/unordered/unordered_map.hpp>
#include "stdtypes.h"
#include "lfidbearer.h"
#include "lluuid.h"

class NACLAntiSpamQueue;
class NACLAntiSpamRegistry final : public LLSingleton<NACLAntiSpamRegistry>
{
	friend class LLSingleton<NACLAntiSpamRegistry>;
	NACLAntiSpamRegistry();
public:
	static void startup();

	enum Type : U8 {
		QUEUE_CHAT,
		QUEUE_INVENTORY,
		QUEUE_IM,
		QUEUE_CALLING_CARD,
		QUEUE_SOUND,
		QUEUE_SOUND_PRELOAD,
		QUEUE_SCRIPT_DIALOG,
		QUEUE_TELEPORT,
		QUEUE_MAX
	};
	bool checkQueue(const Type& name, const LLUUID& source, const LFIDBearer::Type& type = LFIDBearer::AVATAR, const U32& multiplier = 1);
	void blockOnQueue(const Type& name, const LLUUID& owner_id);
	void idle();
	void resetQueues();
private:
	void setAllQueueTimes(U32 amount);
	void setAllQueueAmounts(U32 time);
	void purgeAllQueues();
	void initializeQueues(bool global, const U32& time, const U32& amount);
	static bool handleNaclAntiSpamGlobalQueueChanged(const LLSD& newvalue);
	static bool handleNaclAntiSpamTimeChanged(const LLSD& newvalue);
	static bool handleNaclAntiSpamAmountChanged(const LLSD& newvalue);
	std::unique_ptr<std::array<NACLAntiSpamQueue, QUEUE_MAX>> mQueues;
	std::unique_ptr<NACLAntiSpamQueue> mGlobalQueue;
	std::array<boost::signals2::scoped_connection, 3> mConnections;
};
