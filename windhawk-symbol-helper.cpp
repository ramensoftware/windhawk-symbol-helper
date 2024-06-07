#include "stdafx.h"

#include "MainDlg.h"

#include "symbols_from_binary.h"

CAppModule _Module;

namespace {

struct CommandLineOptions {
    SymbolsFromBinaryOptions symbolsFromBinaryOptions;
    std::wstring outputFile;
};

std::optional<CommandLineOptions> OptionsFromCommandLine() {
    if (__argc != 8) {
        return std::nullopt;
    }

    const auto is_true = [](const WCHAR* value) {
        return _wcsicmp(value, L"true") == 0 || _wcsicmp(value, L"1") == 0;
    };

    return CommandLineOptions{
        .symbolsFromBinaryOptions =
            {
                .engineDir = __wargv[1],
                .symbolsDir = __wargv[2],
                .symbolServer = __wargv[3],
                .targetExecutable = __wargv[4],
                .undecorated = is_true(__wargv[5]),
                .decorated = is_true(__wargv[6]),
            },
        .outputFile = __wargv[7],
    };
}

// https://stackoverflow.com/a/69410299
std::string WideStringToString(PCWSTR wide_string) {
    int wide_string_len = static_cast<int>(wcslen(wide_string));
    int size_needed = WideCharToMultiByte(
        CP_UTF8, 0, wide_string, wide_string_len, nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) {
        throw std::runtime_error("WideCharToMultiByte() failed: " +
                                 std::to_string(size_needed));
    }

    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide_string, wide_string_len, result.data(),
                        size_needed, nullptr, nullptr);
    return result;
}

bool WriteContentsToFile(PCWSTR path, const void* data, DWORD size) {
    HANDLE hFile = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD dwBytesWritten;
    bool written = WriteFile(hFile, data, size, &dwBytesWritten, nullptr);
    CloseHandle(hFile);

    return written;
}

}  // namespace

int WINAPI _tWinMain(HINSTANCE hInstance,
                     HINSTANCE /*hPrevInstance*/,
                     LPTSTR /*lpstrCmdLine*/,
                     int nCmdShow) {
    if (auto options = OptionsFromCommandLine()) {
        try {
            auto result =
                SymbolsFromBinary(std::move(options->symbolsFromBinaryOptions),
                                  nullptr, nullptr, nullptr);

            auto resultUtf8 = WideStringToString(result);

            DWORD resultUtf8Size = static_cast<DWORD>(resultUtf8.size());
            if (resultUtf8Size < resultUtf8.size()) {
                throw std::runtime_error("Size too large");
            }

            if (!WriteContentsToFile(options->outputFile.c_str(),
                                     resultUtf8.data(), resultUtf8Size)) {
                throw std::runtime_error("WriteFile() failed");
            }

            return 0;
        } catch (const std::exception&) {
            return 1;
        }
    }

    HRESULT hRes = ::CoInitialize(nullptr);
    ATLASSERT(SUCCEEDED(hRes));

    AtlInitCommonControls(
        ICC_BAR_CLASSES);  // add flags to support other controls

    hRes = _Module.Init(nullptr, hInstance);
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
