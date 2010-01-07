/******************************************************************************\
* File:          listitem.cpp
* Purpose:       Implementation of class 'wxExListItem'
* Author:        Anton van Wezenbeek
* RCS-ID:        $Id$
*
* Copyright (c) 1998-2009 Anton van Wezenbeek
* All rights are reserved. Reproduction in whole or part is prohibited
* without the written consent of the copyright owner.
\******************************************************************************/

#include <wx/config.h>
#include <wx/extension/frame.h>
#include <wx/extension/report/listitem.h>
#include <wx/extension/report/dir.h>
#include <wx/extension/report/textfile.h>
#include <wx/extension/report/util.h>

// Do not give an error if columns do not exist.
// E.g. the LIST_PROCESS has none of the file columns.
wxExListItem::wxExListItem(
  wxExListView* lv, 
  int itemnumber)
  : m_ListView(lv)
  , m_FileName(
      (!lv->GetItemText(itemnumber, _("File Name"), false).empty() ?
          lv->GetItemText(itemnumber, _("In Folder"), false) + wxFileName::GetPathSeparator() +
          lv->GetItemText(itemnumber, _("File Name"), false) : wxString(wxEmptyString))
      )
  , m_FileSpec(lv->GetItemText(itemnumber, _("Type"), false))
{
  SetId(itemnumber);
  m_IsReadOnly = (m_ListView->GetItemData(itemnumber) > 0);
}

wxExListItem::wxExListItem(
  wxExListView* listview,
  const wxExFileName& filename,
  const wxString& filespec)
  : m_ListView(listview)
  , m_FileName(filename)
  , m_FileSpec(filespec)
{
  SetId(-1);
}

void wxExListItem::Insert(long index)
{
  SetId(index == -1 ? m_ListView->GetItemCount(): index);
  const int col = m_ListView->FindColumn(_("File Name"), false);
  const wxString filename = (
    m_FileName.FileExists() || m_FileName.DirExists() ?
      m_FileName.GetFullName():
      m_FileName.GetFullPath());

  if (col == 0)
  {
    SetColumn(col); // do not combine this with next statement in SetItemText!!
    SetText(filename);
  }

  m_ListView->InsertItem(*this);
  
  if (m_ListView->IsShown())
  {
#if wxUSE_STATUSBAR
    m_ListView->UpdateStatusBar();
#endif
  }

  if (m_FileName.GetStat().IsOk())
  {
    SetImage(m_FileName.GetIconID());
  }
  else
  {
    SetImage(-1);
  }

  Update();

  if (col > 0)
  {
    SetItemText(col, filename);
  }
}

const wxExFileStatistics wxExListItem::Run(const wxExTool& tool)
{
#if wxUSE_STATUSBAR
  wxExFrame::StatusText(m_FileName.GetFullPath());
#endif

  if (m_FileName.FileExists())
  {
    wxExTextFileWithListView file(m_FileName, tool);

    if (file.RunTool())
    {
      if (tool.IsRCSType())
      {
        Update();
      }
    }

    return file.GetStatistics();
  }
  else
  {
    wxExDirTool dir(tool, m_FileName.GetFullPath(), m_FileSpec);

    if (dir.FindFiles())
    {
      // Here we show the counts of individual folders on the top level.
      if (tool.IsCount() && m_ListView->GetSelectedItemCount() > 1)
      {
        tool.Log(&dir.GetStatistics().GetElements(), m_FileName.GetFullPath());
      }
    }

    return dir.GetStatistics();
  }
}

void wxExListItem::SetReadOnly(bool readonly)
{
  if (readonly)
  {
    SetTextColour(wxConfigBase::Get()->ReadObject(
      _("List Colour"), wxColour("RED")));
  }
  else
  {
    SetTextColour(*wxBLACK);
  }

  // Using GetTextColour did not work, so keep state in boolean.
  m_IsReadOnly = readonly;
  m_ListView->SetItemData(GetId(), m_IsReadOnly);
}

void wxExListItem::Update()
{
  SetReadOnly(m_FileName.GetStat().IsReadOnly());

  if (!m_ListView->SetItem(*this))
  {
    wxFAIL;
    return;
  }

  if (m_FileName.FileExists() ||
      wxFileName::DirExists(m_FileName.GetFullPath()))
  {
    const unsigned long size = m_FileName.GetStat().st_size; // to prevent warning
    SetItemText(_("Type"),
      (wxFileName::DirExists(m_FileName.GetFullPath()) ?
         m_FileSpec:
         m_FileName.GetExt()));
    SetItemText(_("In Folder"), m_FileName.GetPath());
    SetItemText(_("Size"),
      (!wxFileName::DirExists(m_FileName.GetFullPath()) ?
         (wxString::Format("%lu", size)):
          wxString(wxEmptyString)));
    SetItemText(_("Modified"), m_FileName.GetStat().GetModificationTime());
  }
}

void wxExListItem::UpdateRevisionList(const wxExRCS& rcs)
{
  SetItemText(_("Revision"), rcs.GetRevisionNumber());
  SetItemText(_("Date"), rcs.GetRevisionTime().FormatISOCombined(' '));
  SetItemText(_("Initials"), rcs.GetUser());
  SetItemText(_("Revision Comment"), rcs.GetDescription());
}
