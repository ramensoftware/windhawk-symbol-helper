#include "stdafx.h"

#include "MainDlg.h"

CAppModule _Module;

int WINAPI _tWinMain(HINSTANCE hInstance,
                     HINSTANCE /*hPrevInstance*/,
                     LPTSTR /*lpstrCmdLine*/,
                     int nCmdShow) {
    HRESULT hRes = ::CoInitialize(NULL);
    ATLASSERT(SUCCEEDED(hRes));

    AtlInitCommonControls(
        ICC_BAR_CLASSES);  // add flags to support other controls

    hRes = _Module.Init(NULL, hInstance);
    ATLASSERT(SUCCEEDED(hRes));

    CMessageLoop theLoop;
    _Module.AddMessageLoop(&theLoop);

    CMainDlg dlgMain;
    int nRet = 0;

    if (dlgMain.Create(nullptr)) {
        dlgMain.ShowWindow(nCmdShow);
        nRet = theLoop.Run();
    } else {
        ATLTRACE(L"Main dialog creation failed!\n");
    }

    _Module.RemoveMessageLoop();

    _Module.Term();
    ::CoUninitialize();

    return nRet;
}
