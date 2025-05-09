/////////////////////////////////////////////////////////////////////////////
// Name:        wx/qt/stattext.h
// Author:      Peter Most, Mariano Reingart
// Copyright:   (c) 2010 wxWidgets dev team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_QT_STATTEXT_H_
#define _WX_QT_STATTEXT_H_

class QLabel;

class WXDLLIMPEXP_CORE wxStaticText : public wxStaticTextBase
{
public:
    wxStaticText() = default;

    wxStaticText(wxWindow *parent,
                 wxWindowID id,
                 const wxString &label,
                 const wxPoint &pos = wxDefaultPosition,
                 const wxSize &size = wxDefaultSize,
                 long style = 0,
                 const wxString &name = wxASCII_STR(wxStaticTextNameStr) );

    ~wxStaticText();

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString &label,
                const wxPoint &pos = wxDefaultPosition,
                const wxSize &size = wxDefaultSize,
                long style = 0,
                const wxString &name = wxASCII_STR(wxStaticTextNameStr) );

    virtual void SetLabel(const wxString& label) override;

    QLabel* GetQLabel() const;

protected:
    virtual wxString WXGetVisibleLabel() const override;
    virtual void WXSetVisibleLabel(const wxString& str) override;

    wxDECLARE_DYNAMIC_CLASS(wxStaticText);
};

#endif // _WX_QT_STATTEXT_H_
