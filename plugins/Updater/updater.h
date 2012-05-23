#ifndef __UPDATER_H__
#define __UPDATER_H__

#pragma comment(lib, "Wininet.lib")

#define CINTERFACE
#define COBJMACROS

#include "phdk.h"
#include "phappresource.h"
#include "mxml.h"

#include <time.h>
#include <WinInet.h>
#include <windowsx.h>
#include <Netlistmgr.h>

#include "resource.h"

#define SETTING_AUTO_CHECK L"ProcessHacker.Updater.PromptStart"
#define SETTING_ENABLE_CACHE L"ProcessHacker.Updater.EnableCache"

#define BUFFER_LEN 0x1000
#define UPDATE_MENUITEM 101

#define UPDATE_URL L"processhacker.sourceforge.net"
#define UPDATE_FILE L"/update.php"
#define DOWNLOAD_SERVER L"sourceforge.net"

extern PPH_PLUGIN PluginInstance;

typedef enum _PH_UPDATER_STATE
{
    Download,
    Install
} PH_UPDATER_STATE;

typedef struct _UPDATER_XML_DATA
{
    ULONG MinorVersion;
    ULONG MajorVersion;
    PPH_STRING Version;
    PPH_STRING RelDate;
    PPH_STRING Size;
    PPH_STRING Hash;
} UPDATER_XML_DATA, *PUPDATER_XML_DATA;


VOID ShowUpdateDialog(
    VOID
    );

VOID StartInitialCheck(
    VOID
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

BOOL QueryXmlData(
    __out PUPDATER_XML_DATA XmlData
    );

mxml_type_t QueryXmlDataCallback(
    __in mxml_node_t *node
    );

VOID LogEvent(
    __in_opt HWND hwndDlg,
    __in PPH_STRING str
    );

VOID NTAPI LoadCallback(
    __in_opt PVOID Parameter,
    __in_opt PVOID Context
    );

VOID NTAPI MenuItemCallback(
    __in_opt PVOID Parameter,
    __in_opt PVOID Context
    );

VOID NTAPI MainWindowShowingCallback(
    __in_opt PVOID Parameter,
    __in_opt PVOID Context
    );

VOID NTAPI ShowOptionsCallback(
    __in_opt PVOID Parameter,
    __in_opt PVOID Context
    );

INT_PTR CALLBACK UpdaterWndProc(
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