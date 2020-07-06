/**
 * @file aixmllindengenepool.cpp
 * @brief XML linden_genepool serialization support.
 *
 * Copyright (c) 2013, Aleric Inglewood.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution.
 *
 * CHANGELOG
 *   and additional copyright holders.
 *
 *   01/11/2013
 *   Initial version, written by Aleric Inglewood @ SL
 */

// metaversion 1.0
// ===============
//
// Added as child of <linden_genepool>:
//
//   <meta gridnick="secondlife" date="2013-07-16T16:49:00.40Z" />
//
// Optionally, as child of <archetype>, the following node may appear:
//
//   <meta path="clothing/jackets" name="Purple jacket" description="A jacket with mainly the color purple" />
//
// Furthermore, metaversion 1.0 and higher allow the occurance of one or more <archetype> blocks.
// If this is used then it is strongly advised to use one <archetype> per wearable, so that
// the the <meta> node makes sense (it then refers to the wearable of that <archetype>).
//
// The reason for this clumsy way to link wearable to extra meta data is to stay
// compatible with the older format (no metaversion).

#include "llviewerprecompiledheaders.h"
#include "aixmllindengenepool.h"
#include "hippogridmanager.h"
#include "llinventorymodel.h"
#include "llvisualparam.h"
#include "llviewerwearable.h"
#include "llquantize.h"

void AIXMLLindenGenepool::MetaData::toXML(std::ostream& os, int indentation) const
{
  AIXMLElement tag(os, "meta", indentation);
  tag.attribute("gridnick", mGridNick);
  tag.attribute(DEFAULT_LLDATE_NAME, mDate);
}

AIXMLLindenGenepool::MetaData::MetaData(AIXMLElementParser const& parser)
{
  parser.attribute("gridnick", mGridNick);
  parser.attribute(DEFAULT_LLDATE_NAME, mDate);
}

AIXMLLindenGenepool::AIXMLLindenGenepool(const std::string& filename) : AIXMLRootElement(filename, "linden_genepool")
{
  attribute("version", "1.0");
  attribute("metaversion", "1.0");
  child(MetaData(gHippoGridManager->getConnectedGrid()->getGridNick(),  LLDate::now()));
}

void AIVisualParamIDValuePair::toXML(std::ostream& os, int indentation) const
{
  LLVisualParam const* visual_param = mVisualParam;
  if (!visual_param && mWearable)
  {
	visual_param = mWearable->getVisualParam(mID);
  }
  if (visual_param)
  {
	AIXMLElement tag(os, "param", indentation);
	tag.attribute("id", mID);
	tag.attribute("name", visual_param->getName());
	tag.attribute("value", mValue);
	tag.attribute("u8", (U32)F32_to_U8(mValue, visual_param->getMinWeight(), visual_param->getMaxWeight()));
	tag.attribute("type", visual_param->getTypeString());
	tag.attribute("wearable", visual_param->getDumpWearableTypeName());
  }
}

AIVisualParamIDValuePair::AIVisualParamIDValuePair(AIXMLElementParser const& parser)
{
  // Only id and value are relevant. Ignore all other attributes.
  parser.attribute("id", mID);
  parser.attribute("value", mValue);
}

void AITextureIDUUIDPair::toXML(std::ostream& os, int indentation) const
{
  AIXMLElement tag(os, "texture", indentation);
  tag.attribute("te", mID);
  tag.attribute(DEFAULT_LLUUID_NAME, mUUID);
}

AITextureIDUUIDPair::AITextureIDUUIDPair(AIXMLElementParser const& parser)
{
  parser.attribute("te", mID);
  parser.attribute(DEFAULT_LLUUID_NAME, mUUID);
}

void AIArchetype::MetaData::toXML(std::ostream& os, int indentation) const
{
  AIXMLElement tag(os, "meta", indentation);
  tag.attribute("path", mPath);
  tag.attribute("name", mName);
  tag.attribute("description", mDescription);
}

AIArchetype::MetaData::MetaData(AIXMLElementParser const& parser)
{
  char const* missing = NULL;
  if (!parser.attribute("path", mPath))
  {
	missing = "path";
  }
  if (!parser.attribute("name", mName))
  {
	missing = "name";
  }
  if (!parser.attribute("description", mDescription))
  {
	missing = "description";
  }
  if (missing)
  {
	THROW_ALERT("AIArchetype_MetaData_archetype_meta_has_no_ATTRIBUTE", AIArgs("[ATTRIBUTE]", missing));
  }
}

AIArchetype::MetaData::MetaData(LLViewerWearable const* wearable) : mName(wearable->getName()), mDescription(wearable->getDescription())
{
	// Path should already end on '/' if it is not empty.
	const LLUUID& root_id = gInventory.getRootFolderID();
	LLUUID id = wearable->getItemID();
	for(const LLInventoryObject* obj = gInventory.getObject(id); obj && id != root_id; obj = gInventory.getCategory(id))
	{
		std::string temp(mPath);
		mPath = obj->getName();
		if (!temp.empty())
		{
			mPath.append(1, '/');
			mPath.append(temp);
		}
		id = obj->getParentUUID();
	}
}

AIArchetype::AIArchetype(void) : mType(LLWearableType::WT_NONE)
{
}

AIArchetype::AIArchetype(LLWearableType::EType type) : mType(type)
{
}

AIArchetype::AIArchetype(LLViewerWearable const* wearable) : mType(wearable->getType()), mMetaData(wearable)
{
}

void AIArchetype::toXML(std::ostream& os, int indentation) const
{
  AIXMLElement tag(os, "archetype", indentation);
  if (mType == LLWearableType::WT_NONE)
  {
	tag.attribute("name", "???");
  }
  else
  {
	tag.attribute("name", LLWearableType::getTypeName(mType));
  }
  // mMetaData.mPath can be empty even exporting from the Appearance editor:
  // when the asset is in the "My Inventory" root.
  if (!mMetaData.mPath.empty() || !mMetaData.mName.empty())		// Is this not an old-style linden_genepool?
  {
	tag.child(mMetaData);
  }
  tag.child(mParams.begin(), mParams.end());
  tag.child(mTextures.begin(), mTextures.end());
}

AIArchetype::AIArchetype(AIXMLElementParser const& parser)
{
  std::string name;
  mType = LLWearableType::WT_NONE;

  if (!parser.attribute("name", name))
  {
	LL_WARNS() << "The <archetype> tag in file \"" << parser.filename() << "\" is missing the 'name' parameter." << LL_ENDL;
  }
  else if (name != "???")
  {
	mType = LLWearableType::typeNameToType(name);
  }
  if (parser.version() >= 1)
  {
	if (!parser.child("meta", mMetaData))
	{
	  THROW_ALERT("AIArchetype_archetype_has_no_meta");
	}
  }
  parser.push_back_children("param", mParams);
  parser.push_back_children("texture", mTextures);
}

