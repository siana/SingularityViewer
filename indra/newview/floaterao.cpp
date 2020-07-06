/**
 * @file llfloaterao.cpp
 * @brief clientside animation overrider floater
 * by Skills Hak & Liru Færs
 */

#include "llviewerprecompiledheaders.h"

#include "floaterao.h"
#include "aosystem.h"

#include "llagent.h"
#include "llcombobox.h"
#include "llfirstuse.h"
#include "llinventorypanel.h"
#include "llmemorystream.h"
#include "llpreviewnotecard.h"
#include "lltexteditor.h"
#include "lluictrlfactory.h"
#include "roles_constants.h"

// Uncomment and use instead if we ever add the chatbar as a command line - MC
void cmdline_printchat(const std::string& message);

class AONotecardCallback final : public LLInventoryCallback
{
public:
	AONotecardCallback(const std::string& filename) : mFileName(filename) {}

	void fire(const LLUUID &inv_item) override
	{
		if (!mFileName.empty())
		{ 
			auto nc = (LLPreviewNotecard*)LLPreview::show(inv_item);
			if (!nc)
			{
				auto item = gInventory.getItem(inv_item);
				open_notecard(item, "Note: " + item->getName(), LLUUID::null, false);
				nc = (LLPreviewNotecard*)LLPreview::find(inv_item);
			}

			if (nc)
			{
				if (LLTextEditor *text = nc->getEditor())
				{
					text->clear();
					text->makePristine();

					std::ifstream file(mFileName.c_str());

					std::string line;
					while (!file.eof())
					{
						getline(file, line);
						text->insertText(line + '\n');
					}
					file.close();

					nc->saveIfNeeded();
				}
			}
		}
	}

private:
	std::string mFileName;
};


LLFloaterAO::LLFloaterAO(const LLSD&) : LLFloater("floater_ao")
, mCombos({})
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_ao.xml", nullptr, false);
}

void LLFloaterAO::onOpen()
{
	LLFirstUse::useAO();
}

LLFloaterAO::~LLFloaterAO()
{
//	LL_INFOS() << "floater destroyed" << LL_ENDL;
}

BOOL LLFloaterAO::postBuild()
{
	gSavedSettings.getControl("AOAdvanced")->getSignal()->connect(boost::bind(&LLFloaterAO::updateLayout, this, _2));

	getChild<LLUICtrl>("reloadcard")->setCommitCallback(boost::bind(&LLFloaterAO::onClickReloadCard, this));
	getChild<LLUICtrl>("opencard")->setCommitCallback(boost::bind(&LLFloaterAO::onClickOpenCard, this));
	getChild<LLUICtrl>("newcard")->setCommitCallback(boost::bind(&LLFloaterAO::onClickNewCard, this));
	getChild<LLUICtrl>("prevstand")->setCommitCallback(boost::bind(&LLFloaterAO::onClickCycleStand, this, false));
	getChild<LLUICtrl>("nextstand")->setCommitCallback(boost::bind(&LLFloaterAO::onClickCycleStand, this, true));
	getChild<LLUICtrl>("standtime")->setCommitCallback(boost::bind(&LLFloaterAO::onSpinnerCommit, this));

	auto ao = AOSystem::getIfExists();
	const auto& cb = boost::bind(&LLFloaterAO::onComboBoxCommit, this, _1);
	const auto& setup_combo = [&](const std::string& name, const AOState& state) {
		if (auto combo = findChild<LLComboBox>(name))
		{
			combo->setCommitCallback(cb);
			LLControlVariablePtr setting = nullptr;
			if (auto aop = ao ? ao->mAOOverrides[state] : nullptr)
				setting = aop->setting;
			else // AO is down or missing override struct(why?), try getting from UI
				setting = combo->getControl(combo->getControlName());
			if (setting) combo->add(setting->get().asStringRef(), ADD_BOTTOM, true);
			mCombos[state] = combo;
		}
	};
	setup_combo("stands", STATE_AGENT_IDLE);
	setup_combo("walks", STATE_AGENT_WALK);
	setup_combo("runs", STATE_AGENT_RUN);
	setup_combo("prejumps", STATE_AGENT_PRE_JUMP);
	setup_combo("jumps", STATE_AGENT_JUMP);
	setup_combo("turnlefts", STATE_AGENT_TURNLEFT);
	setup_combo("turnrights", STATE_AGENT_TURNRIGHT);
	setup_combo("sits", STATE_AGENT_SIT);
	setup_combo("gsits", STATE_AGENT_SIT_GROUND);
	setup_combo("hovers", STATE_AGENT_HOVER);
	setup_combo("flydowns", STATE_AGENT_HOVER_DOWN);
	setup_combo("flyups", STATE_AGENT_HOVER_UP);
	setup_combo("crouchs", STATE_AGENT_CROUCH);
	setup_combo("cwalks", STATE_AGENT_CROUCHWALK);
	setup_combo("falls", STATE_AGENT_FALLDOWN);
	setup_combo("standups", STATE_AGENT_STANDUP);
	setup_combo("lands", STATE_AGENT_LAND);
	setup_combo("flys", STATE_AGENT_FLY);
	setup_combo("flyslows", STATE_AGENT_FLYSLOW);
	setup_combo("typings", STATE_AGENT_TYPE);
	setup_combo("swimdowns", STATE_AGENT_SWIM_DOWN);
	setup_combo("swimups", STATE_AGENT_SWIM_UP);
	setup_combo("swims", STATE_AGENT_SWIM);
	setup_combo("floats", STATE_AGENT_FLOAT);

	updateLayout(gSavedSettings.getBOOL("AOAdvanced"));
	AOSystem::requestConfigNotecard(false);

	return true;
}

void LLFloaterAO::updateLayout(bool advanced)
{
	reshape(advanced ? 800 : 200, getRect().getHeight());
	childSetVisible("tabcontainer", advanced);
}

void LLFloaterAO::onSpinnerCommit() const
{
	if (auto ao = AOSystem::getIfExists())
		ao->mAOStandTimer.reset();
}

void LLFloaterAO::onComboBoxCommit(LLUICtrl* ctrl) const
{
	if (LLComboBox* box = (LLComboBox*)ctrl)
	{
		std::string stranim = box->getValue().asString();
		if (auto ao = AOSystem::getIfExists())
		{
			if (box == mCombos[STATE_AGENT_IDLE])
			{
				ao->stand_iterator = box->getCurrentIndex();
				llassert(ao->stand_iterator < ao->mAOStands.size());
				const auto& name = ao->mAOStands[ao->stand_iterator].anim_name;
				llassert(name == box->getValue().asStringRef());
				cmdline_printchat("Changing stand to " + name + '.');
				ao->updateStand();
				ao->mAOStandTimer.reset();
			}
			else
			{
				auto end = mCombos.end();
				auto it = std::find(mCombos.begin(), end, box);
				llassert(it != end);
				const auto state = std::distance(mCombos.begin(), it);
				if (auto aop = ao->mAOOverrides[state])
				{
					LLUUID getAssetIDByName(const std::string & name);
//					LL_INFOS() << "state " << state << " - " << aop->playing() << LL_ENDL;
					bool was_playing = aop->playing;
					if (was_playing) aop->play(false);
					aop->ao_id = getAssetIDByName(stranim);
					if (was_playing) aop->play();
				}
			}
		}
		gSavedPerAccountSettings.setString(ctrl->getControlName(), stranim);
	}
}

void LLFloaterAO::onClickCycleStand(bool next) const
{
	auto ao = AOSystem::getIfExists();
	if (!ao) return;
	auto stand = ao->cycleStand(next, false);
	ao->mAOStandTimer.reset();
	if (stand < 0) return;
	cmdline_printchat("Changed stand to " + ao->mAOStands[stand].anim_name + '.');
}

void LLFloaterAO::onClickReloadCard() const
{
	AOSystem::instance().initSingleton();
}

void LLFloaterAO::onClickOpenCard() const
{
	auto config_nc_id = (LLUUID)gSavedPerAccountSettings.getString("AOConfigNotecardID");
	if (config_nc_id.notNull())
		if (LLViewerInventoryItem* item = gInventory.getItem(config_nc_id))
			if (gAgent.allowOperation(PERM_COPY, item->getPermissions(), GP_OBJECT_MANIPULATE) || gAgent.isGodlike())
				if(!item->getAssetUUID().isNull())
					open_notecard(item, "Note: " + item->getName(), LLUUID::null, false);
}

void LLFloaterAO::onClickNewCard() const
{
	// load the template file from app_settings/ao_template.ini then
	// create a new properly-formatted notecard in the user's inventory
	std::string ao_template = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "ao_template.ini");
	if (ao_template.empty())
	{
		ao_template = "#Can't find ao_template.ini in app_settings!";
		LL_WARNS() << ao_template << LL_ENDL;
		ao_template += "\n#ZHAO II Style Notecards are supported.";
	}

	create_inventory_item(gAgentID, gAgentSessionID,
						LLUUID::null, LLTransactionID::tnull, "New AO Notecard", 
						"Drop this notecard in your AO window to use", LLAssetType::AT_NOTECARD,
						LLInventoryType::IT_NOTECARD, NOT_WEARABLE, PERM_ALL, new AONotecardCallback(ao_template));
}
