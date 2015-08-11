// NewCompare.cpp : implements CNewCompare dialog
//
// Copyright (c) 2015 by Andrew W. Phillips
//
// This file is distributed under the MIT license, which basically says
// you can do what you want with it and I take no responsibility for bugs.
// See http://www.opensource.org/licenses/mit-license.php for full details.
//

#include "stdafx.h"
#include "NewCompare.h"
#include "afxdialogex.h"

// CNewCompare dialog

IMPLEMENT_DYNAMIC(CNewCompare, CHexDialog)

CNewCompare::CNewCompare(CWnd* pParent /*=NULL*/)
	: CHexDialog(CNewCompare::IDD, pParent)
{
	compare_type_ = compare_display_ = 0;
	insdel_ = 0;
	minmatch_ = 10;
	auto_sync_ = auto_scroll_ = 1;
}

CNewCompare::~CNewCompare()
{
}

void CNewCompare::DoDataExchange(CDataExchange* pDX)
{
	CHexDialog::DoDataExchange(pDX);
	DDX_Radio(pDX, IDC_COMPARE_SELF, compare_type_);
	DDX_Text(pDX, IDC_COMPARE_FILENAME, file_name_);
	DDX_Radio(pDX, IDC_COMPARE_SPLIT, compare_display_);
	DDX_Check(pDX, IDC_COMPARE_INSDEL, insdel_);
	DDX_Text(pDX, IDC_COMPARE_MINMATCH, minmatch_);
	DDV_MinMaxUInt(pDX, minmatch_, 7, 64);
	DDX_Check(pDX, IDC_COMPARE_AUTOSYNC, auto_sync_);
	DDX_Check(pDX, IDC_COMPARE_AUTOSCROLL, auto_scroll_);
	//DDX_Text(pDX, IDC_COMPARE_COMMENT, comment_);
	DDX_Control(pDX, IDC_COMPARE_FILENAME, ctl_file_name_);
	DDX_Control(pDX, IDC_COMPARE_BROWSE, ctl_browse_);
	DDX_Control(pDX, IDC_COMPARE_COMMENT, ctl_comment_);
	DDX_Control(pDX, IDC_COMPARE_STATIC1, ctl_static1_);
	DDX_Control(pDX, IDC_COMPARE_MINMATCH, ctl_minmatch_);
}

BEGIN_MESSAGE_MAP(CNewCompare, CHexDialog)
	ON_BN_CLICKED(IDC_COMPARE_SELF, &CNewCompare::OnBnClickedCompareSelf)
	ON_BN_CLICKED(IDC_COMPARE_FILE, &CNewCompare::OnBnClickedCompareFile)
	ON_BN_CLICKED(IDC_COMPARE_BROWSE, &CNewCompare::OnBnClickedAttachmentBrowse)
	ON_BN_CLICKED(IDC_COMPARE_INSDEL, &CNewCompare::OnBnClickedInsDel)
END_MESSAGE_MAP()

BOOL CNewCompare::OnInitDialog()
{
	CHexDialog::OnInitDialog();

	ASSERT(GetDlgItem(IDC_COMPARE_MINMATCH_SPIN) != NULL);
	((CSpinButtonCtrl *)GetDlgItem(IDC_COMPARE_MINMATCH_SPIN))->SetRange(7, 64);

	// Make sure when dialog is resized that the controls move to sensible places
	resizer_.Create(this);
	resizer_.SetMinimumTrackingSize();
	resizer_.SetGripEnabled(TRUE);

	// Controls to adjust -              left  top  width height
	resizer_.Add(IDC_FIND_COMBINED_STRING,  0,   0, 100,   0);
	resizer_.Add(IDC_COMPARE_FILENAME,      0,   0, 100,   0);
	resizer_.Add(IDC_COMPARE_BROWSE,      100,   0,   0,   0);
	resizer_.Add(IDC_COMPARE_HELP,          0, 100,   0,   0);
	resizer_.Add(IDOK,                    100, 100,   0,   0);
	resizer_.Add(IDCANCEL,                100, 100,   0,   0);

	CHexDialog::RestorePos();

	fix_controls();

	if (compare_display_ > -1)
	{
		GetDlgItem(IDC_COMPARE_SPLIT)->EnableWindow(0);
		GetDlgItem(IDC_COMPARE_TABBED)->EnableWindow(0);
	}
	else
	{
		// Default to split window
		compare_display_ = 0;
		((CButton*)GetDlgItem(IDC_COMPARE_SPLIT))->SetCheck(1);
	}

	return TRUE;
}

void CNewCompare::OnOK()
{
	if (!UpdateData(TRUE))
		return;

	if (compare_type_ == 1 && file_name_[0] == '\0')
	{
		TaskMessageBox("No File Specified", "Please enter the name of a file to compare with.");
		ASSERT(GetDlgItem(IDC_COMPARE_FILENAME) != NULL);
		GetDlgItem(IDC_COMPARE_FILENAME)->SetFocus();
		return;
	}
	if (compare_type_ == 1 && _access(file_name_, 0) == -1)
	{
		TaskMessageBox("File Not Found", "The file to compare with is not present on disk or you do not have permission to read it.");
		ASSERT(GetDlgItem(IDC_COMPARE_FILENAME) != NULL);
		GetDlgItem(IDC_COMPARE_FILENAME)->SetFocus();
		return;
	}

	CDialog::OnOK();
}


void CNewCompare::fix_controls()
{
	ctl_file_name_.EnableWindow(compare_type_ == 1);
	ctl_browse_.EnableWindow(compare_type_ == 1);
	if (compare_type_ == 0)
		ctl_comment_.SetWindowText("Show previous version of file in:");
	else
		ctl_comment_.SetWindowText("Show file, to be compared with, in:");
	ctl_static1_.EnableWindow(insdel_ == 1);
	ctl_minmatch_.EnableWindow(insdel_ == 1);
}

// CNewCompare message handlers

void CNewCompare::OnBnClickedCompareSelf()
{
	UpdateData();
	fix_controls();
}

void CNewCompare::OnBnClickedCompareFile()
{
	UpdateData();
	fix_controls();
}

void CNewCompare::OnBnClickedAttachmentBrowse()
{
	UpdateData();

	CHexFileDialog dlgFile("CompareFileDlg", HIDD_FILE_ATTACH, TRUE, NULL, file_name_,
						   OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_DONTADDTORECENT,
						   theApp.GetCurrentFilters(), "Compare", this);
	dlgFile.m_ofn.lpstrTitle = "Compare To File";

	if (dlgFile.DoModal() == IDOK)
	{
		file_name_ = dlgFile.GetPathName();
		UpdateData(FALSE);
	}
}

void CNewCompare::OnBnClickedInsDel()
{
	UpdateData();
	fix_controls();
}