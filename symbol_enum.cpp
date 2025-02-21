#include "stdafx.h"

#include "symbol_enum.h"

namespace {

std::filesystem::path g_enginePath;
SymbolEnum::Callbacks* g_symbolServerCallbacks;

void VLogLine(PCWSTR format, va_list args) {
    WCHAR buffer[1025];
    int len = _vsnwprintf_s(buffer, _TRUNCATE, format, args);
    if (len == -1) {
        // Truncation occurred.
        len = _countof(buffer) - 1;
    }

    while (--len >= 0 && buffer[len] == L'\n') {
        // Skip all newlines at the end.
    }

    // Leave only a single trailing newline.
    if (buffer[len + 1] == L'\n' && buffer[len + 2] == L'\n') {
        buffer[len + 2] = L'\0';
    }

    OutputDebugString(buffer);
}

void LogLine(PCWSTR format, ...) {
    va_list args;
    va_start(args, format);
    VLogLine(format, args);
    va_end(args);
}

#define LOG(format, ...) \
    LogLine(L"[!] SymbolEnum (%S): " format L"\n", __FUNCTION__, __VA_ARGS__)
#define VERBOSE(format, ...) \
    LogLine(L"[+] SymbolEnum (%S): " format L"\n", __FUNCTION__, __VA_ARGS__)

std::wstring GetSymbolsSearchPath(PCWSTR symbolsPath, PCWSTR symbolServer) {
    std::wstring symSearchPath = L"srv*";
    symSearchPath += symbolsPath;
    symSearchPath += L'*';
    symSearchPath += symbolServer;

    return symSearchPath;
}

void LogSymbolServerEvent(PCSTR msg) {
    // Trim leading and trailing whitespace and control characters (mainly \b
    // which is used for console output).

    PCSTR p = msg;
    while (*p != '\0' && (isspace(*p) || iscntrl(*p))) {
        p++;
    }

    if (*p == '\0') {
        return;
    }

    size_t len = strlen(p);
    while (len > 0 && (isspace(p[len - 1]) || iscntrl(p[len - 1]))) {
        len--;
    }

    VERBOSE(L"%.*S", static_cast<int>(len), p);
}

int PercentFromSymbolServerEvent(PCSTR msg) {
    size_t msgLen = strlen(msg);
    while (msgLen > 0 && isspace(msg[msgLen - 1])) {
        msgLen--;
    }

    constexpr char suffix[] = " percent";
    constexpr size_t suffixLen = ARRAYSIZE(suffix) - 1;

    if (msgLen <= suffixLen ||
        strncmp(suffix, msg + msgLen - suffixLen, suffixLen) != 0) {
        return -1;
    }

    char percentStr[] = "000";
    int digitsCount = 0;

    for (size_t i = 1; i <= 3; i++) {
        if (i > msgLen - suffixLen) {
            break;
        }

        char p = msg[msgLen - suffixLen - i];
        if (p < '0' || p > '9') {
            break;
        }

        percentStr[3 - i] = p;
        digitsCount++;
    }

    if (digitsCount == 0) {
        return -1;
    }

    int percent = (percentStr[0] - '0') * 100 + (percentStr[1] - '0') * 10 +
                  (percentStr[2] - '0');
    if (percent > 100) {
        return -1;
    }

    return percent;
}

void** FindImportPtr(HMODULE hFindInModule,
                     PCSTR pModuleName,
                     PCSTR pImportName) {
    IMAGE_DOS_HEADER* pDosHeader = (IMAGE_DOS_HEADER*)hFindInModule;
    IMAGE_NT_HEADERS* pNtHeader =
        (IMAGE_NT_HEADERS*)((char*)pDosHeader + pDosHeader->e_lfanew);

    if (pNtHeader->OptionalHeader.NumberOfRvaAndSizes <=
            IMAGE_DIRECTORY_ENTRY_IMPORT ||
        !pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]
             .VirtualAddress) {
        return nullptr;
    }

    ULONG_PTR ImageBase = (ULONG_PTR)hFindInModule;
    IMAGE_IMPORT_DESCRIPTOR* pImportDescriptor =
        (IMAGE_IMPORT_DESCRIPTOR*)(ImageBase +
                                   pNtHeader->OptionalHeader
                                       .DataDirectory
                                           [IMAGE_DIRECTORY_ENTRY_IMPORT]
                                       .VirtualAddress);

    while (pImportDescriptor->OriginalFirstThunk) {
        if (_stricmp((char*)(ImageBase + pImportDescriptor->Name),
                     pModuleName) == 0) {
            IMAGE_THUNK_DATA* pOriginalFirstThunk =
                (IMAGE_THUNK_DATA*)(ImageBase +
                                    pImportDescriptor->OriginalFirstThunk);
            IMAGE_THUNK_DATA* pFirstThunk =
                (IMAGE_THUNK_DATA*)(ImageBase + pImportDescriptor->FirstThunk);

            while (ULONG_PTR ImageImportByName =
                       pOriginalFirstThunk->u1.Function) {
                if (!IMAGE_SNAP_BY_ORDINAL(ImageImportByName)) {
                    if ((ULONG_PTR)pImportName & ~0xFFFF) {
                        ImageImportByName += sizeof(WORD);

                        if (strcmp((char*)(ImageBase + ImageImportByName),
                                   pImportName) == 0) {
                            return (void**)pFirstThunk;
                        }
                    }
                } else {
                    if (((ULONG_PTR)pImportName & ~0xFFFF) == 0) {
                        if (IMAGE_ORDINAL(ImageImportByName) ==
                            (ULONG_PTR)pImportName) {
                            return (void**)pFirstThunk;
                        }
                    }
                }

                pOriginalFirstThunk++;
                pFirstThunk++;
            }
        }

        pImportDescriptor++;
    }

    return nullptr;
}

BOOL CALLBACK SymbolServerCallback(UINT_PTR action,
                                   ULONG64 data,
                                   ULONG64 context) {
    SymbolEnum::Callbacks* callbacks = g_symbolServerCallbacks;
    if (!callbacks) {
        return FALSE;
    }

    switch (action) {
        case SSRVACTION_QUERYCANCEL: {
            if (callbacks->queryCancel) {
                ULONG64* doCancel = (ULONG64*)data;
                *doCancel = callbacks->queryCancel();
                return TRUE;
            }
            return FALSE;
        }

        case SSRVACTION_EVENT: {
            IMAGEHLP_CBA_EVENT* evt = (IMAGEHLP_CBA_EVENT*)data;
            LogSymbolServerEvent(evt->desc);
            if (callbacks->notifyLog) {
                callbacks->notifyLog(evt->desc);
            }
            int percent = PercentFromSymbolServerEvent(evt->desc);
            if (percent >= 0 && callbacks->notifyProgress) {
                callbacks->notifyProgress(percent);
            }
            return TRUE;
        }
    }

    return FALSE;
}

struct DiaLoadCallback : public IDiaLoadCallback2 {
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                             void** ppvObject) override {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IDiaLoadCallback)) {
            *ppvObject = static_cast<IDiaLoadCallback*>(this);
            return S_OK;
        } else if (riid == __uuidof(IDiaLoadCallback2)) {
            *ppvObject = static_cast<IDiaLoadCallback2*>(this);
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return 2;  // On stack
    }

    ULONG STDMETHODCALLTYPE Release() override {
        return 1;  // On stack
    }

    HRESULT STDMETHODCALLTYPE NotifyDebugDir(BOOL fExecutable,
                                             DWORD cbData,
                                             BYTE* pbData) override {
        // VERBOSE(L"Debug directory found in %s file",
        //         fExecutable ? L"exe" : L"dbg");
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE NotifyOpenDBG(LPCOLESTR dbgPath,
                                            HRESULT resultCode) override {
        VERBOSE(L"Opened dbg file %s: %s (%08X)", dbgPath,
                resultCode == S_OK ? L"success" : L"error", resultCode);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE NotifyOpenPDB(LPCOLESTR pdbPath,
                                            HRESULT resultCode) override {
        VERBOSE(L"Opened pdb file %s: %s (%08X)", pdbPath,
                resultCode == S_OK ? L"success" : L"error", resultCode);
        return S_OK;
    }

    // Only use explicitly specified search paths, restrict all but symbol
    // server access:
    HRESULT STDMETHODCALLTYPE RestrictRegistryAccess() override {
        return E_FAIL;
    }
    HRESULT STDMETHODCALLTYPE RestrictSymbolServerAccess() override {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE RestrictOriginalPathAccess() override {
        return E_FAIL;
    }
    HRESULT STDMETHODCALLTYPE RestrictReferencePathAccess() override {
        return E_FAIL;
    }
    HRESULT STDMETHODCALLTYPE RestrictDBGAccess() override { return E_FAIL; }
    HRESULT STDMETHODCALLTYPE RestrictSystemRootAccess() override {
        return E_FAIL;
    }
};

HMODULE WINAPI MsdiaLoadLibraryExWHook(LPCWSTR lpLibFileName,
                                       HANDLE hFile,
                                       DWORD dwFlags) {
    if (wcscmp(lpLibFileName, L"SYMSRV.DLL") != 0) {
        return LoadLibraryExW(lpLibFileName, hFile, dwFlags);
    }

    try {
        DWORD dwNewFlags = dwFlags;
        dwNewFlags |= LOAD_WITH_ALTERED_SEARCH_PATH;

        // Strip flags incompatible with LOAD_WITH_ALTERED_SEARCH_PATH.
        dwNewFlags &= ~LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR;
        dwNewFlags &= ~LOAD_LIBRARY_SEARCH_APPLICATION_DIR;
        dwNewFlags &= ~LOAD_LIBRARY_SEARCH_USER_DIRS;
        dwNewFlags &= ~LOAD_LIBRARY_SEARCH_SYSTEM32;
        dwNewFlags &= ~LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;

        auto symsrvPath = g_enginePath / L"symsrv_windhawk.dll";
        HMODULE symsrvModule =
            LoadLibraryExW(symsrvPath.c_str(), hFile, dwNewFlags);
        if (!symsrvModule) {
            DWORD error = GetLastError();
            LOG(L"Couldn't load symsrv: %u", error);
            SetLastError(error);
            return symsrvModule;
        }

        PSYMBOLSERVERSETOPTIONSPROC pSymbolServerSetOptions =
            reinterpret_cast<PSYMBOLSERVERSETOPTIONSPROC>(
                GetProcAddress(symsrvModule, "SymbolServerSetOptions"));
        if (pSymbolServerSetOptions) {
            pSymbolServerSetOptions(SSRVOPT_UNATTENDED, TRUE);
            pSymbolServerSetOptions(SSRVOPT_CALLBACK,
                                    (ULONG_PTR)SymbolServerCallback);
            pSymbolServerSetOptions(SSRVOPT_TRACE, TRUE);
        } else {
            LOG(L"Couldn't find SymbolServerSetOptions");
        }

        return symsrvModule;
    } catch (const std::exception& e) {
        LOG(L"Couldn't load symsrv: %S", e.what());
        SetLastError(ERROR_MOD_NOT_FOUND);
        return nullptr;
    }
}

}  // namespace

SymbolEnum::SymbolEnum(HMODULE moduleBase,
                       PCWSTR enginePath,
                       PCWSTR symbolsPath,
                       PCWSTR symbolServer,
                       UndecorateMode undecorateMode,
                       Callbacks callbacks) {
    if (!moduleBase) {
        moduleBase = GetModuleHandle(nullptr);
    }

    std::wstring modulePath = wil::GetModuleFileName<std::wstring>(moduleBase);

    SymbolEnum(modulePath.c_str(), moduleBase, enginePath, symbolsPath,
               symbolServer, undecorateMode, std::move(callbacks));
}

SymbolEnum::SymbolEnum(PCWSTR modulePath,
                       HMODULE moduleBase,
                       PCWSTR enginePath,
                       PCWSTR symbolsPath,
                       PCWSTR symbolServer,
                       UndecorateMode undecorateMode,
                       Callbacks callbacks)
    : m_moduleBase(moduleBase), m_undecorateMode(undecorateMode) {
#ifdef _WIN64
    g_enginePath = std::filesystem::path(enginePath) / L"64";
#else
    g_enginePath = std::filesystem::path(enginePath) / L"32";
#endif

    wil::com_ptr<IDiaDataSource> diaSource = LoadMsdia();

    std::wstring symSearchPath =
        GetSymbolsSearchPath(symbolsPath, symbolServer);

    g_symbolServerCallbacks = &callbacks;
    auto msdiaCallbacksCleanup =
        wil::scope_exit([] { g_symbolServerCallbacks = nullptr; });

    DiaLoadCallback diaLoadCallback;
    THROW_IF_FAILED(diaSource->loadDataForExe(modulePath, symSearchPath.c_str(),
                                              &diaLoadCallback));

    wil::com_ptr<IDiaSession> diaSession;
    THROW_IF_FAILED(diaSource->openSession(&diaSession));

    THROW_IF_FAILED(diaSession->get_globalScope(&m_diaGlobal));

    THROW_IF_FAILED(
        m_diaGlobal->findChildren(kSymTags[0], nullptr, nsNone, &m_diaSymbols));
}

std::optional<SymbolEnum::Symbol> SymbolEnum::GetNextSymbol() {
    while (true) {
        wil::com_ptr<IDiaSymbol> diaSymbol;
        ULONG count = 0;
        HRESULT hr = m_diaSymbols->Next(1, &diaSymbol, &count);
        THROW_IF_FAILED(hr);

        if (hr == S_FALSE || count == 0) {
            m_symTagIndex++;
            if (m_symTagIndex < ARRAYSIZE(kSymTags)) {
                THROW_IF_FAILED(m_diaGlobal->findChildren(
                    kSymTags[m_symTagIndex], nullptr, nsNone, &m_diaSymbols));
                continue;
            }

            return std::nullopt;
        }

        DWORD currentSymbolRva;
        hr = diaSymbol->get_relativeVirtualAddress(&currentSymbolRva);
        THROW_IF_FAILED(hr);
        if (hr == S_FALSE) {
            continue;  // no RVA
        }

        hr = diaSymbol->get_name(&m_currentSymbolName);
        THROW_IF_FAILED(hr);
        if (hr == S_FALSE) {
            m_currentSymbolName.reset();  // no name
        }

        PCWSTR currentSymbolNameUndecoratedPrefix = nullptr;

        // Temporary compatibility code.
        if (m_undecorateMode == UndecorateMode::OldVersionCompatible) {
            // get_undecoratedName uses 0x20800 as flags:
            // * UNDNAME_32_BIT_DECODE (0x800)
            // * UNDNAME_NO_PTR64 (0x20000)
            // For some reason, the old msdia version still included ptr64 in
            // the output. For compatibility, use get_undecoratedNameEx and
            // don't pass this flag.
            constexpr DWORD kUndname32BitDecode = 0x800;
            hr = diaSymbol->get_undecoratedNameEx(
                kUndname32BitDecode, &m_currentSymbolNameUndecorated);
        } else if (m_undecorateMode == UndecorateMode::Default) {
            hr =
                diaSymbol->get_undecoratedName(&m_currentSymbolNameUndecorated);
        } else {
            m_currentSymbolNameUndecorated.reset();
            hr = S_OK;
        }
        THROW_IF_FAILED(hr);
        if (hr == S_FALSE) {
            m_currentSymbolNameUndecorated.reset();  // no name
        } else if (m_currentSymbolNameUndecorated) {
            // For ARM64EC binaries, functions with native and ARM64EC versions
            // have the same undecorated names. The only difference between them
            // is the "$$h" tag. This tag is mentioned here:
            // https://learn.microsoft.com/en-us/cpp/build/reference/decorated-names?view=msvc-170
            // An example from comctl32.dll version 6.10.22621.4825:
            // Decorated, native:
            // ??1CLink@@UEAA@XZ
            // Decorated, ARM64EC:
            // ??1CLink@@$$hUEAA@XZ
            // Undecorated (in both cases):
            // public: virtual __cdecl CLink::~CLink(void)
            //
            // To be able to disambiguate between these two undecorated names,
            // we add a prefix to the ARM64EC undecorated name. In the above
            // example, it becomes:
            // ARM64EC\public: virtual __cdecl CLink::~CLink(void)
            //
            // The "\" symbol was chosen after looking for an ASCII character
            // that's not being used in symbol names. It looks like the only
            // three such characters in the ASCII range of 0x21-0x7E are: " ; \.
            // Note: The # character doesn't seem to be used outside of ARM64
            // symbols, but it's being used extensively as an ARM64-related
            // marker.
            //
            // Below is a simplistic check that only checks that the "$$h"
            // string is present in the symbol name. Hopefully it's good enough
            // so that full parsing of the decorated name is not needed.
            bool isArm64Ec =
                m_currentSymbolName &&
                wcsstr(m_currentSymbolName.get(), L"$$h") != nullptr;
            if (isArm64Ec) {
                currentSymbolNameUndecoratedPrefix = L"ARM64EC\\";
            }
        }

        return SymbolEnum::Symbol{
            reinterpret_cast<void*>(reinterpret_cast<BYTE*>(m_moduleBase) +
                                    currentSymbolRva),
            m_currentSymbolName.get(), currentSymbolNameUndecoratedPrefix,
            m_currentSymbolNameUndecorated.get()};
    }
}

wil::com_ptr<IDiaDataSource> SymbolEnum::LoadMsdia() {
    auto msdiaPath = g_enginePath / L"msdia140_windhawk.dll";

    m_msdiaModule.reset(LoadLibraryEx(msdiaPath.c_str(), nullptr,
                                      LOAD_WITH_ALTERED_SEARCH_PATH));
    THROW_LAST_ERROR_IF_NULL(m_msdiaModule);

    // msdia loads symsrv.dll by using the following call:
    // LoadLibraryExW(L"SYMSRV.DLL");
    // This is problematic for the following reasons:
    // * If another file named symsrv.dll is already loaded,
    //   it will be used instead.
    // * If not, the library loading search path doesn't include our folder
    //   by default.
    // Especially due to the first point, we patch msdia in memory to use
    // the full path to our copy of symsrv.dll.
    // Also, to prevent from other msdia instances to load our version of
    // symsrv, we name it differently.

    void** msdiaLoadLibraryExWPtr =
        FindImportPtr(m_msdiaModule.get(), "kernel32.dll", "LoadLibraryExW");

    DWORD dwOldProtect;
    THROW_IF_WIN32_BOOL_FALSE(
        VirtualProtect(msdiaLoadLibraryExWPtr, sizeof(*msdiaLoadLibraryExWPtr),
                       PAGE_EXECUTE_READWRITE, &dwOldProtect));
    *msdiaLoadLibraryExWPtr = MsdiaLoadLibraryExWHook;
    THROW_IF_WIN32_BOOL_FALSE(VirtualProtect(msdiaLoadLibraryExWPtr,
                                             sizeof(*msdiaLoadLibraryExWPtr),
                                             dwOldProtect, &dwOldProtect));

    wil::com_ptr<IDiaDataSource> diaSource;
    THROW_IF_FAILED(NoRegCoCreate(msdiaPath.c_str(), CLSID_DiaSource,
                                  IID_PPV_ARGS(&diaSource)));

    // Decrements the reference count incremented by NoRegCoCreate.
    FreeLibrary(m_msdiaModule.get());

    return diaSource;
}
