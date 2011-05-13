/******************************************************************************\
* File:          stc.h
* Purpose:       Declaration of class wxExSTC
* Author:        Anton van Wezenbeek
* RCS-ID:        $Id$
*
* Copyright (c) 1998-2009, Anton van Wezenbeek
* All rights are reserved. Reproduction in whole or part is prohibited
* without the written consent of the copyright owner.
\******************************************************************************/

#ifndef _EXSTC_H
#define _EXSTC_H

#include <vector> 
#include <wx/stc/stc.h>
#include <wx/extension/filename.h>
#include <wx/extension/lexer.h>
#include <wx/extension/menu.h>
#include <wx/extension/stcfile.h>
#include <wx/extension/vi.h>

#if wxUSE_GUI
class wxExConfigDialog;

/// Offers a styled text ctrl with find/replace, printing, popup menu, 
/// macro support, vi support and lexer support (syntax colouring, folding).
class WXDLLIMPEXP_BASE wxExSTC : public wxStyledTextCtrl
{
public:
  /// Menu and tooltip flags (0 is used for no menu).
  enum wxExMenuFlags
  {
    STC_MENU_SIMPLE    = 0x0001, ///< for adding copy/paste etc. menu
    STC_MENU_FIND      = 0x0002, ///< for adding find menu
    STC_MENU_REPLACE   = 0x0004, ///< for adding replace menu
    STC_MENU_OPEN_LINK = 0x0020, ///< for adding link open menu

    STC_MENU_DEFAULT   = 0xFFFF, ///< all
  };

  /// Window flags (0 is used as default).
  enum wxExWindowFlags
  {
    STC_WIN_READ_ONLY   = 0x0001, ///< window is readonly, 
                                  ///<   this mode overrides real mode from disk
    STC_WIN_HEX         = 0x0002, ///< window in hex mode
    STC_WIN_FROM_OTHER  = 0x0020, ///< opened from within another file (e.g. a link)
  };

  /// Config dialog flags (0 gives
  /// a modal dialog with all options).
  enum wxExConfigFlags
  {
    STC_CONFIG_MODELESS   = 0x0001, ///< use as modeless dialog
    STC_CONFIG_WITH_APPLY = 0x0002, ///< add the apply button
    STC_CONFIG_SIMPLE     = 0x0004, ///< only 'simple' options on dialog
  };

  /// Constructor.
  wxExSTC(wxWindow *parent, 
    const wxString& value = wxEmptyString,
    long win_flags = 0,
    const wxString& title = wxEmptyString,
    long menu_flags = STC_MENU_DEFAULT,
    wxWindowID id = wxID_ANY,
    const wxPoint& pos = wxDefaultPosition,
    const wxSize& size = wxDefaultSize, 
    long style = 0);

  /// Constructor, opens the file.
  /// See also Open.
  wxExSTC(wxWindow* parent,
    const wxExFileName& filename,
    int line_number = 0,
    const wxString& match = wxEmptyString,
    long win_flags = 0,
    long menu_flags = STC_MENU_DEFAULT,
    wxWindowID id = wxID_ANY,
    const wxPoint& pos = wxDefaultPosition,
    const wxSize& size = wxDefaultSize,
    long style = 0);

  /// Copy constructor.
  wxExSTC(const wxExSTC& stc);

  /// Adds an ascii table to current document.
  void AddAsciiTable();

  /// Adds base path.
  void AddBasePathToPathList();

  /// Adds a header.
  void AddHeader();

  /// Adds text in hex mode.
  void AddTextHexMode(wxFileOffset start, const wxCharBuffer& buffer);

  /// Appends text, possibly with timestamp, even if the document is readonly.
  /// If caret was at end, it is repositioned at the end.
  void AppendTextForced(const wxString& text, bool withTimestamp = true);

  // Clears the component: all text is cleared and all styles are reset.
  // Invoked by Open and DoFileNew.
  // (Clear is used by scintilla to clear the selection).
  void ClearDocument();

  /// Shows a dialog with options, returns dialog return code.
  /// If used modeless, it uses the dialog id as specified,
  /// so you can use that id in wxExFrame::OnCommandConfigDialog.
  static int ConfigDialog(
    wxWindow* parent,
    const wxString& title = _("Editor Options"),
    long flags = 0,
    wxWindowID id = wxID_ANY);

  /// Sets the configurable parameters to values currently in config.
  void ConfigGet();

  /// Edit the current selected char as a control char, or if nothing selected,
  /// add a new control char.
  void ControlCharDialog(const wxString& caption = _("Enter Control Character"));
  
  /// Cuts text to clipboard.
  void Cut();

  /// Shows a menu with current line type checked, and allows you to change it.
  void FileTypeMenu();

  /// Finds next with settings from find replace data.
  bool FindNext(bool find_next = true);

  /// Finds next.
  bool FindNext(
    /// text to find
    const wxString& text, 
    /// search flags to be used
    /// wxSTC_FIND_WHOLEWORD
    /// wxSTC_FIND_MATCHCASE
    /// wxSTC_FIND_WORDSTART
    /// wxSTC_FIND_REGEXP
    /// wxSTC_FIND_POSIX
    int search_flags = 0,
    /// finds next or previous
    bool find_next = true);
    
  /// Folds.
  void Fold();

  /// Gets EOL string.
  /// If you only want to insert a newline, use NewLine()
  /// (from wxStyledTextCtrl).
  const wxString GetEOL() const;

  /// Gets the file.
  wxExFile& GetFile() {return m_File;};

  /// Gets the filename, as used by the file.
  const wxExFileName& GetFileName() const {return m_File.GetFileName();};

  /// Gets find string, from selected text or from config.
  /// The search flags are taken from frd.
  /// If text is selected, it also sets the find string.
  const wxString GetFindString() const;

  /// Gets current flags.
  long GetFlags() const {return m_Flags;};
  
  /// Gets the lexer.
  const wxExLexer& GetLexer() const {return m_Lexer;};

  /// Gets line number at current position.
  int GetLineNumberAtCurrentPos() const;

  /// Gets selected text.
  const wxString GetSelectedText() const;

  /// Gets text at current position.
  const wxString GetTextAtCurrentPos() const;
  
  /// Gets vi component.
  const wxExVi& GetVi() const {return m_vi;};
  
  /// Gets word at position.
  const wxString GetWordAtPos(int pos) const;

  /// Asks for a line number and goes to the line.
  bool GotoDialog(const wxString& caption = _("Enter Line Number"));

  /// Goes to line and selects the line or the specified text in it.
  void GotoLineAndSelect(
    int line_number, 
    const wxString& text = wxEmptyString);

  /// Guesses the file type using a small sample size from this document, 
  /// and sets EOL mode and updates statusbar if it found eols.
  void GuessType();

  /// Indent range of lines.
  void Indent(int begin, int end, bool forward);
  
  /// Indents lines from current position.
  void Indent(int lines, bool forward = true);
  
  /// Returns true if specified target is a RE, to be used by
  /// ReplaceTargetRE.
  bool IsTargetRE(const wxString& target) const;

  /// A macro has been recorded.
  bool MacroIsRecorded() const {return !m_Macro.empty();};

  /// A marco is now being recorded.
  bool MacroIsRecording() const {return m_MacroIsRecording;};

  /// Plays back the last recorded macro.
  void MacroPlayback();
  
  /// Marks specified line as changed.
  void MarkerAddChange(int line);
  
  /// Deletes all change markers.
  void MarkerDeleteAllChange();
  
  /// Mark target as changed.
  void MarkTargetChange();
  
  /// Opens the file, reads the content into the window, then closes the file
  /// and sets the lexer.
  virtual bool Open(
    /// file to open
    const wxExFileName& filename,
    /// goes to the line if > 0, if -1 goes to end of file
    int line_number = 0,
    /// if not empty selects the text on that line.
    const wxString& match = wxEmptyString,
    /// flags
    long flags = 0);

  /// Paste text from clipboard.
  void Paste();

#if wxUSE_PRINTING_ARCHITECTURE
  /// Prints the document.
  void Print(bool prompt = true);

  /// Shows a print preview.
  void PrintPreview();
#endif

  /// Shows properties on the statusbar.
  virtual void PropertiesMessage();
  
  /// Reloads current document using specified flags.
  void Reload(long flags);

  /// Replaces all text.
  /// It there is a selection, it replaces in the selection, otherwise
  /// in the entire document.
  /// Returns the number of replacements.
  int ReplaceAll(
    const wxString& find_text, 
    const wxString& replace_text);
  
  /// Replaces text and calls find next.
  /// Uses settings from find replace data.
  bool ReplaceNext(bool find_next);

  /// Replaces text and calls find next.
  /// It there is a selection, it replaces in the selection, otherwise
  /// it starts at current position.
  bool ReplaceNext(
    /// text to find
    const wxString& find_text, 
    /// text to replace with
    const wxString& replace_text,
    /// search flags to be used
    /// wxSTC_FIND_WHOLEWORD
    /// wxSTC_FIND_MATCHCASE
    /// wxSTC_FIND_WORDSTART
    /// wxSTC_FIND_REGEXP
    /// wxSTC_FIND_POSIX
    int search_flags = 0,
    /// argument passed on to FindNext
    bool find_next = true);
  
  /// Reset all margins.
  /// Default also resets the divider margin.
  void ResetMargins(bool divider_margin = true);

  /// Asks for a sequence and adds it if not cancelled.
  void SequenceDialog();

  /// Sets the (scintilla) lexer for this document.
  bool SetLexer(const wxString& lexer, bool fold = false);
  
  /// Sets lexer prop name and value,
  /// but does not apply them.
  void SetLexerProperty(const wxString& name, const wxString& value);

  /// Sets the text.
  void SetText(const wxString& value);

  /// Asks for confirmation to sort the selection.
  void SortSelectionDialog(
    bool sort_ascending,
    const wxString& caption = _("Enter Sort Position"));

  /// Starts recording the macro, and empties the previous one.
  /// There is only one shared macro for all objects.
  void StartRecord();

  /// Stops recording the macro.
  void StopRecord();

#if wxUSE_STATUSBAR
  /// Updates the specified statusbar pane with current values.
  void UpdateStatusBar(const wxString& pane);
#endif
protected:
  /// Builds the popup menu.
  virtual void BuildPopupMenu(wxExMenu& menu);
  
  void OnChar(wxKeyEvent& event);
  void OnCommand(wxCommandEvent& event);
  void OnFocus(wxFocusEvent& event);
  void OnIdle(wxIdleEvent& event);
  void OnKeyDown(wxKeyEvent& event);
  void OnKeyUp(wxKeyEvent& event);
  void OnMouse(wxMouseEvent& event);
  void OnStyledText(wxStyledTextEvent& event);
private:
  void CheckAutoComp(const wxUniChar& c);
  bool CheckBrace(int pos);
  bool CheckBraceHex(int pos);
  void EOLModeUpdate(int eol_mode);
  bool FileReadOnlyAttributeChanged(); // sets changed read-only attribute
  void FoldAll();
  void HexDecCalltip(int pos);
  void Initialize();
  bool LinkOpen(
    const wxString& link,
    int line_number = 0, 
    wxString* filename = NULL); // name of found file
  /// After pressing enter, starts new line at same place
  /// as previous line.
  bool SmartIndentation();

  const int m_MarginDividerNumber;
  const int m_MarginFoldingNumber;
  const int m_MarginLineNumber;
  const wxExMarker m_MarkerChange;
  const long m_MenuFlags;

  long m_Flags; // win flags
  long m_GotoLineNumber;
  bool m_MacroIsRecording;

  // We use a separate lexer here as well,
  // though m_File offers one, as you can manually override
  // the lexer.
  wxExLexer m_Lexer;
  wxExSTCFile m_File;
  wxExVi m_vi;

  wxPathList m_PathList;

  // All objects share the following:
  static wxExConfigDialog* m_ConfigDialog;
  static std::vector <wxString> m_Macro;
  static int m_Zoom;

  DECLARE_EVENT_TABLE()
};
#endif // wxUSE_GUI
#endif