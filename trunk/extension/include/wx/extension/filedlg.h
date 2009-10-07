////////////////////////////////////////////////////////////////////////////////
// Name:      filedlg.h
// Purpose:   Declaration of wxWidgets file dialog class
// Author:    Anton van Wezenbeek
// Created:   2009-10-07
// RCS-ID:    $Id$
// Copyright: (c) 2009 Anton van Wezenbeek
////////////////////////////////////////////////////////////////////////////////

#ifndef _EXFILEDLG_H
#define _EXFILEDLG_H

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/filedlg.h>

class wxExFile;

/// Adds an wxExFile to wxFileDialog.
class wxExFileDialog : public wxFileDialog
{
public:
  /// Constructor.
  wxExFileDialog(
    wxWindow *parent,
    wxExFile* file,
    const wxString &message=wxFileSelectorPromptStr, 
    const wxString &wildcard=wxFileSelectorDefaultWildcardStr,
    long style=wxFD_DEFAULT_STYLE, 
    const wxPoint &pos=wxDefaultPosition, 
    const wxSize &size=wxDefaultSize, 
    const wxString &name=wxFileDialogNameStr);

  /// Shows the dialog depending on the changes on the file.
  bool Continue();

  /// Shows the dialog, and returns dialog return code.
  int ShowModal(bool ask_for_continue = true);
private:
  wxExFile* m_File;
};
#endif
