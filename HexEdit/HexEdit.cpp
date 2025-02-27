// HexEdit.cpp : Defines the class behaviors for the application.
//
// Copyright (c) 2015 by Andrew W. Phillips
//
// This file is distributed under the MIT license, which basically says
// you can do what you want with it and I take no responsibility for bugs.
// See http://www.opensource.org/licenses/mit-license.php for full details.
//

#include "stdafx.h"
//#include <vld.h>                // For visual leak detector
#include "afxwinappex.h"

#include <locale.h>

#include <afxadv.h>     // for CRecentFileList
#include <io.h>         // for _access()

//#include <bcghelpids.h>     // For help on customize dlg

// #include <afxhtml.h>    // for CHtmlView

#include <MAPI.h>       // for MAPI constants

#include <shlwapi.h>    // for SHDeleteKey

#define COMPILE_MULTIMON_STUBS 1   // Had to remove this to link with BCG static lib
#include <MultiMon.h>   // for multiple monitor support
#undef COMPILE_MULTIMON_STUBS

#include "HexEdit.h"

#include <HtmlHelp.h>
#include <imagehlp.h>           // For ::MakeSureDirectoryPathExists()

#include "HexFileList.h"
#include "RecentFileDlg.h"
#include "BookmarkDlg.h"
#include "Bookmark.h"
#include "boyer.h"
#include "SystemSound.h"

#include "MainFrm.h"
#include "ChildFrm.h"
#include "HexEditDoc.h"
#include "HexEditView.h"
#include "DataFormatView.h"
#include "AerialView.h"
#include "CompareView.h"
#include "PrevwView.h"
#include "TabView.h"
#include "UserTool.h"   // For CHexEditUserTool
#include "Dialog.h"
#include "OpenSpecialDlg.h"
#include "Register.h"   // For About dialog
#include "Algorithm.h"  // For encruption algorithm selection
#include "CompressDlg.h" // For compression settings dialog
#include "Password.h"   // For encryption password dialog
#include "Splasher.h"       // For splash window

#include <FreeImage.h>

// The following is not in a public header
extern BOOL AFXAPI AfxFullPath(LPTSTR lpszPathOut, LPCTSTR lpszFileIn);

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern DWORD hid_last_file_dialog = HIDD_FILE_OPEN;
const char *CHexEditApp::bin_format_name  = "BinaryData";               // for copy2cb_binary
const char *CHexEditApp::temp_format_name = "HexEditLargeDataTempFile"; // for copy2cb_file
const char *CHexEditApp::flag_format_name = "HexEditFlagTextIsHex";     // for copy2cb_flag_text_is_hextext

/////////////////////////////////////////////////////////////////////////////
// CHexEditDocManager

class CHexEditDocManager : public CDocManager
{
public:
	// We have to do it this way as CWinAppEx::DoPromptFileName is not virtual for some reason
	virtual BOOL DoPromptFileName(CString& fileName, UINT nIDSTitle,
			DWORD lFlags, BOOL bOpenFileDialog, CDocTemplate* pTemplate);
};

BOOL CHexEditDocManager::DoPromptFileName(CString& fileName, UINT nIDSTitle, DWORD lFlags, BOOL bOpenFileDialog, CDocTemplate* pTemplate)
{
	ASSERT(!bOpenFileDialog);  // we should only be calling this for save
	if (bOpenFileDialog)
		return CDocManager::DoPromptFileName(fileName, nIDSTitle, lFlags, bOpenFileDialog, pTemplate);

	(void)pTemplate;  // This is passed to get associated filters but in our case we don't have any (and they're set by the user via GetCurrentFilters())

	//hid_last_file_dialog = HIDD_FILE_SAVE;
	CHexFileDialog dlgFile("FileSaveDlg", HIDD_FILE_SAVE, FALSE, NULL, fileName, lFlags | OFN_SHOWHELP | OFN_ENABLESIZING,
	                       theApp.GetCurrentFilters(), "Save", AfxGetMainWnd());

	CString title;
	ENSURE(title.LoadString(nIDSTitle));
	dlgFile.m_ofn.lpstrTitle = title;

	// Change the initial directory
	CHexEditView * pv, * pvfirst;
	CString strDir;      // folder name passed to dialog [this must not be destroyed until after dlgFile.DoModal()]
	switch (theApp.save_locn_)
	{
	case FL_DOC:
		// Get path of most recently viewed window backed by a file (ie excluding new files not yet saved to disk)
		pvfirst = GetView();
		for (pv = pvfirst ; pv != NULL; )
		{
			if (pv->GetDocument() != NULL && pv->GetDocument()->pfile1_ != NULL)
			{
				// Get the path from the filename of the active file
				CString filename = pv->GetDocument()->pfile1_->GetFilePath();
				int path_len;                   // Length of path (full name without filename)
				path_len = filename.ReverseFind('\\');
				if (path_len == -1) path_len = filename.ReverseFind('/');
				if (path_len == -1) path_len = filename.ReverseFind(':');
				if (path_len == -1)
					path_len = 0;
				else
					++path_len;
				strDir = filename.Left(path_len);
				break;
			}

			// Get next most recent view and check if we have wrapped around to start
			if ((pv = pv->PrevView()) == pvfirst)
				pv = NULL;
		}
		break;
	case FL_LAST:
		strDir = theApp.last_save_folder_;
		break;
	case FL_BOTH:
		strDir = theApp.last_both_folder_;
		break;
	default:
		ASSERT(0);
		// fall through
	case FL_SPECIFIED:
		strDir = theApp.save_folder_;
		break;
	}
	// If still empty default to the specified folder
	if (strDir.IsEmpty())
		strDir = theApp.save_folder_;
	dlgFile.m_ofn.lpstrInitialDir = strDir;

	dlgFile.m_ofn.lpstrFile = fileName.GetBuffer(_MAX_PATH + 1);
	dlgFile.m_ofn.nMaxFile = _MAX_PATH;
	INT_PTR nResult = dlgFile.DoModal();
	fileName.ReleaseBuffer();

	theApp.last_open_folder_ = theApp.last_both_folder_ = fileName.Left(dlgFile.m_ofn.nFileOffset);

	return nResult == IDOK;
}

/////////////////////////////////////////////////////////////////////////////
// CHexEditApp
const char * CHexEditApp::HexEditClassName = "HexEditMDIFrame";
const char * CHexEditApp::RegHelper = "RegHelper.exe";           // helper for things that require admin privileges

// The following are used to enable the "Open with HexEdit" shell shortcut menu option.
const char *CHexEditApp::HexEditSubKey = "*\\shell\\HexEdit";
const char *CHexEditApp::HexEditSubSubKey = "*\\shell\\HexEdit\\command";

// The following is used to enable "Open With" file extension associations (so that files can be on Win7 task list).
const char * CHexEditApp::ProgID = "HexEdit.file";

BEGIN_MESSAGE_MAP(CHexEditApp, CWinAppEx)
		ON_COMMAND(CG_IDS_TIPOFTHEDAY, ShowTipOfTheDay)
		//{{AFX_MSG_MAP(CHexEditApp)
		ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
		ON_COMMAND(ID_OPTIONS, OnOptions)
		ON_COMMAND(ID_OPTIONS2, OnOptions2)
		ON_COMMAND(ID_OPTIONS_CODEPAGE, OnOptionsCodePage)
		ON_COMMAND(ID_RECORD, OnMacroRecord)
		ON_COMMAND(ID_PLAY, OnMacroPlay)
		ON_UPDATE_COMMAND_UI(ID_PLAY, OnUpdateMacroPlay)
		ON_UPDATE_COMMAND_UI(ID_RECORD, OnUpdateMacroRecord)
		ON_COMMAND(ID_PROPERTIES, OnProperties)
		ON_COMMAND(ID_PROPERTIES_BMP, OnPropertiesBitmap)
		ON_COMMAND(ID_MULTI_PLAY, OnMultiPlay)
		ON_COMMAND(ID_HELP_REPORT_ISSUE, OnHelpReportIssue)
		ON_UPDATE_COMMAND_UI(ID_HELP_REPORT_ISSUE, OnUpdateHelpReportIssue)
		ON_COMMAND(ID_HELP_WEB, OnHelpWeb)
		ON_UPDATE_COMMAND_UI(ID_HELP_WEB, OnUpdateHelpWeb)
		ON_COMMAND(ID_WEB_PAGE, OnWebPage)
		ON_COMMAND(ID_ENCRYPT_ALG, OnEncryptAlg)
		ON_COMMAND(ID_ENCRYPT_CLEAR, OnEncryptClear)
		ON_UPDATE_COMMAND_UI(ID_ENCRYPT_CLEAR, OnUpdateEncryptClear)
		ON_COMMAND(ID_ENCRYPT_PASSWORD, OnEncryptPassword)
		ON_COMMAND(ID_MACRO_MESSAGE, OnMacroMessage)
		ON_UPDATE_COMMAND_UI(ID_MACRO_MESSAGE, OnUpdateMacroMessage)
		ON_UPDATE_COMMAND_UI(ID_MULTI_PLAY, OnUpdateMultiPlay)
		ON_COMMAND(ID_RECENT_FILES, OnRecentFiles)
		ON_COMMAND(ID_BOOKMARKS_EDIT, OnBookmarksEdit)
		ON_COMMAND(ID_TAB_ICONS, OnTabIcons)
		ON_COMMAND(ID_TABS_AT_BOTTOM, OnTabsAtBottom)
		ON_UPDATE_COMMAND_UI(ID_TAB_ICONS, OnUpdateTabIcons)
		ON_UPDATE_COMMAND_UI(ID_TABS_AT_BOTTOM, OnUpdateTabsAtBottom)
	ON_COMMAND(ID_FILE_OPEN_SPECIAL, OnFileOpenSpecial)
	ON_UPDATE_COMMAND_UI(ID_FILE_OPEN_SPECIAL, OnUpdateFileOpenSpecial)
	//}}AFX_MSG_MAP
		ON_COMMAND(ID_ZLIB_SETTINGS, OnCompressionSettings)
		ON_COMMAND(ID_HELP_FORUM, OnHelpWebForum)
		ON_COMMAND(ID_HELP_HOMEPAGE, OnHelpWebHome)
		ON_UPDATE_COMMAND_UI(ID_HELP_FORUM, OnUpdateHelpWeb)
		ON_UPDATE_COMMAND_UI(ID_HELP_HOMEPAGE, OnUpdateHelpWeb)

		// Repair commands
		ON_COMMAND(ID_REPAIR_COPYUSERFILES, OnRepairFiles)
		ON_COMMAND(ID_REPAIR_DIALOGBARS, OnRepairDialogbars)
		ON_COMMAND(ID_REPAIR_CUST, OnRepairCust)
		ON_COMMAND(ID_REPAIR_SETTINGS, OnRepairSettings)
		ON_COMMAND(ID_REPAIR_ALL, OnRepairAll)
//        ON_COMMAND(ID_REPAIR_CHECK, OnRepairCheck)

		ON_COMMAND(ID_FILE_SAVE_ALL, OnFileSaveAll)
		ON_COMMAND(ID_FILE_CLOSE_ALL, OnFileCloseAll)
		ON_COMMAND(ID_FILE_CLOSE_OTHERS, OnFileCloseOthers)

		// Standard file based document commands
		ON_COMMAND(ID_FILE_NEW, OnFileNew)
		ON_COMMAND(ID_FILE_OPEN, OnFileOpen)
		ON_COMMAND_EX_RANGE(ID_FILE_MRU_FILE1, ID_FILE_MRU_FILE16, OnOpenRecentFile)
		ON_COMMAND(ID_APP_EXIT, OnAppExit)
		// Standard print setup command
		ON_COMMAND(ID_FILE_PRINT_SETUP, OnFilePrintSetup)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CHexEditApp construction

#ifdef _DEBUG
// Memory allocation hook function used for debugging memory allocation exceptions
#if _MSC_VER <= 1100
	static int alloc_hook( int allocType, void *userData, size_t size, int blockType, 
		long requestNumber, const char *filename, int lineNumber)
#else
	// For some reason this declaration has changed slightly (filename is now uchar)
	static int alloc_hook( int allocType, void *userData, size_t size, int blockType, 
		long requestNumber, const unsigned char *filename, int lineNumber)
#endif
{
	BOOL retval = TRUE;         // TRUE = proceed normally, FALSE = alloc error
	// Change retval to 0 (in debugger) before returning to test error handling
	switch (allocType)
	{
	case _HOOK_ALLOC:           // malloc/calloc <= C++ new
		return retval;
	case _HOOK_REALLOC:         // realloc
		return retval;
	case _HOOK_FREE:            // free <= C++ delete & delete[]
		return retval;
	}
	ASSERT(0);
	return TRUE;
}
#endif

CHexEditApp::CHexEditApp() : default_scheme_(""),
							 default_ascii_scheme_(ASCII_NAME), default_ansi_scheme_(ANSI_NAME),
							 default_oem_scheme_(OEM_NAME), default_ebcdic_scheme_(EBCDIC_NAME),
							 default_unicode_scheme_(UNICODE_NAME), default_codepage_scheme_(CODEPAGE_NAME),
							 default_multi_scheme_(MULTI_NAME)
{
#ifdef FILE_PREVIEW
	cleanup_thread_ = NULL;
#endif

	// Add a memory allocation hook for debugging purposes
	// (Does nothing in release version.)
	_CrtSetAllocHook(&alloc_hook);
	recording_ = FALSE;
	macro_version_ = INTERNAL_VERSION;

	set_options_timer.reset();
	highlight_ = FALSE;
	playing_ = 0;
	pv_ = pview_ = NULL;
	refresh_off_ = false;
	open_plf_ = open_oem_plf_ = open_mb_plf_ = NULL;
	last_cb_size_ = last_cb_seq_ = 0;
#ifndef NDEBUG
	// Make default capacity for mac_ vector small to force reallocation sooner.
	// This increases likelihood of catching bugs related to reallocation.
	mac_.reserve(2);
#else
	// Pre-allocate room for 128 elts for initial speed
	mac_.reserve(128);
#endif
	open_disp_state_ = -1;

	// Set up the default colour scheme
	default_scheme_.AddRange("Other", -1, "0:255");
	default_scheme_.mark_col_ = -1;
	default_scheme_.hex_addr_col_ = -1;
	default_scheme_.dec_addr_col_ = -1;

	default_ascii_scheme_.AddRange("ASCII text", -1, "32:126");
	default_ascii_scheme_.AddRange("Special (TAB,LF,CR)", RGB(0,128,0), "9,10,13");
	default_ascii_scheme_.AddRange("Other", RGB(255,0,0), "0:255");
	default_ascii_scheme_.bg_col_ = RGB(255, 253, 244);
	default_ascii_scheme_.addr_bg_col_ = RGB(224, 240, 224);

	default_ansi_scheme_.AddRange("ASCII text", -1, "32:126");
	default_ansi_scheme_.AddRange("Special (TAB,LF,CR)", RGB(0,128,0), "9,10,13");
	default_ansi_scheme_.AddRange("ANSI text", RGB(0,0,128), "130:140,145:156,159:255");
	default_ansi_scheme_.AddRange("Other", RGB(255,0,0), "0:255");

	default_oem_scheme_.AddRange("ASCII text", -1, "32:126");
	default_oem_scheme_.AddRange("Other", RGB(255,0,0), "0:255");
	default_oem_scheme_.bg_col_ = RGB(255, 248, 255);
	default_oem_scheme_.addr_bg_col_ = RGB(240, 224, 240);

	default_ebcdic_scheme_.AddRange("EBCDIC text", -1, "64,74:80,90:97,106:111,121:127,"
									"129:137,145:153,161:169,192:201,208:217,224,226:233,240:249");
	default_ebcdic_scheme_.AddRange("Control", RGB(0,0,128), "0:7,10:34"
									"36:39,42:43,45:47,50,52:55,59:61,63");
	default_ebcdic_scheme_.AddRange("Unassigned", RGB(255,0,0), "0:255");
	default_ebcdic_scheme_.bg_col_ = RGB(240, 248, 255);
	default_ebcdic_scheme_.addr_bg_col_ = RGB(192, 224, 240);

	default_unicode_scheme_.bg_col_ = RGB(255, 255, 240);
	default_unicode_scheme_.addr_bg_col_ = RGB(224, 255, 255);
	default_unicode_scheme_.AddRange("All", RGB(128,128,0), "0:255");

	default_codepage_scheme_.bg_col_ = RGB(240, 255, 255);
	default_codepage_scheme_.addr_bg_col_ = RGB(255, 240, 216);
	default_codepage_scheme_.AddRange("All", RGB(0,96,96), "0:255");

	CString strRange;
	for (int ii = 0; ii < 51; ++ii)  // Split into 51 ranges of 5 -> 255 colours
	{
		// Make 5 shades all of this same colour (hue)
		// We generate 51 hues in the full range - ie 1 to 98 (skipping a few).
		// Note that the get_rgb() has a bug (hue 99 == hue 0 == red) so we stop at 98
		int hue = (ii * 99) / 51 + 1;

		// Get byte number for this colour
		strRange.Format("%d", ii*5 + 1);
		default_multi_scheme_.AddRange("Byte"+strRange, get_rgb(hue, 50, 100), strRange);

		strRange.Format("%d", ii*5 + 2);
		default_multi_scheme_.AddRange("Byte"+strRange, get_rgb(hue, 55, 60), strRange);  // less saturated
		strRange.Format("%d", ii*5 + 3);
		default_multi_scheme_.AddRange("Byte"+strRange, get_rgb(hue, 40, 100), strRange); // less bright
		strRange.Format("%d", ii*5 + 4);
		default_multi_scheme_.AddRange("Byte"+strRange, get_rgb(hue, 65, 100), strRange); // more bright
		strRange.Format("%d", ii*5 + 5);
		default_multi_scheme_.AddRange("Byte"+strRange, get_rgb(hue, 45, 60), strRange);  // less bright and less saturated
	}
	default_multi_scheme_.AddRange("All", RGB(255, 255, 255), "0:255");  // 00 (and any of the above later deleted) are white

	pboyer_ = NULL;

	algorithm_ = 0;   // Default to built-in encryption

	m_pbookmark_list = NULL;
	open_current_readonly_ = -1;
	open_current_shared_ = -1;

	delete_reg_settings_ = FALSE;
	delete_all_settings_ = FALSE;

	// MFC 7.1 has special HTML help mode (but this stuffed our use of HTML help)
	SetHelpMode(afxHTMLHelp);

	no_ask_insert_ = false;
}

CHexEditApp::~CHexEditApp()
{
	if (open_plf_ != NULL)
		delete open_plf_;

	if (open_oem_plf_ != NULL)
		delete open_oem_plf_;

	if (open_mb_plf_ != NULL)
		delete open_mb_plf_;
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CHexEditApp object

CHexEditApp theApp;

UINT CHexEditApp::wm_hexedit = ::RegisterWindowMessage("HexEditOpenMessage");


static void freeImageOutput(FREE_IMAGE_FORMAT fif, const char* msg)
{
	TRACE("### [FreeImage] [Format=%d] %s\r\n", (int)fif, msg);
}

/////////////////////////////////////////////////////////////////////////////
// CHexEditApp initialization


BOOL CHexEditApp::InitInstance()
{
	FreeImage_SetOutputMessage(freeImageOutput);

#if _MFC_VER >= 0x0A00
		CString appid;
		appid.LoadStringA(AFX_IDS_APP_ID);
		SetAppID(appid);
#endif

		// Note: if this is changed you also need to change the registry string
		// at the end of ExitInstance (see delete_reg_settings_).
		SetRegistryKey("ECSoftware");           // Required before registry use (and prevents use of .INI file)

		//Bring up the splash screen in a secondary UI thread
		CSplashThread * pSplashThread = NULL;

		// Check reg setting directly rather than use splash_ so we can display the splash screen quickly (before LoadOptions() is called)
		if (GetProfileInt("Options", "Splash", 1) == 1)
		{
			CString sFileName = ::GetExePath() + FILENAME_SPLASH;
			if (_access(sFileName, 0) != -1)
				pSplashThread = (CSplashThread*) AfxBeginThread(RUNTIME_CLASS(CSplashThread), THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);

			if (pSplashThread != NULL)
			{
				ASSERT(pSplashThread->IsKindOf(RUNTIME_CLASS(CSplashThread)));
				pSplashThread->SetBitmapToUse(sFileName);
				pSplashThread->ResumeThread();  //Resume the thread now that we have set it up
			}
		}

		if (!AfxOleInit())              // For BCG and COM (calls CoInitialize())
		{
			AfxMessageBox("OLE initialization failed.  Make sure that the OLE libraries are the correct version.");
			return FALSE;
		}

		// InitCommonControlsEx() is required on Windows XP if an application
		// manifest specifies use of ComCtl32.dll version 6 or later to enable
		// visual styles.  Otherwise, any window creation will fail.
		INITCOMMONCONTROLSEX InitCtrls;
		InitCtrls.dwSize = sizeof(InitCtrls);
		// Set this to include all the common control classes you want to use
		// in your application.
		InitCtrls.dwICC = ICC_WIN95_CLASSES;
		InitCommonControlsEx(&InitCtrls);

		CWinAppEx::InitInstance();

		// Check for /clean on command line Parse command line (before use of 
		// CCommandLineParser - see later) before callling LoadOptions() so
		// that we can force a clean of registry settings before code is run
		// that may cause a crash if something is really wrong with reg settings.
		if (__argc > 1 && (__argv[1][0] == '/' || __argv[1][0] == '-') && _strnicmp(__argv[1]+1, "clean", 5) == 0)
		{
			// /cleanup found on cmd line so delete registry settings and exit
			theApp.delete_reg_settings_ = TRUE;  // this forces ExitInstance to delete all reg. settings
			theApp.ExitInstance();
			exit(0);
		}

		// Override the document manager so we can make save dialog resizeable
		if (m_pDocManager != NULL) delete m_pDocManager;
		m_pDocManager = new CHexEditDocManager;

		LoadOptions();
		InitVersionInfo();

		// CCommandLineParser replaces app's CommandLineInfo class.
		// This uses ParseParam() method (via app's ParseCommandLine() method)
		// to get all file names on the command line and open them (or tell
		// already running copt of HexEdit to open them if "one_only_" is true).
		CCommandLineParser cmdInfo;

		// Work out if there is a previous instance running
		hwnd_1st_ = ::FindWindow(HexEditClassName, NULL);
		if (hwnd_1st_ != (HWND)0 && one_only_)
		{
#ifdef _DEBUG
			AfxMessageBox("HexEdit ALREADY RUNNING!");
#endif
			// Make sure it's on top and not minimised before opening files in it
			::BringWindowToTop(hwnd_1st_);
			WINDOWPLACEMENT wp;
			wp.length = sizeof(wp);
			if (::GetWindowPlacement(hwnd_1st_, &wp) &&
				(wp.showCmd == SW_MINIMIZE || wp.showCmd == SW_SHOWMINIMIZED))
			{
				::ShowWindow(hwnd_1st_, SW_RESTORE);
			}

			// Now use command line parser (CCommandLineParser::ParseParam) to open
			// any files specified on the command line in running instance
			CCommandLineParser cmdInfo;
			ParseCommandLine(cmdInfo);

			// Terminate this instance
			return FALSE;
		}

		InitWorkspace();

		// Register the application's document templates.  Document templates
		//  serve as the connection between documents, frame windows and views.

//        CMultiDocTemplate* pDocTemplate;
		// Made pDocTemplate a member variable so I can use it to get all documents of the app.
		// A better way may have been to get the CDocTemplate from m_pDocManger->m_templateList?
		m_pDocTemplate = new CMultiDocTemplate(
				IDR_HEXEDTYPE,
				RUNTIME_CLASS(CHexEditDoc),
				RUNTIME_CLASS(CChildFrame), // custom MDI child frame
				RUNTIME_CLASS(CHexEditView));
		AddDocTemplate(m_pDocTemplate);

		// We must do this before we create the mainframe so that we have bookmarks for the bookmarks dlg
		m_pbookmark_list = new CBookmarkList(FILENAME_BOOKMARKS);
		m_pbookmark_list->ReadList();

		// create main MDI Frame window.
		// NOTE: This triggers a lot of other initialization (see CMainFrame::OnCreate)
		CMainFrame* pMainFrame = new CMainFrame;
		m_pMainWnd = pMainFrame;  // Need this before LoadFrame as calc constructor get main window
		if (!pMainFrame->LoadFrame(IDR_MAINFRAME))
		{
				return FALSE;
		}
		EnableLoadWindowPlacement(FALSE);
		m_pMainWnd->DragAcceptFiles();

//        CHexEditFontCombo::SaveCharSet();

		// NOTE: the name "RecentFiles" is also used at end of ExitInstance
		m_pRecentFileList = new CHexFileList(0, FILENAME_RECENTFILES, recent_files_);
		m_pRecentFileList->ReadList();

		// NOTE: Don't try to open files (eg command line) before this point

		GetFileList()->SetupJumpList();  // set up Win7 task bar

		// This used to be after the command line parsing but was moved here so that
		// when files are opened the size of the main window is known so that they
		// are opened in sensible sizes and positions.
		pMainFrame->ShowWindow(m_nCmdShow);
		m_pMainWnd->SetFocus();
//          pMainFrame->ShowWindow(SW_SHOWMAXIMIZED);

		// Open any files on the command line
		ParseCommandLine(cmdInfo);

		// If ParseCommandLine found a shell operation to perform then do it.
		// If the command is FileOpen then the file has alreay been opened - so nothing needed.
		// If the command is FileNothing or FileNew then do nothing.
		if (cmdInfo.m_nShellCommand != CCommandLineInfo::FileOpen &&
			cmdInfo.m_nShellCommand != CCommandLineInfo::FileNothing &&
			cmdInfo.m_nShellCommand != CCommandLineInfo::FileNew &&
			!ProcessShellCommand(cmdInfo))
		{
			return FALSE;
		}

		//pMainFrame->FixPanes();  // workaround for BCG bug
#ifdef _DEBUG
		// This avoids all sorts of confusion when testing/debugging
		// due to toolbars being restored to a previous state.
		// (Another way would be to just remove all registry settings.)
		CMFCToolBar::ResetAll();
#endif

		// It's about time to hide the splash window
		if (pSplashThread != NULL)
			pSplashThread->HideSplash();

		pMainFrame->UpdateWindow();

		// I moved this here (from LoadOptions) as the window sometimes comes up behind and there is
		// no way to tell that it is even there (since there was no main window-> nothing on task bar).
		if (cb_text_type_ >= 4 /*CB_TEXT_LAST*/)
		{
			const TASKDIALOG_BUTTON custom_buttons[] = {
				{ IDYES, L"Use \"Hex Text\" format" },
				{ IDNO,  L"Use \"traditional\" binary + text formats" },
			};
			cb_text_type_ = AvoidableTaskDialog
							(
								IDS_CLIPBOARD_FORMAT,
								"HexEdit supports two main options for using the Windows "
									"Clipboard. Binary data can be cut, copied and pasted "
									"as hex digits or as binary data + text.\n\n"
									"Choose your preferred option for using binary data.",
								"\nOn user request the \"Hex Text\" clipboard format is now supported "
									"but this format does have disadvantages as explained below.\n\n"
									"Hex Text\n\n"
									"Each byte is placed on the clipboard as two hex digits (with "
									"appropriate spacing). This makes it easy to paste the hex data "
									"into a text editor or document. The disadvantage is that it "
									"uses more memory which may be a problem for large amounts of "
									"data. It also means you can't copy the actual text content (eg. "
									"if the binary data happens to be ASCII or other form of text).\n\n"
									"Binary Data + Text\n\n"
									"The data is stored in two different formats - as binary data and "
									"as text. The binary format is efficient and compatible with the "
									"Visual Studio hex editor. The text format depends on the current "
									"text format in use such as ASCII or Unicode and allows you to "
									"copy any text content (non-text byte are ignored).\n\n"
									"Note that both options preserve all binary values when copying "
									"and pasting, including NUL (zero) bytes.\n\n"
									"Also note that you can change this setting at any time "
									"or choose other options using the Workspace/Edit page "
									"of the Options dialog.",
								NULL,
								0,
								MAKEINTRESOURCE(IDI_QUESTIONMARK),
								custom_buttons,
								2
							) == IDYES;
		}
		// CG: This line inserted by 'Tip of the Day' component.
		ShowTipAtStartup();

		if (run_autoexec_) RunAutoExec();

#ifdef FILE_PREVIEW
		// Start background task to clean up old thumbnails
		CleanupPreviewFiles();
#endif

		return TRUE;
}

void CHexEditApp::InitVersionInfo()
{
	// Get HexEdit version info from version resource
	DWORD dummy;                                        // Version functions take parameters that do nothing?!
	size_t vi_size;                                     // Size of all version info
	void* buf = nullptr;                                // Buffer to hold all version info
	void *p_version;                                    // Holds ptr to product version string in buffer
	UINT len;                                           // Length of product version string

	if ((vi_size = ::GetFileVersionInfoSize(__argv[0], &dummy)) > 0 &&
		(buf = malloc(vi_size+1)) != NULL &&
		::GetFileVersionInfo(__argv[0], dummy, vi_size, buf) &&
		::VerQueryValue(buf, "\\StringFileInfo\\040904B0\\ProductVersion",
						&p_version, &len) )
	{
		CString strVer;
		if (*((char *)p_version + 1) == 0)
			strVer = CString((wchar_t *)p_version);
		else
			strVer = CString((char *)p_version);

		char *endptr = strVer.GetBuffer();
		version_ = short(strtol(endptr, &endptr, 10) * 100);
		if (*endptr != '\0')
		{
			++endptr;
			version_ += short(strtol(endptr, &endptr, 10));

			// Check if a beta version
			if (*endptr != '\0')
			{
				++endptr;
				beta_ = (short)strtol(endptr, &endptr, 10);

				if (*endptr != '\0')
				{
					++endptr;
					revision_ = (short)strtol(endptr, &endptr, 10);
				}
			}
		}
	}
	free(buf);

	// Getting OS version info
	OSVERSIONINFO osvi;
	osvi.dwOSVersionInfoSize = sizeof(osvi);
	GetVersionEx(&osvi);
	is_nt_ = osvi.dwPlatformId == VER_PLATFORM_WIN32_NT;
	is_xp_ = osvi.dwPlatformId == VER_PLATFORM_WIN32_NT && osvi.dwMajorVersion >= 5 && osvi.dwMinorVersion >= 1;
	is_vista_ = osvi.dwPlatformId == VER_PLATFORM_WIN32_NT && osvi.dwMajorVersion >= 6;
	is_win7_ = osvi.dwPlatformId == VER_PLATFORM_WIN32_NT && osvi.dwMajorVersion >= 6 && osvi.dwMinorVersion >= 1;

	// Determine if multiple monitor supported (Win 98 or NT >= 5.0)
	// Note that Windows 95 is 4.00 and Windows 98 is 4.10
	mult_monitor_ = 
		(osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS &&
			 osvi.dwMajorVersion == 4 &&
			 osvi.dwMinorVersion >= 10  ||
			 osvi.dwMajorVersion >= 5) && ::GetSystemMetrics(SM_CMONITORS) > 1;
//        mult_monitor_ = osvi.dwMajorVersion >= 5;

	// Check for hexedit.chm file  (USE_HTML_HELP)
	htmlhelp_file_ = m_pszHelpFilePath;
	htmlhelp_file_.MakeUpper();
	if (htmlhelp_file_.Right(4) == ".HLP")
	{
		htmlhelp_file_ = htmlhelp_file_.Left(htmlhelp_file_.GetLength() - 4) + ".CHM";
		if (_access(htmlhelp_file_, 0) == -1)
			htmlhelp_file_.Empty();
	}

	// We must do this after getting version info as it relies on is_nt_
	m_pspecial_list = special_list_scan_ ? new CSpecialList() : NULL;

	InitConversions();                   // Read EBCDIC conversion table etc and validate conversions

	// Seed the random number generators
	unsigned int seed = ::GetTickCount();
	srand(seed);                    // Seed compiler PRNG (simple one)
	::rand_good_seed(seed);           // Seed our own PRNG

	// Set the locale to the native environment -- hopefully the MSC run-time
	// code does something suitable.  (This is currently just for thousands sep.)
	setlocale(LC_ALL, "");      // Set to native locale

	// Get decimal point characters, thousands separator and grouping
	struct lconv *plconv = localeconv();

	// Set defaults
	dec_sep_char_ = ','; dec_group_ = 3; dec_point_ = '.';

	// Work out thousands separator
	if (strlen(plconv->thousands_sep) == 1)
		dec_sep_char_ = *plconv->thousands_sep;

	// Work out thousands grouping
	if (strlen(plconv->grouping) != 1)
	{
		// Rarely used option of no grouping
		switch (GetProfileInt("Options", "AllowNoDigitGrouping", -1))
		{
		case -1:
			if (TaskMessageBox("Number Display Problem",
							  "You have digit grouping for large numbers turned "
							  "off or are using an unsupported grouping/format.  "
							  "(See Regional Settings in the Control Panel.)\n\n"
							  "Numbers will be displayed without grouping, eg:\n"
							  "\n2489754937\n\n"
							  "Do you want the default digit grouping instead? eg:\n"
							  "\n2,489,754,937\n\n", MB_YESNO) == IDNO)
			{
				WriteProfileInt("Options", "AllowNoDigitGrouping", 1);
				dec_group_ = 9999;  // a big number so that grouping is not done
			}
			else
			{
				// Remember for next time so we don't ask every time HexEdit is run
				WriteProfileInt("Options", "AllowNoDigitGrouping", 0);
			}
			break;
		case 1:
			dec_group_ = 9999;  // a big number so that grouping is not done
			break;
		case 0:
			break; // Nothing required - just use default settings above
		default:
			ASSERT(0);
		}
	}
	else
		dec_group_ = *plconv->grouping;

	// Work out decimal point
	if (strlen(plconv->decimal_point) == 1)
		dec_point_ = *plconv->decimal_point;

	// Work out if we appear to be in US for spelling changes
	//TODO: the string compare here seems like it'd be fragile - is there a more reliable way?
	//  Probably not from pure C or C++ library calls, so probably need to query from the platform.
	is_us_ = _strnicmp("English_United States", ::setlocale(LC_COLLATE, NULL), 20) == 0;
}

void CHexEditApp::InitWorkspace()
{
	// The following are for MFC9 (BCG) init
	SetRegistryBase(_T("Settings"));
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));
	// CMFCVisualManager::GetInstance()->SetFadeInactiveImage(FALSE);
	VERIFY(InitMouseManager());
	VERIFY(InitContextMenuManager());
	VERIFY(InitKeyboardManager());
	VERIFY(InitShellManager());
	//CMFCPopupMenu::EnableMenuSound(FALSE);

	// These are commands that are always shown in menus (not to be confused
	// with commands not on toolbars - see AddToolBarForImageCollection)
	static int dv_id[] =
	{
		ID_FILE_NEW,
		ID_FILE_OPEN,
		ID_FILE_CLOSE,
		ID_FILE_SAVE,
		ID_FILE_PRINT,
		ID_EXPORT_HEX_TEXT,
		ID_IMPORT_HEX_TEXT,
		ID_APP_EXIT,

		ID_EDIT_UNDO,
		ID_EDIT_CUT,
		ID_EDIT_COPY,
		ID_EDIT_PASTE,
		ID_COPY_HEX,
		ID_COPY_CCHAR,
		ID_PASTE_ASCII,
		ID_PASTE_UNICODE,
		ID_PASTE_EBCDIC,
		ID_MARK,
		ID_GOTO_MARK,
		ID_BOOKMARKS_EDIT,
		ID_EDIT_FIND,
		ID_EDIT_GOTO,

		ID_VIEW_VIEWBAR,
		ID_VIEW_EDITBAR,
		ID_VIEW_CALCULATOR,
		ID_VIEW_BOOKMARKS,
		ID_VIEW_FIND,
		ID_VIEW_PROPERTIES,
		ID_VIEW_EXPL,
		ID_VIEW_STATUS_BAR,
		ID_VIEW_RULER,
		ID_AUTOFIT,
		ID_ADDR_TOGGLE,
		ID_DISPLAY_HEX,
		ID_DISPLAY_BOTH,
		ID_CHARSET_ASCII,
		ID_CHARSET_ANSI,
		ID_CHARSET_OEM,
		ID_CHARSET_EBCDIC,
		ID_CONTROL_NONE,
		ID_CONTROL_ALPHA,
		ID_CONTROL_C,
		ID_PROPERTIES,

		ID_ASC2EBC,
		ID_EBC2ASC,
		ID_ANSI2IBM,
		ID_IBM2ANSI,
		ID_CRC32,
		ID_MD5,
		ID_SHA1,
		ID_SHA256,
		ID_ENCRYPT_ENCRYPT,
		ID_ENCRYPT_DECRYPT,
		ID_ZLIB_COMPRESS,
		ID_ZLIB_DECOMPRESS,
		ID_RAND_BYTE,
		ID_FLIP_16BIT,
		ID_REV_BYTE,
		ID_ASSIGN_BYTE,
		ID_NEG_BYTE,
		ID_INC_BYTE,
		ID_DEC_BYTE,
		ID_GTR_BYTE,
		ID_LESS_BYTE,
		ID_ADD_BYTE,
		ID_SUBTRACT_BYTE,
		ID_SUBTRACT_X_BYTE,
		ID_MUL_BYTE,
		ID_DIV_BYTE,
		ID_DIV_X_BYTE,
		ID_MOD_BYTE,
		ID_MOD_X_BYTE,
		ID_AND_BYTE,
		ID_OR_BYTE,
		ID_INVERT,
		ID_XOR_BYTE,
		ID_ROL_BYTE,
		ID_ROR_BYTE,
		ID_LSL_BYTE,
		ID_LSR_BYTE,
		ID_ASR_BYTE,

		ID_DFFD_NEW,
		ID_DFFD_OPEN_FIRST,
		ID_DFFD_HIDE,
		ID_DFFD_SPLIT,
		ID_DFFD_SYNC,
		ID_DFFD_REFRESH,
		ID_AERIAL_HIDE,
		ID_AERIAL_SPLIT,
		ID_COMP_NEW,
		ID_COMP_HIDE,
		ID_COMP_SPLIT,
		ID_PREVW_HIDE,
		ID_PREVW_SPLIT,

		ID_CALCULATOR,
		ID_CUSTOMIZE,
		ID_OPTIONS,
		ID_RECORD,
		ID_MACRO_LAST,
		ID_PLAY,

		ID_WINDOW_NEW,
		ID_WINDOW_NEXT,

		ID_HELP_FINDER,
		ID_CONTEXT_HELP,
		ID_HELP_TUTE4,
		ID_HELP_TUTE1,
		ID_HELP_TUTE2,
		ID_HELP_TUTE3,
		ID_HELP_FORUM,
		ID_HELP_HOMEPAGE,
		ID_REPAIR_DIALOGBARS,
		ID_REPAIR_CUST,
		ID_APP_ABOUT,

		ID_HIGHLIGHT,
		ID_DIALOGS_DOCKABLE,
		ID_IND_SEL,
		ID_ANT_SEL,
		ID_AERIAL_ZOOM1,
		ID_AERIAL_ZOOM2,
		ID_AERIAL_ZOOM4,
		ID_AERIAL_ZOOM8,
	};

	CMFCMenuBar::SetRecentlyUsedMenus(FALSE);
	for (int ii = 0; ii < sizeof(dv_id)/sizeof(*dv_id); ++ii)
		CMFCToolBar::AddBasicCommand(dv_id[ii]);

	// Enable BCG tools menu handling
	// (see CMainFrame::LoadFrame for default tools setup)
	EnableUserTools(ID_TOOLS_ENTRY, ID_TOOL1, ID_TOOL9,
		RUNTIME_CLASS (CHexEditUserTool));

#ifdef SYS_SOUNDS
	// Make sure sound registry settings are present
	CSystemSound::Add(_T("Invalid Character"),                 CSystemSound::Get(_T(".Default"), _T(".Default"), _T(".Default")));
	CSystemSound::Add(_T("Macro Finished"),                    CSystemSound::Get(_T("SystemAsterisk"), _T(".Default"), _T(".Default")));
	CSystemSound::Add(_T("Macro Error"),                       CSystemSound::Get(_T(".Default"), _T(".Default"), _T(".Default")));
	CSystemSound::Add(_T("Calculator Overflow"),               CSystemSound::Get(_T(".Default"), _T(".Default"), _T(".Default")));
	CSystemSound::Add(_T("Calculator Error"),                  CSystemSound::Get(_T(".Default"), _T(".Default"), _T(".Default")));
	CSystemSound::Add(_T("Comparison Difference Found"));
	CSystemSound::Add(_T("Comparison Difference Not Found"),   CSystemSound::Get(_T("SystemAsterisk"), _T(".Default"), _T(".Default")));
	CSystemSound::Add(_T("Search Text Found"));
	CSystemSound::Add(_T("Search Text Not Found"),             CSystemSound::Get(_T("SystemAsterisk"), _T(".Default"), _T(".Default")));
	CSystemSound::Add(_T("Background Search Finished"));
	CSystemSound::Add(_T("Background Scan Finished"));
	CSystemSound::Add(_T("Background Compare Finished"));
	CSystemSound::Add(_T("Invalid Address"),                   CSystemSound::Get(_T("SystemAsterisk"), _T(".Default"), _T(".Default")));
	CSystemSound::Add(_T("Read Only"),                         CSystemSound::Get(_T("SystemAsterisk"), _T(".Default"), _T(".Default")));
#endif

	LoadStdProfileSettings(0);  // Load standard INI file options (including MRU)

	GetXMLFileList();
}

void CHexEditApp::OnAppExit()
{
	SaveToMacro(km_exit);
	CWinAppEx::OnAppExit();
}

#if 0 // xxx do we need this?
// Called on 1st run after upgrade to a new version
void CHexEditApp::OnNewVersion(int old_ver, int new_ver)
{
	if (old_ver == 200)
	{
		// Version 2.0 used BCG 5.3 which did not support resource smart update
		CMFCToolBar::ResetAll();
	}
	else if (old_ver == 210)
	{
		// We need to reset the Edit Bar otherwise the edit controls (Find/Jump tools) don't work properly
		CMainFrame *mm = (CMainFrame *)AfxGetMainWnd();
		mm->m_wndEditBar.RestoreOriginalstate();
	}
}
#endif

// Called when a user runs HexEdit who has never run it before
void CHexEditApp::OnNewUser()
{
	CString dstFolder;
	if (!::GetDataPath(dstFolder))
		return;       // no point in continuing (Win95?)

	CString dstFile = dstFolder + FILENAME_DTD;

	// If DTD file exists, probably just the registry settings were deleted so don't ask
	// if we want to copy (but copy over files if not already there
	if (::_access(dstFile, 0) == -1 &&
		TaskMessageBox("New User",
					  "This is the first time you have run this version of HexEdit.\n\n"
					  "Do you want to set up you own personal copies of templates and macros?",
					  MB_YESNO) == IDYES)
	{
		CopyUserFiles();
	}
}

void CHexEditApp::CopyUserFiles()
{
	CString srcFolder = ::GetExePath();
	CString dstFolder;
	if (srcFolder.IsEmpty() || !::GetDataPath(dstFolder))
	{
		ASSERT(0);
		return;       // no point in continuing if we can't get paths (Win95?)
	}

	CString srcFile, dstFile;
	dstFile = dstFolder + FILENAME_DTD;

	// We need to copy the following files from the HexEdit binary directory
	// to the user's application data directory:
	// BinaryFileFormat.DTD    - DTD for templates
	// *.XML                   - the templates including Default.XML, _standard_types.XML etc
	// _windows_constants.TXT  - used by C/C++ parser
	// *.HEM                   - any provided keystroke macros

	srcFile = srcFolder + FILENAME_DTD;
	//dstFile = dstFolder + FILENAME_DTD; // already done above
	::MakeSureDirectoryPathExists(dstFile);   // this creates any folders if necessary
	::CopyFile(srcFile, dstFile, FALSE);

	srcFile = srcFolder + "_windows_constants.txt";
	dstFile = dstFolder + "_windows_constants.txt";
	::CopyFile(srcFile, dstFile, TRUE);

	CFileFind ff;
	BOOL bContinue = ff.FindFile(srcFolder + "*.XML");
	while (bContinue)
	{
		bContinue = ff.FindNextFile();
		::CopyFile(ff.GetFilePath(), dstFolder + ff.GetFileName(), TRUE);
	}

	bContinue = ff.FindFile(srcFolder + "*.HEM");
	while (bContinue)
	{
		bContinue = ff.FindNextFile();
		::CopyFile(ff.GetFilePath(), dstFolder + ff.GetFileName(), TRUE);
	}
}

void CHexEditApp::OnFileNew()
{
	no_ask_insert_ = false;  // make sure we show ask the user for insert options
	CWinAppEx::OnFileNew();
}

void CHexEditApp::FileFromString(LPCTSTR str)
{
	no_ask_insert_ = true;   // turn off insert options as we are simply inserting a string
	CWinAppEx::OnFileNew();
	no_ask_insert_ = false;
	CHexEditView * pview = GetView();
	if (pview != NULL)
		pview->GetDocument()->Change(mod_insert, 0, strlen(str), (unsigned char *)str, 0, pview);
}

void CHexEditApp::OnFileOpen()
{
	DWORD flags = OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST | OFN_SHOWHELP;
	if (no_recent_add_)
		flags |= OFN_DONTADDTORECENT;
	CFileOpenDialog dlgFile(NULL, flags, GetCurrentFilters());
	//CHexFileDialog dlgFile("FileOpenDlg", TRUE, NULL, NULL,
	//                       flags, GetCurrentFilters());

	// Set up buffer where selected file names are stored & default to none
	char all_files[16384];
	all_files[0] = all_files[1] = '\0';
	dlgFile.m_ofn.lpstrFile = all_files;
	dlgFile.m_ofn.nMaxFile = sizeof(all_files)-1;

	// Set up the title of the dialog
	CString title;
	VERIFY(title.LoadString(AFX_IDS_OPENFILE));
	dlgFile.m_ofn.lpstrTitle = title;

	// Change the initial directory
	CHexEditView * pv;
	CString strDir;      // folder name passed to dialog [this must not be detroyed until after dlgFile.DoModal()]
	switch (open_locn_)
	{
	case FL_DOC:
		if ((pv = GetView()) != NULL && pv->GetDocument() != NULL && pv->GetDocument()->pfile1_ != NULL)
		{
			// Get the path from the filename of the active file
			CString filename = pv->GetDocument()->pfile1_->GetFilePath();
			int path_len;                   // Length of path (full name without filename)
			path_len = filename.ReverseFind('\\');
			if (path_len == -1) path_len = filename.ReverseFind('/');
			if (path_len == -1) path_len = filename.ReverseFind(':');
			if (path_len == -1)
				path_len = 0;
			else
				++path_len;
			strDir = filename.Left(path_len);
		}
		break;
	case FL_LAST:
		strDir = last_open_folder_;
		break;
	case FL_BOTH:
		strDir = last_both_folder_;
		break;
	default:
		ASSERT(0);
		// fall through
	case FL_SPECIFIED:
		strDir = open_folder_;
		break;
	}
	// If still empty default to the specified folder
	if (strDir.IsEmpty())
		strDir = open_folder_;
	dlgFile.m_ofn.lpstrInitialDir = strDir;

	if (dlgFile.DoModal() != IDOK)                      // ===== run dialog =======
	{
		mac_error_ = 2;
		return;
	}
	//open_file_readonly_ = (dlgFile.m_ofn.Flags & OFN_READONLY) != 0;

	// For some absolutely ridiculous reason if the user only selects one file
	// the full filename is just returned rather than as specified for more than
	// one file with OFN_ALLOWMULTISELECT.  So we need special code to handle this.
	if (dlgFile.m_ofn.nFileOffset < strlen(all_files))
	{
		ASSERT(all_files[dlgFile.m_ofn.nFileOffset-1] == '\\');
		all_files[strlen(all_files)+1] = '\0';      // Ensure double null ended
		all_files[dlgFile.m_ofn.nFileOffset-1] = '\0';
	}

	// Get directory name as first part of files buffer
	CString dir_name(all_files);
	last_open_folder_ = last_both_folder_ = dir_name;

	// Get file names separated by nul char ('\0') stop on 2 nul chars
	for (const char *pp = all_files + strlen(all_files) + 1; *pp != '\0';
					 pp = pp + strlen(pp) + 1)
	{
		CString filename;
		if (dir_name[dir_name.GetLength()-1] == '\\')
			filename = dir_name + pp;
		else
			filename= dir_name + "\\" + pp;

		// Store this here so the document can find out if it has to open read-only
		// Note; this is done for each file as it is cleared after each use in file_open
		ASSERT(open_current_readonly_ == -1);
		ASSERT(open_current_shared_ == -1);
		open_current_readonly_ = (dlgFile.m_ofn.Flags & OFN_READONLY) != 0;
		open_current_shared_ = dlgFile.open_shareable_;

		CHexEditDoc *pdoc;
		if ((pdoc = (CHexEditDoc*)(OpenDocumentFile(filename))) != NULL)
		{
			// Completed OK so store in macro if recording
			if (recording_ && mac_.size() > 0 && (mac_.back()).ktype == km_focus)
			{
				// We don't want focus change recorded (see CHexEditView::OnSetFocus)
				mac_.pop_back();
			}
			SaveToMacro(km_open, filename);
		}
		else
			mac_error_ = 20;
	}
}

CDocument* CHexEditApp::OpenDocumentFile(LPCTSTR lpszFileName)
{
	CWaitCursor wc;
	CDocument *retval = CWinAppEx::OpenDocumentFile(lpszFileName);
	open_current_readonly_ = open_current_shared_ = -1;   // just in case OnOpenDocument() not called because file is already open

	if (retval == NULL)
		return NULL;     // File or device not found (error message has already been shown)

	// Get file extension and change "." to "_" and make macro filename
	ASSERT(mac_dir_.Right(1) == "\\");
	CString mac_filename = lpszFileName;
	if (mac_filename.ReverseFind('.') == -1)
		mac_filename = mac_dir_ + "_.hem";               // Filename without extension
	else
		mac_filename = mac_dir_ + CString("_") + mac_filename.Mid(mac_filename.ReverseFind('.')+1) + ".hem";

	std::vector<key_macro> mac;
	CString comment;
	int halt_lev;
	long plays;
	int version;  // Version of HexEdit in which the macro was recorded

	if (::_access(mac_filename, 0) == 0 &&
		macro_load(mac_filename, &mac, comment, halt_lev, plays, version))
	{
		((CMainFrame *)AfxGetMainWnd())->StatusBarText(comment);
		macro_play(plays, &mac, halt_lev);
	}

	return retval;
}

void CHexEditApp::CloseByName(const char * fname)
{
	// For each document, allow the user to save it if modified, then close it
	POSITION posn = m_pDocTemplate->GetFirstDocPosition();
	while (posn != NULL)
	{
		CHexEditDoc *pdoc = dynamic_cast<CHexEditDoc *>(m_pDocTemplate->GetNextDoc(posn));
		ASSERT(pdoc != NULL);
		if (pdoc->GetFilePath().CompareNoCase(fname) == 0)
		{
			pdoc->OnCloseDocument();
			return;
		}
	}
}

BOOL CHexEditApp::OnOpenRecentFile(UINT nID)
{
	// I had to completely override OnOpenRecentFile (copying most of the code from
	// CWinAppEx::OnOpenRecentFile) since it does not return FALSE if the file is not
	// found -- in fact it always returns TRUE.  I'd say this is an MFC bug.
	ASSERT(m_pRecentFileList != NULL);

	ASSERT(nID >= ID_FILE_MRU_FILE1);
	ASSERT(nID < ID_FILE_MRU_FILE1 + (UINT)m_pRecentFileList->GetSize());
	int nIndex = nID - ID_FILE_MRU_FILE1;
	ASSERT((*m_pRecentFileList)[nIndex].GetLength() != 0);

	TRACE2("MRU: open file (%d) '%s'.\n", (nIndex) + 1,
					(LPCTSTR)(*m_pRecentFileList)[nIndex]);

	// Save file name now since its index will be zero after it's opened
	CString file_name = (*m_pRecentFileList)[nIndex];

	ASSERT(open_current_readonly_ == -1);
	ASSERT(open_current_shared_ == -1);
	if (OpenDocumentFile((*m_pRecentFileList)[nIndex]) == NULL)
	{
		if (AvoidableTaskDialog(IDS_RECENT_GONE,
								"The file or device could not be opened.\n\n"
								"Do you want to remove the entry from the recent file list?",
								NULL, NULL,
								TDCBF_YES_BUTTON | TDCBF_NO_BUTTON) == IDYES)
		{
			m_pRecentFileList->Remove(nIndex);
		}
		mac_error_ = 10;                        // User has been told that file could not be found
		return FALSE;
	}
	else
	{
		// Completed OK so store in macro if recording
		if (recording_ && mac_.size() > 0 && (mac_.back()).ktype == km_focus)
		{
			// We don't want focus change recorded (see CHexEditView::OnSetFocus)
			mac_.pop_back();
		}
		SaveToMacro(km_open, file_name);
		return TRUE;
	}
	ASSERT(0);                                  // We shouldn't get here
}

void CHexEditApp::OnFilePrintSetup()
{
	CPrintDialog pd(TRUE);
	DoPrintDialog(&pd);
	SaveToMacro(km_print_setup);
}

void CHexEditApp::OnFileSaveAll()
{
	// SaveAllModified will prompt to save - we want to simply always save
	//m_pDocManager->SaveAllModified();

	// Get each open document and force save
	POSITION posn = m_pDocTemplate->GetFirstDocPosition();
	while (posn != NULL)
	{
		CHexEditDoc *pdoc = dynamic_cast<CHexEditDoc *>(m_pDocTemplate->GetNextDoc(posn));
		ASSERT(pdoc != NULL);
		// Note: We don't display a message if the file name is empty as this was a
		// file that was not yet written to disk and the user probably cancelled
		// out of the file save dialog (so they already know the file is not saved).
		if (pdoc->IsModified() && !pdoc->DoFileSave() && !pdoc->GetFileName().IsEmpty())
			TaskMessageBox("File Not Saved", "During the \"Save All\" operation the following file could not be saved:\n\n" +
						   pdoc->GetFileName());
	}
}

void CHexEditApp::OnFileCloseAll()
{
	// For each document, allow the user to save it if modified, then close it
	POSITION posn = m_pDocTemplate->GetFirstDocPosition();
	while (posn != NULL)
	{
		CHexEditDoc *pdoc = dynamic_cast<CHexEditDoc *>(m_pDocTemplate->GetNextDoc(posn));
		ASSERT(pdoc != NULL);
		if (!pdoc->SaveModified())      // save/no save return true, cancel/error returns false
			return;
		pdoc->OnCloseDocument();
	}
}

void CHexEditApp::OnFileCloseOthers()
{
	CHexEditDoc *pcurrent = NULL;          // current doc (not to be closed)
	CHexEditView *pview = GetView();   // get current view
	if (pview != NULL)
		pcurrent = pview->GetDocument();

	// For each document, allow the user to save it if modified, then close it
	POSITION posn = m_pDocTemplate->GetFirstDocPosition();
	while (posn != NULL)
	{
		CHexEditDoc *pdoc = dynamic_cast<CHexEditDoc *>(m_pDocTemplate->GetNextDoc(posn));
		ASSERT(pdoc != NULL);
		if (pdoc == pcurrent)
			continue;                   // bypass current document

		if (!pdoc->SaveModified())      // save/no save return true, cancel/error returns false
			return;
		pdoc->OnCloseDocument();
	}
}

void CHexEditApp::OnFileOpenSpecial()
{
	COpenSpecialDlg dlg;

	if (dlg.DoModal() == IDOK)
	{
		// Store this here so the document can find out if it has to open read-only
		ASSERT(open_current_readonly_ == -1);
		open_current_readonly_ = dlg.ReadOnly();

		CHexEditDoc *pdoc;
		if ((pdoc = (CHexEditDoc*)(OpenDocumentFile(dlg.SelectedDevice()))) != NULL)
		{
			// Completed OK so store in macro if recording
			if (recording_ && mac_.size() > 0 && (mac_.back()).ktype == km_focus)
			{
				// We don't want focus change recorded (see CHexEditView::OnSetFocus)
				mac_.pop_back();
			}
			SaveToMacro(km_open, dlg.SelectedDevice());
		}
		else
			mac_error_ = 20;
	}
	else
		mac_error_ = 2;         // User cancelled out of dialog is a minor error
}

void CHexEditApp::OnUpdateFileOpenSpecial(CCmdUI* pCmdUI)
{
	pCmdUI->Enable();
}

void CHexEditApp::OnRepairFiles()
{
	if (TaskMessageBox("Set Up Personal Files?",
					  "This copies factory default files to your personal data folders, "
					  "including macros and templates.\n"
					  "\nIf you already have these files and have made changes to any "
					  "of them then your changes will be overwritten.\n"
					  "\nDo you want to continue?",
					  MB_YESNO) != IDYES)
		return;

	CopyUserFiles();
}

void CHexEditApp::OnRepairDialogbars()
{
	if (TaskMessageBox("Restore All Dialogs?",
					  "This restores all modeless dialogs so they are "
					  "visible, undocked and unrolled.  They include:\n"
					  "* Calculator\n"
					  "* Properties dialog\n"
					  "* Bookmarks dialog\n"
					  "* Find dialog\n"
					  "* Explorer Window\n"
					  "\nYou may need to do this if you cannot restore any of the above dialogs.\n"
					  "\nDo you want to continue?",
					  MB_YESNO) != IDYES)
		return;

	CMainFrame *mm = (CMainFrame *)AfxGetMainWnd();
	ASSERT(mm != NULL);

	mm->InitDockWindows();

	//mm->m_paneExpl.ShowAndUnroll();
	mm->m_paneExpl.ShowPane(TRUE, FALSE, TRUE);
	mm->m_paneFind.ShowPane(TRUE, FALSE, TRUE);
	mm->m_paneCompareList.ShowPane(TRUE, FALSE, TRUE);
}

void CHexEditApp::OnRepairCust()
{
	if (TaskMessageBox("Reset Customizations?",
					  "You may need to repair customizations if:\n"
					  "* standard toolbars are missing\n"
					  "* toolbar buttons or menu items are missing\n"
					  "* toolbar or menu icons are incorrect\n"
					  "* standard keystrokes do not work\n"
					  "* you cannot assign custom keystrokes to a command\n\n"
					  "You will lose all customizations including:\n"
					  "* changes to standard toolbars\n"
					  "* changes to all menus including context (popup) menus\n"
					  "* keyboard customizations\n"
					  "\nDo you want to continue?",
					  MB_YESNO, MAKEINTRESOURCE(IDI_CROSS)) != IDYES)
		return;

	GetContextMenuManager()->ResetState();
	GetKeyboardManager()->ResetAll();
	CMFCToolBar::ResetAll();
}

void CHexEditApp::OnRepairSettings()
{
	if (TaskMessageBox("Reset Settings?",
					  "All customizations and changes to "
					  "settings will be removed.  "
					  "(All registry entries will be removed.)\n"
					  "\nTo do this HexEdit must close.\n"
					  "\nDo you want to continue?",
					  MB_YESNO, MAKEINTRESOURCE(IDI_CROSS)) != IDYES)
		return;

	// Signal deletion of all registry settings
	delete_reg_settings_ = TRUE;

	CWinAppEx::OnAppExit();
}

void CHexEditApp::OnRepairAll()
{
	if (TaskMessageBox("Repair All",
					  "All customizations, registry settings,  "
					  "file settings etc will be removed, including:\n"
					  "* toolbar, menu and keyboard customizations\n"
					  "* settings made in the Options dialog\n"
					  "* previously opened files settings (columns etc)\n"
					  "* recent file list, bookmarks, highlights etc\n\n"
					  "When complete you will need to restart HexEdit.\n"
					  "\nAre you absolutely sure you want to continue?",
					  MB_YESNO, MAKEINTRESOURCE(IDI_CROSS)) != IDYES)
		return;

	// Signal deletion of all registry settings and settings files
	delete_all_settings_ = TRUE;

	CWinAppEx::OnAppExit();
}

void CHexEditApp::OnMacroRecord()
{
	// Allow calculator to tidy up any pending macro ops
	if (recording_)
		((CMainFrame *)AfxGetMainWnd())->m_wndCalc.FinishMacro();

	recording_ = !recording_;
	// Don't clear the last macro until we get the first key of the next
	if (recording_)
		no_keys_ = TRUE;            // Flag to say new macro started
	else
		// Track that we are recording in the current version.  Note that macro_version_
		// may be different if we loaded a macro recorded in a diff version of HexEdt.
		macro_version_ = INTERNAL_VERSION;

#ifdef _DEBUG
	// Some commands call other commands as part of there task.  This can acc-
	// identally result in extra commands being unintentionally recorded. To
	// help detect this we display a trace message (in debug version) if there
	// is more than one entry in the macro vector - to detect problems we must
	// record a macro with just one command & check the debug window when run.
	if (!recording_ && mac_.size() > 1)
		TRACE1("Macro size is %ld\n", long(mac_.size()));
#endif
	// If invoked from toolbar make sure focus returns to view
	CHexEditView *pview = GetView();    // The active view (or NULL if none)
	if (pview != NULL && pview != pview->GetFocus())
		pview->SetFocus();
}

void CHexEditApp::OnUpdateMacroRecord(CCmdUI* pCmdUI)
{
	if (recording_)
		pCmdUI->SetText("Stop Recording");
	else
		pCmdUI->SetText("Record Macro");
	pCmdUI->SetCheck(recording_);
}

void CHexEditApp::OnMacroPlay()
{
	ASSERT(!recording_);
	macro_play();

	// Set focus to currently active view
	CHexEditView *pview = GetView();    // The active view (or NULL if none)
	if (pview != NULL && pview != pview->GetFocus())
		pview->SetFocus();
}

void CHexEditApp::OnUpdateMacroPlay(CCmdUI* pCmdUI)
{
	// We can play the current macro if we're not recording it and it's not empty
	pCmdUI->Enable(!recording_ && mac_.size() > 0);
}

void CHexEditApp::OnMacroMessage()
{
	ASSERT(recording_);
	if (recording_)
	{
		CMacroMessage dlg;

		if (dlg.DoModal() == IDOK)
			SaveToMacro(km_macro_message, dlg.message_);
	}
}

void CHexEditApp::OnUpdateMacroMessage(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(recording_);
}

void CHexEditApp::OnMultiPlay()
{
	CMultiplay dlg;
	dlg.plays_ = plays_;

	if (dlg.DoModal() == IDOK)
	{
		if (dlg.macro_name_ == DEFAULT_MACRO_NAME)
		{
			ASSERT(!recording_);
			plays_ = dlg.plays_;
			macro_play(plays_);
		}
		else
		{
			play_macro_file(dlg.macro_name_, dlg.plays_);
		}
	}

	// Set focus to currently active view
	CHexEditView *pview = GetView();    // The active view (or NULL if none)
	if (pview != NULL && pview != pview->GetFocus())
		pview->SetFocus();
}

void CHexEditApp::OnUpdateMultiPlay(CCmdUI* pCmdUI)
{
	if (!recording_ && mac_.size() > 0)
	{
		pCmdUI->Enable(TRUE);
		return;
	}
	else
	{
		CFileFind ff;
		ASSERT(mac_dir_.Right(1) == "\\");
		BOOL bContinue = ff.FindFile(mac_dir_ + "*.hem");

		while (bContinue)
		{
			// At least one match - check them all
			bContinue = ff.FindNextFile();

			// Enable if there are macro file that do not start with an underscore OR
			// there are any macro file if we are recording.
			if (recording_ || ff.GetFileTitle().Left(1) != "_")
			{
				pCmdUI->Enable(TRUE);
				return;
			}
		}
	}
	pCmdUI->Enable(FALSE);
}

void CHexEditApp::play_macro_file(const CString &filename, int pp /*= -1*/)
{
	std::vector<key_macro> tmp;
	CString comment;
	int halt_lev;
	long plays;
	int version;  // Version of HexEdit in which the macro was recorded

	ASSERT(mac_dir_.Right(1) == "\\");
	if (macro_load(mac_dir_ + filename + ".hem", &tmp, comment, halt_lev, plays, version))
	{
		BOOL saved_recording = recording_;
		recording_ = FALSE;
		macro_play(pp == -1 ? plays : pp, &tmp, halt_lev);
		recording_ = saved_recording;
		SaveToMacro(km_macro_play, filename);
	}
}

void CHexEditApp::RunAutoExec()
{
	ASSERT(mac_dir_.Right(1) == "\\");
	CString filename = mac_dir_ + "autoexec.hem";
	std::vector<key_macro> mac;
	CString comment;
	int halt_lev;
	long plays;
	int version;  // Version of HexEdit in which the macro was recorded

	if (::_access(filename, 0) == 0 &&
		macro_load(filename, &mac, comment, halt_lev, plays, version))
	{
		((CMainFrame *)AfxGetMainWnd())->StatusBarText(comment);
		macro_play(plays, &mac, halt_lev);

		// Set focus to currently active view
		CHexEditView *pview = GetView();    // The active view (or NULL if none)
		if (pview != NULL && pview != pview->GetFocus())
			pview->SetFocus();
	}
}

void CHexEditApp::OnRecentFiles()
{
	CRecentFileDlg dlg;
	dlg.DoModal();
}

void CHexEditApp::OnBookmarksEdit()
{
	ASSERT(AfxGetMainWnd() != NULL);
	((CMainFrame *)AfxGetMainWnd())->m_paneBookmarks.ShowAndUnroll();
}

void CHexEditApp::OnTabIcons()
{
	tabicons_ = !tabicons_;
	update_tabs();
}

void CHexEditApp::OnUpdateTabIcons(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(tabicons_);
}

void CHexEditApp::OnTabsAtBottom()
{
	tabsbottom_ = !tabsbottom_;
	update_tabs();
}

void CHexEditApp::OnUpdateTabsAtBottom(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(tabsbottom_);
}

void CHexEditApp::update_tabs()
{
	CMainFrame *mm = (CMainFrame *)AfxGetMainWnd();
	if (mm != NULL)
	{
		ASSERT_KINDOF(CMainFrame, mm);
		CMDITabInfo mdiTabParams;
		mdiTabParams.m_bTabIcons = tabicons_;
		mdiTabParams.m_tabLocation = tabsbottom_ ? CMFCTabCtrl::LOCATION_BOTTOM : 
		                                           CMFCTabCtrl::LOCATION_TOP;
		mdiTabParams.m_bActiveTabCloseButton = tabclose_;
		mdiTabParams.m_bAutoColor = tabcolour_;
		mdiTabParams.m_style = tabcolour_ ? CMFCTabCtrl::STYLE_3D_ONENOTE : CMFCTabCtrl::STYLE_3D_SCROLLED;

		mdiTabParams.m_bTabCustomTooltips = TRUE;
		mm->EnableMDITabbedGroups(mditabs_, mdiTabParams);
	}
}

/////////////////////////////////////////////////////////////////////////////
// CHexEditApp commands

int CHexEditApp::ExitInstance()
{
#ifdef FILE_PREVIEW
	// Tell cleanup thread to forget it
	appdata_.Lock();
	if (cleanup_thread_ != NULL)
		thread_stop_ = true;
	appdata_.Unlock();
#endif

	// Save "save on exit" option if it has changed
	if (save_exit_ != orig_save_exit_)
		WriteProfileInt("Options", "SaveExit", save_exit_ ? 1 : 0);

	// Save other options if saving on exit
	if (save_exit_)
		SaveOptions();

	// If we have a custom clipboard (large temp file) format
	// then we need to remove it and the temp file.
	if (!last_cb_temp_file_.IsEmpty())
	{
		// First check if it is still on the clipboard
		if (last_cb_seq_ == ::GetClipboardSequenceNumber())
		{
#ifdef _DEBUG
			// Ensure that the format is still present (else something is very wrong)
			UINT fmt = ::RegisterClipboardFormat(temp_format_name);
			ASSERT(::IsClipboardFormatAvailable(fmt));
#endif
			if (::OpenClipboard(HWND(0)))
				::EmptyClipboard();
			::CloseClipboard();
		}
		::remove(last_cb_temp_file_);
		last_cb_temp_file_.Empty();
	}

	// If we wrote something big to the clipboard ask the user if they want to delete it
	else if (last_cb_size_ > 500000 && 
			 last_cb_seq_ == ::GetClipboardSequenceNumber() &&
			 ::OpenClipboard(HWND(0)))
	{
		CString mess;
		mess.Format("You currently have a large amount of data on the clipboard (%sbytes).\n\n"
			        "Do you want to leave the data on the clipboard?", NumScale((double)last_cb_size_));
		if (AvoidableTaskDialog(IDS_LEAVE_LARGE_CB, mess, NULL, NULL, TDCBF_YES_BUTTON | TDCBF_NO_BUTTON) != IDYES)
			::EmptyClipboard();

		::CloseClipboard();
	}

	if (pboyer_ != NULL)
		delete pboyer_;

	afxGlobalData.CleanUp();

	if (m_pbookmark_list != NULL)
	{
		m_pbookmark_list->WriteList();
		delete m_pbookmark_list;
	}

	if (m_pspecial_list != NULL)
	{
		delete m_pspecial_list;
	}

	int retval = CWinAppEx::ExitInstance();

	if (delete_reg_settings_ || delete_all_settings_)
	{
		::SHDeleteKey(HKEY_CURRENT_USER, "Software\\ECSoftware\\HexEdit");  // user settings
		::SHDeleteKey(HKEY_LOCAL_MACHINE, "Software\\ECSoftware\\HexEdit");  // machine settings
	}

	if (delete_all_settings_)
	{
		CString data_path;
		::GetDataPath(data_path);
		if (!data_path.IsEmpty())
		{
			// NOTE: This needs to be updated when new data files added
			remove(data_path + FILENAME_RECENTFILES);
			remove(data_path + FILENAME_BOOKMARKS);
			remove(data_path + FILENAME_BACKGROUND);
		}
	}

	return retval;
}

BOOL CHexEditApp::PreTranslateMessage(MSG* pMsg)
{
	// The following is necessary to allow controls in the calculator and bookmarks dialog to process
	// keystrokes normally (eg DEL and TAB keys).  If this is not done CWnd::WalkPreTranslateTree
	// is called which allows the mainframe window to process accelerator keys.  This problem was noticed
	// when DEL and TAB were turned into commands (with associated accelerators) but could have always
	// happened since 2.0 if the user assigned these keys, arrow keys etc to a command in Customize dialog.
	HWND hw = ::GetParent(pMsg->hwnd);
	if (m_pMainWnd != NULL && pMsg->message == WM_KEYDOWN &&
		(hw == ((CMainFrame*)m_pMainWnd)->m_wndCalc || 
		 hw == ((CMainFrame*)m_pMainWnd)->m_wndExpl ||
		 hw == ((CMainFrame*)m_pMainWnd)->m_wndBookmarks) )
	{
		// Return 0 to allow processing (WM_KEYDOWN) but because we don't call base class version
		// (CWinAppEx::PreTranslateMessage) we avoid the key being absorbed by a keyboard accelerator.
		return FALSE;
	}

	// This allows a tilde to be inserted in char area (rather than being used for NOT command)
	CWnd *pwnd = CWnd::FromHandlePermanent(pMsg->hwnd);
	CHexEditView *pv;
	if (pMsg->message == WM_CHAR &&
		pwnd != NULL &&
		pwnd->IsKindOf(RUNTIME_CLASS(CHexEditView)) &&
		(pv = DYNAMIC_DOWNCAST(CHexEditView, pwnd)) != NULL &&
		pv->CharMode())
	{
		return FALSE;
	}
#ifdef _DEBUG
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == 65)
		pMsg->message = WM_KEYDOWN;   // This is just so we can put a breakpoint here
#endif

	return CWinAppEx::PreTranslateMessage(pMsg);
}

void CHexEditApp::WinHelp(DWORD dwData, UINT nCmd)
{
	switch(nCmd)
	{
	case HELP_CONTEXT:
		if (::HtmlHelp(m_pMainWnd->GetSafeHwnd(), htmlhelp_file_, HH_HELP_CONTEXT, dwData))
			return;
		break;
	case HELP_FINDER:
		if (::HtmlHelp(m_pMainWnd->GetSafeHwnd(), htmlhelp_file_, HH_DISPLAY_TOPIC, dwData))
			return;
		break;
	}
	ASSERT(0);
	AfxMessageBox(AFX_IDP_FAILED_TO_LAUNCH_HELP);
}

void CHexEditApp::OnAppContextHelp (CWnd* pWndControl, const DWORD dwHelpIDArray [])
{
	CWinAppEx::OnAppContextHelp(pWndControl, dwHelpIDArray);
}

BOOL CHexEditApp::OnIdle(LONG lCount)
{
	ASSERT(AfxGetMainWnd() != NULL);
	CMainFrame *mm = (CMainFrame *)AfxGetMainWnd();
	mm->m_wndFind.SendMessage(WM_KICKIDLE);
	mm->m_wndBookmarks.SendMessage(WM_KICKIDLE);
	mm->m_wndProp.SendMessage(WM_KICKIDLE);
	mm->m_wndCalc.SendMessage(WM_KICKIDLE);
	mm->m_wndCalcHist.SendMessage(WM_KICKIDLE);
	mm->m_wndExpl.SendMessage(WM_KICKIDLE);
	mm->m_wndCompareList.SendMessage(WM_KICKIDLE);

	// Allow docs to check if their background processing has completed
	POSITION posn = m_pDocTemplate->GetFirstDocPosition();
	while (posn != NULL)
	{
		CHexEditDoc *pdoc = dynamic_cast<CHexEditDoc *>(m_pDocTemplate->GetNextDoc(posn));
		ASSERT(pdoc != NULL);
		if (pdoc != NULL)
			pdoc->CheckBGProcessing();
	}

	CHexEditView *pview = GetView();
	if (lCount == 1 && pview != NULL)
	{
		// Check things for the active view
		pview->check_error();                           // check if read error
	}

	BOOL tmp1 = mm->UpdateBGSearchProgress();
	BOOL tmp2 = mm->UpdateBGCompareProgress();
	if (tmp1 || tmp2)
	{
		(void)CWinAppEx::OnIdle(lCount);
		return TRUE;                    // we want more processing
	}

	if (last_cb_seq_ != ::GetClipboardSequenceNumber())
	{
		// clipboard has changed
		if (!last_cb_temp_file_.IsEmpty())
		{
			::remove(last_cb_temp_file_);
			last_cb_temp_file_.Empty();
		}
	}

	return CWinAppEx::OnIdle(lCount);
}

void CHexEditApp::PreLoadState()
{
	// Add double click handlers
	GetMouseManager()->AddView(IDR_CONTEXT_ADDRESS, "Address Area");
	GetMouseManager()->SetCommandForDblClk(IDR_CONTEXT_ADDRESS, ID_SELECT_LINE);
	GetMouseManager()->AddView(IDR_CONTEXT_HEX, "Hex Area");
	GetMouseManager()->SetCommandForDblClk(IDR_CONTEXT_HEX, ID_MARK);
	GetMouseManager()->AddView(IDR_CONTEXT_CHAR, "Character Area");
	GetMouseManager()->SetCommandForDblClk(IDR_CONTEXT_CHAR, ID_MARK);
	GetMouseManager()->AddView(IDR_CONTEXT_HIGHLIGHT, "Highlight");
	GetMouseManager()->SetCommandForDblClk(IDR_CONTEXT_HIGHLIGHT, ID_HIGHLIGHT_SELECT);
	GetMouseManager()->AddView(IDR_CONTEXT_BOOKMARKS, "Bookmark");
	GetMouseManager()->SetCommandForDblClk(IDR_CONTEXT_BOOKMARKS, ID_BOOKMARKS_EDIT);
	GetMouseManager()->AddView(IDR_CONTEXT_OFFSET_HANDLE,   "Ruler:Offset Handle");
	GetMouseManager()->AddView(IDR_CONTEXT_GROUP_BY_HANDLE, "Ruler:Group Handle");
	GetMouseManager()->AddView(IDR_CONTEXT_ROWSIZE_HANDLE,  "Ruler:Cols Handle");
	GetMouseManager()->SetCommandForDblClk(IDR_CONTEXT_ROWSIZE_HANDLE, ID_AUTOFIT);  // default to turn on autofit
	GetMouseManager()->AddView(IDR_CONTEXT_RULER,  "Ruler");
	GetMouseManager()->SetCommandForDblClk(IDR_CONTEXT_RULER, ID_ADDR_TOGGLE);       // default to toggle hex/dec addresses in adress area and ruler

	GetContextMenuManager()->AddMenu(_T("Address Area"), IDR_CONTEXT_ADDRESS);
	GetContextMenuManager()->AddMenu(_T("Hex Area"), IDR_CONTEXT_HEX);
	GetContextMenuManager()->AddMenu(_T("Character Area"), IDR_CONTEXT_CHAR);
	GetContextMenuManager()->AddMenu(_T("Aerial View"), IDR_CONTEXT_AERIAL);
	GetContextMenuManager()->AddMenu(_T("Preview View"), IDR_CONTEXT_PREVW);
	GetContextMenuManager()->AddMenu(_T("Compare View"), IDR_CONTEXT_COMPARE);
	GetContextMenuManager()->AddMenu(_T("Highlight"), IDR_CONTEXT_HIGHLIGHT);
	GetContextMenuManager()->AddMenu(_T("Bookmarks"), IDR_CONTEXT_BOOKMARKS);
	GetContextMenuManager()->AddMenu(_T("Selection"), IDR_CONTEXT_SELECTION);
	GetContextMenuManager()->AddMenu(_T("Status Bar"), IDR_CONTEXT_STATBAR);
	GetContextMenuManager()->AddMenu(_T("Window Tabs"), IDR_CONTEXT_TABS);
}

// Starts bg searches in all documents except the active doc (passed in pp)
void CHexEditApp::StartSearches(CHexEditDoc *pp)
{
	POSITION posn = m_pDocTemplate->GetFirstDocPosition();
	while (posn != NULL)
	{
		CHexEditDoc *pdoc = dynamic_cast<CHexEditDoc *>(m_pDocTemplate->GetNextDoc(posn));
		ASSERT(pdoc != NULL);
		if (pdoc != pp)
		{
			CHexEditView *pview = pdoc->GetBestView();
			ASSERT(pview != NULL);
			pdoc->base_addr_ = theApp.align_rel_ ? pview->GetSearchBase() : 0;
			pdoc->StartSearch();
		}
	}
}

// Starts bg searches in all documents except the active doc (passed in pp)
void CHexEditApp::StopSearches()
{
	POSITION posn = m_pDocTemplate->GetFirstDocPosition();
	while (posn != NULL)
	{
		CHexEditDoc *pdoc = dynamic_cast<CHexEditDoc *>(m_pDocTemplate->GetNextDoc(posn));
		ASSERT(pdoc != NULL);
		pdoc->StopSearch();
	}
}

void CHexEditApp::NewSearch(const unsigned char *pat, const unsigned char *mask,
							size_t len, BOOL icase, int tt, BOOL ww,
							int aa, int offset, bool align_rel)
{
	CSingleLock s2(&appdata_, TRUE);

	// Save search params for bg search to use
	if (pboyer_ != NULL) delete pboyer_;
	pboyer_ = new boyer(pat, len, mask);

	text_type_ = tt;
	icase_ = icase;
	wholeword_ = ww;
	alignment_ = aa;
	offset_ = offset;
	align_rel_ = align_rel;
}

// Get new encryption algorithm
void CHexEditApp::OnEncryptAlg()
{
	CAlgorithm dlg;
	dlg.m_alg = algorithm_;

	if (dlg.DoModal() == IDOK)
	{
		// Clear password for old algorithm
		if (algorithm_ > 0)
			crypto_.SetPassword(algorithm_-1, NULL);

		// Get new algorithm and create key based on current password
		algorithm_ = dlg.m_alg;
		ASSERT(algorithm_ == 0 || algorithm_ - 1 < (int)crypto_.GetNum());

		if (algorithm_ > 0 && !password_.IsEmpty())
		{
			// Set key based on password
			crypto_.SetPassword(algorithm_-1, password_);
		}
		else if (algorithm_ > 0 && password_.IsEmpty())
		{
			// Clear password
			crypto_.SetPassword(algorithm_-1, NULL);
		}
		SaveToMacro(km_encrypt_alg, algorithm_==0 ? INTERNAL_ALGORITHM : crypto_.GetName(algorithm_-1));
	}
}

void CHexEditApp::set_alg(const char *pp)
{
	algorithm_ = 0;             // Default to internal in case it's not found

	if (strcmp(pp, INTERNAL_ALGORITHM) != 0)
	{
		int ii;
		for (ii = 0; ii < (int)crypto_.GetNum(); ++ii)
		{
			if (strcmp(crypto_.GetName(ii), pp) == 0)
			{
				algorithm_ = ii+1;
				break;
			}
		}
		if (ii == crypto_.GetNum())
		{
			AfxMessageBox(CString("Encryption algorithm not found:\n") + pp);
			mac_error_ = 10;
			return;
		}

		//
		ASSERT(algorithm_ > 0);
		if (!password_.IsEmpty())
		{
			// Set key based on password
			crypto_.SetPassword(algorithm_-1, password_);
		}
		else if (algorithm_ > 0 && password_.IsEmpty())
		{
			// Clear password
			crypto_.SetPassword(algorithm_-1, NULL);
		}
	}
}

// Get new encryption password
void CHexEditApp::OnEncryptPassword()
{
	CPassword dlg;
	dlg.m_password = password_;
	if (dlg.DoModal() == IDOK)
	{
		password_ = dlg.m_password;
		// Create encryption key with new password
		if (algorithm_ > 0)
		{
			ASSERT(algorithm_ - 1 < (int)crypto_.GetNum());
			crypto_.SetPassword(algorithm_-1, password_);
		}
		SaveToMacro(km_encrypt_password, (const char *)password_);
	}
}

void CHexEditApp::OnEncryptClear()
{
	password_.Empty();  // Clear password
	if (algorithm_ > 0)
	{
		ASSERT(algorithm_ - 1 < (int)crypto_.GetNum());
		crypto_.SetPassword(algorithm_-1, NULL);
	}
	SaveToMacro(km_encrypt_password, "");
}

void CHexEditApp::set_password(const char *pp)
{
	password_ = pp;
	if (algorithm_ > 0)
	{
		ASSERT(algorithm_ - 1 < (int)crypto_.GetNum());
		if (password_.IsEmpty())
			crypto_.SetPassword(algorithm_-1, NULL);
		else
			crypto_.SetPassword(algorithm_-1, password_);
	}
}

bool CHexEditApp::ensure_current_alg_password()
{
	if (password_.IsEmpty())
	{
		// No password yet, so prompt for one
		CPassword dlg;
		dlg.m_password = password_;

		if (dlg.DoModal() != IDOK)
		{
			// user canceled out
			return false;
		}

		password_ = dlg.m_password;

		// Create encryption key with new password
		if (algorithm_ > 0)
		{
			ASSERT(algorithm_ - 1 < (int)crypto_.GetNum());
			crypto_.SetPassword(algorithm_ - 1, password_);
		}
	}
	else if (algorithm_ > 0 && crypto_.NeedsPassword(algorithm_ - 1))
	{
		// We have a password but somehow it has not been set for this alg
		ASSERT(algorithm_ - 1 < (int)crypto_.GetNum());
		crypto_.SetPassword(algorithm_ - 1, password_);
	}

	return true;
}

void CHexEditApp::OnUpdateEncryptClear(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(password_.IsEmpty());   // Set check if password is clear
}

void CHexEditApp::OnCompressionSettings()
{
	CCompressDlg dlg;

	dlg.m_defaultLevel   = GetProfileInt("Options", "ZlibCompressionDVLevel", 1) ? TRUE : FALSE;
	dlg.m_defaultWindow  = GetProfileInt("Options", "ZlibCompressionDVWindow", 1) ? TRUE : FALSE;
	dlg.m_defaultMemory  = GetProfileInt("Options", "ZlibCompressionDVMemory", 1) ? TRUE : FALSE;
	dlg.m_level          = GetProfileInt("Options", "ZlibCompressionLevel", 7);
	dlg.m_window         = GetProfileInt("Options", "ZlibCompressionWindow", 15);
	dlg.m_memory         = GetProfileInt("Options", "ZlibCompressionMemory", 9);
	dlg.m_sync           = GetProfileInt("Options", "ZlibCompressionSync", 0);
	dlg.m_headerType     = GetProfileInt("Options", "ZlibCompressionHeaderType", 1);
	dlg.m_strategy       = GetProfileInt("Options", "ZlibCompressionStrategy", 0);

	if (dlg.DoModal() == IDOK)
	{
		WriteProfileInt("Options", "ZlibCompressionDVLevel", dlg.m_defaultLevel);
		WriteProfileInt("Options", "ZlibCompressionDVWindow", dlg.m_defaultWindow);
		WriteProfileInt("Options", "ZlibCompressionDVMemory", dlg.m_defaultMemory);
		WriteProfileInt("Options", "ZlibCompressionLevel", dlg.m_level);
		WriteProfileInt("Options", "ZlibCompressionWindow", dlg.m_window);
		WriteProfileInt("Options", "ZlibCompressionMemory", dlg.m_memory);
		WriteProfileInt("Options", "ZlibCompressionSync", dlg.m_sync);
		WriteProfileInt("Options", "ZlibCompressionHeaderType", dlg.m_headerType);
		WriteProfileInt("Options", "ZlibCompressionStrategy", dlg.m_strategy);

		SaveToMacro(km_compress);
	}
}

// Retrieve options from .INI file/registry
void CHexEditApp::LoadOptions()
{
	switch(GetProfileInt("Options", "SaveExit", 99))
	{
	case 0:
		orig_save_exit_ = save_exit_ = FALSE;
		break;
	case 99:
		// Option not found
		OnNewUser();

		// Since save_exit_ is only saved when it changes this sets it to
		// the default (TRUE) and ensure the initial value is saved.
		orig_save_exit_ = !(save_exit_ = TRUE);
		break;
	default:
		orig_save_exit_ = save_exit_ = TRUE;
	}
	one_only_ = GetProfileInt("Options", "OneInstanceOnly", 1) ? TRUE : FALSE;
	open_restore_ = GetProfileInt("MainFrame", "Restore", 1) ? TRUE : FALSE;
	special_list_scan_ = GetProfileInt("Options", "DeviceScan", 0) ? TRUE : FALSE;
	splash_ = GetProfileInt("Options", "Splash", 1) ? TRUE : FALSE;
	tipofday_ = GetProfileInt("Tip", "StartUp", 0) ? FALSE : TRUE;      // inverted
	run_autoexec_ = GetProfileInt("Options", "RunAutoExec", 1) ? TRUE : FALSE;

	open_locn_   = GetProfileInt("Options", "OpenLocn",  1);
	open_folder_ = GetProfileString("Options", "OpenFolder");
	if (open_folder_.IsEmpty() && !::GetDataPath(open_folder_))
		open_folder_ = ::GetExePath();
	save_locn_   = GetProfileInt("Options", "SaveLocn",  0);
	save_folder_ = GetProfileString("Options", "SaveFolder");
	if (save_folder_.IsEmpty() && !::GetDataPath(save_folder_))
		save_folder_ = ::GetExePath();

#ifdef FILE_PREVIEW
	thumbnail_ = GetProfileInt("Options", "ThumbNail",  1) ? TRUE : FALSE;
	//thumb_8bit_ = GetProfileInt("Options", "ThumbNail8Bit",  0) ? TRUE : FALSE;
	thumb_frame_ = GetProfileInt("Options", "ThumbNailAllViews",  1) ? TRUE : FALSE;
	thumb_size_ = GetProfileInt("Options", "ThumbNailSize",  300);
	if (thumb_size_ < 100 || thumb_size_ > 2000) thumb_size_ = 300 ;
	thumb_zoom_ =  strtod(GetProfileString("Options", "ThumbNailZoom",  "1.5"), NULL);
	if (thumb_zoom_ < 0.5 || thumb_zoom_ > 10) thumb_zoom_ = 1.0;
	thumb_type_ = GetProfileInt("Options", "ThumbNailType", JPEG_AVERAGE);  // default to ave. jpeg
	if (thumb_type_ <= THUMB_NONE || thumb_type_ >= THUMB_LAST) thumb_type_ = JPEG_AVERAGE;
	cleanup_days_ = GetProfileInt("Options", "ThumbCleanDays",  100);
#endif

	wipe_type_ = (wipe_t)GetProfileInt("Options", "WipeStrategy", 1);
	if (wipe_type_ < 0 || wipe_type_ >= WIPE_LAST)
		wipe_type_ = WIPE_GOOD;
	show_not_indexed_ = (BOOL)GetProfileInt("Options", "ShowNotIndexedAttribute",  1);
	sync_tree_ = (BOOL)GetProfileInt("Options", "SyncTreeWithExplorerFolder",  1);
	custom_explorer_menu_ = (BOOL)GetProfileInt("Options", "CustomExplorerContextMenu",  1);

	backup_        = (BOOL)GetProfileInt("Options", "CreateBackup",  0);
	backup_space_  = (BOOL)GetProfileInt("Options", "BackupIfSpace", 1);
	backup_size_   =       GetProfileInt("Options", "BackupIfLess",  0);  // 1 = 1KByte, 0 = always
	backup_prompt_ = (BOOL)GetProfileInt("Options", "BackupPrompt",  1);

	bg_search_ = GetProfileInt("Options", "BackgroundSearch", 1) ? TRUE : FALSE;
	bg_stats_ = GetProfileInt("Options", "BackgroundStats", 0) ? TRUE : FALSE;
	bg_stats_crc32_ = GetProfileInt("Options", "BackgroundStatsCRC32", 1) ? TRUE : FALSE;
	bg_stats_md5_ = GetProfileInt("Options", "BackgroundStatsMD5", 1) ? TRUE : FALSE;
	bg_stats_sha1_ = GetProfileInt("Options", "BackgroundStatsSHA1", 1) ? TRUE : FALSE;
	bg_stats_sha256_ = GetProfileInt("Options", "BackgroundStatsSHA256", 0) ? TRUE : FALSE;
	bg_stats_sha512_ = GetProfileInt("Options", "BackgroundStatsSHA512", 0) ? TRUE : FALSE;

	bg_exclude_network_ = GetProfileInt("Options", "BackgroundExcludeNetwork", 1) ? TRUE : FALSE;
	bg_exclude_removeable_ = GetProfileInt("Options", "BackgroundExcludeRemoveable", 0) ? TRUE : FALSE;
	bg_exclude_optical_ = GetProfileInt("Options", "BackgroundExcludeOptical", 1) ? TRUE : FALSE;
	bg_exclude_device_ = GetProfileInt("Options", "BackgroundExcludeDevice", 1) ? TRUE : FALSE;

	large_cursor_ = GetProfileInt("Options", "LargeCursor", 0) ? TRUE : FALSE;
	show_other_ = GetProfileInt("Options", "OtherAreaCursor", 1) ? TRUE : FALSE;

	no_recent_add_ = GetProfileInt("Options", "DontAddToRecent", 1) ? TRUE : FALSE;
	bool clear = GetProfileInt("Options", "ClearHist", 0) ? TRUE : FALSE;  // if old reg entry true then default new list sizes to zero
	max_search_hist_ = GetProfileInt("History", "MaxSearch", clear ? 0 : 48);
	max_replace_hist_ = GetProfileInt("History", "MaxReplace", clear ? 0 : 16);
	max_hex_jump_hist_ = GetProfileInt("History", "MaxHexJump", clear ? 0 : 16);
	max_dec_jump_hist_ = GetProfileInt("History", "MaxDecJump", clear ? 0 : 16);
	max_expl_dir_hist_ = GetProfileInt("History", "MaxExplorerFolders", clear ? 0 : 32);
	max_expl_filt_hist_ = GetProfileInt("History", "MaxExplorerFilters", clear ? 0 : 16);

	clear_recent_file_list_ = GetProfileInt("Options", "ClearRecentFileList", 0) ? TRUE : FALSE;
	clear_bookmarks_ = GetProfileInt("Options", "ClearBookmarks", 0) ? TRUE : FALSE;
	clear_on_exit_ = GetProfileInt("Options", "ClearOnExit", 1) ? TRUE : FALSE;

	hex_ucase_ = GetProfileInt("Options", "UpperCaseHex", 1) ? TRUE : FALSE;
	k_abbrev_ = GetProfileInt("Options", "KAbbrev", 1);
	if (k_abbrev_ < 0) k_abbrev_ = 1;

	dlg_dock_ = TRUE; //GetProfileInt("MainFrame", "DockableDialogs", 0) > 0 ? TRUE : FALSE;
	dlg_move_ = GetProfileInt("MainFrame", "FloatDialogsMove", 1) ? TRUE : FALSE;
	nice_addr_ = GetProfileInt("Options", "NiceAddresses", 1) ? TRUE : FALSE;
	sel_len_tip_ = GetProfileInt("Options", "SelLenTip", 1) ? TRUE : FALSE;
	sel_len_div2_ = GetProfileInt("Options", "SelLenDiv2", 1) ? TRUE : FALSE;
	scroll_past_ends_ = GetProfileInt("Options", "ScrollPastEnds", 1) ? TRUE : FALSE;
	autoscroll_accel_ = GetProfileInt("Options", "AutoscrollAcceleration", 10);
	if (autoscroll_accel_ < 0 || autoscroll_accel_ > 50) autoscroll_accel_ = 10;
	reverse_zoom_ = GetProfileInt("Options", "ReverseMouseWheelZoomDirn", 1) ? TRUE : FALSE;

	cont_char_ = GetProfileInt("Options", "ContinuationChar", UCODE_BLANK);
	invalid_char_ = GetProfileInt("Options", "InvalidChar", UCODE_FFFD);

	ruler_ = GetProfileInt("Options", "ShowRuler", 1) ? TRUE : FALSE;
	ruler_hex_ticks_ = GetProfileInt("Options", "RulerHexTicks", 4);
	ruler_dec_ticks_ = GetProfileInt("Options", "RulerDecTicks", 5);
	ruler_hex_nums_  = GetProfileInt("Options", "RulerHexNums", 1);
	ruler_dec_nums_  = GetProfileInt("Options", "RulerDecNums", 10);
	hl_caret_ = GetProfileInt("Options", "ShowCursorInRuler", 1) ? TRUE : FALSE;
	hl_mouse_ = GetProfileInt("Options", "ShowMouseInRuler", 1) ? TRUE : FALSE;

	intelligent_undo_ = GetProfileInt("Options", "UndoIntelligent", 0) ? TRUE : FALSE;
	undo_limit_ = GetProfileInt("Options", "UndoMerge", 5);
	cb_text_type_ = GetProfileInt("Options", "TextToClipboardAs", INT_MAX);

	char buf[2];
	if (::GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IMEASURE, buf, 2) > 0 &&
		buf[0] == '0' &&
		GetProfileString("Printer", "LeftMargin").IsEmpty() )
	{
		print_units_ = prn_unit_t(GetProfileInt("Printer", "Units", 1));
	}
	else
	{
		print_units_ = prn_unit_t(GetProfileInt("Printer", "Units", 0));
	}
	left_margin_ = atof(GetProfileString("Printer", "LeftMargin",     print_units_ ? "0.3" : "0.1"));
	right_margin_ = atof(GetProfileString("Printer", "RightMargin",   print_units_ ? "0.3" : "0.1"));
	top_margin_ = atof(GetProfileString("Printer", "TopMargin",       print_units_ ? "1.1" : "0.4"));
	bottom_margin_ = atof(GetProfileString("Printer", "BottomMargin", print_units_ ? "0.8" : "0.3"));
	print_watermark_ = GetProfileInt("Printer", "PrintWatermark", 0) != 0 ? true : false;
	watermark_ = GetProfileString("Printer", "Watermark", "CONFIDENTIAL");
	header_ = GetProfileString("Printer", "Header", "&F | | &D");     // dv = filename + date
	diff_first_header_ = GetProfileInt("Printer", "DiffFirstHeader", 0) != 0 ? true : false;
	first_header_ = GetProfileString("Printer", "FirstHeader", "&A | &X | &N");
	footer_ = GetProfileString("Printer", "Footer", " | = &P = | ");
	diff_first_footer_ = GetProfileInt("Printer", "DiffFirstFooter", 0) != 0 ? true : false;
	first_footer_ = GetProfileString("Printer", "FirstFooter", "Created: &C | Last modified: &M | Category: &G");
	even_reverse_ = GetProfileInt("Printer", "ReverseHeaderFooterOnEvenPages", 0) != 0 ? true : false;

	header_edge_ = atof(GetProfileString("Printer", "HeaderEdge",     print_units_ ? "0.5" : "0.2"));
	footer_edge_ = atof(GetProfileString("Printer", "FooterEdge",     print_units_ ? "0.3" : "0.1"));
	print_box_ = GetProfileInt("Printer", "Border", 1) != 0 ? true : false;
	print_hdr_ = GetProfileInt("Printer", "Headings", 0) != 0 ? true : false;
	spacing_ = GetProfileInt("Printer", "LineSpacing", 0);
	print_mark_ = GetProfileInt("Printer", "PrintMark", 1) != 0 ? true : false;
	print_bookmarks_ = GetProfileInt("Printer", "PrintBookmarks", 1) != 0 ? true : false;
	print_highlights_ = GetProfileInt("Printer", "PrintHighlights", 1) != 0 ? true : false;
	print_search_ = GetProfileInt("Printer", "PrintSearchOccurrences", 1) != 0 ? true : false;
	print_change_ = GetProfileInt("Printer", "PrintChangeTracking", 1) != 0 ? true : false;
	print_compare_ = GetProfileInt("Printer", "PrintCompareTracking", 1) != 0 ? true : false;
	print_sectors_ = GetProfileInt("Printer", "PrintSectors", 0) != 0 ? true : false;

	int wt_flags = GetProfileInt("Options", "WindowTabs", 1);
	switch (wt_flags & 0x3)
	{
	case 1:
		mditabs_ = TRUE;
		tabsbottom_ = FALSE;
		break;
	case 2:
		mditabs_ = TRUE;
		tabsbottom_ = TRUE;
		break;
	default:
		mditabs_ = FALSE;
		tabsbottom_ = FALSE;
		break;
	}
	tabicons_ = (wt_flags & 0x4) == 0;
	tabclose_ = (wt_flags & 0x8) == 0;
	tabcolour_ = (wt_flags & 0x10) == 0;

	aerialview_ = GetProfileInt("Options", "AerialView", 0);
	aerial_disp_state_ = GetProfileInt("Options", "AerialDisplay", 0x000010B1F);
	auto_aerial_zoom_ = GetProfileInt("Options", "AerialAutoZoom", 1) != 0 ? true : false;   // xxx check box in opt dlg
	aerial_max_ = GetProfileInt("Aerial", "MaxBitmapInMbytes", 256);
	if (aerial_max_ < 16) aerial_max_ = 16; else if (aerial_max_ > 999) aerial_max_ = 999;
	aerial_max_ *= 1024*1024;

	compview_ = GetProfileInt("Options", "CompareView", 0);

	prevwview_ = GetProfileInt("Options", "PrevwView", 0);

	dffdview_ = GetProfileInt("DataFormat", "TreeView", 0);
	max_fix_for_elts_ = GetProfileInt("DataFormat", "MaxFixForElts", 20);
	alt_data_bg_cols_ = GetProfileInt("DataFormat", "AltDataBgCols", 1) != 0 ? true : false;

	default_char_format_ = GetProfileString("DataFormat", "CharDefault", "'%c'");
	default_string_format_ = GetProfileString("DataFormat", "StringDefault", "\"%s\"");
	default_int_format_ = GetProfileString("DataFormat", "IntDefault", "%d");
	default_unsigned_format_ = GetProfileString("DataFormat", "UnsignedDefault", "%u");
	default_real_format_ = GetProfileString("DataFormat", "RealDefault", "%g");
	default_date_format_ = GetProfileString("DataFormat", "DateDefault", "%c");


	xml_dir_ = GetProfileString("DataFormat", "Folder");
	if (xml_dir_.IsEmpty())
	{
		// If templates in data folder use it else use hexedit .exe directory
		CString ss; ::GetDataPath(ss);
		CFileFind ff;
		if (!ss.IsEmpty() && ff.FindFile(ss + "*.xml"))
			xml_dir_ = ss;
		else
			xml_dir_ = GetExePath();
	}
//    if (xml_dir_.Right(1) != "\\")
//        xml_dir_ += "\\";

	mac_dir_ = GetProfileString("MacroOptions", "Folder");
	if (mac_dir_.IsEmpty())
	{
		// If macros in data folder use it else use hexedit .exe directory
		CString ss; ::GetDataPath(ss);
		CFileFind ff;
		if (!ss.IsEmpty() && ff.FindFile(ss + "*.hem"))
			mac_dir_ = ss;
		else
			mac_dir_ = GetExePath();
	}
//    if (mac_dir_.Right(1) != "\\")
//        mac_dir_ += "\\";

	refresh_ = GetProfileInt("MacroOptions", "Refresh", 1);
	num_secs_ = GetProfileInt("MacroOptions", "Seconds", 5);
	num_keys_ = GetProfileInt("MacroOptions", "Keys", 1);
	num_plays_ = GetProfileInt("MacroOptions", "Plays", 1);
	refresh_bars_ = GetProfileInt("MacroOptions", "StatusBarUpdate", 1) ? TRUE : FALSE;
	refresh_props_ = GetProfileInt("MacroOptions", "PropertiesUpdate", 0) ? TRUE : FALSE;
	halt_level_ = GetProfileInt("MacroOptions", "ErrorHaltLevel", 1);
	plays_ = GetProfileInt("MacroOptions", "NumPlays", 1);

	open_max_ = GetProfileInt("Options", "OpenMax", 1) ? TRUE : FALSE;
	open_disp_state_ = GetProfileInt("Options", "OpenDisplayOptions", -1);
	if (open_disp_state_ == -1)
	{
		open_disp_state_ = 0;  // all options off

		// The following block of code is usually redundant as we are saving all state info
		// in open_disp_state_ but this is here in case of old registry entries.
		{
			open_display_.hex_area = GetProfileInt("Options", "OpenDisplayHex", 1) ? TRUE : FALSE;
			open_display_.char_area = GetProfileInt("Options", "OpenDisplayChar", 1) ? TRUE : FALSE;

			if (GetProfileInt("Options", "OpenCodePage", 1) != 0)
				open_display_.char_set = CHARSET_CODEPAGE;
			else if (GetProfileInt("Options", "OpenEBCDIC", 0) != 0)
				open_display_.char_set = CHARSET_EBCDIC;
			else if (GetProfileInt("Options", "OpenGraphicChars", 0) == 0)  // no graphics means ASCII
				open_display_.char_set = CHARSET_ASCII;
			else if (GetProfileInt("Options", "OpenOemChars", 0) != 0)
				open_display_.char_set = CHARSET_OEM;
			else
				open_display_.char_set = CHARSET_ANSI;

			open_display_.control = GetProfileInt("Options", "OpenControlChars", 0);

			open_display_.autofit = GetProfileInt("Options", "OpenAutoFit", 0) ? TRUE : FALSE;
			open_display_.dec_addr = GetProfileInt("Options", "OpenDecimalAddresses", 0) ? TRUE : FALSE;
			open_display_.hex_addr = !(open_display_.decimal_addr = open_display_.dec_addr); // don't change this - see above
			open_display_.hide_highlight = GetProfileInt("Options", "OpenHideHighlight", 0) ? TRUE : FALSE;
			open_display_.hide_bookmarks = GetProfileInt("Options", "OpenHideBookmarks", 0) ? TRUE : FALSE;

			open_display_.hide_replace = GetProfileInt("Options", "OpenHideReplace", 0) ? TRUE : FALSE;
			open_display_.hide_insert = GetProfileInt("Options", "OpenHideInsert", 0) ? TRUE : FALSE;
			open_display_.hide_delete = GetProfileInt("Options", "OpenHideDelete", 0) ? TRUE : FALSE;
			open_display_.delete_count = GetProfileInt("Options", "OpenDeleteCount", 1) ? TRUE : FALSE;

			open_display_.readonly = GetProfileInt("Options", "OpenAllowMods", 0) ? FALSE : TRUE; // reverse of reg value!
			open_display_.overtype = GetProfileInt("Options", "OpenInsert", 0) ? FALSE : TRUE;    // reverse of reg value!

			open_display_.big_endian = GetProfileInt("Options", "OpenBigEndian", 0) ? TRUE : FALSE;

			// Don't add any more DISPLAY flags here
		}

		// Save back now in new entry in case we crash (and are left with no registry settings)
		WriteProfileInt("Options", "OpenDisplayOptions", open_disp_state_);

		// Clear out reg entries to save reg space and to avoid confusing user by leaving unused entries around
		WriteProfileString("Options", "OpenDisplayHex", NULL);
		WriteProfileString("Options", "OpenDisplayChar", NULL);
		WriteProfileString("Options", "OpenGraphicChars", NULL);
		WriteProfileString("Options", "OpenOemChars", NULL);
		WriteProfileString("Options", "OpenEBCDIC", NULL);
		WriteProfileString("Options", "OpenControlChars", NULL);

		WriteProfileString("Options", "OpenAutoFit", NULL);
		WriteProfileString("Options", "OpenDecimalAddresses", NULL);
		WriteProfileString("Options", "OpenHideHighlight", NULL);
		WriteProfileString("Options", "OpenHideBookmarks", NULL);

		WriteProfileString("Options", "OpenHideReplace", NULL);
		WriteProfileString("Options", "OpenHideInsert", NULL);
		WriteProfileString("Options", "OpenHideDelete", NULL);
		WriteProfileString("Options", "OpenDeleteCount", NULL);

		WriteProfileString("Options", "OpenAllowMods", NULL);
		WriteProfileString("Options", "OpenInsert", NULL);

		WriteProfileString("Options", "OpenBigEndian", NULL);
	}
	if (!open_display_.char_area) open_display_.hex_area = TRUE;  // We need to display one or the other (or both)
	if (!open_display_.hex_area) open_display_.edit_char = open_display_.mark_char = TRUE;

	// Make sure char_set values are valid
	// Note: this will be removed later when we support more char sets (Unicode and code page char sets)
	if (open_display_.char_set == 2)
		open_display_.char_set = CHARSET_ASCII;       // ASCII:  0/2 -> 0
	//else if (open_display_.char_set > 4)
	//	open_display_.char_set = CHARSET_EBCDIC;      // EBCDIC: 4/5/6/7 -> 4
	open_code_page_ = GetProfileInt("Options", "OpenCodePage", 1252);

	CString strFont = GetProfileString("Options", "OpenFont", "Courier,16"); // Font info string (fields are comma sep.)
	CString strFace;                                            // Font FaceName from string
	CString strHeight;                                          // Font height as string
	AfxExtractSubString(strFace, strFont, 0, ',');
	AfxExtractSubString(strHeight, strFont, 1, ',');
	if (!strFace.IsEmpty())
	{
		open_plf_ = new LOGFONT;
		memset((void *)open_plf_, '\0', sizeof(*open_plf_));
		strncpy(open_plf_->lfFaceName, strFace, LF_FACESIZE-1);
		open_plf_->lfFaceName[LF_FACESIZE-1] = '\0';
		open_plf_->lfHeight = atol(strHeight);
		if (open_plf_->lfHeight < 2 || open_plf_->lfHeight > 100)
			open_plf_->lfHeight = 16;
		open_plf_->lfCharSet = ANSI_CHARSET;           // Only allow ANSI character set fonts
	}
	else
		open_plf_ = NULL;

	strFont = GetProfileString("Options", "OpenOemFont", "Terminal,18");     // Font info for oem font
	AfxExtractSubString(strFace, strFont, 0, ',');
	AfxExtractSubString(strHeight, strFont, 1, ',');
	if (!strFace.IsEmpty())
	{
		open_oem_plf_ = new LOGFONT;
		memset((void *)open_oem_plf_, '\0', sizeof(*open_oem_plf_));
		strncpy(open_oem_plf_->lfFaceName, strFace, LF_FACESIZE-1);
		open_oem_plf_->lfFaceName[LF_FACESIZE-1] = '\0';
		open_oem_plf_->lfHeight = atol(strHeight);
		if (open_oem_plf_->lfHeight < 2 || open_oem_plf_->lfHeight > 100)
			open_oem_plf_->lfHeight = 18;
		open_oem_plf_->lfCharSet = OEM_CHARSET;            // Only allow OEM/IBM character set fonts
	}
	else
		open_oem_plf_ = NULL;

	strFont = GetProfileString("Options", "OpenMultibyteFont", "Lucida Sans Unicode,18");     // Font info for CodePage/Unicode font
	AfxExtractSubString(strFace, strFont, 0, ',');
	AfxExtractSubString(strHeight, strFont, 1, ',');
	if (!strFace.IsEmpty())
	{
		open_mb_plf_ = new LOGFONT;
		memset((void *)open_mb_plf_, '\0', sizeof(*open_mb_plf_));
		strncpy(open_mb_plf_->lfFaceName, strFace, LF_FACESIZE-1);
		open_mb_plf_->lfFaceName[LF_FACESIZE-1] = '\0';
		open_mb_plf_->lfHeight = atol(strHeight);
		if (open_mb_plf_->lfHeight < 2 || open_mb_plf_->lfHeight > 100)
			open_mb_plf_->lfHeight = 18;
	}
	else
		open_mb_plf_ = NULL;

	open_rowsize_ = GetProfileInt("Options", "OpenColumns", 16);
	if (open_rowsize_ < 4 || open_rowsize_ > CHexEditView::max_buf) open_rowsize_ = 4;
	open_group_by_ = GetProfileInt("Options", "OpenGrouping", 4);
	if (open_group_by_ < 2) open_group_by_ = 2;
	open_offset_ = GetProfileInt("Options", "OpenOffset", 0);
	if (open_offset_ < 0 || open_offset_ >= open_rowsize_) open_offset_ = 0;
	open_vertbuffer_ = GetProfileInt("Options", "OpenScrollZone", 0);
	if (open_vertbuffer_ < 0) open_vertbuffer_ = 0;

	open_scheme_name_ = GetProfileString("Options", "OpenScheme");

	open_keep_times_ = GetProfileInt("Options", "OpenKeepTimes", 0) ? TRUE : FALSE;

	LoadSchemes();

	// Always default back to little-endian for decimal & fp pages
	prop_dec_endian_ = FALSE;
	prop_fp_endian_ = FALSE;
	//prop_ibmfp_endian_ = TRUE;
	prop_date_endian_ = FALSE;

	password_mask_ = GetProfileInt("Options", "PasswordMask", 1) ? TRUE : FALSE;
	password_min_ = GetProfileInt("Options", "PasswordMinLength", 8);

	// Last settings for property sheet
	prop_page_ = GetProfileInt("Property-Settings", "PropPage", 0);
	prop_dec_signed_ = GetProfileInt("Property-Settings", "DecFormat", 1);
	prop_fp_format_  = GetProfileInt("Property-Settings", "FPFormat", 1);
	//prop_ibmfp_format_  = GetProfileInt("Property-Settings", "IBMFPFormat", 1);
	prop_date_format_  = GetProfileInt("Property-Settings", "DateFormat", 0);

	// Restore default file dialog directories
	last_open_folder_ = GetProfileString("File-Settings", "DirOpen");
	last_save_folder_ = GetProfileString("File-Settings", "DirSave");
	last_both_folder_ = GetProfileString("File-Settings", "DirBoth");
	//open_file_readonly_ = GetProfileInt("File-Settings", "OpenReadOnly", 0);
	//open_file_shared_   = GetProfileInt("File-Settings", "OpenShareable", 0);
//    current_save_ = GetProfileString("File-Settings", "Save");
	current_write_ = GetProfileString("File-Settings", "Write");
	current_read_ = GetProfileString("File-Settings", "Read");
//    current_append_ = GetProfileString("File-Settings", "Append");
	current_export_ = GetProfileString("File-Settings", "Export");
	export_base_addr_ = GetProfileInt("File-Settings", "ExportBaseAddress", 0);
	export_line_len_ = GetProfileInt("File-Settings", "ExportLineLen", 32);
	current_import_ = GetProfileString("File-Settings", "Import");
	import_discon_ = GetProfileInt("File-Settings", "ImportDiscontiguous", 0);
	import_highlight_ = GetProfileInt("File-Settings", "ImportHighlight", 0);
	recent_files_ = GetProfileInt("File-Settings", "RecentFileList", 8);

	current_filters_ = GetProfileString("File-Settings", "Filters",
						_T(
						   "All Files (*.*)|*.*|"
						   "Executable files (.exe;.dll;.ocx)|*.exe;*.dll;*.ocx|"
						   "Text files (.txt;.asc;read*)|>*.txt;*.asc;read*|"
						   "INI files (.ini)|>*.ini|"
						   "Batch files (.bat;.cmd)|>*.bat;*.cmd|"
						   "EBCDIC files (.cbl;.cob;.cpy;.ddl;.bms)|>*.cbl;*.cob;*.cpy;*.ddl;*.bms|"
						   "Bitmap image files (.bmp;.dib)|>*.bmp;*.dib|"
						   "Internet image files (.gif;.jpg;.jpeg;.png)|>*.gif;*.jpg;*.jpeg;*.png|"
						   "Windows image files (.ico;.cur;.wmf)|>*.ico;*.cur;*.wmf|"
						   "Other image files (.tif,.pcx)|>*.tif;*.pcx|"
						   "All image file formats|*.bmp;*.dib;*.gif;*.jpg;*.jpeg;*.png;*.ico;*.cur;*.wmf;*.tif;*.pcx|"
						   "Postscript files (.eps;.ps;.prn)|*.eps;*.ps;*.prn|"
						   "MS Word files (.doc;.dot;.rtf)|*.doc;*.dot;*.rtf|"
						   "Windows Write (.wri)|>*.wri|"
						   "Rich Text Format (.rtf)|>*.rtf|"
						   "MS Excel files (.xl*)|>*.xl*|"
						   "MS Access databases (.mdb;.mda;.mdw;.mde)|*.mdb;*.mdw;*.mde;*.mda|"
						   "Resource files (.rc;.rct;.res)|>*.rc;*.rct;*.res|"
						   "HTML files (.htm;.html;.?html;.asp;.css;.php;.php?)|>*.htm;*.html;*.?html;*.asp;*.css;*.php;*.php?|"
						   "XML files (.xml)|>*.xml|"
						"|"));

	// Get settings for info tip window
	tip_transparency_ = GetProfileInt("Options", "InfoTipTransparency", 200);
	tip_offset_.cx = GetProfileInt("Options", "InfoTipXOffset", 16);
	tip_offset_.cy = GetProfileInt("Options", "InfoTipYOffset", 16);

	// get flags which say which hard-coded info is enabled (currently only option is bookmarks)
	int hard = GetProfileInt("Options", "InfoTipFlags", 0);  // if bit is zero option is in use
	tip_name_.push_back("Bookmarks");
	tip_on_.push_back( (hard&0x1) == 0);
	tip_expr_.push_back("");
	tip_format_.push_back("");

	ASSERT(tip_name_.size() == FIRST_USER_TIP);

	// Next get user specified info lines. Each line has 4 fields:
	//   1. name       - name shown to user - if preceded by '>' then this line is disabled (not shown)
	//   2. expression - evaluated expression involving special symbols such as "address", "byte", etc
	//   3. format     - how the result of the expression is display
	//   4. unused     - for future use (possibly colour and other options)
	// Symbols: address, sector, offset, byte, sbyte, word, uword, dword, udword, qword, uqword,
	//          ieee32, ieee64, ibm32, ibm64, time_t, time_t_80, time_t_1899, time_t_mins, time64_t
	CString ss = GetProfileString("Options", "InfoTipUser",
					">Address;address;;;"
					">Hex Address;address;hex;;"
					">Dec Address;address;dec;;"
					">Offset Address;address - offset;;;"
					">Offset Address;address - offset;hex;;"
					">Offset Address;address - offset;dec;;"
					">From Mark;address - mark;;;"
					">From Mark;address - mark;hex;;"
					">From Mark;address - mark;dec;;"
					">Hex Sector;sector;hex;;"
					">Sector;sector;dec;;"
					">ASCII char;byte;%c;;"
					">Bits     ;byte;bin;;"
					">High Bit ;(byte&0x80)!=0;OFF;;"
					">Dec Byte;byte;dec;;"
					">Sign Byte;sbyte;dec;;"
					">Octal Byte;byte;oct;;"
					">Word;word;dec;;"
					">Word Bits;uword;bin;;"
					">Unsigned Word;uword;dec;;"
					">Double Word;dword;dec;;"
					">Unsigned DWord;udword;dec;;"
					">Quad Word;qword;dec;;"
					">32 bit IEEE float;ieee32;%.7g;;"
					">64 bit IEEE float;ieee64;%.15g;;"
					">32 bit IBM float;ibm32;%.7g;;"
					">64 bit IBM float;ibm64;%.16g;;"
					">Date/time;time_t;%c;;"
#ifdef TIME64_T
					">64 bit date/time;time64_t;%c;;"
#endif
					">time_t MSC 5.1;time_t_80;%c;;"
					">time_t MSC 7;time_t_1899;%c;;"
					">time_t MINS;time_t_mins;%c;;"
					">ANSI string;astring;%s;;"
					// TODO - also handle EBCDIC and Unicode strings
					">To EOF;eof - address;dec;;"
					";;");


	for (int ii = 0; ; ii += 4)
	{
		CString name, expr, format;
		AfxExtractSubString(name,   ss, ii,   ';');
		AfxExtractSubString(expr,   ss, ii+1, ';');
		if (name.IsEmpty() && expr.IsEmpty())
			break;
		AfxExtractSubString(format, ss, ii+2, ';');
		bool on = true;
		if (name[0] == '>')
		{
			on = false;
			name = name.Mid(1);
		}
		tip_name_.push_back(name);
		tip_on_.push_back(on);
		tip_expr_.push_back(expr);
		tip_format_.push_back(format);
	}
}

// Save global options to .INI file/registry
void CHexEditApp::SaveOptions()
{
	CString ss;

	// Save general options
	WriteProfileInt("Options", "SaveExit", save_exit_ ? 1 : 0);
	WriteProfileInt("Options", "OneInstanceOnly", one_only_ ? 1 : 0);
	WriteProfileInt("Options", "DeviceScan", special_list_scan_ ? 1 : 0);
	WriteProfileInt("Options", "Splash", splash_ ? 1 : 0);
	WriteProfileInt("Tip", "StartUp", tipofday_ ? 0 : 1);   // inverted
	WriteProfileInt("Options", "RunAutoExec", run_autoexec_ ? 1 : 0);

	WriteProfileInt("Options", "OpenLocn", open_locn_);
	WriteProfileString("Options", "OpenFolder", open_folder_);
	WriteProfileInt("Options", "SaveLocn", save_locn_);
	WriteProfileString("Options", "SaveFolder", save_folder_);

#ifdef FILE_PREVIEW
	WriteProfileInt("Options", "ThumbNail", thumbnail_ ? 1 : 0);
	//WriteProfileInt("Options", "ThumbNail8Bit", thumb_8bit_ ? 1 : 0);
	WriteProfileInt("Options", "ThumbNailAllViews", thumb_frame_ ? 1 : 0);
	WriteProfileInt("Options", "ThumbNailSize", thumb_size_);
	ss.Format("%g", thumb_zoom_);
	WriteProfileString("Options", "ThumbNailZoom", ss);
	WriteProfileInt("Options", "ThumbNailType", thumb_type_);
	WriteProfileInt("Options", "ThumbCleanDays", cleanup_days_);
#endif

	WriteProfileInt("Options", "WipeStrategy", wipe_type_);
	WriteProfileInt("Options", "ShowNotIndexedAttribute", show_not_indexed_ ? 1 : 0);
	WriteProfileInt("Options", "SyncTreeWithExplorerFolder", sync_tree_ ? 1 : 0);
	WriteProfileInt("Options", "CustomExplorerContextMenu", custom_explorer_menu_ ? 1 : 0);

	//WriteProfileInt("MainFrame", "DockableDialogs", dlg_dock_ ? 1 : 0);
	WriteProfileInt("MainFrame", "FloatDialogsMove", dlg_move_ ? 1 : 0);
	WriteProfileInt("Options", "UpperCaseHex", hex_ucase_ ? 1 : 0);
	WriteProfileInt("Options", "KAbbrev", k_abbrev_);

	WriteProfileInt("Options", "NiceAddresses", nice_addr_ ? 1 : 0);
	WriteProfileInt("Options", "SelLenTip", sel_len_tip_ ? 1 : 0);
	WriteProfileInt("Options", "SelLenDiv2", sel_len_div2_ ? 1 : 0);
	WriteProfileInt("Options", "ScrollPastEnds", scroll_past_ends_ ? 1 : 0);
	WriteProfileInt("Options", "AutoscrollAcceleration", autoscroll_accel_);
	WriteProfileInt("Options", "ReverseMouseWheelZoomDirn", reverse_zoom_ ? 1 : 0);

	WriteProfileInt("Options", "ContinuationChar", cont_char_);
	WriteProfileInt("Options", "InvalidChar", invalid_char_);

	WriteProfileInt("Options", "ShowRuler", ruler_ ? 1 : 0);
	WriteProfileInt("Options", "RulerHexTicks", ruler_hex_ticks_);
	WriteProfileInt("Options", "RulerDecTicks", ruler_dec_ticks_);
	WriteProfileInt("Options", "RulerHexNums", ruler_hex_nums_);
	WriteProfileInt("Options", "RulerDecNums", ruler_dec_nums_);
	WriteProfileInt("Options", "ShowCursorInRuler", hl_caret_ ? 1 : 0);
	WriteProfileInt("Options", "ShowMouseInRuler", hl_mouse_ ? 1 : 0);

	WriteProfileInt("Options", "CreateBackup", backup_);
	WriteProfileInt("Options", "BackupIfSpace", int(backup_space_));
	WriteProfileInt("Options", "BackupIfLess", backup_size_);
	WriteProfileInt("Options", "BackupPrompt", int(backup_prompt_));
	WriteProfileInt("Options", "BackgroundSearch", bg_search_ ? 1 : 0);
	WriteProfileInt("Options", "BackgroundStats", bg_stats_ ? 1 : 0);
	WriteProfileInt("Options", "BackgroundStatsCRC32", bg_stats_crc32_ ? 1 : 0);
	WriteProfileInt("Options", "BackgroundStatsMD5", bg_stats_md5_ ? 1 : 0);
	WriteProfileInt("Options", "BackgroundStatsSHA1", bg_stats_sha1_ ? 1 : 0);
	WriteProfileInt("Options", "BackgroundStatsSHA256", bg_stats_sha256_ ? 1 : 0);
	WriteProfileInt("Options", "BackgroundStatsSHA512", bg_stats_sha512_ ? 1 : 0);
	WriteProfileInt("Options", "BackgroundExcludeNetwork", bg_exclude_network_ ? 1 : 0);
	WriteProfileInt("Options", "BackgroundExcludeRemoveable", bg_exclude_removeable_ ? 1 : 0);
	WriteProfileInt("Options", "BackgroundExcludeOptical", bg_exclude_optical_ ? 1 : 0);
	WriteProfileInt("Options", "BackgroundExcludeDevice", bg_exclude_device_ ? 1 : 0);
	WriteProfileInt("Options", "LargeCursor", large_cursor_ ? 1 : 0);
	WriteProfileInt("Options", "OtherAreaCursor", show_other_ ? 1 : 0);


	WriteProfileInt("Options", "DontAddToRecent", no_recent_add_ ? 1 : 0);
	WriteProfileInt("History", "MaxSearch", max_search_hist_);
	WriteProfileInt("History", "MaxReplace", max_replace_hist_);
	WriteProfileInt("History", "MaxHexJump", max_hex_jump_hist_);
	WriteProfileInt("History", "MaxDecJump", max_dec_jump_hist_);
	WriteProfileInt("History", "MaxExplorerFolders", max_expl_dir_hist_);
	WriteProfileInt("History", "MaxExplorerFilters", max_expl_filt_hist_);
	WriteProfileInt("Options", "ClearRecentFileList", clear_recent_file_list_ ? 1 : 0);
	WriteProfileInt("Options", "ClearBookmarks", clear_bookmarks_ ? 1 : 0);
	// ClearHist has been replaced by MaxSearch etc being set to zero
	//WriteProfileInt("Options", "ClearHist", clear_hist_ ? 1 : 0);
	WriteProfileInt("Options", "ClearOnExit", clear_on_exit_ ? 1 : 0);

	WriteProfileInt("Options", "UndoIntelligent", intelligent_undo_ ? 1 : 0);
	WriteProfileInt("Options", "UndoMerge", undo_limit_);
	WriteProfileInt("Options", "TextToClipboardAs", cb_text_type_);

	WriteProfileInt("Printer", "Border", print_box_ ? 1 : 0);
	WriteProfileInt("Printer", "Headings", print_hdr_ ? 1 : 0);
	WriteProfileInt("Printer", "PrintMark", print_mark_ ? 1 : 0);
	WriteProfileInt("Printer", "PrintBookmarks", print_bookmarks_ ? 1 : 0);
	WriteProfileInt("Printer", "PrintHighlights", print_highlights_ ? 1 : 0);
	WriteProfileInt("Printer", "PrintSearchOccurrences", print_search_ ? 1 : 0);
	WriteProfileInt("Printer", "PrintChangeTracking", print_change_ ? 1 : 0);
	WriteProfileInt("Printer", "PrintCompareTracking", print_compare_ ? 1 : 0);
	WriteProfileInt("Printer", "PrintSectors", print_sectors_ ? 1 : 0);
	WriteProfileInt("Printer", "Units", int(print_units_));
	ss.Format("%g", left_margin_);
	WriteProfileString("Printer", "LeftMargin", ss);
	ss.Format("%g", right_margin_);
	WriteProfileString("Printer", "RightMargin", ss);
	ss.Format("%g", top_margin_);
	WriteProfileString("Printer", "TopMargin", ss);
	ss.Format("%g", bottom_margin_);
	WriteProfileString("Printer", "BottomMargin", ss);
	WriteProfileInt("Printer", "LineSpacing", spacing_);

	WriteProfileInt("Printer", "PrintWatermark", print_watermark_ ? 1 : 0);
	WriteProfileString("Printer", "Watermark", watermark_);
	WriteProfileString("Printer", "Header", header_);
	WriteProfileInt("Printer", "DiffFirstHeader", diff_first_header_ ? 1 : 0);
	WriteProfileString("Printer", "FirstHeader", first_header_);
	WriteProfileString("Printer", "Footer", footer_);
	WriteProfileInt("Printer", "DiffFirstFooter", diff_first_footer_ ? 1 : 0);
	WriteProfileString("Printer", "FirstFooter", first_footer_);
	WriteProfileInt("Printer", "ReverseHeaderFooterOnEvenPages", even_reverse_ ? 1 : 0);
	ss.Format("%g", header_edge_);
	WriteProfileString("Printer", "HeaderEdge", ss);
	ss.Format("%g", footer_edge_);
	WriteProfileString("Printer", "FooterEdge", ss);

	WriteProfileInt("Options", "WindowTabs", !mditabs_ ? 0 : 
					(tabsbottom_ ? 2 : 1) | (!tabicons_ ? 4 : 0) | (!tabclose_ ? 8 : 0) | (!tabcolour_ ? 0x10 : 0));
	CMainFrame *mm = (CMainFrame *)AfxGetMainWnd();
	if (mm != NULL)
		mm->SaveFrameOptions();

	WriteProfileInt("Options", "AerialView", aerialview_);
	WriteProfileInt("Options", "AerialDisplay", aerial_disp_state_);
	WriteProfileInt("Options", "AerialAutoZoom", auto_aerial_zoom_);
	WriteProfileInt("Aerial", "MaxBitmapInMbytes", aerial_max_/(1024*1024));

	WriteProfileInt("Options", "CompareView", compview_);

	WriteProfileInt("Options", "PrevwView", prevwview_);

	// Save data format view options
	WriteProfileInt("DataFormat", "TreeView", dffdview_);
	WriteProfileInt("DataFormat", "MaxFixForElts", max_fix_for_elts_);
	WriteProfileInt("DataFormat", "AltDataBgCols", alt_data_bg_cols_ ? 1 : 0);
	ASSERT(xml_dir_.Right(1) == "\\");

	WriteProfileString("DataFormat", "CharDefault", default_char_format_);
	WriteProfileString("DataFormat", "StringDefault", default_string_format_);
	WriteProfileString("DataFormat", "IntDefault", default_int_format_);
	WriteProfileString("DataFormat", "UnsignedDefault", default_unsigned_format_);
	WriteProfileString("DataFormat", "RealDefault", default_real_format_);
	WriteProfileString("DataFormat", "DateDefault", default_date_format_);
	WriteProfileString("DataFormat", "Folder", xml_dir_);

	// Save macro options
	ASSERT(mac_dir_.Right(1) == "\\");
	WriteProfileString("MacroOptions", "Folder", mac_dir_);
	WriteProfileInt("MacroOptions", "Refresh", refresh_);
	WriteProfileInt("MacroOptions", "Seconds", num_secs_);
	WriteProfileInt("MacroOptions", "Keys", num_keys_);
	WriteProfileInt("MacroOptions", "Plays", num_plays_);
	WriteProfileInt("MacroOptions", "StatusBarUpdate", refresh_bars_ ? 1 : 0);
	WriteProfileInt("MacroOptions", "PropertiesUpdate", refresh_props_ ? 1 : 0);
	WriteProfileInt("MacroOptions", "ErrorHaltLevel", halt_level_);
	WriteProfileInt("MacroOptions", "NumPlays", plays_);

	// Save default window options
	WriteProfileInt("Options", "OpenMax", open_max_ ? 1 : 0);

	WriteProfileInt("Options", "OpenDisplayOptions", open_disp_state_);
	//WriteProfileInt("Options", "OpenDisplayHex", open_display_.hex_area ? 1 : 0);
	//WriteProfileInt("Options", "OpenDisplayChar", open_display_.char_area ? 1 : 0);
	//WriteProfileInt("Options", "OpenGraphicChars", open_display_.graphic ? 1 : 0);
	//WriteProfileInt("Options", "OpenOemChars", open_display_.oem ? 1 : 0);
	//WriteProfileInt("Options", "OpenEBCDIC", open_display_.ebcdic ? 1 : 0);
	//WriteProfileInt("Options", "OpenControlChars", open_display_.control);

	//WriteProfileInt("Options", "OpenAutoFit", open_display_.autofit ? 1 : 0);
	//WriteProfileInt("Options", "OpenDecimalAddresses", open_display_.dec_addr ? 1 : 0);
	//WriteProfileInt("Options", "OpenHideHighlight", open_display_.hide_highlight ? 1 : 0);
	//WriteProfileInt("Options", "OpenHideBookmarks", open_display_.hide_bookmarks ? 1 : 0);

	//WriteProfileInt("Options", "OpenHideReplace", open_display_.hide_replace ? 1 : 0);
	//WriteProfileInt("Options", "OpenHideInsert", open_display_.hide_insert ? 1 : 0);
	//WriteProfileInt("Options", "OpenHideDelete", open_display_.hide_delete ? 1 : 0);
	//WriteProfileInt("Options", "OpenDeleteCount", open_display_.delete_count ? 1 : 0);

	//WriteProfileInt("Options", "OpenAllowMods", open_display_.readonly ? 0 : 1); // reverse of reg value!
	//WriteProfileInt("Options", "OpenInsert", open_display_.overtype ? 0 : 1);    // reverse of reg value!

	//WriteProfileInt("Options", "OpenBigEndian", open_display_.big_endian ? 1 : 0);
	WriteProfileInt("Options", "OpenCodePage", open_code_page_);

	WriteProfileInt("Options", "OpenKeepTimes", open_keep_times_ ? 1 : 0);

	CString strFont;
	if (open_plf_ != NULL)
	{
		strFont.Format("%s,%ld", open_plf_->lfFaceName, open_plf_->lfHeight);
		WriteProfileString("Options", "OpenFont", strFont);
	}
	if (open_oem_plf_ != NULL)
	{
		strFont.Format("%s,%ld", open_oem_plf_->lfFaceName, open_oem_plf_->lfHeight);
		WriteProfileString("Options", "OpenOemFont", strFont);
	}
	if (open_mb_plf_ != NULL)
	{
		strFont.Format("%s,%ld", open_mb_plf_->lfFaceName, open_mb_plf_->lfHeight);
		WriteProfileString("Options", "OpenMultibyteFont", strFont);
	}

	WriteProfileInt("Options", "OpenColumns", open_rowsize_);
	WriteProfileInt("Options", "OpenGrouping", open_group_by_);
	WriteProfileInt("Options", "OpenOffset", open_offset_);
	WriteProfileInt("Options", "OpenScrollZone", open_vertbuffer_);

	WriteProfileString("Options", "OpenScheme", open_scheme_name_);

	// Encryption password options
	WriteProfileInt("Options", "PasswordMask", password_mask_ ? 1 : 0);
	if (password_min_ != 8)
		WriteProfileInt("Options", "PasswordMinLength", password_min_);

	SaveSchemes();

	// Save info about modeless dialogs (find and properties)
	WriteProfileInt("Property-Settings", "PropPage", prop_page_);
	WriteProfileInt("Property-Settings", "DecFormat", prop_dec_signed_);
	WriteProfileInt("Property-Settings", "FPFormat", prop_fp_format_);
	//WriteProfileInt("Property-Settings", "IBMFPFormat", prop_ibmfp_format_);
	WriteProfileInt("Property-Settings", "DateFormat", prop_date_format_);

	// Save directories for file dialogs
	WriteProfileString("File-Settings", "DirOpen", last_open_folder_);
	WriteProfileString("File-Settings", "DirSave", last_save_folder_);
	WriteProfileString("File-Settings", "DirBoth", last_both_folder_);

	//WriteProfileInt("File-Settings", "OpenReadOnly",  open_file_readonly_);
	//WriteProfileInt("File-Settings", "OpenShareable", open_file_shared_);
//    WriteProfileString("File-Settings", "Save", current_save_);
	WriteProfileString("File-Settings", "Write", current_write_);
	WriteProfileString("File-Settings", "Read", current_read_);

	WriteProfileInt("File-Settings", "RecentFileList", recent_files_);
//    WriteProfileString("File-Settings", "Append", current_append_);
	WriteProfileString("File-Settings", "Export", current_export_);
	WriteProfileInt("File-Settings", "ExportBaseAddress", export_base_addr_);
	WriteProfileInt("File-Settings", "ExportLineLen", export_line_len_);
	WriteProfileString("File-Settings", "Import", current_import_);
	WriteProfileInt("File-Settings", "ImportDiscontiguous", import_discon_);
	WriteProfileInt("File-Settings", "ImportHighlight", import_highlight_);

	WriteProfileString("File-Settings", "Filters", current_filters_);

	// Save tip (info) window options
	int hard = 0;
	CString soft;
	for (size_t ii = 0; ii < tip_name_.size(); ++ii)
	{
		if (ii < FIRST_USER_TIP)
		{
			if (!tip_on_[ii])
				hard |= (1<<ii);      // turn bit on to indicate this one is disabled
		}
		else
		{
			CString ss;
			ss.Format("%s%s;%s;%s;;",
					  tip_on_[ii] ? "" : ">",
					  tip_name_[ii],
					  tip_expr_[ii],
					  tip_format_[ii]);
			soft += ss;
		}
	}
	soft += ";;";  // mark end of list
	WriteProfileInt("Options", "InfoTipTransparency", tip_transparency_);
	WriteProfileInt("Options", "InfoTipXOffset", tip_offset_.cx);
	WriteProfileInt("Options", "InfoTipYOffset", tip_offset_.cy);
	WriteProfileInt("Options", "InfoTipFlags", hard);
	WriteProfileString("Options", "InfoTipUser", soft);
}

void CHexEditApp::LoadSchemes()
{
	scheme_.clear();

	// Get the number of schemes
	int num_schemes = GetProfileInt("Options", "NumberOfSchemes", 0);

	// For each scheme
	bool multi_found = false;
	for (int ii = 0; ii < num_schemes; ++ii)
	{
		CString strKey;

		strKey.Format("Scheme%d", ii+1);

		// get name, and normal colours (bg etc)
		CString scheme_name = GetProfileString(strKey, "Name", "");
		if (scheme_name.IsEmpty())
			break;

		ASSERT(ii != 0 || scheme_name == ASCII_NAME);
		ASSERT(ii != 1 || scheme_name == ANSI_NAME);
		ASSERT(ii != 2 || scheme_name == OEM_NAME);
		ASSERT(ii != 3 || scheme_name == EBCDIC_NAME);
		ASSERT(ii != 4 || scheme_name == UNICODE_NAME);
		ASSERT(ii != 5 || scheme_name == CODEPAGE_NAME);
		if (scheme_name == MULTI_NAME)
			multi_found = true;

		CScheme scheme(scheme_name);
		scheme.bg_col_ = GetProfileInt(strKey, "BackgroundColour", -2);     // Use -2 here to detect new scheme
		bool is_new = (scheme.bg_col_ == -2);
		if (is_new) scheme.bg_col_ = -1;									// use default background for new scheme
		scheme.dec_addr_col_ = GetProfileInt(strKey, "DecAddressColour", -1);
		scheme.hex_addr_col_ = GetProfileInt(strKey, "HexAddressColour", -1);
		scheme.hi_col_ = GetProfileInt(strKey, "HiColour", -1);
		scheme.bm_col_ = GetProfileInt(strKey, "BookmarkColour", -1);
		scheme.mark_col_ = GetProfileInt(strKey, "MarkColour", -2);
		// If this is an old scheme but no has mark colour (-2) make it the same as
		// bookmark colour unless bookmark colour is default (-1) thence make it cyan.
		// (This should make the mark the same colour as in previous version for upgraders.)
		if (scheme.mark_col_ == -2) scheme.mark_col_ = is_new ? -1 : (scheme.bm_col_ == -1 ? RGB(0, 224, 224) : scheme.bm_col_);
		scheme.search_col_ = GetProfileInt(strKey, "SearchColour", -1);
		scheme.trk_col_ = GetProfileInt(strKey, "ChangeTrackingColour", -1);
		scheme.comp_col_ = GetProfileInt(strKey, "CompareColour", -1);
		scheme.addr_bg_col_ = GetProfileInt(strKey, "AddressBackgroundColour", -2);
		// Make address background same as normal background for upgraders.
		if (scheme.addr_bg_col_ == -2) scheme.addr_bg_col_ = is_new ? -1 : scheme.bg_col_;
		scheme.sector_col_ = GetProfileInt(strKey, "SectorColour", -1);

		// For all ranges
		for (int jj = 0; ; ++jj)
		{
			// Get name, colour and range
			CString name, range, ss;
			COLORREF col;

			ss.Format("Name%d", jj+1);
			name = GetProfileString(strKey, ss);
			if (name.IsEmpty())
				break;

			ss.Format("Colour%d", jj+1);
			col = (COLORREF)GetProfileInt(strKey, ss, 0);

			ss.Format("Range%d", jj+1);
			range = GetProfileString(strKey, ss, "0:255");

			scheme.AddRange(name, col, range);
		}

		scheme_.push_back(scheme);
	}

	num_schemes = scheme_.size();

	// Ensure the "standard" schemes are present
	if (num_schemes < 1 || scheme_[0].name_.Compare(ASCII_NAME) != 0)
		scheme_.insert(scheme_.begin() + 0, default_ascii_scheme_);
	if (num_schemes < 2 || scheme_[1].name_.Compare(ANSI_NAME) != 0)
		scheme_.insert(scheme_.begin() + 1, default_ansi_scheme_);
	if (num_schemes < 3 || scheme_[2].name_.Compare(OEM_NAME) != 0)
		scheme_.insert(scheme_.begin() + 2, default_oem_scheme_);
	if (num_schemes < 4 || scheme_[3].name_.Compare(EBCDIC_NAME) != 0)
		scheme_.insert(scheme_.begin() + 3, default_ebcdic_scheme_);
	if (num_schemes < 5 || scheme_[4].name_.Compare(UNICODE_NAME) != 0)
		scheme_.insert(scheme_.begin() + 4, default_unicode_scheme_);
	if (num_schemes < 6 || scheme_[5].name_.Compare(CODEPAGE_NAME) != 0)
		scheme_.insert(scheme_.begin() + 5, default_codepage_scheme_);

	// If we had to add schemes then also add our extra ones
	if (num_schemes < 4)
	{
		// At least one standard scheme was missing so it seems that this is
		// a new installation so should add the plain and multi schemes.
		CScheme new_scheme(PLAIN_NAME);
		new_scheme.AddRange("ALL", -1, "0:255");
		// Restore these to "Automatic" values which are plain greys & pastels
		new_scheme.mark_col_ = new_scheme.hex_addr_col_ = new_scheme.dec_addr_col_ = -1;
		new_scheme.addr_bg_col_  = RGB(240, 240, 240);  // Make sure this is always grey
		new_scheme.hi_col_ = RGB(255, 255, 192);
		new_scheme.sector_col_ = RGB(224, 192, 192);
		new_scheme.trk_col_ = RGB(255, 192, 96);
		new_scheme.comp_col_ = RGB(255, 128, 255);
		// new_scheme.can_delete_ = TRUE;
		scheme_.push_back(new_scheme);

/*  // Leave out rainbow scheme now that we have "Many" scheme
		CScheme new_scheme2(PRETTY_NAME);
		new_scheme2.AddRange("NullByte", RGB(254, 254, 254), "0");   // Give nul bytes their own colour (grey)
		new_scheme2.AddRange("range1", RGB(200, 0, 0), "1:21");
		new_scheme2.AddRange("range2", RGB(200, 100, 0), "22:42");
		new_scheme2.AddRange("range3", RGB(200, 200, 0), "43:63");
		new_scheme2.AddRange("range4", RGB(100, 200, 0), "64:84");
		new_scheme2.AddRange("range5", RGB(0, 200, 0), "85:105");
		new_scheme2.AddRange("range6", RGB(0, 200, 100), "106:127");
		new_scheme2.AddRange("range7", RGB(0, 200, 200), "128:148");
		new_scheme2.AddRange("range8", RGB(0, 100, 200), "149:169");
		new_scheme2.AddRange("range9", RGB(0, 0, 200), "170:191");
		new_scheme2.AddRange("range10", RGB(100, 0, 200), "192:212");
		new_scheme2.AddRange("range11", RGB(200, 0, 200), "213:233");
		new_scheme2.AddRange("range12", RGB(200, 0, 100), "234:254");
		new_scheme2.AddRange("CatchAll", -1, "0:255"); // This should only catch 0xFF
		scheme_.push_back(new_scheme2);
*/
	}
	// Add multi scheme if not found
	if (!multi_found)
		scheme_.push_back(default_multi_scheme_);
}

void CHexEditApp::SaveSchemes()
{
	std::vector<CScheme>::const_iterator ps;
	int ii, jj;                         // Loop counters
	CString ss;                         // Temp reg key name

	// Save the number of schemes
	WriteProfileInt("Options", "NumberOfSchemes", scheme_.size());

	// Save each scheme
	for (ii = 0, ps = scheme_.begin(); ps != scheme_.end(); ++ii, ++ps)
	{
		CString strKey;

		strKey.Format("Scheme%d", ii+1);

		// Save name, and normal colours
		WriteProfileString(strKey, "Name", ps->name_);
		WriteProfileInt(strKey, "BackgroundColour", ps->bg_col_);
		WriteProfileInt(strKey, "DecAddressColour", ps->dec_addr_col_);
		WriteProfileInt(strKey, "HexAddressColour", ps->hex_addr_col_);
		WriteProfileInt(strKey, "HiColour", ps->hi_col_);
		WriteProfileInt(strKey, "BookmarkColour", ps->bm_col_);
		WriteProfileInt(strKey, "MarkColour", ps->mark_col_);
		WriteProfileInt(strKey, "SearchColour", ps->search_col_);
		WriteProfileInt(strKey, "ChangeTrackingColour", ps->trk_col_);
		WriteProfileInt(strKey, "CompareColour", ps->comp_col_);
		WriteProfileInt(strKey, "AddressBackgroundColour", ps->addr_bg_col_);
		WriteProfileInt(strKey, "SectorColour", ps->sector_col_);

		// Save each range
		for (jj = 0; jj < (int)ps->range_name_.size(); ++jj)
		{
			ss.Format("Name%d", jj+1);
			WriteProfileString(strKey, ss, ps->range_name_[jj]);

			ss.Format("Colour%d", jj+1);
			WriteProfileInt(strKey, ss, ps->range_col_[jj]);

			std::ostringstream strstr;
			strstr << ps->range_val_[jj];

			ss.Format("Range%d", jj+1);
			WriteProfileString(strKey, ss, strstr.str().c_str());
		}

		// Delete entry past last one so that any old values are not used
		ss.Format("Name%d", jj+1);
		WriteProfileString(strKey, ss, NULL);
	}
}

// Get name or description of all XML files for display in drop list
void CHexEditApp::GetXMLFileList()
{
	CFileFind ff;

	xml_file_name_.clear();

	ASSERT(xml_dir_.Right(1) == "\\");
	BOOL bContinue = ff.FindFile(xml_dir_ + _T("*.XML"));

	while (bContinue)
	{
		// At least one match - check them all
		bContinue = ff.FindNextFile();

		xml_file_name_.push_back(ff.GetFileTitle());
	}
}

#if 0  // replaced by schemes
bool CHexEditApp::GetColours(const char *section, const char *key1, const char *key2,
				const char *key3, partn &retval)
{
	CString name = GetProfileString(section, key1, "");
	if (name.IsEmpty()) return false;

	COLORREF colour = (COLORREF)GetProfileInt(section, key2, 0);
	CString range = GetProfileString(section, key3, "0:255");

	retval = partn(name, colour, range);
	return true;
}

void CHexEditApp::SetColours(const char *section, const char *key1, const char *key2,
				const char *key3, const partn &v)
{
	// Write the range name and RGB value
	WriteProfileString(section, key1, v.name);
	WriteProfileInt(section, key2, v.col);

	// Convert the range itself to a string and write it
	std::ostringstream strstr;
	strstr << v.range;
	WriteProfileString(section, key3, strstr.str().c_str());
}
#endif

void CHexEditApp::OnProperties()
{
	CMainFrame *mm = (CMainFrame *)AfxGetMainWnd();

	// Save this so that we get it before the page activation "keystroke"
	// (km_prop_file, km_prop_char, etc)
	SaveToMacro(km_prop);

	mm->m_paneProp.ShowAndUnroll();
	mm->m_wndProp.SetFocus();
	mm->m_wndProp.UpdateWindow(); // Needed for when prop dlg opened in a macro
}

void CHexEditApp::OnPropertiesBitmap()
{
	CMainFrame *mm = (CMainFrame *)AfxGetMainWnd();

	SaveToMacro(km_prop_bitmap);

	mm->m_paneProp.ShowAndUnroll();
	mm->m_wndProp.SetFocus();
	if (mm->m_wndProp.SetActivePage(&mm->m_wndProp.prop_bitmap))
		mm->m_wndProp.prop_bitmap.UpdateWindow();
}

void CHexEditApp::OnOptions()
{
	display_options();
}

void CHexEditApp::OnOptions2()
{
	display_options(WIN_OPTIONS_PAGE);
}

void CHexEditApp::OnOptionsCodePage()
{
	CHexEditView *pview = GetView();
	if (pview != NULL && !pview->CodePageMode())
		pview->OnCharsetCodepage();    // switch to code page mode if not already in it
	display_options(WIN_OPTIONS_PAGE);
}

// Invoke the options dialog
// Param "display_page" indicates which page we want to show
// Param "must_show_page" means to force showing of the page
void CHexEditApp::display_options(int display_page /* = -1 */, BOOL must_show_page /*=FALSE*/)
{
	// Construct property sheet + its pages
	COptSheet optSheet(_T("HexEdit Options"), display_page, must_show_page);

	// Load current settings into the property sheet
	get_options(optSheet.val_);

	optSheet.m_psh.dwFlags &= ~(PSH_HASHELP);      // Turn off help button
	optSheet.DoModal();
	// Note: updating the options in response to DoModal returning IDOK is
	// not a good idea as then the Apply button does nothing.
}

// Get current options into member variables for most pages.
// Note: Info tips, Colours and Filters pages are handled differently
// They handle their own initialization (OnInitDialog)
void CHexEditApp::get_options(struct OptValues &val)
{
	char buf[_MAX_PATH + 3];            // Stores value of key (not used)
	long buf_size = sizeof(buf);        // Size of buffer and returned key value

	// System
	val.save_exit_ = save_exit_;
	val.shell_open_ = RegQueryValue(HKEY_CLASSES_ROOT, HexEditSubSubKey, buf, &buf_size) == ERROR_SUCCESS;
	val.one_only_ = one_only_;
	val.special_list_scan_ = special_list_scan_;
	val.splash_ = splash_;
	val.tipofday_ = tipofday_;
	val.run_autoexec_ = run_autoexec_;

	// Folder options
	val.open_locn_ = open_locn_;
	val.open_folder_ = open_folder_;
	val.save_locn_ = save_locn_;
	val.save_folder_ = save_folder_;

	// Preview options
	val.thumbnail_ = thumbnail_;
	val.thumb_frame_ = thumb_frame_;
	val.thumb_size_ = thumb_size_;
	val.thumb_type_ = thumb_type_ - 1;  // THUMB_TYPE to drop-list index
	val.thumb_zoom_ = thumb_zoom_;
	val.cleanup_days_ = cleanup_days_;

	// Explorer options
	val.wipe_type_ = wipe_type_;
	val.show_not_indexed_ = show_not_indexed_;
	val.sync_tree_ = sync_tree_;
	val.custom_explorer_menu_ = custom_explorer_menu_;

	// History
	val.recent_files_ = recent_files_;
	val.no_recent_add_ = no_recent_add_;
	val.max_search_hist_ = max_search_hist_;
	val.max_replace_hist_ = max_replace_hist_;
	val.max_hex_jump_hist_ = max_hex_jump_hist_;
	val.max_dec_jump_hist_ = max_dec_jump_hist_;
	val.max_expl_dir_hist_ = max_expl_dir_hist_;
	val.max_expl_filt_hist_ = max_expl_filt_hist_;
	val.clear_recent_file_list_ = clear_recent_file_list_;
	val.clear_bookmarks_ = clear_bookmarks_;
	val.clear_on_exit_ = clear_on_exit_;

	// Macros
	val.refresh_ = refresh_;
	val.num_secs_ = num_secs_;
	val.num_keys_ = num_keys_;
	val.num_plays_ = num_plays_;
	val.refresh_props_ = refresh_props_;
	val.refresh_bars_ = refresh_bars_;
	val.halt_level_ = halt_level_;

	// Printer pages
	val.border_ = print_box_;
	val.headings_ = print_hdr_;
	val.print_mark_ = print_mark_;
	val.print_bookmarks_ = print_bookmarks_;
	val.print_highlights_ = print_highlights_;
	val.print_search_ = print_search_;
	val.print_change_ = print_change_;
	val.print_compare_ = print_compare_;
	val.print_sectors_ = print_sectors_;
	val.units_ = int(print_units_);
	val.print_watermark_ = print_watermark_;
	val.watermark_ = watermark_;
	val.header_ = header_;
	val.diff_first_header_ = diff_first_header_;
	val.first_header_ = first_header_;
	val.footer_ = footer_;
	val.diff_first_footer_ = diff_first_footer_;
	val.first_footer_ = first_footer_;
	val.even_reverse_ = even_reverse_;
	val.left_ = left_margin_;
	val.right_ = right_margin_;
	val.top_ = top_margin_;
	val.bottom_ = bottom_margin_;
	val.header_edge_ = header_edge_;
	val.footer_edge_ = footer_edge_;
	val.spacing_ = spacing_;

	// Global display
	val.open_restore_ = open_restore_;
	val.mditabs_ = mditabs_;
	val.tabsbottom_ = tabsbottom_;
	val.tabicons_ = tabicons_;
	val.tabclose_ = tabclose_;
	val.tabcolour_ = tabcolour_;
	//val.dlg_dock_ = dlg_dock_;
	val.dlg_move_ = dlg_move_;
	val.hex_ucase_ = hex_ucase_;
	val.k_abbrev_ = std::min(k_abbrev_, 3);  // may need to increase this if we add more options (eg Tera, etc)
	val.large_cursor_ = large_cursor_;
	val.show_other_ = show_other_;
	val.nice_addr_ = nice_addr_;
	val.sel_len_tip_ = sel_len_tip_;
	val.sel_len_div2_ = sel_len_div2_;
	val.ruler_ = ruler_;
	val.ruler_dec_ticks_ = ruler_dec_ticks_;
	val.ruler_dec_nums_ = ruler_dec_nums_;
	val.ruler_hex_ticks_ = ruler_hex_ticks_;
	val.ruler_hex_nums_ = ruler_hex_nums_;
	val.scroll_past_ends_ = scroll_past_ends_;
	val.autoscroll_accel_ = autoscroll_accel_;
	val.reverse_zoom_ = reverse_zoom_;
	val.hl_caret_ = hl_caret_;
	val.hl_mouse_ = hl_mouse_;
	val.cont_char_ = cont_char_;
	val.invalid_char_ = invalid_char_;

	// Workspace
	val.intelligent_undo_ = intelligent_undo_;
	val.undo_limit_ = undo_limit_ - 1;
	val.cb_text_type_ = cb_text_type_;

	val.bg_search_ = bg_search_;
	val.bg_stats_ = bg_stats_;
	val.bg_stats_crc32_ = bg_stats_crc32_;
	val.bg_stats_md5_ = bg_stats_md5_;
	val.bg_stats_sha1_ = bg_stats_sha1_;
	val.bg_stats_sha256_ = bg_stats_sha256_;
	val.bg_stats_sha512_ = bg_stats_sha512_;
	val.bg_exclude_network_ = bg_exclude_network_;
	val.bg_exclude_removeable_ = bg_exclude_removeable_;
	val.bg_exclude_optical_ = bg_exclude_optical_;
	val.bg_exclude_device_ = bg_exclude_device_;

	// Backup
	val.backup_ = backup_;
	val.backup_space_ = backup_space_;
	val.backup_if_size_ = backup_size_ > 0;
	val.backup_size_ = backup_size_;
	val.backup_prompt_ = backup_prompt_;

	// Export
	val.address_specified_ = export_base_addr_ != -1;
	val.base_address_ = export_base_addr_ != -1 ? export_base_addr_ : 0;
	val.export_line_len_ = export_line_len_;

	val.aerial_max_ = aerial_max_/(1024*1024);

	// Global template
	val.max_fix_for_elts_ = max_fix_for_elts_;
	val.default_char_format_ = default_char_format_;
	val.default_int_format_ = default_int_format_;
	val.default_unsigned_format_ = default_unsigned_format_;
	val.default_string_format_ = default_string_format_;
	val.default_real_format_ = default_real_format_;
	val.default_date_format_ = default_date_format_;

	// Window page(s)
	CHexEditView *pview = GetView();
	if (pview != NULL)
	{
		// Get the name of the active view window and save in window_name_
		((CMDIChildWnd *)((CMainFrame *)AfxGetMainWnd())->MDIGetActive())->
			GetWindowText(val.window_name_);

		val.display_template_ = pview->TemplateViewType();
		val.display_aerial_ = pview->AerialViewType();
		val.display_comp_ = pview->CompViewType();
		val.display_prevw_ = pview->PrevwViewType();

		val.disp_state_ = pview->disp_state_;
		val.code_page_ = pview->code_page_;
		val.lf_ = pview->lf_;
		val.oem_lf_ = pview->oem_lf_;
		val.mb_lf_ = pview->mb_lf_;

		val.cols_ = pview->rowsize_;
		val.offset_ = pview->real_offset_;
		val.grouping_ = pview->group_by_;
		val.vertbuffer_ = pview->GetVertBufferZone();

		// Get whether or not the window is currently maximized
		WINDOWPLACEMENT wp;
		CWnd *cc = pview->GetParent();    // Get owner child frame (ancestor window)
		while (cc != NULL && !cc->IsKindOf(RUNTIME_CLASS(CChildFrame)))  // view may be in splitter(s)
			cc = cc->GetParent();
		ASSERT_KINDOF(CChildFrame, cc);
		cc->GetWindowPlacement(&wp);
		val.maximize_ = wp.showCmd == SW_SHOWMAXIMIZED;
	}
}

// Set options from the dialog after user clicks OK/APPLY
void CHexEditApp::set_options(struct OptValues &val)
{
	// This is a big kludge because of problems with property sheets.
	// First the problem: Each page of a property sheet is an autonomous
	// dialog with it's own OnOK() etc.  The trouble is if the user never
	// uses a page it is not even created & nothing in that page is called
	// (ie OnInitDialog() etc) so you can't rely on anything particularly
	// being called when the user click the OK (or Apply) button in the
	// property sheet; you can't rely on any particular property page's
	// OnOK() being called and there is no (easy) way to intercept the
	// property sheet's button clicking.
	// We want this so we can update from all pages in this function.
	// This allows easier moving of controls between pages etc.
	// But we need to make sure that set_options() is called exactly once
	// when the user clicks OK or Apply.  The solution is to use a timer.
	// If it has been less than half a second since the last call then
	// we can assume this is still for the same click of the OK button.
	if (set_options_timer.elapsed() < 0.3)
	{
		TRACE("----------- NOT setting options -------------\n");
		set_options_timer.reset();  // ensure repeated calls don't eventually time out
		return;
	}

	TRACE("--------------- setting options -------------\n");
	bool invalidate_views = false;      // Has something changed that requires redrawing hex windows?
	CMainFrame *mm = dynamic_cast<CMainFrame *>(AfxGetMainWnd());

	char buf[_MAX_PATH + 3];            // Stores value of key (not used)
	long buf_size = sizeof(buf);        // Size of buffer and returned key value

	if (val.shell_open_ != 
		(RegQueryValue(HKEY_CLASSES_ROOT, HexEditSubSubKey, buf, &buf_size) == ERROR_SUCCESS))
	{
		// Option has been changed (turned on or off)
		if (AvoidableTaskDialog(IDS_REG_REQUIRED,
								"In order to change registry settings for all users "
								"you may be prompted for Administrator privileges.\n\n"
								"Do you wish to continue?\n\n",
								NULL,
								"Admin Privileges",
								TDCBF_YES_BUTTON | TDCBF_NO_BUTTON) == IDYES)
		{
			if (val.shell_open_)
			{
				// Create the registry entries that allow "Open with HexEdit" on shortcut menus
				RegisterOpenAll();
			}
			else
			{
				UnregisterOpenAll();
			}
		}
	}

	save_exit_ = val.save_exit_;
	one_only_ = val.one_only_;
	special_list_scan_ = val.special_list_scan_;
	splash_ = val.splash_;
	tipofday_ = val.tipofday_;
	run_autoexec_ = val.run_autoexec_;

	open_locn_ = val.open_locn_;
	open_folder_ = val.open_folder_;
	save_locn_ = val.save_locn_;
	save_folder_ = val.save_folder_;

	thumbnail_ = val.thumbnail_;
	thumb_frame_ = val.thumb_frame_;
	thumb_size_ = val.thumb_size_;
	thumb_type_ = val.thumb_type_ + 1;   // drop-list index to enum THUMB_TYPE
	thumb_zoom_ = val.thumb_zoom_;
	cleanup_days_ = val.cleanup_days_;

	wipe_type_ = (wipe_t)val.wipe_type_;
	show_not_indexed_ = val.show_not_indexed_;
	if (sync_tree_ != val.sync_tree_)
	{
		sync_tree_ = val.sync_tree_;
		if (sync_tree_)
			((CMainFrame*)m_pMainWnd)->m_wndExpl.LinkToTree();
		else
			((CMainFrame*)m_pMainWnd)->m_wndExpl.UnlinkToTree();
	}
	custom_explorer_menu_ = val.custom_explorer_menu_;

	if (recent_files_ != val.recent_files_)
	{
		recent_files_ = val.recent_files_;
		GetFileList()->ChangeSize(recent_files_);
	}
	no_recent_add_ = val.no_recent_add_;
	max_search_hist_ = val.max_search_hist_;
	max_replace_hist_ = val.max_replace_hist_;
	max_hex_jump_hist_ = val.max_hex_jump_hist_;
	max_dec_jump_hist_ = val.max_dec_jump_hist_;
	max_expl_dir_hist_ = val.max_expl_dir_hist_;
	max_expl_filt_hist_ = val.max_expl_filt_hist_;
	clear_recent_file_list_ = val.clear_recent_file_list_;
	clear_bookmarks_ = val.clear_bookmarks_;
	clear_on_exit_ = val.clear_on_exit_;

	intelligent_undo_ = val.intelligent_undo_;
	undo_limit_ = val.undo_limit_ + 1;
	cb_text_type_ = val.cb_text_type_;

	// Remember what has changed before starting/stopping background processing
	bool search_changed = bg_search_ != val.bg_search_ ||
	                      bg_exclude_device_ != val.bg_exclude_device_ ||
	                      bg_exclude_network_ != val.bg_exclude_network_ ||
	                      bg_exclude_removeable_ != val.bg_exclude_removeable_ ||
	                      bg_exclude_optical_ != val.bg_exclude_optical_;
	bool stats_changed = bg_stats_ != val.bg_stats_ ||
	                     bg_exclude_device_ != val.bg_exclude_device_ ||
	                     bg_exclude_network_ != val.bg_exclude_network_ ||
	                     bg_exclude_removeable_ != val.bg_exclude_removeable_ ||
	                     bg_exclude_optical_ != val.bg_exclude_optical_;
	bool stats_restart = false;
	if (!stats_changed && val.bg_stats_)
	{
		// Stats was on and is still on - we need to check if it needs to restart
		stats_restart =
			!bg_stats_crc32_ && val.bg_stats_crc32_ ||
			!bg_stats_md5_ && val.bg_stats_md5_ ||
			!bg_stats_sha1_ && val.bg_stats_sha1_ ||
			!bg_stats_sha256_ && val.bg_stats_sha256_ ||
			!bg_stats_sha512_ && val.bg_stats_sha512_;
	}

	bg_search_ = val.bg_search_;
	bg_stats_ = val.bg_stats_;
	bg_stats_crc32_ = val.bg_stats_crc32_;
	bg_stats_md5_ = val.bg_stats_md5_;
	bg_stats_sha1_ = val.bg_stats_sha1_;
	bg_stats_sha256_ = val.bg_stats_sha256_;
	bg_stats_sha512_ = val.bg_stats_sha512_;
	bg_exclude_network_ = val.bg_exclude_network_;
	bg_exclude_removeable_ = val.bg_exclude_removeable_;
	bg_exclude_optical_ = val.bg_exclude_optical_;
	bg_exclude_device_ = val.bg_exclude_device_;

	if (search_changed)
	{
		POSITION posn = m_pDocTemplate->GetFirstDocPosition();
		while (posn != NULL)
		{
			CHexEditDoc *pdoc = dynamic_cast<CHexEditDoc *>(m_pDocTemplate->GetNextDoc(posn));
			ASSERT(pdoc != NULL);
			pdoc->AlohaSearch();  // say hello or goodbye to the bg search thread
		}
	}
	if (stats_changed)
	{
		POSITION posn = m_pDocTemplate->GetFirstDocPosition();
		while (posn != NULL)
		{
			CHexEditDoc *pdoc = dynamic_cast<CHexEditDoc *>(m_pDocTemplate->GetNextDoc(posn));
			ASSERT(pdoc != NULL);
			pdoc->AlohaStats();  // say hello or goodbye to the bg stats thread
		}
	}
	else if (stats_restart)
	{
		ASSERT(bg_stats_);  // stats should be on before we restart them
		bg_stats_ = FALSE;
		POSITION posn = m_pDocTemplate->GetFirstDocPosition();
		while (posn != NULL)
		{
			CHexEditDoc *pdoc = dynamic_cast<CHexEditDoc *>(m_pDocTemplate->GetNextDoc(posn));
			ASSERT(pdoc != NULL);
			pdoc->AlohaStats();  // stop
		}
		bg_stats_ = TRUE;
		posn = m_pDocTemplate->GetFirstDocPosition();
		while (posn != NULL)
		{
			CHexEditDoc *pdoc = dynamic_cast<CHexEditDoc *>(m_pDocTemplate->GetNextDoc(posn));
			ASSERT(pdoc != NULL);
			pdoc->AlohaStats();  // restart
		}
	}

	backup_ = val.backup_;
	backup_space_ = val.backup_space_;
	backup_size_ = val.backup_if_size_ ? val.backup_size_ : 0;
	backup_prompt_ = val.backup_prompt_;

	export_base_addr_ = val.address_specified_ ? val.base_address_ : -1;
	export_line_len_ = val.export_line_len_;


	/////////////////////////////////////////////////////////
	open_restore_ = val.open_restore_;
	if (mditabs_ != val.mditabs_ ||
		(mditabs_ && tabsbottom_ != val.tabsbottom_) ||
		(mditabs_ && tabicons_ != val.tabicons_) ||
		(mditabs_ && tabclose_ != val.tabclose_) ||
		(mditabs_ && tabcolour_ != val.tabcolour_)  )
	{
		mditabs_ = val.mditabs_;
		tabsbottom_ = val.tabsbottom_;
		tabicons_ = val.tabicons_;
		tabclose_ = val.tabclose_;
		tabcolour_ = val.tabcolour_;

		update_tabs();
	}

	if (hex_ucase_ != val.hex_ucase_)
	{
		hex_ucase_ = val.hex_ucase_;

		invalidate_views = true;        // redraw windows as they are probably showing hex digits

		// Fix up case of search strings (find tool, find dlg)
		CSearchEditControl::RedisplayAll();
		mm->m_wndFind.Redisplay();

		// Fix up case of hex addresses in hex jump tool(s)
		CHexEditControl::RedisplayAll();
	}
	if (k_abbrev_ != val.k_abbrev_)
	{
		k_abbrev_ = val.k_abbrev_;
		mm->m_wndProp.Update(GetView());  // may need to update file sizes
	}

	//if (dlg_dock_ != val.dlg_dock_)
	//	mm->OnDockableToggle();
	dlg_move_ = val.dlg_move_;

	if (large_cursor_ != val.large_cursor_)
	{
		large_cursor_ = val.large_cursor_;

		// Change caret of all views
		CMDIChildWnd *nextc;    // Loops through all MDI child frames

		// Find all view windows
		for (nextc = dynamic_cast<CMDIChildWnd *>(mm->MDIGetActive());
			 nextc != NULL;
			 nextc = dynamic_cast<CMDIChildWnd *>(nextc->GetWindow(GW_HWNDNEXT)) )
		{
			CHexEditView *pview = dynamic_cast<CHexEditView *>(nextc->GetActiveView());
			// Note pview may be NULL if in print preview
			if (pview != NULL && pview->IsKindOf(RUNTIME_CLASS(CHexEditView)))
			{
				if (large_cursor_)
					pview->BlockCaret();
				else
					pview->LineCaret();
			}
		}
	}

	if (show_other_ != val.show_other_)
	{
		show_other_ = val.show_other_;
		invalidate_views = true;
	}

	if (nice_addr_ != val.nice_addr_)
	{
		nice_addr_ = val.nice_addr_;
		invalidate_views = true;
	}
	sel_len_tip_  = val.sel_len_tip_;
	sel_len_div2_ = val.sel_len_div2_;

	if (ruler_ != val.ruler_)
	{
		ruler_ = val.ruler_;
		invalidate_views = true;
	}
	if (ruler_dec_ticks_ != val.ruler_dec_ticks_)
	{
		ruler_dec_ticks_ = val.ruler_dec_ticks_;
		invalidate_views = true;
	}
	if (ruler_dec_nums_ != val.ruler_dec_nums_)
	{
		ruler_dec_nums_ = val.ruler_dec_nums_;
		invalidate_views = true;
	}
	if (ruler_hex_ticks_ != val.ruler_hex_ticks_)
	{
		ruler_hex_ticks_ = val.ruler_hex_ticks_;
		invalidate_views = true;
	}
	if (ruler_hex_nums_ != val.ruler_hex_nums_)
	{
		ruler_hex_nums_ = val.ruler_hex_nums_;
		invalidate_views = true;
	}

	if (hl_caret_ != val.hl_caret_)
	{
		hl_caret_ = val.hl_caret_;
		invalidate_views = true;
	}
	if (hl_mouse_ != val.hl_mouse_)
	{
		hl_mouse_ = val.hl_mouse_;
		invalidate_views = true;
	}

	if (scroll_past_ends_ != val.scroll_past_ends_)
	{
		scroll_past_ends_ = val.scroll_past_ends_;
		invalidate_views = true;
	}
	if (autoscroll_accel_ != val.autoscroll_accel_)
	{
		autoscroll_accel_ = val.autoscroll_accel_;
		invalidate_views = true;        // causes recalc_display which sets accel
	}
	reverse_zoom_ = val.reverse_zoom_;

	if (cont_char_ != val.cont_char_)
	{
		cont_char_ = val.cont_char_;
		invalidate_views = true;
	}
	if (invalid_char_ != val.invalid_char_)
	{
		invalid_char_ = val.invalid_char_;
		invalidate_views = true;
	}

	aerial_max_ = val.aerial_max_ * 1024 * 1024;

	// global template options
	max_fix_for_elts_ = val.max_fix_for_elts_;
	default_char_format_ = val.default_char_format_;
	default_int_format_ = val.default_int_format_;
	default_unsigned_format_ = val.default_unsigned_format_;
	default_string_format_ = val.default_string_format_;
	default_real_format_ = val.default_real_format_;
	default_date_format_ = val.default_date_format_;

	/////////////////////////////////////////////////////////
	refresh_ = val.refresh_;
	num_secs_ = val.num_secs_;
	num_keys_ = val.num_keys_;
	num_plays_ = val.num_plays_;
	refresh_props_ = val.refresh_props_;
	refresh_bars_ = val.refresh_bars_;
	halt_level_ = val.halt_level_;

	/////////////////////////////////////////////////////////
	print_box_ = val.border_ ? true : false;
	print_hdr_ = val.headings_ ? true : false;
	print_mark_ = val.print_mark_ ? true : false;
	print_bookmarks_ = val.print_bookmarks_ ? true : false;
	print_highlights_ = val.print_highlights_ ? true : false;
	print_search_ = val.print_search_ ? true : false;
	print_change_ = val.print_change_ ? true : false;
	print_compare_ = val.print_compare_ ? true : false;
	print_sectors_ = val.print_sectors_ ? true : false;
	print_units_ = prn_unit_t(val.units_);
	print_watermark_ = val.print_watermark_ ? true : false;
	watermark_ = val.watermark_;
	header_ = val.header_;
	diff_first_header_ = val.diff_first_header_ ? true : false;
	first_header_ = val.first_header_;
	footer_ = val.footer_;
	diff_first_footer_ = val.diff_first_footer_ ? true : false;
	first_footer_ = val.first_footer_;
	even_reverse_ = val.even_reverse_ ? true : false;
	left_margin_ = val.left_;
	right_margin_ = val.right_;
	top_margin_ = val.top_;
	bottom_margin_ = val.bottom_;
	header_edge_ = val.header_edge_;
	footer_edge_ = val.footer_edge_;
	spacing_ = val.spacing_;

	/////////////////////////////////////////////////////////
	// Window page(s)
	CHexEditView *pview = GetView();
	if (pview != NULL)
	{
		bool one_done = false;              // Have we already made a change (saving undo info)
		// one_done is used to keep track of the changes made to the view so that
		// "previous_too" flag is set for all changes made at once except for the
		// first.  This allows all the changes made at the same time to be undone
		// with one undo operation, which will make more sense to the user.

		// Make the window maximized or not depending on setting chosen
		WINDOWPLACEMENT wp;

		// Get owner child frame (ancestor window)
		CWnd *cc = pview->GetParent();
		while (cc != NULL && !cc->IsKindOf(RUNTIME_CLASS(CChildFrame)))  // view may be in splitter(s)
			cc = cc->GetParent();
		ASSERT_KINDOF(CChildFrame, cc);

		cc->GetWindowPlacement(&wp);
		if (val.maximize_ != (wp.showCmd == SW_SHOWMAXIMIZED))
		{
			wp.showCmd = val.maximize_ ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
			cc->SetWindowPlacement(&wp);
		}

		if (val.display_template_ != pview->TemplateViewType())
		{
			switch (val.display_template_)
			{
			case 0:
				pview->OnDffdHide();
				break;
			case 1:
				pview->OnDffdSplit();
				break;
			case 2:
				pview->OnDffdTab();
				break;
			}
		}

		if (val.display_aerial_ != pview->AerialViewType())
		{
			switch (val.display_aerial_)
			{
			case 0:
				pview->OnAerialHide();
				break;
			case 1:
				pview->OnAerialSplit();
				break;
			case 2:
				pview->OnAerialTab();
				break;
			}
		}

		if (val.display_comp_ != pview->CompViewType())
		{
			switch (val.display_comp_)
			{
			case 0:
				pview->OnCompHide();
				break;
			case 1:
				pview->OnCompSplit();
				break;
			case 2:
				pview->OnCompTab();
				break;
			}
		}

		if (val.display_prevw_ != pview->PrevwViewType())
		{
			switch (val.display_prevw_)
			{
			case 0:
				pview->OnPrevwHide();
				break;
			case 1:
				pview->OnPrevwSplit();
				break;
			case 2:
				pview->OnPrevwTab();
				break;
			}
		}

		// Make other (undoable) changes if any of the options have changed
		bool change_required = (!val.autofit_ && pview->rowsize_ != val.cols_) ||
								pview->group_by_ != val.grouping_ ||
								pview->real_offset_ != val.offset_ ||
								pview->disp_state_ != val.disp_state_ ||
								pview->code_page_ != val.code_page_ ||
								(val.display_.FontRequired() == FONT_ANSI  && memcmp(&pview->lf_, &val.lf_, sizeof(LOGFONT)) != 0) ||
								(val.display_.FontRequired() == FONT_OEM   && memcmp(&pview->oem_lf_, &val.oem_lf_, sizeof(LOGFONT)) != 0) ||
								(val.display_.FontRequired() == FONT_UCODE && memcmp(&pview->mb_lf_, &val.mb_lf_, sizeof(LOGFONT)) != 0);

		if (change_required)
			pview->begin_change();

		// If current scheme is std scheme and it matches charset change scheme to match new charset
		if (val.display_.char_set != pview->display_.char_set)
		{
			if (pview->scheme_name_ == ANSI_NAME     && pview->display_.char_set == CHARSET_ANSI ||
				pview->scheme_name_ == ASCII_NAME    && pview->display_.char_set == CHARSET_ASCII ||
				pview->scheme_name_ == OEM_NAME      && pview->display_.char_set == CHARSET_OEM  ||
				pview->scheme_name_ == EBCDIC_NAME   && pview->display_.char_set == CHARSET_EBCDIC ||
				pview->scheme_name_ == UNICODE_NAME  && (pview->display_.char_set == CHARSET_UCODE_EVEN || pview->display_.char_set == CHARSET_UCODE_ODD) || 
				pview->scheme_name_ == CODEPAGE_NAME && pview->display_.char_set == CHARSET_CODEPAGE )
			{
				if (val.display_.char_set == CHARSET_ASCII)
					pview->SetScheme(ASCII_NAME);
				else if (val.display_.char_set == CHARSET_OEM)
					pview->SetScheme(OEM_NAME);
				else if (val.display_.char_set == CHARSET_EBCDIC)
					pview->SetScheme(EBCDIC_NAME);
				else if (pview->display_.char_set == CHARSET_UCODE_EVEN || pview->display_.char_set == CHARSET_UCODE_ODD)
					pview->SetScheme(UNICODE_NAME);
				else if (val.display_.char_set == CHARSET_CODEPAGE)
					pview->SetScheme(CODEPAGE_NAME);
				else
					pview->SetScheme(ANSI_NAME);
				if (one_done) pview->undo_.back().previous_too = true;
				one_done = true;
			}
		}
		if (val.display_.autofit != pview->display_.autofit)
		{
			pview->do_autofit(val.display_.autofit);
			if (one_done) pview->undo_.back().previous_too = true;
			one_done = true;
		}

		pview->disp_state_ = val.disp_state_;
		pview->SetCodePage(val.code_page_);    // xxx also allow undo of code page

		if (change_required)
		{
			one_done = pview->make_change(one_done) != 0;    // save disp_state_ in undo vector
			SaveToMacro(km_display_state, pview->disp_state_);
		}

		if (val.display_.vert_display && pview->GetVertBufferZone() < 2)
			pview->SetVertBufferZone(2);   // Make sure we can always see the other 2 rows at the same address

		// Do this after autofit changed so that rowsize_ is not messed up by old autofit value
		if (!val.display_.autofit && pview->rowsize_ != val.cols_)
		{
			pview->change_rowsize(val.cols_);
			if (one_done) pview->undo_.back().previous_too = true;
			one_done = true;
		}
		if (pview->group_by_ != val.grouping_)
		{
			pview->change_group_by(val.grouping_);
			if (one_done) pview->undo_.back().previous_too = true;
			one_done = true;
		}
		if (pview->real_offset_ != val.offset_)
		{
			pview->change_offset(val.offset_);
			if (one_done) pview->undo_.back().previous_too = true;
			one_done = true;
		}

		// Do this after disp_state_ change as restoring the correct font
		// (ANSI or OEM) relies on the state being correct.
		if (val.display_.FontRequired() == FONT_ANSI &&
			memcmp(&pview->lf_, &val.lf_, sizeof(LOGFONT)) != 0)
		{
			LOGFONT *prev_lf = new LOGFONT;
			*prev_lf = pview->lf_;
			pview->lf_ = val.lf_;
			pview->undo_.push_back(view_undo(undo_font));      // Allow undo of font change
			pview->undo_.back().plf = prev_lf;
			if (one_done) pview->undo_.back().previous_too = true;
			one_done = true;
		}
		if (val.display_.FontRequired() == FONT_OEM &&
					memcmp(&pview->oem_lf_, &val.oem_lf_, sizeof(LOGFONT)) != 0)
		{
			LOGFONT *prev_lf = new LOGFONT;
			*prev_lf = pview->oem_lf_;
			pview->oem_lf_ = val.oem_lf_;
			pview->undo_.push_back(view_undo(undo_font));      // Allow undo of font change
			pview->undo_.back().plf = prev_lf;
			if (one_done) pview->undo_.back().previous_too = true;
			one_done = true;
		}
		// xxx TODO we should have 3 separate undo_font_xxx for the 3 fonts ??? xxx
		if (val.display_.FontRequired() == FONT_UCODE &&
					memcmp(&pview->mb_lf_, &val.mb_lf_, sizeof(LOGFONT)) != 0)
		{
			LOGFONT *prev_lf = new LOGFONT;
			*prev_lf = pview->mb_lf_;
			pview->mb_lf_ = val.mb_lf_;
			pview->undo_.push_back(view_undo(undo_font));      // Allow undo of font change
			pview->undo_.back().plf = prev_lf;
			if (one_done) pview->undo_.back().previous_too = true;
			one_done = true;
		}

		// Vert buffer zone is not an undoable operation
		if (val.vertbuffer_ != pview->GetVertBufferZone())
			pview->SetVertBufferZone(val.vertbuffer_);

		if (change_required)
		{
			pview->end_change();                // updates display
			AfxGetApp()->OnIdle(0);             // This forces buttons & status bar pane update when Apply button used
		}
	}

	if (invalidate_views)
	{
		// Invalidate all views to change display of hex digits
		CMainFrame *mm = dynamic_cast<CMainFrame *>(AfxGetMainWnd());
		CMDIChildWnd *nextc;    // Loops through all MDI child frames

		// Invalidate all view windows
		for (nextc = dynamic_cast<CMDIChildWnd *>(mm->MDIGetActive());
			nextc != NULL;
			nextc = dynamic_cast<CMDIChildWnd *>(nextc->GetWindow(GW_HWNDNEXT)) )
		{
			CChildFrame * pfrm = dynamic_cast<CChildFrame *>(nextc);

			CHexEditView * phev = pfrm->GetHexEditView();
			if (phev != NULL)
			{
				phev->recalc_display(); // Address width, etc may have changed
				phev->DoInvalidate();
			}
		}
	}

	set_options_timer.reset();
}

CString CHexEditApp::GetCurrentFilters()
{
	CString retval;

	// Set up the grid cells
	for (int ii = 0; ; ++ii)
	{
		CString s1, s2;

		AfxExtractSubString(s1, current_filters_, ii*2, '|');

		AfxExtractSubString(s2, current_filters_, ii*2+1, '|');

		if (s1.IsEmpty() && s2.IsEmpty())
			break;

		if (s1.IsEmpty() || s2.IsEmpty() || s2[0] == '>')
			continue;

		retval += s1 + "|" + s2 + "|";
	}
	if (retval.IsEmpty())
		retval = "All Files (*.*)|*.*||";
	else
		retval += "|";

	return retval;
}

void CHexEditApp::ShowTipAtStartup(void)
{
		// CG: This function added by 'Tip of the Day' component.

		CCommandLineInfo cmdInfo;
		ParseCommandLine(cmdInfo);
		if (cmdInfo.m_bShowSplash /* && _access("HexEdit.tip", 0) == 0*/)
		{
				CTipDlg dlg;
				if (dlg.m_bStartup)
						dlg.DoModal();
		}
}

void CHexEditApp::ShowTipOfTheDay(void)
{
		// CG: This function added by 'Tip of the Day' component.

		CTipDlg dlg;
		dlg.DoModal();

		// This bit not added by the automatic "Tip of the Day" component
		SaveToMacro(km_tips);
}

void CHexEditApp::OnHelpReportIssue()
{
	::BrowseWeb(IDS_WEB_REPORT_ISSUE);
}

void CHexEditApp::OnUpdateHelpReportIssue(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(TRUE);
}

void CHexEditApp::OnWebPage()
{
	// Go to hexedit web site
	::BrowseWeb(IDS_WEB_ADDRESS);
}

BOOL CHexEditApp::backup(LPCTSTR filename, FILE_ADDRESS file_len) const
{
	BOOL retval = FALSE;

	if (backup_)
	{
		// Default to TRUE but if any test below fails set to FALSE
		retval = TRUE;

		if (retval && backup_space_ && 
			file_len >= AvailableSpace(filename) - 65536)    // allow 64K leeway
		{
			retval = FALSE;
		}

		// Make sure file length is less than max backup size (in Mbytes)
		if (retval && backup_size_ > 0 && 
			file_len > FILE_ADDRESS(backup_size_)*1024)
		{
			retval = FALSE;
		}

		// If still backing up and prompt the user (if required)
		if (retval && backup_prompt_ &&
			TaskMessageBox("Create Backup?", "Do you want to create a backup copy of this file?", MB_YESNO) != IDYES)
		{
			retval = FALSE;
		}
	}

	return retval;
}

#if 0 // This dummy doc no longer needed since we no lomger use CHtmlView
// This is all needed I think just to get a dummy document class
class CDummyDoc : public CDocument
{
protected: // create from serialization only
	CDummyDoc() {}
	DECLARE_DYNCREATE(CDummyDoc)
public:
	virtual ~CDummyDoc() {}
protected:
	DECLARE_MESSAGE_MAP()
};

IMPLEMENT_DYNCREATE(CDummyDoc, CDocument)

BEGIN_MESSAGE_MAP(CDummyDoc, CDocument)
END_MESSAGE_MAP()
#endif

void CHexEditApp::OnHelpWeb()
{
#if 0  // Don't use HtmlView, just fire up browser instead
	static CMultiDocTemplate *pDocTemplate = NULL;
	if (pDocTemplate == NULL)
	{
		pDocTemplate = new CMultiDocTemplate(
				IDR_HEXEDTYPE,
				RUNTIME_CLASS(CDummyDoc),
				RUNTIME_CLASS(CChildFrame), // custom MDI child frame
				RUNTIME_CLASS(CHtmlView));
	}

	CDocument *pDoc = pDocTemplate->OpenDocumentFile(NULL);

	ASSERT(pDoc != NULL);
	if (pDoc == NULL) return;

	POSITION pos = pDoc->GetFirstViewPosition();
	if (pos != NULL)
	{
		CHtmlView *pView = (CHtmlView *)pDoc->GetNextView(pos);
		ASSERT_KINDOF(CHtmlView, pView);

		// Go to hexedit web site
		CString str;
		VERIFY(str.LoadString(IDS_WEB_HELP));
		pView->Navigate2(str);
	}
#elif 0
	::BrowseWeb(IDS_WEB_HELP);
#else
	// TODO: online help.
	HWND hWindow = AfxGetMainWnd()->GetSafeHwnd();
	MessageBox(hWindow, "Online help is not available just yet. Sorry for the inconvenience!", "HexEdit", MB_ICONINFORMATION | MB_OK);
#endif
}

void CHexEditApp::OnHelpWebForum()
{
	// note: links to github discussions (as of 9 August 2021)
	::BrowseWeb(IDS_WEB_FORUMS);
}

void CHexEditApp::OnHelpWebHome()
{
	// note: links to github repo home (as of 9 August 2021)
	::BrowseWeb(IDS_WEB_ADDRESS);
}

void CHexEditApp::OnUpdateHelpWeb(CCmdUI* pCmdUI)
{
	// TODO: online help.
	BOOL enabled = pCmdUI->m_nID != ID_HELP_WEB;
	pCmdUI->Enable(enabled);
}

// App command to run the dialog
void CHexEditApp::OnAppAbout()
{
		CAbout aboutDlg;
		aboutDlg.DoModal();
}

void CHexEditApp::UpdateAllViews()
{
	POSITION posn = m_pDocTemplate->GetFirstDocPosition();
	while (posn != NULL)
	{
		CHexEditDoc *pdoc = dynamic_cast<CHexEditDoc *>(m_pDocTemplate->GetNextDoc(posn));
		ASSERT(pdoc != NULL);

		pdoc->UpdateAllViews(NULL);
	}
}

#ifdef FILE_PREVIEW
// Startup point of the background thread that cleans up preview thumbnail files
static UINT cleanup_func(LPVOID pParam)
{
	CHexEditApp *pApp = (CHexEditApp *)pParam;

	return pApp->RunCleanupThread();
}

// Called to start thumbnail cleanup
void CHexEditApp::CleanupPreviewFiles()
{
	thread_stop_ = false;
	cleanup_thread_ = AfxBeginThread(&cleanup_func, this, THREAD_PRIORITY_LOWEST);
}

// Handles thumbnail cleanup
UINT CHexEditApp::RunCleanupThread()
{
	// Wait for a little while to avoid any chance of slowing things while starting up
	for (int ii = 0; ii < 100; ++ii)
	{
		appdata_.Lock();
		if (thread_stop_)
		{
			cleanup_thread_ = NULL;
			appdata_.Unlock();
			return 0;
		}
		appdata_.Unlock();
		Sleep(1000);
	}

	CString preview_folder;
	if (::GetDataPath(preview_folder))
	{
		// Get directory used for preview thumbnails
		preview_folder += DIRNAME_PREVIEW;

		// Work out the cut-off date for which files modified before that time are deleted
		SYSTEMTIME now;
		GetSystemTime(&now);
		COleDateTime dtCut(now);
		dtCut -= COleDateTimeSpan(cleanup_days_, 0, 0, 0);

		// Get all the names of all the files with modification times before the cut-off time
		std::vector<CString> fileNames;
		CFileFind ff;
		BOOL bContinue = ff.FindFile(preview_folder + "HE*.*");
		while (bContinue)
		{
			bContinue = ff.FindNextFile();

			// Work out the modification time of this file as a COleDateTime
			FILETIME ft;
			SYSTEMTIME st;
			VERIFY(ff.GetLastWriteTime(&ft));
			FileTimeToSystemTime(&ft, &st);
			COleDateTime dtFile(st);

			// If the time is before the cut-off remember this file name for deletion
			if (dtFile < dtCut)
				fileNames.push_back(ff.GetFilePath());

			// Check if we have been told to terminate
			appdata_.Lock();
			if (thread_stop_)
			{
				cleanup_thread_ = NULL;
				appdata_.Unlock();
				return 0;
			}
			appdata_.Unlock();
		}

		// Now delete all the files
		for (std::vector<CString>::const_iterator pfn = fileNames.begin(); pfn != fileNames.end(); ++pfn)
			::DeleteFile(*pfn);
	}

	appdata_.Lock();
	cleanup_thread_ = NULL;
	appdata_.Unlock();
	return 0;
}
#endif  // FILE_PREVIEW

#if _MFC_VER >= 0x0A00  // Only needed for Win7 jump lists which are only supported in MFC 10

// RegisterExtensions is used to register file extensions so that the type of files can be opened 
// in HexEdit (ie HexEdit appears on the "Open With" Explorer menu).
// Since this requires admin privileges as separate program (RegHelper.exe) is fired up while 
// using the ShellExecute() verb "runas".  The parameter takes one or more file extensions,
// separated by vertical bars (|), for example ".jpg|.jpeg".
bool CHexEditApp::RegisterExtensions(LPCTSTR extensions)
{
	CString strCmdLine;
    CString strExeFullName;
    AfxGetModuleFileName(0, strExeFullName);
	strCmdLine.Format("EXT  \"%s\"  \"%s\"  \"HexEdit file\"  \"%s\" \"%s\"",
	                  strExeFullName,
					  ProgID,
				      m_pszAppID,
				      extensions);

	return CallRegHelper(strCmdLine);
}
#endif //_MFC_VER >= 0x0A00

bool CHexEditApp::RegisterOpenAll()
{
	CString strCmdLine;
    //CString FullName;
    //AfxGetModuleFileName(0, FullName);
	TCHAR FullName[MAX_PATH];
	::GetModuleFileName(0, FullName, sizeof(FullName));
	strCmdLine.Format("REGALL  \"%s\"  \"%s\"  \"%s\"  \"Open with HexEdit\"",
	                  FullName,
					  HexEditSubKey,
				      HexEditSubSubKey);

	return CallRegHelper(strCmdLine);
}

bool CHexEditApp::UnregisterOpenAll()
{
	CString strCmdLine;
	strCmdLine.Format("UNREGALL  \"%s\"  \"%s\"",
					  HexEditSubKey,
				      HexEditSubSubKey);

	return CallRegHelper(strCmdLine);
}

bool CHexEditApp::CallRegHelper(LPCTSTR cmdLine)
{
    CString strRegHelperFullName = GetExePath() + RegHelper;

    SHELLEXECUTEINFO shex;

    memset( &shex, 0, sizeof( shex) );
    shex.cbSize         = sizeof( SHELLEXECUTEINFO );
    shex.fMask          = 0;
    shex.hwnd           = AfxGetMainWnd()->GetSafeHwnd();
    shex.lpVerb         = _T("runas");
    shex.lpFile         = strRegHelperFullName;
    shex.lpParameters   = cmdLine;
    shex.lpDirectory    = _T(".");
    shex.nShow          = SW_NORMAL;

    if (!::ShellExecuteEx( &shex ))
	{
		TaskMessageBox("Modifying Registry Settings (All Users)",
			           "There was an error running:\n\n" + CString(RegHelper) +
					   "\n\nfrom the folder:\n\n" + GetExePath());
		return false;
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////
// CCommandLineParser member functions

void CCommandLineParser::ParseParam(const TCHAR* pszParam, BOOL bFlag, BOOL bLast)
{
	if (non_std)
	{
		if (bFlag)
		{
			// If there are any flags set then assume this is a standard command line
			non_std = FALSE;
		}
		else
		{
			CHexEditApp *aa = dynamic_cast<CHexEditApp *>(AfxGetApp());
			ASSERT(aa->open_current_readonly_ == -1);
			ASSERT(aa->open_current_shared_ == -1);

			if (aa->hwnd_1st_ != (HWND)0 && aa->one_only_)
			{
				// Get full path name as other instance may have different current directory
				char fullname[_MAX_PATH];
				AfxFullPath(fullname, pszParam);

				ATOM atom = ::GlobalAddAtom(fullname);
				ASSERT(atom != 0);
				if (atom == 0) return;
				DWORD dw = 0;
				::SendMessageTimeout(aa->hwnd_1st_, aa->wm_hexedit, 0, (LPARAM)atom,
									 SMTO_ABORTIFHUNG, 2000, &dw);
				::GlobalDeleteAtom(atom);
				return;
			}
			else if (aa->OpenDocumentFile(pszParam))
			{
				m_nShellCommand = FileNothing;  // file already opened so no shell command required
				return;
			}
		}
	}

	// Do standard command line processing using base class
	CCommandLineInfo::ParseParam(pszParam, bFlag, bLast);
}

/////////////////////////////////////////////////////////////////////////////
// Global functions

// Return a ptr to the active view or NULL if none
// Ie, the current hex view if it is the active view OR
// the hex view associated with the active view if it is an aerial/template/compare view.
CHexEditView *GetView()
{
	if (theApp.pv_ != NULL)
	{
		ASSERT(theApp.playing_ > 0);
		if (!IsWindow(theApp.pv_->m_hWnd) || !theApp.pv_->IsKindOf(RUNTIME_CLASS(CHexEditView)))
			return NULL;
		else
			return theApp.pv_;
	}

	CMainFrame *mm = (CMainFrame *)AfxGetMainWnd();

	if (mm != NULL)
	{
		CMDIChildWnd *pwind = mm->MDIGetActive();

		// Ignore if there are no views open
		if (pwind != NULL)
		{
//            CHexEditView *pv = dynamic_cast<CHexEditView *>(pwind->GetActiveView());
			CView *pv = pwind->GetActiveView();
			if (pv != NULL)                         // May be NULL if print preview
			{
				if (pv->IsKindOf(RUNTIME_CLASS(CHexEditView)))
					return (CHexEditView *)pv;
				else if (pv->IsKindOf(RUNTIME_CLASS(CDataFormatView)))
					return ((CDataFormatView *)pv)->phev_;
				else if (pv->IsKindOf(RUNTIME_CLASS(CAerialView)))
					return ((CAerialView *)pv)->phev_;
				else if (pv->IsKindOf(RUNTIME_CLASS(CCompareView)))
					return ((CCompareView *)pv)->phev_;
				else if (pv->IsKindOf(RUNTIME_CLASS(CPrevwView)))
					return ((CPrevwView *)pv)->phev_;
				else if (pv->IsKindOf(RUNTIME_CLASS(CHexTabView)))
				{
					// Find the hex view (left-most tab)
					CHexTabView *ptv = (CHexTabView *)pv;
					ptv->SetActiveView(0);  // hex view is always left-most (index 0)
					ASSERT_KINDOF(CHexEditView, ptv->GetActiveView());
					return (CHexEditView *)ptv->GetActiveView();
				}
			}
		}
	}
	return NULL;
}

COLORREF BestDecAddrCol()
{
	CHexEditView *pv = GetView();
	if (pv == NULL)
		return theApp.scheme_[0].dec_addr_col_;
	else
		return pv->GetDecAddrCol();
}

COLORREF BestHexAddrCol()
{
	CHexEditView *pv = GetView();
	if (pv == NULL)
		return theApp.scheme_[0].hex_addr_col_;
	else
		return pv->GetHexAddrCol();
}

COLORREF BestSearchCol()
{
	CHexEditView *pv = GetView();
	if (pv == NULL)
		return theApp.scheme_[0].search_col_;
	else
		return pv->GetSearchCol();
}
