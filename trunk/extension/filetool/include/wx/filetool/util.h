/******************************************************************************\
* File:          util.h
* Purpose:       Include file for wxextension report utilities
* Author:        Anton van Wezenbeek
* RCS-ID:        $Id$
*
* Copyright (c) 1998-2009, Anton van Wezenbeek
* All rights are reserved. Reproduction in whole or part is prohibited
* without the written consent of the copyright owner.
\******************************************************************************/

#ifndef _EX_REPORT_UTIL_H
#define _EX_REPORT_UTIL_H

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/aui/auibar.h>
#include <wx/aui/auibook.h>
#include <wx/filename.h>

#include <wx/extension/dir.h>
#include <wx/extension/statistics.h>

class exFrameWithHistory;
class exListViewFile;

/// Adds frame and listview interface to exDir.
class exDirWithReport : public exDir
{
public:
  /// When using FindFiles each found file is added as listitem to the listview.
  /// When using RunTool findfiles is also used to get all matching files,
  /// and on these files RunTool is invoked.
  exDirWithReport(exListViewFile* listview,
    const wxString& fullpath,
    const wxString& filespec = wxEmptyString);

  /// Allows you to FindFiles on a frame.
  /// In the findfiles all found files are opened using OpenFile from frame.
  exDirWithReport(exFrameWithHistory* frame,
    const wxString& fullpath,
    const wxString& filespec,
    long flags = 0);

  /// Calls RunTool from exTextFile on all matching files.
  /// SetupTool from exTextFile should already be called.
  size_t RunTool(const exTool& tool, int flags = wxDIR_DEFAULT);

  /// Gets the statistics.
  const exFileNameStatistics& GetStatistics() {return m_Statistics;};
protected:
  virtual void OnFile(const wxString& file);
private:
  exFileNameStatistics m_Statistics;
  exFrameWithHistory* m_Frame;
  exListViewFile* m_ListView;
  const long m_Flags;
  bool m_RunningTool;
  exTool m_Tool;
};

/// Offers a find toolbar, containing a find combobox and checkboxes.
/// The find combobox allows you to find in an exSTCWithFrame 
/// component on the specified exFrameWithHistory.
class exFindToolBar : public wxAuiToolBar
{
public:
  /// Constructor.
  exFindToolBar(wxWindow* parent, exFrameWithHistory* frame, wxWindowID id = wxID_ANY);
protected:
  void OnCommand(wxCommandEvent& event);
private:
  wxCheckBox* m_RegularExpression;
  wxCheckBox* m_MatchCase;
  wxCheckBox* m_MatchWholeWord;

  DECLARE_EVENT_TABLE()
};

/*! \file */

/// Compares the files, using wxExecute on comparator set in the config.
bool exCompareFile(const wxFileName& file1, const wxFileName& file2);

/// Shows a find in files dialog and finds or replaces text in files if chosen.
void exFindInFiles(exFrameWithHistory* frame, bool replace = false);

/// Finds other filenames from the one specified in the same dir structure.
/// Results are put on the list if not null, or in the filename if not null.
bool exFindOtherFileName(
  const wxFileName& filename,
  exListViewFile* listview,
  wxFileName* lastfile); // in case more files found, only most recent here

/// Do something (id) for all pages on the notebook.
bool exForEach(wxAuiNotebook* notebook, int id, const wxFont& font = wxFont());

/// Opens files and updates history, both for files and projects.
void exOpenFiles(exFrameWithHistory* frame,
  const wxArrayString& files,
  long flags = 0);
#endif
