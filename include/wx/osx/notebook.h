/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/notebook.h
// Purpose:     MSW/GTK compatible notebook (a.k.a. property sheet)
// Author:      Stefan Csomor
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_NOTEBOOK_H_
#define _WX_NOTEBOOK_H_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------
#include "wx/event.h"

#include <vector>

// ----------------------------------------------------------------------------
// types
// ----------------------------------------------------------------------------

// fwd declarations
class WXDLLIMPEXP_FWD_CORE wxImageList;
class WXDLLIMPEXP_FWD_CORE wxWindow;

// ----------------------------------------------------------------------------
// wxNotebook
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxNotebook : public wxNotebookBase
{
public:
  // ctors
  // -----
    // default for dynamic class
  wxNotebook() = default;
    // the same arguments as for wxControl (@@@ any special styles?)
  wxNotebook(wxWindow *parent,
             wxWindowID id,
             const wxPoint& pos = wxDefaultPosition,
             const wxSize& size = wxDefaultSize,
             long style = 0,
             const wxString& name = wxASCII_STR(wxNotebookNameStr))
    { Create( parent, id, pos, size, style, name ); }
    // Create() function
  bool Create(wxWindow *parent,
              wxWindowID id,
              const wxPoint& pos = wxDefaultPosition,
              const wxSize& size = wxDefaultSize,
              long style = 0,
              const wxString& name = wxASCII_STR(wxNotebookNameStr));
    // dtor
  virtual ~wxNotebook();

  // accessors
  // ---------
    // set the currently selected page, return the index of the previously
    // selected one (or wxNOT_FOUND on error)
    // NB: this function will _not_ generate wxEVT_NOTEBOOK_PAGE_xxx events
  int SetSelection(size_t nPage) override { return DoSetSelection(nPage, SetSelection_SendEvent); }

    // changes selected page without sending events
  int ChangeSelection(size_t nPage) override { return DoSetSelection(nPage); }

    // set/get the title of a page
  bool SetPageText(size_t nPage, const wxString& strText) override;
  wxString GetPageText(size_t nPage) const override;

    // sets/returns item's image index in the current image list
  int  GetPageImage(size_t nPage) const override;
  bool SetPageImage(size_t nPage, int nImage) override;

  // control the appearance of the notebook pages
    // set the size (the same for all pages)
  virtual void SetPageSize(const wxSize& size) override;
    // set the padding between tabs (in pixels)
  virtual void SetPadding(const wxSize& padding) override;
    // sets the size of the tabs (assumes all tabs are the same size)
  virtual void SetTabSize(const wxSize& sz) override;

  // hit test
  virtual int HitTest(const wxPoint& pt, long *flags = nullptr) const override;

  // calculate size for wxNotebookSizer
  wxSize CalcSizeFromPage(const wxSize& sizePage) const override;
  wxRect GetPageRect() const override;

  // operations
  // ----------
    // remove all pages
  bool DeleteAllPages() override;
    // the same as AddPage(), but adds it at the specified position
  bool InsertPage(size_t nPage,
                  wxNotebookPage *pPage,
                  const wxString& strText,
                  bool bSelect = false,
                  int imageId = NO_IMAGE) override;

  // callbacks
  // ---------
  void OnSize(wxSizeEvent& event);
  void OnSetFocus(wxFocusEvent& event);
  void OnNavigationKey(wxNavigationKeyEvent& event);

    // implementation
    // --------------

#if wxUSE_CONSTRAINTS
  virtual void SetConstraintSizes(bool recurse = true) override;
  virtual bool DoPhase(int nPhase) override;

#endif

  // base class virtuals
  // -------------------
  virtual void Command(wxCommandEvent& event) override;
    // osx specific event handling common for all osx-ports

    virtual bool OSXHandleClicked( double timestampsec ) override;

protected:
  virtual wxNotebookPage *DoRemovePage(size_t page) override;
  // common part of all ctors
  void Init();

  // helper functions
  void ChangePage(int nOldSel, int nSel); // change pages
  void MacSetupTabs();

  int DoSetSelection(size_t nPage, int flags = 0) override;

private:
  // this vector is synchronized with m_pages in the base class
  struct PageData
  {
      PageData(const wxString& text_, int image_)
          : text{text_}, image{image_}
      {
      }

      PageData(const PageData&) = default;
      PageData& operator=(const PageData&) = default;

      PageData(PageData&&) = default;
      PageData& operator=(PageData&&) = default;

      wxString text;
      int image = wxNOT_FOUND;
  };
  std::vector<PageData> m_pagesData;

  wxDECLARE_DYNAMIC_CLASS(wxNotebook);
  wxDECLARE_EVENT_TABLE();
};


#endif // _WX_NOTEBOOK_H_
