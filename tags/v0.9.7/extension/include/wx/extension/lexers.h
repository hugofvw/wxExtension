/******************************************************************************\
* File:          lexers.h
* Purpose:       Declaration of wxExLexers class
* Author:        Anton van Wezenbeek
* RCS-ID:        $Id$
*
* Copyright (c) 2009, Anton van Wezenbeek
* All rights are reserved. Reproduction in whole or part is prohibited
* without the written consent of the copyright owner.
\******************************************************************************/

#ifndef _EXLEXERS_H
#define _EXLEXERS_H

#include <map>
#include <set>
#include <vector>
#include <wx/filename.h>
#include <wx/window.h>
#include <wx/xml/xml.h>
#include <wx/extension/indicator.h>
#include <wx/extension/lexer.h>
#include <wx/extension/marker.h>
#include <wx/extension/property.h>
#include <wx/extension/style.h>

class wxStyledTextCtrl;

/// Collection of all lexers.
/// The lexers are read in from lexers.xml, this is done
/// automatically during the first Get call.
class WXDLLIMPEXP_BASE wxExLexers
{
public:
  /// Constructor for lexers from specified filename.
  /// This must be an existing xml file containing all lexers.
  /// It does not Read this file, however if you use the global Get,
  /// it both constructs and reads the lexers.
  wxExLexers(const wxFileName& filename);

  /// Sets global styles (and colours and indicators) 
  /// for current theme for specified component.
  void ApplyGlobalStyles(wxStyledTextCtrl* stc) const;

  /// Sets hex styles for specified component.
  void ApplyHexStyles(wxStyledTextCtrl* stc) const;

  /// Sets indicators for specified component.
  void ApplyIndicators(wxStyledTextCtrl* stc) const;

  /// Applies macro to text:
  /// if text is referring to a macro, text is replaced by the macro value.
  /// Otherwise the same text is returned.
  const wxString ApplyMacro(const wxString& text) const;

  /// Sets markers for specified component.
  void ApplyMarkers(wxStyledTextCtrl* stc) const;

  /// Sets properties for specified component.
  void ApplyProperties(wxStyledTextCtrl* stc) const;

  /// Builds a wildcard string from available lexers using specified filename.
  const wxString BuildWildCards(const wxFileName& filename) const;

  /// Returns the number of lexers.
  size_t Count() const {return m_Lexers.size();};

  /// Finds a lexer specified by a filename.
  const wxExLexer FindByFileName(const wxFileName& filename) const;

  /// Finds a lexer specified by the (scintilla) name.
  const wxExLexer FindByName(const wxString& name) const;

  /// Finds a lexer if text starts with some special tokens.
  const wxExLexer FindByText(const wxString& text) const;

  /// Returns the lexers object.
  /// If this is the first invocation, and createOnDemand is true,
  /// it also invokes Read.
  static wxExLexers* Get(bool createOnDemand = true);

  /// Returns the default style.
  const wxExStyle& GetDefaultStyle() const {return m_DefaultStyle;};

  /// Gets the filename.
  const wxFileName& GetFileName() const {return m_FileName;};

  /// Gets the macros.
  const std::map<wxString, wxString>& GetMacros() const {return m_Macros;};

  /// Returns the current theme, as present in the config.
  /// It checks whether the config theme is really
  /// present as a theme, if not, the empty theme is returned.
  const wxString GetTheme() const;
  
  /// Returns whether the current theme is not the empty theme.
  bool GetThemeOk() const {return GetTheme() != m_NoTheme;};
  
  /// Gets the theme macros for the current theme.
  const std::map<wxString, wxString>& GetThemeMacros() const;

  /// Returns true if specified indicator is available.
  bool IndicatorIsLoaded(const wxExIndicator& indic) const;

  /// Returns true if specified marker is available.
  bool MarkerIsLoaded(const wxExMarker& marker) const;

  /// Parses properties node.
  const std::vector<wxExProperty> ParseNodeProperties(
    const wxXmlNode* node) const;

  /// Reads all lexers (first clears them) from file.
  /// Returns true if the file could be read and loaded as valid xml file.
  bool Read();

  /// Sets the object as the current one, returns the pointer 
  /// to the previous current object 
  /// (both the parameter and returned value may be NULL). 
  static wxExLexers* Set(wxExLexers* lexers);

  /// Shows a dialog with all lexers, allowing you to choose one.
  /// Returns true and fills the lexer if you selected one.
  bool ShowDialog(
    /// parent
    wxWindow* parent,
    /// If you specify an existing lexer, it is selected
    /// in the list. If you press OK, the lexer is 
    /// set to the selected lexer.
    wxString& lexer,
    /// caption
    const wxString& caption = _("Enter Lexer")) const;
    
  /// Shows a dialog with all themes, allowing you to choose one.
  /// Returns true and sets current theme if you select one.
  bool ShowThemeDialog(
    /// parent
    wxWindow* parent,
    /// caption
    const wxString& caption = _("Enter Theme"));
private:
  const wxString GetLexerExtensions() const;
  void Init();
  void ParseNodeGlobal(const wxXmlNode* node);
  void ParseNodeMacro(const wxXmlNode* node);
  void ParseNodeTheme(const wxXmlNode* node);
  void ParseNodeThemes(const wxXmlNode* node);

  std::map<wxString, wxExLexer> m_Lexers;
  std::map<wxString, wxString> m_Macros;
  std::map<wxString, wxString> m_TempColours;
  std::map<wxString, wxString> m_TempMacros;
  std::map<wxString, std::map<wxString, wxString> > m_ThemeColours;
  std::map<wxString, std::map<wxString, wxString> > m_ThemeMacros;

  std::set<wxExIndicator> m_Indicators;
  std::set<wxExMarker> m_Markers;

  std::vector<wxExProperty> m_GlobalProperties;
  std::vector<wxExStyle> m_Styles;
  std::vector<wxExStyle> m_StylesHex;

  wxExStyle m_DefaultStyle;

  const wxFileName m_FileName;
  const wxString m_NoTheme;

  static wxExLexers* m_Self;
};
#endif