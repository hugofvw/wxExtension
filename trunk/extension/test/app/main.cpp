/******************************************************************************\
* File:          main.cpp
* Purpose:       main for wxextension cpp unit testing
* Author:        Anton van Wezenbeek
* RCS-ID:        $Id$
* Created:       za 17 jan 2009 11:51:20 CET
*
* Copyright (c) 2009 Anton van Wezenbeek
* All rights are reserved. Reproduction in whole or part is prohibited
* without the written consent of the copyright owner.
\******************************************************************************/

#include <ui/text/TestRunner.h>
#include <cppunit/TestRunner.h>
#include "test.h"

IMPLEMENT_APP(wxExTestApp)

bool wxExTestApp::OnInit()
{
  SetAppName("wxExTestApp");

  wxExApp::OnInit();

  wxExFrame *frame = new wxExFrame(NULL, wxID_ANY, "wxExTestApp");
  frame->Show(true);

  SetTopWindow(frame);

  CppUnit::TextUi::TestRunner runner;

  wxExTestSuite* suite = new wxExTestSuite;

  runner.addTest(suite);
  runner.run();

  return true;
}
