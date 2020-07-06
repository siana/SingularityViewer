#pragma once

#include "aostate.h"
#include "llfloater.h"

class LLFloaterAO final : public LLFloater, public LLFloaterSingleton<LLFloaterAO>
{
	friend class AOSystem;
public:

	LLFloaterAO(const LLSD&);
	BOOL postBuild() override;
	void onOpen() override;
	virtual ~LLFloaterAO();
	void updateLayout(bool advanced);

	void onClickCycleStand(bool next) const;
	void onClickReloadCard() const;
	void onClickOpenCard() const;
	void onClickNewCard() const;

private:
	void onSpinnerCommit() const;
	void onComboBoxCommit(LLUICtrl* ctrl) const;
	std::array<class LLComboBox*, STATE_AGENT_END> mCombos;

protected:

	LLComboBox* getComboFromState(const U8& state) const { return mCombos[state]; }
};
