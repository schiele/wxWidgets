/////////////////////////////////////////////////////////////////////////////
// Name:        include/wx/aui/tabartgtk.h
// Purpose:     declaration of the wxAuiGTKTabArt
// Author:      Jens Lody and Teodor Petrov
// Created:     2012-03-23
// Copyright:   (c) 2012 Jens Lody <jens@codeblocks.org>
//                  and Teodor Petrov
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_AUI_TABARTGTK_H_
#define _WX_AUI_TABARTGTK_H_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/defs.h"

#if wxUSE_AUI

#include "wx/aui/tabart.h"
#include "wx/gdicmn.h"

class wxWindow;
class wxDC;

class WXDLLIMPEXP_AUI wxAuiGtkTabArt : public wxAuiGenericTabArt
{
public:
    wxAuiGtkTabArt();

    wxNODISCARD virtual wxAuiTabArt* Clone() override;
    virtual void DrawBorder(wxDC& dc, wxWindow* wnd, const wxRect& rect) override;
    virtual void DrawBackground(wxDC& dc, wxWindow* wnd, const wxRect& rect) override;
    virtual void DrawTab(wxDC& dc,
                         wxWindow* wnd,
                         const wxAuiNotebookPage& page,
                         const wxRect& in_rect,
                         int close_button_state,
                         wxRect* out_tab_rect,
                         wxRect* out_button_rect,
                         int* x_extent) override;
    void DrawButton(wxDC& dc, wxWindow* wnd, const wxRect& in_rect, int bitmap_id,
                    int button_state, int orientation, wxRect* out_rect) override;
    int GetBestTabCtrlSize(wxWindow* wnd, const wxAuiNotebookPageArray& pages,
                            const wxSize& required_bmp_size) override;
    int GetBorderWidth(wxWindow* wnd) override;
    int GetAdditionalBorderSpace(wxWindow* wnd) override;
    virtual wxSize GetTabSize(wxReadOnlyDC& dc, wxWindow* wnd, const wxString& caption,
                              const wxBitmapBundle& bitmap, bool active,
                              int close_button_state, int* x_extent) override;
};

#endif  // wxUSE_AUI

#endif // _WX_AUI_TABARTGTK_H_
