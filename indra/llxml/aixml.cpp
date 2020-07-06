/**
 * @file aixml.cpp
 * @brief XML serialization support.
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
 *   30/07/2013
 *   Initial version, written by Aleric Inglewood @ SL
 */

#include "sys.h"
#include "aixml.h"
#include "llmd5.h"
#include <boost/tokenizer.hpp>

//=============================================================================
// Overview

// The AIXML* classes provide an Object Oriented way to serialize objects
// to and from an XML file.
//
// The following classes are provided:
//
// AIXMLRootElement			- Write an object to a file (including XML declaration at the top).
// AIXMLElement				- Write an ojbect to an ostream (just one XML element).
//
// AIXMLParser				- Read and deserialize an XML file written with AIXMLRootElement.
// AIXMLElementParser		- Read and deserialize an XML stream written with AIXMLElement.
//
// Classes that need to be written to and from XML would typically
// supply two member functions. For example,

#ifdef EXAMPLE_CODE // undefined

class HelloWorld {
  public:
	// Write object to XML.
	void toXML(std::ostream& os, int indentation) const;
	// Read object from XML.
	HelloWorld(AIXMLElementParser const& parser);

  private:
	// Example member variables...
	Attribute1 mAttribute1;
	Attribute2 mAttribute2;
	// etc.
	Custom1 mCustom;
	std::vector<Custom2> mContainer;
	LLDate mDate;
	LLMD5 mMd5;
	LLUUID mUUID;
};

// Typical serialization member function.
void HelloWorld::toXML(std::ostream& os, int indentation) const
{
  AIXMLElement tag(os, "helloworld", indentation);

  // Zero or more attributes:
  tag.attribute("attributename1", mAttribute1);			// Uses operator<<(std::ostream&, Attribute1 const&) to write mAttribute1.
  tag.attribute("attributename2", mAttribute2);			// Uses operator<<(std::ostream&, Attribute2 const&) to write mAttribute2.
  	// etc.

  // Zero or more child elements:
  tag.child("tagname", mChild1);
  tag.child(mCustom);									// Calls mCustom.toXML() to insert the object.
  tag.child(mContainer.begin(), mContainer.end());		// Calls tag.child(element) for each element of the container.
  // Special allowed cases:
  tag.child(mDate);										// Uses "date" as tag name.
  tag.child(mMd5);										// Uses "md5" as tag name.
  tag.child(mUUID);										// Uses "uuid" as tag name.
}

// Typical deserialization member function.
HelloWorld::HelloWorld(AIXMLElementParser const& parser)
{
  // Zero or more attributes:
  parser.attribute("attributename1", "foobar");			// Throws std::runtime_error is attributename1 is missing or does not have the value "foobar".
  if (!parser.attribute("attributename2", mAttribute2))	// Reads value of attributename2 into mAttribute2 (throws if it could not be parsed).
  {
	throw std::runtime_error("...");					// Attribute was missing.
  }

  // Zero or more child elements:
  parser.child("tagname", mChild1);
  parser.child("custom1", mCustom);
  parser.insert_children("custom2", mContainer);
  // Special allowed cases:
  parser.child(mDate);
  parser.child(mMd5);
  parser.child(mUUID);
}

// To actually write to an XML file one would do, for example:

  LLFILE* fp = fopen(...);
  AIXMLRootElement tag(fp, "rootname");
  tag.attribute("version", "1.0");
  tag.child(LLDate::now());
  tag.child(mHelloWorld);

// And to read it again,

  AIXMLParser helloworld(filename, "description of file used for error reporting", "rootname", 1);
  helloworld.attribute("version", "1.0");
  helloworld.child("helloworld", mHelloWorld);

// Of course, both would need to be in a try { } catch block.

#endif // EXAMPLE_CODE

// Do NOT change these - it would break old databases.
char const* const DEFAULT_LLUUID_NAME = "uuid";
char const* const DEFAULT_MD5STR_NAME = "md5";
char const* const DEFAULT_LLDATE_NAME = "date";

std::string const DEFAULT_MD5STR_ATTRIBUTE_NAME = DEFAULT_MD5STR_NAME;
std::string const DEFAULT_LLUUID_ATTRIBUTE_NAME = DEFAULT_LLUUID_NAME;
std::string const DEFAULT_LLDATE_ATTRIBUTE_NAME = DEFAULT_LLDATE_NAME;
std::string const DEFAULT_VERSION_ATTRIBUTE_NAME = "version";

struct xdigit {
  bool isxdigit;
  xdigit(void) : isxdigit(true) { }
  void operator()(char c) { isxdigit = isxdigit && std::isxdigit(c); }
  operator bool() const { return isxdigit; }
};

static bool is_valid_md5str(std::string const& str)
{
  return str.length() == MD5HEX_STR_BYTES && std::for_each(str.begin(), str.end(), xdigit());
}

// Conversion routine that is a lot more strict then LLStringUtil::convertToU32.
// This version does not allow leading or trailing spaces, nor does it allow a leading minus sign.
// Leading zeroes are not allowed except a 0 by itself.
bool convertToU32strict(std::string const& str, U32& value)
{
  bool first = true;
  value = 0;
  for (std::string::const_iterator i = str.begin(); i != str.end(); ++i)
  {
    if (value == 0 && !first || !std::isdigit(*i))		// Reject leading zeroes and non-digits.
      return false;
    value = value * 10 + *i - '0';
    first = false;
  }
  return !first;										// Reject empty string.
}

typedef boost::tokenizer<boost::char_separator<char> > boost_tokenizer;

bool decode_version(std::string const& version, U32& major, U32& minor)
{
  boost_tokenizer tokens(version, boost::char_separator<char>("", "."));
  boost_tokenizer::const_iterator itTok = tokens.begin();
  return itTok != tokens.end() && convertToU32strict(*itTok++, major) &&
         itTok != tokens.end() && *itTok++ == "." &&
         itTok != tokens.end() && convertToU32strict(*itTok, minor);
}

bool md5strFromXML(LLXmlTreeNode* node, std::string& md5str_out)
{
  static LLStdStringHandle const DEFAULT_MD5STR_ATTRIBUTE_NAME_HANDLE = LLXmlTree::addAttributeString(DEFAULT_MD5STR_ATTRIBUTE_NAME);
  return node->getFastAttributeString(DEFAULT_MD5STR_ATTRIBUTE_NAME_HANDLE, md5str_out) && is_valid_md5str(md5str_out);
}

bool md5strFromXML(LLXmlTreeNode* node, std::string& md5str_out, std::string const& attribute_name)
{
  return node->getAttributeString(attribute_name, md5str_out) && is_valid_md5str(md5str_out);
}

bool UUIDFromXML(LLXmlTreeNode* node, LLUUID& uuid_out)
{
  static LLStdStringHandle const DEFAULT_LLUUID_ATTRIBUTE_NAME_HANDLE = LLXmlTree::addAttributeString(DEFAULT_LLUUID_ATTRIBUTE_NAME);
  return node->getFastAttributeUUID(DEFAULT_LLUUID_ATTRIBUTE_NAME_HANDLE, uuid_out);
}

bool UUIDFromXML(LLXmlTreeNode* node, LLUUID& uuid_out, std::string const& attribute_name)
{
  return node->getAttributeUUID(attribute_name, uuid_out);
}

bool dateFromXML(LLXmlTreeNode* node, LLDate& date_out)
{
  static LLStdStringHandle const DEFAULT_LLDATE_ATTRIBUTE_NAME_HANDLE = LLXmlTree::addAttributeString(DEFAULT_LLDATE_ATTRIBUTE_NAME);
  std::string date_s;
  return node->getFastAttributeString(DEFAULT_LLDATE_ATTRIBUTE_NAME_HANDLE, date_s) && date_out.fromString(date_s);
}

bool dateFromXML(LLXmlTreeNode* node, LLDate& date_out, std::string const& attribute_name)
{
  std::string date_s;
  return node->getAttributeString(attribute_name, date_s) && date_out.fromString(date_s);
}

bool versionFromXML(LLXmlTreeNode* node, U32& major_out, U32& minor_out)
{
  static LLStdStringHandle const DEFAULT_VERSION_ATTRIBUTE_NAME_HANDLE = LLXmlTree::addAttributeString(DEFAULT_VERSION_ATTRIBUTE_NAME);
  major_out = minor_out = 0;
  std::string version_s;
  return node->getFastAttributeString(DEFAULT_VERSION_ATTRIBUTE_NAME_HANDLE, version_s) && decode_version(version_s, major_out, minor_out);
}

bool versionFromXML(LLXmlTreeNode* node, U32& major_out, U32& minor_out, std::string const& attribute_name)
{
  major_out = minor_out = 0;
  std::string version_s;
  return node->getAttributeString(attribute_name, version_s) && decode_version(version_s, major_out, minor_out);
}

//-----------------------------------------------------------------------------
// AIXMLElement

AIXMLElement::AIXMLElement(std::ostream& os, char const* name, int indentation) :
    mOs(os), mName(name), mIndentation(indentation), mHasChildren(false)
{
  mOs << std::string(mIndentation, ' ') << '<' << mName;
  if (!mOs.good())
  {
	THROW_ALERT("AIXMLElement_Failed_to_write_DATA", AIArgs("[DATA]", "<" + mName));
  }
}

int AIXMLElement::open_child(void)
{
  if (!mHasChildren)
  {
	mOs << ">\n";
	if (!mOs.good())
	{
	  THROW_ALERT("AIXMLElement_closing_child_Failed_to_write_DATA", AIArgs("[DATA]", ">\\n"));
	}
	mHasChildren = true;
  }
  mIndentation += 2;
  return mIndentation;
}

void AIXMLElement::close_child(void)
{
  mIndentation -= 2;
}

AIXMLElement::~AIXMLElement() noexcept(false)
{
  if (mHasChildren)
  {
	mOs << std::string(mIndentation, ' ') << "</" << mName << ">\n";
	if (!mOs.good())
	{
	  THROW_ALERT("AIXMLElement_closing_child_Failed_to_write_DATA",
		  AIArgs("[DATA]", "\\n" + std::string(mIndentation, ' ') + "</" + mName + ">\\n"));
	}
  }
  else
  {
	mOs << " />\n";
	if (!mOs.good())
	{
	  THROW_ALERT("AIXMLElement_closing_child_Failed_to_write_DATA", AIArgs("[DATA]", " />\\n"));
	}
  }
}

template<>
void AIXMLElement::child(LLUUID const& element)
{
  open_child();
  write_child(DEFAULT_LLUUID_NAME, element);
  close_child();
}

template<>
void AIXMLElement::child(LLMD5 const& element)
{
  open_child();
  write_child(DEFAULT_MD5STR_NAME, element);
  close_child();
}

template<>
void AIXMLElement::child(LLDate const& element)
{
  open_child();
  write_child(DEFAULT_LLDATE_NAME, element);
  close_child();
}

//-----------------------------------------------------------------------------
// AIXMLStream

AIXMLStream::AIXMLStream(const std::string& filename, bool standalone) : mOfs(filename)
{
  mOfs << "<?xml version=\"1.0\" encoding=\"utf-8\"" << (standalone ? " standalone=\"yes\"" : LLStringUtil::null) << "?>\n";
  if (!mOfs)
  {
	// I don't think that errno is set to anything else but EBADF here,
	// so there is not really any informative message to add here.
	THROW_MALERT("AIXMLStream_fprintf_failed_to_write_xml_header");
  }
}

AIXMLStream::~AIXMLStream()
{
  if (mOfs.is_open())
  {
	mOfs.close();
  }
}

//-----------------------------------------------------------------------------
// AIXMLParser

AIXMLParser::AIXMLParser(std::string const& filename, char const* file_desc, std::string const& name, U32 major_version) :
	AIXMLElementParser(mFilename, mFileDesc, major_version),
	mFilename(filename), mFileDesc(file_desc)
{
  char const* error = NULL;
  AIArgs args;
  if (!mXmlTree.parseFile(filename, TRUE))
  {
	error = "AIXMLParser_Cannot_parse_FILEDESC_FILENAME";
  }
  else
  {
	mNode = mXmlTree.getRoot();
	if (!mNode)
	{
	  error = "AIXMLParser_No_root_node_found_in_FILEDESC_FILENAME";
	}
	else if (!mNode->hasName(name))
	{
	  error = "AIXMLParser_Missing_header_NAME_invalid_FILEDESC_FILENAME";
	  args("[NAME]", name);
	}
	else if (!versionFromXML(mNode, mVersionMajor, mVersionMinor))
	{
	  error = "AIXMLParser_Invalid_or_missing_NAME_version_attribute_in_FILEDESC_FILENAME";
	  args("[NAME]", name);
	}
	else if (mVersionMajor != major_version)
	{
	  error = "AIXMLParser_Incompatible_NAME_version_MAJOR_MINOR_in";
	  args("[NAME]", name)("[MAJOR]", llformat("%u", mVersionMajor))("[MINOR]", llformat("%u", mVersionMinor));
	}
  }
  if (error)
  {
	THROW_MALERT(error, args("[FILEDESC]", mFileDesc)("[FILENAME]", mFilename));
  }
}

//-----------------------------------------------------------------------------
// AIXMLElementParser

template<>
LLMD5 AIXMLElementParser::read_string(std::string const& value) const
{ 
  if (!is_valid_md5str(value))
  {
	THROW_MALERT("AIXMLElementParser_read_string_Invalid_MD5_VALUE_in_FILEDESC_FILENAME",
		AIArgs("[VALUE]", value)("[FILEDESC]", mFileDesc)("[FILENAME]", mFilename));
  }
  unsigned char digest[16];
  std::memset(digest, 0, sizeof(digest));
  for (int i = 0; i < 32; ++i)
  {
    int x = value[i];
    digest[i >> 1] += (x - (x & 0xf0) + (x >> 6) * 9) << ((~i & 1) << 2);
  }
  LLMD5 result;
  result.clone(digest);
  return result;
}

template<>
LLDate AIXMLElementParser::read_string(std::string const& value) const
{
  LLDate result;
  result.fromString(value);
  return result;
}

template<typename T>
T AIXMLElementParser::read_integer(char const* type, std::string const& value) const
{
  long long int result;
  sscanf(value.c_str(),"%lld", &result);
  if (result < (std::numeric_limits<T>::min)() || result > (std::numeric_limits<T>::max)())
  {
	THROW_MALERT("AIXMLElementParser_read_integer_Invalid_TYPE_VALUE_in_FILEDESC_FILENAME",
		AIArgs("[TYPE]", type)("[VALUE]", value)("FILEDESC", mFileDesc)("[FILENAME]", mFilename));
  }
  return result;
}

template<>
U8 AIXMLElementParser::read_string(std::string const& value) const
{
  return read_integer<U8>("U8", value);
}

template<>
S8 AIXMLElementParser::read_string(std::string const& value) const
{
  return read_integer<S8>("S8", value);
}

template<>
U16 AIXMLElementParser::read_string(std::string const& value) const
{
  return read_integer<U16>("U16", value);
}

template<>
S16 AIXMLElementParser::read_string(std::string const& value) const
{
  return read_integer<S16>("S16", value);
}

template<>
U32 AIXMLElementParser::read_string(std::string const& value) const
{
  return read_integer<U32>("U32", value);
}

template<>
S32 AIXMLElementParser::read_string(std::string const& value) const
{
  return read_integer<S32>("S32", value);
}

double read_float(std::string const& value)
{
  double result;
  sscanf(value.c_str(),"%lf", &result);
  return result;
}

template<>
F32 AIXMLElementParser::read_string(std::string const& value) const
{
  return read_float(value);
}

template<>
F64 AIXMLElementParser::read_string(std::string const& value) const
{
  return read_float(value);
}

template<>
bool AIXMLElementParser::read_string(std::string const& value) const
{
  if (value == "true")
  {
	return true;
  }
  else if (value != "false")
  {
	THROW_MALERT("AIXMLElementParser_read_string_Invalid_boolean_VALUE_in_FILEDESC_FILENAME",
		AIArgs("[VALUE]", value)("FILEDESC]", mFileDesc)("[FILENAME]", mFilename));
  }
  return false;
}

void AIXMLElementParser::attribute(char const* name, char const* required_value) const
{
  char const* error = NULL;
  AIArgs args;
  std::string value;
  if (!mNode->getAttributeString(name, value))
  {
	error = "AIXMLElementParser_attribute_Missing_NAME_attribute_in_NODENAME_of_FILEDESC_FILENAME";
  }
  else if (value != required_value)
  {
	error = "AIXMLElementParser_attribute_Invalid_NAME_attribute_should_be_REQUIRED_in_NODENAME_of_FILEDESC_FILENAME";
	args("[REQUIRED]", required_value);
  }
  if (error)
  {
	THROW_MALERT(error, args("[NAME]", name)("[NODENAME]", node_name())("[FILEDESC]", mFileDesc)("[FILENAME]", mFilename));
  }
}

template<>
LLUUID AIXMLElementParser::read_child(LLXmlTreeNode* node) const
{
  LLUUID result;
  if (!LLUUID::parseUUID(node->getContents(), &result))
  {
	THROW_MALERT("AIXMLElementParser_read_child_Invalid_uuid_in_FILEDESC_FILENAME",
		AIArgs("[FILEDESC]", mFileDesc)("[FILENAME]", mFilename));
  }
  return result;
}

template<>
LLMD5 AIXMLElementParser::read_child(LLXmlTreeNode* node) const
{
  return read_string<LLMD5>(node->getContents());
}

template<>
LLDate AIXMLElementParser::read_child(LLXmlTreeNode* node) const
{
  LLDate result;
  if (!result.fromString(node->getContents()))
  {
	THROW_MALERT("AIXMLElementParser_read_child_Invalid_date_DATE_in_FILEDESC_FILENAME",
		AIArgs("[DATE]", node->getContents())("[FILEDESC]", mFileDesc)("[FILENAME]", mFilename));
  }
  return result;
}

template<>
S8 AIXMLElementParser::read_child(LLXmlTreeNode* node) const
{
  return read_integer<S8>("S8", node->getContents());
}

template<>
U8 AIXMLElementParser::read_child(LLXmlTreeNode* node) const
{
  return read_integer<U8>("U8", node->getContents());
}

template<>
S16 AIXMLElementParser::read_child(LLXmlTreeNode* node) const
{
  return read_integer<S16>("S16", node->getContents());
}

template<>
U16 AIXMLElementParser::read_child(LLXmlTreeNode* node) const
{
  return read_integer<U16>("U16", node->getContents());
}

template<>
S32 AIXMLElementParser::read_child(LLXmlTreeNode* node) const
{
  return read_integer<S32>("S32", node->getContents());
}

template<>
U32 AIXMLElementParser::read_child(LLXmlTreeNode* node) const
{
  return read_integer<U32>("U32", node->getContents());
}

template<>
F32 AIXMLElementParser::read_child(LLXmlTreeNode* node) const
{
  return read_string<F32>(node->getContents());
}

template<>
F64 AIXMLElementParser::read_child(LLXmlTreeNode* node) const
{
  return read_string<F64>(node->getContents());
}

template<>
bool AIXMLElementParser::read_child(LLXmlTreeNode* node) const
{
  return read_string<bool>(node->getContents());
}

bool AIXMLElementParser::child(LLUUID& uuid) const
{
  return child(DEFAULT_LLUUID_NAME, uuid);
}

bool AIXMLElementParser::child(LLMD5& md5) const
{
  return child(DEFAULT_MD5STR_NAME, md5);
}

bool AIXMLElementParser::child(LLDate& date) const
{
  return child(DEFAULT_LLDATE_NAME, date);
}

