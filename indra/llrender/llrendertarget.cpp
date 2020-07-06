/** 
 * @file llrendertarget.cpp
 * @brief LLRenderTarget implementation
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

#include "linden_common.h"

#include "llrendertarget.h"
#include "llrender.h"
#include "llgl.h"

LLRenderTarget* LLRenderTarget::sBoundTarget = NULL;
U32 LLRenderTarget::sBytesAllocated = 0;

void check_framebuffer_status()
{
	if (gDebugGL)
	{
		GLenum status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
		switch (status)
		{
		case GL_FRAMEBUFFER_COMPLETE:
			break;
		default:
			LL_WARNS() << "check_framebuffer_status failed -- " << std::hex << status << std::dec << LL_ENDL;
			ll_fail("check_framebuffer_status failed");	
			break;
		}
	}
}

bool LLRenderTarget::sUseFBO = false;
LLRenderTarget* LLRenderTarget::sCurFBO = 0;

LLRenderTarget::LLRenderTarget() :
	mResX(0),
	mResY(0),
	mFBO(0),
	mPreviousFBO(0),
	mDepth(0),
	mStencil(0),
	mUseDepth(false),
	mRenderDepth(false),
	mUsage(LLTexUnit::TT_TEXTURE),
	mSampleBuffer(NULL)
{
}

LLRenderTarget::~LLRenderTarget()
{
	release();
}

void LLRenderTarget::resize(U32 resx, U32 resy)
{
	//for accounting, get the number of pixels added/subtracted
	S32 pix_diff = (resx*resy)-(mResX*mResY);

	mResX = resx;
	mResY = resy;

	llassert(mInternalFormat.size() == mTex.size());

	for (U32 i = 0; i < mTex.size(); ++i)
	{ //resize color attachments
		gGL.getTexUnit(0)->bindManual(mUsage, mTex[i]);
		LLImageGL::setManualImage(LLTexUnit::getInternalType(mUsage), 0, mInternalFormat[i], mResX, mResY, GL_RGBA, GL_UNSIGNED_BYTE);
		sBytesAllocated += pix_diff*4;
	}

	if (mDepth)
	{ //resize depth attachment
		if (mStencil && mFBO)
		{
			//use render buffers where stencil buffers are in play
			glBindRenderbuffer(GL_RENDERBUFFER, mDepth);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, mResX, mResY);
			glBindRenderbuffer(GL_RENDERBUFFER, 0);
		}
		else
		{
			gGL.getTexUnit(0)->bindManual(mUsage, mDepth);
			U32 internal_type = LLTexUnit::getInternalType(mUsage);
			if(!mStencil)
				LLImageGL::setManualImage(internal_type, 0, GL_DEPTH_COMPONENT24, mResX, mResY, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT);
			else
				LLImageGL::setManualImage(internal_type, 0, GL_DEPTH24_STENCIL8, mResX, mResY, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8);
		}

		sBytesAllocated += pix_diff*4;
	}
	if(mSampleBuffer)
		mSampleBuffer->resize(resx,resy);
}

void LLRenderTarget::setSampleBuffer(LLMultisampleBuffer* buffer)
{
	mSampleBuffer = buffer;
}

bool LLRenderTarget::allocate(U32 resx, U32 resy, U32 color_fmt, bool depth, bool stencil, LLTexUnit::eTextureType usage, bool use_fbo)
{
	resx = llmin(resx, (U32) 4096);
	resy = llmin(resy, (U32) 4096);

	stop_glerror();
	release();
	stop_glerror();

	mResX = resx;
	mResY = resy;

	mStencil = stencil;
	mUsage = usage;
	mUseDepth = depth;

	if ((sUseFBO || use_fbo) && gGLManager.mHasFramebufferObject)
	{
		glGenFramebuffers(1, (GLuint *) &mFBO);

		if (depth)
		{
			if (!allocateDepth())
			{
				LL_WARNS() << "Failed to allocate depth buffer for render target." << LL_ENDL;
				return false;
			}
		}

		if (mDepth)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
			if (mStencil)
			{
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mDepth);
				stop_glerror();
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, mDepth);
				stop_glerror();
			}
			else
			{
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, LLTexUnit::getInternalType(mUsage), mDepth, 0);
				stop_glerror();
			}
			glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO ? sCurFBO->getFBO() : 0);
		}

		stop_glerror();
	}

	return addColorAttachment(color_fmt);
}

bool LLRenderTarget::addColorAttachment(U32 color_fmt)
{
	if (color_fmt == 0)
	{
		return true;
	}

	U32 offset = mTex.size();

	if( offset >= 4 )
	{
		LL_WARNS() << "Too many color attachments" << LL_ENDL;
		llassert( offset < 4 );
		return false;
	}
	if( offset > 0 && (mFBO == 0 || !gGLManager.mHasDrawBuffers) )
	{
		LL_WARNS() << "FBO not used or no drawbuffers available; mFBO=" << (U32)mFBO << " gGLManager.mHasDrawBuffers=" << (U32)gGLManager.mHasDrawBuffers << LL_ENDL;
		llassert(  mFBO != 0 );
		llassert( gGLManager.mHasDrawBuffers );
		return false;
	}

	auto texName = LLImageGL::createTextureName();
	U32 tex = texName->getTexName();
	gGL.getTexUnit(0)->bindManual(mUsage, tex);

	stop_glerror();


	{
		clear_glerror();
		LLImageGL::setManualImage(LLTexUnit::getInternalType(mUsage), 0, color_fmt, mResX, mResY, GL_RGBA, GL_UNSIGNED_BYTE);
		if (glGetError() != GL_NO_ERROR)
		{
			LL_WARNS() << "Could not allocate color buffer for render target." << LL_ENDL;
			return false;
		}
	}
	
	sBytesAllocated += mResX*mResY*4;

	stop_glerror();

	
	if (offset == 0)
	{ //use bilinear filtering on single texture render targets that aren't multisampled
		gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
		stop_glerror();
	}
	else
	{ //don't filter data attachments
		gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
		stop_glerror();
	}

	gGL.getTexUnit(0)->setTextureAddressMode(LLTexUnit::TAM_MIRROR);
	stop_glerror();

	if (mFBO)
	{
		stop_glerror();
		glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0+offset,
			LLTexUnit::getInternalType(mUsage), tex, 0);
			stop_glerror();

		check_framebuffer_status();
		
		glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO ? sCurFBO->getFBO() : 0);
	}

	mTex.push_back(tex);
	mTexName.push_back(texName);
	mInternalFormat.push_back(color_fmt);

	if (gDebugGL)
	{ //bind and unbind to validate target
		bindTarget();
		flush();
	}

	return true;
}

bool LLRenderTarget::allocateDepth()
{
	mDepthName.reset();
	if (mStencil && mFBO)
	{
		//use render buffers where stencil buffers are in play
		glGenRenderbuffers(1, (GLuint *) &mDepth);
		glBindRenderbuffer(GL_RENDERBUFFER, mDepth);
		stop_glerror();
		clear_glerror();
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, mResX, mResY);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
	}
	else
	{
		mDepthName = LLImageGL::createTextureName();
		mDepth = mDepthName->getTexName();

		gGL.getTexUnit(0)->bindManual(mUsage, mDepth);
		
		U32 internal_type = LLTexUnit::getInternalType(mUsage);
		stop_glerror();
		clear_glerror();
		if(!mStencil)
			LLImageGL::setManualImage(internal_type, 0, GL_DEPTH_COMPONENT24, mResX, mResY, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT);
		else
			LLImageGL::setManualImage(internal_type, 0, GL_DEPTH24_STENCIL8, mResX, mResY, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8);
		gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
	}

	if (glGetError() != GL_NO_ERROR)
	{
		LL_WARNS() << "Unable to allocate depth buffer for render target." << LL_ENDL;
		return false;
	}

	sBytesAllocated += mResX*mResY*4;

	return true;
}

void LLRenderTarget::shareDepthBuffer(LLRenderTarget& target)
{
	if (!mFBO || !target.mFBO)
	{
		LL_ERRS() << "Cannot share depth buffer between non FBO render targets." << LL_ENDL;
	}

	if (target.mDepth)
	{
		LL_ERRS() << "Attempting to override existing depth buffer.  Detach existing buffer first." << LL_ENDL;
	}

	if (target.mUseDepth)
	{
		LL_ERRS() << "Attempting to override existing shared depth buffer. Detach existing buffer first." << LL_ENDL;
	}

	if (mDepth)
	{
		stop_glerror();
		glBindFramebuffer(GL_FRAMEBUFFER, target.mFBO);
		stop_glerror();

		if (mStencil)
		{
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mDepth);
			stop_glerror();
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, mDepth);			
			stop_glerror();
			target.mStencil = true;
		}
		else
		{
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, LLTexUnit::getInternalType(mUsage), mDepth, 0);
			stop_glerror();
		}

		check_framebuffer_status();

		glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO ? sCurFBO->getFBO() : 0);

		target.mUseDepth = true;
	}
}

void LLRenderTarget::release()
{
	if (mDepth)
	{
		if (mStencil && mFBO)
		{
			glDeleteRenderbuffers(1, (GLuint*) &mDepth);
			stop_glerror();
		}
		else
		{
			//Release before delete.
			if(mFBO)
			{
				glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, LLTexUnit::getInternalType(mUsage), 0, 0);
				glBindFramebuffer(GL_FRAMEBUFFER,0);
			}
			stop_glerror();
		}
		mDepth = 0;

		sBytesAllocated -= mResX*mResY*4;
	}
	else if (mUseDepth && mFBO)
	{ //detach shared depth buffer
		glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
		if (mStencil)
		{ //attached as a renderbuffer
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
			mStencil = false;
		}
		else
		{ //attached as a texture
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, LLTexUnit::getInternalType(mUsage), 0, 0);
		}
		glBindFramebuffer(GL_FRAMEBUFFER,0);
		mUseDepth = false;
	}

	if (mFBO)
	{
		glDeleteFramebuffers(1, (GLuint *) &mFBO);
		mFBO = 0;
	}

	sBytesAllocated -= mResX * mResY * 4 * mTex.size();
	mTex.clear();
	mInternalFormat.clear();
	mDepthName.reset();
	mTexName.clear(); // Were textures not being released at all..?

	mResX = mResY = 0;

	mSampleBuffer = NULL;
	sBoundTarget = NULL;
}

void LLRenderTarget::bindTarget()
{
	if (mFBO)
	{
		stop_glerror();

		mPreviousFBO = sCurFBO;
		if (mSampleBuffer)
		{
			mSampleBuffer->bindTarget(this);
			sCurFBO = mSampleBuffer;
			stop_glerror();
		}
		else
		{
			glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
			sCurFBO = this;

			stop_glerror();
			if (gGLManager.mHasDrawBuffers)
			{ //setup multiple render targets
				GLenum drawbuffers[] = {GL_COLOR_ATTACHMENT0,
										GL_COLOR_ATTACHMENT1,
										GL_COLOR_ATTACHMENT2,
										GL_COLOR_ATTACHMENT3};
				glDrawBuffersARB(mTex.size(), drawbuffers);
			}
			
			if (mTex.empty())
			{ //no color buffer to draw to
				glDrawBuffer(GL_NONE);
				glReadBuffer(GL_NONE);
			}

			check_framebuffer_status();

			stop_glerror();
		}
	}
	gGL.setViewport(0, 0, mResX, mResY);
	sBoundTarget = this;
}

void LLRenderTarget::clear(U32 mask_in)
{
	U32 mask = GL_COLOR_BUFFER_BIT;
	if (mUseDepth)
	{
		mask |= GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
	}

	if (mFBO)
	{
		check_framebuffer_status();
		stop_glerror();
		LLGLDisable<GL_SCISSOR_TEST> scissor;
		gGL.syncContextState();
		glClear(mask & mask_in);
		stop_glerror();
	}
	else
	{
		LLGLEnable<GL_SCISSOR_TEST> scissor;
		gGL.setScissor(0, 0, mResX, mResY);
		stop_glerror();
		gGL.syncContextState();
		glClear(mask & mask_in);
	}
}

U32 LLRenderTarget::getTexture(U32 attachment) const
{
	if (attachment > mTex.size()-1)
	{
		LL_ERRS() << "Invalid attachment index." << LL_ENDL;
	}
	if (mTex.empty())
	{
		return 0;
	}
	return mTex[attachment];
}

void LLRenderTarget::bindTexture(U32 index, S32 channel)
{
	gGL.getTexUnit(channel)->bindManual(mUsage, getTexture(index));
}

void LLRenderTarget::flush(bool fetch_depth)
{
	gGL.flush();
	if (!mFBO)
	{
		gGL.getTexUnit(0)->bind(this);
		glCopyTexSubImage2D(LLTexUnit::getInternalType(mUsage), 0, 0, 0, 0, 0, mResX, mResY);
		stop_glerror();

		if (fetch_depth)
		{
			if (!mDepth)
			{
				allocateDepth();
			}

			gGL.getTexUnit(0)->bind(this,true);
			glCopyTexSubImage2D(LLTexUnit::getInternalType(mUsage), 0, 0, 0, 0, 0, mResX, mResY);
			stop_glerror();
			//glCopyTexImage2D(LLTexUnit::getInternalType(mUsage), 0, GL_DEPTH24_STENCIL8, 0, 0, mResX, mResY, 0);
		}

		gGL.getTexUnit(0)->disable();
	}
	else
	{
		stop_glerror();
		if(mSampleBuffer)
		{
			LLGLEnable<GL_MULTISAMPLE> multisample;
			stop_glerror();
			glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
			stop_glerror();
			check_framebuffer_status();
			glBindFramebuffer(GL_READ_FRAMEBUFFER, mSampleBuffer->mFBO);
			check_framebuffer_status();

			stop_glerror();
			if(gGLManager.mIsATI)
			{
				glBlitFramebuffer(0, 0, mResX, mResY, 0, 0, mResX, mResY, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
				glBlitFramebuffer(0, 0, mResX, mResY, 0, 0, mResX, mResY, GL_STENCIL_BUFFER_BIT, GL_NEAREST);
			}
			else
			{
				glBlitFramebuffer(0, 0, mResX, mResY, 0, 0, mResX, mResY, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
			}
			stop_glerror();		

			//Following case never currently evalutes true, but it's still good to have.
			if (mTex.size() > 1)
			{
				for (U32 i = 1; i < mTex.size(); ++i)
				{
					glDrawBuffer(GL_COLOR_ATTACHMENT0 + i);
					glReadBuffer(GL_COLOR_ATTACHMENT0 + i);
					stop_glerror();
					glBlitFramebuffer(0, 0, mResX, mResY, 0, 0, mResX, mResY, GL_COLOR_BUFFER_BIT, GL_NEAREST);
					stop_glerror();
				}

				/*for (U32 i = 1; i < mTex.size(); ++i)
				{

					glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
										LLTexUnit::getInternalType(mUsage), mTex[i], 0);
					stop_glerror();
					glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, mSampleBuffer->mTex[i]);
					stop_glerror();
					glBlitFramebuffer(0, 0, mResX, mResY, 0, 0, mResX, mResY, GL_COLOR_BUFFER_BIT, GL_NEAREST);		
					stop_glerror();
				}

				for (U32 i = 0; i < mTex.size(); ++i)
				{
					glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0+i,
										LLTexUnit::getInternalType(mUsage), mTex[i], 0);
					stop_glerror();
					glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0+i, GL_RENDERBUFFER, mSampleBuffer->mTex[i]);
					stop_glerror();
				}*/
			}
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		}
		glBindFramebuffer(GL_FRAMEBUFFER, mPreviousFBO ? mPreviousFBO->getFBO() : 0);
		sCurFBO = mPreviousFBO;

		if(mPreviousFBO)
		{
			gGL.setViewport(0, 0, mPreviousFBO->mResX, mPreviousFBO->mResY);
			mPreviousFBO = NULL;
		}
		else
		{
			gGL.setViewport(gGLViewport);
		}
		stop_glerror();
	}
}

void LLRenderTarget::copyContents(LLRenderTarget& source, S32 srcX0, S32 srcY0, S32 srcX1, S32 srcY1,
						S32 dstX0, S32 dstY0, S32 dstX1, S32 dstY1, U32 mask, U32 filter)
{
	GLboolean write_depth = mask & GL_DEPTH_BUFFER_BIT ? TRUE : FALSE;

	LLGLDepthTest depth(write_depth, write_depth);

	gGL.flush();
	if (!source.mFBO || !mFBO)
	{
		LL_WARNS() << "Cannot copy framebuffer contents for non FBO render targets." << LL_ENDL;
		return;
	}

	if (mSampleBuffer)
	{
		mSampleBuffer->copyContents(source, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
	}
	else
	{
		if (mask == GL_DEPTH_BUFFER_BIT && !mStencil && source.mStencil != mStencil)
		{
			stop_glerror();
		
			glBindFramebuffer(GL_FRAMEBUFFER, source.mFBO);
			check_framebuffer_status();
			gGL.getTexUnit(0)->bind(this, true);
			stop_glerror();
			glCopyTexSubImage2D(LLTexUnit::getInternalType(mUsage), 0, srcX0, srcY0, dstX0, dstY0, dstX1, dstY1);
			stop_glerror();
			glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO ? sCurFBO->getFBO() : 0);
			stop_glerror();
		}
		else
		{
			glBindFramebuffer(GL_READ_FRAMEBUFFER, source.mFBO);
			stop_glerror();
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mFBO);
			stop_glerror();
			check_framebuffer_status();
			stop_glerror();
			if(gGLManager.mIsATI && mask & GL_STENCIL_BUFFER_BIT)
			{
				mask &= ~GL_STENCIL_BUFFER_BIT;
				glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, GL_STENCIL_BUFFER_BIT, filter);
			}
			if(mask)
				glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
			stop_glerror();
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			stop_glerror();
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			stop_glerror();
			glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO ? sCurFBO->getFBO() : 0);
			stop_glerror();
		}
	}
}

//static
void LLRenderTarget::copyContentsToFramebuffer(LLRenderTarget& source, S32 srcX0, S32 srcY0, S32 srcX1, S32 srcY1,
						S32 dstX0, S32 dstY0, S32 dstX1, S32 dstY1, U32 mask, U32 filter)
{
	if (!source.mFBO)
	{
		LL_WARNS() << "Cannot copy framebuffer contents for non FBO render targets." << LL_ENDL;
		return;
	}
	{
		GLboolean write_depth = mask & GL_DEPTH_BUFFER_BIT ? TRUE : FALSE;

		LLGLDepthTest depth(write_depth, write_depth);
		
		glBindFramebuffer(GL_READ_FRAMEBUFFER, source.mSampleBuffer ? source.mSampleBuffer->mFBO : source.mFBO);
		stop_glerror();
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		stop_glerror();
		check_framebuffer_status();
		stop_glerror();
		if(gGLManager.mIsATI && mask & GL_STENCIL_BUFFER_BIT)
		{
			mask &= ~GL_STENCIL_BUFFER_BIT;
			glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, GL_STENCIL_BUFFER_BIT, filter);
		}
		if(mask)
			glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
		stop_glerror();
		glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO ? sCurFBO->getFBO() : 0);
		stop_glerror();
	}
}

bool LLRenderTarget::isComplete() const
{
	return (!mTex.empty() || mDepth) ? true : false;
}

void LLRenderTarget::getViewport(LLRect& viewport)
{
	viewport = LLRect(0, mResY, mResX, 0);
}

//==================================================
// LLMultisampleBuffer implementation
//==================================================
LLMultisampleBuffer::LLMultisampleBuffer() :
	mSamples(0),
	mColorFormat(0)
{

}

LLMultisampleBuffer::~LLMultisampleBuffer()
{
	release();
}

void LLMultisampleBuffer::release()
{
	if (mFBO)
	{
		glDeleteFramebuffers(1, (GLuint *) &mFBO);
		mFBO = 0;
	}

	if (mTex.size() > 0)
	{
		sBytesAllocated -= mResX*mResY*4*mTex.size();
		glDeleteRenderbuffers(mTex.size(), (GLuint *) &mTex[0]);
		mTex.clear();
		
	}

	if (mDepth)
	{
		sBytesAllocated -= mResX*mResY*4;
		glDeleteRenderbuffers(1, (GLuint *) &mDepth);
		mDepth = 0;
	}
}

void LLMultisampleBuffer::bindTarget()
{
	bindTarget(this);
}

void LLMultisampleBuffer::bindTarget(LLRenderTarget* ref)
{
	if (!ref)
	{
		ref = this;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
	if (gGLManager.mHasDrawBuffers)
	{ //setup multiple render targets
		GLenum drawbuffers[] = {GL_COLOR_ATTACHMENT0,
								GL_COLOR_ATTACHMENT1,
								GL_COLOR_ATTACHMENT2,
								GL_COLOR_ATTACHMENT3};
		glDrawBuffersARB(ref->mTex.size(), drawbuffers);
	}

	check_framebuffer_status();

	gGL.setViewport(0, 0, mResX, mResY);

	sBoundTarget = this;
}

bool LLMultisampleBuffer::allocate(U32 resx, U32 resy, U32 color_fmt, bool depth, bool stencil,  LLTexUnit::eTextureType usage, bool use_fbo )
{
	return allocate(resx,resy,color_fmt,depth,stencil,usage,use_fbo,2);
}

bool LLMultisampleBuffer::allocate(U32 resx, U32 resy, U32 color_fmt, bool depth, bool stencil,  LLTexUnit::eTextureType usage, bool use_fbo, U32 samples )
{
	release();
	stop_glerror();

	if (!gGLManager.mHasFramebufferMultisample || !gGLManager.mHasFramebufferObject || !(sUseFBO || use_fbo))
		return false;

	if(color_fmt != GL_RGBA)
	{
		LL_WARNS() << "Unsupported color format: " << color_fmt << LL_ENDL;
		return false;
	}

	//Restrict to valid sample count
	{
		mSamples = samples;
		//mSamples = llmin(mSamples, (U32)4);	//Cap to prevent memory bloat.
		mSamples = llmin(mSamples, (U32) gGLManager.mMaxSamples);
	}

	if (mSamples <= 1)
		return false;
	
	mResX = resx;
	mResY = resy;

	mUsage = usage;
	mUseDepth = depth;
	mStencil = stencil;
	mColorFormat = color_fmt;

	{

		if (depth)
		{
			stop_glerror();
			if(!allocateDepth())
			{
				release();
				return false;
			}
			stop_glerror();
		}
		glGenFramebuffers(1, (GLuint *) &mFBO);
		glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
		stop_glerror();
		clear_glerror();
		if (mDepth)
		{
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mDepth);
			if (mStencil)
			{
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, mDepth);			
			}
		}
		
		stop_glerror();
		glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO ? sCurFBO->getFBO() : 0);
		stop_glerror();
		
		
	}

	return addColorAttachment(color_fmt);
}

void LLMultisampleBuffer::resize(U32 resx, U32 resy)
{
	//for accounting, get the number of pixels added/subtracted
	S32 pix_diff = (resx*resy)-(mResX*mResY);
		
	mResX = resx;
	mResY = resy;

	for (U32 i = 0; i < mTex.size(); ++i)
	{ //resize color attachments
		glBindRenderbuffer(GL_RENDERBUFFER, mTex[i]);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, mSamples, mColorFormat, mResX, mResY);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
		sBytesAllocated += pix_diff*4;
	}

	if (mDepth)
	{ //resize depth attachment
		if (mStencil)
		{
			//use render buffers where stencil buffers are in play
			glBindRenderbuffer(GL_RENDERBUFFER, mDepth);
			glRenderbufferStorageMultisample(GL_RENDERBUFFER, mSamples, GL_DEPTH24_STENCIL8, mResX, mResY);
			glBindRenderbuffer(GL_RENDERBUFFER, 0);
		}
		else
		{
			glBindRenderbuffer(GL_RENDERBUFFER, mDepth);
			glRenderbufferStorageMultisample(GL_RENDERBUFFER, mSamples, GL_DEPTH_COMPONENT24, mResX, mResY);
			glBindRenderbuffer(GL_RENDERBUFFER, 0);
			
		}
		sBytesAllocated += pix_diff*4;
	}
}

bool LLMultisampleBuffer::addColorAttachment(U32 color_fmt)
{
	if (color_fmt == 0)
	{
		return true;
	}

	U32 offset = mTex.size();
	if (offset >= 4 ||
		(offset > 0 && (mFBO == 0 || !gGLManager.mHasDrawBuffers)))
	{
		LL_ERRS() << "Too many color attachments!" << LL_ENDL;
	}

	U32 tex;
	glGenRenderbuffers(1, &tex);
	glBindRenderbuffer(GL_RENDERBUFFER, tex);
	stop_glerror();
	clear_glerror();
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, mSamples, color_fmt, mResX, mResY);
	if (glGetError() != GL_NO_ERROR)
	{
		LL_WARNS() << "Unable to allocate color buffer for multisample render target." << LL_ENDL;
		release();
		return false;
	}
	
	sBytesAllocated += mResX*mResY*4;
	
	if (mFBO)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0+offset, GL_RENDERBUFFER, tex);
		check_framebuffer_status();
		glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO ? sCurFBO->getFBO() : 0);
	}

	mTex.push_back(tex);
	return true;
}

bool LLMultisampleBuffer::allocateDepth()
{
	glGenRenderbuffers(1, (GLuint* ) &mDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, mDepth);
	stop_glerror();
	clear_glerror();
	if (mStencil)
	{
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, mSamples, GL_DEPTH24_STENCIL8, mResX, mResY);
	}
	else
	{
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, mSamples, GL_DEPTH_COMPONENT24, mResX, mResY);
	}

	if (glGetError() != GL_NO_ERROR)
	{
		LL_WARNS() << "Unable to allocate depth buffer for multisample render target." << LL_ENDL;
		return false;
	}
	
	sBytesAllocated += mResX*mResY*4;
	
	return true;
}
