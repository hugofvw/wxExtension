/******************************************************************************\
* File:          dir.cpp
* Purpose:       Implementation of class 'wxExDir'
* Author:        Anton van Wezenbeek
* RCS-ID:        $Id$
*
* Copyright (c) 1998-2009 Anton van Wezenbeek
* All rights are reserved. Reproduction in whole or part is prohibited
* without the written consent of the copyright owner.
\******************************************************************************/

#include <wx/extension/dir.h>
#include <wx/extension/util.h>

class wxExDirTraverser: public wxDirTraverser
{
public:
  wxExDirTraverser(wxExDir& dir, size_t& files)
    : m_Dir(dir)
    , m_Files(files)
    {}

  virtual wxDirTraverseResult OnDir(const wxString& dirname)
  {
    if (m_Dir.Cancelled())
    {
      return wxDIR_STOP;
    }

    if (m_Dir.GetFlags() & wxDIR_DIRS)
    {
      m_Files++;
      m_Dir.OnFile(dirname);
    }

    if (wxIsMainThread())
    {
      wxTheApp->Yield();
    }
    else
    {
      wxThread::This()->Yield();
    }

    return wxDIR_CONTINUE;
  }

  virtual wxDirTraverseResult OnFile(const wxString& filename)
  {
    if (m_Dir.Cancelled())
    {
      return wxDIR_STOP;
    }

    wxFileName file(filename);

    if (wxExMatchesOneOf(file, m_Dir.GetFileSpec()))
    {
      m_Files++;
      m_Dir.OnFile(filename);
    }

    if (wxIsMainThread() && wxTheApp != NULL)
    {
      wxTheApp->Yield();
    }
    else
    {
      wxThread::This()->Yield();
    }

    return wxDIR_CONTINUE;
  }

private:
  wxExDir& m_Dir;
  size_t& m_Files;
};

wxExDir::wxExDir(const wxString& fullpath, const wxString& filespec)
  : wxDir(fullpath)
  , m_FileSpec(filespec)
  , m_Flags(wxDIR_DEFAULT)
{
}

wxExDir::~wxExDir()
{
}

size_t wxExDir::FindFiles(int flags)
{
  if (!IsOpened()) return 0;

  size_t files = 0;

  m_Flags = flags;

  wxExDirTraverser traverser(*this, files);

  // Using m_FileSpec here does not work, as it might
  // contain several specs (*.cpp;*.h), wxDir does not handle that.
  Traverse(traverser, wxEmptyString, m_Flags);

  return files;
}
