/*
 * Process Hacker ToolStatus - 
 *   main program
 * 
 * Copyright (C) 2010 wj32
 * 
 * This file is part of Process Hacker.
 * 
 * Process Hacker is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Process Hacker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Process Hacker.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <phdk.h>
#include "resource.h"
#include "toolstatus.h"
#include <phappresource.h>
#include <windowsx.h>

INT_PTR CALLBACK OptionsDlgProc(
    __in HWND hwndDlg,
    __in UINT uMsg,
    __in WPARAM wParam,
    __in LPARAM lParam
    );

PPH_PLUGIN PluginInstance;
PH_CALLBACK_REGISTRATION PluginLoadCallbackRegistration;
PH_CALLBACK_REGISTRATION PluginShowOptionsCallbackRegistration;
PH_CALLBACK_REGISTRATION MainWindowShowingCallbackRegistration;
PH_CALLBACK_REGISTRATION ProcessesUpdatedCallbackRegistration;

HWND ToolBarHandle;
BOOLEAN EnableToolBar;
HWND StatusBarHandle;
BOOLEAN EnableStatusBar;
HIMAGELIST ToolBarImageList;
ULONG StatusMask;

ULONG IdRangeBase;
ULONG ToolBarIdRangeBase;
ULONG ToolBarIdRangeEnd;

BOOLEAN TargetingWindow = FALSE;
HWND TargetingCurrentWindow = NULL;
BOOLEAN TargetingCurrentWindowDraw = FALSE;
BOOLEAN TargetingWithThread;

ULONG ProcessesUpdatedCount = 0;
ULONG StatusBarMaxWidths[STATUS_COUNT] = { 0 };

LOGICAL DllMain(
    __in HINSTANCE Instance,
    __in ULONG Reason,
    __reserved PVOID Reserved
    )
{
    switch (Reason)
    {
    case DLL_PROCESS_ATTACH:
        {
            PH_PLUGIN_INFORMATION info;

            info.DisplayName = L"Toolbar and Status Bar";
            info.Author = L"wj32";
            info.Description = L"Adds a toolbar and a status bar.";
            info.HasOptions = TRUE;

            PluginInstance = PhRegisterPlugin(L"ProcessHacker.ToolStatus", Instance, &info);

            if (!PluginInstance)
                return FALSE;

            PhRegisterCallback(
                PhGetPluginCallback(PluginInstance, PluginCallbackLoad),
                LoadCallback,
                NULL,
                &PluginLoadCallbackRegistration
                );
            PhRegisterCallback(
                PhGetPluginCallback(PluginInstance, PluginCallbackShowOptions),
                ShowOptionsCallback,
                NULL,
                &PluginShowOptionsCallbackRegistration
                );

            PhRegisterCallback(
                PhGetGeneralCallback(GeneralCallbackMainWindowShowing),
                MainWindowShowingCallback,
                NULL,
                &MainWindowShowingCallbackRegistration
                );
            PhRegisterCallback(
                PhGetGeneralCallback(GeneralCallbackProcessesUpdated),
                ProcessesUpdatedCallback,
                NULL,
                &ProcessesUpdatedCallbackRegistration
                );

            {
                static PH_SETTING_CREATE settings[] =
                {
                    { IntegerSettingType, L"ProcessHacker.ToolStatus.EnableToolBar", L"1" },
                    { IntegerSettingType, L"ProcessHacker.ToolStatus.EnableStatusBar", L"1" },
                    { IntegerSettingType, L"ProcessHacker.ToolStatus.StatusMask", L"d" }
                };

                PhAddSettings(settings, sizeof(settings) / sizeof(PH_SETTING_CREATE));
            }
        }
        break;
    }

    return TRUE;
}

VOID NTAPI LoadCallback(
    __in_opt PVOID Parameter,
    __in_opt PVOID Context
    )
{
    INITCOMMONCONTROLSEX icex;

    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_BAR_CLASSES;

    InitCommonControlsEx(&icex);
}

VOID NTAPI ShowOptionsCallback(
    __in_opt PVOID Parameter,
    __in_opt PVOID Context
    )
{
    DialogBox(
        PluginInstance->DllBase,
        MAKEINTRESOURCE(IDD_OPTIONS),
        (HWND)Parameter,
        OptionsDlgProc
        );
}

VOID NTAPI MainWindowShowingCallback(
    __in_opt PVOID Parameter,
    __in_opt PVOID Context
    )
{
    static TBBUTTON buttons[7];
    ULONG buttonIndex;
    ULONG idIndex;
    ULONG imageIndex;

    IdRangeBase = PhPluginReserveIds(2 + 6);

    ToolBarIdRangeBase = IdRangeBase + 2;
    ToolBarIdRangeEnd = ToolBarIdRangeBase + 6;

    ToolBarHandle = CreateWindowEx(
        0,
        TOOLBARCLASSNAME,
        L"",
        WS_CHILD | CCS_TOP | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS,
        0,
        0,
        0,
        0,
        PhMainWndHandle,
        (HMENU)(IdRangeBase),
        PluginInstance->DllBase,
        NULL
        );

    SendMessage(ToolBarHandle, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    SendMessage(ToolBarHandle, TB_SETEXTENDEDSTYLE, 0,
        TBSTYLE_EX_DOUBLEBUFFER | TBSTYLE_EX_HIDECLIPPEDBUTTONS | TBSTYLE_EX_MIXEDBUTTONS);
    SendMessage(ToolBarHandle, TB_SETMAXTEXTROWS, 0, 0); // hide the text

    ToolBarImageList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 0, 0);
    ImageList_SetImageCount(ToolBarImageList, sizeof(buttons) / sizeof(TBBUTTON));
    PhSetImageListBitmap(ToolBarImageList, 0, PluginInstance->DllBase, MAKEINTRESOURCE(IDB_ARROW_REFRESH));
    PhSetImageListBitmap(ToolBarImageList, 1, PluginInstance->DllBase, MAKEINTRESOURCE(IDB_COG_EDIT));
    PhSetImageListBitmap(ToolBarImageList, 2, PluginInstance->DllBase, MAKEINTRESOURCE(IDB_FIND));
    PhSetImageListBitmap(ToolBarImageList, 3, PluginInstance->DllBase, MAKEINTRESOURCE(IDB_CHART_LINE));
    PhSetImageListBitmap(ToolBarImageList, 4, PluginInstance->DllBase, MAKEINTRESOURCE(IDB_APPLICATION));
    PhSetImageListBitmap(ToolBarImageList, 5, PluginInstance->DllBase, MAKEINTRESOURCE(IDB_APPLICATION_GO));

    SendMessage(ToolBarHandle, TB_SETIMAGELIST, 0, (LPARAM)ToolBarImageList);

    buttonIndex = 0;
    idIndex = 0;
    imageIndex = 0;

#define DEFINE_BUTTON(Text) \
    buttons[buttonIndex].iBitmap = imageIndex++; \
    buttons[buttonIndex].idCommand = ToolBarIdRangeBase + (idIndex++); \
    buttons[buttonIndex].fsState = TBSTATE_ENABLED; \
    buttons[buttonIndex].fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE; \
    buttons[buttonIndex].iString = (INT_PTR)(Text); \
    buttonIndex++

#define DEFINE_SEPARATOR() \
    buttons[buttonIndex].iBitmap = 0; \
    buttons[buttonIndex].idCommand = 0; \
    buttons[buttonIndex].fsState = 0; \
    buttons[buttonIndex].fsStyle = TBSTYLE_SEP; \
    buttons[buttonIndex].iString = 0; \
    buttonIndex++

    DEFINE_BUTTON(L"Refresh");
    DEFINE_BUTTON(L"Options");
    DEFINE_BUTTON(L"Find Handles or DLLs");
    DEFINE_BUTTON(L"System Information");
    DEFINE_SEPARATOR();
    DEFINE_BUTTON(L"Find Window");
    DEFINE_BUTTON(L"Find Window and Thread");

    SendMessage(ToolBarHandle, TB_ADDBUTTONS, sizeof(buttons) / sizeof(TBBUTTON), (LPARAM)buttons);
    SendMessage(ToolBarHandle, WM_SIZE, 0, 0);

    StatusBarHandle = CreateWindowEx(
        0,
        STATUSCLASSNAME,
        L"",
        WS_CHILD | CCS_BOTTOM | SBARS_SIZEGRIP | SBARS_TOOLTIPS,
        0,
        0,
        0,
        0,
        PhMainWndHandle,
        (HMENU)(IdRangeBase + 1),
        PluginInstance->DllBase,
        NULL
        );

    if (EnableToolBar = !!PhGetIntegerSetting(L"ProcessHacker.ToolStatus.EnableToolBar"))
        ShowWindow(ToolBarHandle, SW_SHOW);
    if (EnableStatusBar = !!PhGetIntegerSetting(L"ProcessHacker.ToolStatus.EnableStatusBar"))
        ShowWindow(StatusBarHandle, SW_SHOW);

    StatusMask = PhGetIntegerSetting(L"ProcessHacker.ToolStatus.StatusMask");

    SetWindowSubclass(PhMainWndHandle, MainWndSubclassProc, 0, 0);
}

VOID NTAPI ProcessesUpdatedCallback(
    __in_opt PVOID Parameter,
    __in_opt PVOID Context
    )
{
    ProcessesUpdatedCount++;
    UpdateStatusBar();
}

VOID DrawWindowBorderForTargeting(
    __in HWND hWnd
    )
{
    RECT rect;
    HDC hdc;

    GetWindowRect(hWnd, &rect);
    hdc = GetWindowDC(hWnd);

    if (hdc)
    {
        ULONG penWidth = GetSystemMetrics(SM_CXBORDER) * 3;
        INT oldDc;
        HPEN pen;
        HBRUSH brush;

        oldDc = SaveDC(hdc);

        // Get an inversion effect.
        SetROP2(hdc, R2_NOT);

        pen = CreatePen(PS_INSIDEFRAME, penWidth, RGB(0x00, 0x00, 0x00));
        SelectObject(hdc, pen);

        brush = GetStockObject(NULL_BRUSH);
        SelectObject(hdc, brush);

        // Draw the rectangle.
        Rectangle(hdc, 0, 0, rect.right - rect.left, rect.bottom - rect.top);

        // Cleanup.
        DeleteObject(pen);

        RestoreDC(hdc, oldDc);
        ReleaseDC(hWnd, hdc);
    }
}

LRESULT CALLBACK MainWndSubclassProc(
    __in HWND hWnd,
    __in UINT uMsg,
    __in WPARAM wParam,
    __in LPARAM lParam,
    __in UINT_PTR uIdSubclass,
    __in DWORD_PTR dwRefData
    )
{
    switch (uMsg)
    {
    case WM_COMMAND:
        {
            ULONG id = (ULONG)(USHORT)LOWORD(wParam);
            ULONG toolbarId;

            if (id >= ToolBarIdRangeBase && id < ToolBarIdRangeEnd)
            {
                toolbarId = id - ToolBarIdRangeBase;

                switch (toolbarId)
                {
                case TIDC_REFRESH:
                    SendMessage(hWnd, WM_COMMAND, PHAPP_ID_VIEW_REFRESH, 0);
                    break;
                case TIDC_OPTIONS:
                    SendMessage(hWnd, WM_COMMAND, PHAPP_ID_HACKER_OPTIONS, 0);
                    break;
                case TIDC_FINDOBJ:
                    SendMessage(hWnd, WM_COMMAND, PHAPP_ID_HACKER_FINDHANDLESORDLLS, 0);
                    break;
                case TIDC_SYSINFO:
                    SendMessage(hWnd, WM_COMMAND, PHAPP_ID_VIEW_SYSTEMINFORMATION, 0);
                    break;
                }

                goto DefaultWndProc;
            }
        }
        break;
    case WM_NOTIFY:
        {
            LPNMHDR hdr = (LPNMHDR)lParam;

            if (hdr->hwndFrom == ToolBarHandle)
            {
                switch (hdr->code)
                {
                case TBN_BEGINDRAG:
                    {
                        LPNMTOOLBAR toolbar = (LPNMTOOLBAR)hdr;
                        ULONG id;

                        id = (ULONG)toolbar->iItem - ToolBarIdRangeBase;

                        if (id == TIDC_FINDWINDOW || id == TIDC_FINDWINDOWTHREAD)
                        {
                            // Direct all mouse events to this window.
                            SetCapture(hWnd);
                            // Send the window to the bottom.
                            SetWindowPos(hWnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);

                            TargetingWindow = TRUE;
                            TargetingCurrentWindow = NULL;
                            TargetingCurrentWindowDraw = FALSE;
                            TargetingWithThread = id == TIDC_FINDWINDOWTHREAD;

                            SendMessage(hWnd, WM_MOUSEMOVE, 0, 0);
                        }
                    }
                    break;
                }

                goto DefaultWndProc;
            }
            else if (hdr->hwndFrom == StatusBarHandle)
            {
                switch (hdr->code)
                {
                case NM_RCLICK:
                    {
                        POINT cursorPos;

                        GetCursorPos(&cursorPos);
                        ShowStatusMenu(&cursorPos);
                    }
                    break;
                }

                goto DefaultWndProc;
            }
        }
        break;
    case WM_MOUSEMOVE:
        {
            if (TargetingWindow)
            {
                POINT cursorPos;
                HWND windowOverMouse;
                ULONG processId;
                ULONG threadId;

                GetCursorPos(&cursorPos);
                windowOverMouse = WindowFromPoint(cursorPos);

                if (TargetingCurrentWindow != windowOverMouse)
                {
                    if (TargetingCurrentWindow && TargetingCurrentWindowDraw)
                    {
                        // Invert the old border (to remove it).
                        DrawWindowBorderForTargeting(TargetingCurrentWindow);
                    }

                    if (windowOverMouse)
                    {
                        threadId = GetWindowThreadProcessId(windowOverMouse, &processId);

                        // Draw a rectangle over the current window (but not if it's one of our own).
                        if (UlongToHandle(processId) != NtCurrentProcessId())
                        {
                            DrawWindowBorderForTargeting(windowOverMouse);
                            TargetingCurrentWindowDraw = TRUE;
                        }
                        else
                        {
                            TargetingCurrentWindowDraw = FALSE;
                        }
                    }

                    TargetingCurrentWindow = windowOverMouse;
                }

                goto DefaultWndProc;
            }
        }
        break;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
        {
            if (TargetingWindow)
            {
                ULONG processId;
                ULONG threadId;

                BringWindowToTop(hWnd);
                TargetingWindow = FALSE;
                ReleaseCapture();

                if (TargetingCurrentWindow)
                {
                    if (TargetingCurrentWindowDraw)
                    {
                        // Remove the border on the window we found.
                        DrawWindowBorderForTargeting(TargetingCurrentWindow);
                    }

                    threadId = GetWindowThreadProcessId(TargetingCurrentWindow, &processId);

                    if (threadId && processId && UlongToHandle(processId) != NtCurrentProcessId())
                    {
                        PPH_PROCESS_NODE processNode;

                        processNode = PhFindProcessNode(UlongToHandle(processId));

                        if (processNode)
                        {
                            ProcessHacker_SelectTabPage(hWnd, 0);
                            ProcessHacker_SelectProcessNode(hWnd, processNode);
                        }

                        if (TargetingWithThread)
                        {
                            PPH_PROCESS_PROPCONTEXT propContext;
                            PPH_PROCESS_ITEM processItem;

                            if (processItem = PhReferenceProcessItem(UlongToHandle(processId)))
                            {
                                if (propContext = PhCreateProcessPropContext(hWnd, processItem))
                                {
                                    PhSetSelectThreadIdProcessPropContext(propContext, UlongToHandle(threadId));
                                    PhShowProcessProperties(propContext);
                                    PhDereferenceObject(propContext);
                                }

                                PhDereferenceObject(processItem);
                            }
                        }
                    }
                }

                goto DefaultWndProc;
            }
        }
        break;
    case WM_SIZE:
        {
            RECT padding;
            RECT rect;

            ProcessHacker_GetLayoutPadding(hWnd, &padding);

            if (EnableToolBar)
            {
                // Get the toolbar to resize itself.
                SendMessage(ToolBarHandle, WM_SIZE, 0, 0);
                GetClientRect(ToolBarHandle, &rect);
                padding.top = rect.bottom + 2;
            }
            else
            {
                padding.top = 0;
            }

            if (EnableStatusBar)
            {
                SendMessage(StatusBarHandle, WM_SIZE, 0, 0);
                GetClientRect(StatusBarHandle, &rect);
                padding.bottom = rect.bottom;
            }
            else
            {
                padding.bottom = 0;
            }

            ProcessHacker_SetLayoutPadding(hWnd, &padding);
        }
        break;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
DefaultWndProc:
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

VOID UpdateStatusBar()
{
    static ULONG lastTickCount = 0;

    PPH_STRING text[STATUS_COUNT];
    ULONG widths[STATUS_COUNT];
    ULONG i;
    ULONG index;
    ULONG count;
    HDC hdc;
    PH_PLUGIN_SYSTEM_STATISTICS statistics;
    BOOLEAN resetMaxWidths = FALSE;

    if (ProcessesUpdatedCount < 2)
        return;

    if (!(StatusMask & (STATUS_MAXIMUM - 1)))
    {
        // The status bar doesn't cope well with 0 parts.
        widths[0] = -1;
        SendMessage(StatusBarHandle, SB_SETPARTS, 1, (LPARAM)widths);
        SendMessage(StatusBarHandle, SB_SETTEXT, 0, (LPARAM)L"");
        return;
    }

    PhPluginGetSystemStatistics(&statistics);

    hdc = GetDC(StatusBarHandle);
    SelectObject(hdc, (HFONT)SendMessage(StatusBarHandle, WM_GETFONT, 0, 0));

    // Reset max. widths for Max. CPU Process and Max. I/O Process parts once in a while.
    {
        ULONG tickCount;

        tickCount = GetTickCount();

        if (tickCount - lastTickCount >= 10000)
        {
            resetMaxWidths = TRUE;
            lastTickCount = tickCount;
        }
    }

    count = 0;
    index = 0;

    for (i = STATUS_MINIMUM; i != STATUS_MAXIMUM; i <<= 1)
    {
        if (StatusMask & i)
        {
            SIZE size;
            PPH_PROCESS_ITEM processItem;
            ULONG width;

            switch (i)
            {
            case STATUS_CPUUSAGE:
                text[count] = PhFormatString(L"CPU Usage: %.2f%%", (statistics.CpuKernelUsage + statistics.CpuUserUsage) * 100);
                break;
            case STATUS_COMMIT:
                text[count] = PhFormatString(L"Commit Charge: %.2f%%",
                    (FLOAT)statistics.CommitPages * 100 / statistics.Performance->CommitLimit);
                break;
            case STATUS_PHYSICAL:
                text[count] = PhFormatString(L"Physical Memory: %.2f%%",
                    (FLOAT)statistics.PhysicalPages * 100 / PhSystemBasicInformation.NumberOfPhysicalPages);
                break;
            case STATUS_NUMBEROFPROCESSES:
                text[count] = PhConcatStrings2(L"Processes: ",
                    PhaFormatUInt64(statistics.NumberOfProcesses, TRUE)->Buffer);
                break;
            case STATUS_NUMBEROFTHREADS:
                text[count] = PhConcatStrings2(L"Threads: ",
                    PhaFormatUInt64(statistics.NumberOfThreads, TRUE)->Buffer);
                break;
            case STATUS_NUMBEROFHANDLES:
                text[count] = PhConcatStrings2(L"Handles: ",
                    PhaFormatUInt64(statistics.NumberOfHandles, TRUE)->Buffer);
                break;
            case STATUS_IOREADOTHER:
                text[count] = PhConcatStrings2(L"I/O R+O: ", PhaFormatSize(
                    statistics.IoReadDelta.Delta + statistics.IoOtherDelta.Delta, -1)->Buffer);
                break;
            case STATUS_IOWRITE:
                text[count] = PhConcatStrings2(L"I/O W: ", PhaFormatSize(
                    statistics.IoWriteDelta.Delta, -1)->Buffer);
                break;
            case STATUS_MAXCPUPROCESS:
                if (statistics.MaxCpuProcessId && (processItem = PhReferenceProcessItem(statistics.MaxCpuProcessId)))
                {
                    text[count] = PhFormatString(
                        L"%s (%u): %.2f%%",
                        processItem->ProcessName->Buffer,
                        (ULONG)processItem->ProcessId,
                        processItem->CpuUsage * 100
                        );
                    PhDereferenceObject(processItem);
                }
                else
                {
                    text[count] = PhCreateString(L"-");
                }

                if (resetMaxWidths)
                    StatusBarMaxWidths[index] = 0;

                break;
            case STATUS_MAXIOPROCESS:
                if (statistics.MaxIoProcessId && (processItem = PhReferenceProcessItem(statistics.MaxIoProcessId)))
                {
                    text[count] = PhFormatString(
                        L"%s (%u): %s",
                        processItem->ProcessName->Buffer,
                        (ULONG)processItem->ProcessId,
                        PhaFormatSize(processItem->IoReadDelta.Delta + processItem->IoWriteDelta.Delta + processItem->IoOtherDelta.Delta, -1)->Buffer
                        );
                    PhDereferenceObject(processItem);
                }
                else
                {
                    text[count] = PhCreateString(L"-");
                }

                if (resetMaxWidths)
                    StatusBarMaxWidths[index] = 0;

                break;
            }

            if (!GetTextExtentPoint32(hdc, text[count]->Buffer, text[count]->Length / 2, &size))
                size.cx = 200;

            if (count != 0)
                widths[count] = widths[count - 1];
            else
                widths[count] = 0;

            width = size.cx + 10;

            if (width <= StatusBarMaxWidths[index])
                width = StatusBarMaxWidths[index];
            else
                StatusBarMaxWidths[index] = width;

            widths[count] += width;

            count++;
        }
        else
        {
            StatusBarMaxWidths[index] = 0;
        }

        index++;
    }

    ReleaseDC(StatusBarHandle, hdc);

    SendMessage(StatusBarHandle, SB_SETPARTS, count, (LPARAM)widths);

    for (i = 0; i < count; i++)
    {
        SendMessage(StatusBarHandle, SB_SETTEXT, i, (LPARAM)text[i]->Buffer);
        PhDereferenceObject(text[i]);
    }
}

VOID ShowStatusMenu(
    __in PPOINT Point
    )
{
    HMENU menu;
    HMENU subMenu;
    ULONG i;
    ULONG id;
    ULONG bit;

    menu = LoadMenu(PluginInstance->DllBase, MAKEINTRESOURCE(IDR_STATUS));
    subMenu = GetSubMenu(menu, 0);

    // Check the enabled items.
    for (i = STATUS_MINIMUM; i != STATUS_MAXIMUM; i <<= 1)
    {
        if (StatusMask & i)
        {
            switch (i)
            {
            case STATUS_CPUUSAGE:
                id = ID_STATUS_CPUUSAGE;
                break;
            case STATUS_COMMIT:
                id = ID_STATUS_COMMITCHARGE;
                break;
            case STATUS_PHYSICAL:
                id = ID_STATUS_PHYSICALMEMORY;
                break;
            case STATUS_NUMBEROFPROCESSES:
                id = ID_STATUS_NUMBEROFPROCESSES;
                break;
            case STATUS_NUMBEROFTHREADS:
                id = ID_STATUS_NUMBEROFTHREADS;
                break;
            case STATUS_NUMBEROFHANDLES:
                id = ID_STATUS_NUMBEROFHANDLES;
                break;
            case STATUS_IOREADOTHER:
                id = ID_STATUS_IO_RO;
                break;
            case STATUS_IOWRITE:
                id = ID_STATUS_IO_W;
                break;
            case STATUS_MAXCPUPROCESS:
                id = ID_STATUS_MAX_CPU_PROCESS;
                break;
            case STATUS_MAXIOPROCESS:
                id = ID_STATUS_MAX_IO_PROCESS;
                break;
            }

            CheckMenuItem(subMenu, id, MF_CHECKED);
        }
    }

    id = (ULONG)TrackPopupMenu(
        subMenu,
        TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD,
        Point->x,
        Point->y,
        0,
        PhMainWndHandle,
        NULL
        );
    DestroyMenu(menu);

    switch (id)
    {
    case ID_STATUS_CPUUSAGE:
        bit = STATUS_CPUUSAGE;
        break;
    case ID_STATUS_COMMITCHARGE:
        bit = STATUS_COMMIT;
        break;
    case ID_STATUS_PHYSICALMEMORY:
        bit = STATUS_PHYSICAL;
        break;
    case ID_STATUS_NUMBEROFPROCESSES:
        bit = STATUS_NUMBEROFPROCESSES;
        break;
    case ID_STATUS_NUMBEROFTHREADS:
        bit = STATUS_NUMBEROFTHREADS;
        break;
    case ID_STATUS_NUMBEROFHANDLES:
        bit = STATUS_NUMBEROFHANDLES;
        break;
    case ID_STATUS_IO_RO:
        bit = STATUS_IOREADOTHER;
        break;
    case ID_STATUS_IO_W:
        bit = STATUS_IOWRITE;
        break;
    case ID_STATUS_MAX_CPU_PROCESS:
        bit = STATUS_MAXCPUPROCESS;
        break;
    case ID_STATUS_MAX_IO_PROCESS:
        bit = STATUS_MAXIOPROCESS;
        break;
    default:
        return;
    }

    StatusMask ^= bit;
    PhSetIntegerSetting(L"ProcessHacker.ToolStatus.StatusMask", StatusMask);

    UpdateStatusBar();
}

INT_PTR CALLBACK OptionsDlgProc(
    __in HWND hwndDlg,
    __in UINT uMsg,
    __in WPARAM wParam,
    __in LPARAM lParam
    )
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            Button_SetCheck(GetDlgItem(hwndDlg, IDC_ENABLETOOLBAR), EnableToolBar ? BST_CHECKED : BST_UNCHECKED);
            Button_SetCheck(GetDlgItem(hwndDlg, IDC_ENABLESTATUSBAR), EnableStatusBar ? BST_CHECKED : BST_UNCHECKED);
        }
        break;
    case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
            case IDCANCEL:
                EndDialog(hwndDlg, IDCANCEL);
                break;
            case IDOK:
                {
                    PhSetIntegerSetting(L"ProcessHacker.ToolStatus.EnableToolBar",
                        (EnableToolBar = Button_GetCheck(GetDlgItem(hwndDlg, IDC_ENABLETOOLBAR)) == BST_CHECKED));
                    PhSetIntegerSetting(L"ProcessHacker.ToolStatus.EnableStatusBar",
                        (EnableStatusBar = Button_GetCheck(GetDlgItem(hwndDlg, IDC_ENABLESTATUSBAR)) == BST_CHECKED));

                    ShowWindow(ToolBarHandle, EnableToolBar ? SW_SHOW : SW_HIDE);
                    ShowWindow(StatusBarHandle, EnableStatusBar ? SW_SHOW : SW_HIDE);
                    // Refresh the layout.
                    SendMessage(PhMainWndHandle, WM_SIZE, 0, 0);

                    EndDialog(hwndDlg, IDOK);
                }
                break;
            }
        }
        break;
    }

    return FALSE;
}
