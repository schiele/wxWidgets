///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/appbase.cpp
// Purpose:     implements wxAppConsoleBase class
// Author:      Vadim Zeitlin
// Created:     19.06.2003 (extracted from common/appcmn.cpp)
// Copyright:   (c) 2003 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"


#ifndef WX_PRECOMP
    #ifdef __WINDOWS__
        #include  "wx/msw/wrapwin.h"  // includes windows.h for MessageBox()
    #endif
    #include "wx/list.h"
    #include "wx/app.h"
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/utils.h"
    #include "wx/wxcrtvararg.h"
#endif //WX_PRECOMP

#include "wx/apptrait.h"
#include "wx/cmdline.h"
#include "wx/confbase.h"
#include "wx/evtloop.h"
#include "wx/filename.h"
#include "wx/msgout.h"
#include "wx/scopedptr.h"
#include "wx/sysopt.h"
#include "wx/tokenzr.h"
#include "wx/thread.h"
#include "wx/stdpaths.h"

#include "wx/private/safecall.h"

#if wxUSE_EXCEPTIONS
    #include <exception>        // for std::current_exception()
    #include <utility>          // for std::swap()

    #include <typeinfo>
#endif // wxUSE_EXCEPTIONS

#if !defined(__WINDOWS__)
  #include  <signal.h>      // for SIGTRAP used by wxTrap()
#endif  //Win/Unix

#include <locale.h>

#if wxUSE_FONTMAP
    #include "wx/fontmap.h"
#endif // wxUSE_FONTMAP

#if wxUSE_LOG
    #include "wx/private/log.h"
#endif // wxUSE_LOG

#if wxDEBUG_LEVEL
    #if wxUSE_STACKWALKER
        #include "wx/stackwalk.h"
        #ifdef __WINDOWS__
            #include "wx/msw/debughlp.h"
        #endif
    #endif // wxUSE_STACKWALKER

    #include "wx/recguard.h"
#endif // wxDEBUG_LEVEL

#include <memory>

// wxABI_VERSION can be defined when compiling applications but it should be
// left undefined when compiling the library itself, it is then set to its
// default value in version.h
#if wxABI_VERSION != wxMAJOR_VERSION * 10000 + wxMINOR_VERSION * 100 + 99
#error "wxABI_VERSION should not be defined when compiling the library"
#endif

// ----------------------------------------------------------------------------
// private functions prototypes
// ----------------------------------------------------------------------------

#if wxDEBUG_LEVEL
    // really just show the assert dialog
    static bool DoShowAssertDialog(const wxString& msg);

    // prepare for showing the assert dialog, use the given traits or
    // DoShowAssertDialog() as last fallback to really show it
    static
    void ShowAssertDialog(const wxString& file,
                          int line,
                          const wxString& func,
                          const wxString& cond,
                          const wxString& msg,
                          wxAppTraits *traits = nullptr);

    // Used to pass the function name from wxDefaultAssertHandler().
    static wxString gs_assertFunc;
#endif // wxDEBUG_LEVEL

#ifdef __WXDEBUG__
    // turn on the trace masks specified in the env variable WXTRACE
    static void LINKAGEMODE SetTraceMasks();
#endif // __WXDEBUG__

// ----------------------------------------------------------------------------
// global vars
// ----------------------------------------------------------------------------

wxAppConsole *wxAppConsoleBase::ms_appInstance = nullptr;

wxAppInitializerFunction wxAppConsoleBase::ms_appInitFn = nullptr;

int wxAppConsoleBase::ms_fatalErrorExitCode = 255;

wxSocketManager *wxAppTraitsBase::ms_manager = nullptr;

WXDLLIMPEXP_DATA_BASE(wxList) wxPendingDelete;

// ----------------------------------------------------------------------------
// wxEventLoopPtr
// ----------------------------------------------------------------------------

// this defines wxEventLoopPtr
wxDEFINE_TIED_SCOPED_PTR_TYPE(wxEventLoopBase)

// ============================================================================
// wxAppConsoleBase implementation
// ============================================================================

// ----------------------------------------------------------------------------
// ctor/dtor
// ----------------------------------------------------------------------------

wxAppConsoleBase::wxAppConsoleBase()
{
    ms_appInstance = reinterpret_cast<wxAppConsole *>(this);

#ifdef __WXDEBUG__
    SetTraceMasks();
#endif

    wxEvtHandler::AddFilter(this);
}

wxAppConsoleBase::~wxAppConsoleBase()
{
    wxEvtHandler::RemoveFilter(this);

    // we're being destroyed and using this object from now on may not work or
    // even crash so don't leave dangling pointers to it
    ms_appInstance = nullptr;

    delete m_traits;
}

// ----------------------------------------------------------------------------
// initialization/cleanup
// ----------------------------------------------------------------------------

void wxAppConsoleBase::WXAppConstructed()
{
    wxASSERT_MSG( !m_fullyConstructed, "must be called only once" );

    m_fullyConstructed = true;
}

bool wxAppConsoleBase::Initialize(int& WXUNUSED(argc), wxChar **WXUNUSED(argv))
{
    if ( IsGUI() )
    {
        // GUI wxApp code must call WXAppConstructed() at the very end.
        wxASSERT_MSG( m_fullyConstructed, "Forgot to call WXAppConstructed()?" );
    }
    else // console application
    {
        // wxAppConsole doesn't call WXAppConstructed() as otherwise it would
        // be called twice when it's used as base class of a GUI wxApp, so call
        // it ourselves here.
        WXAppConstructed();
    }

#if defined(__WINDOWS__)
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
#endif

    return true;
}

wxString wxAppConsoleBase::GetAppName() const
{
    wxString name = m_appName;
    if ( name.empty() )
    {
        if ( argv )
        {
            // the application name is, by default, the name of its executable file
            wxFileName::SplitPath(argv[0], nullptr, &name, nullptr);
        }
#if wxUSE_STDPATHS
        else // fall back to the executable file name, if we can determine it
        {
            const wxString pathExe = wxStandardPaths::Get().GetExecutablePath();
            if ( !pathExe.empty() )
            {
                wxFileName::SplitPath(pathExe, nullptr, &name, nullptr);
            }
        }
#endif // wxUSE_STDPATHS
    }
    return name;
}

wxString wxAppConsoleBase::GetAppDisplayName() const
{
    // use the explicitly provided display name, if any
    if ( !m_appDisplayName.empty() )
        return m_appDisplayName;

    // if the application name was explicitly set, use it as is as capitalizing
    // it won't always produce good results
    if ( !m_appName.empty() )
        return m_appName;

    // if neither is set, use the capitalized version of the program file as
    // it's the most reasonable default
    return GetAppName().Capitalize();
}

wxEventLoopBase *wxAppConsoleBase::CreateMainLoop()
{
    return GetTraits()->CreateEventLoop();
}

void wxAppConsoleBase::CleanUp()
{
#if wxUSE_CONFIG
    // Delete the global wxConfig object, if any, and reset it.
    delete wxConfigBase::Set(nullptr);
#endif // wxUSE_CONFIG

    wxDELETE(m_mainLoop);
}

// ----------------------------------------------------------------------------
// OnXXX() callbacks
// ----------------------------------------------------------------------------

bool wxAppConsoleBase::OnInit()
{
#if wxUSE_CMDLINE_PARSER
    wxCmdLineParser parser(argc, argv);

    OnInitCmdLine(parser);

    bool cont;
    switch ( parser.Parse(false /* don't show usage */) )
    {
        case -1:
            cont = OnCmdLineHelp(parser);
            break;

        case 0:
            cont = OnCmdLineParsed(parser);
            break;

        default:
            cont = OnCmdLineError(parser);
            break;
    }

    if ( !cont )
        return false;
#endif // wxUSE_CMDLINE_PARSER

    return true;
}

int wxAppConsoleBase::OnRun()
{
    return MainLoop();
}

void wxAppConsoleBase::OnLaunched()
{
}

int wxAppConsoleBase::OnExit()
{
    // Delete all pending objects first, they might use wxConfig to save their
    // state during their destruction.
    DeletePendingObjects();

#if wxUSE_CONFIG
    // Ensure we won't create it on demand any more if we hadn't done it yet.
    wxConfigBase::DontCreateOnDemand();
#endif // wxUSE_CONFIG

    return 0;
}

void wxAppConsoleBase::Exit()
{
    if (m_mainLoop != nullptr)
        ExitMainLoop();
    else
        exit(-1);
}

// ----------------------------------------------------------------------------
// traits stuff
// ----------------------------------------------------------------------------

wxAppTraits *wxAppConsoleBase::CreateTraits()
{
    return new wxConsoleAppTraits;
}

wxAppTraits *wxAppConsoleBase::GetTraits()
{
    // If we already have valid traits, just return them.
    if ( m_traits )
        return m_traits;

    // Otherwise, create a new traits object as it would be unexpected (and
    // backwards incompatible) to return a null pointer from this function.
    auto* const traits = CreateTraits();

    // But only remember it if we're fully constructed to prevent using wrong
    // traits object later: if m_fullyConstructed is false, it means that the
    // object of the user-defined wxApp-derived class hasn't been fully
    // constructed yet, and so its possibly overridden CreateTraits() wasn't
    // called above, so make sure we do call it the next time GetTraits() is
    // called.
    if ( m_fullyConstructed )
        m_traits = traits;

    wxASSERT_MSG( traits, wxT("wxApp::CreateTraits() failed?") );

    return traits;
}

/* static */
wxAppTraits *wxAppConsoleBase::GetTraitsIfExists()
{
    wxAppConsole * const app = GetInstance();
    return app ? app->GetTraits() : nullptr;
}

/* static */
wxAppTraits& wxAppConsoleBase::GetValidTraits()
{
    static wxConsoleAppTraits s_traitsConsole;
    wxAppTraits* const traits = GetTraitsIfExists();

    return *(traits ? traits : &s_traitsConsole);
}

// ----------------------------------------------------------------------------
// wxEventLoop redirection
// ----------------------------------------------------------------------------

int wxAppConsoleBase::MainLoop()
{
    wxEventLoopBaseTiedPtr mainLoop(&m_mainLoop, CreateMainLoop());

    if (wxTheApp)
        wxTheApp->OnLaunched();

    return m_mainLoop ? m_mainLoop->Run() : -1;
}

void wxAppConsoleBase::ExitMainLoop()
{
    // we should exit from the main event loop, not just any currently active
    // (e.g. modal dialog) event loop
    if ( m_mainLoop && m_mainLoop->IsRunning() )
    {
        m_mainLoop->Exit(0);
    }
}

bool wxAppConsoleBase::Pending()
{
    // use the currently active message loop here, not m_mainLoop, because if
    // we're showing a modal dialog (with its own event loop) currently the
    // main event loop is not running anyhow
    wxEventLoopBase * const loop = wxEventLoopBase::GetActive();

    return loop && loop->Pending();
}

bool wxAppConsoleBase::Dispatch()
{
    // see comment in Pending()
    wxEventLoopBase * const loop = wxEventLoopBase::GetActive();

    return loop && loop->Dispatch();
}

bool wxAppConsoleBase::Yield(bool onlyIfNeeded)
{
    wxEventLoopBase * const loop = wxEventLoopBase::GetActive();
    if ( loop )
       return loop->Yield(onlyIfNeeded);

    std::unique_ptr<wxEventLoopBase> tmpLoop(CreateMainLoop());
    return tmpLoop->Yield(onlyIfNeeded);
}

void wxAppConsoleBase::WakeUpIdle()
{
    wxEventLoopBase * const loop = wxEventLoopBase::GetActive();

    if ( loop )
        loop->WakeUp();
}

bool wxAppConsoleBase::ProcessIdle()
{
    // synthesize an idle event and check if more of them are needed
    wxIdleEvent event;
    event.SetEventObject(this);

    // Don't let exceptions propagate from the user-defined handler, we may be
    // called from an extern "C" callback (e.g. this is the case in wxGTK).
    SafelyProcessEvent(event);

#if wxUSE_LOG
    // flush the logged messages if any (do this after processing the events
    // which could have logged new messages)
    wxLog::FlushActive();
#endif

    // Garbage collect all objects previously scheduled for destruction.
    DeletePendingObjects();

    return event.MoreRequested();
}

bool wxAppConsoleBase::UsesEventLoop() const
{
    // in console applications we don't know whether we're going to have an
    // event loop so assume we won't -- unless we already have one running
    return wxEventLoopBase::GetActive() != nullptr;
}

// ----------------------------------------------------------------------------
// events
// ----------------------------------------------------------------------------

/* static */
bool wxAppConsoleBase::IsMainLoopRunning()
{
    const wxAppConsole * const app = GetInstance();

    return app && app->m_mainLoop != nullptr;
}

int wxAppConsoleBase::FilterEvent(wxEvent& WXUNUSED(event))
{
    // process the events normally by default
    return Event_Skip;
}

void wxAppConsoleBase::DelayPendingEventHandler(wxEvtHandler* toDelay)
{
    wxENTER_CRIT_SECT(m_handlersWithPendingEventsLocker);

    // move the handler from the list of handlers with processable pending events
    // to the list of handlers with pending events which needs to be processed later
    m_handlersWithPendingEvents.Remove(toDelay);

    if (m_handlersWithPendingDelayedEvents.Index(toDelay) == wxNOT_FOUND)
        m_handlersWithPendingDelayedEvents.Add(toDelay);

    wxLEAVE_CRIT_SECT(m_handlersWithPendingEventsLocker);
}

void wxAppConsoleBase::RemovePendingEventHandler(wxEvtHandler* toRemove)
{
    wxENTER_CRIT_SECT(m_handlersWithPendingEventsLocker);

    if (m_handlersWithPendingEvents.Index(toRemove) != wxNOT_FOUND)
    {
        m_handlersWithPendingEvents.Remove(toRemove);

        // check that the handler was present only once in the list
        wxASSERT_MSG( m_handlersWithPendingEvents.Index(toRemove) == wxNOT_FOUND,
                        "Handler occurs twice in the m_handlersWithPendingEvents list!" );
    }
    //else: it wasn't in this list at all, it's ok

    if (m_handlersWithPendingDelayedEvents.Index(toRemove) != wxNOT_FOUND)
    {
        m_handlersWithPendingDelayedEvents.Remove(toRemove);

        // check that the handler was present only once in the list
        wxASSERT_MSG( m_handlersWithPendingDelayedEvents.Index(toRemove) == wxNOT_FOUND,
                        "Handler occurs twice in m_handlersWithPendingDelayedEvents list!" );
    }
    //else: it wasn't in this list at all, it's ok

    wxLEAVE_CRIT_SECT(m_handlersWithPendingEventsLocker);
}

void wxAppConsoleBase::AppendPendingEventHandler(wxEvtHandler* toAppend)
{
    wxENTER_CRIT_SECT(m_handlersWithPendingEventsLocker);

    if ( m_handlersWithPendingEvents.Index(toAppend) == wxNOT_FOUND )
        m_handlersWithPendingEvents.Add(toAppend);

    wxLEAVE_CRIT_SECT(m_handlersWithPendingEventsLocker);
}

bool wxAppConsoleBase::HasPendingEvents() const
{
    wxENTER_CRIT_SECT(const_cast<wxAppConsoleBase*>(this)->m_handlersWithPendingEventsLocker);

    bool has = !m_handlersWithPendingEvents.IsEmpty();

    wxLEAVE_CRIT_SECT(const_cast<wxAppConsoleBase*>(this)->m_handlersWithPendingEventsLocker);

    return has;
}

void wxAppConsoleBase::SuspendProcessingOfPendingEvents()
{
    m_bDoPendingEventProcessing = false;
}

void wxAppConsoleBase::ResumeProcessingOfPendingEvents()
{
    m_bDoPendingEventProcessing = true;
}

void wxAppConsoleBase::ProcessPendingEvents()
{
    if ( m_bDoPendingEventProcessing )
    {
        wxENTER_CRIT_SECT(m_handlersWithPendingEventsLocker);

        wxCHECK_RET( m_handlersWithPendingDelayedEvents.IsEmpty(),
                     "this helper list should be empty" );

        // iterate until the list becomes empty: the handlers remove themselves
        // from it when they don't have any more pending events
        while (!m_handlersWithPendingEvents.IsEmpty())
        {
            // NOTE: we always call ProcessPendingEvents() on the first event handler
            //       with pending events because handlers auto-remove themselves
            //       from this list (see RemovePendingEventHandler) if they have no
            //       more pending events.
            wxEvtHandler* const handler = m_handlersWithPendingEvents[0];

            // In ProcessPendingEvents(), new handlers might be added
            // and we can safely leave the critical section here as we're not
            // accessing m_handlersWithPendingEvents while we don't hold it.
            wxLEAVE_CRIT_SECT(m_handlersWithPendingEventsLocker);

            handler->ProcessPendingEvents();

            wxENTER_CRIT_SECT(m_handlersWithPendingEventsLocker);
        }

        // now the wxHandlersWithPendingEvents is surely empty; however some event
        // handlers may have moved themselves into wxHandlersWithPendingDelayedEvents
        // because of a selective wxYield call in progress.
        // Now we need to move them back to wxHandlersWithPendingEvents so the next
        // call to this function has the chance of processing them:
        if (!m_handlersWithPendingDelayedEvents.IsEmpty())
        {
            WX_APPEND_ARRAY(m_handlersWithPendingEvents, m_handlersWithPendingDelayedEvents);
            m_handlersWithPendingDelayedEvents.Clear();
        }

        wxLEAVE_CRIT_SECT(m_handlersWithPendingEventsLocker);
    }
}

void wxAppConsoleBase::DeletePendingEvents()
{
    wxENTER_CRIT_SECT(m_handlersWithPendingEventsLocker);

    wxCHECK_RET( m_handlersWithPendingDelayedEvents.IsEmpty(),
                 "this helper list should be empty" );

    for (unsigned int i=0; i<m_handlersWithPendingEvents.GetCount(); i++)
        m_handlersWithPendingEvents[i]->DeletePendingEvents();

    m_handlersWithPendingEvents.Clear();

    wxLEAVE_CRIT_SECT(m_handlersWithPendingEventsLocker);
}

// ----------------------------------------------------------------------------
// delayed objects destruction
// ----------------------------------------------------------------------------

bool wxAppConsoleBase::IsScheduledForDestruction(wxObject *object) const
{
    return wxPendingDelete.Member(object);
}

void wxAppConsoleBase::ScheduleForDestruction(wxObject *object)
{
    if ( !UsesEventLoop() )
    {
        // we won't be able to delete it later so do it right now
        delete object;
        return;
    }
    //else: we either already have or will soon start an event loop

    if ( !wxPendingDelete.Member(object) )
        wxPendingDelete.Append(object);
}

void wxAppConsoleBase::DeletePendingObjects()
{
    wxList::compatibility_iterator node = wxPendingDelete.GetFirst();
    while (node)
    {
        wxObject *obj = node->GetData();

        // remove it from the list first so that if we get back here somehow
        // during the object deletion (e.g. wxYield called from its dtor) we
        // wouldn't try to delete it the second time
        if ( wxPendingDelete.Member(obj) )
            wxPendingDelete.Erase(node);

        delete obj;

        // Deleting one object may have deleted other pending
        // objects, so start from beginning of list again.
        node = wxPendingDelete.GetFirst();
    }
}

// ----------------------------------------------------------------------------
// exception handling
// ----------------------------------------------------------------------------

#if wxUSE_EXCEPTIONS

void
wxAppConsoleBase::HandleEvent(wxEvtHandler *handler,
                              wxEventFunction func,
                              wxEvent& event) const
{
    // by default, simply call the handler
    (handler->*func)(event);
}

void wxAppConsoleBase::CallEventHandler(wxEvtHandler *handler,
                                        wxEventFunctor& functor,
                                        wxEvent& event) const
{
    // If the functor holds a method then, for backward compatibility, call
    // HandleEvent():
    wxEventFunction eventFunction = functor.GetEvtMethod();

    if ( eventFunction )
        HandleEvent(handler, eventFunction, event);
    else
        functor(handler, event);
}

void wxAppConsoleBase::OnUnhandledException()
{
    // we're called from an exception handler so we can re-throw the exception
    // to recover its type
    wxString what;
    try
    {
        throw;
    }
    catch ( std::exception& e )
    {
#ifdef wxNO_RTTI
        what.Printf("standard exception with message \"%s\"", e.what());
#else
        what.Printf("standard exception of type \"%s\" with message \"%s\"",
                    typeid(e).name(), e.what());
#endif
    }
    catch ( ... )
    {
        what = "unknown exception";
    }

    wxMessageOutputBest().Printf(
        "Unhandled %s; terminating %s.\n",
        what,
        wxIsMainThread() ? "the application" : "the thread in which it happened"
    );
}

/* static */
void wxAppConsoleBase::CallOnUnhandledException()
{
    if ( wxTheApp )
    {
        wxSafeCall<void>([]()
        {
            wxTheApp->OnUnhandledException();
        }, []()
        {
            // And OnUnhandledException() absolutely shouldn't throw,
            // but we still must account for the possibility that it
            // did. At least show some information about the exception
            // in this case by calling our, non-overridden version.
            wxTheApp->wxAppConsoleBase::OnUnhandledException();
        });
    }
}

// ----------------------------------------------------------------------------
// exceptions support
// ----------------------------------------------------------------------------

bool wxAppConsoleBase::OnExceptionInMainLoop()
{
    throw;
}

static std::exception_ptr gs_storedException;

bool wxAppConsoleBase::StoreCurrentException()
{
    if ( gs_storedException )
    {
        // We can't store more than one exception currently: while we could
        // support this by just using a vector<exception_ptr>, it shouldn't be
        // actually necessary because we should never have more than one active
        // exception anyhow.
        return false;
    }

    gs_storedException = std::current_exception();

    return true;
}

void wxAppConsoleBase::RethrowStoredException()
{
    if ( gs_storedException )
    {
        std::exception_ptr storedException;
        std::swap(storedException, gs_storedException);

        std::rethrow_exception(storedException);
    }
}

#endif // wxUSE_EXCEPTIONS

// ----------------------------------------------------------------------------
// cmd line parsing
// ----------------------------------------------------------------------------

#if wxUSE_CMDLINE_PARSER

#define OPTION_VERBOSE "verbose"

void wxAppConsoleBase::OnInitCmdLine(wxCmdLineParser& parser)
{
    // the standard command line options
    static const wxCmdLineEntryDesc cmdLineDesc[] =
    {
        {
            wxCMD_LINE_SWITCH,
            "h",
            "help",
            gettext_noop("show this help message"),
            wxCMD_LINE_VAL_NONE,
            wxCMD_LINE_OPTION_HELP
        },

#if wxUSE_LOG
        {
            wxCMD_LINE_SWITCH,
            nullptr,
            OPTION_VERBOSE,
            gettext_noop("generate verbose log messages"),
            wxCMD_LINE_VAL_NONE,
            0x0
        },
#endif // wxUSE_LOG

        // terminator
        wxCMD_LINE_DESC_END
    };

    parser.SetDesc(cmdLineDesc);
}

bool wxAppConsoleBase::OnCmdLineParsed(wxCmdLineParser& parser)
{
#if wxUSE_LOG
    if ( parser.Found(OPTION_VERBOSE) )
    {
        wxLog::SetVerbose(true);
    }
#else
    wxUnusedVar(parser);
#endif // wxUSE_LOG

    return true;
}

bool wxAppConsoleBase::OnCmdLineHelp(wxCmdLineParser& parser)
{
    parser.Usage();

    return false;
}

bool wxAppConsoleBase::OnCmdLineError(wxCmdLineParser& parser)
{
    parser.Usage();

    return false;
}

#endif // wxUSE_CMDLINE_PARSER

// ----------------------------------------------------------------------------
// debugging support
// ----------------------------------------------------------------------------

/* static */
bool wxAppConsoleBase::CheckBuildOptions(const char *optionsSignature,
                                         const char *componentName)
{
#if 0 // can't use wxLogTrace, not up and running yet
    printf("checking build options object '%s' (ptr %p) in '%s'\n",
             optionsSignature, optionsSignature, componentName);
#endif

    if ( strcmp(optionsSignature, WX_BUILD_OPTIONS_SIGNATURE) != 0 )
    {
        wxString lib = wxString::FromAscii(WX_BUILD_OPTIONS_SIGNATURE);
        wxString prog = wxString::FromAscii(optionsSignature);
        wxString progName = wxString::FromAscii(componentName);

        wxLogFatalError(wxT("Mismatch between the program and library build versions detected.\nThe library used %s,\nand %s used %s."),
                        lib, progName, prog);

        // normally wxLogFatalError doesn't return
        return false;
    }

    return true;
}

void wxAppConsoleBase::OnAssertFailure(const wxChar *file,
                                       int line,
                                       const wxChar *func,
                                       const wxChar *cond,
                                       const wxChar *msg)
{
#if wxDEBUG_LEVEL
    ShowAssertDialog(file, line, func, cond, msg, GetTraits());
#else
    // this function is still present even in debug level 0 build for ABI
    // compatibility reasons but is never called there and so can simply do
    // nothing in it
    wxUnusedVar(file);
    wxUnusedVar(line);
    wxUnusedVar(func);
    wxUnusedVar(cond);
    wxUnusedVar(msg);
#endif // wxDEBUG_LEVEL/!wxDEBUG_LEVEL
}

void wxAppConsoleBase::OnAssert(const wxChar *file,
                                int line,
                                const wxChar *cond,
                                const wxChar *msg)
{
#if wxDEBUG_LEVEL
    OnAssertFailure(file, line, gs_assertFunc.wc_str(), cond, msg);
#else
    OnAssertFailure(file, line, nullptr, cond, msg);
#endif
}

// ----------------------------------------------------------------------------
// Miscellaneous other methods
// ----------------------------------------------------------------------------

void wxAppConsoleBase::SetCLocale()
{
    // We want to use the user locale by default in GUI applications in order
    // to show the numbers, dates &c in the familiar format -- and also accept
    // this format on input (especially important for decimal comma/dot).
    wxSetlocale(LC_ALL, "");
}

// ============================================================================
// other classes implementations
// ============================================================================

// ----------------------------------------------------------------------------
// wxConsoleAppTraitsBase
// ----------------------------------------------------------------------------

#if wxUSE_LOG

wxLog *wxConsoleAppTraitsBase::CreateLogTarget()
{
    return new wxLogOutputBest;
}

#endif // wxUSE_LOG

wxMessageOutput *wxConsoleAppTraitsBase::CreateMessageOutput()
{
    return new wxMessageOutputBest;
}

#if wxUSE_FONTMAP

wxFontMapper *wxConsoleAppTraitsBase::CreateFontMapper()
{
    return (wxFontMapper *)new wxFontMapperBase;
}

#endif // wxUSE_FONTMAP

wxRendererNative *wxConsoleAppTraitsBase::CreateRenderer()
{
    // console applications don't use renderers
    return nullptr;
}

bool wxConsoleAppTraitsBase::ShowAssertDialog(const wxString& msg)
{
    return wxAppTraitsBase::ShowAssertDialog(msg);
}

bool wxConsoleAppTraitsBase::HasStderr()
{
    // console applications always have stderr, even under Mac/Windows
    return true;
}

bool wxConsoleAppTraitsBase::SafeMessageBox(const wxString& WXUNUSED(text),
                                            const wxString& WXUNUSED(title))
{
    // console applications don't show message boxes by default, although this
    // can be done in platform-specific cases
    return false;
}

// ----------------------------------------------------------------------------
// wxAppTraits
// ----------------------------------------------------------------------------

#if wxUSE_THREADS
void wxMutexGuiEnterImpl();
void wxMutexGuiLeaveImpl();

void wxAppTraitsBase::MutexGuiEnter()
{
    wxMutexGuiEnterImpl();
}

void wxAppTraitsBase::MutexGuiLeave()
{
    wxMutexGuiLeaveImpl();
}

void WXDLLIMPEXP_BASE wxMutexGuiEnter()
{
    wxAppTraits * const traits = wxAppConsoleBase::GetTraitsIfExists();
    if ( traits )
        traits->MutexGuiEnter();
}

void WXDLLIMPEXP_BASE wxMutexGuiLeave()
{
    wxAppTraits * const traits = wxAppConsoleBase::GetTraitsIfExists();
    if ( traits )
        traits->MutexGuiLeave();
}
#endif // wxUSE_THREADS

bool wxAppTraitsBase::ShowAssertDialog(const wxString& msgOriginal)
{
#if wxDEBUG_LEVEL
    wxString msg;

#if wxUSE_STACKWALKER
    const wxString stackTrace = GetAssertStackTrace();
    if ( !stackTrace.empty() )
    {
        msg << wxT("\n\nCall stack:\n") << stackTrace;

        wxMessageOutputDebug().Output(msg);
    }
#endif // wxUSE_STACKWALKER

    return DoShowAssertDialog(msgOriginal + msg);
#else // !wxDEBUG_LEVEL
    wxUnusedVar(msgOriginal);

    return false;
#endif // wxDEBUG_LEVEL/!wxDEBUG_LEVEL
}

#if wxUSE_STACKWALKER
wxString wxAppTraitsBase::GetAssertStackTrace()
{
#if wxDEBUG_LEVEL

#if !defined(__WINDOWS__)
    // on Unix stack frame generation may take some time, depending on the
    // size of the executable mainly... warn the user that we are working
    wxFputs("Collecting stack trace information, please wait...", stderr);
    fflush(stderr);
#endif // !__WINDOWS__


    class StackDump : public wxStackWalker
    {
    public:
        StackDump() { m_numFrames = 0; }

        const wxString& GetStackTrace() const { return m_stackTrace; }

    protected:
        virtual void OnStackFrame(const wxStackFrame& frame) override
        {
            // don't show more than maxLines or we could get a dialog too tall
            // to be shown on screen: 20 should be ok everywhere as even with
            // 15 pixel high characters it is still only 300 pixels...
            if ( m_numFrames++ > 20 )
                return;

            m_stackTrace << wxString::Format(wxT("[%02u] "), m_numFrames);

            const wxString name = frame.GetName();
            if ( name.StartsWith("wxOnAssert") )
            {
                // Ignore all frames until the wxOnAssert() one, they are
                // internal to wxWidgets and not interesting for the user
                // (but notice that if we never find the wxOnAssert() frame,
                // e.g. because we don't have symbol info at all, we would show
                // everything which is better than not showing anything).
                m_stackTrace.clear();
                m_numFrames = 0;
                return;
            }

            if ( !name.empty() )
            {
                m_stackTrace << wxString::Format("%-80s", name);
            }
            else
            {
                m_stackTrace << wxString::Format("%-80p", frame.GetAddress());
            }

            if ( frame.HasSourceLocation() )
            {
                m_stackTrace << wxString::Format("%s:%zu",
                                                 frame.GetFileName(),
                                                 frame.GetLine());
            }

            m_stackTrace << wxT('\n');
        }

    private:
        wxString m_stackTrace;
        unsigned m_numFrames;
    };

    StackDump dump;
    dump.Walk();
    return dump.GetStackTrace();
#else // !wxDEBUG_LEVEL
    // this function is still present for ABI-compatibility even in debug level
    // 0 build but is not used there and so can simply do nothing
    return wxString();
#endif // wxDEBUG_LEVEL/!wxDEBUG_LEVEL
}
#endif // wxUSE_STACKWALKER


// ============================================================================
// global functions implementation
// ============================================================================

void wxExit()
{
    if ( wxTheApp )
    {
        wxTheApp->Exit();
    }
    else
    {
        // what else can we do?
        exit(-1);
    }
}

void wxWakeUpIdle()
{
    if ( wxTheApp )
    {
        wxTheApp->WakeUpIdle();
    }
    //else: do nothing, what can we do?
}

// wxASSERT() helper
bool wxAssertIsEqual(int x, int y)
{
    return x == y;
}

void wxAbort()
{
    abort();
}

#if wxDEBUG_LEVEL

// break into the debugger
#ifndef wxTrap

void wxTrap()
{
#if defined(__WINDOWS__)
    DebugBreak();
#elif defined(_MSL_USING_MW_C_HEADERS) && _MSL_USING_MW_C_HEADERS
    Debugger();
#elif defined(__UNIX__)
    raise(SIGTRAP);
#else
    // TODO
#endif // Win/Unix
}

#endif // wxTrap already defined as a macro

// default assert handler
static void
wxDefaultAssertHandler(const wxString& file,
                       int line,
                       const wxString& func,
                       const wxString& cond,
                       const wxString& msg)
{
    // FIXME MT-unsafe
    static int s_bInAssert = 0;

    wxRecursionGuard guard(s_bInAssert);
    if ( guard.IsInside() )
    {
        // can't use assert here to avoid infinite loops, so just trap
        wxTrap();

        return;
    }

    // If this option is set, we should abort immediately when assert happens.
    if ( wxSystemOptions::GetOptionInt("exit-on-assert") )
        wxAbort();

    if ( !wxTheApp )
    {
        // by default, show the assert dialog box -- we can't customize this
        // behaviour
        ShowAssertDialog(file, line, func, cond, msg);
    }
    else
    {
        // let the app process it as it wants

        // for compatibility, call the old function after stashing the function
        // name into a global, so that it could pass it to the new one
        gs_assertFunc = func;
        wxTheApp->OnAssert(file.wc_str(), line, cond.wc_str(), msg.wc_str());
    }
}

wxAssertHandler_t wxTheAssertHandler = wxDefaultAssertHandler;

void wxSetDefaultAssertHandler()
{
    wxTheAssertHandler = wxDefaultAssertHandler;
}

void wxOnAssert(const wxString& file,
                int line,
                const wxString& func,
                const wxString& cond,
                const wxString& msg)
{
    wxTheAssertHandler(file, line, func, cond, msg);
}

void wxOnAssert(const wxString& file,
                int line,
                const wxString& func,
                const wxString& cond)
{
    wxTheAssertHandler(file, line, func, cond, wxString());
}

void wxOnAssert(const char *file,
                int line,
                const char *func,
                const char *cond,
                const wxString& msg)
{
    wxTheAssertHandler(file, line, func, cond, msg);
}

void wxOnAssert(const char *file,
                int line,
                const char *func,
                const char *cond,
                const wxCStrData& msg)
{
    wxTheAssertHandler(file, line, func, cond, msg);
}

void wxOnAssert(const char *file,
                int line,
                const char *func,
                const char *cond)
{
    wxTheAssertHandler(file, line, func, cond, wxString());
}

void wxOnAssert(const char *file,
                int line,
                const char *func,
                const char *cond,
                const char *msg)
{
    wxTheAssertHandler(file, line, func, cond, msg);
}

void wxOnAssert(const char *file,
                int line,
                const char *func,
                const char *cond,
                const wxChar *msg)
{
    wxTheAssertHandler(file, line, func, cond, msg);
}

#endif // wxDEBUG_LEVEL

// ============================================================================
// private functions implementation
// ============================================================================

#ifdef __WXDEBUG__

static void LINKAGEMODE SetTraceMasks()
{
#if wxUSE_LOG
    wxString mask;
    if ( wxGetEnv(wxT("WXTRACE"), &mask) )
    {
        wxStringTokenizer tkn(mask, wxT(",;:"));
        while ( tkn.HasMoreTokens() )
            wxLog::AddTraceMask(tkn.GetNextToken());
    }

    wxString ts;
    if ( wxGetEnv("WXLOG_TIME_FORMAT", &ts) )
        wxLog::SetTimestamp(ts);
#endif // wxUSE_LOG
}

#endif // __WXDEBUG__

#if wxDEBUG_LEVEL

bool wxTrapInAssert = false;

static
bool DoShowAssertDialog(const wxString& msg)
{
    // under Windows we can show the dialog even in the console mode
#if defined(__WINDOWS__)
    wxString msgDlg(msg);

    // this message is intentionally not translated -- it is for developers
    // only -- and the less code we use here, less is the danger of recursively
    // asserting and dying
    msgDlg += wxT("\nDo you want to stop the program?\n")
              wxT("You can also choose [Cancel] to suppress ")
              wxT("further warnings.");

    switch ( ::MessageBox(nullptr, msgDlg.t_str(), wxT("wxWidgets Debug Alert"),
                          MB_YESNOCANCEL | MB_DEFBUTTON2 | MB_ICONSTOP ) )
    {
        case IDYES:
            // If we called wxTrap() directly from here, the programmer would
            // see this function and a few more calls between his own code and
            // it in the stack trace which would be perfectly useless and often
            // confusing. So instead just set the flag here and let the macros
            // defined in wx/debug.h call wxTrap() themselves, this ensures
            // that the debugger will show the line in the user code containing
            // the failing assert.
            wxTrapInAssert = true;
            break;

        case IDCANCEL:
            // stop the asserts
            return true;

        //case IDNO: nothing to do
    }
#else // !__WINDOWS__
    wxUnusedVar(msg);
#endif // __WINDOWS__/!__WINDOWS__

    // continue with the asserts by default
    return false;
}

// show the standard assert dialog
static
void ShowAssertDialog(const wxString& file,
                      int line,
                      const wxString& func,
                      const wxString& cond,
                      const wxString& msgUser,
                      wxAppTraits *traits)
{
    // this variable can be set to true to suppress "assert failure" messages
    static bool s_bNoAsserts = false;

    wxString msg;
    msg.reserve(2048);

    // make life easier for people using VC++ IDE by using this format: like
    // this, clicking on the message will take us immediately to the place of
    // the failed assert
    msg.Printf(wxT("%s(%d): assert \"%s\" failed"), file, line, cond);

    // add the function name, if any
    if ( !func.empty() )
        msg << wxT(" in ") << func << wxT("()");

    // and the message itself
    if ( !msgUser.empty() )
    {
        msg << wxT(": ") << msgUser;
    }
    else // no message given
    {
        msg << wxT('.');
    }

#if wxUSE_THREADS
    if ( !wxThread::IsMain() )
    {
        msg += wxString::Format(" [in thread %lx]", wxThread::GetCurrentId());
    }
#endif // wxUSE_THREADS

    // log the assert in any case
    wxMessageOutputDebug().Output(msg);

    if ( !s_bNoAsserts )
    {
        if ( traits )
        {
            // delegate showing assert dialog (if possible) to that class
            s_bNoAsserts = traits->ShowAssertDialog(msg);
        }
        else // no traits object
        {
            // fall back to the function of last resort
            s_bNoAsserts = DoShowAssertDialog(msg);
        }
    }
}

#endif // wxDEBUG_LEVEL
