/*
 * Copyright (c) The Singularity dev Team and Melanie Thielker
 * Refer to the singularity project for a full lst of copyright holders
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Neither the name of the Singularity Viewwer Project nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "llviewerprecompiledheaders.h"
#include "generichandlers.h"
#include "llviewergenericmessage.h"
#include "llviewerinventory.h"
#include "llappearancemgr.h"
#include "llviewerregion.h"
#include "llagent.h"

GenericHandlers *gGenericHandlers = NULL;

class DispatchReplaceOutfit : public LLDispatchHandler
{
public:
	virtual bool operator()(
		const LLDispatcher* dispatcher,
		const std::string& key,
		const LLUUID& invoice,
		const sparam_t& strings)
	{
		if (strings.size() != 1) return false;
		LLUUID folder_id(strings[0]);

		bool success = false;
		LLViewerInventoryCategory *cat = gInventory.getCategory(folder_id);
		if (cat != NULL)
		{
			LLAppearanceMgr::instance().wearInventoryCategory(cat, FALSE, FALSE);
			success = true;
		}

		LLViewerRegion* regionp = gAgent.getRegion();
		if (!regionp) return true;

		std::string url = regionp->getCapability("WearablesLoaded");
		if (url.empty()) return true;

		LLSD data = LLSD(success);

		LLHTTPClient::post(url, data, new LLHTTPClient::ResponderIgnore);

		return true;
	}
};

static DispatchReplaceOutfit sDispatchReplaceOutfit;

GenericHandlers::GenericHandlers()
{
	gGenericDispatcher.addHandler("replaceoutfit", &sDispatchReplaceOutfit);
}

