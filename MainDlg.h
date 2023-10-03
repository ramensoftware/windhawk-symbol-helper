#pragma once

#include "resource.h"
#include "view.h"

class CMainDlg : public CDialogImpl<CMainDlg>,
                 public CMessageFilter,
                 public CDialogResize<CMainDlg> {
   public:
    enum { IDD = IDD_MAINDLG };

    enum {
        UWM_PROGRESS = WM_APP,
        UWM_ENUM_SYMBOLS_DONE,
    };

    BEGIN_DLGRESIZE_MAP(CMainDlg)
        DLGRESIZE_CONTROL(IDC_STATIC_ENGINE_DIR, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_ENGINE_DIR, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_STATIC_SYMBOLS_DIR, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_SYMBOLS_DIR, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_STATIC_SYMBOL_SERVER, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_SYMBOL_SERVER, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_STATIC_TARGET_EXECUTABLE, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_TARGET_EXECUTABLE, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_STATIC_RESULTS, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_RESULTS, DLSZ_SIZE_X | DLSZ_SIZE_Y)
        DLGRESIZE_CONTROL(IDOK, DLSZ_MOVE_Y)
        DLGRESIZE_CONTROL(ID_APP_ABOUT, DLSZ_CENTER_X | DLSZ_MOVE_Y)
        DLGRESIZE_CONTROL(IDCANCEL, DLSZ_MOVE_X | DLSZ_MOVE_Y)
    END_DLGRESIZE_MAP()

   private:
    BEGIN_MSG_MAP(CMainDlg)
        CHAIN_MSG_MAP(CDialogResize<CMainDlg>)
        MSG_WM_INITDIALOG(OnInitDialog)
        MSG_WM_DESTROY(OnDestroy)
        MSG_WM_DROPFILES(OnDropFiles)
        COMMAND_ID_HANDLER_EX(ID_APP_ABOUT, OnAppAbout)
        COMMAND_ID_HANDLER_EX(IDOK, OnOK)
        COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
        MESSAGE_HANDLER_EX(UWM_PROGRESS, OnProgress)
        MESSAGE_HANDLER_EX(UWM_ENUM_SYMBOLS_DONE, OnEnumSymbolsDone)
        CHAIN_COMMANDS_MEMBER(m_resultsEdit)
    END_MSG_MAP()

    BOOL PreTranslateMessage(MSG* pMsg) override;

    BOOL OnInitDialog(CWindow wndFocus, LPARAM lInitParam);
    void OnDestroy();
    void OnDropFiles(HDROP hDropInfo);
    void OnAppAbout(UINT uNotifyCode, int nID, CWindow wndCtl);
    void OnOK(UINT uNotifyCode, int nID, CWindow wndCtl);
    void OnCancel(UINT uNotifyCode, int nID, CWindow wndCtl);
    LRESULT OnProgress(UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT OnEnumSymbolsDone(UINT uMsg, WPARAM wParam, LPARAM lParam);

    HACCEL m_accelerator = nullptr;
    CEditView m_resultsEdit;
    CFont m_resultsEditFont;

    std::optional<std::jthread> m_enumSymbolsThread;
    CString m_enumSymbolsResult;
};
