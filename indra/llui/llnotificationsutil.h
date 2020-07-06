/**
 * @file llnotificationsutil.h
 *
 * $LicenseInfo:firstyear=2008&license=viewerlgpl$
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
#ifndef LLNOTIFICATIONSUTIL_H
#define LLNOTIFICATIONSUTIL_H

// The vast majority of clients of the notifications system just want to add 
// a notification to the screen, so define this lightweight public interface
// to avoid including the heavyweight llnotifications.h

#include "llnotificationptr.h"
#include "lluuid.h"
#include "aialert.h"

#ifndef BOOST_FUNCTION_HPP_INCLUDED
#include <boost/function.hpp>
#define BOOST_FUNCTION_HPP_INCLUDED
#endif

class LLSD;

namespace AIAlert
{
	// Add an alert directly to LLNotifications.

	// Look up xml_desc in strings.xml.
	LLNotificationPtr add(std::string const& xml_desc,
	                      modal_nt type = not_modal);
	// ... with replacement args.
	LLNotificationPtr add(std::string const& xml_desc, AIArgs const& args,
	                      modal_nt type = not_modal);

	// Append it to an existing alert error.
	LLNotificationPtr add(Error const& error,
	                      std::string const& xml_desc,
	                      unsigned int suppress_mask = 0, modal_nt type = not_modal);
	LLNotificationPtr add(Error const& error,
	                      std::string const& xml_desc, AIArgs const& args,
	                      unsigned int suppress_mask = 0, modal_nt type = not_modal);
	// Prepend it to an existing alert error.
	LLNotificationPtr add(std::string const& xml_desc,
	                      Error const& error,
	                      unsigned int suppress_mask = 0, modal_nt type = not_modal);
	LLNotificationPtr add(std::string const& xml_desc, AIArgs const& args,
	                      Error const& error,
	                      unsigned int suppress_mask = 0, modal_nt type = not_modal);

	// Just show the caught alert error.
	LLNotificationPtr add(Error const& error,
	                      unsigned int suppress_mask = 0, modal_nt type = not_modal);

	// Short cuts for enforcing modal alerts.
	inline LLNotificationPtr add_modal(std::string const& xml_desc) { return add(xml_desc, modal); }
	inline LLNotificationPtr add_modal(std::string const& xml_desc, AIArgs const& args) { return add(xml_desc, args, modal); }
	inline LLNotificationPtr add_modal(Error const& error, std::string const& xml_desc, unsigned int suppress_mask = 0) { return add(error, xml_desc, suppress_mask, modal); }
	inline LLNotificationPtr add_modal(Error const& error, std::string const& xml_desc, AIArgs const& args, unsigned int suppress_mask = 0) { return add(error, xml_desc, args, suppress_mask, modal); }
	inline LLNotificationPtr add_modal(std::string const& xml_desc, Error const& error, unsigned int suppress_mask = 0) { return add(xml_desc, error, suppress_mask, modal); }
	inline LLNotificationPtr add_modal(std::string const& xml_desc, AIArgs const& args, Error const& error, unsigned int suppress_mask = 0) { return add(xml_desc, args, error, suppress_mask, modal); }
	inline LLNotificationPtr add_modal(Error const& error, unsigned int suppress_mask = 0) { return add(error, suppress_mask, modal); }

	// Return the full, translated, texted of the alert (possibly suppressing certain output).
	std::string text(Error const& error, int suppress_mask = 0);
}

namespace LLNotificationsUtil
{
	LLNotificationPtr add(const std::string& name);
	
	LLNotificationPtr add(const std::string& name, 
						  const LLSD& substitutions);
	
	LLNotificationPtr add(const std::string& name, 
						  const LLSD& substitutions, 
						  const LLSD& payload);
	
	LLNotificationPtr add(const std::string& name, 
						  const LLSD& substitutions, 
						  const LLSD& payload, 
						  const std::string& functor_name);

	LLNotificationPtr add(const std::string& name, 
						  const LLSD& substitutions, 
						  const LLSD& payload, 
						  boost::function<void (const LLSD&, const LLSD&)> functor);
	
	S32 getSelectedOption(const LLSD& notification, const LLSD& response);

	void cancel(LLNotificationPtr pNotif);

	LLNotificationPtr find(LLUUID uuid);
}

#endif
