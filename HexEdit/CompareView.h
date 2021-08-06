// CompareView.h : header file of the CCompareView class
//
// Copyright (c) 2015 by Andrew W. Phillips
//
// This file is distributed under the MIT license, which basically says
// you can do what you want with it and I take no responsibility for bugs.
// See http://www.opensource.org/licenses/mit-license.php for full details.
//

#ifndef COMPAREVIEW_INCLUDED
#define COMPAREVIEW_INCLUDED  1

#include "ScrView.h"
#include <vector>

// Forward declarations
class CHexEditDoc;
class CChildFrame;

// When comparing files we use this to displayed the compared with file.
// It depends on the corresponding CHexEditView for a lot of its display
// formating but there is independent control over:
//  - file displayed and hence OnDraw and OnInitialUpdate
//  - current position (unless using auto-sync)
//  - selection (so the user can select parts of this file)
//  - searches

class CCompareView : public CScrView
{
	friend CHexEditView;
protected: // create from serialization only
	CCompareView();
	DECLARE_DYNCREATE(CCompareView)

public:
	CHexEditView * phev_;

// Attributes
public:
	CHexEditDoc * GetDocument() { return (CHexEditDoc*)phev_->m_pDocument; }

	FILE_ADDRESS GetPos() const { return pos2addr(GetCaret()); }
	BOOL ReadOnly() const { return TRUE; }
	BOOL CharMode() const { return phev_->display_.edit_char; }
	BOOL EbcdicMode() const { return phev_->display_.char_set == CHARSET_EBCDIC; }
	BOOL OemMode() const { return phev_->display_.char_set == CHARSET_OEM; }
	BOOL AnsiMode() const { return phev_->display_.char_set == CHARSET_ANSI; }
	BOOL DecAddresses() const { return !phev_->display_.hex_addr; }  // Now that user can show both addresses at once this is probably the best return value

// Operations
	//virtual void SetSel(CPointAp, CPointAp, bool base1 = false);

	bool CopyToClipboard();
	virtual BOOL MovePos(UINT nChar, UINT nRepCnt, BOOL, BOOL, BOOL);
	void MoveToAddress(FILE_ADDRESS astart, FILE_ADDRESS aend = -1, int row = 0);

public:

// Overrides
	//virtual void DisplayCaret(int char_width = -1);
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra,
		AFX_CMDHANDLERINFO* pHandlerInfo)
	{
		// If compare view can't handle it try "owner" hex view
		if (CScrView::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
			return TRUE;
		else if (phev_ != NULL)
			return phev_->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
		else
			return FALSE;
	}

public:
	virtual void OnDraw(CDC* pDC);  // overridden to draw this view
	virtual void OnInitialUpdate();
//    virtual void OnPrepareDC(CDC* pDC, CPrintInfo* pInfo = NULL);
protected:
	//virtual BOOL OnPreparePrinting(CPrintInfo* pInfo);
	//virtual void OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo);
	//virtual void OnEndPrinting(CDC* pDC, CPrintInfo* pInfo);
	//virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	//virtual void OnPrint(CDC* pDC, CPrintInfo* pInfo);
	//virtual void OnEndPrintPreview(CDC* pDC, CPrintInfo* pInfo, POINT point, CPreviewView* pView);
	//virtual void OnActivateView(BOOL bActivate, CView* pActivateView, CView* pDeactiveView);
	//virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

// Implementation
public:
	//virtual void DoInvalidate();

protected:
	virtual void ValidateCaret(CPointAp &pos, BOOL inside=TRUE);
	//virtual void InvalidateRange(CPointAp start, CPointAp end, bool f = false);
	//virtual void DoInvalidateRect(LPCRECT lpRect);
	//virtual void DoInvalidateRgn(CRgn* pRgn);
	virtual void DoScrollWindow(int xx, int yy);
	//virtual void DoUpdateWindow();
	//virtual void DoHScroll(int total, int page, int pos);
	//virtual void DoVScroll(int total, int page, int pos);
	//void DoUpdate();
	virtual void AfterScroll(CPointAp newpos)
	{
		if (phev_ != NULL && phev_->display_.auto_scroll_comp)
			phev_->SetScroll(newpos);
	}

protected:
	//afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnSetFocus(CWnd* pNewWnd);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg LRESULT OnMouseHover(WPARAM wp, LPARAM lp);
	afx_msg LRESULT OnMouseLeave(WPARAM wp, LPARAM lp);

	afx_msg void OnUpdateDisable(CCmdUI* pCmdUI) { pCmdUI->Enable(FALSE); }

	afx_msg void OnCompFirst();
	afx_msg void OnUpdateCompFirst(CCmdUI* pCmdUI);
	afx_msg void OnCompPrev();
	afx_msg void OnUpdateCompPrev(CCmdUI* pCmdUI);
	afx_msg void OnCompNext();
	afx_msg void OnUpdateCompNext(CCmdUI* pCmdUI);
	afx_msg void OnCompLast();
	afx_msg void OnUpdateCompLast(CCmdUI* pCmdUI);
	afx_msg void OnEditCopy();
	afx_msg void OnUpdateEditCopy(CCmdUI* pCmdUI);
	afx_msg void OnSelectAll();

	DECLARE_MESSAGE_MAP()

private:
	void calc_addr_width(FILE_ADDRESS);     // Also used by recalc_display
	//void draw_bg(CDC* pDC, const CRectAp &doc_rect, bool neg_x, bool neg_y,
	//			 int char_height, int char_width, int char_width_w,
	//			 COLORREF, FILE_ADDRESS start_addr, FILE_ADDRESS end_addr,
	//			 int draw_height = -1);
	void draw_bg(CDC* pDC, const CRectAp &doc_rect, bool neg_x, bool neg_y,
				 int line_height, int char_width, int char_width_w,
				 COLORREF clr, FILE_ADDRESS start_addr, FILE_ADDRESS end_addr,
				 bool merge = true, int draw_height = -1);
	void draw_deletions(CDC* pDC, const std::vector<FILE_ADDRESS> & addr, const std::vector<FILE_ADDRESS> & len,
						FILE_ADDRESS first_virt, FILE_ADDRESS last_virt,
						const CRectAp &doc_rect, bool neg_x, bool neg_y,
						int line_height, int char_width, int char_width_w,
						COLORREF colour);
	void draw_backgrounds(CDC* pDC,
						const std::vector<FILE_ADDRESS> & addr, const std::vector<FILE_ADDRESS> & len,
						FILE_ADDRESS first_virt, FILE_ADDRESS last_virt,
						const CRectAp &doc_rect, bool neg_x, bool neg_y,
						int line_height, int char_width, int char_width_w,
						COLORREF colour, bool merge = true, int draw_height = -1);

	CPointAp addr2pos(FILE_ADDRESS address, int row = 0) const; // Convert byte address in doc to display position
	int hex_pos(int column, int width=0) const // get X coord of hex display column
	{
		if (width == 0) width = phev_->text_width_;
		return (addr_width_ + column*3 + column/phev_->group_by_)*width;
	}
	int char_pos(int column, int widthd=0, int widthw=0) const // get X coord of ASCII/EBCDIC display column
	{
		if (widthd == 0) widthd = phev_->text_width_;
		if (widthw == 0) widthw = phev_->text_width_w_;
		if (phev_->display_.vert_display)
			return addr_width_*widthd +
				   (column + column/phev_->group_by_)*widthw;
		else if (phev_->display_.hex_area)
			return (addr_width_ + phev_->rowsize_*3)*widthd +
				   ((phev_->rowsize_-1)/phev_->group_by_)*widthd +
				   column*widthw;
		else
			return addr_width_*widthd +
				   column*widthw;
	}
	int pos_hex(int, int inside = FALSE) const;  // Closest hex display col given X
	int pos_char(int, int inside = FALSE) const; // Closest char area col given X
	FILE_ADDRESS pos2addr(CPointAp pos, BOOL inside = TRUE) const; // Convert a display position to closest address
	int pos2row(CPointAp pos);                    // Find vert_display row (0, 1, or 2) of display position
	BOOL GetSelAddr(FILE_ADDRESS &start_addr, FILE_ADDRESS &end_addr)
	{
		ASSERT(phev_->line_height_ > 0);
		CPointAp start, end;
		BOOL retval = GetSel(start, end);
		start_addr = pos2addr(start);
		end_addr   = pos2addr(end);
		return retval;
	}

	// Functions for selection tip (sel_tip_)
//    void show_selection_tip();
	void invalidate_addr_range(FILE_ADDRESS, FILE_ADDRESS); // Invalidate hex/aerial display for address range
	void invalidate_hex_addr_range(FILE_ADDRESS start_addr, FILE_ADDRESS end_addr);  // Invalidate hex view only

	void recalc_display();
	int addr_width_;            // How much room in display does address area take?
	int hex_width_, dec_width_, num_width_; // Components of addr_width_

	void begin_change();                        // Store current state etc
	void end_change();                          // Fix display etc
	BOOL previous_caret_displayed_;
	FILE_ADDRESS previous_start_addr_, previous_end_addr_; // selection
	BOOL previous_end_base_;
	int previous_row_;                          // row (0-2) if vert_display
};

#endif // COMPAREVIEW_INCLUDED

/////////////////////////////////////////////////////////////////////////////
