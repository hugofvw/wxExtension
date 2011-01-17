/******************************************************************************\
* File:          vcs.cpp
* Purpose:       Implementation of wxExVCS class
* Author:        Anton van Wezenbeek
* RCS-ID:        $Id$
*
* Copyright (c) 1998-2009 Anton van Wezenbeek
* All rights are reserved. Reproduction in whole or part is prohibited
* without the written consent of the copyright owner.
\******************************************************************************/

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/config.h>
#include <wx/xml/xml.h>
#include <wx/stdpaths.h>
#include <wx/extension/vcs.h>
#include <wx/extension/configdlg.h>
#include <wx/extension/defs.h>
#include <wx/extension/filename.h>
#include <wx/extension/util.h>

std::map<wxString, wxExVCSEntry> wxExVCS::m_Entries;
wxFileName wxExVCS::m_FileName;

wxExVCS::wxExVCS(const wxArrayString& files, int menu_id)
  : m_Files(files)
{
  Initialize(menu_id);
}

bool wxExVCS::CheckPath(const wxString& vcs, const wxFileName& fn)
{
  if (!fn.IsOk() || vcs.empty())
  {
    return false;
  }
  
  // these cannot be combined, as AppendDir is a void (2.9.1).
  wxFileName path(fn);
  path.AppendDir(vcs);

  if (path.DirExists())
  {
    return true;
  }
  else
  {
    wxFileName path(fn);
    path.AppendDir("." + vcs);
    return path.DirExists();
  }
}

bool wxExVCS::CheckPathAll(
  const wxString& vcs, 
  const wxFileName& fn)
{
  if (!fn.IsOk() || vcs.empty())
  {
    return false;
  }
  
  const wxString use_vcs = (vcs == "mercurial" ? "hg": vcs);

  // The .git dir only exists in the root, so check all components.
  wxFileName root(fn);

  while (root.DirExists() && root.GetDirCount() > 0)
  {
    wxFileName path(root);
    path.AppendDir("." + use_vcs);

    if (path.DirExists())
    {
      return true;
    }

    root.RemoveLastDir();
  }

  return false;
}

#if wxUSE_GUI
int wxExVCS::ConfigDialog(
  wxWindow* parent,
  const wxString& title) const
{
  if (m_Entries.empty())
  {
    return wxID_CANCEL;
  }
  
  std::map<long, const wxString> choices;
  choices.insert(std::make_pair((long)VCS_NONE, _("None")));
  choices.insert(std::make_pair((long)VCS_AUTO, "Auto"));

  for (
    auto it = m_Entries.begin();
    it != m_Entries.end();
    ++it)
  {
    choices.insert(std::make_pair(it->second.GetNo(), it->second.GetName()));
  }

  // Estimate number of columns used by the radiobox.
  int cols = 5;

  switch (choices.size())
  {
    case 6:
    case 11:
      cols = 3;
      break;

    case 7:
    case 8:
    case 12:
    case 16:
      cols = 4;
      break;
  }

  std::vector<wxExConfigItem> v;

  v.push_back(wxExConfigItem(
    "VCS",
    choices,
    true, // use a radiobox 
    wxEmptyString, 
    cols));

  for (
    auto it2 = m_Entries.begin();
    it2 != m_Entries.end();
    ++it2)
  {
    v.push_back(wxExConfigItem(it2->second.GetName(), CONFIG_FILEPICKERCTRL));
  }

  return wxExConfigDialog(parent, v, title).ShowModal();
}
#endif

bool wxExVCS::DirExists(const wxFileName& filename)
{
  const wxString vcs = FindEntry(filename).GetName();

  // When adding a vcs, also check FindEntry.
  if ((vcs == "git" || vcs == "mercurial") && CheckPathAll(vcs, filename))
  {
    return true;
  }
  // This is the default check.
  else 
  {
    return CheckPath(vcs, filename);
  }
}

long wxExVCS::Execute()
{
  const wxFileName filename(GetFile());

  wxString wd;
  wxString file;

  if (!filename.IsOk())
  {
    wd = wxExConfigFirstOf(_("Base folder"));

    if (m_Command.IsAdd())
    {
      file = wxExConfigFirstOf(_("Path"));
    }
  }
  else
  {
    if (m_Files.size() > 1)
    {
      // File should not be surrounded by double quotes.
      for (
        auto it = m_Files.begin();
        it != m_Files.end();
        it++)
      {
        file += *it + " ";
      }
    }
    else if (m_Entry.GetName() == "git")
    {
      wd = filename.GetPath();
      file = "\"" + filename.GetFullName() + "\"";
    }
    else if (m_Entry.GetName() == "SCCS")
    {
      file = "\"" + 
      // SCCS for windows does not handle windows paths,
      // so convert them to UNIX, and add volume as well.
#ifdef __WXMSW__      
        filename.GetVolume() + filename.GetVolumeSeparator() +
#endif        
        filename.GetFullPath(wxPATH_UNIX) + "\"";
    }
    else
    {
      file = "\"" + filename.GetFullPath() + "\"";
    }
  }

  wxString comment;

  if (m_Command.IsCommit())
  {
    comment = 
      "-m \"" + wxExConfigFirstOf(_("Revision comment")) + "\" ";
  }

  wxString subcommand;
  
  if (UseSubcommand())
  {
    subcommand = wxConfigBase::Get()->Read(_("Subcommand"));

    if (!subcommand.empty())
    {
      subcommand += " ";
    }
  }

  wxString flags;

  if (UseFlags())
  {
    flags = GetFlags();

    if (!flags.empty())
    {
      flags += " ";

      // If we specified help flags, we do not need a file argument.      
      if (flags.Contains("help"))
      {
        file.clear();
      }
    }
  }

  const wxString bin = wxConfigBase::Get()->Read(m_Entry.GetName(), "svn");

  if (bin.empty())
  {
    wxLogError(m_Entry.GetName() + " " + _("path is empty"));
    return -1;
  }

  if (m_Entry.GetFlagsLocation() == wxExVCSEntry::VCS_FLAGS_LOCATION_POSTFIX)
  {
    return wxExCommand::Execute(
      bin + " " + m_Command.GetCommand() + " " + subcommand + flags + comment + file, 
      wd);
  }
  else
  {
    return wxExCommand::Execute(
      bin + " " + flags + m_Command.GetCommand() + " " + subcommand + comment + file, 
      wd);
  }
}

#if wxUSE_GUI
wxStandardID wxExVCS::ExecuteDialog(wxWindow* parent)
{
  if (ShowDialog(parent) == wxID_CANCEL)
  {
    return wxID_CANCEL;
  }

  return (Execute() < 0 ? wxID_CANCEL: wxID_OK);
}
#endif

const wxExVCSEntry wxExVCS::FindEntry(const wxFileName& filename)
{
  const long vcs = wxConfigBase::Get()->ReadLong("VCS", VCS_AUTO);

  if (vcs == VCS_AUTO)
  {
    if (!filename.IsOk())
    {
      return wxExVCSEntry();
    }
    else
    {
      for (
        auto it = m_Entries.begin();
        it != m_Entries.end();
        ++it)
      {
        const wxString name = it->second.GetName();

        if ((name == "git" || name == "mercurial") &&
             CheckPathAll(name, filename))
        {
          return it->second;
        }
        else if (CheckPath(name, filename))
        {
          return it->second;
        }
      }

      return wxExVCSEntry();
    }
  }
  else
  {
    for (
      auto it = m_Entries.begin();
      it != m_Entries.end();
      ++it)
    {
      if (it->second.GetNo() == vcs)
      {
        return it->second;
      }
    }
  }
  
  return wxExVCSEntry();
}

const wxString wxExVCS::GetFile() const
{
  if (m_Files.empty())
  {
    return m_Command.GetCommand().empty() ? 
      wxExConfigFirstOf(_("Base folder")): wxString(wxEmptyString);
  }
  else
  {
    return m_Files[0];
  }
}

const wxString wxExVCS::GetFlags() const
{
  return wxConfigBase::Get()->Read(_("Flags"));
}

void wxExVCS::Initialize(int menu_id)
{
  m_Entry = FindEntry(GetFile());
  
  m_Command = m_Entry.GetCommand(menu_id);
  
  if (!m_Entry.GetName().empty())
  {
    m_Caption = m_Entry.GetName() + " " + m_Command.GetCommand();
    m_FlagsKey = wxString::Format(
      "vcsflags/%s%d", m_Entry.GetName().c_str(), m_Command.GetNo());
  
    if (!m_Command.IsHelp() && m_Files.size() == 1)
    {
      m_Caption += " " + m_Files[0];
    }
  }
  else
  {
    // This should not really occur,
    // give some defaults to be able to fix this
    // using the dialog.
    m_Caption = "VCS";
    m_Command = wxExVCSCommand("help");
    m_FlagsKey = "vcsflags/VCS";
  }
}

bool wxExVCS::Read()
{
  // This test is to prevent showing an error if the vcs file does not exist,
  // as this is not required.
  if (!m_FileName.FileExists())
  {
    return false;
  }

  wxXmlDocument doc;

  if (!doc.Load(m_FileName.GetFullPath()))
  {
    return false;
  }

  // Initialize members.
  const int old_entries = m_Entries.size();
  
  m_Entries.clear();
  wxExVCSEntry::ResetInstances();

  wxXmlNode* child = doc.GetRoot()->GetChildren();

  while (child)
  {
    if (child->GetName() == "vcs")
    {
      const wxExVCSEntry vcs(child);
      m_Entries.insert(std::make_pair(vcs.GetName(), vcs));
    }

    child = child->GetNext();
  }

  if (old_entries == 0)
  {
    // Add default VCS.
    if (!wxConfigBase::Get()->Exists("VCS"))
    {
      wxConfigBase::Get()->Write("VCS", (long)VCS_AUTO);
    }
  }
  else if (old_entries != m_Entries.size())
  {
    // If current number of entries differs from old one,
    // we added or removed an entry. That might give problems
    // with the vcs id stored in the config, so reset it. 
    wxConfigBase::Get()->Write("VCS", (long)VCS_AUTO);
  }
  
  return true;
}

#if wxUSE_GUI
wxStandardID wxExVCS::Request(wxWindow* parent)
{
  wxStandardID retValue;

  if ((retValue = ExecuteDialog(parent)) == wxID_OK)
  {
    ShowOutput();
  }

  return retValue;
}
#endif

#if wxUSE_GUI
int wxExVCS::ShowDialog(wxWindow* parent) const
{
  std::vector<wxExConfigItem> v;

  if (m_Command.IsCommit())
  {
    v.push_back(wxExConfigItem(
      _("Revision comment"), 
      CONFIG_COMBOBOX,
      wxEmptyString,
      true)); // required
  }

  if (m_Files.empty() && !m_Command.IsHelp())
  {
    v.push_back(wxExConfigItem(
      _("Base folder"), 
      CONFIG_COMBOBOXDIR, 
      wxEmptyString, 
      true,
      1000));

    if (m_Command.IsAdd())
    {
      v.push_back(wxExConfigItem(
        _("Path"), 
        CONFIG_COMBOBOX,
        wxEmptyString, 
        true)); // required
    }
  }

  if (UseFlags())
  {
    wxConfigBase::Get()->Write(
      _("Flags"), 
      wxConfigBase::Get()->Read(m_FlagsKey));

    v.push_back(wxExConfigItem(_("Flags")));
  }

  if (UseSubcommand())
  {
    v.push_back(wxExConfigItem(_("Subcommand")));
  }
  
  const int retValue = wxExConfigDialog(parent,
    v,
    m_Caption).ShowModal();
    
  if (retValue == wxID_OK)
  {
    if (UseFlags())
    {
      wxConfigBase::Get()->Write(m_FlagsKey, GetFlags());
    }
  }
  
  return retValue;
}
#endif

#if wxUSE_GUI
void wxExVCS::ShowOutput(const wxString& caption) const
{
  // Add a lexer when appropriate.
  if (m_Command.IsOpen() && !GetError() && !m_Command.IsHistory())
  {
    const wxExFileName filename(GetFile());
  
    if (filename.GetLexer().IsOk())
    {
      GetDialog()->SetLexer(filename.GetLexer().GetScintillaLexer());
    }
  }
  else if (m_Command.IsDiff())
  {
    GetDialog()->SetLexer("diff");
  }
  else
  {
    GetDialog()->SetLexer(wxEmptyString);
  }

  wxExCommand::ShowOutput(m_Caption);
}
#endif

bool wxExVCS::Use() const
{
  return wxConfigBase::Get()->ReadLong("VCS", VCS_AUTO) != VCS_NONE;
}