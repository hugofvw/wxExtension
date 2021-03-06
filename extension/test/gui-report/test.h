////////////////////////////////////////////////////////////////////////////////
// Name:      test.h
// Purpose:   Declaration of classes for wxExtension report cpp unit testing
// Author:    Anton van Wezenbeek
// Copyright: (c) 2011 Anton van Wezenbeek
////////////////////////////////////////////////////////////////////////////////

#ifndef _FTTESTUNIT_H
#define _FTTESTUNIT_H

#include <TestFixture.h>
#include <TestSuite.h>
#include <wx/extension/app.h>

/// CppUnit test suite.
class wxExTestSuite : public CppUnit::TestSuite
{
public:
  /// Default constructor.
  wxExTestSuite();
};

/// Derive your application from wxExApp.
class wxExTestApp: public wxExApp
{
public:
  /// Constructor.
  wxExTestApp() {}
private:
  /// Override the OnInit.
  virtual bool OnInit();
};

/// CppUnit app test fixture.
/// These classes require either an wxExApp object, or wx to be initialized.
class wxExGuiReportTestFixture : public CppUnit::TestFixture
{
public:
  /// Default constructor.
  wxExGuiReportTestFixture() : TestFixture() {};

  /// Destructor.
 ~wxExGuiReportTestFixture() {};

  /// From TestFixture.
  /// Set up context before running a test.
  virtual void setUp() {};

  /// Clean up after the test run.
  virtual void tearDown() {};

  void testDirWithListView();
  void testFrameWithHistory();
  void testListViewFile();
  void testListViewWithFrame();
  void testProcess();
  void testSTCWithFrame();
  void testTextFileWithListView();
};
#endif

