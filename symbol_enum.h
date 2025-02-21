#pragma once

class SymbolEnum {
   public:
    enum class UndecorateMode {
        Default = 0,
        OldVersionCompatible,
        None,
    };

    struct Callbacks {
        std::function<bool()> queryCancel;
        std::function<void(int)> notifyProgress;
        std::function<void(PCSTR)> notifyLog;
    };

    SymbolEnum(HMODULE moduleBase,
               PCWSTR enginePath,
               PCWSTR symbolsPath,
               PCWSTR symbolServer,
               UndecorateMode undecorateMode,
               Callbacks callbacks = {});
    SymbolEnum(PCWSTR modulePath,
               HMODULE moduleBase,
               PCWSTR enginePath,
               PCWSTR symbolsPath,
               PCWSTR symbolServer,
               UndecorateMode undecorateMode,
               Callbacks callbacks = {});

    struct Symbol {
        void* address;
        PCWSTR name;
        PCWSTR nameUndecoratedPrefix1;
        PCWSTR nameUndecoratedPrefix2;
        PCWSTR nameUndecorated;
    };

    std::optional<Symbol> GetNextSymbol();

    // https://ntdoc.m417z.com/image_chpe_range_entry
    typedef struct _IMAGE_CHPE_RANGE_ENTRY {
        union {
            ULONG StartOffset;
            struct {
                ULONG NativeCode : 1;
                ULONG AddressBits : 31;
            } DUMMYSTRUCTNAME;
        } DUMMYUNIONNAME;

        ULONG Length;
    } IMAGE_CHPE_RANGE_ENTRY, *PIMAGE_CHPE_RANGE_ENTRY;

   private:
    void InitModuleInfo(PCWSTR modulePath);
    wil::com_ptr<IDiaDataSource> LoadMsdia();

    static constexpr enum SymTagEnum kSymTags[] = {
        SymTagPublicSymbol,
        SymTagFunction,
        SymTagData,
    };

    struct ModuleInfo {
        wil::unique_hmodule moduleLoadedAsImageResources;
        WORD magic;
        bool isHybrid;
        std::span<const IMAGE_CHPE_RANGE_ENTRY> chpeRanges;
    };

    HMODULE m_moduleBase;
    UndecorateMode m_undecorateMode;
    ModuleInfo m_moduleInfo;
    wil::unique_hmodule m_msdiaModule;
    wil::com_ptr<IDiaSymbol> m_diaGlobal;
    wil::com_ptr<IDiaEnumSymbols> m_diaSymbols;
    size_t m_symTagIndex = 0;
    wil::unique_bstr m_currentSymbolName;
    wil::unique_bstr m_currentSymbolNameUndecorated;
};
