/**
 * @file llfloaterobjectweights.cpp
 * @brief Object weights advanced view floater
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2011, Linden Research, Inc.
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
#include "llviewerprecompiledheaders.h"

#include "llfloaterobjectweights.h"

#include "llparcel.h"

#include "lltextbox.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llfloatertools.h" // Singu Note: For placement beside the tools floater when it is opened.
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"

// virtual
bool LLCrossParcelFunctor::apply(LLViewerObject* obj)
{
	// Add the root object box.
	mBoundingBox.addBBoxAgent(LLBBox(obj->getPositionRegion(), obj->getRotationRegion(), obj->getScale() * -0.5f, obj->getScale() * 0.5f).getAxisAligned());

	// Extend the bounding box across all the children.
	LLViewerObject::const_child_list_t const& children = obj->getChildren();
	for (const auto& iter : children)
	{
		LLViewerObject* child = iter;
		mBoundingBox.addBBoxAgent(LLBBox(child->getPositionRegion(), child->getRotationRegion(), child->getScale() * -0.5f, child->getScale() * 0.5f).getAxisAligned());
	}

	bool result = false;

	LLViewerRegion* region = obj->getRegion();
	if (region)
	{
		std::vector<LLBBox> boxes;
		boxes.push_back(mBoundingBox);
		result = region->objectsCrossParcel(boxes);
	}

	return result;
}

LLFloaterObjectWeights::LLFloaterObjectWeights(const LLSD& key)
:	LLFloater(key),
	mSelectedObjects(nullptr),
	mSelectedPrims(nullptr),
	mSelectedDownloadWeight(nullptr),
	mSelectedPhysicsWeight(nullptr),
	mSelectedServerWeight(nullptr),
	mSelectedDisplayWeight(nullptr),
	mSelectedOnLand(nullptr),
	mRezzedOnLand(nullptr),
	mRemainingCapacity(nullptr),
	mTotalCapacity(nullptr)
{
	//buildFromFile("floater_tools.xml");
	LLUICtrlFactory::getInstance()->buildFloater(this,"floater_object_weights.xml", NULL, false);
}

LLFloaterObjectWeights::~LLFloaterObjectWeights()
{
}

// virtual
BOOL LLFloaterObjectWeights::postBuild()
{
	mSelectedObjects = getChild<LLTextBox>("objects");
	mSelectedPrims = getChild<LLTextBox>("prims");

	mSelectedDownloadWeight = getChild<LLTextBox>("download");
	mSelectedPhysicsWeight = getChild<LLTextBox>("physics");
	mSelectedServerWeight = getChild<LLTextBox>("server");
	mSelectedDisplayWeight = getChild<LLTextBox>("display");

	mSelectedOnLand = getChild<LLTextBox>("selected");
	mRezzedOnLand = getChild<LLTextBox>("rezzed_on_land");
	mRemainingCapacity = getChild<LLTextBox>("remaining_capacity");
	mTotalCapacity = getChild<LLTextBox>("total_capacity");

	return TRUE;
}

// virtual
void LLFloaterObjectWeights::onOpen()
{
	const LLRect& tools_rect = gFloaterTools->getRect();
	setOrigin(tools_rect.mRight, tools_rect.mTop - getRect().getHeight());
	refresh();
	updateLandImpacts(LLViewerParcelMgr::getInstance()->getFloatingParcelSelection()->getParcel());
}

// virtual
void LLFloaterObjectWeights::onWeightsUpdate(const SelectionCost& selection_cost)
{
	mSelectedDownloadWeight->setText(llformat("%.2f", selection_cost.mNetworkCost));
	mSelectedPhysicsWeight->setText(llformat("%.2f", selection_cost.mPhysicsCost));
	mSelectedServerWeight->setText(llformat("%.2f", selection_cost.mSimulationCost));

	S32 render_cost = LLSelectMgr::getInstance()->getSelection()->getSelectedObjectRenderCost();
	mSelectedDisplayWeight->setText(llformat("%d", render_cost));

	toggleWeightsLoadingIndicators(false);
}

//virtual
void LLFloaterObjectWeights::setErrorStatus(U32 status, const std::string& reason)
{
	const std::string text = getString("nothing_selected");

	mSelectedDownloadWeight->setText(text);
	mSelectedPhysicsWeight->setText(text);
	mSelectedServerWeight->setText(text);
	mSelectedDisplayWeight->setText(text);

	toggleWeightsLoadingIndicators(false);
}

void LLFloaterObjectWeights::updateLandImpacts(const LLParcel* parcel)
{
	auto selection = LLSelectMgr::instance().getSelection();
	if (!parcel || !selection || selection->isEmpty())
	{
		updateIfNothingSelected();
	}
	else
	{
		S32 rezzed_prims = parcel->getSimWidePrimCount();
		S32 total_capacity = parcel->getSimWideMaxPrimCapacity();

		mRezzedOnLand->setText(llformat("%d", rezzed_prims));
		mRemainingCapacity->setText(llformat("%d", total_capacity - rezzed_prims));
		mTotalCapacity->setText(llformat("%d", total_capacity));

		toggleLandImpactsLoadingIndicators(false);
	}
}

void LLFloaterObjectWeights::refresh()
{
	auto selection = LLSelectMgr::instance().getSelection();

	if (!selection || selection->isEmpty())
	{
		updateIfNothingSelected();
	}
	else
	{
		S32 prim_count = selection->getObjectCount();
		S32 link_count = selection->getRootObjectCount();
		F32 prim_equiv = selection->getSelectedLinksetCost();

		mSelectedObjects->setText(llformat("%d", link_count));
		mSelectedPrims->setText(llformat("%d", prim_count));
		mSelectedOnLand->setText(llformat("%.1d", (S32)prim_equiv));

		LLCrossParcelFunctor func;
		if (selection->applyToRootObjects(&func, true))
		{
			// Some of the selected objects cross parcel bounds.
			// We don't display object weights and land impacts in this case.
			const std::string text = getString("nothing_selected");

			mRezzedOnLand->setText(text);
			mRemainingCapacity->setText(text);
			mTotalCapacity->setText(text);

			toggleLandImpactsLoadingIndicators(false);
		}

		LLViewerRegion* region = gAgent.getRegion();
		if (region && region->capabilitiesReceived())
		{
			for (LLObjectSelection::valid_root_iterator iter = selection->valid_root_begin();
					iter != selection->valid_root_end(); ++iter)
			{
				LLAccountingCostManager::getInstance()->addObject((*iter)->getObject()->getID());
			}

			std::string url = region->getCapability("ResourceCostSelected");
			if (!url.empty())
			{
				// Update the transaction id before the new fetch request
				generateTransactionID();

				LLAccountingCostManager::getInstance()->fetchCosts(Roots, url, getObserverHandle());
				toggleWeightsLoadingIndicators(true);
			}
		}
		else
		{
			LL_WARNS() << "Failed to get region capabilities" << LL_ENDL;
		}
	}
}

// virtual
void LLFloaterObjectWeights::generateTransactionID()
{
	mTransactionID.generate();
}

void LLFloaterObjectWeights::toggleWeightsLoadingIndicators(bool visible)
{
	/* Singu TODO: LLLoadingIndicator
	childSetVisible("download_loading_indicator", visible);
	childSetVisible("physics_loading_indicator", visible);
	childSetVisible("server_loading_indicator", visible);
	childSetVisible("display_loading_indicator", visible);
	*/

	mSelectedDownloadWeight->setVisible(!visible);
	mSelectedPhysicsWeight->setVisible(!visible);
	mSelectedServerWeight->setVisible(!visible);
	mSelectedDisplayWeight->setVisible(!visible);
}

void LLFloaterObjectWeights::toggleLandImpactsLoadingIndicators(bool visible)
{
	/* Singu TODO: LLLoadingIndicator
	childSetVisible("selected_loading_indicator", visible);
	childSetVisible("rezzed_on_land_loading_indicator", visible);
	childSetVisible("remaining_capacity_loading_indicator", visible);
	childSetVisible("total_capacity_loading_indicator", visible);
	*/

	mSelectedOnLand->setVisible(!visible);
	mRezzedOnLand->setVisible(!visible);
	mRemainingCapacity->setVisible(!visible);
	mTotalCapacity->setVisible(!visible);
}

void LLFloaterObjectWeights::updateIfNothingSelected()
{
	const std::string text = getString("nothing_selected");

	mSelectedObjects->setText(text);
	mSelectedPrims->setText(text);

	mSelectedDownloadWeight->setText(text);
	mSelectedPhysicsWeight->setText(text);
	mSelectedServerWeight->setText(text);
	mSelectedDisplayWeight->setText(text);

	mSelectedOnLand->setText(text);
	mRezzedOnLand->setText(text);
	mRemainingCapacity->setText(text);
	mTotalCapacity->setText(text);

	toggleWeightsLoadingIndicators(false);
	toggleLandImpactsLoadingIndicators(false);
}
