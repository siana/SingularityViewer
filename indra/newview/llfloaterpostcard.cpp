/** 
 * @file llfloaterpostcard.cpp
 * @brief Postcard send floater, allows setting name, e-mail address, etc.
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 * 
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#include "llfloaterpostcard.h"

#include "llfontgl.h"
#include "llsys.h"
#include "llgl.h"
#include "v3dmath.h"
#include "lldir.h"

#include "llagent.h"
#include "llagentui.h"
#include "llui.h"
#include "lllineeditor.h"
#include "llviewertexteditor.h"
#include "llbutton.h"
#include "llnotificationsutil.h"
#include "llviewercontrol.h"
#include "llviewernetwork.h"
#include "lluictrlfactory.h"
#include "lluploaddialog.h"
#include "llviewerstats.h"
#include "llviewerwindow.h"
#include "llstatusbar.h"
#include "llviewerregion.h"
#include "lltrans.h"

#include "llgl.h"
#include "llglheaders.h"
#include "llimagejpeg.h"
#include "llimagej2c.h"
#include "llvfile.h"
#include "llvfs.h"

#include "llassetuploadresponders.h"

#include "hippogridmanager.h"
#include "llfloatersnapshot.h"

#if LL_MSVC
#pragma warning( disable       : 4265 )	// "class has virtual functions, but destructor is not virtual"
#endif
#include <boost/regex.hpp>  //boost.regex lib

///----------------------------------------------------------------------------
/// Local function declarations, constants, enums, and typedefs
///----------------------------------------------------------------------------

//static
LLFloaterPostcard::instance_list_t LLFloaterPostcard::sInstances;

///----------------------------------------------------------------------------
/// Class LLFloaterPostcard
///----------------------------------------------------------------------------

LLFloaterPostcard::LLFloaterPostcard(LLImageJPEG* jpeg, LLViewerTexture *img, const LLVector2& img_scale, const LLVector3d& pos_taken_global, int index)
:	LLFloater(std::string("Postcard Floater")),
	mJPEGImage(jpeg),
	mViewerImage(img),
	mImageScale(img_scale),
	mPosTakenGlobal(pos_taken_global),
	mHasFirstMsgFocus(false),
	mSnapshotIndex(index)
{
	init();
}

void LLFloaterPostcard::init()
{
	// pick up the user's up-to-date email address
	if(!gAgent.getID().isNull())
	{
		// we're logged in, so we can get this info.
		gAgent.sendAgentUserInfoRequest();
	}

	sInstances.insert(this);
}

// Destroys the object
LLFloaterPostcard::~LLFloaterPostcard()
{
	sInstances.erase(this);
	mJPEGImage = NULL; // deletes image
}

BOOL LLFloaterPostcard::postBuild()
{
	childSetAction("cancel_btn", onClickCancel, this);
	childSetAction("send_btn", onClickSend, this);

	childDisable("from_form");

	std::string name_string;
	LLAgentUI::buildFullname(name_string);
	childSetValue("name_form", LLSD(name_string));

	LLTextEditor* MsgField = getChild<LLTextEditor>("msg_form");
	if (MsgField)
	{
		MsgField->setWordWrap(TRUE);

		// For the first time a user focusess to .the msg box, all text will be selected.
		MsgField->setFocusChangedCallback(boost::bind(&LLFloaterPostcard::onMsgFormFocusRecieved, this, _1, MsgField));
	}
	
	childSetFocus("to_form", TRUE);

    return TRUE;
}



// static
LLFloaterPostcard* LLFloaterPostcard::showFromSnapshot(LLImageJPEG *jpeg, LLViewerTexture *img, const LLVector2 &image_scale, const LLVector3d& pos_taken_global, int index)
{
	// Take the images from the caller
	// It's now our job to clean them up
	LLFloaterPostcard *instance = new LLFloaterPostcard(jpeg, img, image_scale, pos_taken_global, index);

	LLUICtrlFactory::getInstance()->buildFloater(instance, "floater_postcard.xml");

	S32 left, top;
	gFloaterView->getNewFloaterPosition(&left, &top);
	instance->setOrigin(left, top - instance->getRect().getHeight());
	
	instance->open();		/*Flawfinder: ignore*/

	return instance;
}

void LLFloaterPostcard::draw()
{
	LLGLSUIDefault gls_ui;
	LLFloater::draw();

	if(!isMinimized() && mViewerImage.notNull() && mJPEGImage.notNull()) 
	{
		LLRect rect(getRect());

		// first set the max extents of our preview
		rect.translate(-rect.mLeft, -rect.mBottom);
		rect.mLeft += 280;
		rect.mRight -= 10;
		rect.mTop -= 20;
		rect.mBottom = rect.mTop - 130;

		// then fix the aspect ratio
		F32 ratio = (F32)mJPEGImage->getWidth() / (F32)mJPEGImage->getHeight();
		if ((F32)rect.getWidth() / (F32)rect.getHeight() >= ratio)
		{
			rect.mRight = (S32)((F32)rect.mLeft + ((F32)rect.getHeight() * ratio));
		}
		else
		{
			rect.mBottom = (S32)((F32)rect.mTop - ((F32)rect.getWidth() / ratio));
		}
		{
			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
			gl_rect_2d(rect, LLColor4(0.f, 0.f, 0.f, 1.f));
			rect.stretch(-1);
		}
		{

		gGL.matrixMode(LLRender::MM_TEXTURE);
		gGL.pushMatrix();
		{
			gGL.scalef(mImageScale.mV[VX], mImageScale.mV[VY], 1.f);
			gGL.matrixMode(LLRender::MM_MODELVIEW);
			gl_draw_scaled_image(rect.mLeft,
								 rect.mBottom,
								 rect.getWidth(),
								 rect.getHeight(),
								 mViewerImage, 
								 LLColor4::white);
		}
		gGL.matrixMode(LLRender::MM_TEXTURE);
		gGL.popMatrix();
		gGL.matrixMode(LLRender::MM_MODELVIEW);
		}
	}
}

// static
void LLFloaterPostcard::onClickCancel(void* data)
{
	if (data)
	{
		LLFloaterPostcard *self = (LLFloaterPostcard *)data;
		self->close(false);
	}
}

void LLFloaterPostcard::onClose(bool app_quitting)
{
	LLFloaterSnapshot::savePostcardDone(false, mSnapshotIndex);
	destroy();
}

class LLSendPostcardResponder final : public LLAssetUploadResponder
{
private:
	int mSnapshotIndex;

public:
	LLSendPostcardResponder(const LLSD &post_data,
							const LLUUID& vfile_id,
							LLAssetType::EType asset_type,
							int index) :
	    LLAssetUploadResponder(post_data, vfile_id, asset_type),
		mSnapshotIndex(index)
	{	
	}
	// *TODO define custom uploadFailed here so it's not such a generic message
	void uploadComplete(const LLSD& content) override final
	{
		// we don't care about what the server returns from this post, just clean up the UI
		LLFloaterSnapshot::savePostcardDone(true, mSnapshotIndex);
	}
	void uploadFailure(const LLSD& content) override final
	{
		LLAssetUploadResponder::uploadFailure(content);
		LLFloaterSnapshot::savePostcardDone(false, mSnapshotIndex);
	}
	void httpFailure(void) override final
	{
		LLAssetUploadResponder::httpFailure();
		LLFloaterSnapshot::savePostcardDone(false, mSnapshotIndex);
	}
	char const* getName(void) const override final { return "LLSendPostcardResponder"; }
};

// static
void LLFloaterPostcard::onClickSend(void* data)
{
	if (data)
	{
		LLFloaterPostcard *self = (LLFloaterPostcard *)data;

		std::string to(self->childGetValue("to_form").asString());
		
		boost::regex emailFormat("[A-Za-z0-9.%+-_]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}(,[ \t]*[A-Za-z0-9.%+-_]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,})*");
		
		if (to.empty() || !boost::regex_match(to, emailFormat))
		{
			LLNotificationsUtil::add("PromptRecipientEmail");
			return;
		}

		std::string subject(self->childGetValue("subject_form").asString());
		if(subject.empty() || !self->mHasFirstMsgFocus)
		{
			LLNotificationsUtil::add("PromptMissingSubjMsg", LLSD(), LLSD(), boost::bind(&LLFloaterPostcard::missingSubjMsgAlertCallback, self, _1, _2));
			return;
		}

		if (self->mJPEGImage.notNull())
		{
			self->sendPostcard();
		}
		else
		{
			LLNotificationsUtil::add("ErrorProcessingSnapshot");
		}
	}
}

// static
void LLFloaterPostcard::uploadCallback(const LLUUID& asset_id, void *user_data, S32 result, LLExtStat ext_status) // StoreAssetData callback (fixed)
{
	LLFloaterPostcard *self = (LLFloaterPostcard *)user_data;
	
	LLFloaterSnapshot::savePostcardDone(!result, self->mSnapshotIndex);

	if (result)
	{
		LLSD args;
		args["REASON"] = std::string(LLAssetStorage::getErrorString(result));
		LLNotificationsUtil::add("ErrorUploadingPostcard", args);
	}
	else
	{
		// only create the postcard once the upload succeeds

		// request the postcard
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessage("SendPostcard");
		msg->nextBlock("AgentData");
		msg->addUUID("AgentID", gAgent.getID());
		msg->addUUID("SessionID", gAgent.getSessionID());
		msg->addUUID("AssetID", self->mAssetID);
		msg->addVector3d("PosGlobal", self->mPosTakenGlobal);
		msg->addString("To", self->childGetValue("to_form").asString());
		msg->addString("From", self->childGetValue("from_form").asString());
		msg->addString("Name", self->childGetValue("name_form").asString());
		msg->addString("Subject", self->childGetValue("subject_form").asString());
		msg->addString("Msg", self->childGetValue("msg_form").asString());
		msg->addBOOL("AllowPublish", FALSE);
		msg->addBOOL("MaturePublish", FALSE);
		gAgent.sendReliableMessage();
	}

	self->destroy();
}

// static
void LLFloaterPostcard::updateUserInfo(const std::string& email)
{
	for (auto& instance : sInstances)
	{
		const std::string& text = instance->childGetValue("from_form").asString();
		if (text.empty())
		{
			// there's no text in this field yet, pre-populate
			instance->childSetValue("from_form", LLSD(email));
		}
	}
}

void LLFloaterPostcard::onMsgFormFocusRecieved(LLFocusableElement* receiver, LLTextEditor* msg_form)
{
	if(msg_form && msg_form == receiver && msg_form->hasFocus() && !(mHasFirstMsgFocus))
	{
		mHasFirstMsgFocus = true;
		msg_form->setText(LLStringUtil::null);
	}
}

bool LLFloaterPostcard::missingSubjMsgAlertCallback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if(0 == option)
	{
		// User clicked OK
		if((childGetValue("subject_form").asString()).empty())
		{
			// Stuff the subject back into the form.
			LLStringUtil::format_map_t targs;
			targs["[SECOND_LIFE]"] = LLTrans::getString("SECOND_LIFE");
			std::string subj = getString("default_subject");
			LLStringUtil::format(subj, targs);

			childSetValue("subject_form", subj);
		}

		if(!mHasFirstMsgFocus)
		{
			// The user never switched focus to the messagee window. 
			// Using the default string.
			childSetValue("msg_form", getString("default_message"));
		}

		sendPostcard();
	}
	return false;
}

void LLFloaterPostcard::sendPostcard()
{
	mTransactionID.generate();
	mAssetID = mTransactionID.makeAssetID(gAgent.getSecureSessionID());
	LLVFile::writeFile(mJPEGImage->getData(), mJPEGImage->getDataSize(), gVFS, mAssetID, LLAssetType::AT_IMAGE_JPEG);

	// upload the image
	LLFloaterSnapshot::saveStart(mSnapshotIndex);
	std::string url = gAgent.getRegion()->getCapability("SendPostcard");
	if(!url.empty())
	{
		LL_INFOS() << "Send Postcard via capability" << LL_ENDL;
		LLSD body = LLSD::emptyMap();
		// the capability already encodes: agent ID, region ID
		body["pos-global"] = mPosTakenGlobal.getValue();
		body["to"] = childGetValue("to_form").asString();
		body["name"] = childGetValue("name_form").asString();
		body["subject"] = childGetValue("subject_form").asString();
		body["msg"] = childGetValue("msg_form").asString();
		LLHTTPClient::post(url, body, new LLSendPostcardResponder(body, mAssetID, LLAssetType::AT_IMAGE_JPEG, mSnapshotIndex));
	} 
	else
	{
		gAssetStorage->storeAssetData(mTransactionID, LLAssetType::AT_IMAGE_JPEG, &uploadCallback, (void *)this, FALSE);
	}
	
	// give user feedback of the event
	gViewerWindow->playSnapshotAnimAndSound();
	LLUploadDialog::modalUploadDialog(getString("upload_message"));

	// don't destroy the window until the upload is done
	// this way we keep the information in the form
	setVisible(FALSE);
}
