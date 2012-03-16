#ifndef __UPDATER_H__
#define __UPDATER_H__

#define STRICT
#define WIN32_LEAN_AND_MEAN
#define CINTERFACE
#define COBJMACROS

#pragma comment(lib, "Wininet.lib")
#include "phdk.h"
#include "resource.h"
#include "mxml.h"

#include <wininet.h>
#include <windowsx.h>
#include <Netlistmgr.h>

// Always consider the remote version newer
#define TEST_MODE

#define BUFFER_LEN 512
#define UPDATE_MENUITEM 1

#define UPDATE_URL L"processhacker.sourceforge.net"
#define UPDATE_FILE L"/update.php"
#define DOWNLOAD_SERVER L"sourceforge.net"

#define Edit_Visible(hWnd, visible) \
	ShowWindow(hWnd, visible ? SW_SHOW : SW_HIDE)

PPH_PLUGIN PluginInstance;
PH_CALLBACK_REGISTRATION PluginMenuItemCallbackRegistration;
PH_CALLBACK_REGISTRATION MainWindowShowingCallbackRegistration;
PH_CALLBACK_REGISTRATION PluginShowOptionsCallbackRegistration;

typedef struct _UPDATER_XML_DATA
{
    ULONG MinorVersion;
    ULONG MajorVersion;
    WCHAR RelDate[MAX_PATH];
    WCHAR Size[MAX_PATH];
    WCHAR Hash[MAX_PATH];
} UPDATER_XML_DATA, *PUPDATER_XML_DATA;

typedef enum _PH_UPDATER_STATE
{
    Default,
    Downloading,
    Hashing,
    Installing,
	Retry
} PH_UPDATER_STATE;

VOID ShowUpdateDialog(
    __in PVOID Parameter
    );
VOID DisposeStrings(
    VOID
    );
BOOL PhInstalledUsingSetup(
    VOID
    );
BOOL ConnectionAvailable(
    VOID
    );

VOID RunAction(
    __in HWND hwndDlg
    );

BOOL ParseVersionString(
    __in PWSTR String,
    __out PULONG MajorVersion,
    __out PULONG MinorVersion
    );

LONG CompareVersions(
    __in ULONG MajorVersion1,
    __in ULONG MinorVersion1,
    __in ULONG MajorVersion2,
    __in ULONG MinorVersion2
    );

BOOL ReadRequestString(
    __in HINTERNET Handle,
    __out PSTR *Data,
    __out_opt PULONG DataLength
    );

BOOL QueryXmlData(
    __out PUPDATER_XML_DATA XmlData
    );

mxml_type_t QueryXmlDataCallback(
    __in mxml_node_t *node
    );

void FreeXmlData(
    __in PUPDATER_XML_DATA XmlData
    );

void LogEvent(
    __in HWND hwndDlg,
    __in PPH_STRING str
    );

void NTAPI LoadCallback(
    __in_opt PVOID Parameter,
    __in_opt PVOID Context
    );

void NTAPI MenuItemCallback(
    __in_opt PVOID Parameter,
    __in_opt PVOID Context
    );

void NTAPI MainWindowShowingCallback(
    __in_opt PVOID Parameter,
    __in_opt PVOID Context
    );

void NTAPI ShowOptionsCallback(
    __in_opt PVOID Parameter,
    __in_opt PVOID Context
    );

INT_PTR CALLBACK MainWndProc(
    __in HWND hwndDlg,
    __in UINT uMsg,
    __in WPARAM wParam,
    __in LPARAM lParam
    );

INT_PTR CALLBACK OptionsDlgProc(
    __in HWND hwndDlg,
    __in UINT uMsg,
    __in WPARAM wParam,
    __in LPARAM lParam
    );

#endif