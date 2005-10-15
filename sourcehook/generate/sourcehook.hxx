/* ======== SourceHook ========
* Copyright (C) 2004-2005 Metamod:Source Development Team
* No warranties of any kind
*
* License: zlib/libpng
*
* Author(s): Pavol "PM OnoTo" Marko
* ============================
*/

/**
*	@file sourcehook.h
*	@brief Contains the public SourceHook API
*/

#ifndef __SOURCEHOOK_H__
#define __SOURCEHOOK_H__

// Interface revisions:
//  1 - Initial revision
//  2 - Changed to virtual functions for iterators and all queries
//  3 - Added "hook loop status variable"
//  4 - Reentrant

#define SH_IFACE_VERSION 4
#define SH_IMPL_VERSION 3

// The value of SH_GLOB_SHPTR has to be a pointer to SourceHook::ISourceHook
// It's used in various macros
#ifndef SH_GLOB_SHPTR
#define SH_GLOB_SHPTR g_SHPtr
#endif

// Used to identify the current plugin
#ifndef SH_GLOB_PLUGPTR
#define SH_GLOB_PLUGPTR g_PLID
#endif

#ifdef SH_DEBUG
# include <stdio.h>
# include <stdlib.h>

# define SH_ASSERT(x, info) \
	do { \
		if (!(x)) \
		{ \
			printf("SOURCEHOOK DEBUG ASSERTION FAILED: %s:%u(%s): %s: ", __FILE__, __LINE__, __FUNCTION__, #x); \
			printf info; \
			putchar('\n'); \
			abort(); \
		} \
	} while(0)

#else
# define SH_ASSERT(x, info)
#endif

// System
#define SH_SYS_WIN32 1
#define SH_SYS_LINUX 2

#ifdef _WIN32
# define SH_SYS SH_SYS_WIN32
#elif defined __linux__
# define SH_SYS SH_SYS_LINUX
#else
# error Unsupported system
#endif

// Compiler
#define SH_COMP_GCC 1
#define SH_COMP_MSVC 2

#ifdef _MSC_VER
# define SH_COMP SH_COMP_MSVC
#elif defined __GNUC__
# define SH_COMP SH_COMP_GCC
#else
# error Unsupported compiler
#endif

#if SH_COMP==SH_COMP_MSVC
# define vsnprintf _vsnprintf
#endif

#define SH_PTRSIZE sizeof(void*)

#include "FastDelegate.h"
#include "sh_memfuncinfo.h"

// Good old metamod!

// Flags returned by a plugin's api function.
// NOTE: order is crucial, as greater/less comparisons are made.
enum META_RES
{
	MRES_IGNORED=0,		// plugin didn't take any action
	MRES_HANDLED,		// plugin did something, but real function should still be called
	MRES_OVERRIDE,		// call real function, but use my return value
	MRES_SUPERCEDE		// skip real function; use my return value
};


namespace SourceHook
{
	/**
	*	@brief	Specifies the size (in bytes) for the internal buffer of vafmt(printf-like) function handlers
	*/
	const int STRBUF_LEN=4096;

	/**
	*	@brief	An empty class. No inheritance used. Used for original-function-call hacks
	*/
	class EmptyClass
	{
	};

	/**
	*	@brief Implicit cast.
	*/
	template <class In, class Out>
		inline Out implicit_cast(In input)
		{
			return input;
		}

	/**
	*	@brief A plugin typedef
	*
	*	SourceHook doesn't really care what this is. As long as the ==, != and = operators work on it and every
	*	plugin has a unique identifier, everything is ok.
	*	It should also be possible to set it to 0.
	*/
	typedef int Plugin;

	/**
	*	@brief Specifies the actions for hook managers
	*/
	enum HookManagerAction
	{
		HA_GetInfo = 0,			//!< Store info about the hook manager
		HA_Register,			//!< Save the IHookManagerInfo pointer for future reference
		HA_Unregister			//!< Clear the saved pointer
	};

	struct IHookManagerInfo;

	/**
	*	@brief Pointer to hook manager interface function
	*
	*	A "hook manager" is a the only thing that knows the actual protoype of the function at compile time.
	*
	*   @param ha What the hook manager should do
	*	@param hi A pointer to IHookManagerInfo
	*/
	typedef int (*HookManagerPubFunc)(HookManagerAction ha, IHookManagerInfo *hi);

	class ISHDelegate
	{
	public:
		virtual void DeleteThis() = 0;				// Ugly, I know
		virtual bool IsEqual(ISHDelegate *other) = 0;
	};

	template <class T> class CSHDelegate : public ISHDelegate
	{
		T m_Deleg;
	public:
		CSHDelegate(T deleg) : m_Deleg(deleg)
		{
		}

		CSHDelegate(const CSHDelegate &other) : m_Deleg(other.m_Deleg)
		{
		}

		void DeleteThis()
		{
			delete this;
		}

		bool IsEqual(ISHDelegate *other)
		{
			return static_cast<CSHDelegate<T>* >(other)->GetDeleg() == GetDeleg();
		}

		T &GetDeleg()
		{
			return m_Deleg;
		}
	};

	struct IHookList
	{
		struct IIter
		{
			virtual bool End() = 0;
			virtual void Next() = 0;
			virtual ISHDelegate *Handler() = 0;
			virtual int ThisPtrOffs() = 0;
		};
		virtual IIter *GetIter() = 0;
		virtual void ReleaseIter(IIter *pIter) = 0;
	};

	struct IIface
	{
		virtual void *GetPtr() = 0;
		virtual IHookList *GetPreHooks() = 0;
		virtual IHookList *GetPostHooks() = 0;
	};

	struct IVfnPtr
	{
		virtual void *GetVfnPtr() = 0;
		virtual void *GetOrigEntry() = 0;

		virtual IIface *FindIface(void *ptr) = 0;
	};

	struct IHookManagerInfo
	{
		virtual IVfnPtr *FindVfnPtr(void *vfnptr) = 0;

		virtual void SetInfo(int vtbloffs, int vtblidx, const char *proto) = 0;
		virtual void SetHookfuncVfnptr(void *hookfunc_vfnptr) = 0;
	};

	class AutoHookIter
	{
		IHookList *m_pList;
		IHookList::IIter *m_pIter;
	public:
		AutoHookIter(IHookList *pList)
			: m_pList(pList), m_pIter(pList->GetIter())
		{
		}

		~AutoHookIter()
		{
			if (m_pList)
				m_pList->ReleaseIter(m_pIter);
		}

		bool End()
		{
			return m_pIter->End();
		}

		void Next()
		{
			m_pIter->Next();
		}

		ISHDelegate *Handler()
		{
			return m_pIter->Handler();
		}

		int ThisPtrOffs()
		{
			return m_pIter->ThisPtrOffs();
		}

		void SetToZero()
		{
			m_pList = 0;
		}
	};

	template<class B> struct CallClass
	{
		virtual B *GetThisPtr() = 0;
		virtual void *GetOrigFunc(int vtbloffs, int vtblidx) = 0;
	};

	typedef CallClass<void> GenericCallClass;

	/**
	*	@brief The main SourceHook interface
	*/
	class ISourceHook
	{
	public:
		/**
		*	@brief Return interface version
		*/
		virtual int GetIfaceVersion() = 0;

		/**
		*	@brief Return implementation version
		*/
		virtual int GetImplVersion() = 0;

		/**
		*	@brief Add a hook.
		*
		*	@return True if the function succeeded, false otherwise
		*
		*	@param plug The unique identifier of the plugin that calls this function
		*	@param iface The interface pointer
		*	@param ifacesize The size of the class iface points to
		*	@param myHookMan A hook manager function that should be capable of handling the function
		*	@param handler A pointer to a FastDelegate containing the hook handler
		*	@param post Set to true if you want a post handler
		*/
		virtual bool AddHook(Plugin plug, void *iface, int thisptr_offs, HookManagerPubFunc myHookMan,
			ISHDelegate *handler, bool post) = 0;

		/**
		*	@brief Removes a hook.
		*
		*	@return True if the function succeeded, false otherwise
		*
		*	@param plug The unique identifier of the plugin that calls this function
		*	@param iface The interface pointer
		*	@param myHookMan A hook manager function that should be capable of handling the function
		*	@param handler A pointer to a FastDelegate containing the hook handler
		*	@param post Set to true if you want a post handler
		*/
		virtual bool RemoveHook(Plugin plug, void *iface, int thisptr_offs, HookManagerPubFunc myHookMan,
			ISHDelegate *handler, bool post) = 0;

		/**
		*	@brief Checks whether a plugin has (a) hook manager(s) that is/are currently used by other plugins
		*
		*	@param plug The unique identifier of the plugin in question
		*/
		virtual bool IsPluginInUse(Plugin plug) = 0;

		/**
		*	@brief Return a pointer to a callclass. Generate a new one if required.
		*
		*	@param iface The interface pointer
		*	@param size Size of the class instance
		*/
		virtual GenericCallClass *GetCallClass(void *iface, size_t size) = 0;

		/**
		*	@brief Release a callclass
		*
		*	@param ptr Pointer to the callclass
		*/
		virtual void ReleaseCallClass(GenericCallClass *ptr) = 0;

		virtual void SetRes(META_RES res) = 0;				//!< Sets the meta result
		virtual META_RES GetPrevRes() = 0;					//!< Gets the meta result of the
															//!<  previously calledhandler
		virtual META_RES GetStatus() = 0;					//!< Gets the highest meta result
		virtual const void *GetOrigRet() = 0;				//!< Gets the original result.
															//!<  If not in post function, undefined
		virtual const void *GetOverrideRet() = 0;			//!< Gets the override result.
															//!<  If none is specified, NULL
		virtual void *GetIfacePtr() = 0;					//!< Gets the interface pointer
		//////////////////////////////////////////////////////////////////////////
		// For hook managers
		virtual void HookLoopBegin(IIface *pIface) = 0;			//!< Should be called when a hook loop begins
		virtual void HookLoopEnd() = 0;							//!< Should be called when a hook loop exits
		virtual void SetCurResPtr(META_RES *mres) = 0;			//!< Sets pointer to the current meta result
		virtual void SetPrevResPtr(META_RES *mres) = 0;			//!< Sets pointer to previous meta result
		virtual void SetStatusPtr(META_RES *mres) = 0;			//!< Sets pointer to the status variable
		virtual void SetIfacePtrPtr(void **pp) = 0;				//!< Sets pointer to the interface this pointer
		virtual void SetOrigRetPtr(const void *ptr) = 0;		//!< Sets the original return pointer
		virtual void SetOverrideRetPtr(const void *ptr) = 0;	//!< Sets the override result pointer
		virtual bool ShouldContinue() = 0;						//!< Returns false if the hook loop should exit
	};
}

/************************************************************************/
/* High level interface                                                 */
/************************************************************************/
#define SET_META_RESULT(result)				SH_GLOB_SHPTR->SetRes(result)
#define RETURN_META(result)					do { SET_META_RESULT(result); return; } while(0)
#define RETURN_META_VALUE(result, value)	do { SET_META_RESULT(result); return (value); } while(0)

#define META_RESULT_STATUS					SH_GLOB_SHPTR->GetStatus()
#define META_RESULT_PREVIOUS				SH_GLOB_SHPTR->GetPrevRes()
#define META_RESULT_ORIG_RET(type)			*reinterpret_cast<const type*>(SH_GLOB_SHPTR->GetOrigRet())
#define META_RESULT_OVERRIDE_RET(type)		*reinterpret_cast<const type*>(SH_GLOB_SHPTR->GetOverrideRet())
#define META_IFACEPTR(type)					reinterpret_cast<type*>(SH_GLOB_SHPTR->GetIfacePtr())


/**
*	@brief Get/generate callclass for an interface pointer
*
*	@param ifaceptr The interface pointer
*/
template<class ifacetype>
inline SourceHook::CallClass<ifacetype> *SH_GET_CALLCLASS_R(SourceHook::ISourceHook *shptr, ifacetype *ptr)
{
	return reinterpret_cast<SourceHook::CallClass<ifacetype>*>(
		shptr->GetCallClass(reinterpret_cast<void*>(ptr), sizeof(ifacetype)));
}

template<class ifacetype>
inline void SH_RELEASE_CALLCLASS_R(SourceHook::ISourceHook *shptr, SourceHook::CallClass<ifacetype> *ptr)
{
	shptr->ReleaseCallClass(reinterpret_cast<SourceHook::GenericCallClass*>(ptr));
}

#define SH_GET_CALLCLASS(ptr) SH_GET_CALLCLASS_R(SH_GLOB_SHPTR, ptr)
#define SH_RELEASE_CALLCLASS(ptr) SH_RELEASE_CALLCLASS_R(SH_GLOB_SHPTR, ptr)

#define SH_ADD_HOOK(ifacetype, ifacefunc, ifaceptr, handler, post) \
	__SourceHook_FHAdd##ifacetype##ifacefunc((void*)SourceHook::implicit_cast<ifacetype*>(ifaceptr), \
	post, handler)
#define SH_ADD_HOOK_STATICFUNC(ifacetype, ifacefunc, ifaceptr, handler, post) \
	SH_ADD_HOOK(ifacetype, ifacefunc, ifaceptr, fastdelegate::MakeDelegate(handler), post)
#define SH_ADD_HOOK_MEMFUNC(ifacetype, ifacefunc, ifaceptr, handler_inst, handler_func, post) \
	SH_ADD_HOOK(ifacetype, ifacefunc, ifaceptr, fastdelegate::MakeDelegate(handler_inst, handler_func), post)

#define SH_REMOVE_HOOK(ifacetype, ifacefunc, ifaceptr, handler, post) \
	__SourceHook_FHRemove##ifacetype##ifacefunc((void*)SourceHook::implicit_cast<ifacetype*>(ifaceptr), \
	post, handler)
#define SH_REMOVE_HOOK_STATICFUNC(ifacetype, ifacefunc, ifaceptr, handler, post) \
	SH_REMOVE_HOOK(ifacetype, ifacefunc, ifaceptr, fastdelegate::MakeDelegate(handler), post)
#define SH_REMOVE_HOOK_MEMFUNC(ifacetype, ifacefunc, ifaceptr, handler_inst, handler_func, post) \
	SH_REMOVE_HOOK(ifacetype, ifacefunc, ifaceptr, fastdelegate::MakeDelegate(handler_inst, handler_func), post)

#define SH_NOATTRIB




#if SH_COMP == SH_COMP_MSVC
# define SH_SETUP_MFP(mfp) \
	reinterpret_cast<void**>(&mfp)[0] = vfnptr_origentry;
#elif SH_COMP == SH_COMP_GCC
# define SH_SETUP_MFP(mfp) \
	reinterpret_cast<void**>(&mfp)[0] = vfnptr_origentry; \
	reinterpret_cast<void**>(&mfp)[1] = 0;
#else
# error Not supported yet.
#endif

//////////////////////////////////////////////////////////////////////////
#define SH_FHCls(ift, iff, ov) __SourceHook_FHCls_##ift##iff##ov

#define SHINT_MAKE_HOOKMANPUBFUNC(ifacetype, ifacefunc, overload, funcptr) \
	SH_FHCls(ifacetype,ifacefunc,overload)() \
	{ \
		GetFuncInfo(funcptr, ms_MFI); \
	} \
	\
	static int HookManPubFunc(::SourceHook::HookManagerAction action, ::SourceHook::IHookManagerInfo *param) \
	{ \
		using namespace ::SourceHook; \
		GetFuncInfo(funcptr, ms_MFI); \
		/* Verify interface version */ \
		if (SH_GLOB_SHPTR->GetIfaceVersion() != SH_IFACE_VERSION) \
			return 1; \
		\
		if (action == HA_GetInfo) \
		{ \
			param->SetInfo(ms_MFI.vtbloffs, ms_MFI.vtblindex, ms_Proto); \
			\
			MemFuncInfo mfi; \
			GetFuncInfo(&SH_FHCls(ifacetype,ifacefunc,overload)::Func, mfi); \
			param->SetHookfuncVfnptr( \
				reinterpret_cast<void**>(reinterpret_cast<char*>(&ms_Inst) + mfi.vtbloffs)[mfi.vtblindex]); \
			return 0; \
		} \
		else if (action == HA_Register) \
		{ \
			ms_HI = param; \
			return 0; \
		} \
		else if (action == HA_Unregister) \
		{ \
			ms_HI = NULL; \
			return 0; \
		} \
		else \
			return 1; \
	}

// It has to be possible to use the macros in namespaces
// -> So we need to access and extend the global SourceHook namespace
// We use a namespace alias for this
#define SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, funcptr) \
	struct SH_FHCls(ifacetype,ifacefunc,overload) \
	{ \
		static SH_FHCls(ifacetype,ifacefunc,overload) ms_Inst; \
		static ::SourceHook::MemFuncInfo ms_MFI; \
		static ::SourceHook::IHookManagerInfo *ms_HI; \
		static const char *ms_Proto; \
		SHINT_MAKE_HOOKMANPUBFUNC(ifacetype, ifacefunc, overload, funcptr)

#define SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, proto, funcptr) \
	}; \
	SH_FHCls(ifacetype,ifacefunc,overload) SH_FHCls(ifacetype,ifacefunc,overload)::ms_Inst; \
	::SourceHook::MemFuncInfo SH_FHCls(ifacetype,ifacefunc,overload)::ms_MFI; \
	::SourceHook::IHookManagerInfo *SH_FHCls(ifacetype,ifacefunc,overload)::ms_HI; \
	const char *SH_FHCls(ifacetype,ifacefunc,overload)::ms_Proto = proto; \
	bool __SourceHook_FHAdd##ifacetype##ifacefunc(void *iface, bool post, \
		SH_FHCls(ifacetype,ifacefunc,overload)::FD handler) \
	{ \
		using namespace ::SourceHook; \
		MemFuncInfo mfi; \
		GetFuncInfo(funcptr, mfi); \
		if (mfi.thisptroffs < 0) \
			return false; /* No virtual inheritance supported */ \
		\
		return SH_GLOB_SHPTR->AddHook(SH_GLOB_PLUGPTR, iface, mfi.thisptroffs, \
			SH_FHCls(ifacetype,ifacefunc,overload)::HookManPubFunc, \
			new CSHDelegate<SH_FHCls(ifacetype,ifacefunc,overload)::FD>(handler), post); \
	} \
	bool __SourceHook_FHRemove##ifacetype##ifacefunc(void *iface, bool post, \
		SH_FHCls(ifacetype,ifacefunc,overload)::FD handler) \
	{ \
		using namespace ::SourceHook; \
		MemFuncInfo mfi; \
		GetFuncInfo(funcptr, mfi); \
		if (mfi.thisptroffs < 0) \
			return false; /* No virtual inheritance supported */ \
		\
		CSHDelegate<SH_FHCls(ifacetype,ifacefunc,overload)::FD> tmp(handler); \
		return SH_GLOB_SHPTR->RemoveHook(SH_GLOB_PLUGPTR, iface, mfi.thisptroffs, \
			SH_FHCls(ifacetype,ifacefunc,overload)::HookManPubFunc, &tmp, post); \
	} \

#define SH_SETUPCALLS(rettype, paramtypes, params) \
	/* 1) Find the vfnptr */ \
	using namespace ::SourceHook; \
	void *ourvfnptr = reinterpret_cast<void*>( \
		*reinterpret_cast<void***>(reinterpret_cast<char*>(this) + ms_MFI.vtbloffs) + ms_MFI.vtblindex); \
	IVfnPtr *vfnptr = ms_HI->FindVfnPtr(ourvfnptr); \
	SH_ASSERT(vfnptr, ("Called with vfnptr 0x%p which couldn't be found in the list", ourvfnptr)); \
	\
	void *vfnptr_origentry = vfnptr->GetOrigEntry(); \
	/* ... and the iface */ \
	IIface *ifinfo = vfnptr->FindIface(reinterpret_cast<void*>(this)); \
	if (!ifinfo) \
	{ \
		/* The iface info was not found. Redirect the call to the original function. */ \
		rettype (EmptyClass::*mfp)paramtypes; \
		SH_SETUP_MFP(mfp); \
		return (reinterpret_cast<EmptyClass*>(this)->*mfp)params; \
	} \
	/* 2) Declare some vars and set it up */ \
	SH_GLOB_SHPTR->HookLoopBegin(ifinfo); \
	IHookList *prelist = ifinfo->GetPreHooks(); \
	IHookList *postlist = ifinfo->GetPostHooks(); \
	META_RES status = MRES_IGNORED; \
	META_RES prev_res; \
	META_RES cur_res; \
	SH_GLOB_SHPTR->SetStatusPtr(&status); \
	SH_GLOB_SHPTR->SetPrevResPtr(&prev_res); \
	SH_GLOB_SHPTR->SetCurResPtr(&cur_res); \
	rettype orig_ret; \
	rettype override_ret; \
	rettype plugin_ret; \
	void* ifptr; \
	SH_GLOB_SHPTR->SetIfacePtrPtr(&ifptr); \
	SH_GLOB_SHPTR->SetOrigRetPtr(reinterpret_cast<void*>(&orig_ret)); \
	SH_GLOB_SHPTR->SetOverrideRetPtr(NULL);

#define SH_CALL_HOOKS(post, params) \
	if (SH_GLOB_SHPTR->ShouldContinue()) \
	{ \
		prev_res = MRES_IGNORED; \
		for (AutoHookIter iter(post##list); !iter.End(); iter.Next()) \
		{ \
			cur_res = MRES_IGNORED; \
			ifptr = reinterpret_cast<void*>(reinterpret_cast<char*>(this) - iter.ThisPtrOffs()); \
			plugin_ret = reinterpret_cast<CSHDelegate<FD>*>(iter.Handler())->GetDeleg() params; \
			prev_res = cur_res; \
			if (cur_res > status) \
				status = cur_res; \
			if (cur_res >= MRES_OVERRIDE) \
			{ \
				override_ret = plugin_ret; \
				SH_GLOB_SHPTR->SetOverrideRetPtr(&override_ret); \
			} \
			if (!SH_GLOB_SHPTR->ShouldContinue()) \
			{ \
				iter.SetToZero(); \
				break; \
			} \
		} \
	}

#define SH_CALL_ORIG(ifacetype, ifacefunc, rettype, paramtypes, params) \
	if (status != MRES_SUPERCEDE) \
	{ \
		rettype (EmptyClass::*mfp)paramtypes; \
		SH_SETUP_MFP(mfp); \
		orig_ret = (reinterpret_cast<EmptyClass*>(this)->*mfp)params; \
	} \
	else \
		orig_ret = override_ret;

#define SH_RETURN() \
	SH_GLOB_SHPTR->HookLoopEnd(); \
	return status >= MRES_OVERRIDE ? override_ret : orig_ret;

#define SH_HANDLEFUNC(ifacetype, ifacefunc, paramtypes, params, rettype) \
	SH_SETUPCALLS(rettype, paramtypes, params) \
	SH_CALL_HOOKS(pre, params) \
	SH_CALL_ORIG(ifacetype, ifacefunc, rettype, paramtypes, params) \
	SH_CALL_HOOKS(post, params) \
	SH_RETURN()

//////////////////////////////////////////////////////////////////////////
#define SH_SETUPCALLS_void(paramtypes, params) \
	/* 1) Find the vfnptr */ \
	using namespace ::SourceHook; \
	void *ourvfnptr = reinterpret_cast<void*>( \
		*reinterpret_cast<void***>(reinterpret_cast<char*>(this) + ms_MFI.vtbloffs) + ms_MFI.vtblindex); \
	IVfnPtr *vfnptr = ms_HI->FindVfnPtr(ourvfnptr); \
	SH_ASSERT(vfnptr, ("Called with vfnptr 0x%p which couldn't be found in the list", ourvfnptr)); \
	\
	void *vfnptr_origentry = vfnptr->GetOrigEntry(); \
	/* ... and the iface */ \
	IIface *ifinfo = vfnptr->FindIface(reinterpret_cast<void*>(this)); \
	if (!ifinfo) \
	{ \
		/* The iface info was not found. Redirect the call to the original function. */ \
		void (EmptyClass::*mfp)paramtypes; \
		SH_SETUP_MFP(mfp); \
		(reinterpret_cast<EmptyClass*>(this)->*mfp)params; \
		return; \
	} \
	/* 2) Declare some vars and set it up */ \
	SH_GLOB_SHPTR->HookLoopBegin(ifinfo); \
	IHookList *prelist = ifinfo->GetPreHooks(); \
	IHookList *postlist = ifinfo->GetPostHooks(); \
	META_RES status = MRES_IGNORED; \
	META_RES prev_res; \
	META_RES cur_res; \
	SH_GLOB_SHPTR->SetStatusPtr(&status); \
	SH_GLOB_SHPTR->SetPrevResPtr(&prev_res); \
	SH_GLOB_SHPTR->SetCurResPtr(&cur_res); \
	void* ifptr; \
	SH_GLOB_SHPTR->SetIfacePtrPtr(&ifptr); \
	SH_GLOB_SHPTR->SetOverrideRetPtr(NULL); \
	SH_GLOB_SHPTR->SetOrigRetPtr(NULL);

#define SH_CALL_HOOKS_void(post, params) \
	if (SH_GLOB_SHPTR->ShouldContinue()) \
	{ \
		prev_res = MRES_IGNORED; \
		for (AutoHookIter iter(post##list); !iter.End(); iter.Next()) \
		{ \
			cur_res = MRES_IGNORED; \
			ifptr = reinterpret_cast<void*>(reinterpret_cast<char*>(this) - iter.ThisPtrOffs()); \
			reinterpret_cast<CSHDelegate<FD>*>(iter.Handler())->GetDeleg() params; \
			prev_res = cur_res; \
			if (cur_res > status) \
				status = cur_res; \
			if (!SH_GLOB_SHPTR->ShouldContinue()) \
			{ \
				iter.SetToZero(); \
				break; \
			} \
		} \
	}

#define SH_CALL_ORIG_void(ifacetype, ifacefunc, paramtypes, params) \
	if (status != MRES_SUPERCEDE) \
	{ \
		void (EmptyClass::*mfp)paramtypes; \
		SH_SETUP_MFP(mfp); \
		(reinterpret_cast<EmptyClass*>(this)->*mfp)params; \
	}

#define SH_RETURN_void() \
	SH_GLOB_SHPTR->HookLoopEnd();

#define SH_HANDLEFUNC_void(ifacetype, ifacefunc, paramtypes, params) \
	SH_SETUPCALLS_void(paramtypes, params) \
	SH_CALL_HOOKS_void(pre, params) \
	SH_CALL_ORIG_void(ifacetype, ifacefunc, paramtypes, params) \
	SH_CALL_HOOKS_void(post, params) \
	SH_RETURN_void()


// Special vafmt handlers
#define SH_HANDLEFUNC_vafmt(ifacetype, ifacefunc, paramtypes, params_orig, params_plug, rettype) \
	SH_SETUPCALLS(rettype, paramtypes, params_orig) \
	SH_CALL_HOOKS(pre, params_plug) \
	SH_CALL_ORIG(ifacetype, ifacefunc, rettype, paramtypes, params_orig) \
	SH_CALL_HOOKS(post, params_plug) \
	SH_RETURN()

#define SH_HANDLEFUNC_void_vafmt(ifacetype, ifacefunc, paramtypes, params_orig, params_plug) \
	SH_SETUPCALLS_void(paramtypes, params_orig) \
	SH_CALL_HOOKS_void(pre, params_plug) \
	SH_CALL_ORIG_void(ifacetype, ifacefunc, paramtypes, params_orig) \
	SH_CALL_HOOKS_void(post, params_plug) \
	SH_RETURN_void()

//////////////////////////////////////////////////////////////////////////

@VARARGS@
// ********* Support for @$@ arguments *********
#define SH_DECL_HOOK@$@(ifacetype, ifacefunc, attr, overload, rettype@, param%%@) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(@param%%|, @) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate@$@<@param%%|, @@, @rettype> FD; \
		virtual rettype Func(@param%% p%%|, @) \
		{ SH_HANDLEFUNC(ifacetype, ifacefunc, (@param%%|, @), (@p%%|, @), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, #attr "|" #rettype @"|" #param%%| @, \
	(static_cast<rettype (ifacetype::*)(@param%%|, @) attr>(&ifacetype::ifacefunc)))

#define SH_DECL_HOOK@$@_void(ifacetype, ifacefunc, attr, overload@, param%%@) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(@param%%|, @) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate@$@<@param%%|, @> FD; \
		virtual void Func(@param%% p%%|, @) \
		{ SH_HANDLEFUNC_void(ifacetype, ifacefunc, (@param%%|, @), (@p%%|, @)); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, #attr @"|" #param%%| @, \
	(static_cast<void (ifacetype::*)(@param%%|, @) attr>(&ifacetype::ifacefunc)))

#define SH_DECL_HOOK@$@_vafmt(ifacetype, ifacefunc, attr, overload, rettype@, param%%@) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(@param%%|, @@, @const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate@$+1@<@param%%|, @@, @const char *, rettype> FD; \
		virtual rettype Func(@param%% p%%|, @@, @const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_vafmt(ifacetype, ifacefunc, (@param%%|, @@, @...), (@p%%|, @@, @"%s", buf), (@p%%|, @@, @buf), rettype); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, #attr "|" #rettype @"|" #param%%| @ "|const char*|...", \
	(static_cast<rettype (ifacetype::*)(@param%%|, @@, @const char *, ...) attr>(&ifacetype::ifacefunc)))

#define SH_DECL_HOOK@$@_void_vafmt(ifacetype, ifacefunc, attr, overload@, param%%@) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(@param%%|, @@, @const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate@$+1@<@param%%|, @@, @const char *> FD; \
		virtual void Func(@param%% p%%|, @@, @const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_void_vafmt(ifacetype, ifacefunc, (@param%%|, @@, @...), (@p%%|, @@, @"%s", buf), (@p%%|, @@, @buf)); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, #attr @"|" #param%%| @ "|const char*|...", \
	(static_cast<void (ifacetype::*)(@param%%|, @@, @const char *, ...) attr>(&ifacetype::ifacefunc)))

@ENDARGS@


//////////////////////////////////////////////////////////////////////////
// SH_CALL

#if SH_COMP == SH_COMP_MSVC

# define SH_MAKE_EXECUTABLECLASS_OB(call, prms) \
{ \
	using namespace ::SourceHook; \
	MemFuncInfo mfi; \
	GetFuncInfo(m_CC->GetThisPtr(), m_MFP, mfi); \
	void *origfunc = m_CC->GetOrigFunc(mfi.thisptroffs + mfi.vtbloffs, mfi.vtblindex); \
	if (!origfunc) \
		return (m_CC->GetThisPtr()->*m_MFP)call; \
	\
	/* It's hooked. Call the original function. */ \
	union \
	{ \
		RetType(EmptyClass::*mfpnew)prms; \
		void *addr; \
	} u; \
	u.addr = origfunc; \
	\
	void *adjustedthisptr = reinterpret_cast<void*>(reinterpret_cast<char*>(m_CC->GetThisPtr()) + mfi.thisptroffs); \
	return (reinterpret_cast<EmptyClass*>(adjustedthisptr)->*u.mfpnew)call; \
}

#elif SH_COMP == SH_COMP_GCC

# define SH_MAKE_EXECUTABLECLASS_OB(call, prms) \
{ \
	using namespace ::SourceHook; \
	MemFuncInfo mfi; \
	GetFuncInfo(m_CC->GetThisPtr(), m_MFP, mfi); \
	void *origfunc = m_CC->GetOrigFunc(mfi.thisptroffs + mfi.vtbloffs, mfi.vtblindex); \
	if (!origfunc) \
		return (m_CC->GetThisPtr()->*m_MFP)call; \
	\
	/* It's hooked. Call the original function. */ \
	union \
	{ \
		RetType(EmptyClass::*mfpnew)prms; \
		struct \
		{ \
			void *addr; \
			intptr_t adjustor; \
		} s; \
	} u; \
	u.s.addr = origfunc; \
	u.s.adjustor = mfi.thisptroffs; \
	\
	return (reinterpret_cast<EmptyClass*>(m_CC->GetThisPtr())->*u.mfpnew)call; \
}

#endif

namespace SourceHook
{
	template<class CCType, class RetType, class MFPType> class ExecutableClass
	{
		CCType *m_CC;
		MFPType m_MFP;
	public:
		ExecutableClass(CCType *cc, MFPType mfp) : m_CC(cc), m_MFP(mfp)
		{
		}

@VARARGS@
		// Support for @$@ arguments
		@template<@@class Param%%|, @@> @RetType operator()(@Param%% p%%|, @) const
			SH_MAKE_EXECUTABLECLASS_OB((@p%%|, @), (@Param%%|, @))

@ENDARGS@
	};
}

// SH_CALL needs to deduce the return type -> it uses templates and function overloading
// That's why SH_CALL takes two parameters: "mfp2" of type RetType(X::*mfp)(params), and "mfp" of type MFP
// The only purpose of the mfp2 parameter is to extract the return type

@VARARGS@
// Support for @$@ arguments
template <class X, class Y, class MFP, class RetType@, @@class Param%%|, @>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(@Param%%|, @))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType@, @@class Param%%|, @>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(@Param%%|, @)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

@ENDARGS@

#if SH_COMP != SH_COMP_MSVC || _MSC_VER > 1300
// GCC & MSVC 7.1 need this, MSVC 7.0 doesn't like it

@VARARGS@
// Support for @$@ arguments
template <class X, class Y, class MFP, class RetType@, @@class Param%%|, @>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(@Param%%|, @@, @...))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType@, @@class Param%%|, @>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(@Param%%|, @@, @...)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

@ENDARGS@

#endif

#define SH_CALL(ptr, mfp) SH_CALL2((ptr), (mfp), (mfp))

#undef SH_MAKE_EXECUTABLECLASS_OB

#endif
	// The pope is dead. -> :(
