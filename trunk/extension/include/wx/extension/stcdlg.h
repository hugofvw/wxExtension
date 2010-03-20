////////////////////////////////////////////////////////////////////////////////
// Name:      stcdlg.h
// Purpose:   Declaration of class wxExSTCEntryDialog
// Author:    Anton van Wezenbeek
// Created:   2009-11-18
// RCS-ID:    $Id$
// Copyright: (c) 2009 Anton van Wezenbeek
////////////////////////////////////////////////////////////////////////////////

#ifndef _EXSTCDLG_H
#define _EXSTCDLG_H

#include <wx/extension/dialog.h> // for wxExDialog

#if wxUSE_GUI
class wxExSTC;

/// Offers an wxExSTC as a dialog (like wxTextEntryDialog).
/// The prompt is allowed to be empty, in that case no sizer is used for it.
/// The size is used for the exSTC component.
class wxExSTCEntryDialog : public wxExDialog
{
public:
  /// Constructor.
  wxExSTCEntryDialog(
    wxWindow* parent,
    const wxString& caption,
    const wxString& text,
    const wxString& prompt = wxEmptyString,
    long button_style = wxOK | wxCANCEL,
    wxWindowID id = wxID_ANY,
    long style = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);

  /// Gets the STC scintilla lexer.
  const wxString GetLexer() const;

  /// Gets the normal text value.
  const wxString GetText() const;

  /// Gets raw text value.
  const wxCharBuffer GetTextRaw() const;

  /// Sets the STC lexer.
  void SetLexer(const wxString& lexer);

  /// Sets the text (either normal or raw).
  void SetText(const wxString& text);
protected:
  void OnCommand(wxCommandEvent& command);
private:
  wxExSTC* m_STC;

  DECLARE_EVENT_TABLE()
};

#endif // wxUSE_GUI
#endif
