/** 
 * @file llui.h
 * @brief General static UI services.
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


#ifndef LL_LLUI_H
#define LL_LLUI_H

#include "llrect.h"
#include "llcontrol.h"
#include "llcoord.h"
#include "v2math.h"
#include "llregistry.h"
#include "llrender2dutils.h"
#include "llpointer.h"
#include "lluiimage.h"
#include <boost/signals2.hpp>

// LLUIFactory
#include "llsd.h"

class LLHtmlHelp;
class LLUUID;
class LLWindow;
class LLView;

void make_ui_sound(const char* name);

// Used to hide the flashing text cursor when window doesn't have focus.
extern BOOL gShowTextEditCursor;

class LLImageProviderInterface;

typedef	void (*LLUIAudioCallback)(const LLUUID& uuid);

class LLUI
{
	LOG_CLASS(LLUI);
public:
	//
	// Methods
	//
	static void initClass(LLControlGroup* config, 
						  LLControlGroup* account,
						  LLControlGroup* ignores,
						  LLControlGroup* colors, 
						  LLImageProviderInterface* image_provider,
						  LLUIAudioCallback audio_callback = NULL,
						  const LLVector2 *scale_factor = NULL,
						  const std::string& language = LLStringUtil::null);
	static void cleanupClass();

	static void pushMatrix() { LLRender2D::pushMatrix(); }
	static void popMatrix() { LLRender2D::popMatrix(); }
	static void loadIdentity() { LLRender2D::loadIdentity(); }
	static void translate(F32 x, F32 y, F32 z = 0.0f) { LLRender2D::translate(x, y, z); }

	// Return the ISO639 language name ("en", "ko", etc.) for the viewer UI.
	// http://www.loc.gov/standards/iso639-2/php/code_list.php
	static std::string getLanguage();

	//helper functions (should probably move free standing rendering helper functions here)
	static LLView* getRootView() { return sRootView; }
	static void setRootView(LLView* view) { sRootView = view; }
	static std::string locateSkin(const std::string& filename);
	static void setMousePositionScreen(S32 x, S32 y);
	static void getMousePositionScreen(S32 *x, S32 *y);
	static void setMousePositionLocal(const LLView* viewp, S32 x, S32 y);
	static void getMousePositionLocal(const LLView* viewp, S32 *x, S32 *y);
	static LLVector2& getScaleFactor() { return LLRender2D::sGLScaleFactor; }
	static void setScaleFactor(const LLVector2& scale_factor) { LLRender2D::setScaleFactor(scale_factor); }
	static void setLineWidth(F32 width) { LLRender2D::setLineWidth(width); }
	static LLPointer<LLUIImage> getUIImageByID(const LLUUID& image_id, S32 priority = 0)
		{ return LLRender2D::getUIImageByID(image_id, priority); }
	static LLPointer<LLUIImage> getUIImage(const std::string& name, S32 priority = 0)
		{ return LLRender2D::getUIImage(name, priority); }
	static LLVector2 getWindowSize();
	static void screenPointToGL(S32 screen_x, S32 screen_y, S32 *gl_x, S32 *gl_y);
	static void glPointToScreen(S32 gl_x, S32 gl_y, S32 *screen_x, S32 *screen_y);
	static void screenRectToGL(const LLRect& screen, LLRect *gl);
	static void glRectToScreen(const LLRect& gl, LLRect *screen);
	// Returns the control group containing the control name, or the default group
	static LLControlGroup& getControlControlGroup (const std::string& controlname);
	static LLWindow* getWindow() { return sWindow; }
	static void setHtmlHelp(LLHtmlHelp* html_help);

	//
	// Data
	//
	static LLControlGroup* sConfigGroup;
	static LLControlGroup* sAccountGroup;
	static LLControlGroup* sIgnoresGroup;
	static LLControlGroup* sColorsGroup;
	static LLUIAudioCallback sAudioCallback;
	static LLWindow*		sWindow;
	static LLView*			sRootView;
	static BOOL             sShowXUINames;
	static LLHtmlHelp*		sHtmlHelp;

	// *TODO: remove the following when QAR-369 settings clean-up work is in.
	// Also remove the call to this method which will then be obsolete.
	// Search for QAR-369 below to enable the proper accessing of this feature. -MG
	static void setQAMode(BOOL b);
	static BOOL sQAMode;

};

//	FactoryPolicy is a static class that controls the creation and lookup of UI elements, 
//	such as floaters.
//	The key parameter is used to provide a unique identifier and/or associated construction 
//	parameters for a given UI instance
//
//	Specialize this traits for different types, or provide a class with an identical interface 
//	in the place of the traits parameter
//
//	For example:
//
//	template <>
//	class FactoryPolicy<MyClass> /* FactoryPolicy specialized for MyClass */
//	{
//	public:
//		static MyClass* findInstance(const LLSD& key = LLSD())
//		{
//			/* return instance of MyClass associated with key */
//		}
//	
//		static MyClass* createInstance(const LLSD& key = LLSD())
//		{
//			/* create new instance of MyClass using key for construction parameters */
//		}
//	}
//	
//	class MyClass : public LLUIFactory<MyClass>
//	{
//		/* uses FactoryPolicy<MyClass> by default */
//	}

template <class T>
class FactoryPolicy
{
public:
	// basic factory methods
	static T* findInstance(const LLSD& key); // unimplemented, provide specialiation
	static T* createInstance(const LLSD& key); // unimplemented, provide specialiation
};

//	VisibilityPolicy controls the visibility of UI elements, such as floaters.
//	The key parameter is used to store the unique identifier of a given UI instance
//
//	Specialize this traits for different types, or duplicate this interface for specific instances
//	(see above)

template <class T>
class VisibilityPolicy
{
public:
	// visibility methods
	static bool visible(T* instance, const LLSD& key); // unimplemented, provide specialiation
	static void show(T* instance, const LLSD& key); // unimplemented, provide specialiation
	static void hide(T* instance, const LLSD& key); // unimplemented, provide specialiation
};

//	Manages generation of UI elements by LLSD, such that (generally) there is
//	a unique instance per distinct LLSD parameter
//	Class T is the instance type being managed, and the FACTORY_POLICY and VISIBILITY_POLICY 
//	classes provide static methods for creating, accessing, showing and hiding the associated 
//	element T
template <class T, class FACTORY_POLICY = FactoryPolicy<T>, class VISIBILITY_POLICY = VisibilityPolicy<T> >
class LLUIFactory
{
public:
	// give names to the template parameters so derived classes can refer to them
	// except this doesn't work in gcc
	typedef FACTORY_POLICY factory_policy_t;
	typedef VISIBILITY_POLICY visibility_policy_t;

	LLUIFactory()
	{
	}

 	virtual ~LLUIFactory() 
	{ 
	}

	// default show and hide methods
	static T* showInstance(const LLSD& key = LLSD()) 
	{ 
		T* instance = getInstance(key); 
		if (instance != NULL)
		{
			VISIBILITY_POLICY::show(instance, key);
		}
		return instance;
	}

	static void hideInstance(const LLSD& key = LLSD()) 
	{ 
		T* instance = getInstance(key); 
		if (instance != NULL)
		{
			VISIBILITY_POLICY::hide(instance, key);
		}
	}

	static void toggleInstance(const LLSD& key = LLSD())
	{
		if (instanceVisible(key))
		{
			hideInstance(key);
		}
		else
		{
			showInstance(key);
		}
	}

	static bool instanceVisible(const LLSD& key = LLSD())
	{
		T* instance = FACTORY_POLICY::findInstance(key);
		return instance != NULL && VISIBILITY_POLICY::visible(instance, key);
	}

	static T* getInstance(const LLSD& key = LLSD()) 
	{
		T* instance = FACTORY_POLICY::findInstance(key);
		if (instance == NULL)
		{
			instance = FACTORY_POLICY::createInstance(key);
		}
		return instance;
	}

};


//	Creates a UI singleton by ignoring the identifying parameter
//	and always generating the same instance via the LLUIFactory interface.
//	Note that since UI elements can be destroyed by their hierarchy, this singleton
//	pattern uses a static pointer to an instance that will be re-created as needed.
//	
//	Usage Pattern:
//	
//	class LLFloaterFoo : public LLFloater, public LLUISingleton<LLFloaterFoo>
//	{
//		friend class LLUISingleton<LLFloaterFoo>;
//		private:
//			LLFloaterFoo(const LLSD& key);
//	};
//	
//	Note that LLUISingleton takes an option VisibilityPolicy parameter that defines
//	how showInstance(), hideInstance(), etc. work.
// 
//  https://wiki.lindenlab.com/mediawiki/index.php?title=LLUISingleton&oldid=79352

template <class T, class VISIBILITY_POLICY = VisibilityPolicy<T> >
class LLUISingleton: public LLUIFactory<T, LLUISingleton<T, VISIBILITY_POLICY>, VISIBILITY_POLICY>
{
protected:

	// T must derive from LLUISingleton<T>
	LLUISingleton() { sInstance = static_cast<T*>(this); }

	~LLUISingleton() { sInstance = NULL; }

public:
	static T* findInstance(const LLSD& key = LLSD())
	{
		return sInstance;
	}
	
	static T* createInstance(const LLSD& key = LLSD())
	{
		if (sInstance == NULL)
		{
			sInstance = new T(key);
		}
		return sInstance;
	}

private:
	static T*	sInstance;
};

template <class T, class U> T* LLUISingleton<T,U>::sInstance = NULL;

// Moved LLLocalClipRect to lllocalcliprect.h

class LLCallbackRegistry
{
public:
	typedef boost::signals2::signal<void()> callback_signal_t;
	
	void registerCallback(const callback_signal_t::slot_type& slot)
	{
		mCallbacks.connect(slot);
	}

	void fireCallbacks()
	{
		mCallbacks();
	}

private:
	callback_signal_t mCallbacks;
};

class LLInitClassList : 
	public LLCallbackRegistry, 
	public LLSingleton<LLInitClassList>
{
	friend class LLSingleton<LLInitClassList>;
private:
	LLInitClassList() {}
};

class LLDestroyClassList : 
	public LLCallbackRegistry, 
	public LLSingleton<LLDestroyClassList>
{
	friend class LLSingleton<LLDestroyClassList>;
private:
	LLDestroyClassList() {}
};

template<typename T>
class LLRegisterWith
{
public:
	LLRegisterWith(boost::function<void ()> func)
	{
		T::instance().registerCallback(func);
	}

	// this avoids a MSVC bug where non-referenced static members are "optimized" away
	// even if their constructors have side effects
	void reference()
	{
#if LL_WINDOWS
		S32 dummy;
		dummy = 0;
#endif /*LL_WINDOWS*/
	}
};

template<typename T>
class LLInitClass
{
public:
	LLInitClass() { sRegister.reference(); }

	static LLRegisterWith<LLInitClassList> sRegister;
private:

	static void initClass()
	{
		llerrs << "No static initClass() method defined for " << typeid(T).name() << llendl;
	}
};

template<typename T>
class LLDestroyClass
{
public:
	LLDestroyClass() { sRegister.reference(); }

	static LLRegisterWith<LLDestroyClassList> sRegister;
private:

	static void destroyClass()
	{
		llerrs << "No static destroyClass() method defined for " << typeid(T).name() << llendl;
	}
};

template <typename T> LLRegisterWith<LLInitClassList> LLInitClass<T>::sRegister(&T::initClass);
template <typename T> LLRegisterWith<LLDestroyClassList> LLDestroyClass<T>::sRegister(&T::destroyClass);


template <class T>
class LLUICachedControl : public LLCachedControl<T>
{
public:
	// This constructor will declare a control if it doesn't exist in the contol group
	LLUICachedControl(const std::string& name,
					  const T& default_value,
					  const std::string& comment = "Declared In Code")
	:	LLCachedControl<T>(LLUI::getControlControlGroup(name), name, default_value, comment)
	{}

	// This constructor will signal an error if the control doesn't exist in the control group
	LLUICachedControl(const std::string& name)
	:	LLCachedControl<T>(LLUI::getControlControlGroup(name), name)
	{}
};


template <typename DERIVED>
class LLParamBlock
{
protected:
	LLParamBlock() { sBlock = (DERIVED*)this; }

	typedef typename boost::add_const<DERIVED>::type Tconst;

	template <typename T>
	class LLMandatoryParam
	{
	public:
		typedef typename boost::add_const<T>::type T_const;

		LLMandatoryParam(T_const initial_val) : mVal(initial_val), mBlock(sBlock) {}
		LLMandatoryParam(const LLMandatoryParam<T>& other) : mVal(other.mVal) {}

		DERIVED& operator ()(T_const set_value) { mVal = set_value; return *mBlock; }
		operator T() const { return mVal; } 
		T operator=(T_const set_value) { mVal = set_value; return mVal; }

	private: 
		T	mVal;
		DERIVED* mBlock;
	};

	template <typename T>
	class LLOptionalParam
	{
	public:
		typedef typename boost::add_const<T>::type T_const;

		LLOptionalParam(T_const initial_val) : mVal(initial_val), mBlock(sBlock) {}
		LLOptionalParam() : mBlock(sBlock) {}
		LLOptionalParam(const LLOptionalParam<T>& other) : mVal(other.mVal) {}

		DERIVED& operator ()(T_const set_value) { mVal = set_value; return *mBlock; }
		operator T() const { return mVal; } 
		T operator=(T_const set_value) { mVal = set_value; return mVal; }

	private:
		T	mVal;
		DERIVED* mBlock;
	};

	// specialization that requires initialization for reference types 
	template <typename T>
	class LLOptionalParam <T&>
	{
	public:
		typedef typename boost::add_const<T&>::type T_const;

		LLOptionalParam(T_const initial_val) : mVal(initial_val), mBlock(sBlock) {}
		LLOptionalParam(const LLOptionalParam<T&>& other) : mVal(other.mVal) {}

		DERIVED& operator ()(T_const set_value) { mVal = set_value; return *mBlock; }
		operator T&() const { return mVal; } 
		T& operator=(T_const set_value) { mVal = set_value; return mVal; }

	private:
		T&	mVal;
		DERIVED* mBlock;
	};

	// specialization that initializes pointer params to NULL
	template<typename T> 
	class LLOptionalParam<T*>
	{
	public:
		typedef typename boost::add_const<T*>::type T_const;

		LLOptionalParam(T_const initial_val) : mVal(initial_val), mBlock(sBlock) {}
		LLOptionalParam() : mVal((T*)NULL), mBlock(sBlock)  {}
		LLOptionalParam(const LLOptionalParam<T*>& other) : mVal(other.mVal) {}

		DERIVED& operator ()(T_const set_value) { mVal = set_value; return *mBlock; }
		operator T*() const { return mVal; } 
		T* operator=(T_const set_value) { mVal = set_value; return mVal; }
	private:
		T*	mVal;
		DERIVED* mBlock;
	};

	static DERIVED* sBlock;
};

template <typename T> T* LLParamBlock<T>::sBlock = NULL;

#endif
