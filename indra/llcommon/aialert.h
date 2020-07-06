/**
 * @file aialert.h
 * @brief Declaration of AIArgs and AIAlert classes.
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
 *   02/11/2013
 *   Initial version, written by Aleric Inglewood @ SL
 *
 *   05/11/2013
 *   Moved everything in namespace AIAlert, except AIArgs.
 */

#ifndef AI_ALERT
#define AI_ALERT

#include "llpreprocessor.h"
#include "llstring.h"
#include <deque>
#include <exception>

//===================================================================================================================================
// Facility to throw errors that can easily be converted to an informative pop-up floater for the user.

// Throw arbitrary class.
#define   THROW_ALERT_CLASS(Alert, ...) throw Alert(AIAlert::Prefix(),                                                     AIAlert::not_modal, __VA_ARGS__)
#define  THROW_MALERT_CLASS(Alert, ...) throw Alert(AIAlert::Prefix(),                                                         AIAlert::modal, __VA_ARGS__)
#ifdef __GNUC__
#define  THROW_FALERT_CLASS(Alert, ...) throw Alert(AIAlert::Prefix(__PRETTY_FUNCTION__, AIAlert::pretty_function_prefix), AIAlert::not_modal, __VA_ARGS__)
#define THROW_FMALERT_CLASS(Alert, ...) throw Alert(AIAlert::Prefix(__PRETTY_FUNCTION__, AIAlert::pretty_function_prefix),     AIAlert::modal, __VA_ARGS__)
#else
#define  THROW_FALERT_CLASS(Alert, ...) throw Alert(AIAlert::Prefix(__FUNCTION__, AIAlert::pretty_function_prefix), AIAlert::not_modal, __VA_ARGS__)
#define THROW_FMALERT_CLASS(Alert, ...) throw Alert(AIAlert::Prefix(__FUNCTION__, AIAlert::pretty_function_prefix),     AIAlert::modal, __VA_ARGS__)
#endif

// Shortcut to throw AIAlert::Error.
#define   THROW_ALERT(...)   THROW_ALERT_CLASS(AIAlert::Error, __VA_ARGS__)
#define  THROW_MALERT(...)  THROW_MALERT_CLASS(AIAlert::Error, __VA_ARGS__)
#define  THROW_FALERT(...)  THROW_FALERT_CLASS(AIAlert::Error, __VA_ARGS__)
#define THROW_FMALERT(...) THROW_FMALERT_CLASS(AIAlert::Error, __VA_ARGS__)

// Shortcut to throw AIAlert::ErrorCode.
#define   THROW_ALERTC(...)   THROW_ALERT_CLASS(AIAlert::ErrorCode, __VA_ARGS__)
#define  THROW_MALERTC(...)  THROW_MALERT_CLASS(AIAlert::ErrorCode, __VA_ARGS__)
#define  THROW_FALERTC(...)  THROW_FALERT_CLASS(AIAlert::ErrorCode, __VA_ARGS__)
#define THROW_FMALERTC(...) THROW_FMALERT_CLASS(AIAlert::ErrorCode, __VA_ARGS__)

// Shortcut to throw AIAlert::ErrorCode with errno as code.
#define   THROW_ALERTE(...) do { int errn = errno;   THROW_ALERT_CLASS(AIAlert::ErrorCode, errn, __VA_ARGS__); } while(0)
#define  THROW_MALERTE(...) do { int errn = errno;  THROW_MALERT_CLASS(AIAlert::ErrorCode, errn, __VA_ARGS__); } while(0)
#define  THROW_FALERTE(...) do { int errn = errno;  THROW_FALERT_CLASS(AIAlert::ErrorCode, errn, __VA_ARGS__); } while(0)
#define THROW_FMALERTE(...) do { int errn = errno; THROW_FMALERT_CLASS(AIAlert::ErrorCode, errn, __VA_ARGS__); } while(0)

// Examples

#ifdef EXAMPLE_CODE

	//----------------------------------------------------------
	// To show the alert box:

	catch (AIAlert::Error const& error)
    {
	   AIAlert::add(error);		// Optionally pass pretty_function_prefix as second parameter to *suppress* that output.
	}

    // or, for example

	catch (AIAlert::ErrorCode const& error)
    {
	   if (error.getCode() != EEXIST)
	   {
		 AIAlert::add(alert, AIAlert::pretty_function_prefix);
	   }
	}
	//----------------------------------------------------------
    // To throw alerts:

	THROW_ALERT("ExampleKey");																// A) Lookup "ExampleKey" in strings.xml and show translation.
	THROW_ALERT("ExampleKey", AIArgs("[FIRST]", first)("[SECOND]", second)(...etc...));		// B) Same as A, but replace [FIRST] with first, [SECOND] with second, etc.
	THROW_ALERT("ExampleKey", error);														// C) As A, but followed by a colon and a newline, and then the text of 'error'.
	THROW_ALERT(error, "ExampleKey");														// D) The text of 'error', followed by a colon and a newline and then as A.
	THROW_ALERT("ExampleKey", AIArgs("[FIRST]", first)("[SECOND]", second), error);			// E) As B, but followed by a colon and a newline, and then the text of 'error'.
	THROW_ALERT(error, "ExampleKey", AIArgs("[FIRST]", first)("[SECOND]", second));			// F) The text of 'error', followed by a colon and a newline and then as B.
	// where 'error' is a caught Error object (as above) in a rethrow.
	// Prepend ALERT with M and/or F to make the alert box Modal and/or prepend the text with the current function name.
	// For example,
	THROW_MFALERT("ExampleKey", AIArgs("[FIRST]", first));	// Throw a Modal alert box that is prefixed with the current Function name.
	// Append E after ALERT to throw an ErrorCode class that contains the current errno.
	// For example,
	THROW_FALERTE("ExampleKey", AIArgs("[FIRST]", first));	// Throw an alert box that is prefixed with the current Function name and pass errno to the catcher.

#endif // EXAMPLE_CODE

//
//===================================================================================================================================

// A wrapper around LLStringUtil::format_map_t to allow constructing a dictionary
// on one line by doing:
//
// AIArgs("[ARG1]", arg1)("[ARG2]", arg2)("[ARG3]", arg3)...

class LL_COMMON_API AIArgs
{
  private:
	LLStringUtil::format_map_t mArgs;	// The underlying replacement map.

  public:
	// Construct an empty map.
	AIArgs(void) { }
	// Construct a map with a single replacement.
	AIArgs(char const* key, std::string const& replacement) { mArgs[key] = replacement; }
	// Add another replacement.
	AIArgs& operator()(char const* key, std::string const& replacement) { mArgs[key] = replacement; return *this; }
	// The destructor may not throw.
	~AIArgs() noexcept { }

	// Accessor.
	LLStringUtil::format_map_t const& operator*() const { return mArgs; }
};

namespace AIAlert
{

enum modal_nt
{
  not_modal,
  modal
};

enum alert_line_type_nt
{
  normal = 0,
  empty_prefix = 1,
  pretty_function_prefix = 2
  // These must exist of single bits (a mask).
};

// An Prefix currently comes only in two flavors:
//
// empty_prefix           : An empty prefix.
// pretty_function_prefix : A function name prefix, this is the function from which the alert was thrown.

class LL_COMMON_API Prefix
{
  public:
	Prefix(void) : mType(empty_prefix) { }
	Prefix(char const* str, alert_line_type_nt type) : mStr(str), mType(type) { }

	operator bool(void) const { return mType != empty_prefix; }
	alert_line_type_nt type(void) const { return mType; }
	std::string const& str(void) const { return mStr; }

  private:
	std::string mStr;				// Literal text. For example a C++ function name.
	alert_line_type_nt mType;		// The type of this prefix.
};

// A class that represents one line with its replacements.
// The string mXmlDesc shall be looked up in strings.xml.
// This is not done as part of this class because LLTrans::getString
// is not part of llcommon.

class LL_COMMON_API Line
{
  private:
	bool mNewline;					// Prepend this line with a newline if set.
	std::string mXmlDesc;			// The keyword to look up in string.xml.
	AIArgs mArgs;					// Replacement map.
	alert_line_type_nt mType;		// The type of this line: normal for normal lines, other for prefixes.

  public:
	Line(std::string const& xml_desc, bool newline = false) : mNewline(newline), mXmlDesc(xml_desc), mType(normal) { }
	Line(std::string const& xml_desc, AIArgs const& args, bool newline = false) : mNewline(newline), mXmlDesc(xml_desc), mArgs(args), mType(normal) { }
	Line(Prefix const& prefix, bool newline = false) : mNewline(newline), mXmlDesc("AIPrefix"), mArgs("[PREFIX]", prefix.str()), mType(prefix.type()) { }
	// The destructor may not throw.
	~Line() noexcept { }

	// Prepend a newline before this line.
	void set_newline(void) { mNewline = true; }

	// These are to be used like: LLTrans::getString(line.getXmlDesc(), line.args()) and prepend with a \n if prepend_newline() returns true.
	std::string getXmlDesc(void) const { return mXmlDesc; }
	LLStringUtil::format_map_t const& args(void) const { return *mArgs; }
	bool prepend_newline(void) const { return mNewline; }

	// Accessors.
	bool suppressed(unsigned int suppress_mask) const { return (suppress_mask & mType) != 0; }
	bool is_prefix(void) const { return mType != normal; }
};

// This class is used to throw an error that will cause
// an alert box to pop up for the user.
//
// An alert box only has text and an OK button.
// The alert box does not give feed back to the program; it is purely informational.

// The class represents multiple lines, each line is to be translated and catenated,
// separated by newlines, and then written to an alert box. This is not done as part
// of this class because LLTrans::getString and LLNotification is not part of llcommon.
// Instead call LLNotificationUtil::add(Error const&).

class LL_COMMON_API Error : public std::exception
{
  public:
	typedef std::deque<Line> lines_type;

	// The destructor may not throw.
	~Error() noexcept { }

	// Accessors.
	lines_type const& lines(void) const { return mLines; }
	bool is_modal(void) const { return mModal == modal; }

	// Existing alert, just add a prefix and turn alert into modal if appropriate.
	Error(Prefix const& prefix, modal_nt type, Error const& alert);

	// A string with zero or more replacements.
	Error(Prefix const& prefix, modal_nt type,
		  std::string const& xml_desc, AIArgs const& args = AIArgs());

	// Same as above bit prepending the message with the text of another alert.
	Error(Prefix const& prefix, modal_nt type,
		  Error const& alert,
		  std::string const& xml_desc, AIArgs const& args = AIArgs());

	// Same as above but appending the message with the text of another alert.
	// (no args)
	Error(Prefix const& prefix, modal_nt type,
		  std::string const& xml_desc,
		  Error const& alert);
	// (with args)
	Error(Prefix const& prefix, modal_nt type,
		  std::string const& xml_desc, AIArgs const& args,
		  Error const& alert);

  private:
	lines_type mLines;								// The lines (or prefixes) of text to be displayed, each consisting on a keyword (to be looked up in strings.xml) and a replacement map.
	modal_nt mModal;								// If true, make the alert box a modal floater.
};

// Same as Error but allows to pass an additional error code.

class LL_COMMON_API ErrorCode : public Error
{
  private:
	int mCode;

  public:
	// The destructor may not throw.
	~ErrorCode() noexcept { }

	// Accessor.
	int getCode(void) const { return mCode; }

	// Just an Error with a code.
	ErrorCode(Prefix const& prefix, modal_nt type, int code,
	          Error const& alert) :
	  Error(prefix, type, alert), mCode(code) { }

	// A string with zero or more replacements.
	ErrorCode(Prefix const& prefix, modal_nt type, int code,
		      std::string const& xml_desc, AIArgs const& args = AIArgs()) :
	  Error(prefix, type, xml_desc, args), mCode(code) { }

	// Same as above bit prepending the message with the text of another alert.
	ErrorCode(Prefix const& prefix, modal_nt type, int code,
		      Error const& alert,
		      std::string const& xml_desc, AIArgs const& args = AIArgs()) :
	  Error(prefix, type, alert, xml_desc, args), mCode(code) { }

	// Same as above but appending the message with the text of another alert.
	// (no args)
	ErrorCode(Prefix const& prefix, modal_nt type, int code,
		      std::string const& xml_desc,
		      Error const& alert) :
	  Error(prefix, type, xml_desc, alert), mCode(code) { }
	// (with args)
	ErrorCode(Prefix const& prefix, modal_nt type, int code,
		      std::string const& xml_desc, AIArgs const& args,
		      Error const& alert) :
	  Error(prefix, type, xml_desc, args, alert), mCode(code) { }
};

} // namespace AIAlert

#endif // AI_ALERT
