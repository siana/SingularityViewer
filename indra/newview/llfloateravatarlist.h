//
// C++ Interface: llfloateravatarlist
//
// Description: 
//
//
// Original author: Dale Glass <dale@daleglass.net>, (C) 2007
// Heavily modified by Henri Beauchamp 10/2009.
//
// Copyright: See COPYING file that comes with this distribution
//
//
#include "llavatarname.h"
#include "llavatarpropertiesprocessor.h"
#include "llfloater.h"
#include "llfloaterreporter.h"
#include "lluuid.h"
#include "lltimer.h"
#include "llscrolllistctrl.h"

#include <time.h>
#include <bitset>
#include <map>
#include <set>

#include <boost/shared_ptr.hpp>

class LLFloaterAvatarList;

enum ERadarStatType
{
	STAT_TYPE_SIM,
	STAT_TYPE_DRAW,
	STAT_TYPE_SHOUTRANGE,
	STAT_TYPE_CHATRANGE,
	STAT_TYPE_AGE,
	STAT_TYPE_SIZE
};

/**
 * @brief This class is used to hold data about avatars.
 * We cache data about avatars to avoid repeating requests in this class.
 * Instances are kept in a map<LLAvatarListEntry>. We keep track of the
 * frame where the avatar was last seen.
 */
class LLAvatarListEntry : public LLAvatarPropertiesObserver
{

public:

enum ACTIVITY_TYPE
{
	ACTIVITY_NONE,           /** Avatar not doing anything */ 
	ACTIVITY_MOVING,         /** Changing position */
	ACTIVITY_GESTURING,	 /** Playing a gesture */
	ACTIVITY_REZZING,        /** Rezzing objects */
	ACTIVITY_PARTICLES,      /** Creating particles */
	ACTIVITY_TYPING,         /** Typing */
	ACTIVITY_NEW,            /** Avatar just appeared */
	ACTIVITY_SOUND,          /** Playing a sound */
	ACTIVITY_DEAD            /** Avatar isn't around anymore, and will be removed soon from the list */
};
	/**
	 * @brief Initializes a list entry
	 * @param id Avatar's key
	 * @param name Avatar's name
	 * @param position Avatar's current position
	 */
	LLAvatarListEntry(const LLUUID& id = LLUUID::null, const std::string &name = "", const LLVector3d &position = LLVector3d::zero);
	~LLAvatarListEntry();

	// Get properties, such as age and other niceties displayed on profiles.
	/*virtual*/ void processProperties(void* data, EAvatarProcessorType type);

	/**
	 * Update world position.
	 * Affects age.
	 */	
	void setPosition(LLVector3d position, bool this_sim, bool drawn, bool chatrange, bool shoutrange);

	const LLVector3d& getPosition() const { return mPosition; }

	/**
	 * @brief Returns the age of this entry in frames
	 *
	 * This is only used for determining whether the avatar is still around.
	 * @see getEntryAgeSeconds
	 */
	bool getAlive();

	/**
	 * @brief Returns the age of this entry in seconds
	 */
	F32 getEntryAgeSeconds() const;

	/**
	 * @brief Returns the name of the avatar
	 */
	const std::string&  getName() const { return mName; }
	const time_t& getTime() const { return mTime; }

	/**
	 * @brief Returns the ID of the avatar
	 */
	const LLUUID& getID() const { return mID; }

	void setActivity(ACTIVITY_TYPE activity);

	/**
	 * @brief Returns the activity type
	 */
	const ACTIVITY_TYPE getActivity();

	/**
	 * @brief Sets the 'focus' status on this entry (camera focused on this avatar)
	 */
	void setFocus(BOOL value) { mFocused = value; }

	bool isFocused() const { return mFocused; }

	bool isMarked() const { return mMarked; }

	/**
	 * @brief 'InList' signifies that the entry has been displayed in the floaters avatar list
	 *  Until this happens our focusprev/focusnext logic should ignore this entry.
	 */

	void setInList()	{ mIsInList = true; }

	bool isInList() const { return mIsInList; }
	/**
	 * @brief Returns whether the item is dead and shouldn't appear in the list
	 * @returns TRUE if dead
	 */
	bool isDead() const;

	void toggleMark() { mMarked = !mMarked; }

	struct uuidMatch
	{
		uuidMatch(const LLUUID& id) : mID(id) {}
		bool operator()(const boost::shared_ptr<LLAvatarListEntry>& l) { return l->getID() == mID; }
		LLUUID mID;
	};

private:
	friend class LLFloaterAvatarList;

	LLUUID mID;
	std::string mName;
	time_t mTime;
	LLVector3d mPosition;
	bool mMarked;
	bool mFocused;
	bool mIsInList;
	int mAge;

	/**
	 * @brief Bitset to keep track of what stats still hold true about the avatar
	 */
	std::bitset<STAT_TYPE_SIZE> mStats;

	/**
	 * @brief Timer to keep track of whether avatars are still there
	 */

	LLTimer mUpdateTimer;

	ACTIVITY_TYPE mActivityType;

	LLTimer mActivityTimer;

	/**
	 * @brief Last frame when this avatar was updated
	 */
	U32 mFrame;
};


/**
 * @brief Avatar List
 * Implements an avatar scanner in the client.
 *
 * This is my first attempt to modify the SL source. This code is intended
 * to have a dual purpose: doing the task, and providing an example of how
 * to do it. For that reason, it's going to be commented as exhaustively
 * as possible.
 *
 * Since I'm very new to C++ any suggestions on coding, style, etc are very
 * welcome.
 */
class LLFloaterAvatarList : public LLFloater, public LLSingleton<LLFloaterAvatarList>
{
	/**
	 * @brief Creates and initializes the LLFloaterAvatarList
	 * Here the interface is created, and callbacks are initialized.
	 */
	friend class LLSingleton<LLFloaterAvatarList>;
private:
	LLFloaterAvatarList();
public:
	~LLFloaterAvatarList();

	virtual BOOL	handleKeyHere(KEY key, MASK mask);

	/*virtual*/ void onClose(bool app_quitting);
	/*virtual*/ void onOpen();
	/*virtual*/ BOOL postBuild();
	/*virtual*/ void draw();
	static void createInstance(bool visible);
	/**
	 * @brief Toggles interface visibility
	 * There is only one instance of the avatar scanner at any time.
	 */
	static void toggle(void*);

	static void showInstance();

	// Decides which user-chosen columns to show and hide.
	void assessColumns();

	/**
	 * @brief Updates the internal avatar list with the currently present avatars.
	 */
	void updateAvatarList();

	/**
	 * @brief Refresh avatar list (display)
	 */
	void refreshAvatarList();

	/**
	 * @brief Returns the entry for an avatar, if preset
	 * @returns Pointer to avatar entry, NULL if not found.
	 */
	LLAvatarListEntry* getAvatarEntry(LLUUID avatar);

	/**
	 * @brief Returns a string with the selected names in the list
	 */
	std::string getSelectedNames(const std::string& separator = ", ");
	std::string getSelectedName();
	LLUUID getSelectedID();

	static void lookAtAvatar(LLUUID &uuid);

	static void sound_trigger_hook(LLMessageSystem* msg,void **);
	void sendKeys();

	typedef boost::shared_ptr<LLAvatarListEntry> LLAvatarListEntryPtr;
	typedef std::vector< LLAvatarListEntryPtr > av_list_t;

	// when a line editor loses keyboard focus, it is committed.
	// commit callbacks are named onCommitWidgetName by convention.
	//void onCommitBaz(LLUICtrl* ctrl, void *userdata);
	
	enum AVATARS_COLUMN_ORDER
	{
		LIST_MARK,
		LIST_AVATAR_NAME,
		LIST_DISTANCE,
		LIST_POSITION,
		LIST_ALTITUDE,
		LIST_ACTIVITY,
		LIST_AGE,
		LIST_TIME,
		LIST_CLIENT,
	};

	typedef boost::function<void (LLAvatarListEntry*)> avlist_command_t;

	/**
	 * @brief Removes focus status from all avatars in list
	 */
	void removeFocusFromAll();

	/**
	 * @brief Focus camera on specified avatar
	 */
	void setFocusAvatar(const LLUUID& id);

	/**
	 * @brief Focus camera on previous avatar
	 * @param marked_only Whether to choose only marked avatars
	 */
	void focusOnPrev(BOOL marked_only);

	/**
	 * @brief Focus camera on next avatar
	 * @param marked_only Whether to choose only marked avatars
	 */
	void focusOnNext(BOOL marked_only);

	void refreshTracker();
	void trackAvatar(const LLAvatarListEntry* entry);

	/**
	 * @brief Handler for the "refresh" button click.
	 * I am unsure whether this is actually necessary at the time.
	 *
	 * LL: By convention, button callbacks are named onClickButtonLabel
	 * @param userdata Pointer to user data (LLFloaterAvatarList instance)
	 */

	void onClickIM();
	void onClickTeleportOffer();
	void onClickTrack();
	void onClickMute();
	void onClickFocus();
	void onClickGetKey();

	void onSelectName();
	void onCommitUpdateRate();

	/**
	 * @brief These callbacks fire off notifications, which THEN fire the related callback* functions.
	 */
	void onClickFreeze();
	void onClickEject();
	void onClickEjectFromEstate();
	void onClickBanFromEstate();
	void onAvatarSortingChanged();

	/**
	 * @brief Called via notification feedback.
	 */
	static void callbackFreeze(const LLSD& notification, const LLSD& response);
	static void callbackEject(const LLSD& notification, const LLSD& response);
	static void callbackEjectFromEstate(const LLSD& notification, const LLSD& response);
	static void callbackBanFromEstate(const LLSD& notification, const LLSD& response);

	static bool onConfirmRadarChatKeys(const LLSD& notification, const LLSD& response );

	static void callbackIdle(void *userdata);

	void doCommand(avlist_command_t cmd, bool single = false);

	/**
	 * @brief Cleanup avatar list, removing dead entries from it.
	 * This lets dead entries remain for some time. This makes it possible
	 * to keep people passing by in the list long enough that it's possible
	 * to do something to them.
	 */
	void expireAvatarList();
	void updateAvatarSorting();

private:
	/**
	 * @brief Pointer to the avatar scroll list
	 */
	LLScrollListCtrl*			mAvatarList;
	av_list_t	mAvatars;
	bool		mDirtyAvatarSorting;

	/**
	 * @brief TRUE when Updating
	 */
	const LLCachedControl<bool> mUpdate;

	/**
	 * @brief Update rate (if min frames per update)
	 */
	U32 mUpdateRate;

	// tracking data
	bool mTracking;			// Tracking ?
	LLUUID mTrackedAvatar;		// Who we are tracking

	/**
	 * @brief Avatar the camera is focused on
	 */
	LLUUID mFocusedAvatar;
};
