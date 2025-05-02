#include "stdafx.h"

#include "symbols_from_binary.h"

#include "symbol_enum.h"

CString SymbolsFromBinary(SymbolsFromBinaryOptions options,
                          CStringA* logOutput,
                          std::function<void(int)> progressCallback,
                          std::stop_token* stopToken) {
    CString enumSymbolsResult;

    // Use a set for sorting and to avoid duplicate chunks in the output.
    // Duplicates can happen if the same symbol is listed with multiple types,
    // such as SymTagFunction and SymTagPublicSymbol, or if two variants of a
    // decorated name are identical when undecorated.
    std::set<CString> resultListChunks;
    size_t resultListTotalLength = 0;
    size_t count = 0;

    {
        SymbolEnum::Callbacks callbacks;

        if (stopToken) {
            callbacks.queryCancel = [&stopToken]() {
                return stopToken->stop_requested();
            };
        }

        if (progressCallback) {
            callbacks.notifyProgress = progressCallback;
        }

        if (logOutput) {
            callbacks.notifyLog = [&logOutput](PCSTR message) {
                CStringA messageStr = message;
                // Convert all newlines to LF and trim trailing newlines.
                messageStr.Replace("\r\n", "\n");
                messageStr.Replace('\r', '\n');
                messageStr.TrimRight("\n");

                *logOutput += messageStr;
                *logOutput += "\n";
            };
        }

        CString addressPrefix;
        CString chunk;

        SymbolEnum symbolEnum(
            options.targetExecutable.GetString(), nullptr,
            options.engineDir.GetString(), options.symbolsDir.GetString(),
            options.symbolServer.GetString(),
            options.undecorated ? SymbolEnum::UndecorateMode::Default
                                : SymbolEnum::UndecorateMode::None,
            callbacks);

        while (auto iter = symbolEnum.GetNextSymbol()) {
            if (stopToken && stopToken->stop_requested()) {
                throw std::runtime_error("Canceled");
            }

            if (!options.decorated && !options.undecorated) {
                count++;
                continue;
            }

            if (!iter->name && !iter->nameUndecorated) {
                continue;
            }

            addressPrefix.Format(L"[%08" TEXT(PRIXPTR) L"] ", iter->address);

            chunk.Empty();

            if (options.decorated) {
                chunk += addressPrefix;
                chunk += iter->name ? iter->name : L"<no-decorated-name>";
                chunk += L"\n";
            }

            if (options.undecorated) {
                chunk += addressPrefix;
                if (iter->nameUndecorated) {
                    chunk += iter->nameUndecoratedPrefix1;
                    chunk += iter->nameUndecoratedPrefix2;
                    chunk += iter->nameUndecorated;
                } else {
                    chunk += L"<no-undecorated-name>";
                }
                chunk += L"\n";
            }

            auto [_, inserted] = resultListChunks.insert(chunk);
            if (inserted) {
                resultListTotalLength += chunk.GetLength();
                count++;
            }
        }
    }

    enumSymbolsResult.Preallocate((logOutput ? logOutput->GetLength() : 0) +
                                  static_cast<int>(resultListTotalLength) +
                                  128);

    enumSymbolsResult.Format(L"Found %zu symbols\n", count);

    if (logOutput) {
        enumSymbolsResult += *logOutput;
    }

    for (const auto& chunk : resultListChunks) {
        enumSymbolsResult += chunk;
    }

    return enumSymbolsResult;
}
