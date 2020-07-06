/* Copyright (C) 2013 Liru Færs
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA */

#ifndef LFSIMFEATUREHANDLER_H
#define LFSIMFEATUREHANDLER_H

#include "llsingleton.h"
#include "llpermissions.h"	// ExportPolicy

template<typename Type, typename Signal = boost::signals2::signal<void(const Type&)> >
class SignaledType
{
public:
	SignaledType() : mValue(), mDefaultValue() {}
	SignaledType(Type b) : mValue(b), mDefaultValue(b) {}

	typedef typename Signal::slot_type slot_t;
	boost::signals2::connection connect(const slot_t& slot) { return mSignal.connect(slot); }

	SignaledType& operator =(Type val)
	{
		if (val != mValue)
		{
			mValue = val;
			mSignal(val);
		}
		return *this;
	}
	operator Type() const { return mValue; }
	const Type& ref() const { return mValue; }
	void reset() { *this = mDefaultValue; }
	const Type& getDefault() const { return mDefaultValue; }

private:
	Signal mSignal;
	Type mValue;
	const Type mDefaultValue;
};

class LFSimFeatureHandler : public LLSingleton<LFSimFeatureHandler>
{
protected:
	friend class LLSingleton<LFSimFeatureHandler>;
	LFSimFeatureHandler();

public:
	void handleRegionChange();
	void setSupportedFeatures();

	// Connection setters
	boost::signals2::connection setSupportsExportCallback(const SignaledType<bool>::slot_t& slot) { return mSupportsExport.connect(slot); }
	boost::signals2::connection setDestinationGuideURLCallback(const SignaledType<std::string>::slot_t& slot) { return mDestinationGuideURL.connect(slot); }
	boost::signals2::connection setSearchURLCallback(const SignaledType<std::string>::slot_t& slot) { return mSearchURL.connect(slot); }
	boost::signals2::connection setEventsURLCallback(const SignaledType<std::string>::slot_t& slot) { return mEventsURL.connect(slot); }
	boost::signals2::connection setSayRangeCallback(const SignaledType<U32>::slot_t& slot) { return mSayRange.connect(slot); }
	boost::signals2::connection setShoutRangeCallback(const SignaledType<U32>::slot_t& slot) { return mShoutRange.connect(slot); }
	boost::signals2::connection setWhisperRangeCallback(const SignaledType<U32>::slot_t& slot) { return mWhisperRange.connect(slot); }

	// Accessors
	bool simSupportsExport() const { return mSupportsExport; }
	std::string destinationGuideURL() const { return mDestinationGuideURL; }
	std::string mapServerURL() const { return mMapServerURL; }
	std::string searchURL() const { return mSearchURL; }
	const std::string& getEventsURL() const { return mEventsURL.ref(); }
	const std::string& gridName() const { return mGridName.ref(); }
	U32 sayRange() const { return mSayRange; }
	U32 shoutRange() const { return mShoutRange; }
	U32 whisperRange() const { return mWhisperRange; }
	ExportPolicy exportPolicy() const;

private:
	// SignaledTypes
	SignaledType<bool> mSupportsExport;
	SignaledType<std::string> mDestinationGuideURL;
	std::string mMapServerURL;
	SignaledType<std::string> mSearchURL;
	SignaledType<std::string> mGridName;
	SignaledType<std::string> mEventsURL;
	SignaledType<U32> mSayRange;
	SignaledType<U32> mShoutRange;
	SignaledType<U32> mWhisperRange;
};

#endif //LFSIMFEATUREHANDLER_H

