#include "stdafx.h"

#include "MainDlg.h"

#include "symbol_enum.h"

namespace {

void OpenUrl(HWND hWnd, PCWSTR url) {
    if ((INT_PTR)ShellExecute(hWnd, L"open", url, nullptr, nullptr,
                              SW_SHOWNORMAL) <= 32) {
        CString errorMsg;
        errorMsg.Format(
            L"Failed to open link, you can copy it with Ctrl+C and open it in "
            L"a browser manually:\n\n%s",
            url);
        MessageBox(hWnd, errorMsg, nullptr, MB_ICONHAND);
    }
}

}  // namespace

BOOL CMainDlg::PreTranslateMessage(MSG* pMsg) {
    if (m_resultsEdit.PreTranslateMessage(pMsg)) {
        return TRUE;
    }

    if (m_accelerator && ::TranslateAccelerator(m_hWnd, m_accelerator, pMsg)) {
        return TRUE;
    }

    return CWindow::IsDialogMessage(pMsg);
}

BOOL CMainDlg::OnInitDialog(CWindow wndFocus, LPARAM lInitParam) {
    // Center the dialog on the screen.
    CenterWindow();

    // Set icons.
    HICON hIcon = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR,
                                   ::GetSystemMetrics(SM_CXICON),
                                   ::GetSystemMetrics(SM_CYICON));
    SetIcon(hIcon, TRUE);
    HICON hIconSmall = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR,
                                        ::GetSystemMetrics(SM_CXSMICON),
                                        ::GetSystemMetrics(SM_CYSMICON));
    SetIcon(hIconSmall, FALSE);

    // Bind keys...
    m_accelerator = AtlLoadAccelerators(IDR_MAINFRAME);

    // Register object for message filtering and idle updates.
    CMessageLoop* pLoop = _Module.GetMessageLoop();
    ATLASSERT(pLoop != nullptr);
    pLoop->AddMessageFilter(this);

    // Populate values.
    CEdit(GetDlgItem(IDC_ENGINE_DIR))
        .SetWindowText(LR"(C:\Program Files\Windhawk\Engine\1.3.1)");
    CEdit(GetDlgItem(IDC_SYMBOLS_DIR))
        .SetWindowText(LR"(C:\ProgramData\Windhawk\Engine\Symbols)");
    CEdit(GetDlgItem(IDC_SYMBOL_SERVER))
        .SetWindowText(LR"(https://msdl.microsoft.com/download/symbols)");
    CEdit(GetDlgItem(IDC_TARGET_EXECUTABLE))
        .SetWindowText(LR"(C:\Windows\Explorer.exe)");
    CButton(GetDlgItem(IDC_UNDECORATED)).SetCheck(BST_CHECKED);

    // Init edit control.
    auto resultsPlaceholderControl = GetDlgItem(IDC_RESULTS_PLACEHOLDER);

    CRect rc;
    resultsPlaceholderControl.GetClientRect(&rc);
    resultsPlaceholderControl.MapWindowPoints(m_hWnd, rc);
    m_resultsEdit.Create(m_hWnd, rc, nullptr,
                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE |
                             ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN |
                             ES_NOHIDESEL | WS_VSCROLL | WS_HSCROLL,
                         WS_EX_CLIENTEDGE, IDC_RESULTS);

    CLogFont fontAttributes(AtlGetDefaultGuiFont());
    wcscpy_s(fontAttributes.lfFaceName, L"Consolas");
    m_resultsEditFont = fontAttributes.CreateFontIndirect();
    m_resultsEdit.SetFont(m_resultsEditFont);

    // Init resizing.
    DlgResize_Init();

    return TRUE;
}

void CMainDlg::OnDestroy() {
    m_enumSymbolsThread.reset();

    PostQuitMessage(0);
}

void CMainDlg::OnAppAbout(UINT uNotifyCode, int nID, CWindow wndCtl) {
    PCWSTR content =
        L"A tool to get symbols from executables the same way Windhawk mods do "
        L"with the symbols API.\n\n"
        L"The tool was created to help with Windhawk mod development.\n\n"
        L"Windhawk can be downloaded at <A "
        L"HREF=\"https://windhawk.net\">windhawk.net</A>.\n\n"
        L"Compiled at " __DATE__ " " __TIME__ "\n\n";

    TASKDIALOGCONFIG taskDialogConfig{
        .cbSize = sizeof(taskDialogConfig),
        .hwndParent = m_hWnd,
        .hInstance = _Module.GetModuleInstance(),
        .dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION |
                   TDF_POSITION_RELATIVE_TO_WINDOW,
        .pszWindowTitle = L"About",
        .pszMainIcon = MAKEINTRESOURCE(IDR_MAINFRAME),
        .pszMainInstruction = L"Windhawk Symbol Helper",
        .pszContent = content,
        .pfCallback = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                         LONG_PTR lpRefData) -> HRESULT {
            switch (msg) {
                case TDN_HYPERLINK_CLICKED:
                    OpenUrl(hwnd, (PCWSTR)lParam);
                    break;
            }

            return S_OK;
        },
    };

    ::TaskDialogIndirect(&taskDialogConfig, nullptr, nullptr, nullptr);
}

void CMainDlg::OnOK(UINT uNotifyCode, int nID, CWindow wndCtl) {
    if (m_enumSymbolsThread) {
        m_enumSymbolsThread->request_stop();
        GetDlgItem(IDOK).EnableWindow(FALSE);
        return;
    }

    struct {
        CString engineDir;
        CString symbolsDir;
        CString symbolServer;
        CString targetExecutable;
        bool undecorated;
        bool decorated;
        bool log;
    } threadParams;

    GetDlgItemText(IDC_ENGINE_DIR, threadParams.engineDir);
    GetDlgItemText(IDC_SYMBOLS_DIR, threadParams.symbolsDir);
    GetDlgItemText(IDC_SYMBOL_SERVER, threadParams.symbolServer);
    GetDlgItemText(IDC_TARGET_EXECUTABLE, threadParams.targetExecutable);
    threadParams.undecorated =
        CButton(GetDlgItem(IDC_UNDECORATED)).GetCheck() != BST_UNCHECKED;
    threadParams.decorated =
        CButton(GetDlgItem(IDC_DECORATED)).GetCheck() != BST_UNCHECKED;
    threadParams.log = CButton(GetDlgItem(IDC_LOG)).GetCheck() != BST_UNCHECKED;

    SetDlgItemText(IDOK, L"Cancel");

    m_enumSymbolsThread = std::jthread([threadParams = std::move(threadParams),
                                        &enumSymbolsResult =
                                            m_enumSymbolsResult,
                                        hWnd =
                                            m_hWnd](std::stop_token stopToken) {
        CStringA logOutput;

        try {
            CString result;
            int count = 0;

            {
                SymbolEnum::Callbacks callbacks{
                    .queryCancel =
                        [&stopToken]() { return stopToken.stop_requested(); },
                    .notifyProgress =
                        [hWnd](int progress) {
                            CWindow(hWnd).PostMessage(UWM_PROGRESS,
                                                      (WPARAM)progress);
                        },
                };

                if (threadParams.log) {
                    callbacks.notifyLog = [&logOutput](PCSTR message) {
                        logOutput += message;
                        logOutput += "\r\n";
                    };
                }

                SymbolEnum symbolEnum(threadParams.targetExecutable.GetString(),
                                      0, threadParams.engineDir.GetString(),
                                      threadParams.symbolsDir.GetString(),
                                      threadParams.symbolServer.GetString(),
                                      callbacks);

                while (auto iter = symbolEnum.GetNextSymbol(false)) {
                    if (stopToken.stop_requested()) {
                        throw std::runtime_error("Cancelled");
                    }

                    CString addressStr;
                    addressStr.Format(L"%p", iter->address);

                    if (threadParams.undecorated) {
                        result += L"[";
                        result += addressStr;
                        result += L"] ";
                        result += iter->name;
                        result += L"\r\n";
                    }

                    if (threadParams.decorated) {
                        result += L"[";
                        result += addressStr;
                        result += L"] ";
                        result += iter->nameDecorated;
                        result += L"\r\n";
                    }

                    count++;
                }
            }

            enumSymbolsResult.Format(L"Found %d symbols\r\n%S%s", count,
                                     logOutput.GetString(), result.GetString());
        } catch (const std::exception& e) {
            enumSymbolsResult.Format(L"Error: %S\r\n%S", e.what(),
                                     logOutput.GetString());
        }

        CWindow(hWnd).PostMessage(UWM_ENUM_SYMBOLS_DONE);
    });
}

void CMainDlg::OnCancel(UINT uNotifyCode, int nID, CWindow wndCtl) {
    DestroyWindow();
}

LRESULT CMainDlg::OnProgress(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    int progress = (int)wParam;

    CString text;
    text.Format(L"[%d%%] Cancel", progress);

    SetDlgItemText(IDOK, text);

    return 0;
}

LRESULT CMainDlg::OnEnumSymbolsDone(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    m_enumSymbolsThread.reset();

    SetDlgItemText(IDC_RESULTS, m_enumSymbolsResult);
    m_enumSymbolsResult.Empty();

    SetDlgItemText(IDOK, L"Get &symbols");
    GetDlgItem(IDOK).EnableWindow(TRUE);

    return 0;
}
