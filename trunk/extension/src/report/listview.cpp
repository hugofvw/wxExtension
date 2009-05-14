/******************************************************************************\
* File:          listview.cpp
* Purpose:       Implementation of class 'wxExListViewFile' and support classes
* Author:        Anton van Wezenbeek
* RCS-ID:        $Id$
*
* Copyright (c) 1998-2009 Anton van Wezenbeek
* All rights are reserved. Reproduction in whole or part is prohibited
* without the written consent of the copyright owner.
\******************************************************************************/

#include <wx/tokenzr.h>
#include <wx/extension/configdialog.h>
#include <wx/extension/extension.h>
#include <wx/extension/svn.h>
#include <wx/extension/report/listview.h>
#include <wx/extension/report/listitem.h>
#include <wx/extension/report/defs.h>
#include <wx/extension/report/frame.h>
#include <wx/extension/report/process.h>
#include <wx/extension/report/stc.h>
#include <wx/extension/report/textfile.h>
#include <wx/extension/report/util.h>

#if wxUSE_DRAG_AND_DROP
class ListViewDropTarget : public wxFileDropTarget
{
public:
  ListViewDropTarget(wxExListViewFile* owner) {m_Owner = owner;}
private:
  virtual bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames);
  wxExListViewFile* m_Owner;
};
#endif

#ifdef __WXMSW__
class RBSFile : public wxExFile
{
public:
  RBSFile(wxExListViewFile* listview);
  void GenerateDialog();
private:
  void Body(
    const wxString& filename,
    const wxString& source,
    const wxString& destination);
  void Footer();
  void GenerateTransmit(const wxString& text);
  void GenerateWaitFor(const wxString& text);
  void Header();
  bool Substitute(
    wxString& text,
    const wxString& pattern,
    const wxString& new_pattern,
    const bool is_required);
  wxExListViewFile* m_Owner;
  wxString m_Prompt;
};
#endif

const int ID_LISTVIEW = 100;

BEGIN_EVENT_TABLE(wxExListViewFile, wxExListView)
  EVT_IDLE(wxExListViewFile::OnIdle)
  EVT_LIST_ITEM_ACTIVATED(ID_LISTVIEW, wxExListViewFile::OnList)
  EVT_LIST_ITEM_SELECTED(ID_LISTVIEW, wxExListViewFile::OnList)
  EVT_MENU(wxID_ADD, wxExListViewFile::OnCommand)
  EVT_MENU(ID_TERMINATED_PROCESS, wxExListViewFile::OnCommand)
  EVT_MENU_RANGE(wxID_CUT, wxID_PROPERTIES, wxExListViewFile::OnCommand)
  EVT_MENU_RANGE(ID_LIST_LOWEST, ID_LIST_HIGHEST, wxExListViewFile::OnCommand)
  EVT_MENU_RANGE(ID_TOOL_LOWEST, ID_TOOL_HIGHEST, wxExListViewFile::OnCommand)
  EVT_LEFT_DOWN(wxExListViewFile::OnMouse)
  EVT_RIGHT_DOWN(wxExListViewFile::OnMouse)
END_EVENT_TABLE()

wxExProcessWithListView* wxExListViewFile::m_Process = NULL;

wxExListViewFile::wxExListViewFile(wxWindow* parent,
  ListType type,
  long menu_flags,
  const wxExLexer* lexer,
  const wxPoint& pos,
  const wxSize& size,
  long style,
  const wxValidator& validator)
  : wxExListView(parent, ID_LISTVIEW, pos, size, style, validator, IMAGE_FILE_ICON)
  , wxExFile()
  , m_ContentsChanged(false)
  , m_ItemUpdated(false)
  , m_ItemNumber(0)
  , m_MenuFlags(menu_flags)
  , m_Type(type)
{
  Initialize(lexer);
}

wxExListViewFile::wxExListViewFile(wxWindow* parent,
  const wxString& file,
  const wxString& wildcard,
  long menu_flags,
  const wxPoint& pos,
  const wxSize& size,
  long style,
  const wxValidator& validator)
  : wxExListView(parent, ID_LISTVIEW, pos, size, style, validator, IMAGE_FILE_ICON)
  , wxExFile()
  , m_ContentsChanged(false)
  , m_ItemUpdated(false)
  , m_ItemNumber(0)
  , m_MenuFlags(menu_flags)
  , m_Type(LIST_PROJECT)
{
  Initialize(NULL);
  SetWildcard(wildcard);
  FileOpen(file);
}

int wxExListViewFile::AddItems()
{
  // To initialize the combobox.
  wxExApp::GetConfig(_("Add what"), wxExApp::GetLexers()->BuildComboBox());

  std::vector<wxExConfigItem> v;
  v.push_back(wxExConfigItem(_("Add what"), CONFIG_COMBOBOX, wxEmptyString, true));
  v.push_back(wxExConfigItem(_("In folder"), CONFIG_COMBOBOXDIR, wxEmptyString, true));
  v.push_back(wxExConfigItem());
  v.push_back(wxExConfigItem(_("Add files"), CONFIG_CHECKBOX));
  v.push_back(wxExConfigItem());
  v.push_back(wxExConfigItem(_("Add folders"), CONFIG_CHECKBOX));

  if (wxExConfigDialog(NULL,
    wxExApp::GetConfig(),
    v,
    _("Add Files")).ShowModal() == wxID_CANCEL)
  {
    return 0;
  }

  wxExDirWithListView dir(
    this,
    wxExApp::GetConfig(_("In folder")),
    wxExApp::GetConfig(_("Add what")));

  int flags = 0;
  if (wxExApp::GetConfigBool(_("Add files"))) flags |= wxDIR_FILES;
  if (wxExApp::GetConfigBool(_("Add folders"))) flags |= wxDIR_DIRS;

  const int retValue = dir.FindFiles(flags);

  if (retValue > 0)
  {
    m_ContentsChanged = true;
  }

  if (wxExApp::GetConfigBool("List/SortSync") && m_Type == LIST_PROJECT)
  {
    SortColumn(wxExApp::GetConfig(
      "List/SortColumn",
      FindColumn(_("Modified"))), SORT_KEEP);
  }

  const wxString text = _("Added") + wxString::Format(" %d ", retValue) + _("file(s)");

  wxExFrame::StatusText(text);

  return retValue;
}

void wxExListViewFile::AfterSorting()
{
  // Only if we are not sort syncing, set contents changed.
  if (!wxExApp::GetConfigBool("List/SortSync"))
  {
    m_ContentsChanged = true;
  }
}

void wxExListViewFile::BuildPopupMenu(wxExMenu& menu)
{
  bool add = false;
  bool exists = true;
  bool is_folder = false;
  bool is_make = false;
  bool read_only = false;

  if (GetSelectedItemCount() == 1)
  {
    const wxExListItemWithFileName item(this, GetFirstSelected());

    is_folder = wxDirExists(item.GetFileName().GetFullPath());
    exists = item.GetFileName().GetStat().IsOk();
    read_only = item.GetFileName().GetStat().IsReadOnly();
    is_make = item.GetFileName().GetLexer().GetScintillaLexer() == "makefile";
  }

  if (GetSelectedItemCount() >= 1)
  {
    if (exists)
    {
      menu.Append(ID_LIST_OPEN_ITEM, _("&Open"), wxART_FILE_OPEN);

      if (is_make)
      {
        menu.Append(ID_LIST_RUN_MAKE, _("&Make"));
      }
    }

    if (GetSelectedItemCount() > 1)
    {
      if (!wxExApp::GetConfig(_("Comparator")).empty())
      {
        menu.Append(ID_LIST_COMPARE, _("C&ompare"));
      }
    }
    else
    {
      if (m_Type != LIST_PROJECT &&
          !wxExApp::GetConfigBool("SVN") &&
          m_Frame != NULL && exists && !is_folder)
      {
        wxExListViewFile* list = m_Frame->Activate(LIST_PROJECT);

        if (list != NULL && list->GetSelectedItemCount() == 1)
        {
          wxExListItemWithFileName thislist(this, GetFirstSelected());
          const wxString current_file = thislist.GetFileName().GetFullPath();

          wxExListItemWithFileName otherlist(list, list->GetFirstSelected());
          const wxString with_file = otherlist.GetFileName().GetFullPath();

          if (current_file != with_file &&
              !wxExApp::GetConfig(_("Comparator")).empty())
          {
            menu.Append(ID_LIST_COMPARE,
              _("&Compare With") + " " + wxExGetEndOfText(with_file));
          }
        }
      }
    }

    if (exists && !is_folder)
    {
      if (!wxExApp::GetConfigBool("SVN"))
      {
        if (!wxExApp::GetConfig(_("Comparator")).empty())
        {
          menu.Append(ID_LIST_COMPARELAST, _("&Compare Recent Version"));
        }

        menu.Append(ID_LIST_VERSIONLIST, _("&Version List"));
      }
      else if (GetSelectedItemCount() == 1)
      {
        wxMenu* svnmenu = new wxMenu;
        svnmenu->Append(ID_LIST_SVN_DIFF, wxExEllipsed(_("&Diff")));
        svnmenu->Append(ID_LIST_SVN_LOG, wxExEllipsed(_("&Log")));
        svnmenu->Append(ID_LIST_SVN_CAT, wxExEllipsed(_("&Cat")));
        menu.AppendSubMenu(svnmenu, "&SVN");
      }
    }

#ifdef __WXMSW__
    if (exists && !is_folder && (m_MenuFlags & LIST_MENU_RBS))
    {
      menu.AppendSeparator();
      menu.Append(ID_LIST_SEND_ITEM, wxExEllipsed(_("&Build RBS File")));
      menu.AppendSeparator();
    }
#endif
  }
  else
  {
    if (m_Type == LIST_PROJECT)
    {
      if (!m_FileName.IsOk() ||
          !m_FileName.FileExists() ||
          (m_FileName.FileExists() && !m_FileName.GetStat().IsReadOnly()))
      {
        add = true;
      }
    }
  }

  // The ID_TOOL_REPORT_FIND and REPLACE only work if there is a frame,
  // so if not do not add them.
  // Also, finding in the LIST_FIND and REPLACE would result in recursive calls.
  if ( m_Frame != NULL && exists &&
       m_Type != LIST_FIND && m_Type != LIST_REPLACE &&
       GetSelectedItemCount() > 0 &&
      (m_MenuFlags & LIST_MENU_REPORT_FIND))
  {
    menu.Append(ID_TOOL_REPORT_FIND, wxExEllipsed(GetFindInCaption(ID_TOOL_REPORT_FIND)));

    if (!read_only)
    {
      menu.Append(ID_TOOL_REPORT_REPLACE, wxExEllipsed(GetFindInCaption(ID_TOOL_REPORT_REPLACE)));
    }

    menu.AppendSeparator();
  }

  if (add)
  {
    menu.Append(wxID_ADD);
    menu.AppendSeparator();
  }

  wxExListView::BuildPopupMenu(menu);

  if (m_Frame != NULL && GetSelectedItemCount() > 0 && exists &&
     (m_MenuFlags & LIST_MENU_TOOL))
  {
    menu.AppendSeparator();
    menu.AppendTools();
  }
}

void wxExListViewFile::CleanUp()
{
  wxDELETE(m_Process);
}

void wxExListViewFile::DeleteDoubles()
{
  wxDateTime mtime((time_t)0);
  wxString name;
  const int itemcount = GetItemCount();

  for (int i = itemcount - 1; i >= 0; i--)
  {
    wxExListItemWithFileName item(this, i);

    // Delete this element if it has the same mtime
    // and the same name as the previous one.
    if (mtime == item.GetFileName().GetStat().st_mtime &&
        name == item.GetColumnText(_("File Name")))
    {
      DeleteItem(i);
    }
    else
    {
      mtime = item.GetFileName().GetStat().st_mtime;
      name = item.GetColumnText(_("File Name"));
    }
  }

  if (itemcount != GetItemCount())
  {
    ItemsUpdate();
    m_ContentsChanged = true;
  }
}


bool wxExListViewFile::FileNew(const wxExFileName& filename)
{
  if (!wxExFile::FileNew(filename))
  {
    return false;
  }

  EditClearAll();
  return true;
}

bool wxExListViewFile::FileOpen(const wxExFileName& filename)
{
  if (!wxExFile::FileOpen(filename))
  {
    return false;
  }

  EditClearAll();

  const wxCharBuffer& buffer = Read();

  wxStringTokenizer tkz(buffer.data(), wxTextFile::GetEOL());

  while (tkz.HasMoreTokens())
  {
    ItemFromText(tkz.GetNextToken());
  }

  wxFile::Close();

  if (m_Frame != NULL)
  {
    m_Frame->SetRecentProject(filename.GetFullPath());
  }

  m_ContentsChanged = false; // override behaviour from ItemFromText

  if (wxExApp::GetConfigBool("List/SortSync"))
    SortColumn(wxExApp::GetConfig("List/SortColumn", FindColumn(_("Modified"))), SORT_KEEP);
  else
    SortColumnReset();

  return true;
}

bool wxExListViewFile::FileSave()
{
  if (m_FileName.GetName().empty())
  {
    return FileSaveAs();
  }

  if (!wxFile::Open(m_FileName.GetFullPath(), wxFile::write))
  {
    return false;
  }

  for (int i = 0; i < GetItemCount(); i++)
  {
    Write(ItemToText(i) + wxTextFile::GetEOL());
  }

  wxExFile::FileSave();

  m_ContentsChanged = false;

  return true;
}

const wxString wxExListViewFile::GetFindInCaption(int id)
{
  const wxString prefix =
    (id == ID_TOOL_REPORT_REPLACE ?
       _("Replace In"):
       _("Find In")) + " ";

  if (GetSelectedItemCount() == 1)
  {
    wxExListItem item(this, GetFirstSelected());

    // The File Name is better than using 0, as it can be another column as well.
    return prefix + item.GetColumnText(_("File Name"));
  }
  else if (GetSelectedItemCount() > 1)
  {
    return prefix + _("Selection");
  }
  else if (!m_FileName.GetName().empty())
  {
    return prefix + m_FileName.GetName();
  }
  else
  {
    return prefix + GetName();
  }
}

int wxExListViewFile::GetTypeTool(const wxExTool& tool)
{
  switch (tool.GetId())
  {
    case ID_TOOL_REPORT_COUNT: return LIST_COUNT; break;
    case ID_TOOL_REPORT_FIND: return LIST_FIND; break;
    case ID_TOOL_REPORT_KEYWORD: return LIST_KEYWORD; break;
    case ID_TOOL_REPORT_REPLACE: return LIST_REPLACE; break;
    case ID_TOOL_REPORT_REVISION: return LIST_REVISION; break;
    case ID_TOOL_REPORT_SQL: return LIST_SQL; break;
    case ID_TOOL_REPORT_VERSION: return LIST_VERSION; break;
    default: wxFAIL; return LIST_PROJECT;
  }
}

const wxString wxExListViewFile::GetTypeDescription(ListType type)
{
  wxString value;

  switch (type)
  {
  case LIST_COUNT: value = _("File Count"); break;
  case LIST_FIND: value = _("Find Results"); break;
  case LIST_HISTORY: value = _("History"); break;
  case LIST_KEYWORD: value = _("Keywords"); break;
  case LIST_PROCESS: value = _("Process Output"); break;
  case LIST_PROJECT: value = _("Project"); break;
  case LIST_REPLACE: value = _("Replace Results"); break;
  case LIST_REVISION: value = _("Revisions"); break;
  case LIST_SQL: value = _("SQL Queries"); break;
  case LIST_VERSION: value = _("Version List"); break;
  default: wxFAIL;
  }

  return value;
}

void wxExListViewFile::Initialize(const wxExLexer* lexer)
{
  SetName(GetTypeDescription());

  if (m_Type == LIST_KEYWORD)
  {
    if (lexer == NULL)
    {
      wxFAIL;
      return;
    }

    SetName(GetName() + " " + lexer->GetScintillaLexer());
  }

  wxWindow* window = wxTheApp->GetTopWindow();
  m_Frame = wxDynamicCast(window, wxExFrameWithHistory);

#ifndef __WXMSW__
  // Under Linux this should be done before adding any columns, under MSW it does not matter!
  SetSingleStyle(wxLC_REPORT);
#else
  // Set initial style depending on type.
  // Might be improved.
  SetSingleStyle((m_Type == LIST_PROJECT || m_Type == LIST_HISTORY ?
    wxExApp::GetConfig("List/Style", wxLC_REPORT) :
    wxLC_REPORT));
#endif

  if (m_Type != LIST_PROCESS)
  {
    InsertColumn(_("File Name"), wxExColumn::COL_STRING);
  }

  switch (m_Type)
  {
  case LIST_COUNT:
    // See wxExTextFileWithListView::Report, the order in which columns are set should be the same there.
    InsertColumn(_("Lines"));
    InsertColumn(_("Lines Of Code"));
    InsertColumn(_("Empty Lines"));
    InsertColumn(_("Words Of Code"));
    InsertColumn(_("Comments"));
    InsertColumn(_("Comment Size"));
  break;
  case LIST_FIND:
  case LIST_REPLACE:
    InsertColumn(_("Line"), wxExColumn::COL_STRING, 400);
    InsertColumn(_("Match"), wxExColumn::COL_STRING);
    InsertColumn(_("Line No"));
  break;
  case LIST_KEYWORD:
    for (
      std::set<wxString>::const_iterator it = lexer->GetKeywords().begin();
      it != lexer->GetKeywords().end();
      ++it)
    {
      InsertColumn(*it);
    }

    InsertColumn(_("Keywords"));
  break;
  case LIST_PROCESS:
    InsertColumn(_("Line"), wxExColumn::COL_STRING, 500);
    InsertColumn(_("Line No"));
    InsertColumn(_("File Name"), wxExColumn::COL_STRING);
  break;
  case LIST_REVISION:
    InsertColumn(_("Revision Comment"), wxExColumn::COL_STRING, 400);
    InsertColumn(_("Date"), wxExColumn::COL_DATE);
    InsertColumn(_("Initials"), wxExColumn::COL_STRING);
    InsertColumn(_("Line No"));
    InsertColumn(_("Revision"), wxExColumn::COL_STRING);
  break;
  case LIST_SQL:
    InsertColumn(_("Run Time"), wxExColumn::COL_DATE);
    InsertColumn(_("Query"), wxExColumn::COL_STRING, 400);
    InsertColumn(_("Line No"));
  break;
  default: break; // to prevent warnings
  }

  if (m_Type == LIST_REPLACE)
  {
    InsertColumn(_("Replaced"));
  }

  InsertColumn(_("Modified"), wxExColumn::COL_DATE);
  InsertColumn(_("In Folder"), wxExColumn::COL_STRING, 175);
  InsertColumn(_("Type"), wxExColumn::COL_STRING);
  InsertColumn(_("Size"));

  if (m_Type == LIST_HISTORY && m_Frame != NULL)
  {
    m_Frame->UseFileHistoryList(this);
  }

#if wxUSE_DRAG_AND_DROP
  SetDropTarget(new ListViewDropTarget(this));
#endif
}

bool wxExListViewFile::ItemOpenFile(int item_number)
{
  if (item_number < 0) return false;

  wxExListItemWithFileName item(this, item_number);

  if (wxFileName::DirExists(item.GetFileName().GetFullPath()))
  {
    wxTextEntryDialog dlg(this,
      _("Input") + ":",
      _("Folder Type"),
      item.GetColumnText(_("Type")));

    if (dlg.ShowModal() == wxID_CANCEL)
    {
      return false;
    }
    else
    {
      item.SetColumnText(_("Type"), dlg.GetValue());
      return true;
    }
  }
  else if (item.GetFileName().FileExists() && m_Frame != NULL)
  {
    const wxString line_number_str = item.GetColumnText(_("Line No"), false);
    const int line_number = (!line_number_str.empty() ? atoi(line_number_str.c_str()): 0);
    const wxString match =
      (m_Type == LIST_REPLACE ?
         item.GetColumnText(_("Replaced")):
         item.GetColumnText(_("Match"), false));

    const bool retValue = m_Frame->OpenFile(
      item.GetFileName().GetFullPath(),
      line_number, match);

    SetFocus();
    return retValue;
  }
  else
  {
    return false;
  }
}

void wxExListViewFile::ItemsUpdate()
{
  for (int i = 0; i < GetItemCount(); i++)
  {
    wxExListItemWithFileName item(this, i);
    item.Update();
  }
}

bool wxExListViewFile::ItemFromText(const wxString& text)
{
  if (text.empty())
  {
    wxLogError("Text is empty");
    return false;
  }

  wxStringTokenizer tkz(text, GetFieldSeparator());
  if (tkz.HasMoreTokens())
  {
    const wxString value = tkz.GetNextToken();
    wxFileName fn(value);

    if (fn.FileExists())
    {
      wxExListItemWithFileName item(this, value);
      item.Insert();

      // And try to set the rest of the columns (that are not already set by inserting).
      int col = 1;
      while (tkz.HasMoreTokens() && col < GetColumnCount() - 1)
      {
        const wxString value = tkz.GetNextToken();

        if (col != FindColumn(_("Type")) &&
            col != FindColumn(_("In Folder")) &&
            col != FindColumn(_("Size")) &&
            col != FindColumn(_("Modified")))
        {
          item.SetColumnText(col, value);
        }

        col++;
      }
    }
    else
    {
      // Now we need only the first column (containing findfiles). If more
      // columns are present, these are ignored.
      const wxString findfiles =
        (tkz.HasMoreTokens() ? tkz.GetNextToken(): tkz.GetString());

      wxExListItemWithFileName dir(this, value, findfiles);
      dir.Insert();
    }
  }
  else
  {
    wxExListItemWithFileName item(this, text);
    item.Insert();
  }

  m_ContentsChanged = true;

  return true;
}

const wxString wxExListViewFile::ItemToText(int item_number)
{
  wxExListItemWithFileName item(this, item_number);

  wxString text =
    (item.GetFileName().FileExists() ? item.GetFileName().GetFullPath(): item.GetFileName().GetFullName());

  if (wxFileName::DirExists(item.GetFileName().GetFullPath()))
  {
    text += GetFieldSeparator() + item.GetColumnText(_("Type"));
  }

  if (GetType() != LIST_PROJECT)
  {
    text += GetFieldSeparator() + wxExListView::ItemToText(item_number);
  }

  return text;
}

void wxExListViewFile::OnCommand(wxCommandEvent& event)
{
  if (event.GetId() > ID_TOOL_LOWEST && event.GetId() < ID_TOOL_HIGHEST)
  {
    RunItems(event.GetId());
    return;
  }

  switch (event.GetId())
  {
  // These are added to disable changing this listview if it is read-only,
  // or if there is no file involved.
  case wxID_CLEAR:
  case wxID_CUT:
  case wxID_DELETE:
  case wxID_PASTE:
    if (m_FileName.GetStat().IsOk())
    {
      if (!m_FileName.GetStat().IsReadOnly())
      {
        event.Skip();
        m_ContentsChanged = true;
      }
    }
    else
    {
      event.Skip();
      m_ContentsChanged = false;
    }
  break;

  case wxID_ADD: AddItems(); break;

  case ID_LIST_SVN_CAT:
    wxExSVN(SVN_CAT, wxExListItemWithFileName(this, GetNextSelected(-1)).GetFileName().GetFullPath()).ExecuteAndShowOutput();
  break;
  case ID_LIST_SVN_DIFF:
    wxExSVN(SVN_DIFF, wxExListItemWithFileName(this, GetNextSelected(-1)).GetFileName().GetFullPath()).ExecuteAndShowOutput();
  break;
  case ID_LIST_SVN_LOG:
    wxExSVN(SVN_LOG, wxExListItemWithFileName(this, GetNextSelected(-1)).GetFileName().GetFullPath()).ExecuteAndShowOutput();
  break;

  case ID_LIST_COMPARE:
  case ID_LIST_COMPARELAST:
  case ID_LIST_VERSIONLIST:
  {
    if (m_Frame == NULL) return;

    bool first = true;
    wxString file1,file2;
    int i = -1;
    bool found = false;

    wxExListViewFile* list = NULL;

    if (event.GetId() == ID_LIST_VERSIONLIST)
    {
      list = m_Frame->Activate(LIST_VERSION);
      wxASSERT(list != NULL);
    }

    while ((i = GetNextSelected(i)) != -1)
    {
      wxExListItemWithFileName li(this, i);
      const wxFileName* filename = &li.GetFileName();
      if (wxFileName::DirExists(filename->GetFullPath())) continue;
      switch (event.GetId())
      {
        case ID_LIST_COMPARE:
        {
          if (GetSelectedItemCount() == 1)
          {
            list = m_Frame->Activate(LIST_PROJECT);
            if (list == NULL) return;
            int main_selected = list->GetFirstSelected();
            wxExCompareFile(wxExListItemWithFileName(list, main_selected).GetFileName(), *filename);
          }
          else
          {
            if (first)
            {
              first = false;
              file1 = filename->GetFullPath();
            }
            else
            {
              first = true;
              file2 = filename->GetFullPath();
            }
            if (first) wxExCompareFile(wxFileName(file1), wxFileName(file2));
          }
        }
        break;
        case ID_LIST_COMPARELAST:
        {
          wxFileName lastfile;
          if (wxExFindOtherFileName(*filename, NULL, &lastfile))
          {
            wxExCompareFile(*filename, lastfile);
          }
        }
        break;
        case ID_LIST_VERSIONLIST:
          if (wxExFindOtherFileName(*filename, list, NULL))
          {
            found = true;
          }
        break;
      }
    }

    if (event.GetId() == ID_LIST_VERSIONLIST && found)
    {
      list->SortColumn(list->FindColumn(_("Modified")), SORT_DESCENDING);
      list->DeleteDoubles();
    }
  }
  break;

  case ID_LIST_OPEN_ITEM:
  {
    int i = -1;
    while ((i = GetNextSelected(i)) != -1)
    {
      if (!ItemOpenFile(i)) break;
    }
  }
  break;

  case ID_LIST_RUN_MAKE:
  {
    const wxExListItemWithFileName item(this, GetNextSelected(-1));

    wxSetWorkingDirectory(item.GetFileName().GetPath());

    ProcessRun(
      wxExApp::GetConfig(_("Make")) + wxString(" ") +
      wxExApp::GetConfig("MakeSwitch", "-f") + wxString(" ") +
      item.GetFileName().GetFullPath());
  }
  break;

#ifdef __WXMSW__
  case ID_LIST_SEND_ITEM:
    RBSFile(this).GenerateDialog();
    break;
#endif

  case ID_TERMINATED_PROCESS:
    wxBell();
    wxDELETE(m_Process);
    break;

  default: event.Skip();
  }

  UpdateStatusBar();
}

void wxExListViewFile::OnIdle(wxIdleEvent& event)
{
  event.Skip();

  if (
    !IsShown() ||
     IsOpened() ||
     GetItemCount() == 0 ||
     !wxExApp::GetConfigBool("AllowSync", true))
  {
    return;
  }

  if (m_ItemNumber < GetItemCount())
  {
    wxExListItemWithFileName item(this, m_ItemNumber);

    if ( item.GetFileName().FileExists() &&
        (item.GetFileName().GetStat().GetModificationTime() != item.GetColumnText(_("Modified")) ||
        (size_t)item.GetFileName().GetStat().IsReadOnly() != GetItemData(m_ItemNumber))
        )
    {
      item.Update();
      item.GetFileName().StatusText(STAT_SYNC | STAT_FULLPATH);
      m_ItemUpdated = true;
    }

    m_ItemNumber++;
  }
  else
  {
    m_ItemNumber = 0;

    if (m_ItemUpdated)
    {
      if (wxExApp::GetConfigBool("List/SortSync") && m_Type == LIST_PROJECT)
      {
        SortColumn(wxExApp::GetConfig(
          "List/SortColumn",
          FindColumn(_("Modified"))), SORT_KEEP);
      }

      m_ItemUpdated = false;
    }
  }

  m_FileName.GetStat().Sync();

  CheckSyncNeeded();
}

void wxExListViewFile::OnList(wxListEvent& event)
{
  if (event.GetEventType() == wxEVT_COMMAND_LIST_ITEM_ACTIVATED)
  {
    ItemOpenFile(event.GetIndex());
  }
  else if (event.GetEventType() == wxEVT_COMMAND_LIST_ITEM_SELECTED)
  {
    if (GetSelectedItemCount() == 1)
    {
      wxExListItemWithFileName item(this, event.GetIndex());

      // The LIST_PROCESS is treated specially, since there will oexen be
      // entries that do not exist. We do not want a message in these cases.
      if ((m_Type != LIST_PROCESS) ||
          (m_Type == LIST_PROCESS && item.GetFileName().FileExists()))
      {
        item.GetFileName().StatusText(STAT_FULLPATH);
      }
    }

    event.Skip();
  }
}

void wxExListViewFile::OnMouse(wxMouseEvent& event)
{
  if (event.LeftDown())
  {
    event.Skip();
    int flags = wxLIST_HITTEST_ONITEM;
    const long index = HitTest(wxPoint(event.GetX(), event.GetY()), flags);
    if (index < 0)
    {
      if (m_FileName.FileExists())
      {
        m_FileName.StatusText();
      }
    }
  }
  else if (event.RightDown())
  {
    long style;

    if (m_Type == LIST_PROJECT)
    {
      // This contains the CAN_PASTE flag, only for these types.
      style = wxExMenu::MENU_DEFAULT;
    }
    else
    {
      style = 0;
    }

    if (m_FileName.FileExists() && m_FileName.GetStat().IsReadOnly()) style |= wxExMenu::MENU_IS_READ_ONLY;
    if (GetSelectedItemCount() > 0) style |= wxExMenu::MENU_IS_SELECTED;
    if (GetItemCount() == 0) style |= wxExMenu::MENU_IS_EMPTY;
    if (GetSelectedItemCount() == 0 && GetItemCount() > 0) style |= wxExMenu::MENU_ALLOW_CLEAR;
    wxExMenu menu(style);

    BuildPopupMenu(menu);
    PopupMenu(&menu);
  }

  UpdateStatusBar();
}

const wxString wxExListViewFile::PrintHeader() const
{
  if (m_FileName.FileExists())
  {
    return
      wxExGetEndOfText(m_FileName.GetFullPath(), 20) + " " +
      m_FileName.GetStat().GetModificationTime() + " " +
      wxDateTime::Now().Format();
  }
  else
  {
    return GetTypeDescription() + " " + wxDateTime::Now().Format();
  }
}

bool wxExListViewFile::ProcessIsRunning()
{
  return m_Process != NULL && m_Process->IsRunning();
}

void wxExListViewFile::ProcessRun(const wxString& command)
{
  wxASSERT(m_Process == NULL);

  // This is a static method, we cannot use m_Frame here.
  wxWindow* window = wxTheApp->GetTopWindow();
  wxExFrameWithHistory* frame= wxDynamicCast(window, wxExFrameWithHistory);

  if (frame == NULL) 
  {
    wxFAIL;
    return;
  }

  wxExListViewFile* listview = frame->Activate(LIST_PROCESS);

  if (listview == NULL) 
  {
    wxFAIL;
    return;
  }

  if ((m_Process = new wxExProcessWithListView(listview, command)) != NULL)
  {
    if (m_Process->Execute() <= 0)
    {
      wxDELETE(m_Process);
    }
  }
}

void wxExListViewFile::ProcessStop()
{
  if (ProcessIsRunning())
  {
    m_Process->Kill();
    wxDELETE(m_Process);
  }
}

void wxExListViewFile::RunItems(const wxExTool& tool)
{
  if ((tool.GetId() == ID_TOOL_REPORT_COUNT && m_Type == LIST_COUNT) ||
      (tool.GetId() == ID_TOOL_REPORT_KEYWORD && m_Type == LIST_KEYWORD) ||
      (tool.GetId() == ID_TOOL_REPORT_SQL && m_Type == LIST_SQL)
     )
  {
    return;
  }

  if (tool.IsFindType())
  {
    if (m_Frame != NULL)
    {
      wxExSTCWithFrame* stc = m_Frame->GetCurrentSTC();

      if (stc != NULL)
      {
        stc->GetSearchText();
      }
    }

    std::vector<wxExConfigItem> v;
    v.push_back(wxExConfigItem(_("Find what"), CONFIG_COMBOBOX, wxEmptyString, true));
    if (tool.GetId() == ID_TOOL_REPORT_REPLACE) v.push_back(wxExConfigItem(_("Replace with"), CONFIG_COMBOBOX));
    v.push_back(wxExConfigItem());
    v.push_back(wxExConfigItem(wxExApp::GetConfig()->GetFindReplaceData()->GetInfo()));

    if (wxExConfigDialog(NULL,
      wxExApp::GetConfig(),
      v,
      GetFindInCaption(tool.GetId())).ShowModal() == wxID_CANCEL)
    {
      return;
    }

    wxExApp::Log(wxExApp::GetConfig()->GetFindReplaceData()->GetText(tool.GetId() == ID_TOOL_REPORT_REPLACE));
  }

  if (!wxExTextFileWithListView::SetupTool(tool))
  {
    return;
  }

  int i = -1;

  wxExFileNameStatistics stats(GetFileName().GetName());

  while ((i = GetNextSelected(i)) != -1)
  {
    stats += wxExListItemWithFileName(this, i).Run(tool);

    wxYield();
  }

  stats.Log(tool);

  wxExFrame::StatusText(_("Ready"));

  if (tool.IsCount())
  {
    wxExOpenFile(
      wxExFileNameStatistics::GetLogfileName(),
      wxExSTC::STC_OPEN_FROM_STATISTICS | wxExSTC::STC_OPEN_IS_SYNCED);
  }
}

#if wxUSE_DRAG_AND_DROP
bool ListViewDropTarget::OnDropFiles(wxCoord, wxCoord, const wxArrayString& filenames)
{
  size_t nFiles = filenames.GetCount();

  for (size_t n = 0; n < nFiles; n++)
  {
    m_Owner->ItemFromText(filenames[n]);
  }

  if (wxExApp::GetConfigBool("List/SortSync"))
  {
    m_Owner->SortColumn(
      wxExApp::GetConfig("List/SortColumn",
      m_Owner->FindColumn(_("Modified"))), SORT_KEEP);
  }

  return true;
}
#endif

#ifdef __WXMSW__
RBSFile::RBSFile(wxExListViewFile* listview)
  : wxExFile()
  , m_Owner(listview)
  , m_Prompt(wxExApp::GetConfig("RBS/Prompt", ">"))
{
}

void RBSFile::Body(
  const wxString& filename,
  const wxString& source,
  const wxString& destination)
{
  GenerateTransmit("SET DEF [" + destination + "]");
  GenerateTransmit("; *** Sending: " + filename + " ***");
  GenerateWaitFor(m_Prompt);
  GenerateTransmit("KERMIT RECEIVE");
  Write(".KermitSendFile \"" + source + wxFILE_SEP_PATH + filename + "\",\"" + filename + ("\",rcASCII\n"));

  GenerateTransmit(wxEmptyString);
  GenerateWaitFor(m_Prompt);
  GenerateTransmit("; *** Done: " + filename + " ***");
}

void RBSFile::Footer()
{
  Write("End With\n");
  Write("End Sub\n");
}

void RBSFile::GenerateDialog()
{
  std::vector<wxExConfigItem> v;
  v.push_back(wxExConfigItem(_("RBS File"), CONFIG_FILEPICKERCTRL, wxEmptyString, true));
  v.push_back(wxExConfigItem(_("RBS Pattern"), CONFIG_DIRPICKERCTRL));
  wxExConfigDialog dlg(NULL, wxExApp::GetConfig(), v, _("Build RBS File"));
  if (dlg.ShowModal() == wxID_CANCEL) return;

  const wxString script = wxExApp::GetConfig(_("RBS File"));

  if (!Open(script, wxFile::write))
  {
    return;
  }

  wxBusyCursor wait;

  Header();

  const wxString rsx_pattern = wxExApp::GetConfig(_("RBS Pattern")) + wxFILE_SEP_PATH;
  int i = -1;
  while ((i = m_Owner->GetNextSelected(i)) != -1)
  {
    wxExListItemWithFileName li(m_Owner, i);
    const wxFileName* filename = &li.GetFileName();
    if (!wxFileName::DirExists(filename->GetFullPath()))
    {
      const wxString source = filename->GetPath();
      wxString destination = source, pattern, with;
      if (source.find(rsx_pattern) != wxString::npos)
      {
        pattern = rsx_pattern;
        with = wxExApp::GetConfig("RBS/With");
      }
      else
      {
        wxLogError("Cannot find: %s inside: %s", rsx_pattern.c_str(), source.c_str());
        return;
      }

      if (!Substitute(destination, pattern, with, true)) return;
      Substitute(destination, wxFILE_SEP_PATH, ",", false);
      Body(filename->GetFullName(), source, destination);
    }
  }

  Footer();
  Close();

  wxExApp::Log(_("RBS File") + ": " + script + " " + _("generated"));
}


void RBSFile::GenerateTransmit(const wxString& text)
{
  if (text.empty()) Write(".Transmit Chr$(13)\n");
  else              Write(".Transmit \"" + text + "\" & Chr$(13)\n");
}

void RBSFile::GenerateWaitFor(const wxString& text)
{
  const wxString pdp_11_spec = "Chr$(10) & ";
  Write(".WaitForString " + pdp_11_spec + "\"" + text + "\", 0, rcAllowKeyStrokes\n");
  Write(".Wait 1, rcAllowKeyStrokes\n");
}

void RBSFile::Header()
{
  Write("' Script generated by: " + wxTheApp->GetAppName() + ": " + wxDateTime::Now().Format(EX_TIMESTAMP_FORMAT) + "\n" +
        "' Do not modify this file, all changes will be lost!\n\n" +
        "Option Explicit\n" +
        "Sub Main\n\n" +
        "With Application\n\n");
}

bool RBSFile::Substitute(
  wxString& text,
  const wxString& pattern,
  const wxString& new_pattern,
  const bool is_required)
{
  size_t pos_pattern;
  if ((pos_pattern = text.find(pattern)) == wxString::npos)
  {
    if (is_required)
    {
      wxLogError("Cannot find pattern: " + pattern + " in: " + text);
    }

    return false;
  }

  text = text.substr(0, pos_pattern) + new_pattern + text.substr(pos_pattern + pattern.length());

  return true;
}
#endif
