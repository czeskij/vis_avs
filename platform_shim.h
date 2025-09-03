// Minimal cross-platform shim to let legacy AVS sources compile on non-Windows.
// This is an initial stub; many Win32/GDI/UI features are no-ops outside Windows.
// We'll progressively replace usages with portable abstractions.

#pragma once

#ifdef _WIN32
  #include <windows.h>
#else
// Standard headers
#ifdef __cplusplus
  #include <cstdint>
  #include <cstddef>
  #include <cstring>
  #include <cstdio>
  #include <cstdlib>
  #include <string>
  #include <chrono>
  #include <thread>
#else
  #include <stdint.h>
  #include <stddef.h>
  #include <string.h>
  #include <stdio.h>
  #include <stdlib.h>
  #include <time.h>
#endif

typedef void* HANDLE;
typedef void* HINSTANCE;
typedef void* HWND;
typedef void* HDC;
typedef void* HBITMAP;
typedef void* HFONT;
typedef void* HPEN;
typedef void* HBRUSH;
typedef void* HGDIOBJ;
typedef void* HICON;
typedef void* HCURSOR;
typedef unsigned long DWORD;
typedef unsigned int UINT;
typedef unsigned char BYTE;
typedef long LONG;
typedef int BOOL;
typedef void* LPVOID;
typedef const char* LPCSTR;
typedef char* LPSTR;
typedef unsigned long COLORREF;
typedef long LPARAM;
typedef unsigned long WPARAM;
typedef long LRESULT;
typedef unsigned long long ULONG_PTR;
typedef DWORD* LPDWORD;

struct RECT { LONG left, top, right, bottom; };
struct POINT { LONG x, y; };
struct SIZE { LONG cx, cy; };

// Basic macros/constants
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL 0
#endif

#define CALLBACK
#define WINAPI
#define APIENTRY

#ifndef __declspec
#define __declspec(x)
#endif

#define MAX_PATH 1024

// Message / control constants (subset; extend as needed)
#define WM_USER 0x0400
#define WM_COMMAND 0x0111
#define WM_INITDIALOG 0x0110

// Edit control notifications
#define EN_CHANGE 0x0300
#define EN_KILLFOCUS 0x0200

// LOWORD/HWORD helpers
#ifndef LOWORD
#define LOWORD(l) ((unsigned short)((l) & 0xFFFF))
#endif
#ifndef HIWORD
#define HIWORD(l) ((unsigned short)(((l) >> 16) & 0xFFFF))
#endif

// Trackbar (slider) messages used for config dialogs.
#define TBM_GETPOS 0x0400
#define TBM_SETRANGE (WM_USER+6)
#define TBM_SETPOS (WM_USER+5)

// Tree view expand constants placeholders
#define TVM_EXPAND 0x1102
#define TVE_EXPAND 0x0002

// Button / checkbox constants
#define BM_SETCHECK 0x00F1
#define BST_CHECKED 1
#define BST_UNCHECKED 0

// Scroll
#define WM_VSCROLL 0x0115
#define SB_LINEUP 0
#define SB_LINEDOWN 1

// Combo box
#define CB_ADDSTRING 0x143
#define CB_FINDSTRINGEXACT 0x158
#define CB_SETCURSEL 0x14E

// Rich edit / text formatting stubs
#define EM_EXGETSEL 0x0434
#define EM_EXSETSEL 0x0437
#define EM_SETCHARFORMAT 0x0444
#define WM_SETREDRAW 0x000B
#define SCF_SELECTION 0x0001

// Placeholders for fullscreen/window style flags – we just define to 0
#define WS_EX_TOPMOST 0
#define WS_EX_TOOLWINDOW 0
#define WS_EX_ACCEPTFILES 0
#define WS_VISIBLE 0
#define WS_POPUP 0

// GDI raster op
#define SRCCOPY 0

// Mouse / key state flags
#define MK_LBUTTON 0x0001
#define MK_RBUTTON 0x0002
#define MK_MBUTTON 0x0010

// SendMessageTimeout flags
#define SMTO_BLOCK 0x0001

// Timing helpers
inline DWORD GetTickCount() {
#ifdef __cplusplus
  using namespace std::chrono;
  return (DWORD) duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (DWORD)(ts.tv_sec * 1000ul + ts.tv_nsec / 1000000ul);
#endif
}

inline void Sleep(DWORD ms) {
#ifdef __cplusplus
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
#else
  struct timespec ts; ts.tv_sec = ms/1000; ts.tv_nsec = (ms%1000)*1000000L; nanosleep(&ts, NULL);
#endif
}

// Message sending stubs (return neutral values). These will be replaced by
// portable UI/event abstractions or removed entirely.
inline long SendMessage(HWND, unsigned int, unsigned long, long) { return 0; }
inline long SendMessage(HWND, unsigned int, unsigned long, unsigned long) { return 0; }
inline BOOL SendMessageTimeout(HWND, unsigned int, unsigned long, long, unsigned int, unsigned int, unsigned long*) { return FALSE; }

inline BOOL IsWindow(HWND) { return TRUE; }
inline short GetAsyncKeyState(int) { return 0; }
inline BOOL GetCursorPos(struct POINT* p) { if(p){p->x=0; p->y=0;} return TRUE; }

// Dialog/UI stub helpers
inline int CheckDlgButton(HWND, int, unsigned int) { return 0; }
inline int IsDlgButtonChecked(HWND, int) { return 0; }
inline HWND GetDlgItem(HWND, int) { return (HWND)0x1; }
inline int SetWindowText(HWND, const char*) { return 0; }
inline int GetWindowText(HWND, char* buf, int) { if(buf) buf[0]='\0'; return 0; }
inline long SendDlgItemMessage(HWND, int, unsigned int, unsigned long, unsigned long) { return 0; }
inline int GetDlgItemText(HWND, int, char* buf, int max) { if(buf && max>0) buf[0]='\0'; return 0; }
// Text message constants
#define WM_GETTEXTLENGTH 0x000E

// Memory allocation stubs for VirtualAlloc/Free; map to malloc/free (no protection flags handled)
#define MEM_COMMIT 0x00001000
#define PAGE_READWRITE 0x04
inline LPVOID VirtualAlloc(LPVOID, size_t sz, unsigned long, unsigned long) { return malloc(sz); }
inline int VirtualFree(LPVOID p, size_t, unsigned long) { free(p); return 1; }
#define MEM_RELEASE 0x8000
// GlobalAlloc/Free simple emulation
#define GPTR 0
inline void* GlobalAlloc(unsigned int, size_t sz) { return malloc(sz); }
inline int GlobalFree(void* p) { free(p); return 0; }

// Synchronization primitives (no-op implementations)
typedef struct { int dummy; } CRITICAL_SECTION;
inline void InitializeCriticalSection(CRITICAL_SECTION*) {}
inline void DeleteCriticalSection(CRITICAL_SECTION*) {}
inline void EnterCriticalSection(CRITICAL_SECTION*) {}
inline void LeaveCriticalSection(CRITICAL_SECTION*) {}

// Unused Win32 structs referenced in signatures (empty placeholders)
struct WNDCLASSA { unsigned int style; void* lpfnWndProc; int cbClsExtra; int cbWndExtra; HINSTANCE hInstance; HICON hIcon; HCURSOR hCursor; HBRUSH hbrBackground; const char* lpszMenuName; const char* lpszClassName; };
typedef WNDCLASSA WNDCLASS;

typedef struct {
  UINT CtlType; UINT CtlID; UINT itemID; UINT itemAction; UINT itemState; HWND hwndItem; HDC hDC; RECT rcItem; ULONG_PTR itemData;
} DRAWITEMSTRUCT;

// Stub registration / creation
inline int RegisterClass(WNDCLASS*) { return 1; }
inline HWND CreateWindowEx(unsigned long, const char*, const char*, unsigned long, int, int, int, int, HWND, void*, HINSTANCE, void*) { return (HWND)0x1; }
inline int DestroyWindow(HWND) { return 0; }

// GDI DC/bitmap stubs
inline HDC CreateCompatibleDC(HDC) { return (HDC)0x1; }
inline int DeleteDC(HDC) { return 0; }
inline int DeleteObject(HGDIOBJ) { return 0; }
inline int BitBlt(HDC, int, int, int, int, HDC, int, int, unsigned long) { return 1; }

// System metrics
#define SM_CXSCREEN 0
#define SM_CYSCREEN 0
inline int GetSystemMetrics(int) { return 800; }

// File enumeration placeholder
struct WIN32_FIND_DATA { char cFileName[MAX_PATH]; }; // minimal
inline void* FindFirstFile(const char*, WIN32_FIND_DATA*) { return (void*)0x1; }
inline int FindNextFile(void*, WIN32_FIND_DATA*) { return 0; }
inline int FindClose(void*) { return 0; }

// Clipboard / etc. left unimplemented (add as needed)

// Fallback min/max if not present
#ifndef max
#define max(a,b) (( (a) > (b) ) ? (a):(b))
#endif
#ifndef min
#define min(a,b) (( (a) < (b) ) ? (a):(b))
#endif

#endif // _WIN32
