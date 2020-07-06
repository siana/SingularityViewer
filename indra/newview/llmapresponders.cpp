/** 
 * @file llmapresponders.cpp
 * @brief Processes responses received for map requests.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 * 
 * Copyright (c) 2006-2010, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include "llmapresponders.h"

#include "llfloaterworldmap.h"
#include "llviewertexturelist.h"
#include "llworldmap.h"
#include "llagent.h"
#include "llworldmapmessage.h"
#include "llsdserialize.h"

//virtual 
void LLMapLayerResponder::httpSuccess(void)
{
	LL_INFOS() << "LLMapLayerResponder::mContent from capabilities" << LL_ENDL;

	S32 agent_flags = mContent["AgentData"]["Flags"];
	U32 layer = flagsToLayer(agent_flags);

	if (layer != SIM_LAYER_COMPOSITE)
	{
		LL_WARNS() << "Invalid or out of date map image type returned!" << LL_ENDL;
		return;
	}

	LLUUID image_id;

	LLWorldMap::getInstance()->mMapLayers.clear();

	for (auto const& layer_data : mContent["LayerData"].array())
	{
		LLWorldMapLayer new_layer;
		new_layer.LayerDefined = TRUE;
		
		new_layer.LayerExtents.mLeft = layer_data["Left"];
		new_layer.LayerExtents.mRight = layer_data["Right"];
		new_layer.LayerExtents.mBottom = layer_data["Bottom"];
		new_layer.LayerExtents.mTop = layer_data["Top"];

		new_layer.LayerImageID = layer_data["ImageID"];
		new_layer.LayerImage = LLViewerTextureManager::getFetchedTexture(new_layer.LayerImageID);

		gGL.getTexUnit(0)->bind(new_layer.LayerImage.get());
		new_layer.LayerImage->setAddressMode(LLTexUnit::TAM_CLAMP);

		LLWorldMap::getInstance()->mMapLayers.push_back(new_layer);
	}
	LLWorldMap::getInstance()->mMapLoaded = true;
}
