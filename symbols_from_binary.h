#pragma once

struct SymbolsFromBinaryOptions {
    CString engineDir;
    CString symbolsDir;
    CString symbolServer;
    CString targetExecutable;
    bool undecorated;
    bool decorated;
};

CString SymbolsFromBinary(SymbolsFromBinaryOptions options,
                          CStringA* logOutput,
                          std::function<void(int)> progressCallback,
                          std::stop_token* stopToken);
