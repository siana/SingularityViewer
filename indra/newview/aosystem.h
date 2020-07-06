#pragma once

#include "aostate.h"
#include "llcontrol.h"
#include "lleventtimer.h"

class AOSystem final : public LLSingleton<AOSystem>
{
	friend class LLSingleton<AOSystem>;
	friend class AOInvTimer;
	friend class LLFloaterAO;
	AOSystem();
	~AOSystem();
public:
	static void start(); // Runs the necessary actions to get the AOSystem ready, then initializes it.
	void initSingleton() override;

	static void typing(bool start);

	int stand_iterator;
	bool isStanding() const { return stand().playing; }
	void updateStand();
	int cycleStand(bool next = true, bool random = true);
	void toggleSwim(bool underwater);

	void doMotion(const LLUUID& id, bool start);
	void startMotion(const LLUUID& id) { doMotion(id, true); }
	void stopMotion(const LLUUID& id) { doMotion(id, false); }
	void stopCurrentStand() const { stand().play(false); }
	void stopAllOverrides() const;

protected:
	struct struct_stands
	{
		LLUUID ao_id;
		std::string anim_name;
	};
	typedef std::vector<struct_stands> stands_vec;
	stands_vec mAOStands;

	struct overrides
	{
		virtual bool overrideAnim(bool swimming, const LLUUID& anim) const = 0;
		virtual const LLUUID& getOverride() const { return ao_id; }
		virtual bool play_condition() const; // True if prevented from playing
		virtual bool isLowPriority() const { return false; }
		void play(bool start = true);
		LLUUID ao_id;
		LLPointer<LLControlVariable> setting;
		bool playing;
		virtual ~overrides() {}
	protected:
		overrides(const char* setting_name);
	};
	friend struct override_low_priority;
	friend struct override_single;
	friend struct override_sit;
	struct override_stand final : public overrides
	{
		override_stand() : overrides(nullptr) {}
		bool overrideAnim(bool swimming, const LLUUID& anim) const override;
		bool play_condition() const override;
		bool isLowPriority() const override { return true; }
		void update(const stands_vec& stands, const int& iter)
		{
			if (stands.empty()) ao_id.setNull();
			else ao_id = stands[iter].ao_id;
		}
	};
	std::array<overrides*, STATE_AGENT_END> mAOOverrides;

	override_stand& stand() const { return static_cast<override_stand&>(*mAOOverrides[STATE_AGENT_IDLE]); }

private:
	std::array<boost::signals2::scoped_connection, 3> mConnections;


	class AOStandTimer final : public LLEventTimer
	{
		friend class AOSystem;
	public:
		AOStandTimer() : LLEventTimer(F32_MAX) {}
		~AOStandTimer()
		{
//			LL_INFOS() << "dead" << LL_ENDL;
		}
		BOOL tick() override
		{
//			LL_INFOS() << "tick" << LL_ENDL;
			if (auto ao = AOSystem::getIfExists())
				ao->cycleStand();
			return false;
		}
		void reset();
	};
	AOStandTimer mAOStandTimer;

	static void requestConfigNotecard(bool reload = true);
	static void parseNotecard(LLVFS* vfs, const LLUUID& asset_uuid, LLAssetType::EType type, S32 status, bool reload);
};

