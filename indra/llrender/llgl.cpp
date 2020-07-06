/** 
 * @file llgl.cpp
 * @brief LLGL implementation
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

// This file sets some global GL parameters, and implements some 
// useful functions for GL operations.

#define GLH_EXT_SINGLE_FILE

#include "linden_common.h"

#include "boost/tokenizer.hpp"

#include "llsys.h"

#include "llgl.h"
#include "llrender.h"

#include "llerror.h"
#include "llerrorcontrol.h"
#include "llquaternion.h"
#include "llmath.h"
#include "m4math.h"
#include "llstring.h"
#include "llstacktrace.h"

#include "llglheaders.h"
#include "llglslshader.h"

#ifdef _DEBUG
//#define GL_STATE_VERIFY
#endif


BOOL gDebugSession = FALSE;
BOOL gDebugGL = FALSE;
BOOL gClothRipple = FALSE;
BOOL gNoRender = FALSE;
BOOL gGLActive = FALSE;

std::ofstream gFailLog;

#if !LL_DARWIN //Darwin doesn't load extensions that way! -SG
void* gl_get_proc_address(const char *pStr)
{
	void* pPtr = (void*)GLH_EXT_GET_PROC_ADDRESS(pStr);
	if(!pPtr)
		LL_INFOS() << "Failed to find symbol '" << pStr << "'" << LL_ENDL;
	return pPtr;
}
#undef GLH_EXT_GET_PROC_ADDRESS
#define GLH_EXT_GET_PROC_ADDRESS(p)   gl_get_proc_address(p) 
#undef GLH_EXT_GET_PROC_ADDRESS_CORE
#define GLH_EXT_GET_PROC_ADDRESS_CORE(ver, p)   gl_get_proc_address((mGLVersion >= ver) ? p : p"ARB")
#undef GLH_EXT_GET_PROC_ADDRESS_CORE_EXT
#define GLH_EXT_GET_PROC_ADDRESS_CORE_EXT(ver, p)   gl_get_proc_address((mGLVersion >= ver) ? p : p"EXT")
#undef GLH_EXT_GET_PROC_ADDRESS_CORE_OR_ARB
#define GLH_EXT_GET_PROC_ADDRESS_CORE_OR_ARB(ver, p, arb)   gl_get_proc_address((mGLVersion >= ver) ? p : arb)
#endif //!LL_DARWIN

#if GL_ARB_debug_output

#ifndef APIENTRY
#define APIENTRY
#endif

void APIENTRY gl_debug_callback(GLenum source,
                                GLenum type,
                                GLuint id,
                                GLenum severity,
                                GLsizei length,
                                const GLchar* message,
                                GLvoid* userParam)
{
	if (severity == GL_DEBUG_SEVERITY_HIGH_ARB)
	{
		LL_WARNS() << "----- GL ERROR --------" << LL_ENDL;
	}
	else
	{
		if (severity == GL_DEBUG_SEVERITY_MEDIUM_ARB)
		{
			LL_WARNS() << "----- GL WARNING MEDIUM --------" << LL_ENDL;
		}
		else if (severity == GL_DEBUG_SEVERITY_LOW_ARB)
		{
			if (type == GL_DEBUG_TYPE_OTHER_ARB)
			{
				if (id == 0x20004 || // Silence nvidia glClear noop messages
					id == 0x20043 || // Silence nvidia CSAA messages.
					id == 0x20084	 // Silence nvidia texture mapping with no base level messages.
					)
				{
					return;
				}
			}

			LL_WARNS() << "----- GL WARNING LOW --------" << LL_ENDL;
		}
		else if (severity == 0x826b && id == 0x20071 && type == GL_DEBUG_TYPE_OTHER_ARB && source == GL_DEBUG_SOURCE_API_ARB)
		{
			// Silence nvidia buffer detail info.
			return;
		}
	}

	std::string sourcestr = "Unknown";
	switch (source)
	{
	case GL_DEBUG_SOURCE_API_ARB:
		sourcestr = "OpenGL";
		break;
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB:
		sourcestr = "Window manager";
		break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB:
		sourcestr = "Shader compiler";
		break;
	case GL_DEBUG_SOURCE_THIRD_PARTY_ARB:
		sourcestr = "3rd party";
		break;
	case GL_DEBUG_SOURCE_APPLICATION_ARB:
		sourcestr = "Application";
		break;
	case GL_DEBUG_SOURCE_OTHER_ARB:
		sourcestr = "Other";
		break;
	default:
		break;
	}

	std::string typestr = "Unknown";
	switch (type)
	{
	case GL_DEBUG_TYPE_ERROR_ARB:
		typestr = "Error";
		break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB:
		typestr = "Deprecated behavior";
		break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:
		typestr = "Undefined behavior";
		break;
	case GL_DEBUG_TYPE_PORTABILITY_ARB:
		typestr = "Portability";
		break;
	case GL_DEBUG_TYPE_PERFORMANCE_ARB:
		typestr = "Performance";
		break;
	case GL_DEBUG_TYPE_OTHER_ARB:
		typestr = "Other";
		break;
	default:
		break;
	}

	LL_WARNS() << "Source: " << sourcestr << " (" << std::hex << source << std::dec << ")" << LL_ENDL;
	LL_WARNS() << "Type: " << typestr << " (" << std::hex << type << std::dec << ")" << LL_ENDL;
	LL_WARNS() << "ID: " << std::hex << id << std::dec<< LL_ENDL;
	LL_WARNS() << "Severity: " << std::hex << severity << std::dec << LL_ENDL;
	LL_WARNS() << "Message: " << message << LL_ENDL;
	LL_WARNS() << "-----------------------" << LL_ENDL;
	if (severity == GL_DEBUG_SEVERITY_HIGH_ARB)
	{
		LL_ERRS() << "Halting on GL Error" << LL_ENDL;
	}
}
#endif

void parse_glsl_version(S32& major, S32& minor);

void ll_init_fail_log(std::string filename)
{
	gFailLog.open(filename.c_str());
}


void ll_fail(std::string msg)
{
	
	if (gDebugSession)
	{
		std::vector<std::string> lines;

		gFailLog << LLError::utcTime() << " " << msg << std::endl;

		gFailLog << "Stack Trace:" << std::endl;

		ll_get_stack_trace(lines);
		
		for(size_t i = 0; i < lines.size(); ++i)
		{
			gFailLog << lines[i] << std::endl;
		}

		gFailLog << "End of Stack Trace." << std::endl << std::endl;

		gFailLog.flush();
	}
};

void ll_close_fail_log()
{
	gFailLog.close();
}

LLMatrix4 gGLObliqueProjectionInverse;

#define LL_GL_NAME_POOLING 0

std::list<LLGLUpdate*> LLGLUpdate::sGLQ;

#if (LL_WINDOWS || LL_LINUX) && !LL_MESA_HEADLESS

#if LL_WINDOWS
PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = NULL;
PFNWGLGETPIXELFORMATATTRIBIVARBPROC wglGetPixelFormatAttribivARB = NULL;
PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = NULL;
PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = NULL;
PFNGLDRAWRANGEELEMENTSPROC glDrawRangeElements = NULL;

// GL_ARB_multitexture
PFNGLMULTITEXCOORD1DARBPROC glMultiTexCoord1dARB = NULL;
PFNGLMULTITEXCOORD1DVARBPROC glMultiTexCoord1dvARB = NULL;
PFNGLMULTITEXCOORD1FARBPROC glMultiTexCoord1fARB = NULL;
PFNGLMULTITEXCOORD1FVARBPROC glMultiTexCoord1fvARB = NULL;
PFNGLMULTITEXCOORD1IARBPROC glMultiTexCoord1iARB = NULL;
PFNGLMULTITEXCOORD1IVARBPROC glMultiTexCoord1ivARB = NULL;
PFNGLMULTITEXCOORD1SARBPROC glMultiTexCoord1sARB = NULL;
PFNGLMULTITEXCOORD1SVARBPROC glMultiTexCoord1svARB = NULL;
PFNGLMULTITEXCOORD2DARBPROC glMultiTexCoord2dARB = NULL;
PFNGLMULTITEXCOORD2DVARBPROC glMultiTexCoord2dvARB = NULL;
PFNGLMULTITEXCOORD2FARBPROC glMultiTexCoord2fARB = NULL;
PFNGLMULTITEXCOORD2FVARBPROC glMultiTexCoord2fvARB = NULL;
PFNGLMULTITEXCOORD2IARBPROC glMultiTexCoord2iARB = NULL;
PFNGLMULTITEXCOORD2IVARBPROC glMultiTexCoord2ivARB = NULL;
PFNGLMULTITEXCOORD2SARBPROC glMultiTexCoord2sARB = NULL;
PFNGLMULTITEXCOORD2SVARBPROC glMultiTexCoord2svARB = NULL;
PFNGLMULTITEXCOORD3DARBPROC glMultiTexCoord3dARB = NULL;
PFNGLMULTITEXCOORD3DVARBPROC glMultiTexCoord3dvARB = NULL;
PFNGLMULTITEXCOORD3FARBPROC glMultiTexCoord3fARB = NULL;
PFNGLMULTITEXCOORD3FVARBPROC glMultiTexCoord3fvARB = NULL;
PFNGLMULTITEXCOORD3IARBPROC glMultiTexCoord3iARB = NULL;
PFNGLMULTITEXCOORD3IVARBPROC glMultiTexCoord3ivARB = NULL;
PFNGLMULTITEXCOORD3SARBPROC glMultiTexCoord3sARB = NULL;
PFNGLMULTITEXCOORD3SVARBPROC glMultiTexCoord3svARB = NULL;
PFNGLMULTITEXCOORD4DARBPROC glMultiTexCoord4dARB = NULL;
PFNGLMULTITEXCOORD4DVARBPROC glMultiTexCoord4dvARB = NULL;
PFNGLMULTITEXCOORD4FARBPROC glMultiTexCoord4fARB = NULL;
PFNGLMULTITEXCOORD4FVARBPROC glMultiTexCoord4fvARB = NULL;
PFNGLMULTITEXCOORD4IARBPROC glMultiTexCoord4iARB = NULL;
PFNGLMULTITEXCOORD4IVARBPROC glMultiTexCoord4ivARB = NULL;
PFNGLMULTITEXCOORD4SARBPROC glMultiTexCoord4sARB = NULL;
PFNGLMULTITEXCOORD4SVARBPROC glMultiTexCoord4svARB = NULL;
PFNGLACTIVETEXTUREARBPROC glActiveTextureARB = NULL;
PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTextureARB = NULL;
#endif

#if LL_LINUX_NV_GL_HEADERS
// linux nvidia headers.  these define these differently to mesa's.  ugh.
PFNGLACTIVETEXTUREARBPROC glActiveTextureARB = NULL;
PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTextureARB = NULL;
PFNGLDRAWRANGEELEMENTSPROC glDrawRangeElements = NULL;
#endif // LL_LINUX_NV_GL_HEADERS

// vertex blending prototypes
PFNGLWEIGHTPOINTERARBPROC			glWeightPointerARB = NULL;
PFNGLVERTEXBLENDARBPROC				glVertexBlendARB = NULL;
PFNGLWEIGHTFVARBPROC				glWeightfvARB = NULL;

// Vertex buffer object prototypes
PFNGLBINDBUFFERARBPROC				glBindBufferARB = NULL;
PFNGLDELETEBUFFERSARBPROC			glDeleteBuffersARB = NULL;
PFNGLGENBUFFERSARBPROC				glGenBuffersARB = NULL;
PFNGLISBUFFERARBPROC				glIsBufferARB = NULL;
PFNGLBUFFERDATAARBPROC				glBufferDataARB = NULL;
PFNGLBUFFERSUBDATAARBPROC			glBufferSubDataARB = NULL;
PFNGLGETBUFFERSUBDATAARBPROC		glGetBufferSubDataARB = NULL;
PFNGLMAPBUFFERARBPROC				glMapBufferARB = NULL;
PFNGLUNMAPBUFFERARBPROC				glUnmapBufferARB = NULL;
PFNGLGETBUFFERPARAMETERIVARBPROC	glGetBufferParameterivARB = NULL;
PFNGLGETBUFFERPOINTERVARBPROC		glGetBufferPointervARB = NULL;

//GL_ARB_vertex_array_object
PFNGLBINDVERTEXARRAYPROC glBindVertexArray = NULL;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays = NULL;
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays = NULL;
PFNGLISVERTEXARRAYPROC glIsVertexArray = NULL;

// GL_ARB_map_buffer_range
PFNGLMAPBUFFERRANGEPROC			glMapBufferRange = NULL;
PFNGLFLUSHMAPPEDBUFFERRANGEPROC	glFlushMappedBufferRange = NULL;

// GL_ARB_texture_compression
PFNGLCOMPRESSEDTEXIMAGE3DARBPROC glCompressedTexImage3DARB = NULL;
PFNGLCOMPRESSEDTEXIMAGE2DARBPROC glCompressedTexImage2DARB = NULL;
PFNGLCOMPRESSEDTEXIMAGE1DARBPROC glCompressedTexImage1DARB = NULL;
PFNGLCOMPRESSEDTEXSUBIMAGE3DARBPROC glCompressedTexSubImage3DARB = NULL;
PFNGLCOMPRESSEDTEXSUBIMAGE2DARBPROC glCompressedTexSubImage2DARB = NULL;
PFNGLCOMPRESSEDTEXSUBIMAGE1DARBPROC glCompressedTexSubImage1DARB = NULL;
PFNGLGETCOMPRESSEDTEXIMAGEARBPROC glGetCompressedTexImageARB = NULL;

// GL_ARB_sync
PFNGLFENCESYNCPROC				glFenceSync = NULL;
PFNGLISSYNCPROC					glIsSync = NULL;
PFNGLDELETESYNCPROC				glDeleteSync = NULL;
PFNGLCLIENTWAITSYNCPROC			glClientWaitSync = NULL;
PFNGLWAITSYNCPROC				glWaitSync = NULL;
PFNGLGETINTEGER64VPROC			glGetInteger64v = NULL;
PFNGLGETSYNCIVPROC				glGetSynciv = NULL;

// GL_APPLE_flush_buffer_range
PFNGLBUFFERPARAMETERIAPPLEPROC	glBufferParameteriAPPLE = NULL;
PFNGLFLUSHMAPPEDBUFFERRANGEAPPLEPROC glFlushMappedBufferRangeAPPLE = NULL;

// GL_ARB_occlusion_query
PFNGLGENQUERIESARBPROC glGenQueriesARB = NULL;
PFNGLDELETEQUERIESARBPROC glDeleteQueriesARB = NULL;
PFNGLISQUERYARBPROC glIsQueryARB = NULL;
PFNGLBEGINQUERYARBPROC glBeginQueryARB = NULL;
PFNGLENDQUERYARBPROC glEndQueryARB = NULL;
PFNGLGETQUERYIVARBPROC glGetQueryivARB = NULL;
PFNGLGETQUERYOBJECTIVARBPROC glGetQueryObjectivARB = NULL;
PFNGLGETQUERYOBJECTUIVARBPROC glGetQueryObjectuivARB = NULL;
PFNGLGETQUERYOBJECTUI64VEXTPROC glGetQueryObjectui64vEXT = NULL;

// GL_ARB_point_parameters
PFNGLPOINTPARAMETERFARBPROC glPointParameterfARB = NULL;
PFNGLPOINTPARAMETERFVARBPROC glPointParameterfvARB = NULL;

// GL_ARB_framebuffer_object
PFNGLISRENDERBUFFERPROC glIsRenderbuffer = NULL;
PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer = NULL;
PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers = NULL;
PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers = NULL;
PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage = NULL;
PFNGLGETRENDERBUFFERPARAMETERIVPROC glGetRenderbufferParameteriv = NULL;
PFNGLISFRAMEBUFFERPROC glIsFramebuffer = NULL;
PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = NULL;
PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = NULL;
PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = NULL;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus = NULL;
PFNGLFRAMEBUFFERTEXTURE1DPROC glFramebufferTexture1D = NULL;
PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = NULL;
PFNGLFRAMEBUFFERTEXTURE3DPROC glFramebufferTexture3D = NULL;
PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer = NULL;
PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC glGetFramebufferAttachmentParameteriv = NULL;
PFNGLGENERATEMIPMAPPROC glGenerateMipmap = NULL;
PFNGLBLITFRAMEBUFFERPROC glBlitFramebuffer = NULL;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glRenderbufferStorageMultisample = NULL;
PFNGLFRAMEBUFFERTEXTURELAYERPROC glFramebufferTextureLayer = NULL;

//GL_ARB_texture_multisample
PFNGLTEXIMAGE2DMULTISAMPLEPROC glTexImage2DMultisample = NULL;
PFNGLTEXIMAGE3DMULTISAMPLEPROC glTexImage3DMultisample = NULL;
PFNGLGETMULTISAMPLEFVPROC glGetMultisamplefv = NULL;
PFNGLSAMPLEMASKIPROC glSampleMaski = NULL;

//transform feedback (4.0 core)
PFNGLBEGINTRANSFORMFEEDBACKPROC glBeginTransformFeedback = NULL;
PFNGLENDTRANSFORMFEEDBACKPROC glEndTransformFeedback = NULL;
PFNGLTRANSFORMFEEDBACKVARYINGSPROC glTransformFeedbackVaryings = NULL;
PFNGLBINDBUFFERRANGEPROC glBindBufferRange = NULL;

//GL_ARB_debug_output
PFNGLDEBUGMESSAGECONTROLARBPROC glDebugMessageControlARB = NULL;
PFNGLDEBUGMESSAGEINSERTARBPROC glDebugMessageInsertARB = NULL;
PFNGLDEBUGMESSAGECALLBACKARBPROC glDebugMessageCallbackARB = NULL;
PFNGLGETDEBUGMESSAGELOGARBPROC glGetDebugMessageLogARB = NULL;

// GL_EXT_blend_func_separate
PFNGLBLENDFUNCSEPARATEEXTPROC glBlendFuncSeparateEXT = NULL;

// GL_ARB_draw_buffers
PFNGLDRAWBUFFERSARBPROC glDrawBuffersARB = NULL;

//shader object prototypes
PFNGLDELETEOBJECTARBPROC glDeleteShader = NULL;
PFNGLDELETEOBJECTARBPROC glDeleteProgram = NULL;
PFNGLDETACHOBJECTARBPROC glDetachObjectARB = NULL;
PFNGLCREATESHADEROBJECTARBPROC glCreateShaderObjectARB = NULL;
PFNGLSHADERSOURCEARBPROC glShaderSourceARB = NULL;
PFNGLCOMPILESHADERARBPROC glCompileShaderARB = NULL;
PFNGLCREATEPROGRAMOBJECTARBPROC glCreateProgramObjectARB = NULL;
PFNGLATTACHOBJECTARBPROC glAttachObjectARB = NULL;
PFNGLLINKPROGRAMARBPROC glLinkProgramARB = NULL;
PFNGLUSEPROGRAMOBJECTARBPROC glUseProgramObjectARB = NULL;
PFNGLVALIDATEPROGRAMARBPROC glValidateProgramARB = NULL;
PFNGLUNIFORM1FARBPROC glUniform1fARB = NULL;
PFNGLUNIFORM2FARBPROC glUniform2fARB = NULL;
PFNGLUNIFORM3FARBPROC glUniform3fARB = NULL;
PFNGLUNIFORM4FARBPROC glUniform4fARB = NULL;
PFNGLUNIFORM1IARBPROC glUniform1iARB = NULL;
PFNGLUNIFORM2IARBPROC glUniform2iARB = NULL;
PFNGLUNIFORM3IARBPROC glUniform3iARB = NULL;
PFNGLUNIFORM4IARBPROC glUniform4iARB = NULL;
PFNGLUNIFORM1FVARBPROC glUniform1fvARB = NULL;
PFNGLUNIFORM2FVARBPROC glUniform2fvARB = NULL;
PFNGLUNIFORM3FVARBPROC glUniform3fvARB = NULL;
PFNGLUNIFORM4FVARBPROC glUniform4fvARB = NULL;
PFNGLUNIFORM1IVARBPROC glUniform1ivARB = NULL;
PFNGLUNIFORM2IVARBPROC glUniform2ivARB = NULL;
PFNGLUNIFORM3IVARBPROC glUniform3ivARB = NULL;
PFNGLUNIFORM4IVARBPROC glUniform4ivARB = NULL;
PFNGLUNIFORMMATRIX2FVARBPROC glUniformMatrix2fvARB = NULL;
PFNGLUNIFORMMATRIX3FVARBPROC glUniformMatrix3fvARB = NULL;
PFNGLUNIFORMMATRIX3X4FVPROC glUniformMatrix3x4fv = NULL;
PFNGLUNIFORMMATRIX4FVARBPROC glUniformMatrix4fvARB = NULL;
PFNGLGETOBJECTPARAMETERIVARBPROC glGetShaderiv = NULL;
PFNGLGETOBJECTPARAMETERIVARBPROC glGetProgramiv = NULL;
PFNGLGETINFOLOGARBPROC glGetShaderInfoLog = NULL;
PFNGLGETINFOLOGARBPROC glGetProgramInfoLog = NULL;
PFNGLGETATTACHEDOBJECTSARBPROC glGetAttachedObjectsARB = NULL;
PFNGLGETUNIFORMLOCATIONARBPROC glGetUniformLocationARB = NULL;
PFNGLGETACTIVEUNIFORMARBPROC glGetActiveUniformARB = NULL;
PFNGLGETUNIFORMFVARBPROC glGetUniformfvARB = NULL;
PFNGLGETUNIFORMIVARBPROC glGetUniformivARB = NULL;
PFNGLGETSHADERSOURCEARBPROC glGetShaderSourceARB = NULL;
PFNGLVERTEXATTRIBIPOINTERPROC glVertexAttribIPointer = NULL;

// vertex shader prototypes
PFNGLVERTEXATTRIB1DARBPROC glVertexAttrib1dARB = NULL;
PFNGLVERTEXATTRIB1DVARBPROC glVertexAttrib1dvARB = NULL;
PFNGLVERTEXATTRIB1FARBPROC glVertexAttrib1fARB = NULL;
PFNGLVERTEXATTRIB1FVARBPROC glVertexAttrib1fvARB = NULL;
PFNGLVERTEXATTRIB1SARBPROC glVertexAttrib1sARB = NULL;
PFNGLVERTEXATTRIB1SVARBPROC glVertexAttrib1svARB = NULL;
PFNGLVERTEXATTRIB2DARBPROC glVertexAttrib2dARB = NULL;
PFNGLVERTEXATTRIB2DVARBPROC glVertexAttrib2dvARB = NULL;
PFNGLVERTEXATTRIB2FARBPROC glVertexAttrib2fARB = NULL;
PFNGLVERTEXATTRIB2FVARBPROC glVertexAttrib2fvARB = NULL;
PFNGLVERTEXATTRIB2SARBPROC glVertexAttrib2sARB = NULL;
PFNGLVERTEXATTRIB2SVARBPROC glVertexAttrib2svARB = NULL;
PFNGLVERTEXATTRIB3DARBPROC glVertexAttrib3dARB = NULL;
PFNGLVERTEXATTRIB3DVARBPROC glVertexAttrib3dvARB = NULL;
PFNGLVERTEXATTRIB3FARBPROC glVertexAttrib3fARB = NULL;
PFNGLVERTEXATTRIB3FVARBPROC glVertexAttrib3fvARB = NULL;
PFNGLVERTEXATTRIB3SARBPROC glVertexAttrib3sARB = NULL;
PFNGLVERTEXATTRIB3SVARBPROC glVertexAttrib3svARB = NULL;
PFNGLVERTEXATTRIB4NBVARBPROC glVertexAttrib4NbvARB = NULL;
PFNGLVERTEXATTRIB4NIVARBPROC glVertexAttrib4NivARB = NULL;
PFNGLVERTEXATTRIB4NSVARBPROC glVertexAttrib4NsvARB = NULL;
PFNGLVERTEXATTRIB4NUBARBPROC glVertexAttrib4NubARB = NULL;
PFNGLVERTEXATTRIB4NUBVARBPROC glVertexAttrib4NubvARB = NULL;
PFNGLVERTEXATTRIB4NUIVARBPROC glVertexAttrib4NuivARB = NULL;
PFNGLVERTEXATTRIB4NUSVARBPROC glVertexAttrib4NusvARB = NULL;
PFNGLVERTEXATTRIB4BVARBPROC glVertexAttrib4bvARB = NULL;
PFNGLVERTEXATTRIB4DARBPROC glVertexAttrib4dARB = NULL;
PFNGLVERTEXATTRIB4DVARBPROC glVertexAttrib4dvARB = NULL;
PFNGLVERTEXATTRIB4FARBPROC glVertexAttrib4fARB = NULL;
PFNGLVERTEXATTRIB4FVARBPROC glVertexAttrib4fvARB = NULL;
PFNGLVERTEXATTRIB4IVARBPROC glVertexAttrib4ivARB = NULL;
PFNGLVERTEXATTRIB4SARBPROC glVertexAttrib4sARB = NULL;
PFNGLVERTEXATTRIB4SVARBPROC glVertexAttrib4svARB = NULL;
PFNGLVERTEXATTRIB4UBVARBPROC glVertexAttrib4ubvARB = NULL;
PFNGLVERTEXATTRIB4UIVARBPROC glVertexAttrib4uivARB = NULL;
PFNGLVERTEXATTRIB4USVARBPROC glVertexAttrib4usvARB = NULL;
PFNGLVERTEXATTRIBPOINTERARBPROC glVertexAttribPointerARB = NULL;
PFNGLENABLEVERTEXATTRIBARRAYARBPROC glEnableVertexAttribArrayARB = NULL;
PFNGLDISABLEVERTEXATTRIBARRAYARBPROC glDisableVertexAttribArrayARB = NULL;
PFNGLPROGRAMSTRINGARBPROC glProgramStringARB = NULL;
PFNGLBINDPROGRAMARBPROC glBindProgramARB = NULL;
PFNGLDELETEPROGRAMSARBPROC glDeleteProgramsARB = NULL;
PFNGLGENPROGRAMSARBPROC glGenProgramsARB = NULL;
PFNGLPROGRAMENVPARAMETER4DARBPROC glProgramEnvParameter4dARB = NULL;
PFNGLPROGRAMENVPARAMETER4DVARBPROC glProgramEnvParameter4dvARB = NULL;
PFNGLPROGRAMENVPARAMETER4FARBPROC glProgramEnvParameter4fARB = NULL;
PFNGLPROGRAMENVPARAMETER4FVARBPROC glProgramEnvParameter4fvARB = NULL;
PFNGLPROGRAMLOCALPARAMETER4DARBPROC glProgramLocalParameter4dARB = NULL;
PFNGLPROGRAMLOCALPARAMETER4DVARBPROC glProgramLocalParameter4dvARB = NULL;
PFNGLPROGRAMLOCALPARAMETER4FARBPROC glProgramLocalParameter4fARB = NULL;
PFNGLPROGRAMLOCALPARAMETER4FVARBPROC glProgramLocalParameter4fvARB = NULL;
PFNGLGETPROGRAMENVPARAMETERDVARBPROC glGetProgramEnvParameterdvARB = NULL;
PFNGLGETPROGRAMENVPARAMETERFVARBPROC glGetProgramEnvParameterfvARB = NULL;
PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC glGetProgramLocalParameterdvARB = NULL;
PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC glGetProgramLocalParameterfvARB = NULL;
PFNGLGETPROGRAMIVARBPROC glGetProgramivARB = NULL;
PFNGLGETPROGRAMSTRINGARBPROC glGetProgramStringARB = NULL;
PFNGLGETVERTEXATTRIBDVARBPROC glGetVertexAttribdvARB = NULL;
PFNGLGETVERTEXATTRIBFVARBPROC glGetVertexAttribfvARB = NULL;
PFNGLGETVERTEXATTRIBIVARBPROC glGetVertexAttribivARB = NULL;
PFNGLGETVERTEXATTRIBPOINTERVARBPROC glGetVertexAttribPointervARB = NULL;
PFNGLISPROGRAMARBPROC glIsProgramARB = NULL;
PFNGLBINDATTRIBLOCATIONARBPROC glBindAttribLocationARB = NULL;
PFNGLGETACTIVEATTRIBARBPROC glGetActiveAttribARB = NULL;
PFNGLGETATTRIBLOCATIONARBPROC glGetAttribLocationARB = NULL;
#endif // (LL_WINDOWS || LL_LINUX || LL_SOLARIS)  && !LL_MESA_HEADLESS

LLGLManager gGLManager;

LLGLManager::LLGLManager() :
	mInited(FALSE),
	mIsDisabled(FALSE),

	mHasMultitexture(FALSE),
	mHasATIMemInfo(FALSE),
	mHasNVXMemInfo(FALSE),
	mNumTextureUnits(1),
	mHasMipMapGeneration(FALSE),
	mHasCompressedTextures(FALSE),
	mHasFramebufferObject(FALSE),
	mMaxSamples(0),
	mHasFramebufferMultisample(FALSE),
	mHasBlendFuncSeparate(FALSE),
	mHasSync(FALSE),
	mHasVertexBufferObject(FALSE),
	mHasVertexArrayObject(FALSE),
	mHasMapBufferRange(FALSE),
	mHasFlushBufferRange(FALSE),
	mHasShaderObjects(FALSE),
	mHasVertexShader(FALSE),
	mHasFragmentShader(FALSE),
	mNumTextureImageUnits(0),
	mHasOcclusionQuery(FALSE),
	mHasOcclusionQuery2(FALSE),
	mHasPointParameters(FALSE),
	mHasDrawBuffers(FALSE),
	mHasTransformFeedback(FALSE),
	mMaxIntegerSamples(0),

	mHasAnisotropic(FALSE),
	mHasARBEnvCombine(FALSE),
	mHasCubeMap(FALSE),
	mHasDebugOutput(FALSE),

	mHasGpuShader5(FALSE),
	mHasAdaptiveVsync(FALSE),
	mHasTextureSwizzle(FALSE),

	mIsATI(FALSE),
	mIsNVIDIA(FALSE),
	mIsIntel(FALSE),
	mIsHD3K(FALSE),
	mIsGF2or4MX(FALSE),
	mIsGF3(FALSE),
	mIsGFFX(FALSE),
	mATIOffsetVerticalLines(FALSE),
	mATIOldDriver(FALSE),
#if LL_DARWIN
	mIsMobileGF(FALSE),
#endif
	mHasRequirements(TRUE),

	mDebugGPU(FALSE),

	mDriverVersionMajor(1),
	mDriverVersionMinor(0),
	mDriverVersionRelease(0),
	mGLVersion(1.0f),
	mGLSLVersionMajor(0),
	mGLSLVersionMinor(0),		
	mVRAM(0),
	mGLMaxVertexRange(0),
	mGLMaxIndexRange(0),
	mGLMaxVertexUniformComponents(0)
{
}

std::set<std::string> sGLExtensions; // Not techincally safe to issue this before context is created.
#if LL_WINDOWS
std::set<std::string> sWGLExtensions; // Fine (probably) without context.
#endif
void registerExtension(std::string ext, std::set<std::string>& extensions)
{
	extensions.emplace(ext);
	LL_INFOS("GLExtensions") << ext << LL_ENDL;
}
void loadExtensionStrings()
{
	sGLExtensions.clear();
	U32 gl_version = atoi((const char*)glGetString(GL_VERSION));
	if (gl_version >= 3)
	{
#ifndef LL_DARWIN
		PFNGLGETSTRINGIPROC glGetStringi = (PFNGLGETSTRINGIPROC)GLH_EXT_GET_PROC_ADDRESS("glGetStringi");
#endif
		GLint count = 0;
		glGetIntegerv(GL_NUM_EXTENSIONS, &count);
		for (GLint i = 0; i < count; ++i)
		{
			registerExtension((char const*)glGetStringi(GL_EXTENSIONS, i), sGLExtensions);
		}
	}
	else // Deprecated.
	{
		typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
		boost::char_separator<char> sep(" ");
		std::string extensions((const char*)glGetString(GL_EXTENSIONS));
		for (auto& extension : tokenizer(extensions, sep))
		{
			registerExtension(extension, sGLExtensions);
		}
	}
}
#if LL_WINDOWS
void loadWGLExtensionStrings()
{
	sWGLExtensions.clear();
	PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC)wglGetProcAddress("wglGetExtensionsStringARB");
	if (wglGetExtensionsStringARB)
	{
		typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
		boost::char_separator<char> sep(" ");
		std::string extensions = std::string(wglGetExtensionsStringARB(wglGetCurrentDC()));
		for (auto& extension : tokenizer(extensions, sep))
		{
			registerExtension(extension, sWGLExtensions);
		}
	}
}
#endif

bool ExtensionExists(std::string ext)
{
	auto* extensions = &sGLExtensions;
#if LL_WINDOWS
	if (ext.rfind("WGL_", 0) == 0)
	{
		extensions = &sWGLExtensions;
		if (extensions->empty())
			loadWGLExtensionStrings();
	}
	else
#endif
	if (extensions->empty())
		loadExtensionStrings();
	bool found = extensions->find(ext) != extensions->end();
	if (!found)
		LL_INFOS("GLExtensions") << ext << " MISSING" << LL_ENDL;
	return found;
}

//---------------------------------------------------------------------
// Global initialization for GL
//---------------------------------------------------------------------
void LLGLManager::initWGL()
{
#if LL_WINDOWS && !LL_MESA_HEADLESS
	loadWGLExtensionStrings();
	if (ExtensionExists("WGL_ARB_pixel_format"))
	{
		wglGetPixelFormatAttribivARB = (PFNWGLGETPIXELFORMATATTRIBIVARBPROC)wglGetProcAddress("wglGetPixelFormatAttribivARB");
		wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
	}
	else
	{
		LL_WARNS("RenderInit") << "No ARB pixel format extensions" << LL_ENDL;
	}
		
	if (ExtensionExists("WGL_ARB_create_context"))
	{
		wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
	}
	else
	{
		LL_WARNS("RenderInit") << "No ARB create context extensions" << LL_ENDL;
	}
	
	if (ExtensionExists("WGL_EXT_swap_control"))
	{
        wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
	}
	else
	{
		LL_WARNS("RenderInit") << "No ARB swap control extensions" << LL_ENDL;
	}
#endif
}

// return false if unable (or unwilling due to old drivers) to init GL
bool LLGLManager::initGL()
{
	if (mInited)
	{
		LL_ERRS("RenderInit") << "Calling init on LLGLManager after already initialized!" << LL_ENDL;
	}

	stop_glerror();

	loadExtensionStrings();
	
	stop_glerror();

	// Extract video card strings and convert to upper case to
	// work around driver-to-driver variation in capitalization.
	mGLVendor = std::string((const char *)glGetString(GL_VENDOR));
	LLStringUtil::toUpper(mGLVendor);

	mGLRenderer = std::string((const char *)glGetString(GL_RENDERER));
	LLStringUtil::toUpper(mGLRenderer);

	parse_gl_version( &mDriverVersionMajor, 
		&mDriverVersionMinor, 
		&mDriverVersionRelease, 
		&mDriverVersionVendorString,
		&mGLVersionString);

	mGLVersion = mDriverVersionMajor + mDriverVersionMinor * .1f;

	if (mGLVersion >= 2.f)
	{
		parse_glsl_version(mGLSLVersionMajor, mGLSLVersionMinor);

#if LL_DARWIN
		//never use GLSL greater than 1.20 on OSX
		if (mGLSLVersionMajor > 1 || mGLSLVersionMinor >= 30)
		{
			mGLSLVersionMajor = 1;
			mGLSLVersionMinor = 20;
		}
#endif
	}
	
	// Trailing space necessary to keep "nVidia Corpor_ati_on" cards
	// from being recognized as ATI.
	if (mGLVendor.substr(0,4) == "ATI "
//#if LL_LINUX
//		// The Mesa-based drivers put this in the Renderer string,
//		// not the Vendor string.
//		|| mGLRenderer.find("AMD") != std::string::npos
//#endif //LL_LINUX
		)
	{
		mGLVendorShort = "ATI";
		// *TODO: Fix this?
		mIsATI = TRUE;

#if LL_WINDOWS && !LL_MESA_HEADLESS
		if (mDriverVersionRelease < 3842)
		{
			mATIOffsetVerticalLines = TRUE;
		}
#endif // LL_WINDOWS

#if (LL_WINDOWS || LL_LINUX) && !LL_MESA_HEADLESS
		// count any pre OpenGL 3.0 implementation as an old driver
		if (mGLVersion < 3.f) 
		{
			mATIOldDriver = TRUE;
		}
#endif // (LL_WINDOWS || LL_LINUX) && !LL_MESA_HEADLESS
	}
	else if (mGLVendor.find("NVIDIA ") != std::string::npos)
	{
		mGLVendorShort = "NVIDIA";
		mIsNVIDIA = TRUE;
		if (   mGLRenderer.find("GEFORCE4 MX") != std::string::npos
			|| mGLRenderer.find("GEFORCE2") != std::string::npos
			|| mGLRenderer.find("GEFORCE 2") != std::string::npos
			|| mGLRenderer.find("GEFORCE4 460 GO") != std::string::npos
			|| mGLRenderer.find("GEFORCE4 440 GO") != std::string::npos
			|| mGLRenderer.find("GEFORCE4 420 GO") != std::string::npos)
		{
			mIsGF2or4MX = TRUE;
		}
		else if (mGLRenderer.find("GEFORCE FX") != std::string::npos
				 || mGLRenderer.find("QUADRO FX") != std::string::npos
				 || mGLRenderer.find("NV34") != std::string::npos)
		{
			mIsGFFX = TRUE;
		}
		else if(mGLRenderer.find("GEFORCE3") != std::string::npos)
		{
			mIsGF3 = TRUE;
		}
#if LL_DARWIN
		else if ((mGLRenderer.find("9400M") != std::string::npos)
			  || (mGLRenderer.find("9600M") != std::string::npos)
			  || (mGLRenderer.find("9800M") != std::string::npos))
		{
			mIsMobileGF = TRUE;
		}
#endif

	}
	else if (mGLVendor.find("INTEL") != std::string::npos
#if LL_LINUX
		 // The Mesa-based drivers put this in the Renderer string,
		 // not the Vendor string.
		 || mGLRenderer.find("INTEL") != std::string::npos
#endif //LL_LINUX
		 )
	{
		mGLVendorShort = "INTEL";
		mIsIntel = TRUE;
#if LL_WINDOWS
		if (mGLRenderer.find("HD") != std::string::npos
			&& ((mGLRenderer.find("2000") != std::string::npos || mGLRenderer.find("3000") != std::string::npos)
				|| (mGLVersion == 3.1f && mGLRenderer.find("INTEL(R) HD GRAPHICS") != std::string::npos)))
		{
			mIsHD3K = TRUE;
		}
#endif
	}
	else
	{
		mGLVendorShort = "MISC";
	}
	
	stop_glerror();
	initExtensions();
	stop_glerror();

	if (mGLVersion >= 2.1f && mHasCompressedTextures && LLImageGL::sCompressTextures)
	{ //use texture compression
		glHint(GL_TEXTURE_COMPRESSION_HINT, GL_NICEST);
	}
	else
	{ //GL version is < 3.0, always disable texture compression
		LLImageGL::sCompressTextures = false;
	}

	S32 old_vram = mVRAM;

	if (mHasATIMemInfo)
	{ //ask the gl how much vram is free at startup and attempt to use no more than half of that
		S32 meminfo[4];
		glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, meminfo);

		mVRAM = meminfo[0]/1024;
	}
	else if (mHasNVXMemInfo)
	{
		S32 dedicated_memory;
		glGetIntegerv(GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &dedicated_memory);
		mVRAM = dedicated_memory/1024;
	}

	if (mVRAM < 256)
	{ //something likely went wrong using the above extensions, fall back to old method
		mVRAM = old_vram;
	}

	stop_glerror();

	stop_glerror();

	if (mHasFragmentShader)
	{
		GLint num_tex_image_units;
		glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS_ARB, &num_tex_image_units);
		mNumTextureImageUnits = llmin(num_tex_image_units, 32);
	}

	if (mHasVertexShader)
	{
		//According to the spec, the resulting value should never be less than 512. We need at least 1024 to use skinned shaders.
		glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB, &mGLMaxVertexUniformComponents);
		if (mIsHD3K)
		{
			mGLMaxVertexUniformComponents = llmax(mGLMaxVertexUniformComponents, 4096);
		}
	}

	if (LLRender::sGLCoreProfile)
	{
		// If core is true, then mNumTextureUnits is pretty much unused.
		mNumTextureUnits = llmin(mNumTextureImageUnits, MAX_GL_TEXTURE_UNITS);
	}
	else if (mHasMultitexture)
	{
		GLint num_tex_units;		
		glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &num_tex_units);
		mNumTextureUnits = llmin(num_tex_units, (GLint)MAX_GL_TEXTURE_UNITS);
		if (mIsIntel)
		{
			mNumTextureUnits = llmin(mNumTextureUnits, 2);
		}
	}
	else
	{
		mHasRequirements = FALSE;

		// We don't support cards that don't support the GL_ARB_multitexture extension
		LL_WARNS("RenderInit") << "GL Drivers do not support GL_ARB_multitexture" << LL_ENDL;
		return false;
	}
	
	stop_glerror();
	
	//Singu Note: Multisampled texture stuff in v3 is dead, however we DO use multisampled FBOs.
	if (mHasFramebufferMultisample)
	{
		glGetIntegerv(GL_MAX_INTEGER_SAMPLES, &mMaxIntegerSamples);
		glGetIntegerv(GL_MAX_SAMPLES, &mMaxSamples);
	}

	stop_glerror();
#if LL_WINDOWS
	if (mHasDebugOutput && gDebugGL)
	{ //setup debug output callback
		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallbackARB((GLDEBUGPROCARB) gl_debug_callback, NULL);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
		glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, true);
	}
#endif
	stop_glerror();

#if LL_WINDOWS
	if (mIsIntel && mGLVersion <= 3.f)
	{ //never try to use framebuffer objects on older intel drivers (crashy)
		mHasFramebufferObject = FALSE;
	}
#endif

	stop_glerror();
	
	setToDebugGPU();

	stop_glerror();

	initGLStates();

	stop_glerror();

	return true;
}

void LLGLManager::setToDebugGPU()
{
	//"MOBILE INTEL(R) 965 EXPRESS CHIP", 
	if (mGLRenderer.find("INTEL") != std::string::npos && mGLRenderer.find("965") != std::string::npos)
	{
		mDebugGPU = TRUE ;
	}

	return ;
}

void LLGLManager::getGLInfo(LLSD& info)
{
	info["GLInfo"]["GLVendor"] = std::string((const char *)glGetString(GL_VENDOR));
	info["GLInfo"]["GLRenderer"] = std::string((const char *)glGetString(GL_RENDERER));
	info["GLInfo"]["GLVersion"] = std::string((const char *)glGetString(GL_VERSION));

#if !LL_MESA_HEADLESS
	for (auto& extension : sGLExtensions)
	{
		info["GLInfo"]["GLExtensions"].append(extension);
	}
#endif
}

std::string LLGLManager::getGLInfoString()
{
	std::string info_str;

	info_str += std::string("GL_VENDOR      ") + ll_safe_string((const char *)glGetString(GL_VENDOR)) + std::string("\n");
	info_str += std::string("GL_RENDERER    ") + ll_safe_string((const char *)glGetString(GL_RENDERER)) + std::string("\n");
	info_str += std::string("GL_VERSION     ") + ll_safe_string((const char *)glGetString(GL_VERSION)) + std::string("\n");

#if !LL_MESA_HEADLESS 
	info_str += std::string("GL_EXTENSIONS:\n");
	for (auto& extension : sGLExtensions)
	{
		info_str += extension + "\n";
	}
#endif
	
	return info_str;
}

void LLGLManager::printGLInfoString()
{
	LL_INFOS("RenderInit") << "GL_VENDOR:     " << ((const char *)glGetString(GL_VENDOR)) << LL_ENDL;
	LL_INFOS("RenderInit") << "GL_RENDERER:   " << ((const char *)glGetString(GL_RENDERER)) << LL_ENDL;
	LL_INFOS("RenderInit") << "GL_VERSION:    " << ((const char *)glGetString(GL_VERSION)) << LL_ENDL;

#if !LL_MESA_HEADLESS 
	LL_DEBUGS("RenderInit") << "GL_EXTENSIONS:" << "\n";
	for (auto& extension : sGLExtensions)
	{
		LL_CONT << extension << "\n";
	}
	LL_CONT << LL_ENDL;
#endif
}

std::string LLGLManager::getRawGLString()
{
	std::string gl_string;
	gl_string = ll_safe_string((char*)glGetString(GL_VENDOR)) + " " + ll_safe_string((char*)glGetString(GL_RENDERER));
	return gl_string;
}

void LLGLManager::shutdownGL()
{
	if (mInited)
	{
		glFinish();
		stop_glerror();
		mInited = FALSE;
	}
}

// these are used to turn software blending on. They appear in the Debug/Avatar menu
// presence of vertex skinning/blending or vertex programs will set these to FALSE by default.

void LLGLManager::initExtensions()
{
#if LL_MESA_HEADLESS
# ifdef GL_ARB_multitexture
	mHasMultitexture = TRUE;
# else
	mHasMultitexture = FALSE;
# endif // GL_ARB_multitexture
# ifdef GL_ARB_texture_env_combine
	mHasARBEnvCombine = TRUE;	
# else
	mHasARBEnvCombine = FALSE;
# endif // GL_ARB_texture_env_combine
# ifdef GL_ARB_texture_compression
	mHasCompressedTextures = TRUE;
# else
	mHasCompressedTextures = FALSE;
# endif // GL_ARB_texture_compression
# ifdef GL_ARB_vertex_buffer_object
	mHasVertexBufferObject = TRUE;
# else
	mHasVertexBufferObject = FALSE;
# endif // GL_ARB_vertex_buffer_object
# ifdef GL_EXT_framebuffer_object
	mHasFramebufferObject = TRUE;
# else
	mHasFramebufferObject = FALSE;
# endif // GL_EXT_framebuffer_object
# ifdef GL_EXT_framebuffer_multisample
	mHasFramebufferMultisample = TRUE;
# else
	mHasFramebufferMultisample = FALSE;
# endif // GL_EXT_framebuffer_multisample
# ifdef GL_ARB_draw_buffers
	mHasDrawBuffers = TRUE;
#else
	mHasDrawBuffers = FALSE;
# endif // GL_ARB_draw_buffers
# if defined(GL_NV_depth_clamp) || defined(GL_ARB_depth_clamp)
	mHasDepthClamp = TRUE;
#else
	mHasDepthClamp = FALSE;
#endif // defined(GL_NV_depth_clamp) || defined(GL_ARB_depth_clamp)
# if GL_EXT_blend_func_separate
	mHasBlendFuncSeparate = TRUE;
#else
	mHasBlendFuncSeparate = FALSE;
# endif // GL_EXT_blend_func_separate
	mHasMipMapGeneration = FALSE;
	mHasAnisotropic = FALSE;
	mHasCubeMap = FALSE;
	mHasOcclusionQuery = FALSE;
	mHasPointParameters = FALSE;
	mHasShaderObjects = FALSE;
	mHasVertexShader = FALSE;
	mHasFragmentShader = FALSE;
#ifdef GL_ARB_gpu_shader5
	mHasGpuShader5 = FALSE;
#endif
#else // LL_MESA_HEADLESS
	mHasMultitexture = mGLVersion >= 1.3f || ExtensionExists("GL_ARB_multitexture");
	mHasATIMemInfo = ExtensionExists("GL_ATI_meminfo");
	mHasNVXMemInfo = ExtensionExists("GL_NVX_gpu_memory_info");
	mHasCompressedTextures = mGLVersion >= 1.3 || ExtensionExists("GL_ARB_texture_compression");
	mHasAnisotropic = mGLVersion >= 4.6f || ExtensionExists("GL_EXT_texture_filter_anisotropic");
	mHasCubeMap = mGLVersion >= 1.3f || ExtensionExists("GL_ARB_texture_cube_map");
	mHasARBEnvCombine = mGLVersion >= 2.1f || ExtensionExists("GL_ARB_texture_env_combine");
	mHasOcclusionQuery = mGLVersion >= 1.5f || ExtensionExists("GL_ARB_occlusion_query");
	mHasOcclusionQuery2 = mGLVersion >= 3.3f || ExtensionExists("GL_ARB_occlusion_query2");
	mHasVertexBufferObject = mGLVersion >= 1.5f || ExtensionExists("GL_ARB_vertex_buffer_object");
	mHasVertexArrayObject = mGLVersion >= 3.f || ExtensionExists("GL_ARB_vertex_array_object");
	mHasSync = mGLVersion >= 3.2f || ExtensionExists("GL_ARB_sync");
	mHasMapBufferRange = mGLVersion >= 3.f || ExtensionExists("GL_ARB_map_buffer_range");
	mHasFlushBufferRange = ExtensionExists("GL_APPLE_flush_buffer_range");
	mHasDepthClamp = mGLVersion >= 3.2f || ExtensionExists("GL_ARB_depth_clamp") || ExtensionExists("GL_NV_depth_clamp");
	// mask out FBO support when packed_depth_stencil isn't there 'cause we need it for LLRenderTarget -Brad
#ifdef GL_ARB_framebuffer_object
	mHasFramebufferObject = mGLVersion >= 3.f || ExtensionExists("GL_ARB_framebuffer_object");
#else
	mHasFramebufferObject = mGLVersion >= 3.f || (ExtensionExists("GL_EXT_framebuffer_object") &&
							ExtensionExists("GL_EXT_framebuffer_blit") &&
							ExtensionExists("GL_EXT_framebuffer_multisample") &&
							ExtensionExists("GL_EXT_packed_depth_stencil"));
#endif
	mHasFramebufferMultisample = mGLVersion >= 3.f || (mHasFramebufferObject && ExtensionExists("GL_EXT_framebuffer_multisample"));
	
	mHasMipMapGeneration = mHasFramebufferObject || mGLVersion >= 1.4f;

	mHasDrawBuffers = mGLVersion >= 2.f || ExtensionExists("GL_ARB_draw_buffers");
	mHasBlendFuncSeparate = mGLVersion >= 1.4f || ExtensionExists("GL_EXT_blend_func_separate");
	mHasDebugOutput = mGLVersion >= 4.3f || ExtensionExists("GL_ARB_debug_output");
	mHasTransformFeedback = mGLVersion >= 4.f || ExtensionExists("GL_EXT_transform_feedback");
#if !LL_DARWIN
	mHasPointParameters = mGLVersion >= 2.f || (!mIsATI && ExtensionExists("GL_ARB_point_parameters"));
#endif
	mHasShaderObjects = mGLVersion >= 2.f || ExtensionExists("GL_ARB_shader_objects") && (LLRender::sGLCoreProfile || ExtensionExists("GL_ARB_shading_language_100"));
	mHasVertexShader = mGLVersion >= 2.f || (ExtensionExists("GL_ARB_vertex_program") && ExtensionExists("GL_ARB_vertex_shader")
		&& (LLRender::sGLCoreProfile || ExtensionExists("GL_ARB_shading_language_100")));
	mHasFragmentShader = mGLVersion >= 2.f || ExtensionExists("GL_ARB_fragment_shader") && (LLRender::sGLCoreProfile || ExtensionExists("GL_ARB_shading_language_100"));
#endif
#ifdef GL_ARB_gpu_shader5
	mHasGpuShader5 = mGLVersion >= 4.f || ExtensionExists("GL_ARB_gpu_shader5");;
#endif
#if LL_WINDOWS
	mHasAdaptiveVsync = ExtensionExists("WGL_EXT_swap_control_tear");
#elif LL_LINUX
	mHasAdaptiveVsync = ExtensionExists("GLX_EXT_swap_control_tear");
#endif

#ifdef GL_ARB_texture_swizzle
	mHasTextureSwizzle = mGLVersion >= 3.3f || ExtensionExists("GL_ARB_texture_swizzle");
#endif

#if LL_LINUX || LL_SOLARIS
	LL_INFOS() << "initExtensions() checking shell variables to adjust features..." << LL_ENDL;
	// Our extension support for the Linux Client is very young with some
	// potential driver gotchas, so offer a semi-secret way to turn it off.
	if (getenv("LL_GL_NOEXT"))	/* Flawfinder: ignore */
	{
		//mHasMultitexture = FALSE; // NEEDED!
		mHasDepthClamp = FALSE;
		mHasARBEnvCombine = FALSE;
		mHasCompressedTextures = FALSE;
		mHasVertexBufferObject = FALSE;
		mHasFramebufferObject = FALSE;
		mHasFramebufferMultisample = FALSE;
		mHasDrawBuffers = FALSE;
		mHasBlendFuncSeparate = FALSE;
		mHasMipMapGeneration = FALSE;
		mHasAnisotropic = FALSE;
		mHasCubeMap = FALSE;
		mHasOcclusionQuery = FALSE;
		mHasPointParameters = FALSE;
		mHasShaderObjects = FALSE;
		mHasVertexShader = FALSE;
		mHasFragmentShader = FALSE;
		mHasAdaptiveVsync = FALSE;
		mHasTextureSwizzle = FALSE;
		LL_WARNS("RenderInit") << "GL extension support DISABLED via LL_GL_NOEXT" << LL_ENDL;
	}
	else if (getenv("LL_GL_BASICEXT"))	/* Flawfinder: ignore */
	{
		// This switch attempts to turn off all support for exotic
		// extensions which I believe correspond to fatal driver
		// bug reports.  This should be the default until we get a
		// proper blacklist/whitelist on Linux.
		mHasMipMapGeneration = FALSE;
		mHasAnisotropic = FALSE;
		//mHasCubeMap = FALSE; // apparently fatal on Intel 915 & similar
		//mHasOcclusionQuery = FALSE; // source of many ATI system hangs
		mHasShaderObjects = FALSE;
		mHasVertexShader = FALSE;
		mHasFragmentShader = FALSE;
		mHasBlendFuncSeparate = FALSE;
		LL_WARNS("RenderInit") << "GL extension support forced to SIMPLE level via LL_GL_BASICEXT" << LL_ENDL;
	}
	if (getenv("LL_GL_BLACKLIST"))	/* Flawfinder: ignore */
	{
		// This lets advanced troubleshooters disable specific
		// GL extensions to isolate problems with their hardware.
		// SL-28126
		const char *const blacklist = getenv("LL_GL_BLACKLIST");	/* Flawfinder: ignore */
		LL_WARNS("RenderInit") << "GL extension support partially disabled via LL_GL_BLACKLIST: " << blacklist << LL_ENDL;
		if (strchr(blacklist,'a')) mHasARBEnvCombine = FALSE;
		if (strchr(blacklist,'b')) mHasCompressedTextures = FALSE;
		if (strchr(blacklist,'c')) mHasVertexBufferObject = FALSE;
		if (strchr(blacklist,'d')) mHasMipMapGeneration = FALSE;//S
// 		if (strchr(blacklist,'f')) mHasNVVertexArrayRange = FALSE;//S
// 		if (strchr(blacklist,'g')) mHasNVFence = FALSE;//S
		if (strchr(blacklist,'i')) mHasAnisotropic = FALSE;//S
		if (strchr(blacklist,'j')) mHasCubeMap = FALSE;//S
// 		if (strchr(blacklist,'k')) mHasATIVAO = FALSE;//S
		if (strchr(blacklist,'l')) mHasOcclusionQuery = FALSE;
		if (strchr(blacklist,'m')) mHasShaderObjects = FALSE;//S
		if (strchr(blacklist,'n')) mHasVertexShader = FALSE;//S
		if (strchr(blacklist,'o')) mHasFragmentShader = FALSE;//S
		if (strchr(blacklist,'p')) mHasPointParameters = FALSE;//S
		if (strchr(blacklist,'q')) mHasFramebufferObject = FALSE;//S
		if (strchr(blacklist,'r')) mHasDrawBuffers = FALSE;//S
		if (strchr(blacklist,'s')) mHasFramebufferMultisample = FALSE;
		if (strchr(blacklist,'u')) mHasBlendFuncSeparate = FALSE;//S
		if (strchr(blacklist,'v')) mHasDepthClamp = FALSE;

	}
#endif // LL_LINUX || LL_SOLARIS

	if (!mHasMultitexture)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize multitexturing" << LL_ENDL;
	}
	if (!mHasMipMapGeneration)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize mipmap generation" << LL_ENDL;
	}
	if (!mHasARBEnvCombine)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize GL_ARB_texture_env_combine" << LL_ENDL;
	}
	if (!mHasAnisotropic)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize anisotropic filtering" << LL_ENDL;
	}
	if (!mHasCompressedTextures)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize GL_ARB_texture_compression" << LL_ENDL;
	}
	if (!mHasOcclusionQuery)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize GL_ARB_occlusion_query" << LL_ENDL;
	}
	if (!mHasOcclusionQuery2)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize GL_ARB_occlusion_query2" << LL_ENDL;
	}
	if (!mHasPointParameters)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize GL_ARB_point_parameters" << LL_ENDL;
	}
	if (!mHasShaderObjects)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize GL_ARB_shader_objects" << LL_ENDL;
	}
	if (!mHasVertexShader)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize GL_ARB_vertex_shader" << LL_ENDL;
	}
	if (!mHasFragmentShader)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize GL_ARB_fragment_shader" << LL_ENDL;
	}
	if (!mHasBlendFuncSeparate)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize GL_EXT_blend_func_separate" << LL_ENDL;
	}
	if (!mHasDrawBuffers)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize GL_ARB_draw_buffers" << LL_ENDL;
	}

	// Disable certain things due to known bugs
	if (mIsIntel && mHasMipMapGeneration)
	{
		LL_INFOS("RenderInit") << "Disabling mip-map generation for Intel GPUs" << LL_ENDL;
		mHasMipMapGeneration = FALSE;
	}
	//Don't do this! This has since been fixed. I've benchmarked it. Hardware generation considerably faster. -Shyotl
	/*
	if (mIsATI && mHasMipMapGeneration)
	{
		LL_INFOS("RenderInit") << "Disabling mip-map generation for ATI GPUs (performance opt)" << LL_ENDL;
		mHasMipMapGeneration = FALSE;
	}*/
	
	// Misc
	glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, (GLint*) &mGLMaxVertexRange);
	glGetIntegerv(GL_MAX_ELEMENTS_INDICES, (GLint*) &mGLMaxIndexRange);
	
#if (LL_WINDOWS || LL_LINUX) && !LL_MESA_HEADLESS
	LL_DEBUGS("RenderInit") << "GL Probe: Getting symbols" << LL_ENDL;
#if LL_WINDOWS
	if (mHasMultitexture)
	{
		glMultiTexCoord1dARB = (PFNGLMULTITEXCOORD1DARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord1d");
		glMultiTexCoord1dvARB = (PFNGLMULTITEXCOORD1DVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord1dv");
		glMultiTexCoord1fARB = (PFNGLMULTITEXCOORD1FARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord1f");
		glMultiTexCoord1fvARB = (PFNGLMULTITEXCOORD1FVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord1fv");
		glMultiTexCoord1iARB = (PFNGLMULTITEXCOORD1IARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord1i");
		glMultiTexCoord1ivARB = (PFNGLMULTITEXCOORD1IVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord1iv");
		glMultiTexCoord1sARB = (PFNGLMULTITEXCOORD1SARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord1s");
		glMultiTexCoord1svARB = (PFNGLMULTITEXCOORD1SVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord1sv");
		glMultiTexCoord2dARB = (PFNGLMULTITEXCOORD2DARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord2d");
		glMultiTexCoord2dvARB = (PFNGLMULTITEXCOORD2DVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord2dv");
		glMultiTexCoord2fARB = (PFNGLMULTITEXCOORD2FARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord2f");
		glMultiTexCoord2fvARB = (PFNGLMULTITEXCOORD2FVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord2fv");
		glMultiTexCoord2iARB = (PFNGLMULTITEXCOORD2IARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord2i");
		glMultiTexCoord2ivARB = (PFNGLMULTITEXCOORD2IVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord2iv");
		glMultiTexCoord2sARB = (PFNGLMULTITEXCOORD2SARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord2s");
		glMultiTexCoord2svARB = (PFNGLMULTITEXCOORD2SVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord2sv");
		glMultiTexCoord3dARB = (PFNGLMULTITEXCOORD3DARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord3d");
		glMultiTexCoord3dvARB = (PFNGLMULTITEXCOORD3DVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord3dv");
		glMultiTexCoord3fARB = (PFNGLMULTITEXCOORD3FARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord3f");
		glMultiTexCoord3fvARB = (PFNGLMULTITEXCOORD3FVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord3fv");
		glMultiTexCoord3iARB = (PFNGLMULTITEXCOORD3IARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord3i");
		glMultiTexCoord3ivARB = (PFNGLMULTITEXCOORD3IVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord3iv");
		glMultiTexCoord3sARB = (PFNGLMULTITEXCOORD3SARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord3s");
		glMultiTexCoord3svARB = (PFNGLMULTITEXCOORD3SVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord3sv");
		glMultiTexCoord4dARB = (PFNGLMULTITEXCOORD4DARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord4d");
		glMultiTexCoord4dvARB = (PFNGLMULTITEXCOORD4DVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord4dv");
		glMultiTexCoord4fARB = (PFNGLMULTITEXCOORD4FARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord4f");
		glMultiTexCoord4fvARB = (PFNGLMULTITEXCOORD4FVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord4fv");
		glMultiTexCoord4iARB = (PFNGLMULTITEXCOORD4IARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord4i");
		glMultiTexCoord4ivARB = (PFNGLMULTITEXCOORD4IVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord4iv");
		glMultiTexCoord4sARB = (PFNGLMULTITEXCOORD4SARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord4s");
		glMultiTexCoord4svARB = (PFNGLMULTITEXCOORD4SVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glMultiTexCoord4sv");
		glActiveTextureARB = (PFNGLACTIVETEXTUREARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glActiveTexture");
		glClientActiveTextureARB = (PFNGLCLIENTACTIVETEXTUREARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glClientActiveTexture");
	}
#endif
	if (mHasCompressedTextures)
	{
		glCompressedTexImage3DARB = (PFNGLCOMPRESSEDTEXIMAGE3DARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glCompressedTexImage3D");
		glCompressedTexImage2DARB = (PFNGLCOMPRESSEDTEXIMAGE2DARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glCompressedTexImage2D");
		glCompressedTexImage1DARB = (PFNGLCOMPRESSEDTEXIMAGE1DARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glCompressedTexImage1D");
		glCompressedTexSubImage3DARB = (PFNGLCOMPRESSEDTEXSUBIMAGE3DARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glCompressedTexSubImage3D");
		glCompressedTexSubImage2DARB = (PFNGLCOMPRESSEDTEXSUBIMAGE2DARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glCompressedTexSubImage2D");
		glCompressedTexSubImage1DARB = (PFNGLCOMPRESSEDTEXSUBIMAGE1DARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glCompressedTexSubImage1D");
		glGetCompressedTexImageARB = (PFNGLGETCOMPRESSEDTEXIMAGEARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.3, "glGetCompressedTexImage");
	}
	if (mHasVertexBufferObject)
	{
		glBindBufferARB = (PFNGLBINDBUFFERARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.5, "glBindBuffer");
		if (glBindBufferARB)
		{
			glDeleteBuffersARB = (PFNGLDELETEBUFFERSARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.5, "glDeleteBuffers");
			glGenBuffersARB = (PFNGLGENBUFFERSARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.5, "glGenBuffers");
			glIsBufferARB = (PFNGLISBUFFERARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.5, "glIsBuffer");
			glBufferDataARB = (PFNGLBUFFERDATAARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.5, "glBufferData");
			glBufferSubDataARB = (PFNGLBUFFERSUBDATAARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.5, "glBufferSubData");
			glGetBufferSubDataARB = (PFNGLGETBUFFERSUBDATAARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.5, "glGetBufferSubData");
			glMapBufferARB = (PFNGLMAPBUFFERARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.5, "glMapBuffer");
			glUnmapBufferARB = (PFNGLUNMAPBUFFERARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.5, "glUnmapBuffer");
			glGetBufferParameterivARB = (PFNGLGETBUFFERPARAMETERIVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.5, "glGetBufferParameteriv");
			glGetBufferPointervARB = (PFNGLGETBUFFERPOINTERVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.5, "glGetBufferPointerv");
		}
		else
		{
			mHasVertexBufferObject = FALSE;
		}
	}
	if (mHasVertexArrayObject)
	{
		glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC) GLH_EXT_GET_PROC_ADDRESS("glBindVertexArray");
		glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC) GLH_EXT_GET_PROC_ADDRESS("glDeleteVertexArrays");
		glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC) GLH_EXT_GET_PROC_ADDRESS("glGenVertexArrays");
		glIsVertexArray = (PFNGLISVERTEXARRAYPROC) GLH_EXT_GET_PROC_ADDRESS("glIsVertexArray");
	}
	if (mHasSync)
	{
		glFenceSync = (PFNGLFENCESYNCPROC) GLH_EXT_GET_PROC_ADDRESS("glFenceSync");
		glIsSync = (PFNGLISSYNCPROC) GLH_EXT_GET_PROC_ADDRESS("glIsSync");
		glDeleteSync = (PFNGLDELETESYNCPROC) GLH_EXT_GET_PROC_ADDRESS("glDeleteSync");
		glClientWaitSync = (PFNGLCLIENTWAITSYNCPROC) GLH_EXT_GET_PROC_ADDRESS("glClientWaitSync");
		glWaitSync = (PFNGLWAITSYNCPROC) GLH_EXT_GET_PROC_ADDRESS("glWaitSync");
		glGetInteger64v = (PFNGLGETINTEGER64VPROC) GLH_EXT_GET_PROC_ADDRESS("glGetInteger64v");
		glGetSynciv = (PFNGLGETSYNCIVPROC) GLH_EXT_GET_PROC_ADDRESS("glGetSynciv");
	}
	if (mHasMapBufferRange)
	{
		glMapBufferRange = (PFNGLMAPBUFFERRANGEPROC) GLH_EXT_GET_PROC_ADDRESS("glMapBufferRange");
		glFlushMappedBufferRange = (PFNGLFLUSHMAPPEDBUFFERRANGEPROC) GLH_EXT_GET_PROC_ADDRESS("glFlushMappedBufferRange");
	}
	if (mHasFramebufferObject)
	{
		LL_INFOS() << "initExtensions() FramebufferObject-related procs..." << LL_ENDL;
		glIsRenderbuffer = (PFNGLISRENDERBUFFERPROC) GLH_EXT_GET_PROC_ADDRESS("glIsRenderbuffer");
		glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC) GLH_EXT_GET_PROC_ADDRESS("glBindRenderbuffer");
		glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC) GLH_EXT_GET_PROC_ADDRESS("glDeleteRenderbuffers");
		glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC) GLH_EXT_GET_PROC_ADDRESS("glGenRenderbuffers");
		glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC) GLH_EXT_GET_PROC_ADDRESS("glRenderbufferStorage");
		glGetRenderbufferParameteriv = (PFNGLGETRENDERBUFFERPARAMETERIVPROC) GLH_EXT_GET_PROC_ADDRESS("glGetRenderbufferParameteriv");
		glIsFramebuffer = (PFNGLISFRAMEBUFFERPROC) GLH_EXT_GET_PROC_ADDRESS("glIsFramebuffer");
		glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC) GLH_EXT_GET_PROC_ADDRESS("glBindFramebuffer");
		glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC) GLH_EXT_GET_PROC_ADDRESS("glDeleteFramebuffers");
		glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC) GLH_EXT_GET_PROC_ADDRESS("glGenFramebuffers");
		glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC) GLH_EXT_GET_PROC_ADDRESS("glCheckFramebufferStatus");
		glFramebufferTexture1D = (PFNGLFRAMEBUFFERTEXTURE1DPROC) GLH_EXT_GET_PROC_ADDRESS("glFramebufferTexture1D");
		glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC) GLH_EXT_GET_PROC_ADDRESS("glFramebufferTexture2D");
		glFramebufferTexture3D = (PFNGLFRAMEBUFFERTEXTURE3DPROC) GLH_EXT_GET_PROC_ADDRESS("glFramebufferTexture3D");
		glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC) GLH_EXT_GET_PROC_ADDRESS("glFramebufferRenderbuffer");
		glGetFramebufferAttachmentParameteriv = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC) GLH_EXT_GET_PROC_ADDRESS("glGetFramebufferAttachmentParameteriv");
		glGenerateMipmap = (PFNGLGENERATEMIPMAPPROC) GLH_EXT_GET_PROC_ADDRESS("glGenerateMipmap");
		glBlitFramebuffer = (PFNGLBLITFRAMEBUFFERPROC) GLH_EXT_GET_PROC_ADDRESS("glBlitFramebuffer");
		glRenderbufferStorageMultisample = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC) GLH_EXT_GET_PROC_ADDRESS("glRenderbufferStorageMultisample");
		glFramebufferTextureLayer = (PFNGLFRAMEBUFFERTEXTURELAYERPROC) GLH_EXT_GET_PROC_ADDRESS("glFramebufferTextureLayer");
	}
	if (mHasDrawBuffers)
	{
		glDrawBuffersARB = (PFNGLDRAWBUFFERSARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glDrawBuffers");
	}
	if (mHasBlendFuncSeparate)
	{
		glBlendFuncSeparateEXT = (PFNGLBLENDFUNCSEPARATEEXTPROC)GLH_EXT_GET_PROC_ADDRESS_CORE_EXT(1.4, "glBlendFuncSeparate");
	}
	if (mHasTransformFeedback)
	{
		glBeginTransformFeedback = (PFNGLBEGINTRANSFORMFEEDBACKPROC)GLH_EXT_GET_PROC_ADDRESS_CORE_EXT(4.0, "glBeginTransformFeedback");
		glEndTransformFeedback = (PFNGLENDTRANSFORMFEEDBACKPROC)GLH_EXT_GET_PROC_ADDRESS_CORE_EXT(4.0, "glEndTransformFeedback");
		glTransformFeedbackVaryings = (PFNGLTRANSFORMFEEDBACKVARYINGSPROC)GLH_EXT_GET_PROC_ADDRESS_CORE_EXT(4.0, "glTransformFeedbackVaryings");
		glBindBufferRange = (PFNGLBINDBUFFERRANGEPROC)GLH_EXT_GET_PROC_ADDRESS_CORE_EXT(3.0, "glBindBufferRange");
	}
	if (mHasDebugOutput)
	{
		glDebugMessageControlARB = (PFNGLDEBUGMESSAGECONTROLARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(4.3, "glDebugMessageControl");
		glDebugMessageInsertARB = (PFNGLDEBUGMESSAGEINSERTARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(4.3, "glDebugMessageInsert");
		glDebugMessageCallbackARB = (PFNGLDEBUGMESSAGECALLBACKARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(4.3, "glDebugMessageCallback");
		glGetDebugMessageLogARB = (PFNGLGETDEBUGMESSAGELOGARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(4.3, "glGetDebugMessageLog");
	}
#if !LL_LINUX || LL_LINUX_NV_GL_HEADERS
	// This is expected to be a static symbol on Linux GL implementations, except if we use the nvidia headers - bah
	glDrawRangeElements = (PFNGLDRAWRANGEELEMENTSPROC)GLH_EXT_GET_PROC_ADDRESS("glDrawRangeElements");
	if (!glDrawRangeElements)
	{
		mGLMaxVertexRange = 0;
		mGLMaxIndexRange = 0;
	}
#endif // !LL_LINUX || LL_LINUX_NV_GL_HEADERS
#if LL_LINUX_NV_GL_HEADERS
	// nvidia headers are critically different from mesa-esque
 	glActiveTextureARB = (PFNGLACTIVETEXTUREARBPROC)GLH_EXT_GET_PROC_ADDRESS("glActiveTexture");
 	glClientActiveTextureARB = (PFNGLCLIENTACTIVETEXTUREARBPROC)GLH_EXT_GET_PROC_ADDRESS("glClientActiveTexture");
#endif // LL_LINUX_NV_GL_HEADERS

	if (mHasOcclusionQuery)
	{
		LL_INFOS() << "initExtensions() OcclusionQuery-related procs..." << LL_ENDL;
		glGenQueriesARB = (PFNGLGENQUERIESARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.4, "glGenQueries");
		glDeleteQueriesARB = (PFNGLDELETEQUERIESARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.4, "glDeleteQueries");
		glIsQueryARB = (PFNGLISQUERYARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.4, "glIsQuery");
		glBeginQueryARB = (PFNGLBEGINQUERYARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.4, "glBeginQuery");
		glEndQueryARB = (PFNGLENDQUERYARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.4, "glEndQuery");
		glGetQueryivARB = (PFNGLGETQUERYIVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.4, "glGetQueryiv");
		glGetQueryObjectivARB = (PFNGLGETQUERYOBJECTIVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.4, "glGetQueryObjectiv");
		glGetQueryObjectuivARB = (PFNGLGETQUERYOBJECTUIVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(1.4, "glGetQueryObjectuiv");
	}
#if !LL_DARWIN
	glGetQueryObjectui64vEXT = (PFNGLGETQUERYOBJECTUI64VEXTPROC)GLH_EXT_GET_PROC_ADDRESS_CORE_EXT(3.2, "glGetQueryObjectui64v");
#endif
	if (mHasPointParameters)
	{
		LL_INFOS() << "initExtensions() PointParameters-related procs..." << LL_ENDL;
		glPointParameterfARB = (PFNGLPOINTPARAMETERFARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glPointParameterf");
		glPointParameterfvARB = (PFNGLPOINTPARAMETERFVARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glPointParameterfv");
	}
	if (mHasShaderObjects)
	{
		glDeleteShader = (PFNGLDELETEOBJECTARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE_OR_ARB(2.0, "glDeleteShader", "glDeleteObjectARB");
		glDeleteProgram = (PFNGLDELETEOBJECTARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE_OR_ARB(2.0, "glDeleteProgram", "glDeleteObjectARB");
		glDetachObjectARB = (PFNGLDETACHOBJECTARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE_OR_ARB(2.0, "glDetachShader", "glDetachObjectARB");
		glCreateShaderObjectARB = (PFNGLCREATESHADEROBJECTARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE_OR_ARB(2.0, "glCreateShader", "glCreateShaderObjectARB");
		glShaderSourceARB = (PFNGLSHADERSOURCEARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glShaderSource");
		glCompileShaderARB = (PFNGLCOMPILESHADERARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glCompileShader");
		glCreateProgramObjectARB = (PFNGLCREATEPROGRAMOBJECTARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE_OR_ARB(2.0, "glCreateProgram", "glCreateProgramObjectARB");
		glAttachObjectARB = (PFNGLATTACHOBJECTARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE_OR_ARB(2.0, "glAttachShader", "glAttachObjectARB");
		glLinkProgramARB = (PFNGLLINKPROGRAMARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glLinkProgram");
		glUseProgramObjectARB = (PFNGLUSEPROGRAMOBJECTARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE_OR_ARB(2.0, "glUseProgram", "glUseProgramObjectARB");
		glValidateProgramARB = (PFNGLVALIDATEPROGRAMARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glValidateProgram");
		glUniform1fARB = (PFNGLUNIFORM1FARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glUniform1f");
		glUniform2fARB = (PFNGLUNIFORM2FARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glUniform2f");
		glUniform3fARB = (PFNGLUNIFORM3FARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glUniform3f");
		glUniform4fARB = (PFNGLUNIFORM4FARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glUniform4f");
		glUniform1iARB = (PFNGLUNIFORM1IARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glUniform1i");
		glUniform2iARB = (PFNGLUNIFORM2IARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glUniform2i");
		glUniform3iARB = (PFNGLUNIFORM3IARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glUniform3i");
		glUniform4iARB = (PFNGLUNIFORM4IARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glUniform4i");
		glUniform1fvARB = (PFNGLUNIFORM1FVARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glUniform1fv");
		glUniform2fvARB = (PFNGLUNIFORM2FVARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glUniform2fv");
		glUniform3fvARB = (PFNGLUNIFORM3FVARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glUniform3fv");
		glUniform4fvARB = (PFNGLUNIFORM4FVARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glUniform4fv");
		glUniform1ivARB = (PFNGLUNIFORM1IVARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glUniform1iv");
		glUniform2ivARB = (PFNGLUNIFORM2IVARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glUniform2iv");
		glUniform3ivARB = (PFNGLUNIFORM3IVARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glUniform3iv");
		glUniform4ivARB = (PFNGLUNIFORM4IVARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glUniform4iv");
		glUniformMatrix2fvARB = (PFNGLUNIFORMMATRIX2FVARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glUniformMatrix2fv");
		glUniformMatrix3fvARB = (PFNGLUNIFORMMATRIX3FVARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glUniformMatrix3fv");
		glUniformMatrix3x4fv = (PFNGLUNIFORMMATRIX3X4FVPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.1, "glUniformMatrix3x4fv");
		glUniformMatrix4fvARB = (PFNGLUNIFORMMATRIX4FVARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glUniformMatrix4fv");
		glGetShaderiv = (PFNGLGETOBJECTPARAMETERIVARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE_OR_ARB(2.0, "glGetShaderiv", "glGetObjectParameteriv");
		glGetProgramiv = (PFNGLGETOBJECTPARAMETERIVARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE_OR_ARB(2.0, "glGetProgramiv", "glGetObjectParameteriv");
		glGetShaderInfoLog = (PFNGLGETINFOLOGARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE_OR_ARB(2.0, "glGetShaderInfoLog", "glGetInfoLog");
		glGetProgramInfoLog = (PFNGLGETINFOLOGARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE_OR_ARB(2.0, "glGetProgramInfoLog", "glGetInfoLog");
		glGetAttachedObjectsARB = (PFNGLGETATTACHEDOBJECTSARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE_OR_ARB(2.0, "glGetAttachedShaders", "glGetAttachedObjects");
		glGetUniformLocationARB = (PFNGLGETUNIFORMLOCATIONARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glGetUniformLocation");
		glGetActiveUniformARB = (PFNGLGETACTIVEUNIFORMARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glGetActiveUniform");
		glGetUniformfvARB = (PFNGLGETUNIFORMFVARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glGetUniformfv");
		glGetUniformivARB = (PFNGLGETUNIFORMIVARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glGetUniformiv");
		glGetShaderSourceARB = (PFNGLGETSHADERSOURCEARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glGetShaderSource");
	}
	if (mHasVertexShader)
	{
		LL_INFOS() << "initExtensions() VertexShader-related procs..." << LL_ENDL;
		glGetAttribLocationARB = (PFNGLGETATTRIBLOCATIONARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glGetAttribLocation");
		glBindAttribLocationARB = (PFNGLBINDATTRIBLOCATIONARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glBindAttribLocation");
		glGetActiveAttribARB = (PFNGLGETACTIVEATTRIBARBPROC) GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glGetActiveAttrib");
		glVertexAttrib1dARB = (PFNGLVERTEXATTRIB1DARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib1d");
		glVertexAttrib1dvARB = (PFNGLVERTEXATTRIB1DVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib1dv");
		glVertexAttrib1fARB = (PFNGLVERTEXATTRIB1FARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib1f");
		glVertexAttrib1fvARB = (PFNGLVERTEXATTRIB1FVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib1fv");
		glVertexAttrib1sARB = (PFNGLVERTEXATTRIB1SARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib1s");
		glVertexAttrib1svARB = (PFNGLVERTEXATTRIB1SVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib1sv");
		glVertexAttrib2dARB = (PFNGLVERTEXATTRIB2DARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib2d");
		glVertexAttrib2dvARB = (PFNGLVERTEXATTRIB2DVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib2dv");
		glVertexAttrib2fARB = (PFNGLVERTEXATTRIB2FARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib2f");
		glVertexAttrib2fvARB = (PFNGLVERTEXATTRIB2FVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib2fv");
		glVertexAttrib2sARB = (PFNGLVERTEXATTRIB2SARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib2s");
		glVertexAttrib2svARB = (PFNGLVERTEXATTRIB2SVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib2sv");
		glVertexAttrib3dARB = (PFNGLVERTEXATTRIB3DARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib3d");
		glVertexAttrib3dvARB = (PFNGLVERTEXATTRIB3DVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib3dv");
		glVertexAttrib3fARB = (PFNGLVERTEXATTRIB3FARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib3f");
		glVertexAttrib3fvARB = (PFNGLVERTEXATTRIB3FVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib3fv");
		glVertexAttrib3sARB = (PFNGLVERTEXATTRIB3SARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib3s");
		glVertexAttrib3svARB = (PFNGLVERTEXATTRIB3SVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib3sv");
		glVertexAttrib4NbvARB = (PFNGLVERTEXATTRIB4NBVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib4Nbv");
		glVertexAttrib4NivARB = (PFNGLVERTEXATTRIB4NIVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib4Niv");
		glVertexAttrib4NsvARB = (PFNGLVERTEXATTRIB4NSVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib4Nsv");
		glVertexAttrib4NubARB = (PFNGLVERTEXATTRIB4NUBARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib4Nub");
		glVertexAttrib4NubvARB = (PFNGLVERTEXATTRIB4NUBVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib4Nubv");
		glVertexAttrib4NuivARB = (PFNGLVERTEXATTRIB4NUIVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib4Nuiv");
		glVertexAttrib4NusvARB = (PFNGLVERTEXATTRIB4NUSVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib4Nusv");
		glVertexAttrib4bvARB = (PFNGLVERTEXATTRIB4BVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib4bv");
		glVertexAttrib4dARB = (PFNGLVERTEXATTRIB4DARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib4d");
		glVertexAttrib4dvARB = (PFNGLVERTEXATTRIB4DVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib4dv");
		glVertexAttrib4fARB = (PFNGLVERTEXATTRIB4FARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib4f");
		glVertexAttrib4fvARB = (PFNGLVERTEXATTRIB4FVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib4fv");
		glVertexAttrib4ivARB = (PFNGLVERTEXATTRIB4IVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib4iv");
		glVertexAttrib4sARB = (PFNGLVERTEXATTRIB4SARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib4s");
		glVertexAttrib4svARB = (PFNGLVERTEXATTRIB4SVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib4sv");
		glVertexAttrib4ubvARB = (PFNGLVERTEXATTRIB4UBVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib4ubv");
		glVertexAttrib4uivARB = (PFNGLVERTEXATTRIB4UIVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib4uiv");
		glVertexAttrib4usvARB = (PFNGLVERTEXATTRIB4USVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttrib4usv");
		glVertexAttribPointerARB = (PFNGLVERTEXATTRIBPOINTERARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glVertexAttribPointer");
		glVertexAttribIPointer = (PFNGLVERTEXATTRIBIPOINTERPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(3.0, "glVertexAttribIPointer");
		glEnableVertexAttribArrayARB = (PFNGLENABLEVERTEXATTRIBARRAYARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glEnableVertexAttribArray");
		glDisableVertexAttribArrayARB = (PFNGLDISABLEVERTEXATTRIBARRAYARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glDisableVertexAttribArray");
		glGetVertexAttribdvARB = (PFNGLGETVERTEXATTRIBDVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glGetVertexAttribdv");
		glGetVertexAttribfvARB = (PFNGLGETVERTEXATTRIBFVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glGetVertexAttribfv");
		glGetVertexAttribivARB = (PFNGLGETVERTEXATTRIBIVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glGetVertexAttribiv");
		glGetVertexAttribPointervARB = (PFNGLGETVERTEXATTRIBPOINTERVARBPROC)GLH_EXT_GET_PROC_ADDRESS_CORE(2.0, "glGetVertexAttribPointerv");
	}
	LL_DEBUGS("RenderInit") << "GL Probe: Got symbols" << LL_ENDL;
#endif

	mInited = TRUE;
}

void rotate_quat(LLQuaternion& rotation)
{
	F32 angle_radians, x, y, z;
	rotation.getAngleAxis(&angle_radians, &x, &y, &z);
	gGL.rotatef(angle_radians * RAD_TO_DEG, x, y, z);
}

void flush_glerror()
{
	glGetError();
}

const std::string getGLErrorString(GLenum error)
{
	switch(error)
	{
	case GL_NO_ERROR:
		return "No Error";
	case GL_INVALID_ENUM:
		return "Invalid Enum";
	case GL_INVALID_VALUE:
		return "Invalid Value";
	case GL_INVALID_OPERATION:
		return "Invalid Operation";
	case GL_INVALID_FRAMEBUFFER_OPERATION:
		return "Invalid Framebuffer Operation";
	case GL_OUT_OF_MEMORY:
		return "Out of Memory";
	case GL_STACK_UNDERFLOW:
		return "Stack Underflow";
	case GL_STACK_OVERFLOW:
		return "Stack Overflow";
#ifdef GL_TABLE_TOO_LARGE
	case GL_TABLE_TOO_LARGE:
		return "Table too large";
#endif
	default:
		return "UNKNOWN ERROR";
	}
}

//this function outputs gl error to the log file, does not crash the code.
void log_glerror()
{
	if (LL_UNLIKELY(!gGLManager.mInited))
	{
		return ;
	}
	//  Create or update texture to be used with this data 
	GLenum error;
	error = glGetError();
	while (LL_UNLIKELY(error))
	{
		std::string gl_error_msg = getGLErrorString(error);
		LL_WARNS() << "GL Error: 0x" << std::hex << error << std::dec << " GL Error String: " << gl_error_msg << LL_ENDL;			
		error = glGetError();
	}
}

void do_assert_glerror()
{
	//  Create or update texture to be used with this data 
	GLenum error;
	error = glGetError();
	BOOL quit = FALSE;
	while (LL_UNLIKELY(error))
	{
		quit = TRUE;
		
		std::string gl_error_msg = getGLErrorString(error);
		LL_WARNS("RenderState") << "GL Error: 0x" << std::hex << error << std::dec << LL_ENDL;		
		LL_WARNS("RenderState") << "GL Error String: " << gl_error_msg << LL_ENDL;
		if (gDebugSession)
		{
			gFailLog << "GL Error: 0x" << std::hex << error << std::dec << " GL Error String: " << gl_error_msg << std::endl;
		}
		error = glGetError();
	}

	if (quit)
	{
		if (gDebugSession)
		{
			ll_fail("assert_glerror failed");
		}
		else
		{
			LL_ERRS() << "One or more unhandled GL errors." << LL_ENDL;
		}
	}
}

void assert_glerror()
{
	if(!gNoRender)
	{}
	else
	{
		return;
	}
	if (!gGLActive)
	{
		//LL_WARNS() << "GL used while not active!" << LL_ENDL;
		
		if (!gDebugSession)
		{}
		else
		{
			//ll_fail("GL used while not active");
		}
	}

	if (!gDebugGL) 
	{
		//funny looking if for branch prediction -- gDebugGL is almost always false and assert_glerror is called often
	}
	else
	{
		do_assert_glerror();
	}
}
	

void clear_glerror()
{
	//  Create or update texture to be used with this data 
	glGetError();
}

///////////////////////////////////////////////////////////////
//
// LLGLState
//

// Static members
std::vector<LLGLStateStaticData*> LLGLStateValidator::sStateDataVec;

GLboolean LLGLDepthTest::sDepthEnabled = GL_FALSE; // OpenGL default
GLenum LLGLDepthTest::sDepthFunc = GL_LESS; // OpenGL default
GLboolean LLGLDepthTest::sWriteEnabled = GL_TRUE; // OpenGL default

//static
void LLGLStateValidator::initClass()
{
	stop_glerror();

	gGL.setSceneBlendType(LLRender::BT_ALPHA);
	//make sure multisample defaults to disabled
	glDisable(GL_MULTISAMPLE_ARB);
	stop_glerror();
	for (auto data : sStateDataVec)
	{
		llassert_always(data->depth == 0);
		llassert_always(data->activeInstance == nullptr);
		if (data->disabler && *data->disabler)
		{
			continue;
		}
		const char* stateStr = data->stateStr;
		LLGLenum state = data->state;
		LLGLboolean cur_state = data->currentState;
		llassert_always_msg(cur_state == glIsEnabled(state), llformat("%s expected %s", stateStr, cur_state ? "TRUE" : "FALSE"));
	}
	const bool old = gDebugGL;
	gDebugGL = true;
	checkStates();
	gDebugGL = old;
}

//static
void LLGLStateValidator::restoreGL()
{
	initClass();
}

//static
// Really shouldn't be needed, but seems we sometimes do.
void LLGLStateValidator::resetTextureStates()
{
	gGL.flush();
	GLint maxTextureUnits;
	
	glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &maxTextureUnits);
	for (S32 j = maxTextureUnits-1; j >=0; j--)
	{
		gGL.getTexUnit(j)->activate();
		glClientActiveTextureARB(GL_TEXTURE0_ARB+j);
		j == 0 ? gGL.getTexUnit(j)->enable(LLTexUnit::TT_TEXTURE) : gGL.getTexUnit(j)->disable();
	}
}

void LLGLStateValidator::dumpStates() 
{
	LL_INFOS("RenderState") << "GL States:" << LL_ENDL;
	for (auto data : sStateDataVec)
	{
		LL_INFOS("RenderState") << llformat("%s : %s", data->stateStr, data->currentState ? "TRUE" : "FALSE") << LL_ENDL;
	}
}

void LLGLStateValidator::checkState(LLGLStateStaticData& data)
{
	if (gDebugGL)
	{
		if (data.disabler && *data.disabler)
		{
			return;
		}
		if (!gDebugSession)
		{
			llassert_always(data.currentState == (bool)glIsEnabled(data.state));
		}
		else
		{
			if (data.currentState != (bool)glIsEnabled(data.state))
			{
				ll_fail(llformat("GL enabled state for %s does not match expected state of %s.", data.stateStr, data.currentState ? "TRUE" : "FALSE"));
			}
		}
	}
}

bool LLGLStateValidator::registerStateData(LLGLStateStaticData& data)
{
	sStateDataVec.emplace_back(&data);
	return true;
}

void LLGLStateValidator::checkStates(const std::string& msg)  
{
	if (!gDebugGL || gGLManager.mIsDisabled)
	{
		return;
	}

	stop_glerror();

	GLint src = gGL.getContextSnapshot().blendColorSFactor;
	GLint dst = gGL.getContextSnapshot().blendColorDFactor;
	
	stop_glerror();

	BOOL error = FALSE;

	if (src != LLRender::BF_SOURCE_ALPHA || dst != LLRender::BF_ONE_MINUS_SOURCE_ALPHA)
	{
		if (gDebugSession)
		{
			gFailLog << "Blend function corrupted: " << std::hex << src << " " << std::hex << dst << "  " << msg << std::dec << std::endl;
			error = TRUE;
		}
		else
		{
			LL_GL_ERRS << "Blend function corrupted: " << std::hex << src << " " << std::hex << dst << "  " << msg << std::dec << LL_ENDL;
		}
	}
	
	for (auto data : sStateDataVec)
	{
		if (data->disabler && *data->disabler)
		{
			continue;
		}
		const char* stateStr = data->stateStr;
		LLGLenum state = data->state;
		LLGLboolean cur_state = data->currentState;
		stop_glerror();
		LLGLboolean gl_state = glIsEnabled(state);
		stop_glerror();
		if(cur_state != gl_state)
		{
			dumpStates();
			if (gDebugSession)
			{
				gFailLog << llformat("LLGLState error. State: %s 0x%04x. Expected %s", stateStr, state, cur_state ? "TRUE" : "FALSE") << std::endl;
				error = TRUE;
			}
			else
			{
				LL_GL_ERRS << llformat("LLGLState error. State: %s 0x%04x. Expected %s", stateStr, state, cur_state ? "TRUE" : "FALSE") << LL_ENDL;
			}
		}
	}
	
	if (error)
	{
		ll_fail("LLGLStateValidator::checkStates failed.");
	}
	stop_glerror();
}

void LLGLStateValidator::checkTextureChannels(const std::string& msg)
{
#if 0
	if (!gDebugGL)
	{
		return;
	}

	stop_glerror();

	GLint activeTexture;
	glGetIntegerv(GL_ACTIVE_TEXTURE_ARB, &activeTexture);
	stop_glerror();

	BOOL error = FALSE;

	if (activeTexture == GL_TEXTURE0_ARB)
	{
		GLint tex_env_mode = 0;

		glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &tex_env_mode);
		stop_glerror();

		if (tex_env_mode != GL_MODULATE)
		{
			error = TRUE;
			LL_WARNS("RenderState") << "GL_TEXTURE_ENV_MODE invalid: " << std::hex << tex_env_mode << std::dec << LL_ENDL;
			if (gDebugSession)
			{
				gFailLog << "GL_TEXTURE_ENV_MODE invalid: " << std::hex << tex_env_mode << std::dec << std::endl;
			}
		}
	}

	static const char* label[] =
	{
		"GL_TEXTURE_2D",
		"GL_TEXTURE_COORD_ARRAY",
		"GL_TEXTURE_1D",
		"GL_TEXTURE_CUBE_MAP_ARB",
		"GL_TEXTURE_GEN_S",
		"GL_TEXTURE_GEN_T",
		"GL_TEXTURE_GEN_Q",
		"GL_TEXTURE_GEN_R",
		"GL_TEXTURE_RECTANGLE_ARB"
	};

	static GLint value[] =
	{
		GL_TEXTURE_2D,
		GL_TEXTURE_COORD_ARRAY,
		GL_TEXTURE_1D,
		GL_TEXTURE_CUBE_MAP_ARB,
		GL_TEXTURE_GEN_S,
		GL_TEXTURE_GEN_T,
		GL_TEXTURE_GEN_Q,
		GL_TEXTURE_GEN_R,
		GL_TEXTURE_RECTANGLE_ARB
	};

	GLint stackDepth = 0;

	for (GLint i = 1; i < gGLManager.mNumTextureUnits; i++)
	{
		gGL.getTexUnit(i)->activate();
		glClientActiveTextureARB(GL_TEXTURE0_ARB+i);
		stop_glerror();
		glGetIntegerv(GL_TEXTURE_STACK_DEPTH, &stackDepth);
		stop_glerror();

		if (stackDepth != 1)
		{
			error = TRUE;
			LL_WARNS("RenderState") << "Texture matrix stack corrupted." << LL_ENDL;

			if (gDebugSession)
			{
				gFailLog << "Texture matrix stack corrupted." << std::endl;
			}
		}

		LLMatrix4a mat;
		glGetFloatv(GL_TEXTURE_MATRIX, (GLfloat*) mat.mMatrix);
		stop_glerror();

		if (!mat.isIdentity())
		{
			error = TRUE;
			LL_WARNS("RenderState") << "Texture matrix in channel " << i << " corrupt." << LL_ENDL;
			if (gDebugSession)
			{
				gFailLog << "Texture matrix in channel " << i << " corrupt." << std::endl;
			}
		}

		
		for (S32 j = (i == 0 ? 1 : 0); 
			j < 8; j++)
		{
			if (glIsEnabled(value[j]))
			{
				error = TRUE;
				LL_WARNS("RenderState") << "Texture channel " << i << " still has " << label[j] << " enabled." << LL_ENDL;
				if (gDebugSession)
				{
					gFailLog << "Texture channel " << i << " still has " << label[j] << " enabled." << std::endl;
				}
			}
			stop_glerror();
		}

		glGetFloatv(GL_TEXTURE_MATRIX, mat.m);
		stop_glerror();

		if (mat != identity)
		{
			error = TRUE;
			LL_WARNS("RenderState") << "Texture matrix " << i << " is not identity." << LL_ENDL;
			if (gDebugSession)
			{
				gFailLog << "Texture matrix " << i << " is not identity." << std::endl;
			}
		}

		{
			GLint tex = 0;
			stop_glerror();
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex);
			stop_glerror();

			if (tex != 0)
			{
				error = TRUE;
				LL_WARNS("RenderState") << "Texture channel " << i << " still has texture " << tex << " bound." << LL_ENDL;

				if (gDebugSession)
				{
					gFailLog << "Texture channel " << i << " still has texture " << tex << " bound." << std::endl;
				}
			}
		}
	}

	stop_glerror();
	gGL.getTexUnit(0)->activate();
	glClientActiveTextureARB(GL_TEXTURE0_ARB);
	stop_glerror();

	if (error)
	{
		if (gDebugSession)
		{
			ll_fail("LLGLStateValidator::checkTextureChannels failed.");
		}
		else
		{
			LL_GL_ERRS << "GL texture state corruption detected.  " << msg << LL_ENDL;
		}
	}
#endif
}

void LLGLStateValidator::checkClientArrays(const std::string& msg, U32 data_mask)
{
	if (!gDebugGL || LLGLSLShader::sNoFixedFunction)
	{
		return;
	}

	stop_glerror();
	BOOL error = FALSE;

	GLint active_texture;
	glGetIntegerv(GL_CLIENT_ACTIVE_TEXTURE_ARB, &active_texture);

	if (active_texture != GL_TEXTURE0_ARB)
	{
		LL_WARNS() << "Client active texture corrupted: " << active_texture << LL_ENDL;
		if (gDebugSession)
		{
			gFailLog << "Client active texture corrupted: " << active_texture << std::endl;
		}
		error = TRUE;
	}

	/*glGetIntegerv(GL_ACTIVE_TEXTURE_ARB, &active_texture);
	if (active_texture != GL_TEXTURE0_ARB)
	{
		LL_WARNS() << "Active texture corrupted: " << active_texture << LL_ENDL;
		if (gDebugSession)
		{
			gFailLog << "Active texture corrupted: " << active_texture << std::endl;
		}
		error = TRUE;
	}*/

	static const char* label[] =
	{
		"GL_VERTEX_ARRAY",
		"GL_NORMAL_ARRAY",
		"GL_COLOR_ARRAY",
		"GL_TEXTURE_COORD_ARRAY"
	};

	static GLint value[] =
	{
		GL_VERTEX_ARRAY,
		GL_NORMAL_ARRAY,
		GL_COLOR_ARRAY,
		GL_TEXTURE_COORD_ARRAY
	};

	 U32 mask[] = 
	{ //copied from llvertexbuffer.h
		0x0001, //MAP_VERTEX,
		0x0002, //MAP_NORMAL,
		0x0010, //MAP_COLOR,
		0x0004, //MAP_TEXCOORD
	};


	for (S32 j = 1; j < 4; j++)
	{
		if (glIsEnabled(value[j]))
		{
			if (!(mask[j] & data_mask))
			{
				error = TRUE;
				LL_WARNS("RenderState") << "GL still has " << label[j] << " enabled." << LL_ENDL;
				if (gDebugSession)
				{
					gFailLog << "GL still has " << label[j] << " enabled." << std::endl;
				}
			}
		}
		else
		{
			if (mask[j] & data_mask)
			{
				error = TRUE;
				LL_WARNS("RenderState") << "GL does not have " << label[j] << " enabled." << LL_ENDL;
				if (gDebugSession)
				{
					gFailLog << "GL does not have " << label[j] << " enabled." << std::endl;
				}
			}
		}
	}

	glClientActiveTextureARB(GL_TEXTURE1_ARB);
	gGL.getTexUnit(1)->activate();
	if (glIsEnabled(GL_TEXTURE_COORD_ARRAY))
	{
		if (!(data_mask & 0x0008))
		{
			error = TRUE;
			LL_WARNS("RenderState") << "GL still has GL_TEXTURE_COORD_ARRAY enabled on channel 1." << LL_ENDL;
			if (gDebugSession)
			{
				gFailLog << "GL still has GL_TEXTURE_COORD_ARRAY enabled on channel 1." << std::endl;
			}
		}
	}
	else
	{
		if (data_mask & 0x0008)
		{
			error = TRUE;
			LL_WARNS("RenderState") << "GL does not have GL_TEXTURE_COORD_ARRAY enabled on channel 1." << LL_ENDL;
			if (gDebugSession)
			{
				gFailLog << "GL does not have GL_TEXTURE_COORD_ARRAY enabled on channel 1." << std::endl;
			}
		}
	}

	/*if (glIsEnabled(GL_TEXTURE_2D))
	{
		if (!(data_mask & 0x0008))
		{
			error = TRUE;
			LL_WARNS("RenderState") << "GL still has GL_TEXTURE_2D enabled on channel 1." << LL_ENDL;
			if (gDebugSession)
			{
				gFailLog << "GL still has GL_TEXTURE_2D enabled on channel 1." << std::endl;
			}
		}
	}
	else
	{
		if (data_mask & 0x0008)
		{
			error = TRUE;
			LL_WARNS("RenderState") << "GL does not have GL_TEXTURE_2D enabled on channel 1." << LL_ENDL;
			if (gDebugSession)
			{
				gFailLog << "GL does not have GL_TEXTURE_2D enabled on channel 1." << std::endl;
			}
		}
	}*/

	glClientActiveTextureARB(GL_TEXTURE0_ARB);
	gGL.getTexUnit(0)->activate();

	if (gGLManager.mHasVertexShader && LLGLSLShader::sNoFixedFunction)
	{	//make sure vertex attribs are all disabled
		GLint count;
		glGetIntegerv(GL_MAX_VERTEX_ATTRIBS_ARB, &count);
		for (GLint i = 0; i < count; i++)
		{
			GLint enabled;
			glGetVertexAttribivARB((GLuint) i, GL_VERTEX_ATTRIB_ARRAY_ENABLED_ARB, &enabled);
			if (enabled)
			{
				error = TRUE;
				LL_WARNS("RenderState") << "GL still has vertex attrib array " << i << " enabled." << LL_ENDL;
				if (gDebugSession)
				{
					gFailLog <<  "GL still has vertex attrib array " << i << " enabled." << std::endl;
				}
			}
		}
	}

	if (error)
	{
		if (gDebugSession)
		{
			ll_fail("LLGLStateValidator::checkClientArrays failed.");
		}
		else
		{
			LL_GL_ERRS << "GL client array corruption detected.  " << msg << LL_ENDL;
		}
	}
}

///////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////

initLLGLState(GL_BLEND, false, nullptr);
initLLGLState(GL_CLIP_PLANE0, false, nullptr);
initLLGLState(GL_CULL_FACE, false, nullptr);
initLLGLState(GL_DEPTH_CLAMP, false, nullptr);
initLLGLState(GL_DITHER, true, nullptr);
initLLGLState(GL_LINE_SMOOTH, false, nullptr);
initLLGLState(GL_MULTISAMPLE_ARB, false, nullptr);
initLLGLState(GL_POLYGON_OFFSET_FILL, false, nullptr);
initLLGLState(GL_POLYGON_OFFSET_LINE, false, nullptr);
initLLGLState(GL_POLYGON_SMOOTH, false, nullptr);
initLLGLState(GL_SCISSOR_TEST, false, nullptr);
initLLGLState(GL_STENCIL_TEST, false, nullptr);
initLLGLState(GL_ALPHA_TEST, false, &LLGLSLShader::sNoFixedFunction);
initLLGLState(GL_COLOR_MATERIAL, false, &LLGLSLShader::sNoFixedFunction);
initLLGLState(GL_FOG, false, &LLGLSLShader::sNoFixedFunction);
initLLGLState(GL_LINE_STIPPLE, false, &LLGLSLShader::sNoFixedFunction);
initLLGLState(GL_LIGHTING, false, &LLGLSLShader::sNoFixedFunction);
initLLGLState(GL_NORMALIZE, false, &LLGLSLShader::sNoFixedFunction);
initLLGLState(GL_POLYGON_STIPPLE, false, &LLGLSLShader::sNoFixedFunction);
initLLGLState(GL_TEXTURE_GEN_Q, false, &LLGLSLShader::sNoFixedFunction);
initLLGLState(GL_TEXTURE_GEN_R, false, &LLGLSLShader::sNoFixedFunction);
initLLGLState(GL_TEXTURE_GEN_S, false, &LLGLSLShader::sNoFixedFunction);
initLLGLState(GL_TEXTURE_GEN_T, false, &LLGLSLShader::sNoFixedFunction);

void LLGLManager::initGLStates()
{
	//gl states moved to classes in llglstates.h
	LLGLStateValidator::initClass();
}

////////////////////////////////////////////////////////////////////////////////

void parse_gl_version( S32* major, S32* minor, S32* release, std::string* vendor_specific, std::string* version_string )
{
	// GL_VERSION returns a null-terminated string with the format: 
	// <major>.<minor>[.<release>] [<vendor specific>]

	const char* version = (const char*) glGetString(GL_VERSION);
	*major = 0;
	*minor = 0;
	*release = 0;
	vendor_specific->assign("");

	if( !version )
	{
		return;
	}

	version_string->assign(version);

	std::string ver_copy( version );
	S32 len = (S32)strlen( version );	/* Flawfinder: ignore */
	S32 i = 0;
	S32 start;
	// Find the major version
	start = i;
	for( ; i < len; i++ )
	{
		if( '.' == version[i] )
		{
			break;
		}
	}
	std::string major_str = ver_copy.substr(start,i-start);
	LLStringUtil::convertToS32(major_str, *major);

	if( '.' == version[i] )
	{
		i++;
	}

	// Find the minor version
	start = i;
	for( ; i < len; i++ )
	{
		if( ('.' == version[i]) || isspace(version[i]) )
		{
			break;
		}
	}
	std::string minor_str = ver_copy.substr(start,i-start);
	LLStringUtil::convertToS32(minor_str, *minor);

	// Find the release number (optional)
	if( '.' == version[i] )
	{
		i++;

		start = i;
		for( ; i < len; i++ )
		{
			if( isspace(version[i]) )
			{
				break;
			}
		}

		std::string release_str = ver_copy.substr(start,i-start);
		LLStringUtil::convertToS32(release_str, *release);
	}

	// Skip over any white space
	while( version[i] && isspace( version[i] ) )
	{
		i++;
	}

	// Copy the vendor-specific string (optional)
	if( version[i] )
	{
		vendor_specific->assign( version + i );
	}
}


void parse_glsl_version(S32& major, S32& minor)
{
	// GL_SHADING_LANGUAGE_VERSION returns a null-terminated string with the format: 
	// <major>.<minor>[.<release>] [<vendor specific>]

	const char* version = (const char*) glGetString(GL_SHADING_LANGUAGE_VERSION);
	major = 0;
	minor = 0;
	
	if( !version )
	{
		return;
	}

	std::string ver_copy( version );
	S32 len = (S32)strlen( version );	/* Flawfinder: ignore */
	S32 i = 0;
	S32 start;
	// Find the major version
	start = i;
	for( ; i < len; i++ )
	{
		if( '.' == version[i] )
		{
			break;
		}
	}
	std::string major_str = ver_copy.substr(start,i-start);
	LLStringUtil::convertToS32(major_str, major);

	if( '.' == version[i] )
	{
		i++;
	}

	// Find the minor version
	start = i;
	for( ; i < len; i++ )
	{
		if( ('.' == version[i]) || isspace(version[i]) )
		{
			break;
		}
	}
	std::string minor_str = ver_copy.substr(start,i-start);
	LLStringUtil::convertToS32(minor_str, minor);
}

LLGLUserClipPlane::LLGLUserClipPlane(const LLPlane& p, const LLMatrix4a& modelview, const LLMatrix4a& projection, bool apply)
{
	mApply = apply;

	if (mApply)
	{
		mModelview = modelview;
		mProjection = projection;

		setPlane(p[0], p[1], p[2], p[3]);
	}
}

void LLGLUserClipPlane::setPlane(F32 a, F32 b, F32 c, F32 d)
{
    LLMatrix4a& P = mProjection;
	LLMatrix4a& M = mModelview;

	LLMatrix4a invtrans_MVP;
	invtrans_MVP.setMul(P,M);
	invtrans_MVP.invert();
	invtrans_MVP.transpose();

	LLVector4a oplane(a,b,c,d);
	LLVector4a cplane;
	LLVector4a cplane_splat;
	LLVector4a cplane_neg;

	invtrans_MVP.rotate4(oplane,cplane);
	
	cplane_splat.splat<2>(cplane);
	cplane_splat.setAbs(cplane_splat);
	cplane.div(cplane_splat);
	cplane.sub(LLVector4a(0.f,0.f,0.f,1.f));

	cplane_splat.splat<2>(cplane);
	cplane_neg = cplane;
	cplane_neg.negate();

	cplane.setSelectWithMask( cplane_splat.lessThan( _mm_setzero_ps() ), cplane_neg, cplane );

	LLMatrix4a suffix;
	suffix.setIdentity();
	suffix.setColumn<2>(cplane);
	LLMatrix4a newP;
	newP.setMul(suffix,P);

    gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
    gGL.loadMatrix(newP);
	//gGLObliqueProjectionInverse = LLMatrix4(newP.inverse().transpose().m);
    gGL.matrixMode(LLRender::MM_MODELVIEW);
}

LLGLUserClipPlane::~LLGLUserClipPlane()
{
	if (mApply)
	{
		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.popMatrix();
		gGL.matrixMode(LLRender::MM_MODELVIEW);
	}
}

LLGLNamePool::LLGLNamePool()
{
}

LLGLNamePool::~LLGLNamePool()
{
}

void LLGLNamePool::upkeep()
{
	std::sort(mNameList.begin(), mNameList.end(), CompareUsed());
}

void LLGLNamePool::cleanup()
{
	for (name_list_t::iterator iter = mNameList.begin(); iter != mNameList.end(); ++iter)
	{
		releaseName(iter->name);
	}

	mNameList.clear();
}

GLuint LLGLNamePool::allocate()
{
#if LL_GL_NAME_POOLING
	for (name_list_t::iterator iter = mNameList.begin(); iter != mNameList.end(); ++iter)
	{
		if (!iter->used)
		{
			iter->used = TRUE;
			return iter->name;
		}
	}

	NameEntry entry;
	entry.name = allocateName();
	entry.used = TRUE;
	mNameList.push_back(entry);

	return entry.name;
#else
	return allocateName();
#endif
}

void LLGLNamePool::release(GLuint name)
{
#if LL_GL_NAME_POOLING
	for (name_list_t::iterator iter = mNameList.begin(); iter != mNameList.end(); ++iter)
	{
		if (iter->name == name)
		{
			if (iter->used)
			{
				iter->used = FALSE;
				return;
			}
			else
			{
				LL_ERRS() << "Attempted to release a pooled name that is not in use!" << LL_ENDL;
			}
		}
	}
	LL_ERRS() << "Attempted to release a non pooled name!" << LL_ENDL;
#else
	releaseName(name);
#endif
}

//static
void LLGLNamePool::upkeepPools()
{
	for (tracker_t::instance_iter iter = beginInstances(), iter_end = endInstances(); iter != iter_end; ++iter)
	{
		LLGLNamePool & pool = *iter;
		pool.upkeep();
	}
}

//static
void LLGLNamePool::cleanupPools()
{
	for (tracker_t::instance_iter iter = beginInstances(), iter_end = endInstances(); iter != iter_end; ++iter)
	{
		LLGLNamePool & pool = *iter;
		pool.cleanup();
	}
}

LLGLDepthTest::LLGLDepthTest(GLboolean depth_enabled, GLboolean write_enabled, GLenum depth_func)
: mPrevDepthEnabled(sDepthEnabled), mPrevDepthFunc(sDepthFunc), mPrevWriteEnabled(sWriteEnabled)
{
	stop_glerror();
	
	checkState();

	if (!depth_enabled)
	{ // always disable depth writes if depth testing is disabled
	  // GL spec defines this as a requirement, but some implementations allow depth writes with testing disabled
	  // The proper way to write to depth buffer with testing disabled is to enable testing and use a depth_func of GL_ALWAYS
		write_enabled = FALSE;
	}

	if (depth_enabled != sDepthEnabled)
	{
		gGL.flush();
		if (depth_enabled) glEnable(GL_DEPTH_TEST);
		else glDisable(GL_DEPTH_TEST);
		sDepthEnabled = depth_enabled;
	}
	if (depth_func != sDepthFunc)
	{
		gGL.flush();
		glDepthFunc(depth_func);
		sDepthFunc = depth_func;
	}
	if (write_enabled != sWriteEnabled)
	{
		gGL.flush();
		glDepthMask(write_enabled);
		sWriteEnabled = write_enabled;
	}
}

LLGLDepthTest::~LLGLDepthTest()
{
	checkState();
	if (sDepthEnabled != mPrevDepthEnabled )
	{
		gGL.flush();
		if (mPrevDepthEnabled) glEnable(GL_DEPTH_TEST);
		else glDisable(GL_DEPTH_TEST);
		sDepthEnabled = mPrevDepthEnabled;
	}
	if (sDepthFunc != mPrevDepthFunc)
	{
		gGL.flush();
		glDepthFunc(mPrevDepthFunc);
		sDepthFunc = mPrevDepthFunc;
	}
	if (sWriteEnabled != mPrevWriteEnabled )
	{
		gGL.flush();
		glDepthMask(mPrevWriteEnabled);
		sWriteEnabled = mPrevWriteEnabled;
	}
}

void LLGLDepthTest::checkState()
{
	if (gDebugGL)
	{
		GLint func = 0;
		GLboolean mask = FALSE;

		glGetIntegerv(GL_DEPTH_FUNC, &func);
		glGetBooleanv(GL_DEPTH_WRITEMASK, &mask);

		if (glIsEnabled(GL_DEPTH_TEST) != sDepthEnabled ||
			sWriteEnabled != mask ||
			sDepthFunc != func)
		{
			if (gDebugSession)
			{
				gFailLog << "Unexpected depth testing state." << std::endl;
			}
			else
			{
				LL_GL_ERRS << "Unexpected depth testing state." << LL_ENDL;
			}
		}
	}
}

LLGLSquashToFarClip::LLGLSquashToFarClip(const LLMatrix4a& P_in, U32 layer)
{
	LLMatrix4a P = P_in;
	F32 depth = 0.99999f - 0.0001f * layer;

	LLVector4a col = P.getColumn<3>();
	col.mul(depth);
	P.setColumn<2>(col);

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadMatrix(P);
	gGL.matrixMode(LLRender::MM_MODELVIEW);
}

LLGLSquashToFarClip::~LLGLSquashToFarClip()
{
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
}


	
LLGLSyncFence::LLGLSyncFence()
{
#ifdef GL_ARB_sync
	mSync = 0;
#endif
}

LLGLSyncFence::~LLGLSyncFence()
{
#ifdef GL_ARB_sync
	if (mSync)
	{
		glDeleteSync(mSync);
	}
#endif
}

void LLGLSyncFence::placeFence()
{
#ifdef GL_ARB_sync
	if (mSync)
	{
		glDeleteSync(mSync);
	}
	mSync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
#endif
}

bool LLGLSyncFence::isCompleted()
{
	bool ret = true;
#ifdef GL_ARB_sync
	if (mSync)
	{
		GLenum status = glClientWaitSync(mSync, 0, 1);
		if (status == GL_TIMEOUT_EXPIRED)
		{
			ret = false;
		}
	}
#endif
	return ret;
}

void LLGLSyncFence::wait()
{
#ifdef GL_ARB_sync
	if (mSync)
	{
		while (glClientWaitSync(mSync, 0, FENCE_WAIT_TIME_NANOSECONDS) == GL_TIMEOUT_EXPIRED)
		{ //track the number of times we've waited here
			static S32 waits = 0;
			waits++;
		}
	}
#endif
}

