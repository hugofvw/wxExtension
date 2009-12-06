/******************************************************************************\
* File:          dialog.cpp
* Purpose:       Implementation of wxExDialog class
* Author:        Anton van Wezenbeek
* RCS-ID:        $Id$
*
* Copyright (c) 1998-2009 Anton van Wezenbeek
* All rights are reserved. Reproduction in whole or part is prohibited
* without the written consent of the copyright owner.
\******************************************************************************/

#include <wx/extension/dialog.h>

#if wxUSE_GUI
BEGIN_EVENT_TABLE(wxExDialog, wxDialog)
  EVT_CHAR_HOOK(wxExDialog::OnKeyDown)
END_EVENT_TABLE()

wxExDialog::wxExDialog(wxWindow* parent,
  const wxString& title,
  long button_flags,
  wxWindowID id,
  const wxPoint& pos,
  const wxSize& size,
  long style,
  const wxString& name)
  : wxDialog(parent, id, title, pos, size, style, name)
  , m_ButtonFlags(button_flags)
  , m_TopSizer(new wxFlexGridSizer(1, 0, 0))
  , m_UserSizer(new wxFlexGridSizer(1, 0, 0))
{
}

wxSizerItem* wxExDialog::AddUserSizer(
  wxWindow* window,
  const wxSizerFlags& flags)
{
  wxSizerItem* item = m_UserSizer->Add(window, flags);

  if (flags.GetFlags() & wxEXPAND)
  {
    m_UserSizer->AddGrowableRow(m_UserSizer->GetChildren().GetCount() - 1);
  }

  return item;
}

wxSizerItem* wxExDialog::AddUserSizer(
  wxSizer* sizer,
  const wxSizerFlags& flags)
{
  wxSizerItem* item = m_UserSizer->Add(sizer, flags);

  if (flags.GetFlags() & wxEXPAND)
  {
    m_UserSizer->AddGrowableRow(m_UserSizer->GetChildren().GetCount() - 1);
  }

  return item;
}

void wxExDialog::LayoutSizers()
{
  m_TopSizer->AddGrowableCol(0);
  m_UserSizer->AddGrowableCol(0);

  wxSizerFlags flag;
  flag.Expand().Center().Border();

  // The top sizer starts with a spacer, for a nice border.
  m_TopSizer->AddSpacer(wxSizerFlags::GetDefaultBorder());

  // Then place the growable user sizer.
  m_TopSizer->Add(m_UserSizer, flag);
  m_TopSizer->AddGrowableRow(m_TopSizer->GetChildren().GetCount() - 1); // so this is the user sizer

  // Then the button sizer.
  if (m_ButtonFlags != 0)
  {
    wxSizer* sbz = CreateSeparatedButtonSizer(m_ButtonFlags);

    if (sbz != NULL)
    {
      m_TopSizer->Add(sbz, flag);
    }
  }

  // The top sizer ends with a spacer as well.
  m_TopSizer->AddSpacer(wxSizerFlags::GetDefaultBorder());

  SetSizerAndFit(m_TopSizer);
}

void wxExDialog::OnKeyDown(wxKeyEvent& event)
{
  if (event.GetKeyCode() == WXK_RETURN)
  {
    wxCommandEvent event(wxEVT_COMMAND_BUTTON_CLICKED, wxID_OK);
    wxPostEvent(this, event);
  }
  if (event.GetKeyCode() == WXK_ESCAPE)
  {
    wxCommandEvent event(wxEVT_COMMAND_BUTTON_CLICKED, wxID_CANCEL);
    wxPostEvent(this, event);
  }
  else
  {
    event.Skip();
  }
}

#endif // wxUSE_GUI
