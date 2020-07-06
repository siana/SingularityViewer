/**
 * @file aixmllindengenepool.h
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

#ifndef AIXMLLINDENGENEPOOL_H
#define AIXMLLINDENGENEPOOL_H

#include "aixml.h"
#include "llwearabletype.h"
#include "llviewervisualparam.h"
#include <vector>

class LLViewerWearable;

class AIXMLLindenGenepool : public AIXMLRootElement
{
  public:
	struct MetaData
	{
	  std::string mGridNick;
	  LLDate mDate;

	  MetaData(void) { }
	  MetaData(std::string const& grid_nick, LLDate const& date) : mGridNick(grid_nick), mDate(date) { }

	  void toXML(std::ostream& os, int indentation) const;
	  MetaData(AIXMLElementParser const& parser);
	};

	AIXMLLindenGenepool(const std::string& filename);
};

class AIVisualParamIDValuePair
{
  private:
	// A wearable + ID define the LLVisualParam, but it also possible to specify the LLVisualParam directly.
	LLVisualParam const* mVisualParam;		// Specific LLVisualParam, given at construction, or ...
	LLViewerWearable const* mWearable;		// Underlaying wearable, if any.

	U32 mID;								// The visual parameter id.
	F32 mValue;								// The value of the visual parameter.

  public:
	AIVisualParamIDValuePair(LLVisualParam const* visual_param) :
	    mVisualParam(visual_param), mWearable(NULL), mID(visual_param->getID()), mValue(visual_param->getWeight()) { }

	AIVisualParamIDValuePair(LLVisualParam const* visual_param, F32 value) :
	    mVisualParam(visual_param), mWearable(NULL), mID(visual_param->getID()), mValue(value) { }

	AIVisualParamIDValuePair(LLViewerWearable const* wearable, U32 id, F32 value) :
	    mVisualParam(NULL), mWearable(wearable), mID(id), mValue(value) { }

	void toXML(std::ostream& os, int indentation) const;
	AIVisualParamIDValuePair(AIXMLElementParser const& parser);

	// Accessors.
	U32 getID(void) const { return mID; }
	F32 getValue(void) const { return mValue; }
};

class AITextureIDUUIDPair
{
  private:
	U32 mID;
	LLUUID mUUID;

  public:
	AITextureIDUUIDPair(U32 id, LLUUID const& uuid) : mID(id), mUUID(uuid) { }

	void toXML(std::ostream& os, int indentation) const;
	AITextureIDUUIDPair(AIXMLElementParser const& parser);

	// Accessors.
	U32 getID(void) const { return mID; }
	LLUUID const& getUUID(void) const { return mUUID; }
};

class AIArchetype
{
  public:
	struct MetaData
	{
	  std::string mPath;				// The wearable location in the inventory.
	  std::string mName;				// The wearable name.
	  std::string mDescription;			// The wearable description.

	  MetaData(void) { }
	  MetaData(LLViewerWearable const* wearable);

	  void toXML(std::ostream& os, int indentation) const;
	  MetaData(AIXMLElementParser const& parser);
	};

	typedef std::vector<AIVisualParamIDValuePair> params_type;
	typedef std::vector<AITextureIDUUIDPair> textures_type;

  private:
	LLWearableType::EType mType;	// The type of the wearable.
	MetaData mMetaData;
	params_type mParams;
	textures_type mTextures;

  public:
	// Accessors.
	LLWearableType::EType getType(void) const { return mType; }
	MetaData const& getMetaData(void) const { return mMetaData; }
	params_type const& getParams(void) const { return mParams; }
	textures_type const& getTextures(void) const { return mTextures; }

  public:
	// An archtype without wearable has no (known) metadata. This is recognized because mPath will be empty.
	// An archtype without type with get the attribute name="???".
	AIArchetype(void);										// <archetype name="???">
	AIArchetype(LLWearableType::EType type);				// <archetype name="shirt">
	AIArchetype(LLViewerWearable const* wearable);			// <archetype name="shirt"> <meta path="myclothes" name="blue shirt" description="Awesome blue shirt">

	void add(AIVisualParamIDValuePair const& visual_param_id_value_pair) { mParams.push_back(visual_param_id_value_pair); }
	void add(AITextureIDUUIDPair const& texture_id_uuid_pair) { mTextures.push_back(texture_id_uuid_pair); }

	void toXML(std::ostream& os, int indentation) const;
	AIArchetype(AIXMLElementParser const& parser);
};

#endif // AIXMLLINDENGENEPOOL_H
