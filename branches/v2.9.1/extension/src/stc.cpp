/******************************************************************************\
* File:          stc.cpp
* Purpose:       Implementation of class wxExSTC
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
#include <wx/numdlg.h>
#include <wx/tokenzr.h>
#include <wx/extension/stc.h>
#include <wx/extension/configdlg.h>
#include <wx/extension/configitem.h>
#include <wx/extension/frame.h>
#include <wx/extension/frd.h>
#include <wx/extension/header.h>
#include <wx/extension/lexers.h>
#include <wx/extension/log.h>
#include <wx/extension/printing.h>
#include <wx/extension/util.h>
#include <wx/extension/vcs.h>

#if wxUSE_GUI

const int SCI_ADDTEXT = 2001;

const wxFileOffset bytes_per_line = 16;
const wxFileOffset each_hex_field = 3;
const wxFileOffset space_between_fields = 1;
const wxFileOffset start_hex_field = 10;

BEGIN_EVENT_TABLE(wxExSTC, wxStyledTextCtrl)
  EVT_CHAR(wxExSTC::OnChar)
  EVT_IDLE(wxExSTC::OnIdle)
  EVT_KEY_DOWN(wxExSTC::OnKeyDown)
  EVT_KEY_UP(wxExSTC::OnKeyUp)
  EVT_KILL_FOCUS(wxExSTC::OnFocus)
  EVT_LEFT_UP(wxExSTC::OnMouse)
  EVT_MENU(ID_EDIT_OPEN_BROWSER, wxExSTC::OnCommand)
  EVT_MENU(ID_EDIT_OPEN_LINK, wxExSTC::OnCommand)
  EVT_MENU(ID_EDIT_READ, wxExSTC::OnCommand)
  EVT_MENU(wxID_DELETE, wxExSTC::OnCommand)
  EVT_MENU(wxID_JUMP_TO, wxExSTC::OnCommand)
  EVT_MENU(wxID_SELECTALL, wxExSTC::OnCommand)
  EVT_MENU(wxID_SORT_ASCENDING, wxExSTC::OnCommand)
  EVT_MENU(wxID_SORT_DESCENDING, wxExSTC::OnCommand)
  EVT_MENU_RANGE(ID_EDIT_STC_LOWEST, ID_EDIT_STC_HIGHEST, wxExSTC::OnCommand)
  EVT_MENU_RANGE(wxID_CUT, wxID_CLEAR, wxExSTC::OnCommand)
  EVT_MENU_RANGE(wxID_UNDO, wxID_REDO, wxExSTC::OnCommand)
  EVT_MOUSE_CAPTURE_LOST(wxExSTC::OnMouseCapture)
  EVT_RIGHT_UP(wxExSTC::OnMouse)
  EVT_SET_FOCUS(wxExSTC::OnFocus)
  EVT_STC_CHARADDED(wxID_ANY, wxExSTC::OnStyledText)
  EVT_STC_DWELLEND(wxID_ANY, wxExSTC::OnStyledText)
  EVT_STC_MACRORECORD(wxID_ANY, wxExSTC::OnStyledText)
  EVT_STC_MARGINCLICK(wxID_ANY, wxExSTC::OnStyledText)
//  EVT_STC_DWELLSTART(wxID_ANY, wxExSTC::OnStyledText)
END_EVENT_TABLE()

std::vector <wxString> wxExSTC::m_Macro;
wxExConfigDialog* wxExSTC::m_ConfigDialog = NULL;

wxExSTC::wxExSTC(wxWindow *parent, 
  const wxString& value,
  long win_flags,
  const wxString& title,
  long menu_flags,
  wxWindowID id,
  const wxPoint& pos,
  const wxSize& size, 
  long style)
  : wxStyledTextCtrl(parent, id , pos, size, style, title)
  , m_Flags(win_flags)
  , m_MenuFlags(menu_flags)
  , m_GotoLineNumber(1)
  , m_MarginDividerNumber(1)
  , m_MarginFoldingNumber(2)
  , m_MarginLineNumber(0)
  , m_MarkerChange(1)
  , m_vi(wxExVi(this))
  , m_File(this)
{
  Initialize();

  PropertiesMessage();

  if (!value.empty())
  {
    if (m_Flags & STC_WIN_HEX)
    {
      AddTextHexMode(0, value.c_str());
    }
    else
    {
      SetText(value);
    }

    GuessType();

    if (m_Flags & STC_WIN_READ_ONLY ||
        // At this moment we do not allow to write in hex mode.
        m_Flags & STC_WIN_HEX)
    {
      SetReadOnly(true);
    }
  }
}

wxExSTC::wxExSTC(wxWindow* parent,
  const wxExFileName& filename,
  int line_number,
  const wxString& match,
  long flags,
  long menu_flags,
  wxWindowID id,
  const wxPoint& pos,
  const wxSize& size,
  long style)
  : wxStyledTextCtrl(parent, id, pos, size, style)
  , m_File(this)
  , m_GotoLineNumber(1)
  , m_MarginDividerNumber(1)
  , m_MarginFoldingNumber(2)
  , m_MarginLineNumber(0)
  , m_MarkerChange(1)
  , m_Flags(flags)
  , m_MenuFlags(menu_flags)
  , m_vi(wxExVi(this))
{
  Initialize();

  Open(filename, line_number, match, flags);
}

wxExSTC::wxExSTC(const wxExSTC& stc)
  : wxStyledTextCtrl(stc.GetParent())
  , m_Flags(stc.m_Flags)
  , m_GotoLineNumber(stc.m_GotoLineNumber)
  , m_MenuFlags(stc.m_MenuFlags)
  , m_MarginDividerNumber(stc.m_MarginDividerNumber)
  , m_MarginFoldingNumber(stc.m_MarginFoldingNumber)
  , m_MarginLineNumber(stc.m_MarginLineNumber)
  , m_MarkerChange(stc.m_MarkerChange)
  , m_vi(wxExVi(this)) // do not use stc.m_vi, crash
  , m_File(this)
{
  Initialize();

  if (stc.m_File.GetFileName().IsOk())
  {
    Open(stc.m_File.GetFileName(), -1, wxEmptyString, GetFlags());
  }
}

void wxExSTC::AddAsciiTable()
{
  // Do not show an edge, eol or whitespace for ascii table.
  SetEdgeMode(wxSTC_EDGE_NONE);
  SetViewEOL(false);
  SetViewWhiteSpace(wxSTC_WS_INVISIBLE);

  // And override tab width.
  SetTabWidth(5);

  for (auto i = 1; i <= 255; i++)
  {
    AddText(wxString::Format("%d\t%c", i, (wxUniChar)i));
    AddText((i % 5 == 0) ? GetEOL(): "\t");
  }

  EmptyUndoBuffer();
  SetSavePoint();
}

void wxExSTC::AddBasePathToPathList()
{
  // First find the base path, if this is not yet on the list, add it.
  const wxString basepath_text = "Basepath:";

  const auto find = FindText(
    0,
    1000, // the max pos to look for, this seems enough
    basepath_text,
    wxSTC_FIND_WHOLEWORD);

  if (find == -1)
  {
    return;
  }

  const auto  line = LineFromPosition(find);
  const wxString basepath = GetTextRange(
    find + basepath_text.length() + 1,
    GetLineEndPosition(line) - 3);

  m_PathList.Add(basepath);
}

void wxExSTC::AddHeader()
{
  const wxExHeader header;

  if (header.ShowDialog(this) != wxID_CANCEL)
  {
    if (m_Lexer.GetScintillaLexer() == "hypertext")
    {
      GotoLine(1);
    }
    else
    {
      DocumentStart();
    }

    AddText(header.Get(&m_File.GetFileName()));
  }
}

void wxExSTC::AddTextHexMode(wxFileOffset start, const wxCharBuffer& buffer)
/*
e.g.:
offset    hex field                                         ascii field
00000000: 23 69 6e 63 6c 75 64 65  20 3c 77 78 2f 63 6d 64  #include <wx/cmd
00000010: 6c 69 6e 65 2e 68 3e 20  2f 2f 20 66 6f 72 20 77  line.h> // for w
00000020: 78 43 6d 64 4c 69 6e 65  50 61 72 73 65 72 0a 23  xCmdLineParser #
          <----------------------------------------------> bytes_per_line
          <-> each_hex_field
                                     space_between_fields <>
                                  <- mid_in_hex_field
*/
{
  SetControlCharSymbol('x');
  wxExLexers::Get()->ApplyHexStyles(this);
  wxExLexers::Get()->ApplyMarkers(this);

  // Do not show an edge, eol or whitespace in hex mode.
  SetEdgeMode(wxSTC_EDGE_NONE);
  SetViewEOL(false);
  SetViewWhiteSpace(wxSTC_WS_INVISIBLE);

  const wxFileOffset mid_in_hex_field = 7;

  wxString text;

  // Allocate space for the string.
  // Offset requires 10 * length / 16 bytes (+ 1 + 1 for separators, 
  // hex field 3 * length and the ascii field just the length.
  text.Alloc(
    (start_hex_field + 1 + 1) * buffer.length() / bytes_per_line + 
     buffer.length() * each_hex_field + buffer.length());

  for (
    wxFileOffset offset = 0; 
    offset < buffer.length(); 
    offset += bytes_per_line)
  {
    long count = buffer.length() - offset;
    count =
      (bytes_per_line < count ? bytes_per_line : count);

    wxString field_hex, field_ascii;

    for (register wxFileOffset byte = 0; byte < count; byte++)
    {
      const char c = buffer.data()[offset + byte];

      field_hex += wxString::Format("%02x ", (unsigned char)c);

      // Print an extra space.
      if (byte == mid_in_hex_field)
      {
        field_hex += ' ';
      }

      // We do not want the \n etc. to be printed,
      // as that disturbs the hex view field.
      if (c != 0 && c != '\r' && c != '\n' && c != '\t')
      {
        field_ascii += c;
      }
      else
      {
        // Therefore print an ordinary ascii char.
        field_ascii += '.';
      }
    }

    // The extra space if we ended too soon.
    if (count <= mid_in_hex_field)
    {
      field_hex += ' ';
    }

    text += wxString::Format("%08lx: ", (unsigned long)start + offset) +
      field_hex +
      wxString(
        ' ', 
        space_between_fields + ((bytes_per_line - count)* each_hex_field)) +
      field_ascii +
      GetEOL();
  }

  AddText(text);
}

void wxExSTC::AppendTextForced(const wxString& text, bool withTimestamp)
{
  const bool pos_at_end = (GetCurrentPos() == GetTextLength());
  const bool readonly = GetReadOnly();

  if (readonly)
  {
    SetReadOnly(false);
  }

  if (withTimestamp)
  {
    const wxString now = wxDateTime::Now().Format();
    AppendText(now + " " + text + GetEOL());
  }
  else
  {
    // No GetEOL, that is only added with timestamps.
    AppendText(text);
  }

  SetSavePoint();

  if (readonly)
  {
    SetReadOnly(true);
  }

  if (pos_at_end)
  {
    DocumentEnd();
  }
}

void wxExSTC::BuildPopupMenu(wxExMenu& menu)
{
  const wxString sel = GetSelectedText();

  if (m_MenuFlags & STC_MENU_OPEN_LINK)
  {
    const wxString link = GetTextAtCurrentPos();
    const auto line_no = (!sel.empty() ? 
      wxExGetLineNumber(sel): 
      GetLineNumberAtCurrentPos());

    wxString filename;
    if (LinkOpen(link, line_no, &filename))
    {
      menu.AppendSeparator();
      menu.Append(ID_EDIT_OPEN_LINK, _("Open") + " " + filename);
    }
  }

  if ( m_File.GetFileName().FileExists() && GetSelectedText().empty() &&
      (m_MenuFlags & STC_MENU_COMPARE_OR_VCS))
  {
    if (wxExVCS::Get()->DirExists(m_File.GetFileName()))
    {
      menu.AppendSeparator();
      menu.AppendVCS();
    }
    else if (!wxConfigBase::Get()->Read(_("Comparator")).empty())
    {
      menu.AppendSeparator();
      menu.Append(ID_EDIT_COMPARE, wxExEllipsed(_("&Compare Recent Version")));
    }
  }

  if (GetSelectedText().empty() && 
      (m_Lexer.GetScintillaLexer() == "hypertext" ||
       m_Lexer.GetScintillaLexer() == "xml"))
  {
    menu.AppendSeparator();
    menu.Append(ID_EDIT_OPEN_BROWSER, _("&Open In Browser"));
  }

  if (m_MenuFlags & STC_MENU_FIND && GetTextLength() > 0)
  {
    menu.AppendSeparator();
    menu.Append(wxID_FIND);
  }

  if (!GetReadOnly())
  {
    if (m_MenuFlags & STC_MENU_REPLACE && GetTextLength() > 0)
    {
      menu.Append(wxID_REPLACE);
    }
  }

  menu.AppendSeparator();
  menu.AppendEdit();

  if (!GetReadOnly())
  {
    if (!GetSelectedText().empty())
    {
      wxExMenu* menuSelection = menuSelection = new wxExMenu(menu);
      menuSelection->Append(ID_EDIT_UPPERCASE, _("&Uppercase\tF11"));
      menuSelection->Append(ID_EDIT_LOWERCASE, _("&Lowercase\tF12"));

      if (wxExGetNumberOfLines(GetSelectedText()) > 1)
      {
        wxExMenu* menuSort = new wxExMenu(menu);
        menuSort->Append(wxID_SORT_ASCENDING);
        menuSort->Append(wxID_SORT_DESCENDING);
        menuSelection->AppendSeparator();
        menuSelection->AppendSubMenu(menuSort, _("&Sort"));
      }

      menu.AppendSeparator();
      menu.AppendSubMenu(menuSelection, _("&Selection"));
    }
  }

  if (!GetReadOnly() && (CanUndo() || CanRedo()))
  {
    menu.AppendSeparator();
    if (CanUndo()) menu.Append(wxID_UNDO);
    if (CanRedo()) menu.Append(wxID_REDO);
  }

  // Folding if nothing selected, property is set,
  // and we have a lexer.
  if (
    GetSelectedText().empty() && 
    GetProperty("fold") == "1" &&
    m_Lexer.IsOk())
  {
    menu.AppendSeparator();
    menu.Append(ID_EDIT_TOGGLE_FOLD, _("&Toggle Fold\tCtrl+T"));
    menu.Append(ID_EDIT_FOLD_ALL, _("&Fold All Lines\tF9"));
    menu.Append(ID_EDIT_UNFOLD_ALL, _("&Unfold All Lines\tF10"));
  }
}

void wxExSTC::CheckAutoComp(const wxUniChar& c)
{
  static wxString autoc;

  if (isspace(GetCharAt(GetCurrentPos() - 1)))
  {
    autoc = c;
  }
  else
  {
    autoc += c;

    if (autoc.length() >= 3) // Only autocompletion for large words
    {
      if (!AutoCompActive())
      {
        AutoCompSetIgnoreCase(true);
        AutoCompSetAutoHide(false);
      }

      if (m_Lexer.KeywordStartsWith(autoc))
        AutoCompShow(
          autoc.length() - 1,
          m_Lexer.GetKeywordsString());
      else
        AutoCompCancel();
    }
  }
}

bool wxExSTC::CheckBrace(int pos)
{
  const auto brace_match = BraceMatch(pos);

  if (brace_match != wxSTC_INVALID_POSITION)
  {
    BraceHighlight(pos, brace_match);
    return true;
  }
  else
  {
    BraceHighlight(wxSTC_INVALID_POSITION, wxSTC_INVALID_POSITION);
    return false;
  }
}

bool wxExSTC::CheckBraceHex(int pos)
{
  const auto col = GetColumn(pos);
  const wxFileOffset start_ascii_field =
    start_hex_field + each_hex_field * bytes_per_line + 2 * space_between_fields;

  if (col >= start_ascii_field)
  {
    const auto offset = col - start_ascii_field;
    int space = 0;

    if (col >= start_ascii_field + bytes_per_line / 2)
    {
      space++;
    }

    BraceHighlight(pos,
      PositionFromLine(LineFromPosition(pos)) 
        + start_hex_field + each_hex_field * offset + space);
    return true;
  }
  else if (col >= start_hex_field)
  {
    if (GetCharAt(pos) != ' ')
    {
      int space = 0;

      if (col >= 
        start_hex_field + 
        space_between_fields + 
        (bytes_per_line * each_hex_field) / 2)
      {
        space++;
      }

      const auto offset = (col - (start_hex_field + space)) / each_hex_field;

      BraceHighlight(pos,
        PositionFromLine(LineFromPosition(pos)) + start_ascii_field + offset);
      return true;
    }
  }
  else
  {
    BraceHighlight(wxSTC_INVALID_POSITION, wxSTC_INVALID_POSITION);
  }

  return false;
}

void wxExSTC::ClearDocument()
{
  SetReadOnly(false);
  ClearAll();
#if wxUSE_STATUSBAR
  wxExFrame::StatusText(wxEmptyString, "PaneLines");
#endif
  EmptyUndoBuffer();
  SetSavePoint();
}

// This is a static method, cannot use normal members here.
int wxExSTC::ConfigDialog(
  wxWindow* parent,
  const wxString& title,
  long flags,
  wxWindowID id)
{
  std::vector<wxExConfigItem> items;

  const wxString page = 
    ((flags & STC_CONFIG_SIMPLE) ? wxString(wxEmptyString): _("Setting"));

  std::set<wxString> bchoices;
  bchoices.insert(_("End of line"));
  bchoices.insert(_("Line numbers"));
  bchoices.insert(_("Use tabs"));
  bchoices.insert(_("vi mode"));
  items.push_back(wxExConfigItem(bchoices, page, 2));

  std::map<long, const wxString> choices;
  choices.insert(std::make_pair(wxSTC_WS_INVISIBLE, _("Invisible")));
  choices.insert(std::make_pair(wxSTC_WS_VISIBLEAFTERINDENT, 
    _("Invisible after ident")));
  choices.insert(std::make_pair(wxSTC_WS_VISIBLEALWAYS, _("Visible always")));
  items.push_back(wxExConfigItem(_("Whitespace"), choices, true, page));

  std::map<long, const wxString> wchoices;
  wchoices.insert(std::make_pair(wxSTC_WRAP_NONE, _("None")));
  wchoices.insert(std::make_pair(wxSTC_WRAP_WORD, _("Word")));
  wchoices.insert(std::make_pair(wxSTC_WRAP_CHAR, _("Char")));
  items.push_back(wxExConfigItem(_("Wrap line"), wchoices, true, page));

  std::map<long, const wxString> vchoices;
  vchoices.insert(std::make_pair(wxSTC_WRAPVISUALFLAG_NONE, _("None")));
  vchoices.insert(std::make_pair(wxSTC_WRAPVISUALFLAG_END, _("End")));
  vchoices.insert(std::make_pair(wxSTC_WRAPVISUALFLAG_START, _("Start")));
  items.push_back(wxExConfigItem(_("Wrap visual flags"), vchoices, true, page));

  if (!(flags & STC_CONFIG_SIMPLE))
  {
    items.push_back(wxExConfigItem(_("Edge column"), 0, 500, _("Edge")));

    std::map<long, const wxString> echoices;
    echoices.insert(std::make_pair(wxSTC_EDGE_NONE, _("None")));
    echoices.insert(std::make_pair(wxSTC_EDGE_LINE, _("Line")));
    echoices.insert(std::make_pair(wxSTC_EDGE_BACKGROUND, _("Background")));
    items.push_back(wxExConfigItem(_("Edge line"), echoices, true, _("Edge")));

    items.push_back(wxExConfigItem(_("Auto fold"), 0, INT_MAX, _("Folding")));
    items.push_back(wxExConfigItem(_("Indentation guide"), CONFIG_CHECKBOX, 
      _("Folding")));

    std::map<long, const wxString> fchoices;
    // next no longer available
//    fchoices.insert(std::make_pair(wxSTC_FOLDFLAG_BOX, _("Box")));
    fchoices.insert(std::make_pair(wxSTC_FOLDFLAG_LINEBEFORE_EXPANDED, 
      _("Line before expanded")));
    fchoices.insert(std::make_pair(wxSTC_FOLDFLAG_LINEBEFORE_CONTRACTED, 
      _("Line before contracted")));
    fchoices.insert(std::make_pair(wxSTC_FOLDFLAG_LINEAFTER_EXPANDED, 
      _("Line after expanded")));
    fchoices.insert(std::make_pair(wxSTC_FOLDFLAG_LINEAFTER_CONTRACTED, 
      _("Line after contracted")));
    // next is experimental, wait for scintilla
    //fchoices.insert(std::make_pair(wxSTC_FOLDFLAG_LEVELNUMBERS, _("Level numbers")));
    items.push_back(wxExConfigItem(_("Fold flags"), fchoices, false, 
      _("Folding")));

    items.push_back(wxExConfigItem(_("Calltip"), CONFIG_COLOUR, _("Colour")));
    items.push_back(
      wxExConfigItem(_("Edge colour"), CONFIG_COLOUR, _("Colour")));

    items.push_back(wxExConfigItem(
      _("Tab width"), 
      1, 
      (int)wxConfigBase::Get()->ReadLong(_("Edge column"), 80), _("Margin")));
    items.push_back(wxExConfigItem(
      _("Indent"), 
      1, 
      (int)wxConfigBase::Get()->ReadLong(_("Edge column"), 80), _("Margin")));
    items.push_back(
      wxExConfigItem(_("Divider"), 0, 40, _("Margin")));
    items.push_back(
      wxExConfigItem(_("Folding"), 0, 40, _("Margin")));
    items.push_back(
      wxExConfigItem(_("Line number"), 0, 100, _("Margin")));

    items.push_back(
      wxExConfigItem(
        _("Include directory"), 
        _("Directory"), 
        wxTE_MULTILINE,
        false,
        false)); // no name
  }

  int buttons = wxOK | wxCANCEL;

  if (flags & STC_CONFIG_WITH_APPLY)
  {
    buttons |= wxAPPLY;
  }

  if (!(flags & STC_CONFIG_MODELESS))
  {
    return wxExConfigDialog(
      parent,
      items,
      title,
      0,
      1,
      buttons,
      id).ShowModal();
  }
  else
  {
    if (m_ConfigDialog == NULL)
    {
      m_ConfigDialog = new wxExConfigDialog(
        parent,
        items,
        title,
        0,
        1,
        buttons,
        id);
    }

    return m_ConfigDialog->Show();
  }
}

void wxExSTC::ConfigGet()
{
  if (!wxConfigBase::Get()->Exists(_("Calltip")))
  {
    wxConfigBase::Get()->SetRecordDefaults(true);
  }

  CallTipSetBackground(wxConfigBase::Get()->ReadObject(
    _("Calltip"), wxColour("YELLOW")));

  if (m_File.GetFileName().GetExt().CmpNoCase("log") == 0)
  {
    SetEdgeMode(wxSTC_EDGE_NONE);
  }
  else
  {
    SetEdgeColumn(wxConfigBase::Get()->ReadLong(_("Edge column"), 80));
    SetEdgeColour(wxConfigBase::Get()->ReadObject(
      _("Edge colour"), wxColour("GREY"))); 
    SetEdgeMode(wxConfigBase::Get()->ReadLong(_("Edge line"), wxSTC_EDGE_NONE));
  }
    
  SetFoldFlags(wxConfigBase::Get()->ReadLong( _("Fold flags"),
    wxSTC_FOLDFLAG_LINEBEFORE_CONTRACTED | wxSTC_FOLDFLAG_LINEAFTER_CONTRACTED));
  SetIndent(wxConfigBase::Get()->ReadLong(_("Indent"), 4));
  SetIndentationGuides(
    wxConfigBase::Get()->ReadBool(_("Indentation guide"), false));

  SetMarginWidth(
    m_MarginDividerNumber, 
    wxConfigBase::Get()->ReadLong(_("Divider"), 16));
  SetMarginWidth(
    m_MarginFoldingNumber, 
    wxConfigBase::Get()->ReadLong(_("Folding"), 16));

  const auto margin = wxConfigBase::Get()->ReadLong(
    _("Line number"), 
    TextWidth(wxSTC_STYLE_DEFAULT, "999999"));

  SetMarginWidth(
    m_MarginLineNumber, 
    (wxConfigBase::Get()->ReadBool(_("Line numbers"), false) ? margin: 0));

  SetTabWidth(wxConfigBase::Get()->ReadLong(_("Tab width"), 4));
  SetUseTabs(wxConfigBase::Get()->ReadBool(_("Use tabs"), false));
  SetViewEOL(wxConfigBase::Get()->ReadBool(_("End of line"), false));
  SetViewWhiteSpace(
    wxConfigBase::Get()->ReadLong(_("Whitespace"), wxSTC_WS_INVISIBLE));
  SetWrapMode(wxConfigBase::Get()->ReadLong(_("Wrap line"), wxSTC_WRAP_NONE));
  SetWrapVisualFlags(
    wxConfigBase::Get()->ReadLong(_("Wrap visual flags"), 
    wxSTC_WRAPVISUALFLAG_END));

  m_vi.Use(wxConfigBase::Get()->ReadBool(_("vi mode"), false));

  wxStringTokenizer tkz(
    wxConfigBase::Get()->Read(_("Include directory")),
    "\r\n");

  m_PathList.Empty();
  
  while (tkz.HasMoreTokens())
  {
    m_PathList.Add(tkz.GetNextToken());
  }

  if (wxConfigBase::Get()->IsRecordingDefaults())
  {
    // Set defaults only.
    wxConfigBase::Get()->ReadLong(_("Auto fold"), 2500);

    wxConfigBase::Get()->SetRecordDefaults(false);
  }
}

void wxExSTC::ControlCharDialog(const wxString& caption)
{
  if (GetSelectedText().length() > 1)
  {
    // Do nothing
    return;
  }

  if (GetReadOnly())
  {
    if (GetSelectedText().length() == 1)
    {
      const wxUniChar value = GetSelectedText().GetChar(0);
      wxMessageBox(
        wxString::Format("hex: %x dec: %d", value, value), 
        _("Control Character"));
    }

    return;
  }

  static long value = ' '; // don't use 0 as default as NULL is not handled

  if (GetSelectedText().length() == 1)
  {
    value = GetSelectedText().GetChar(0);
  }

  long new_value;
  if ((new_value = wxGetNumberFromUser(_("Input") + " 0 - 255:",
    wxEmptyString,
    caption,
    value,
    0,
    255,
    this)) < 0)
  {
    return;
  }

  if (GetSelectedText().length() == 1)
  {
    if (value != new_value)
    {
      ReplaceSelection(wxString::Format("%c", (wxUniChar)new_value));
    }

    SetSelection(GetCurrentPos(), GetCurrentPos() + 1);
  }
  else
  {
    char buffer[2];
    buffer[0] = (char)new_value;

    // README: The stc.h equivalents AddText, AddTextRaw, InsertText,
    // InsertTextRaw do not add the length.
    // To be able to add NULLs this is the only way.
    SendMsg(SCI_ADDTEXT, 1, (wxIntPtr)buffer);
  }
}

void wxExSTC::Cut()
{
  wxStyledTextCtrl::Cut();
  
  MarkerAddChange(GetCurrentLine());
}
  
void wxExSTC::EOLModeUpdate(int eol_mode)
{
  ConvertEOLs(eol_mode);
  SetEOLMode(eol_mode);
#if wxUSE_STATUSBAR
  UpdateStatusBar("PaneFileType");
#endif
}

bool wxExSTC::FileReadOnlyAttributeChanged()
{
  if (!(GetFlags() & STC_WIN_HEX))
  {
    SetReadOnly(m_File.GetFileName().GetStat().IsReadOnly()); // does not return anything
#if wxUSE_STATUSBAR
    wxExFrame::StatusText(_("Readonly attribute changed"));
#endif
  }

  return true;
}

void wxExSTC::FileTypeMenu()
{
  if (GetReadOnly())
  {
#if wxUSE_STATUSBAR
    wxExFrame::StatusText(_("Document is readonly"));
#endif
    return;
  }

  wxMenu* eol = new wxMenu();

  // The order here should be the same as the defines for wxSTC_EOL_CRLF.
  // So the FindItemByPosition can work
  eol->Append(ID_EDIT_EOL_DOS, "&DOS", wxEmptyString, wxITEM_CHECK);
  eol->Append(ID_EDIT_EOL_MAC, "&MAC", wxEmptyString, wxITEM_CHECK);
  eol->Append(ID_EDIT_EOL_UNIX, "&UNIX", wxEmptyString, wxITEM_CHECK);
  eol->FindItemByPosition(GetEOLMode())->Check();

  PopupMenu(eol);
}

bool wxExSTC::FindNext(bool find_next)
{
  return FindNext(
    wxExFindReplaceData::Get()->GetFindString(),
    wxExFindReplaceData::Get()->STCFlags(),
    find_next);
}

bool wxExSTC::FindNext(
  const wxString& text, 
  int search_flags,
  bool find_next)
{
  if (text.empty())
  {
    return false;
  }

  static bool recursive = false;
  static int start_pos, end_pos;

  if (find_next)
  {
    if (recursive) start_pos = 0;
    else
    {
      start_pos = GetCurrentPos();
      end_pos = GetTextLength();
    }
  }
  else
  {
    if (recursive) start_pos = GetTextLength();
    else
    {
      start_pos = GetCurrentPos();
      if (GetSelectionStart() != -1)
        start_pos = GetSelectionStart();

      end_pos = 0;
    }
  }

  SetTargetStart(start_pos);
  SetTargetEnd(end_pos);
  SetSearchFlags(search_flags);

  if (SearchInTarget(text) < 0)
  {
    wxExFindResult(text, find_next, recursive);
    
    if (!recursive)
    {
      recursive = true;
      FindNext(text, search_flags, find_next);
      recursive = false;
    }
    
    return false;
  }
  else
  {
    recursive = false;

    if (GetTargetStart() != GetTargetEnd())
    {
      SetSelection(GetTargetStart(), GetTargetEnd());
      EnsureVisible(LineFromPosition(GetTargetStart()));
      EnsureCaretVisible();
    }
    else
    {
      wxFAIL;
      return false;
    }

    return true;
  }
}

void wxExSTC::FoldAll()
{
  if (GetProperty("fold") != "1") return;

  const auto current_line = GetCurrentLine();

  int line = 0;
  while (line < GetLineCount())
  {
    const auto level = GetFoldLevel(line);
    const auto last_child_line = GetLastChild(line, level);

    if (last_child_line > line)
    {
      if (GetFoldExpanded(line)) ToggleFold(line);
      line = last_child_line + 1;
    }
    else
    {
      line++;
    }
  }

  GotoLine(current_line);
}

const wxString wxExSTC::GetEOL() const
{
  switch (GetEOLMode())
  {
  case wxSTC_EOL_CR: return "\r"; break;
  case wxSTC_EOL_CRLF: return "\r\n"; break;
  case wxSTC_EOL_LF: return "\n"; break;
  default: wxFAIL; break;
  }

  return "\r\n";
}

const wxString wxExSTC::GetFindString() const
{
  const wxString selection = const_cast< wxExSTC * >( this )->GetSelectedText();

  if (!selection.empty() && wxExGetNumberOfLines(selection) == 1)
  {
    bool alnum = true;
    
    // If regexp is true, then only use selected text if text does not
    // contain special regexp characters.
    if (wxExFindReplaceData::Get()->STCFlags() & wxSTC_FIND_REGEXP)
    {
      for (size_t i = 0; i < selection.size() && alnum; i++)
      {
        if (!isalnum(selection[i]))
        {
          alnum = false;
        }
      }
    }

    if (alnum)
    {  
      wxExFindReplaceData::Get()->SetFindString(selection);
    }
  }

  return wxExFindReplaceData::Get()->GetFindString();
}

int wxExSTC::GetLineNumberAtCurrentPos() const
{
  // This method is used by LinkOpen.
  // So, if no line number present return 0, 
  // otherwise link open jumps to last line.
  const auto pos = GetCurrentPos();
  const auto line_no = LineFromPosition(pos);

  // Cannot use GetLine, as that includes EOF, and then the ToLong does not
  // return correct number.
  const wxString text = const_cast< wxExSTC * >( this )->GetTextRange(
    PositionFromLine(line_no), 
    GetLineEndPosition(line_no));

  return wxExGetLineNumber(text);
}

const wxString wxExSTC::GetTextAtCurrentPos() const
{
  const wxString sel = const_cast< wxExSTC * >( this )->GetSelectedText();

  if (!sel.empty())
  {
    if (wxExGetNumberOfLines(sel) > 1)
    {
      // wxPathList cannot handle links over several lines.
      return wxEmptyString;
    }

    return sel;
  }
  else
  {
    const auto pos = GetCurrentPos();
    const auto line_no = LineFromPosition(pos);
    const wxString text = GetLine(line_no);

    // Better first try to find "...", then <...>, as in next example.
    // <A HREF="http://www.scintilla.org">scintilla</A> component.

    // So, first get text between " signs.
    size_t pos_char1 = text.find("\"");
    size_t pos_char2 = text.rfind("\"");

    // If that did not succeed, then get text between < and >.
    if (pos_char1 == wxString::npos || 
        pos_char2 == wxString::npos || 
        pos_char2 <= pos_char1)
    {
      pos_char1 = text.find("<");
      pos_char2 = text.rfind(">");
    }

    // If that did not succeed, then get text between : and : (in .po files).
    if (pos_char1 == wxString::npos || 
        pos_char2 == wxString::npos || 
        pos_char2 <= pos_char1)
    {
      pos_char1 = text.find(": ");
      pos_char2 = text.rfind(":");
    }

    // If that did not succeed, then get text between ' and '.
    if (pos_char1 == wxString::npos ||
        pos_char2 == wxString::npos || 
        pos_char2 <= pos_char1)
    {
      pos_char1 = text.find("'");
      pos_char2 = text.rfind("'");
    }

    // If we did not find anything.
    if (pos_char1 == wxString::npos || 
        pos_char2 == wxString::npos || 
        pos_char2 <= pos_char1)
    {
      return wxEmptyString;
    }

    // Okay, get everything inbetween.
    const wxString match = 
      text.substr(pos_char1 + 1, pos_char2 - pos_char1 - 1);

    // And make sure we skip white space.
    return match.Strip(wxString::both);
  }
}

const wxString wxExSTC::GetWordAtPos(int pos) const
{
  const auto word_start = 
    const_cast< wxExSTC * >( this )->WordStartPosition(pos, true);
  const auto word_end = 
    const_cast< wxExSTC * >( this )->WordEndPosition(pos, true);

  if (word_start == word_end && word_start < GetTextLength())
  {
    const wxString word = 
      const_cast< wxExSTC * >( this )->GetTextRange(word_start, word_start + 1);

    if (!isspace(word[0]))
    {
      return word;
    }
    else
    {
      return wxEmptyString;
    }
  }
  else
  {
    const wxString word = 
      const_cast< wxExSTC * >( this )->GetTextRange(word_start, word_end);

    return word;
  }
}

bool wxExSTC::GotoDialog(const wxString& caption)
{
  wxASSERT(m_GotoLineNumber <= GetLineCount() && m_GotoLineNumber > 0);

  long val;
  if ((val = wxGetNumberFromUser(
    _("Input") + wxString::Format(" 1 - %d:", GetLineCount()),
    wxEmptyString,
    caption,
    m_GotoLineNumber, // initial value
    1,
    GetLineCount())) < 0)
  {
    return false;
  }

  GotoLineAndSelect(val);

  return true;
}

void wxExSTC::GotoLineAndSelect(
  int line_number, 
  const wxString& text)
{
  // line_number and m_GotoLineNumber start with 1 and is allowed to be 
  // equal to number of lines.
  // Internally GotoLine starts with 0, therefore 
  // line_number - 1 is used afterwards.
  wxASSERT(line_number <= GetLineCount() && line_number > 0);

  GotoLine(line_number - 1);
  EnsureVisible(line_number - 1);

  m_GotoLineNumber = line_number;

  const auto start_pos = PositionFromLine(line_number - 1);
  const auto end_pos = GetLineEndPosition(line_number - 1);

  SetTargetStart(start_pos);
  SetTargetEnd(end_pos);

  if (!text.empty())
  {
    SetSearchFlags(wxExFindReplaceData::Get()->STCFlags());

    if (SearchInTarget(text) < 0)
    {
      bool recursive = true;
      wxExFindResult(text, true, recursive);
      return;
    }
  }

  SetSelection(GetTargetStart(), GetTargetEnd());
}

void wxExSTC::GuessType()
{
  if (!(GetFlags() & STC_WIN_HEX))
  {
    // Get a small sample from this file to detect the file mode.
    const auto sample_size = (GetTextLength() > 255 ? 255: GetTextLength());
    const wxString text = GetTextRange(0, sample_size);

    if      (text.Contains("\r\n")) SetEOLMode(wxSTC_EOL_CRLF);
    else if (text.Contains("\n"))   SetEOLMode(wxSTC_EOL_LF);
    else if (text.Contains("\r"))   SetEOLMode(wxSTC_EOL_CR);
    else return; // do nothing
  }

#if wxUSE_STATUSBAR
  UpdateStatusBar("PaneFileType");
#endif
}

void wxExSTC::HexDecCalltip(int pos)
{
  if (CallTipActive())
  {
    CallTipCancel();
  }

  wxString word;

  if (!GetSelectedText().empty())
  {
    word = GetSelectedText();
  }
  else
  {
    word = GetWordAtPos(pos);
  }

  if (word.empty()) return;

  const wxUniChar c = word.GetChar(0);

  if (c < 32 || c > 125)
  {
    const wxString text(wxString::Format("hex: %x dec: %d", c, c));
    CallTipShow(pos, text);
    wxExClipboardAdd(text);
    return;
  }

  long base10_val, base16_val;
  const bool base10_ok = word.ToLong(&base10_val);
  const bool base16_ok = word.ToLong(&base16_val, 16);

  if (base10_ok || base16_ok)
  {
    wxString text;

    if      ( base10_ok && !base16_ok) 
      text = wxString::Format("hex: %lx", base10_val);
    else if (!base10_ok &&  base16_ok) 
      text = wxString::Format("dec: %ld", base16_val);
    else if ( base10_ok &&  base16_ok) 
      text = wxString::Format("hex: %lx dec: %ld", base10_val, base16_val);

    CallTipShow(pos, text);
    wxExClipboardAdd(text);
  }
}

void wxExSTC::Indent(int lines, bool forward)
{
  const auto line = LineFromPosition(GetCurrentPos());

  BeginUndoAction();

  for (auto i = 0; i < lines; i++)
  {
    const auto start = PositionFromLine(line + i);

    if (forward)
    {
      const wxUniChar c = (GetUseTabs() ? '\t': ' ');
      InsertText(start, wxString(c, GetIndent()));
    }
    else
    {
      Remove(start, start + GetIndent());
    }
    
    MarkerAddChange(line + i);
  }

  EndUndoAction();
}

void wxExSTC::Initialize()
{
  m_MacroIsRecording = false;
  
  Bind(
    wxEVT_STC_MODIFIED, 
    &wxExSTC::OnStyledText,
    this,
    wxID_ANY);

#ifdef __WXMSW__
  SetEOLMode(wxSTC_EOL_CRLF);
#else
  SetEOLMode(wxSTC_EOL_LF);
#endif

  // Try to set the background (or foreground) colour.
  // Dit not have any effect.
  /*
  SetBackgroundColour(*wxBLACK);
  ClearBackground();
  SetForegroundColour(*wxBLACK);
  */

  SetBackSpaceUnIndents(true);
  SetMouseDwellTime(1000);
  SetMarginType(m_MarginLineNumber, wxSTC_MARGIN_NUMBER);
  SetMarginType(m_MarginDividerNumber, wxSTC_MARGIN_SYMBOL);
  SetMarginType(m_MarginFoldingNumber, wxSTC_MARGIN_SYMBOL);
  SetMarginMask(m_MarginFoldingNumber, wxSTC_MASK_FOLDERS);
  SetMarginSensitive(m_MarginFoldingNumber, true);

  UsePopUp(false); // we have our own

  const int accels = 13; // take max number of entries
  wxAcceleratorEntry entries[accels];

  int i = 0;

  entries[i++].Set(wxACCEL_CTRL, (int)'Z', wxID_UNDO);
  entries[i++].Set(wxACCEL_CTRL, (int)'Y', wxID_REDO);
  entries[i++].Set(wxACCEL_CTRL, (int)'D', ID_EDIT_HEX_DEC_CALLTIP);
  entries[i++].Set(wxACCEL_NORMAL, WXK_F7, wxID_SORT_ASCENDING);
  entries[i++].Set(wxACCEL_NORMAL, WXK_F8, wxID_SORT_DESCENDING);
  entries[i++].Set(wxACCEL_NORMAL, WXK_F9, ID_EDIT_FOLD_ALL);
  entries[i++].Set(wxACCEL_NORMAL, WXK_F10, ID_EDIT_UNFOLD_ALL);
  entries[i++].Set(wxACCEL_NORMAL, WXK_F11, ID_EDIT_UPPERCASE);
  entries[i++].Set(wxACCEL_NORMAL, WXK_F12, ID_EDIT_LOWERCASE);
  entries[i++].Set(wxACCEL_NORMAL, WXK_DELETE, wxID_DELETE);
  entries[i++].Set(wxACCEL_CTRL, WXK_INSERT, wxID_COPY);
  entries[i++].Set(wxACCEL_SHIFT, WXK_INSERT, wxID_PASTE);
  entries[i++].Set(wxACCEL_SHIFT, WXK_DELETE, wxID_CUT);

  wxAcceleratorTable accel(i, entries);
  SetAcceleratorTable(accel);
  
  SetGlobalStyles();
  
  ConfigGet();
}

bool wxExSTC::IsTargetRE(const wxString& target) const
{
  return 
    target.Contains("\\1") ||
    target.Contains("\\2") ||
    target.Contains("\\3") ||
    target.Contains("\\4") ||
    target.Contains("\\5") ||
    target.Contains("\\6") ||
    target.Contains("\\7") ||
    target.Contains("\\8") ||
    target.Contains("\\9");
}

bool wxExSTC::LinkOpen(
  const wxString& link_with_line,
  int line_number,
  wxString* filename)
{
  // Any line info is already in line_number, so skip here.
  const wxString link = link_with_line.BeforeFirst(':');

  if (link.empty())
  {
    return false;
  }

  wxFileName file(link);
  wxString fullpath;

  if (file.FileExists())
  {
    file.MakeAbsolute();
    fullpath = file.GetFullPath();
  }
  else
  {
    if (file.IsRelative())
    {
      if (file.MakeAbsolute(m_File.GetFileName().GetPath()))
      {
        if (file.FileExists())
        {
          fullpath = file.GetFullPath();
        }
      }
    }

    if (fullpath.empty())
    {
      fullpath = m_PathList.FindAbsoluteValidPath(link);
    }
  }
  
  if (!fullpath.empty())
  {
    if (filename == NULL)
    {
      return Open(
        fullpath, 
        line_number, 
        wxEmptyString, 
        GetFlags() | STC_WIN_FROM_OTHER);
    }
    else
    {
      *filename = wxFileName(fullpath).GetFullName();
    }
  }
  
  return !fullpath.empty();
}

void wxExSTC::MacroPlayback()
{
  wxASSERT(MacroIsRecorded());

  for (
    auto it = m_Macro.begin();
    it != m_Macro.end();
    ++it)
  {
    int msg, wp;
    char c = ' ';
    sscanf((*it).c_str(), "%d %d %c", &msg, &wp, &c);
    char txt[2];
    txt[0] = c;
    txt[1] = '\0';

    SendMsg(msg, wp, (wxIntPtr)txt);
  }

#if wxUSE_STATUSBAR
  wxExFrame::StatusText(_("Macro played back"));
#endif
}

void wxExSTC::MarkerAddChange(int line)
{
  if (
    wxExLexers::Get()->MarkerIsLoaded(m_MarkerChange) &&
    m_File.GetFileName().GetStat().IsOk())
  {
    MarkerAdd(line, m_MarkerChange.GetNo());
  }
}
  
void wxExSTC::MarkerDeleteAllChange()
{
  if (wxExLexers::Get()->MarkerIsLoaded(m_MarkerChange))
  {
    MarkerDeleteAll(m_MarkerChange.GetNo());
  }
}
  
// cannot be const because of MarkerAddChange
void wxExSTC::MarkTargetChange()
{
  if (!wxExLexers::Get()->MarkerIsLoaded(m_MarkerChange))
  {
    return;
  }
  
  const auto line_begin = LineFromPosition(GetTargetStart());
  const auto line_end = LineFromPosition(GetTargetEnd());
    
  for (auto i = line_begin; i <= line_end; i++)
  {
    MarkerAddChange(i);
  }
}
      
void wxExSTC::OnChar(wxKeyEvent& event)
{
  const bool skip = m_vi.OnChar(event);

  if (skip && 
      GetReadOnly() && 
      wxIsalnum(event.GetUnicodeKey()))
  {
#if wxUSE_STATUSBAR
    wxExFrame::StatusText(_("Document is readonly"));
#endif
    return;
  }

  if (skip)
  {
    // Auto complete does not yet combine with vi mode.
    if (!m_vi.GetIsActive())
    {
      CheckAutoComp(event.GetUnicodeKey());
    }

    event.Skip();
  }
}

void wxExSTC::OnCommand(wxCommandEvent& command)
{
  switch (command.GetId())
  {
  case wxID_COPY: Copy(); break;
  case wxID_CUT: Cut(); break;
  case wxID_DELETE: if (!GetReadOnly()) Clear(); break;
  case wxID_JUMP_TO: GotoDialog(); break;
  case wxID_PASTE: Paste(); break;
  case wxID_SELECTALL: SelectAll(); break;
  case wxID_UNDO: Undo(); break;
  case wxID_REDO: Redo(); break;
  case wxID_SAVE: m_File.FileSave(); break;
  case wxID_SORT_ASCENDING: SortSelectionDialog(true); break;
  case wxID_SORT_DESCENDING: SortSelectionDialog(false); break;

  case ID_EDIT_COMPARE:
    {
    wxFileName lastfile;

    if (wxExFindOtherFileName(m_File.GetFileName(), &lastfile))
    {
      wxExCompareFile(m_File.GetFileName(), lastfile);
    }
    }
    break;

  case ID_EDIT_CONTROL_CHAR:
    ControlCharDialog();
  break;
  
  case ID_EDIT_EOL_DOS: EOLModeUpdate(wxSTC_EOL_CRLF); break;
  case ID_EDIT_EOL_UNIX: EOLModeUpdate(wxSTC_EOL_LF); break;
  case ID_EDIT_EOL_MAC: EOLModeUpdate(wxSTC_EOL_CR); break;
  
  case ID_EDIT_FOLD_ALL: FoldAll(); break;
  case ID_EDIT_UNFOLD_ALL:
    for (auto i = 0; i < GetLineCount(); i++) EnsureVisible(i);
  break;
  case ID_EDIT_TOGGLE_FOLD:
  {
    const auto level = GetFoldLevel(GetCurrentLine());
    const auto line_to_fold = (level & wxSTC_FOLDLEVELHEADERFLAG) ?
      GetCurrentLine(): GetFoldParent(GetCurrentLine());
    ToggleFold(line_to_fold);
  }
  break;

  case ID_EDIT_HEX_DEC_CALLTIP:
    HexDecCalltip(GetCurrentPos());
  break;

  case ID_EDIT_LOWERCASE: LowerCase(); break;
  case ID_EDIT_UPPERCASE: UpperCase(); break;
  
  case ID_EDIT_OPEN_BROWSER:
    wxLaunchDefaultBrowser(m_File.GetFileName().GetFullPath());
    break;

  case ID_EDIT_OPEN_LINK:
    {
    const wxString sel = GetSelectedText();
    
    if (!sel.empty())
    {
      LinkOpen(sel, wxExGetLineNumber(sel));
    }
    else
    {
      LinkOpen(GetTextAtCurrentPos(), GetLineNumberAtCurrentPos());
    }
    }
    break;

  case ID_EDIT_READ:
    {
    wxExFileName fn(command.GetString());

    if (fn.IsRelative())
    {
      fn.Normalize(wxPATH_NORM_ALL, m_File.GetFileName().GetPath());
    }

    wxExFile file(fn.GetFullPath());

    if (file.IsOpened())
    {
      const wxCharBuffer& buffer = file.Read();
      SendMsg(
        SCI_ADDTEXT, buffer.length(), (wxIntPtr)(const char *)buffer.data());
    }
    else
    {
      wxExFrame::StatusText(wxString::Format(_("file: %s does not exist"), 
        file.GetFileName().GetFullPath()));
    }
    }
    break;

  default: wxFAIL; break;
  }
}

void wxExSTC::OnFocus(wxFocusEvent& event)
{
  event.Skip();

  wxCommandEvent focusevent(wxEVT_COMMAND_MENU_SELECTED, ID_FOCUS_STC);

  if (event.GetEventType() == wxEVT_SET_FOCUS)
  {
    focusevent.SetEventObject(this);
  }
  else
  {
    focusevent.SetEventObject(NULL);
  }

  wxPostEvent(wxTheApp->GetTopWindow(), focusevent);
}

void wxExSTC::OnIdle(wxIdleEvent& event)
{
  event.Skip();

  if (
    m_File.CheckSync() &&
    // the readonly flags bit of course can differ from file actual readonly mode,
    // therefore add this check
    !(GetFlags() & STC_WIN_READ_ONLY) &&
    m_File.GetFileName().GetStat().IsOk() &&
    m_File.GetFileName().GetStat().IsReadOnly() != GetReadOnly())
  {
    FileReadOnlyAttributeChanged();
  }
}

void wxExSTC::OnKeyDown(wxKeyEvent& event)
{
  if (event.GetModifiers() == wxMOD_ALT)
  {
    return;
  }

  if (!m_vi.GetIsActive() || m_vi.OnKeyDown(event))
  {
    if (event.GetKeyCode() == WXK_RETURN)
    {
      if (!SmartIndentation())
      {
        event.Skip();
      }
    }
    else
    {
      event.Skip();
    }
  }
}

void wxExSTC::OnKeyUp(wxKeyEvent& event)
{
  event.Skip();

  if (!CheckBrace(GetCurrentPos()))
  {
    if (!CheckBrace(GetCurrentPos() - 1))
    {
      if (m_Flags & STC_WIN_HEX)
      {
        if (!CheckBraceHex(GetCurrentPos()))
        {
          CheckBraceHex(GetCurrentPos() - 1);
        }
      }
    }
  }
}

void wxExSTC::OnMouse(wxMouseEvent& event)
{
  if (event.LeftUp())
  {
    PropertiesMessage();

    event.Skip();

    if (!CheckBrace(GetCurrentPos()))
    {
      CheckBrace(GetCurrentPos() - 1);
    }
  }
  else if (event.RightUp())
  {
    if (m_MenuFlags == 0)
    {
      event.Skip();
    }
    else
    {
      int style = 0; // otherwise CAN_PASTE already on
      if (GetReadOnly()) style |= wxExMenu::MENU_IS_READ_ONLY;
      // TODO added SelectionIsRectangle() to fix assert when sel is rectangle,
      // however check GetSelectedText() is done quite often
      if (SelectionIsRectangle() || !GetSelectedText().empty()) 
        style |= wxExMenu::MENU_IS_SELECTED;
      if (GetTextLength() == 0) style |= wxExMenu::MENU_IS_EMPTY;
      if (CanPaste()) style |= wxExMenu::MENU_CAN_PASTE;

      wxExMenu menu(style);
      
      BuildPopupMenu(menu);
      
      if (menu.GetMenuItemCount() > 0)
      {
        PopupMenu(&menu);
      }
    }
  }
  else
  {
    wxFAIL;
  }
}

void wxExSTC::OnMouseCapture(wxMouseCaptureLostEvent& event)
{
}

void wxExSTC::OnStyledText(wxStyledTextEvent& event)
{
  if (event.GetEventType() == wxEVT_STC_MODIFIED)
  {
    event.Skip();

//    MarkerAddChange(event.GetLine()); 
  }
  else if (event.GetEventType() == wxEVT_STC_DWELLEND)
  {
    if (CallTipActive())
    {
      CallTipCancel();
    }
  }
  else if (event.GetEventType() == wxEVT_STC_MACRORECORD)
  {
    wxString msg = wxString::Format("%d %d ", 
      event.GetMessage(), 
      event.GetWParam());

    if (event.GetLParam() != 0)
    {
      char* txt = (char *)(wxIntPtr)event.GetLParam();
      msg += txt;
    }
    else
    {
      msg += "0";
    }

    m_Macro.push_back(msg);
  }
  else if (event.GetEventType() == wxEVT_STC_MARGINCLICK)
  {
    if (event.GetMargin() == m_MarginFoldingNumber)
    {
      const auto line = LineFromPosition(event.GetPosition());
      const auto level = GetFoldLevel(line);

      if ((level & wxSTC_FOLDLEVELHEADERFLAG) > 0)
      {
        ToggleFold(line);
      }
    }
  }
  else if (event.GetEventType() == wxEVT_STC_CHARADDED)
  {
    MarkerAddChange(GetCurrentLine());
  }
  else if (event.GetEventType() == wxEVT_STC_MODIFIED)
  {
    event.Skip();

//    MarkerAddChange(event.GetLine());
  }
  else
  {
    wxFAIL;
  }
}

bool wxExSTC::Open(
  const wxExFileName& filename,
  int line_number,
  const wxString& match,
  long flags)
{
  if (m_File.GetFileName() == filename && line_number > 0)
  {
    GotoLineAndSelect(line_number, match);
    PropertiesMessage();
    return true;
  }

  m_Flags = flags;

  Unbind(
    wxEVT_STC_MODIFIED, 
    &wxExSTC::OnStyledText,
    this,
    wxID_ANY);

  if (m_File.FileLoad(filename))
  {
    SetName(filename.GetFullPath());

    if (line_number > 0)
    {
      GotoLineAndSelect(line_number, match);
    }
    else
    {
      if (line_number == -1)
      {
        DocumentEnd();
      }
    }

    Bind(
      wxEVT_STC_MODIFIED, 
      &wxExSTC::OnStyledText,
      this,
      wxID_ANY);

    return true;
  }
  else
  {
    Bind(
      wxEVT_STC_MODIFIED, 
      &wxExSTC::OnStyledText,
      this,
      wxID_ANY);

    return false;
  }
}

void wxExSTC::Paste()
{
  const auto line = GetCurrentLine();

  wxStyledTextCtrl::Paste();
  
  if (wxExLexers::Get()->MarkerIsLoaded(m_MarkerChange))
  {
    for (auto i = line; i <= GetCurrentLine(); i++)
    {
      MarkerAddChange(i);
    }
  }
}

#if wxUSE_PRINTING_ARCHITECTURE
void wxExSTC::Print(bool prompt)
{
  auto* data = wxExPrinting::Get()->GetHtmlPrinter()->GetPrintData();
  wxExPrinting::Get()->GetPrinter()->GetPrintDialogData().SetPrintData(*data);
  wxExPrinting::Get()->GetPrinter()->Print(this, new wxExPrintout(this), prompt);
}
#endif

#if wxUSE_PRINTING_ARCHITECTURE
void wxExSTC::PrintPreview()
{
  wxPrintPreview* preview = new wxPrintPreview(
    new wxExPrintout(this), 
    new wxExPrintout(this));

  if (!preview->Ok())
  {
    delete preview;
    wxLogError("There was a problem previewing.\nPerhaps your current printer is not set correctly?");
    return;
  }

  wxPreviewFrame* frame = new wxPreviewFrame(
    preview,
    this,
    wxExPrintCaption(GetName()));

  frame->Initialize();
  frame->Show();
}
#endif

void wxExSTC::PropertiesMessage()
{
#if wxUSE_STATUSBAR
  wxExFrame::StatusText(m_File.GetFileName());
  UpdateStatusBar("PaneFileType");
  UpdateStatusBar("PaneLexer");
  UpdateStatusBar("PaneLines");
#endif
}

int wxExSTC::ReplaceAll(
  const wxString& find_text,
  const wxString& replace_text)
{
  const wxString selection = GetSelectedText();
  int selection_from_end = 0;
  const auto selstart = GetSelectionStart();
  const auto selend = GetSelectionEnd();

  // We cannot use wxExGetNumberOfLines here if we have a rectangular selection.
  // So do it the other way.
  if (!selection.empty() &&
       LineFromPosition(selend) > LineFromPosition(selstart))
  {
    TargetFromSelection();
    selection_from_end = GetLength() - GetTargetEnd();
  }
  else
  {
    SetTargetStart(0);
    SetTargetEnd(GetLength());
  }

  SetSearchFlags(wxExFindReplaceData::Get()->STCFlags());
  int nr_replacements = 0;

  BeginUndoAction();

  while (SearchInTarget(find_text) > 0)
  {
    const auto target_start = GetTargetStart();
    int length;
    bool skip_replace = false;

    // Check that the target is within the rectangular selection.
    // If not just continue without replacing.
    if (SelectionIsRectangle() && selection_from_end != 0)
    {
      const auto line = LineFromPosition(target_start);
      const auto start_pos = GetLineSelStartPosition(line);
      const auto end_pos = GetLineSelEndPosition(line);
      length = GetTargetEnd() - target_start;

      if (start_pos == wxSTC_INVALID_POSITION ||
          end_pos == wxSTC_INVALID_POSITION ||
          target_start < start_pos ||
          target_start + length > end_pos)
      {
        skip_replace = true;
      }
    }

    if (!skip_replace)
    {
      MarkTargetChange();
  
      length = (IsTargetRE(replace_text) ?
        ReplaceTargetRE(replace_text):
        ReplaceTarget(replace_text));

      nr_replacements++;
    }

    SetTargetStart(target_start + length);
    SetTargetEnd(GetLength() - selection_from_end);
  }

  EndUndoAction();

#if wxUSE_STATUSBAR
  wxExFrame::StatusText(wxString::Format(_("Replaced: %d occurrences of: %s"),
    nr_replacements, find_text.c_str()));
#endif

  return nr_replacements;
}

bool wxExSTC::ReplaceNext(bool find_next)
{
  return ReplaceNext(
    wxExFindReplaceData::Get()->GetFindString(),
    wxExFindReplaceData::Get()->GetReplaceString(),
    wxExFindReplaceData::Get()->STCFlags(),
    find_next);
}

bool wxExSTC::ReplaceNext(
  const wxString& find_text, 
  const wxString& replace_text,
  int search_flags,
  bool find_next)
{
  if (!GetSelectedText().empty())
  {
    TargetFromSelection();
  }
  else
  {
    SetTargetStart(GetCurrentPos());
    SetTargetEnd(GetLength());
    if (SearchInTarget(find_text) == -1) return false;
  }

  MarkTargetChange();
      
  IsTargetRE(replace_text) ?
    ReplaceTargetRE(replace_text):
    ReplaceTarget(replace_text);

  FindNext(find_text, search_flags, find_next);
  
  return true;
}
  
void wxExSTC::ResetMargins(bool divider_margin)
{
  SetMarginWidth(m_MarginFoldingNumber, 0);
  SetMarginWidth(m_MarginLineNumber, 0);

  if (divider_margin)
  {
    SetMarginWidth(m_MarginDividerNumber, 0);
  }
}

void wxExSTC::SequenceDialog()
{
  static wxString start_previous;

  const wxString start = wxGetTextFromUser(
    _("Input") + ":",
    _("Start Of Sequence"),
    start_previous,
    this);

  if (start.empty()) return;

  start_previous = start;

  static wxString end_previous = start;

  const wxString end = wxGetTextFromUser(
    _("Input") + ":",
    _("End Of Sequence"),
    end_previous,
    this);

  if (end.empty()) return;

  end_previous = end;

  if (start.length() != end.length())
  {
    wxLogMessage(_("Start and end sequence should have same length"));
    return;
  }

  long lines = 1;

  for (int pos = end.length() - 1; pos >= 0; pos--)
  {
    lines *= abs(end[pos] - start[pos]) + 1;
  }

  if (wxMessageBox(wxString::Format(_("Generate %ld lines"), lines) + "?",
    _("Confirm"),
    wxOK | wxCANCEL | wxICON_QUESTION) == wxCANCEL)
  {
    return;
  }

  wxBusyCursor wait;

  wxString sequence = start;

  long actual_line = 0;

  while (sequence != end)
  {
    AddText(sequence + GetEOL());
    actual_line++;

    if (actual_line > lines)
    {
      wxFAIL;
      return;
    }

    if (start < end)
    {
      sequence.Last() = (int)sequence.Last() + 1;
    }
    else
    {
      sequence.Last() = (int)sequence.Last() - 1;
    }

    for (int pos = end.length() - 1; pos > 0; pos--)
    {
      if (start < end)
      {
        if (sequence[pos] > end[pos])
        {
          sequence[pos - 1] = (int)sequence[pos - 1] + 1;
          sequence[pos] = start[pos];
        }
      }
      else
      {
        if (sequence[pos] < end[pos])
        {
          sequence[pos - 1] = (int)sequence[pos - 1] - 1;
          sequence[pos] = start[pos];
        }
      }
    }
  }

  AddText(sequence + GetEOL());
}

void wxExSTC::SetGlobalStyles()
{
  wxExLexers::Get()->GetDefaultStyle().Apply(this);

  StyleClearAll();

  wxExLexers::Get()->ApplyGlobalStyles(this);
  wxExLexers::Get()->ApplyIndicators(this);
}

bool wxExSTC::SetLexer(const wxString& lexer)
{
  if (m_Lexer.ApplyLexer(lexer, this))
  {
    if (GetProperty("fold") == "1")
    {
      SetMarginWidth(m_MarginFoldingNumber, 
        wxConfigBase::Get()->ReadLong(_("Folding"), 16));

      SetFoldFlags(
        wxConfigBase::Get()->ReadLong(_("Fold Flags"),
        wxSTC_FOLDFLAG_LINEBEFORE_CONTRACTED | 
          wxSTC_FOLDFLAG_LINEAFTER_CONTRACTED));
    }
    else
    {
      SetMarginWidth(m_MarginFoldingNumber, 0);
    }
    
    if (GetLineCount() > wxConfigBase::Get()->ReadLong(_("Auto fold"), -1))
    {
      FoldAll();
    }

    wxExFrame::StatusText(m_Lexer.GetScintillaLexer(), "PaneLexer");
    
    return true;
  }
  
  return false;
}

void wxExSTC::SetText(const wxString& value)
{
  ClearDocument();

  // The stc.h equivalents SetText, AddText, AddTextRaw, InsertText, 
  // InsertTextRaw do not add the length.
  // So for text with nulls this is the only way for opening.
  SendMsg(SCI_ADDTEXT, value.length(), (wxIntPtr)(const char *)value.c_str());

  DocumentStart();

  // Do not allow the text specified to be undone.
  EmptyUndoBuffer();
}

bool wxExSTC::SmartIndentation()
{
  // At this moment a newline has been given (but not yet processed).
  const wxString line = GetLine(GetCurrentLine());

  if (line.empty() || line == GetEOL())
  {
    return false;
  }

  // We check for a tab or space at the begin of this line,
  // and copy all these characters to the new line.
  // Using isspace is not okay, as that copies the CR and LF too, these
  // are already copied.
  int i = 0;
  if (line[i] == wxUniChar('\t') || line[i] == wxUniChar(' '))
  {
    InsertText(GetCurrentPos(), GetEOL());
    GotoLine(GetCurrentLine() + 1);

    while (line[i] == wxUniChar('\t') || line[i] == wxUniChar(' '))
    {
      InsertText(GetCurrentPos(), line[i]);
      GotoPos(GetCurrentPos() + 1);
      i++;
    }
    
    return true;
  }
  else
  {
    return false;
  }
}

void wxExSTC::SortSelectionDialog(bool sort_ascending, const wxString& caption)
{
  long val;
  if ((val = wxGetNumberFromUser(_("Input") + ":",
    wxEmptyString,
    caption,
    GetCurrentPos() + 1 - PositionFromLine(GetCurrentLine()),
    1,
    GetLineEndPosition(GetCurrentLine()),
    this)) <= 0)
  {
    return;
  }

  wxBusyCursor wait;

  const auto start_line = LineFromPosition(GetSelectionStart());
  const auto start_pos = PositionFromLine(start_line);
  SetSelection(start_pos, PositionFromLine(LineFromPosition(GetSelectionEnd())));

  // Empty lines are not kept after sorting, as they are used as separator.
  wxStringTokenizer tkz(GetSelectedText(), GetEOL());
  std::multimap<wxString, wxString> mm;
  while (tkz.HasMoreTokens())
  {
    const wxString line = tkz.GetNextToken() + GetEOL();

    // Use an empty key if line is to short.
    wxString key;

    if (val - 1 < (long)line.length())
    {
      key = line.substr(val - 1);
    }

    mm.insert(std::make_pair(key, line));
  }

  // The multimap is already sorted, just iterate to get all lines back.
  wxString text;

  if (sort_ascending)
  {
    for (
      auto it = mm.begin();
      it != mm.end();
      ++it)
    {
      text += it->second;
    }
  }
  else
  {
    for (
      auto it = mm.rbegin();
      it != mm.rend();
      ++it)
    {
      text += it->second;
    }
  }

  ReplaceSelection(text);

  // Set selection back, without removed empty lines.
  SetSelection(start_pos, GetLineEndPosition(start_line + mm.size()));
}

void wxExSTC::StartRecord()
{
  wxASSERT(!m_MacroIsRecording);

  m_MacroIsRecording = true;

  m_Macro.clear();

#if wxUSE_STATUSBAR
  wxExFrame::StatusText(_("Macro recording"));
#endif

  wxStyledTextCtrl::StartRecord();
}

void wxExSTC::StopRecord()
{
  wxASSERT(m_MacroIsRecording);

  m_MacroIsRecording = false;

#if wxUSE_STATUSBAR
  if (!m_Macro.empty())
  {
    wxExFrame::StatusText(_("Macro is recorded"));
  }
#endif

  wxStyledTextCtrl::StopRecord();
}

#if wxUSE_STATUSBAR
// Do not make it const, too many const_casts needed,
// I thought that might cause crash in rect selection, but it didn't.
void wxExSTC::UpdateStatusBar(const wxString& pane)
{
  wxString text;

  if (pane == "PaneLines")
  {
    if (GetCurrentPos() == 0) text = wxString::Format("%d", GetLineCount());
    else
    {
      int start;
      int end;
      GetSelection(&start, &end);

      const auto len  = end - start;
      const auto line = GetCurrentLine() + 1;
      const auto pos = GetCurrentPos() + 1 - PositionFromLine(line - 1);

      if (len == 0) text = wxString::Format("%d,%d", line, pos);
      else
      {
        if (SelectionIsRectangle())
        {
          // The number of chars in the selection must be calculated.
          // TODO: However, next code crashes (wxWidgets 2.9.0).
          // GetSelectedText().length()
          text = wxString::Format("%d,%d", line, pos);
        }
        else
        {
          // There might be NULL's inside selection.
          // So use the GetSelectedTextRaw variant.
          const auto number_of_lines = 
            wxExGetNumberOfLines(GetSelectedTextRaw());
            
          if (number_of_lines <= 1) 
            text = wxString::Format("%d,%d,%d", line, pos, len);
          else
            text = wxString::Format("%d,%d,%d", line, number_of_lines, len);
        }
      }
    }
  }
  else if (pane == "PaneLexer")
  {
    text = m_Lexer.GetScintillaLexer();
  }
  else if (pane == "PaneFileType")
  {
    if (GetFlags() & STC_WIN_HEX)
    {
      text = "HEX";
    }
    else
    {
      switch (GetEOLMode())
      {
      case wxSTC_EOL_CRLF: text = "DOS"; break;
      case wxSTC_EOL_CR: text = "MAC"; break;
      case wxSTC_EOL_LF: text = "UNIX"; break;
      default: text = "UNKNOWN";
      }
    }
  }
  else
  {
    wxFAIL;
  }

  wxExFrame::StatusText(text, pane);
}
#endif
#endif // wxUSE_GUI