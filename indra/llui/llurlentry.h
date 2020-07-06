/** 
 * @file llurlentry.h
 * @author Martin Reddy
 * @brief Describes the Url types that can be registered in LLUrlRegistry
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
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

#ifndef LL_LLURLENTRY_H
#define LL_LLURLENTRY_H

#include "lluuid.h"
#include "lluicolor.h"
#include "llstyle.h"

#include "llavatarname.h"
#include "llhost.h" // for resolving parcel name by parcel id

#include <boost/regex.hpp>

class LLAvatarName;

typedef boost::signals2::signal<void (const std::string& url,
									  const std::string& label,
									  const std::string& icon)> LLUrlLabelSignal;
typedef LLUrlLabelSignal::slot_type LLUrlLabelCallback;

///
/// LLUrlEntryBase is the base class of all Url types registered in the 
/// LLUrlRegistry. Each derived classes provides a regular expression
/// to match the Url type (e.g., http://... or secondlife://...) along
/// with an optional icon to display next to instances of the Url in
/// a text display and a XUI file to use for any context menu popup.
/// Functions are also provided to compute an appropriate label and
/// tooltip/status bar text for the Url.
///
/// Some derived classes of LLUrlEntryBase may wish to compute an
/// appropriate label for a Url by asking the server for information.
/// You must therefore provide a callback method, so that you can be
/// notified when an updated label has been received from the server.
/// This label should then be used to replace any previous label
/// that you received from getLabel() for the Url in question.
///
class LLUrlEntryBase
{
public:
	LLUrlEntryBase();
	virtual ~LLUrlEntryBase();
	
	/// Return the regex pattern that matches this Url 
	boost::regex getPattern() const { return mPattern; }

	/// Return the url from a string that matched the regex
	virtual std::string getUrl(const std::string &string) const;

	/// Given a matched Url, return a label for the Url
	virtual std::string getLabel(const std::string &url, const LLUrlLabelCallback &cb) { return url; }

	/// Return port, query and fragment parts for the Url
	virtual std::string getQuery(const std::string &url) const { return ""; }

	/// Return an icon that can be displayed next to Urls of this type
	virtual std::string getIcon(const std::string &url);

	/// Return the style to render the displayed text
	virtual LLStyleSP getStyle() const;

	/// Given a matched Url, return a tooltip string for the hyperlink
	virtual std::string getTooltip(const std::string &string) const { return mTooltip; }

	/// Return the name of a XUI file containing the context menu items
	std::string getMenuName() const { return mMenuName; }

	/// Return the name of a SL location described by this Url, if any
	virtual std::string getLocation(const std::string &url) const { return ""; }

	/// Should this link text be underlined only when mouse is hovered over it?
	virtual bool underlineOnHoverOnly(const std::string &string) const { return true; } // <alchemy/>

	virtual bool isTrusted() const { return false; }

	virtual LLUUID	getID(const std::string &string) const { return LLUUID::null; }

	bool isLinkDisabled() const;

	bool isWikiLinkCorrect(const std::string& url);

	virtual bool isSLURLvalid(const std::string &url) const { return TRUE; };

protected:
	std::string getIDStringFromUrl(const std::string &url) const;
	std::string escapeUrl(const std::string &url) const;
	std::string unescapeUrl(const std::string &url) const;
	std::string getLabelFromWikiLink(const std::string &url) const;
	std::string getUrlFromWikiLink(const std::string &string) const;
	void addObserver(const std::string &id, const std::string &url, const LLUrlLabelCallback &cb); 
	std::string urlToLabelWithGreyQuery(const std::string &url) const;
	std::string urlToGreyQuery(const std::string &url) const;
	virtual void callObservers(const std::string &id, const std::string &label, const std::string& icon);

	typedef struct {
		std::string url;
		LLUrlLabelSignal *signal;
	} LLUrlEntryObserver;

	boost::regex                                   	mPattern;
	std::string                                    	mIcon;
	std::string                                    	mMenuName;
	std::string                                    	mTooltip;
	std::multimap<std::string, LLUrlEntryObserver>	mObservers;
};

///
/// LLUrlEntryHTTP Describes generic http: and https: Urls
///
class LLUrlEntryHTTP final : public LLUrlEntryBase
{
public:
	LLUrlEntryHTTP();
	/*virtual*/ std::string getLabel(const std::string &url, const LLUrlLabelCallback &cb) override;
	/*virtual*/ std::string getUrl(const std::string &string) const override;
	/*virtual*/ std::string getTooltip(const std::string &url) const override;
};

///
/// LLUrlEntryHTTPLabel Describes generic http: and https: Urls with custom labels
///
class LLUrlEntryHTTPLabel final : public LLUrlEntryBase
{
public:
	LLUrlEntryHTTPLabel();
	/*virtual*/ std::string getLabel(const std::string &url, const LLUrlLabelCallback &cb) override;
	/*virtual*/ std::string getTooltip(const std::string &string) const override;
	/*virtual*/ std::string getUrl(const std::string &string) const override;
};

///
/// LLUrlEntryHTTPNoProtocol Describes generic Urls like www.google.com
///
class LLUrlEntryHTTPNoProtocol final : public LLUrlEntryBase
{
public:
	LLUrlEntryHTTPNoProtocol();
	std::string getLabel(const std::string &url, const LLUrlLabelCallback &cb) override;
	std::string getQuery(const std::string &url) const override;
	std::string getUrl(const std::string &string) const override;
	std::string getTooltip(const std::string &url) const override;
};

class LLUrlEntryInvalidSLURL final : public LLUrlEntryBase
{
public:
	LLUrlEntryInvalidSLURL();
	/*virtual*/ std::string getLabel(const std::string &url, const LLUrlLabelCallback &cb) override;
	/*virtual*/ std::string getUrl(const std::string &string) const override;
	/*virtual*/ std::string getTooltip(const std::string &url) const override;

	bool isSLURLvalid(const std::string &url) const override;
};

///
/// LLUrlEntrySLURL Describes http://slurl.com/... Urls
///
class LLUrlEntrySLURL final : public LLUrlEntryBase
{
public:
	LLUrlEntrySLURL();
	/*virtual*/ std::string getLabel(const std::string &url, const LLUrlLabelCallback &cb) override;
	/*virtual*/ std::string getLocation(const std::string &url) const override;
};

///
/// LLUrlEntrySeconlifeURLs Describes *secondlife.com and *lindenlab.com Urls
///
class LLUrlEntrySecondlifeURL : public LLUrlEntryBase
{
public:
	LLUrlEntrySecondlifeURL();
	/*virtual*/ bool isTrusted() const override { return true; }
	/*virtual*/ std::string getUrl(const std::string &string) const override;
	/*virtual*/ std::string getLabel(const std::string &url, const LLUrlLabelCallback &cb) override;
	/*virtual*/ std::string getQuery(const std::string &url) const override;
	/*virtual*/ std::string getTooltip(const std::string &url) const override;
};

///
/// LLUrlEntrySeconlifeURLs Describes *secondlife.com and *lindenlab.com Urls
///
class LLUrlEntrySimpleSecondlifeURL final : public LLUrlEntrySecondlifeURL
{
public:
	LLUrlEntrySimpleSecondlifeURL();
};

///
/// LLUrlEntryAgent Describes a Second Life agent Url, e.g.,
/// secondlife:///app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/about
class LLUrlEntryAgent : public LLUrlEntryBase
{
public:
	LLUrlEntryAgent();
	~LLUrlEntryAgent()
	{
		for(const auto& conn_pair : mAvatarNameCacheConnections)
		{
			if (conn_pair.second.connected())
			{
				conn_pair.second.disconnect();
			}
		}
		mAvatarNameCacheConnections.clear();
	}
	/*virtual*/ std::string getLabel(const std::string &url, const LLUrlLabelCallback &cb) override;
	/*virtual*/ std::string getIcon(const std::string &url) override;
	/*virtual*/ std::string getTooltip(const std::string &string) const override;
	/*virtual*/ LLStyleSP getStyle() const override;
	/*virtual*/ LLUUID	getID(const std::string &string) const override;
	/*virtual*/ bool underlineOnHoverOnly(const std::string &string) const override;
protected:
	/*virtual*/ void callObservers(const std::string &id, const std::string &label, const std::string& icon) override;
private:
	void onAvatarNameCache(const LLUUID& id, const LLAvatarName& av_name);
	using avatar_name_cache_connection_map_t = std::multimap<LLUUID, boost::signals2::connection>;
	avatar_name_cache_connection_map_t mAvatarNameCacheConnections;
};

///
/// LLUrlEntryAgentName Describes a Second Life agent name Url, e.g.,
/// secondlife:///app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/(completename|displayname|username)
/// that displays various forms of user name
/// This is a base class for the various implementations of name display
class LLUrlEntryAgentName : public LLUrlEntryBase, public boost::signals2::trackable
{
public:
	LLUrlEntryAgentName();
	~LLUrlEntryAgentName()
	{
		for (const auto& conn_pair : mAvatarNameCacheConnections)
		{
			if (conn_pair.second.connected())
			{
				conn_pair.second.disconnect();
			}
		}
		mAvatarNameCacheConnections.clear();

	}
	/*virtual*/ std::string getLabel(const std::string &url, const LLUrlLabelCallback &cb) override;
	/*virtual*/ LLStyleSP getStyle() const override;
protected:
	// override this to pull out relevant name fields
	virtual std::string getName(const LLAvatarName& avatar_name) = 0;
private:
	void onAvatarNameCache(const LLUUID& id, const LLAvatarName& av_name);
	using avatar_name_cache_connection_map_t = std::multimap<LLUUID, boost::signals2::connection>;
	avatar_name_cache_connection_map_t mAvatarNameCacheConnections;
};


///
/// LLUrlEntryAgentCompleteName Describes a Second Life agent name Url, e.g.,
/// secondlife:///app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/completename
/// that displays the full display name + user name for an avatar
/// such as "James Linden (james.linden)"
class LLUrlEntryAgentCompleteName final : public LLUrlEntryAgentName
{
public:
	LLUrlEntryAgentCompleteName();
private:
	/*virtual*/ std::string getName(const LLAvatarName& avatar_name) override;
};

class LLUrlEntryAgentLegacyName final : public LLUrlEntryAgentName
{
public:
	LLUrlEntryAgentLegacyName();
private:
	/*virtual*/ std::string getName(const LLAvatarName& avatar_name) override;
};

///
/// LLUrlEntryAgentDisplayName Describes a Second Life agent display name Url, e.g.,
/// secondlife:///app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/displayname
/// that displays the just the display name for an avatar
/// such as "James Linden"
class LLUrlEntryAgentDisplayName final : public LLUrlEntryAgentName
{
public:
	LLUrlEntryAgentDisplayName();
private:
	/*virtual*/ std::string getName(const LLAvatarName& avatar_name) override;
};

///
/// LLUrlEntryAgentUserName Describes a Second Life agent username Url, e.g.,
/// secondlife:///app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/username
/// that displays the just the display name for an avatar
/// such as "james.linden"
class LLUrlEntryAgentUserName final : public LLUrlEntryAgentName
{
public:
	LLUrlEntryAgentUserName();
private:
	/*virtual*/ std::string getName(const LLAvatarName& avatar_name) override;
};

///
/// LLUrlEntryExperienceProfile Describes a Second Life experience profile Url, e.g.,
/// secondlife:///app/experience/0e346d8b-4433-4d66-a6b0-fd37083abc4c/profile
/// that displays the experience name
class LLUrlEntryExperienceProfile final : public LLUrlEntryBase
{
public:
	LLUrlEntryExperienceProfile();
	/*virtual*/ std::string getLabel(const std::string &url, const LLUrlLabelCallback &cb) override;
private:
	void onExperienceDetails(const LLSD& experience_details);
};


///
/// LLUrlEntryGroup Describes a Second Life group Url, e.g.,
/// secondlife:///app/group/00005ff3-4044-c79f-9de8-fb28ae0df991/about
///
class LLUrlEntryGroup final : public LLUrlEntryBase
{
public:
	LLUrlEntryGroup();
	/*virtual*/ std::string getLabel(const std::string &url, const LLUrlLabelCallback &cb) override;
	/*virtual*/ LLStyleSP getStyle() const override;
	/*virtual*/ LLUUID	getID(const std::string &string) const override;
private:
	void onGroupNameReceived(const LLUUID& id, const std::string& name, bool is_group);
};

///
/// LLUrlEntryInventory Describes a Second Life inventory Url, e.g.,
/// secondlife:///app/inventory/0e346d8b-4433-4d66-a6b0-fd37083abc4c/select
///
class LLUrlEntryInventory final : public LLUrlEntryBase
{
public:
	LLUrlEntryInventory();
	/*virtual*/ std::string getLabel(const std::string &url, const LLUrlLabelCallback &cb) override;
private:
};

///
/// LLUrlEntryObjectIM Describes a Second Life inspector for the object Url, e.g.,
/// secondlife:///app/objectim/7bcd7864-da6b-e43f-4486-91d28a28d95b?name=Object&owner=3de548e1-57be-cfea-2b78-83ae3ad95998&slurl=Danger!%20Danger!/200/200/30/&groupowned=1
///
class LLUrlEntryObjectIM final : public LLUrlEntryBase
{
public:
	LLUrlEntryObjectIM();
	/*virtual*/ std::string getLabel(const std::string &url, const LLUrlLabelCallback &cb) override;
	/*virtual*/ std::string getLocation(const std::string &url) const override;
private:
};

///
/// LLUrlEntryParcel Describes a Second Life parcel Url, e.g.,
/// secondlife:///app/parcel/0000060e-4b39-e00b-d0c3-d98b1934e3a8/about
///
class LLUrlEntryParcel final : public LLUrlEntryBase
{
public:
	struct LLParcelData
	{
		LLUUID		parcel_id;
		std::string	name;
		std::string	sim_name;
		F32			global_x;
		F32			global_y;
		F32			global_z;
	};

	LLUrlEntryParcel();
	~LLUrlEntryParcel();
	/*virtual*/ std::string getLabel(const std::string &url, const LLUrlLabelCallback &cb) override;

	// Sends a parcel info request to sim.
	void sendParcelInfoRequest(const LLUUID& parcel_id);

	// Calls observers of certain parcel id providing them with parcel label.
	void onParcelInfoReceived(const std::string &id, const std::string &label);

	// Processes parcel label and triggers notifying observers.
	static void processParcelInfo(const LLParcelData& parcel_data);

	// Next 4 setters are used to update agent and viewer connection information
	// upon events like user login, viewer disconnect and user changing region host.
	// These setters are made public to be accessible from newview and should not be
	// used in other cases.
	static void setAgentID(const LLUUID& id) { sAgentID = id; }
	static void setSessionID(const LLUUID& id) { sSessionID = id; }
	static void setRegionHost(const LLHost& host) { sRegionHost = host; }
	static void setDisconnected(bool disconnected) { sDisconnected = disconnected; }

private:
	static LLUUID						sAgentID;
	static LLUUID						sSessionID;
	static LLHost						sRegionHost;
	static bool							sDisconnected;
	static std::set<LLUrlEntryParcel*>	sParcelInfoObservers;
};

///
/// LLUrlEntryPlace Describes a Second Life location Url, e.g.,
/// secondlife://Ahern/50/50/50
///
class LLUrlEntryPlace final : public LLUrlEntryBase
{
public:
	LLUrlEntryPlace();
	/*virtual*/ std::string getLabel(const std::string &url, const LLUrlLabelCallback &cb) override;
	/*virtual*/ std::string getLocation(const std::string &url) const override;
};

///
/// LLUrlEntryRegion Describes a Second Life location Url, e.g.,
/// secondlife:///app/region/Ahern/128/128/0
///
class LLUrlEntryRegion final : public LLUrlEntryBase
{
public:
	LLUrlEntryRegion();
	/*virtual*/ std::string getLabel(const std::string &url, const LLUrlLabelCallback &cb) override;
	/*virtual*/ std::string getLocation(const std::string &url) const override;
};

///
/// LLUrlEntryTeleport Describes a Second Life teleport Url, e.g.,
/// secondlife:///app/teleport/Ahern/50/50/50/
///
class LLUrlEntryTeleport final : public LLUrlEntryBase
{
public:
	LLUrlEntryTeleport();
	/*virtual*/ std::string getLabel(const std::string &url, const LLUrlLabelCallback &cb) override;
	/*virtual*/ std::string getLocation(const std::string &url) const override;
};

///
/// LLUrlEntrySL Describes a generic SLURL, e.g., a Url that starts
/// with secondlife:// (used as a catch-all for cases not matched above)
///
class LLUrlEntrySL final : public LLUrlEntryBase
{
public:
	LLUrlEntrySL();
	/*virtual*/ std::string getLabel(const std::string &url, const LLUrlLabelCallback &cb) override;
};

///
/// LLUrlEntrySLLabel Describes a generic SLURL, e.g., a Url that starts
/// with secondlife:// with the ability to specify a custom label.
///
class LLUrlEntrySLLabel final : public LLUrlEntryBase
{
public:
	LLUrlEntrySLLabel();
	/*virtual*/ std::string getLabel(const std::string &url, const LLUrlLabelCallback &cb) override;
	/*virtual*/ std::string getUrl(const std::string &string) const override;
	/*virtual*/ std::string getTooltip(const std::string &string) const override;
	/*virtual*/ bool underlineOnHoverOnly(const std::string &string) const override;
};

///
/// LLUrlEntryWorldMap Describes a Second Life worldmap Url, e.g.,
/// secondlife:///app/worldmap/Ahern/50/50/50
///
class LLUrlEntryWorldMap final : public LLUrlEntryBase
{
public:
	LLUrlEntryWorldMap();
	/*virtual*/ std::string getLabel(const std::string &url, const LLUrlLabelCallback &cb) override;
	/*virtual*/ std::string getLocation(const std::string &url) const override;
};

///
/// LLUrlEntryNoLink lets us turn of URL detection with <nolink>...</nolink> tags
///
class LLUrlEntryNoLink final : public LLUrlEntryBase
{
public:
	LLUrlEntryNoLink();
	/*virtual*/ std::string getLabel(const std::string &url, const LLUrlLabelCallback &cb) override;
	/*virtual*/ std::string getUrl(const std::string &string) const override;
	/*virtual*/ LLStyleSP getStyle() const override;
};

///
/// LLUrlEntryIcon describes an icon with <icon>...</icon> tags
///
class LLUrlEntryIcon final : public LLUrlEntryBase
{
public:
	LLUrlEntryIcon();
	/*virtual*/ std::string getUrl(const std::string &string) const override;
	/*virtual*/ std::string getLabel(const std::string &url, const LLUrlLabelCallback &cb) override;
	/*virtual*/ std::string getIcon(const std::string &url) override;
};

///
/// LLUrlEntryEmail Describes a generic mailto: Urls
///
class LLUrlEntryEmail final : public LLUrlEntryBase
{
public:
	LLUrlEntryEmail();
	/*virtual*/ std::string getLabel(const std::string &url, const LLUrlLabelCallback &cb) override;
	/*virtual*/ std::string getUrl(const std::string &string) const override;
};

// <alchemy>
///
/// LLUrlEntryJira describes a Jira Issue
///
class LLUrlEntryJira final : public LLUrlEntryBase
{
public:
	LLUrlEntryJira();
	/*virtual*/ std::string getLabel(const std::string &url, const LLUrlLabelCallback &cb) override;
	/*virtual*/ std::string getTooltip(const std::string &string) const override;
	/*virtual*/ std::string getUrl(const std::string &string) const override;
	
};
// </alchemy>

#endif
