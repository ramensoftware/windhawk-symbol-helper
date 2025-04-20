#pragma once

// Change these values to use different versions
#define WINVER _WIN32_WINNT_WIN10
#define _WIN32_WINNT _WIN32_WINNT_WIN10
#define _WIN32_IE _WIN32_IE_IE110
#define _RICHEDIT_VER 0x0500

#define WIN32_LEAN_AND_MEAN  // Exclude rarely-used stuff from Windows headers
#define NOMINMAX

// WTL

#define _WTL_NO_CSTRING
#define _WTL_NO_WTYPES
#define _WTL_NO_UNION_CLASSES
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS

#include <atlbase.h>
#include <atlstr.h>
#include <atltypes.h>

#include <atlapp.h>
extern CAppModule _Module;

#include <atlwin.h>

#include <atldlgs.h>  // must be included before atlfind.h

#include <atlcrack.h>
#include <atlctrls.h>
#include <atlfind.h>
#include <atlframe.h>

// Windows

#include <dbghelp.h>

// STL

#include <atomic>
#include <cinttypes>
#include <filesystem>
#include <functional>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <thread>

// Libraries

#include <wil/stl.h>  // must be included before other wil includes

#include <dia/dia2.h>
#include <dia/diacreate.h>

#include <wil/com.h>
#include <wil/win32_helpers.h>
