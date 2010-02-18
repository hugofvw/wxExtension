/******************************************************************************\
* File:          menu.cpp
* Purpose:       Implementation of wxExMenu class
* Author:        Anton van Wezenbeek
* RCS-ID:        $Id$
*
* Copyright (c) 1998-2009 Anton van Wezenbeek
* All rights are reserved. Reproduction in whole or part is prohibited
* without the written consent of the copyright owner.
\******************************************************************************/

#include <map>
#include <wx/extension/menu.h>
#include <wx/extension/art.h>
#include <wx/extension/tool.h>
#include <wx/extension/vcs.h>
#include <wx/extension/util.h> // for wxExEllipsed

#if wxUSE_GUI

wxExMenu::wxExMenu(long style)
  : m_Style(style)
  , m_ItemsAppended(0)
  , m_IsSeparator(false)
  , m_MenuVCSFilled(false)
{
}

wxExMenu::wxExMenu(const wxExMenu& menu)
  : m_Style(menu.m_Style)
  , m_ItemsAppended(menu.m_ItemsAppended)
  , m_IsSeparator(menu.m_IsSeparator)
  , m_MenuVCSFilled(menu.m_MenuVCSFilled)
{
}

wxMenuItem* wxExMenu::Append(int id)
{
  m_ItemsAppended++;
  m_IsSeparator = false;

  wxMenuItem* item = new wxMenuItem(this, id);

  const wxExStockArt art(id);

  if (art.GetBitmap().IsOk())
  {
    item->SetBitmap(art.GetBitmap(
      wxART_MENU, 
      wxArtProvider::GetSizeHint(wxART_MENU, true)));
  }

  return wxMenu::Append(item);
}

wxMenuItem* wxExMenu::Append(
  int id,
  const wxString& name,
  const wxString& helptext,
  const wxArtID& artid)
{
  m_ItemsAppended++;
  m_IsSeparator = false;

  wxMenuItem* item = new wxMenuItem(this, id, name, helptext);

  if (!artid.empty())
  {
    const wxBitmap bitmap = 
      wxArtProvider::GetBitmap(
        artid, 
        wxART_MENU, 
        wxArtProvider::GetSizeHint(wxART_MENU, true));

    if (bitmap.IsOk())
    {
      item->SetBitmap(bitmap);
    }
  }

  return wxMenu::Append(item);
}

void wxExMenu::AppendEdit(bool add_invert)
{
  if (!(m_Style & MENU_IS_READ_ONLY) &&
       (m_Style & MENU_IS_SELECTED))
  {
    Append(wxID_CUT);
  }

  if (m_Style & MENU_IS_SELECTED)
  {
    Append(wxID_COPY);
  }

  if (!(m_Style & MENU_IS_READ_ONLY) &&
       (m_Style & MENU_CAN_PASTE))
  {
    Append(wxID_PASTE);
  }

  if (!(m_Style & MENU_IS_SELECTED) &&
      !(m_Style & MENU_IS_EMPTY))
  {
    Append(wxID_SELECTALL);
  }
  else
  {
    if (add_invert && !(m_Style & MENU_IS_EMPTY))
    {
      Append(ID_EDIT_SELECT_NONE, _("&Deselect All"));
    }
  }

  if (m_Style & MENU_ALLOW_CLEAR)
  {
    Append(wxID_CLEAR);
  }

  if (add_invert && !(m_Style & MENU_IS_EMPTY))
  {
    Append(ID_EDIT_SELECT_INVERT, _("&Invert"));
  }

  if (!(m_Style & MENU_IS_READ_ONLY) &&
       (m_Style & MENU_IS_SELECTED) &&
      !(m_Style & MENU_IS_EMPTY))
  {
    Append(wxID_DELETE);
  }
}

void wxExMenu::AppendPrint()
{
  Append(wxID_PRINT_SETUP, wxExEllipsed(_("Page &Setup")));
  Append(wxID_PREVIEW);
  Append(wxID_PRINT);
}

void wxExMenu::AppendSeparator()
{
  if (m_ItemsAppended == 0 || m_IsSeparator) return;

  wxMenu::AppendSeparator();

  m_IsSeparator = true;
}

void wxExMenu::AppendSubMenu(
  wxMenu *submenu,
  const wxString& text,
  const wxString& help,
  int itemid)
{
  m_ItemsAppended++; // count submenu as one
  m_IsSeparator = false;

  if (itemid == wxID_ANY)
  {
    wxMenu::AppendSubMenu(submenu, text, help);
  }
  else
  {
    // This one is deprecated, but is necessary if
    // we have an explicit itemid.
    wxMenu::Append(itemid, text, submenu, help);
  }
}

void wxExMenu::AppendVCS()
{
  wxExMenu* vcsmenu = new wxExMenu;
  vcsmenu->AppendVCS(ID_EDIT_VCS_LOG);
  vcsmenu->AppendVCS(ID_EDIT_VCS_STAT);
  vcsmenu->AppendVCS(ID_EDIT_VCS_DIFF);
  vcsmenu->AppendSeparator();
  vcsmenu->AppendVCS(ID_EDIT_VCS_COMMIT);
  vcsmenu->AppendSeparator();
  vcsmenu->AppendVCS(ID_EDIT_VCS_CAT);
  vcsmenu->AppendVCS(ID_EDIT_VCS_BLAME);
  vcsmenu->AppendSeparator();
  vcsmenu->AppendVCS(ID_EDIT_VCS_PROPLIST);
  vcsmenu->AppendVCS(ID_EDIT_VCS_PROPSET);
  vcsmenu->AppendSeparator();
  vcsmenu->AppendVCS(ID_EDIT_VCS_REVERT);
  vcsmenu->AppendSeparator();
  vcsmenu->AppendVCS(ID_EDIT_VCS_ADD);

  AppendSeparator();
  AppendSubMenu(vcsmenu, "&VCS");
}

void wxExMenu::AppendVCS(int id)
{
  const wxString command = wxExVCS(id).GetCommand();

  if (command.empty())
  {
    return;
  }

  const wxString text(wxExEllipsed("&" + command));

  Append(id, text);
}

void wxExMenu::AppendTools()
{
  wxExMenu* menuTool = new wxExMenu(*this);

  for (
    std::map <int, wxExToolInfo>::const_iterator it = 
      wxExTool::Get()->GetToolInfo().begin();
    it != wxExTool::Get()->GetToolInfo().end();
    ++it)
  {
    if (!it->second.GetText().empty())
    {
      menuTool->Append(
        it->first, 
        it->second.GetText(), 
        it->second.GetHelpText());
    }
  }

  AppendSubMenu(menuTool, _("&Tools"));
}

void wxExMenu::BuildVCS(bool fill)
{
  if (m_MenuVCSFilled)
  {
    wxMenuItem* item;

    while ((item = FindItem(wxID_SEPARATOR)) != NULL)
    {
      Destroy(item);
    }

    Destroy(ID_VCS_STAT);
    Destroy(ID_VCS_INFO);
    Destroy(ID_VCS_LOG);
    Destroy(ID_VCS_LS);
    Destroy(ID_VCS_DIFF);
    Destroy(ID_VCS_HELP);
    Destroy(ID_VCS_UPDATE);
    Destroy(ID_VCS_COMMIT);
    Destroy(ID_VCS_ADD);
  }

  if (fill)
  {
    AppendVCS(ID_VCS_STAT);
    AppendVCS(ID_VCS_INFO);
    AppendVCS(ID_VCS_LOG);
    AppendVCS(ID_VCS_LS);
    AppendVCS(ID_VCS_DIFF);
    AppendVCS(ID_VCS_HELP);
    AppendSeparator();
    AppendVCS(ID_VCS_UPDATE);
    AppendVCS(ID_VCS_COMMIT);
    AppendSeparator();
    AppendVCS(ID_VCS_ADD);
  }

  m_MenuVCSFilled = fill;
}

#endif // wxUSE_GUI
