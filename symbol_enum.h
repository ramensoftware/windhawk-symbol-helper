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
        PCWSTR nameUndecoratedPrefix;
        PCWSTR nameUndecorated;
    };

    std::optional<Symbol> GetNextSymbol();

   private:
    wil::com_ptr<IDiaDataSource> LoadMsdia();

    static constexpr enum SymTagEnum kSymTags[] = {
        SymTagPublicSymbol,
        SymTagFunction,
        SymTagData,
    };

    HMODULE m_moduleBase;
    UndecorateMode m_undecorateMode;
    wil::unique_hmodule m_msdiaModule;
    wil::com_ptr<IDiaSymbol> m_diaGlobal;
    wil::com_ptr<IDiaEnumSymbols> m_diaSymbols;
    size_t m_symTagIndex = 0;
    wil::unique_bstr m_currentSymbolName;
    wil::unique_bstr m_currentSymbolNameUndecorated;
};
