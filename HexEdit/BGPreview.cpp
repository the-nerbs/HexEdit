// BGPreview.cpp : implements preview of graphics files from memory - rendered in a background thread (part of CHexEditDoc)
//
// Copyright (c) 2015 by Andrew W. Phillips.
//
// This file is distributed under the MIT license, which basically says
// you can do what you want with it and I take no responsibility for bugs.
// See http://www.opensource.org/licenses/mit-license.php for full details.
//

#include "stdafx.h"
#include "HexEdit.h"

#include "HexEditDoc.h"
#include "HexEditView.h"
#include <FreeImage.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// TODO xxx
//  test with a 2GByte file

void CHexEditDoc::AddPreviewView(CHexEditView *pview)
{
	TRACE("+++ preview +++ %d\n", preview_count_);
	if (++preview_count_ == 1)
	{
		// Create the background thread and start it scanning
		CreatePreviewThread();
		TRACE("+++ Pulsing preview event\n");
		start_preview_event_.SetEvent();
	}
	ASSERT(pthread6_ != NULL);
}

void CHexEditDoc::RemovePreviewView()
{
	if (--preview_count_ == 0)
	{
		if (pthread6_ != NULL)
			KillPreviewThread();
		FIBITMAP *dib = preview_dib_;
		preview_dib_ = NULL;
		TRACE("+++  preview: FreeImage_Unload(%d)\n", dib);
		if (dib != NULL)
			FreeImage_Unload(dib);
	}
	TRACE("+++  Preview --- %d\n", preview_count_);
}

// Doc or colours have changed - signal to redisplay
void CHexEditDoc::PreviewChange(CHexEditView *pview /*= NULL*/)
{
	if (preview_count_ == 0) return;
	ASSERT(pthread6_ != NULL);
	if (pthread6_ == NULL) return;

	// Wait for thread to stop if necessary
	bool waiting;
	docdata_.Lock();
	preview_command_ = STOP;
	docdata_.Unlock();
	SetThreadPriority(pthread6_->m_hThread, THREAD_PRIORITY_NORMAL);
	for (int ii = 0; ii < 100; ++ii)
	{
		// Wait just a little bit in case the thread was just about to go into wait state
		docdata_.Lock();
		waiting = preview_state_ == WAITING;
		docdata_.Unlock();
		if (waiting)
			break;
		TRACE("+++ PreviewChange - thread not waiting (yet)\n");
		Sleep(1);
	}
	SetThreadPriority(pthread6_->m_hThread, THREAD_PRIORITY_LOWEST);
	ASSERT(waiting);

	// Restart the scan
	docdata_.Lock();
	preview_command_ = NONE;  // make sure we don't stop the scan before it starts
	preview_fin_ = false;
	docdata_.Unlock();

	TRACE("+++ Pulsing preview event (restart)\n");
	start_preview_event_.SetEvent();
}

int CHexEditDoc::PreviewProgress()
{
	return -1;
}

static const char *format_name[] =
{
	"BMP",
	"Icon",
	"JPEG",
	"JNG",
	"KOALA",
	"IFF ILBM",
	"MNG",
	"PBM",
	"PBMRAW",
	"PCD",
	"PCX",
	"PGM",
	"PGMRAW",
	"PNG",
	"PPM",
	"PPMRAW",
	"RAS",
	"TARGA",
	"TIFF",
	"WBMP",
	"PSD",
	"CUT",
	"XBM",
	"XPM",
	"DDS",
	"GIF",
	"HDR",
	"FAXG3",
	"SGI",
	"EXR",
	"J2K",
	"JP2",
	"PFM",
	"PICT",
	"RAW",
	NULL
};

// Returns:
// -4 = no preview
// -2 = load still in progress or failed
int CHexEditDoc::GetBmpInfo(CString &format, CString &bpp, CString &width, CString &height)
{
	// Clear strings in case of error return
	format = "";
	bpp = width = height = "";

	if (pthread6_ == NULL)
		return -4;         // no bitmap preview for this file

	format = "Unknown";

	// Protect access to shared data
	CSingleLock sl(&docdata_, TRUE);

	if (preview_dib_ == NULL)
		return -2;

	ASSERT(preview_fif_ != FREE_IMAGE_FORMAT(-999));
	if (preview_fif_ > FIF_UNKNOWN && preview_fif_ <= FIF_RAW)
	{
		format = CString(format_name[int(preview_fif_)]);
		bpp.Format("%d bits/pixel", preview_bpp_);
		width.Format("%u pixels", preview_width_);
		height.Format("%u pixels", preview_height_);
	}

	return 0;
}

// Returns:
// -3 = no disk file
int CHexEditDoc::GetDiskBmpInfo(CString &format, CString &bpp, CString &width, CString &height)
{
	// Clear strings in case of error return
	format = "Unknown";
	bpp = width = height = "";

	//if (pthread6_ == NULL)
	//	return -4;         // no bitmap preview for this file

	if (pfile1_ == NULL)
		return -3;

	// Protect access to shared data
	CSingleLock sl(&docdata_, TRUE);

	// If we don't have the any info on the disk bitmap
	if (preview_file_fif_ == FREE_IMAGE_FORMAT(-999))
	{
		preview_file_fif_ = FIF_UNKNOWN;

		// Wrap FreeImage stuff in try block as FreeImage_Load may have memory access violations on bad data
		try
		{
			FIBITMAP * dib = NULL;
			// Load info about bitmap from the disk file
			// (Unfortunately we have to load the whole bitmap into memory to get his info.)
			preview_file_fif_ = FreeImage_GetFileType(pfile1_->GetFilePath());
			if (preview_file_fif_ > FIF_UNKNOWN &&
				(dib = FreeImage_Load(preview_file_fif_, pfile1_->GetFilePath())) != NULL)
			{
				preview_file_bpp_ = FreeImage_GetBPP(dib);
				preview_file_width_ = FreeImage_GetWidth(dib);
				preview_file_height_ = FreeImage_GetHeight(dib);

				FreeImage_Unload(dib);
			}
			else
				preview_file_fif_ = FIF_UNKNOWN;
		}
		catch (...)
		{
			preview_file_fif_ = FIF_UNKNOWN;
		}
	}
	if (preview_file_fif_ > FIF_UNKNOWN && preview_file_fif_ <= FIF_RAW)
	{
		format = CString(format_name[int(preview_file_fif_)]);
		bpp.Format("%d bits/pixel", preview_file_bpp_);
		width.Format("%u pixels", preview_file_width_);
		height.Format("%u pixels", preview_file_height_);
	}

	return 0;
}

// Sends a message for the thread to kill itself then tidies up shared members. 
void CHexEditDoc::KillPreviewThread()
{
	ASSERT(pthread6_ != NULL);
	if (pthread6_ == NULL) return;

	HANDLE hh = pthread6_->m_hThread;    // Save handle since it will be lost when thread is killed and object is destroyed
	TRACE1("+++ Killing preview thread for %p\n", this);

	bool waiting, dying;
	docdata_.Lock();
	preview_command_ = DIE;
	docdata_.Unlock();
	SetThreadPriority(pthread6_->m_hThread, THREAD_PRIORITY_NORMAL); // Make it a quick and painless death
	for (int ii = 0; ii < 100; ++ii)
	{
		// Wait just a little bit in case the thread was just about to go into wait state
		docdata_.Lock();
		waiting = preview_state_ == WAITING;
		dying   = preview_state_ == DYING;
		docdata_.Unlock();
		if (waiting || dying)
			break;
		Sleep(1);
	}
	ASSERT(waiting || dying);

	timer tt(true);
	if (waiting)
		start_preview_event_.SetEvent();

	pthread6_ = NULL;
	DWORD wait_status = ::WaitForSingleObject(hh, INFINITE);
	ASSERT(wait_status == WAIT_OBJECT_0 || wait_status == WAIT_FAILED);
	tt.stop();
	TRACE1("+++ Thread took %g secs to kill\n", double(tt.elapsed()));

	// Free resources that are only needed during scan
	if (pfile6_ != NULL)
	{
		pfile6_->Close();
		delete pfile6_;
		pfile6_ = NULL;
	}
	for (int ii = 0; ii < doc_loc::max_data_files; ++ii)
	{
		if (data_file6_[ii] != NULL)
		{
			data_file6_[ii]->Close();
			delete data_file6_[ii];
			data_file6_[ii] = NULL;
		}
	}
}

// bg_func is the entry point for the thread.
// It just calls the RunPreviewThread member for the doc passed to it.
static UINT bg_func(LPVOID pParam)
{
	CHexEditDoc *pDoc = (CHexEditDoc *)pParam;

	TRACE1("+++ Preview thread started for doc %p\n", pDoc);

	return pDoc->RunPreviewThread();
}

// Sets up shared members and creates the thread using bg_func (above)
void CHexEditDoc::CreatePreviewThread()
{
	ASSERT(pthread6_ == NULL);
	ASSERT(pfile6_ == NULL);

	// Open copy of file to be used by background thread
	if (pfile1_ != NULL)
	{
		if (IsDevice())
			pfile6_ = new CFileNC();
		else
			pfile6_ = new CFile64();
		if (!pfile6_->Open(pfile1_->GetFilePath(),
					CFile::modeRead|CFile::shareDenyNone|CFile::typeBinary) )
		{
			TRACE1("+++ Preview file open failed for %p\n", this);
			return;
		}
	}

	// Open copy of any data files in use too
	for (int ii = 0; ii < doc_loc::max_data_files; ++ii)
	{
		ASSERT(data_file6_[ii] == NULL);
		if (data_file_[ii] != NULL)
			data_file6_[ii] = new CFile64(data_file_[ii]->GetFilePath(), 
										  CFile::modeRead|CFile::shareDenyWrite|CFile::typeBinary);
	}

	// Create new thread
	preview_command_ = NONE;
	preview_state_ = STARTING;    // pre start and very beginning
	preview_fin_ = false;
	TRACE1("+++ Creating preview thread for %p\n", this);
	pthread6_ = AfxBeginThread(&bg_func, this, THREAD_PRIORITY_LOWEST);
	ASSERT(pthread6_ != NULL);
}

//---------------------------------------------------------------------------------------
// These 4 funcs (fi_read(), etc) are used with FreeImage for in-memory processing of bitmap data.
// Pointers to these funcs are added to a FreeImageIO struct (see fi_funcs below) and passed
// to FreeImage functions like FreeImage_LoadFromHandle() or FreeImage_GetFileTypeFromHandle().

// fi_read() should behave like fread (returns count if all items were read OK, returns 0 on eof/error)
unsigned DLL_CALLCONV CHexEditDoc::fi_read(void *buffer, unsigned size, unsigned count, fi_handle handle)
{
	CHexEditDoc *pDoc = (CHexEditDoc *)handle;

	if (pDoc->PreviewProcessStop())
		return 0;       // indicate that processing should stop

	size_t got = pDoc->GetData((unsigned char *)buffer, size*count, pDoc->preview_address_);
	pDoc->preview_address_ += got;
	return got/size;
}

unsigned DLL_CALLCONV CHexEditDoc::fi_write(void *buffer, unsigned size, unsigned count, fi_handle handle)
{
	ASSERT(0);   // writing should not happen - we are only reading to display the bitmap
	return 0;
}

int DLL_CALLCONV CHexEditDoc::fi_seek(fi_handle handle, long offset, int origin)
{
	CHexEditDoc *pDoc = (CHexEditDoc *)handle;
	FILE_ADDRESS addr = 0;
	switch (origin)
	{
	case SEEK_SET:
		addr = offset;
		break;
	case SEEK_CUR:
		addr = pDoc->preview_address_ + offset;
		break;
	case SEEK_END:
		addr = pDoc->length_ + offset;
		break;
	}
	if (addr > LONG_MAX)
		return -1L;        // seek failed

	pDoc->preview_address_ = (long)addr;
	return 0L;             // seek succeeded
}

long DLL_CALLCONV CHexEditDoc::fi_tell(fi_handle handle)
{
	CHexEditDoc *pDoc = (CHexEditDoc *)handle;

	return pDoc->preview_address_;
}

FreeImageIO fi_funcs = { &CHexEditDoc::fi_read, &CHexEditDoc::fi_write, &CHexEditDoc::fi_seek, &CHexEditDoc::fi_tell };

// This is what does the work in the background thread
UINT CHexEditDoc::RunPreviewThread()
{
	// Keep looping until we are told to die
	for (;;)
	{
		// Signal that we are waiting then wait for start_preview_event_ to be pulsed
		{
			CSingleLock sl(&docdata_, TRUE);
			preview_state_ = WAITING;
		}
		TRACE1("+++ BGPreview: waiting for %p\n", this);
		DWORD wait_status = ::WaitForSingleObject(HANDLE(start_preview_event_), INFINITE);
		docdata_.Lock();
		preview_state_ = SCANNING;
		docdata_.Unlock();
		start_preview_event_.ResetEvent();
		ASSERT(wait_status == WAIT_OBJECT_0);
		TRACE1("+++ BGPreview: got event for %p\n", this);

		if (PreviewProcessStop())
			continue;

		// Reset for new scan
		docdata_.Lock();
		preview_fin_ = false;
		preview_address_ = 0;
		if (preview_dib_ != NULL)
		{
			FreeImage_Unload(preview_dib_);
			preview_dib_ = NULL;
		}
		docdata_.Unlock();

		FREE_IMAGE_FORMAT fif = FreeImage_GetFileTypeFromHandle(&fi_funcs, this);
		FIBITMAP * dib;
		// Catch FreeImage_Load exceptions since it has been known to have memory access violations on bad data
		try
		{
			dib = FreeImage_LoadFromHandle(fif, &fi_funcs, this);
		}
		catch (...)
		{
			dib = NULL;
		}
		int bpp = FreeImage_GetBPP(dib);
		unsigned width = FreeImage_GetWidth(dib);
		unsigned height = FreeImage_GetHeight(dib);

		TRACE1("+++ BGPreview: finished load for %p\n", this);
		docdata_.Lock();
		preview_fin_ = true;
		preview_address_ = 0;
		preview_dib_ = dib;

		// Save info about the bitmap just loaded
		preview_fif_ = fif;
		preview_bpp_ = bpp;
		preview_width_ = width;
		preview_height_ = height;
		docdata_.Unlock();
	}
	return 0;   // never reached
}

// Check for a stop scanning (or kill) of the background thread
bool CHexEditDoc::PreviewProcessStop()
{
	bool retval = false;

	CSingleLock sl(&docdata_, TRUE);
	switch(preview_command_)
	{
	case STOP:                      // stop scan and wait
		TRACE1("+++ BGPreview: stop for %p\n", this);
		retval = true;
		break;
	case DIE:                       // terminate this thread
		TRACE1("+++ BGPreview: killed thread for %p\n", this);
		preview_state_ = DYING;
		sl.Unlock();                // we need this here as AfxEndThread() never returns so d'tor is not called
		AfxEndThread(1);            // kills thread (no return)
		break;                      // Avoid warning
	case NONE:                      // nothing needed here - just continue scanning
		break;
	default:                        // should not happen
		ASSERT(0);
	}

	// Prevent reprocessing of the same command
	preview_command_ = NONE;
	return retval;
}

