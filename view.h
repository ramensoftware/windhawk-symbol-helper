#include "finddlg.h"

class CEditView : 
	public CWindowImpl<CEditView, CEdit>,
	public CEditCommands<CEditView>,
	public CEditFindReplaceImpl<CEditView, CFindReplaceDialogWithMessageFilter>
{
protected:
	typedef CEditView thisClass;
	typedef CEditCommands<CEditView> editCommandsClass;
	typedef CEditFindReplaceImpl<CEditView, CFindReplaceDialogWithMessageFilter> findReplaceClass;

public:
	DECLARE_WND_SUPERCLASS(NULL, CEdit::GetWndClassName())

	BOOL PreTranslateMessage(MSG* pMsg)
	{
		// In non Multi-thread SDI cases, CFindReplaceDialogWithMessageFilter will add itself to the
		// global message filters list.  In our case, we'll call it directly.
		if(m_pFindReplaceDialog != NULL)
		{
			if(m_pFindReplaceDialog->PreTranslateMessage(pMsg))
				return TRUE;
		}
		return FALSE;
	}

	BEGIN_MSG_MAP(CEditView)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)

		CHAIN_MSG_MAP_ALT(editCommandsClass, 1)

		CHAIN_MSG_MAP_ALT(findReplaceClass, 1)
	END_MSG_MAP()
	
	LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
	{
		LRESULT lRet = DefWindowProc(uMsg, wParam, lParam);
		
		LimitText(0);

		return lRet;
	}

// Overrides from CEditFindReplaceImpl
	CFindReplaceDialogWithMessageFilter* CreateFindReplaceDialog(BOOL bFindOnly, // TRUE for Find, FALSE for FindReplace
			LPCTSTR lpszFindWhat,
			LPCTSTR lpszReplaceWith = NULL,
			DWORD dwFlags = FR_DOWN,
			HWND hWndParent = NULL)
	{
		// In non Multi-Threaded SDI cases, we'd pass in the message loop to CFindReplaceDialogWithMessageFilter.
		// In our case, we'll call PreTranslateMessage directly from this class.
		//CFindReplaceDialogWithMessageFilter* findReplaceDialog =
		//	new CFindReplaceDialogWithMessageFilter(_Module.GetMessageLoop());
		CFindReplaceDialogWithMessageFilter* findReplaceDialog =
			new CFindReplaceDialogWithMessageFilter(NULL);

		if(findReplaceDialog == NULL)
		{
			::MessageBeep(MB_ICONHAND);
		}
		else
		{
			HWND hWndFindReplace = findReplaceDialog->Create(bFindOnly,
				lpszFindWhat, lpszReplaceWith, dwFlags, hWndParent);
			if(hWndFindReplace == NULL)
			{
				delete findReplaceDialog;
				findReplaceDialog = NULL;
			}
			else
			{
				findReplaceDialog->SetActiveWindow();
				findReplaceDialog->ShowWindow(SW_SHOW);
			}
		}

		return findReplaceDialog;
	}

	DWORD GetFindReplaceDialogFlags(void) const
	{
		DWORD dwFlags = FR_HIDEWHOLEWORD;

		if(m_bFindDown)
			dwFlags |= FR_DOWN;
		if(m_bMatchCase)
			dwFlags |= FR_MATCHCASE;

		return dwFlags;
	}
};
