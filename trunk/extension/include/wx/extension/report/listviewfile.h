////////////////////////////////////////////////////////////////////////////////
// Name:      listviewfile.h
// Purpose:   Declaration of class 'wxExListViewFile'
// Author:    Anton van Wezenbeek
// Created:   2010-01-29
// RCS-ID:    $Id$
// Copyright: (c) 2010 Anton van Wezenbeek
////////////////////////////////////////////////////////////////////////////////

#ifndef _EX_REPORT_LISTVIEWFILE_H
#define _EX_REPORT_LISTVIEWFILE_H

#include <wx/extension/file.h>
#include <wx/extension/report/listview.h>

class wxExConfigDialog;

/// Combines wxExListViewWithFrame and wxExFile, giving you a list control with file
/// synchronization support.
class wxExListViewFile : public wxExListViewWithFrame, public wxExFile
{
public:
  /// Constructor for a LIST_FILE, opens the file.
  wxExListViewFile(wxWindow* parent,
    wxExFrameWithHistory* frame,
    const wxString& file,
    wxWindowID id = wxID_ANY,
    long menu_flags = LIST_MENU_DEFAULT,
    const wxPoint& pos = wxDefaultPosition,
    const wxSize& size = wxDefaultSize,
    long style = wxLC_LIST  | wxLC_HRULES | wxLC_VRULES | wxSUNKEN_BORDER,
    const wxValidator& validator = wxDefaultValidator,
    const wxString &name = wxListCtrlNameStr);

  /// Destructor.
 ~wxExListViewFile();

  /// Adds items.
  void AddItems();

  // Interface, for wxExListView overriden methods.
  /// Sets contents changed if we are not syncing.
  virtual void AfterSorting();

  /// Returns member.
  virtual bool GetContentsChanged() const {return m_ContentsChanged;};

  /// Invokes base and clears the list.
  void FileNew(const wxExFileName& filename = wxExFileName());

  virtual bool ItemFromText(const wxString& text);

  /// Resets the member.
  virtual void ResetContentsChanged() {m_ContentsChanged = false;};
protected:
  virtual void BuildPopupMenu(wxExMenu& menu);
  /// Loads the file and gets all data as list items.
  virtual void DoFileLoad(bool synced = false);
  /// Saves list items to file.
  virtual void DoFileSave(bool save_as = false);
  void OnCommand(wxCommandEvent& event);
  void OnIdle(wxIdleEvent& event);
  void OnMouse(wxMouseEvent& event);
private:
  void AddItemsDialog();
  void Initialize();

  bool m_ContentsChanged;
  wxExConfigDialog* m_AddItemsDialog;

  const wxString m_TextAddFiles;
  const wxString m_TextAddFolders;
  const wxString m_TextAddRecursive;
  const wxString m_TextAddWhat;
  const wxString m_TextInFolder;

  DECLARE_EVENT_TABLE()
};
#endif