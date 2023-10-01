#ifndef __FindReplaceDialogWithMessageFilter_h__
#define __FindReplaceDialogWithMessageFilter_h__

#pragma once

#ifndef __cplusplus
	#error ATL requires C++ compilation (use a .cpp suffix)
#endif

#ifndef __ATLAPP_H__
	#error FindReplaceDialogWithMessageFilter.h requires atlapp.h to be included first
#endif

#ifndef __ATLWIN_H__
	#error FindReplaceDialogWithMessageFilter.h requires atlwin.h to be included first
#endif

class CFindReplaceDialogWithMessageFilter :
	public CFindReplaceDialogImpl<CFindReplaceDialogWithMessageFilter>,
	public CMessageFilter
{
protected:
	CMessageLoop* m_messageLoop;

	constexpr static WORD kFindReplaceBufferSize = MAXWORD;  // 64K buffer size.
	std::wstring m_findText = std::wstring(kFindReplaceBufferSize, L'\0');
	std::wstring m_replaceText = std::wstring(kFindReplaceBufferSize, L'\0');

public:
	CFindReplaceDialogWithMessageFilter(CMessageLoop* messageLoop) :
		m_messageLoop(messageLoop) {
		// Override buffers and sizes to have a larger limit. The default limit is
		// 128 bytes, not enough.
		m_fr.lpstrFindWhat = m_findText.data();
		m_fr.wFindWhatLen = kFindReplaceBufferSize;
		m_fr.lpstrReplaceWith = m_replaceText.data();
		m_fr.wReplaceWithLen = kFindReplaceBufferSize;
	}

public:
	BOOL PreTranslateMessage(MSG* pMsg)
	{
		HWND hWndFocus = ::GetFocus();
		if((m_hWnd == hWndFocus) || this->IsChild(hWndFocus))
			return this->IsDialogMessage(pMsg);

		return FALSE;
	}

	virtual void OnFinalMessage(HWND /*hWnd*/)
	{
		delete this;
	}

	BEGIN_MSG_MAP(CFindReplaceDialogWithMessageFilter)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		if(m_messageLoop)
			m_messageLoop->AddMessageFilter(this);

		bHandled = FALSE;
		return 0;
	}

	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		if(m_messageLoop)
			m_messageLoop->RemoveMessageFilter(this);

		bHandled = FALSE;
		return 0;
	}
};

#endif //__FindReplaceDialogWithMessageFilter_h__
