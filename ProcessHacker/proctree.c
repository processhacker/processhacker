/*
 * Process Hacker - 
 *   process tree list
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

#include <phapp.h>
#include <settings.h>
#include <phplug.h>

VOID PhpRemoveProcessNode(
    __in PPH_PROCESS_NODE ProcessNode
    );

BOOLEAN NTAPI PhpProcessTreeListCallback(
    __in HWND hwnd,
    __in PH_TREELIST_MESSAGE Message,
    __in_opt PVOID Parameter1,
    __in_opt PVOID Parameter2,
    __in_opt PVOID Context
    );

BOOLEAN PhpApplyProcessTreeFiltersToNode(
    __in PPH_PROCESS_NODE Node
    );

static HANDLE ProcessTreeListHandle;
static ULONG ProcessTreeListSortColumn;
static PH_SORT_ORDER ProcessTreeListSortOrder;

static PPH_HASH_ENTRY ProcessNodeHashSet[256] = PH_HASH_SET_INIT; // hashtable of all nodes
static PPH_LIST ProcessNodeList; // list of all nodes, used when sorting is enabled
static PPH_LIST ProcessNodeRootList; // list of root nodes

BOOLEAN PhProcessTreeListStateHighlighting = TRUE;
static PPH_POINTER_LIST ProcessNodeStateList; // list of nodes which need to be processed

static PPH_LIST ProcessTreeFilterList = NULL;

static HICON StockAppIcon;

VOID PhProcessTreeListInitialization()
{
    ProcessNodeList = PhCreateList(40);
    ProcessNodeRootList = PhCreateList(10);
    ProcessNodeStateList = PhCreatePointerList(4);
}

VOID PhInitializeProcessTreeList(
    __in HWND hwnd
    )
{
    ProcessTreeListHandle = hwnd;
    SendMessage(ProcessTreeListHandle, WM_SETFONT, (WPARAM)PhIconTitleFont, FALSE);

    TreeList_SetCallback(hwnd, PhpProcessTreeListCallback);
    TreeList_SetPlusMinus(
        hwnd,
        PH_LOAD_SHARED_IMAGE(MAKEINTRESOURCE(IDB_PLUS), IMAGE_BITMAP),
        PH_LOAD_SHARED_IMAGE(MAKEINTRESOURCE(IDB_MINUS), IMAGE_BITMAP)
        );

    // Default columns
    PhAddTreeListColumn(hwnd, PHTLC_NAME, TRUE, L"Name", 200, PH_ALIGN_LEFT, 0, 0);
    PhAddTreeListColumn(hwnd, PHTLC_PID, TRUE, L"PID", 50, PH_ALIGN_RIGHT, 1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHTLC_CPU, TRUE, L"CPU", 45, PH_ALIGN_RIGHT, 2, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHTLC_IOTOTAL, TRUE, L"I/O Total", 70, PH_ALIGN_RIGHT, 3, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHTLC_PVTMEMORY, TRUE, L"Pvt. Memory", 70, PH_ALIGN_RIGHT, 4, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHTLC_USERNAME, TRUE, L"User Name", 140, PH_ALIGN_LEFT, 5, 0);
    PhAddTreeListColumn(hwnd, PHTLC_DESCRIPTION, TRUE, L"Description", 180, PH_ALIGN_LEFT, 6, 0);

    PhAddTreeListColumn(hwnd, PHTLC_COMPANYNAME, FALSE, L"Company Name", 180, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHTLC_VERSION, FALSE, L"Version", 100, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHTLC_FILENAME, FALSE, L"File Name", 180, PH_ALIGN_LEFT, -1, DT_PATH_ELLIPSIS);
    PhAddTreeListColumn(hwnd, PHTLC_COMMANDLINE, FALSE, L"Command Line", 180, PH_ALIGN_LEFT, -1, 0);

    PhAddTreeListColumn(hwnd, PHTLC_PEAKPVTMEMORY, FALSE, L"Peak Pvt. Memory", 70, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHTLC_WORKINGSET, FALSE, L"Working Set", 70, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHTLC_PEAKWORKINGSET, FALSE, L"Peak Working Set", 70, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHTLC_PRIVATEWS, FALSE, L"Private WS", 70, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHTLC_SHAREDWS, FALSE, L"Shared WS", 70, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHTLC_SHAREABLEWS, FALSE, L"Shareable WS", 70, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHTLC_VIRTUALSIZE, FALSE, L"Virtual Size", 70, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHTLC_PEAKVIRTUALSIZE, FALSE, L"Peak Virtual Size", 70, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHTLC_PAGEFAULTS, FALSE, L"Page Faults", 70, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHTLC_SESSIONID, FALSE, L"Session ID", 45, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHTLC_PRIORITYCLASS, FALSE, L"Priority Class", 100, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHTLC_BASEPRIORITY, FALSE, L"Base Priority", 45, PH_ALIGN_RIGHT, -1, DT_RIGHT);

    PhAddTreeListColumn(hwnd, PHTLC_THREADS, FALSE, L"Threads", 45, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHTLC_HANDLES, FALSE, L"Handles", 45, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHTLC_GDIHANDLES, FALSE, L"GDI Handles", 45, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHTLC_USERHANDLES, FALSE, L"USER Handles", 45, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHTLC_IORO, FALSE, L"I/O R+O", 70, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHTLC_IOW, FALSE, L"I/O W", 70, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHTLC_INTEGRITY, FALSE, L"Integrity", 100, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHTLC_IOPRIORITY, FALSE, L"I/O Priority", 45, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHTLC_PAGEPRIORITY, FALSE, L"Page Priority", 45, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHTLC_STARTTIME, FALSE, L"Start Time", 100, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHTLC_TOTALCPUTIME, FALSE, L"Total CPU Time", 90, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHTLC_KERNELCPUTIME, FALSE, L"Kernel CPU Time", 90, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHTLC_USERCPUTIME, FALSE, L"User CPU Time", 90, PH_ALIGN_RIGHT, -1, DT_RIGHT);
    PhAddTreeListColumn(hwnd, PHTLC_VERIFICATIONSTATUS, FALSE, L"Verification Status", 70, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHTLC_VERIFIEDSIGNER, FALSE, L"Verified Signer", 100, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHTLC_RELATIVESTARTTIME, FALSE, L"Relative Start Time", 180, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHTLC_BITS, FALSE, L"Bits", 50, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHTLC_ELEVATION, FALSE, L"Elevation", 60, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHTLC_WINDOWTITLE, FALSE, L"Window Title", 120, PH_ALIGN_LEFT, -1, 0);
    PhAddTreeListColumn(hwnd, PHTLC_WINDOWSTATUS, FALSE, L"Window Status", 60, PH_ALIGN_LEFT, -1, 0);

    TreeList_SetTriState(hwnd, TRUE);
    TreeList_SetSort(hwnd, 0, NoSortOrder);
}

FORCEINLINE BOOLEAN PhCompareProcessNode(
    __in PPH_PROCESS_NODE Value1,
    __in PPH_PROCESS_NODE Value2
    )
{
    return Value1->ProcessId == Value2->ProcessId;
}

FORCEINLINE ULONG PhHashProcessNode(
    __in PPH_PROCESS_NODE Value
    )
{
    return (ULONG)Value->ProcessId / 4;
}

PPH_PROCESS_NODE PhAddProcessNode(
    __in PPH_PROCESS_ITEM ProcessItem,
    __in ULONG RunId
    )
{
    PPH_PROCESS_NODE processNode;
    PPH_PROCESS_NODE parentNode;
    ULONG i;

    processNode = PhAllocate(sizeof(PH_PROCESS_NODE));
    memset(processNode, 0, sizeof(PH_PROCESS_NODE));
    PhInitializeTreeListNode(&processNode->Node);

    if (PhProcessTreeListStateHighlighting && RunId != 1)
    {
        processNode->TickCount = GetTickCount();
        processNode->Node.UseTempBackColor = TRUE;
        processNode->Node.TempBackColor = PhCsColorNew;
        processNode->StateListHandle = PhAddItemPointerList(ProcessNodeStateList, processNode);
        processNode->State = NewItemState;
    }
    else
    {
        processNode->State = NormalItemState;
    }

    processNode->ProcessId = ProcessItem->ProcessId;
    processNode->ProcessItem = ProcessItem;
    PhReferenceObject(ProcessItem);

    memset(processNode->TextCache, 0, sizeof(PH_STRINGREF) * PHTLC_MAXIMUM);
    processNode->Node.TextCache = processNode->TextCache;
    processNode->Node.TextCacheSize = PHTLC_MAXIMUM;

    processNode->Children = PhCreateList(1);

    // Find this process' parent and add the process to it if we found it.
    if (
        (parentNode = PhFindProcessNode(ProcessItem->ParentProcessId)) &&
        parentNode->ProcessItem->CreateTime.QuadPart <= ProcessItem->CreateTime.QuadPart
        )
    {
        PhAddItemList(parentNode->Children, processNode);
        processNode->Parent = parentNode;
    }
    else
    {
        // No parent, add to root list.
        processNode->Parent = NULL;
        PhAddItemList(ProcessNodeRootList, processNode);
    }

    // Find this process' children and move them to this node.

    for (i = 0; i < ProcessNodeRootList->Count; i++)
    {
        PPH_PROCESS_NODE node = ProcessNodeRootList->Items[i];

        if (
            node != processNode && // for cases where the parent PID = PID (e.g. System Idle Process) 
            node->ProcessItem->ParentProcessId == ProcessItem->ProcessId &&
            ProcessItem->CreateTime.QuadPart <= node->ProcessItem->CreateTime.QuadPart
            )
        {
            node->Parent = processNode;
            PhAddItemList(processNode->Children, node);
        }
    }

    for (i = 0; i < processNode->Children->Count; i++)
    {
        PhRemoveItemList(
            ProcessNodeRootList,
            PhFindItemList(ProcessNodeRootList, processNode->Children->Items[i])
            );
    }

    PhAddEntryHashSet(
        ProcessNodeHashSet,
        PH_HASH_SET_SIZE(ProcessNodeHashSet),
        &processNode->HashEntry,
        PhHashProcessNode(processNode)
        );
    PhAddItemList(ProcessNodeList, processNode);

    if (PhCsCollapseServicesOnStart)
    {
        static PH_STRINGREF servicesBaseName = PH_STRINGREF_INIT(L"\\services.exe");
        static BOOLEAN servicesFound = FALSE;
        static PPH_STRING servicesFileName = NULL;

        if (!servicesFound)
        {
            if (!servicesFileName)
            {
                PPH_STRING systemDirectory;

                systemDirectory = PhGetSystemDirectory();
                servicesFileName = PhConcatStringRef2(&systemDirectory->sr, &servicesBaseName);
                PhDereferenceObject(systemDirectory);
            }

            // If this process is services.exe, collapse the node and free the string.
            if (
                ProcessItem->FileName &&
                PhEqualString(ProcessItem->FileName, servicesFileName, TRUE)
                )
            {
                processNode->Node.Expanded = FALSE;
                PhDereferenceObject(servicesFileName);
                servicesFileName = NULL;
                servicesFound = TRUE;
            }
        }
    }

    if (ProcessTreeFilterList)
        processNode->Node.Visible = PhpApplyProcessTreeFiltersToNode(processNode);

    TreeList_NodesStructured(ProcessTreeListHandle);

    return processNode;
}

PPH_PROCESS_NODE PhFindProcessNode(
   __in HANDLE ProcessId
   )
{
    PH_PROCESS_NODE lookupNode;
    PPH_HASH_ENTRY entry;
    PPH_PROCESS_NODE node;

    lookupNode.ProcessId = ProcessId;
    entry = PhFindEntryHashSet(
        ProcessNodeHashSet,
        PH_HASH_SET_SIZE(ProcessNodeHashSet),
        PhHashProcessNode(&lookupNode)
        );

    for (; entry; entry = entry->Next)
    {
        node = CONTAINING_RECORD(entry, PH_PROCESS_NODE, HashEntry);

        if (PhCompareProcessNode(&lookupNode, node))
            return node;
    }

    return NULL;
}

VOID PhRemoveProcessNode(
    __in PPH_PROCESS_NODE ProcessNode
    )
{
    if (PhProcessTreeListStateHighlighting)
    {
        ProcessNode->TickCount = GetTickCount();
        ProcessNode->Node.UseTempBackColor = TRUE;
        ProcessNode->Node.TempBackColor = PhCsColorRemoved;
        if (ProcessNode->State == NormalItemState)
            ProcessNode->StateListHandle = PhAddItemPointerList(ProcessNodeStateList, ProcessNode);
        ProcessNode->State = RemovingItemState;

        TreeList_UpdateNode(ProcessTreeListHandle, &ProcessNode->Node);
    }
    else
    {
        PhpRemoveProcessNode(ProcessNode);
    }
}

VOID PhpRemoveProcessNode(
    __in PPH_PROCESS_NODE ProcessNode
    )
{
    ULONG index;
    ULONG i;

    if (ProcessNode->Parent)
    {
        // Remove the node from its parent.

        if ((index = PhFindItemList(ProcessNode->Parent->Children, ProcessNode)) != -1)
            PhRemoveItemList(ProcessNode->Parent->Children, index);
    }
    else
    {
        // Remove the node from the root list.

        if ((index = PhFindItemList(ProcessNodeRootList, ProcessNode)) != -1)
            PhRemoveItemList(ProcessNodeRootList, index);
    }

    // Move the node's children to the root list.
    for (i = 0; i < ProcessNode->Children->Count; i++)
    {
        PPH_PROCESS_NODE node = ProcessNode->Children->Items[i];

        node->Parent = NULL;
        PhAddItemList(ProcessNodeRootList, node);
    }

    // Remove from hashtable/list and cleanup.

    PhRemoveEntryHashSet(ProcessNodeHashSet, PH_HASH_SET_SIZE(ProcessNodeHashSet), &ProcessNode->HashEntry);

    if ((index = PhFindItemList(ProcessNodeList, ProcessNode)) != -1)
        PhRemoveItemList(ProcessNodeList, index);

    PhDereferenceObject(ProcessNode->Children);

    if (ProcessNode->WindowText) PhDereferenceObject(ProcessNode->WindowText);

    if (ProcessNode->TooltipText) PhDereferenceObject(ProcessNode->TooltipText);

    if (ProcessNode->IoTotalText) PhDereferenceObject(ProcessNode->IoTotalText);
    if (ProcessNode->PrivateMemoryText) PhDereferenceObject(ProcessNode->PrivateMemoryText);
    if (ProcessNode->PeakPrivateMemoryText) PhDereferenceObject(ProcessNode->PeakPrivateMemoryText);
    if (ProcessNode->WorkingSetText) PhDereferenceObject(ProcessNode->WorkingSetText);
    if (ProcessNode->PeakWorkingSetText) PhDereferenceObject(ProcessNode->PeakWorkingSetText);
    if (ProcessNode->PrivateWsText) PhDereferenceObject(ProcessNode->PrivateWsText);
    if (ProcessNode->SharedWsText) PhDereferenceObject(ProcessNode->SharedWsText);
    if (ProcessNode->ShareableWsText) PhDereferenceObject(ProcessNode->ShareableWsText);
    if (ProcessNode->VirtualSizeText) PhDereferenceObject(ProcessNode->VirtualSizeText);
    if (ProcessNode->PeakVirtualSizeText) PhDereferenceObject(ProcessNode->PeakVirtualSizeText);
    if (ProcessNode->PageFaultsText) PhDereferenceObject(ProcessNode->PageFaultsText);
    if (ProcessNode->IoRoText) PhDereferenceObject(ProcessNode->IoRoText);
    if (ProcessNode->IoWText) PhDereferenceObject(ProcessNode->IoWText);
    if (ProcessNode->StartTimeText) PhDereferenceObject(ProcessNode->StartTimeText);
    if (ProcessNode->RelativeStartTimeText) PhDereferenceObject(ProcessNode->RelativeStartTimeText);
    if (ProcessNode->WindowTitleText) PhDereferenceObject(ProcessNode->WindowTitleText);

    PhDereferenceObject(ProcessNode->ProcessItem);

    PhFree(ProcessNode);

    TreeList_NodesStructured(ProcessTreeListHandle);
}

VOID PhUpdateProcessNode(
    __in PPH_PROCESS_NODE ProcessNode
    )
{
    memset(ProcessNode->TextCache, 0, sizeof(PH_STRINGREF) * PHTLC_MAXIMUM);

    if (ProcessNode->TooltipText)
    {
        PhDereferenceObject(ProcessNode->TooltipText);
        ProcessNode->TooltipText = NULL;
    }

    PhInvalidateTreeListNode(&ProcessNode->Node, TLIN_COLOR | TLIN_ICON);
}

VOID PhTickProcessNodes()
{
    // Text invalidation
    {
        ULONG i;

        for (i = 0; i < ProcessNodeList->Count; i++)
        {
            PPH_PROCESS_NODE node = ProcessNodeList->Items[i];

            // The name and PID never change, so we don't invalidate that.
            memset(&node->TextCache[2], 0, sizeof(PH_STRINGREF) * (PHTLC_MAXIMUM - 2));
            node->ValidMask = 0;
        }

        if (ProcessTreeListSortOrder != NoSortOrder)
        {
            // Force a rebuild to sort the items.
            TreeList_NodesStructured(ProcessTreeListHandle);
        }

        InvalidateRect(ProcessTreeListHandle, NULL, FALSE);
    }

    // State highlighting
    if (ProcessNodeStateList->Count != 0)
    {
        PPH_PROCESS_NODE node;
        ULONG enumerationKey = 0;
        ULONG tickCount;
        HANDLE stateListHandle;
        BOOLEAN redrawDisabled = FALSE;

        tickCount = GetTickCount();

        while (PhEnumPointerList(ProcessNodeStateList, &enumerationKey, &node))
        {
            if (PhRoundNumber(tickCount - node->TickCount, 100) < PhCsHighlightingDuration)
                continue;

            stateListHandle = node->StateListHandle;

            if (node->State == NewItemState)
            {
                node->State = NormalItemState;
                node->Node.UseTempBackColor = FALSE;
            }
            else if (node->State == RemovingItemState)
            {
                if (!redrawDisabled)
                {
                    TreeList_SetRedraw(ProcessTreeListHandle, FALSE);
                    redrawDisabled = TRUE;
                }

                PhpRemoveProcessNode(node);
            }

            PhRemoveItemPointerList(ProcessNodeStateList, stateListHandle);
        }

        if (redrawDisabled)
            TreeList_SetRedraw(ProcessTreeListHandle, TRUE);
    }
}

HICON PhGetStockAppIcon()
{
    if (!StockAppIcon)
    {
        SHFILEINFO fileInfo = { 0 };

        SHGetFileInfo(
            L".exe",
            FILE_ATTRIBUTE_NORMAL,
            &fileInfo,
            sizeof(SHFILEINFO),
            SHGFI_USEFILEATTRIBUTES | SHGFI_ICON | SHGFI_SMALLICON
            );
        StockAppIcon = fileInfo.hIcon;
    }

    return StockAppIcon;
}

VOID PhpUpdateProcessNodeWsCounters(
    __inout PPH_PROCESS_NODE ProcessNode
    )
{
    if (!(ProcessNode->ValidMask & PHPN_WSCOUNTERS))
    {
        BOOLEAN success = FALSE;
        HANDLE processHandle;

        if (NT_SUCCESS(PhOpenProcess(
            &processHandle,
            PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
            ProcessNode->ProcessItem->ProcessId
            )))
        {
            if (NT_SUCCESS(PhGetProcessWsCounters(
                processHandle,
                &ProcessNode->WsCounters
                )))
                success = TRUE;

            NtClose(processHandle);
        }

        if (!success)
            memset(&ProcessNode->WsCounters, 0, sizeof(PH_PROCESS_WS_COUNTERS));

        ProcessNode->ValidMask |= PHPN_WSCOUNTERS;
    }
}

VOID PhpUpdateProcessNodeGdiUserHandles(
    __inout PPH_PROCESS_NODE ProcessNode
    )
{
    if (!(ProcessNode->ValidMask & PHPN_GDIUSERHANDLES))
    {
        if (ProcessNode->ProcessItem->QueryHandle)
        {
            ProcessNode->GdiHandles = GetGuiResources(ProcessNode->ProcessItem->QueryHandle, GR_GDIOBJECTS);
            ProcessNode->UserHandles = GetGuiResources(ProcessNode->ProcessItem->QueryHandle, GR_USEROBJECTS);
        }
        else
        {
            ProcessNode->GdiHandles = 0;
            ProcessNode->UserHandles = 0;
        }

        ProcessNode->ValidMask |= PHPN_GDIUSERHANDLES;
    }
}

VOID PhpUpdateProcessNodeIoPagePriority(
    __inout PPH_PROCESS_NODE ProcessNode
    )
{
    if (!(ProcessNode->ValidMask & PHPN_IOPAGEPRIORITY))
    {
        if (ProcessNode->ProcessItem->QueryHandle)
        {
            if (!NT_SUCCESS(PhGetProcessIoPriority(ProcessNode->ProcessItem->QueryHandle, &ProcessNode->IoPriority)))
                ProcessNode->IoPriority = 0;
            if (!NT_SUCCESS(PhGetProcessPagePriority(ProcessNode->ProcessItem->QueryHandle, &ProcessNode->PagePriority)))
                ProcessNode->PagePriority = 0;
        }
        else
        {
            ProcessNode->IoPriority = 0;
            ProcessNode->PagePriority = 0;
        }

        ProcessNode->ValidMask |= PHPN_IOPAGEPRIORITY;
    }
}

BOOL CALLBACK PhpEnumProcessNodeWindowsProc(
    __in HWND hwnd,
    __in LPARAM lParam
    )
{
    PPH_PROCESS_NODE processNode = (PPH_PROCESS_NODE)lParam;
    ULONG threadId;
    ULONG processId;

    threadId = GetWindowThreadProcessId(hwnd, &processId);

    if (UlongToHandle(processId) == processNode->ProcessId)
    {
        HWND parentWindow;

        if (
            !IsWindowVisible(hwnd) || // skip invisible windows
            ((parentWindow = GetParent(hwnd)) && IsWindowVisible(parentWindow)) || // skip windows with visible parents
            GetWindowTextLength(hwnd) == 0 // skip windows with no title
            )
            return TRUE;

        processNode->WindowHandle = hwnd;
        return FALSE;
    }

    return TRUE;
}

VOID PhpUpdateProcessNodeWindow(
    __inout PPH_PROCESS_NODE ProcessNode
    )
{
    if (!(ProcessNode->ValidMask & PHPN_WINDOW))
    {
        ProcessNode->WindowHandle = NULL;
        EnumWindows(PhpEnumProcessNodeWindowsProc, (LPARAM)ProcessNode);

        PhSwapReference(&ProcessNode->WindowText, NULL);

        if (ProcessNode->WindowHandle)
        {
            ProcessNode->WindowText = PhGetWindowText(ProcessNode->WindowHandle);
            ProcessNode->WindowHung = !!IsHungAppWindow(ProcessNode->WindowHandle);
        }

        ProcessNode->ValidMask |= PHPN_WINDOW;
    }
}

#define SORT_FUNCTION(Column) PhpProcessTreeListCompare##Column

#define BEGIN_SORT_FUNCTION(Column) static int __cdecl PhpProcessTreeListCompare##Column( \
    __in const void *_elem1, \
    __in const void *_elem2 \
    ) \
{ \
    PPH_PROCESS_NODE node1 = *(PPH_PROCESS_NODE *)_elem1; \
    PPH_PROCESS_NODE node2 = *(PPH_PROCESS_NODE *)_elem2; \
    PPH_PROCESS_ITEM processItem1 = node1->ProcessItem; \
    PPH_PROCESS_ITEM processItem2 = node2->ProcessItem; \
    int sortResult = 0;

#define END_SORT_FUNCTION \
    if (sortResult == 0) \
        sortResult = intptrcmp((LONG_PTR)processItem1->ProcessId, (LONG_PTR)processItem2->ProcessId); \
    \
    return PhModifySort(sortResult, ProcessTreeListSortOrder); \
}

BEGIN_SORT_FUNCTION(Name)
{
    sortResult = PhCompareString(processItem1->ProcessName, processItem2->ProcessName, TRUE);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Pid)
{
    // Use signed int so DPCs and Interrupts are placed above System Idle Process.
    sortResult = intptrcmp((LONG_PTR)processItem1->ProcessId, (LONG_PTR)processItem2->ProcessId);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Cpu)
{
    sortResult = singlecmp(processItem1->CpuUsage, processItem2->CpuUsage);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(IoTotal)
{
    sortResult = uint64cmp(
        processItem1->IoReadDelta.Delta + processItem1->IoWriteDelta.Delta + processItem1->IoOtherDelta.Delta,
        processItem2->IoReadDelta.Delta + processItem2->IoWriteDelta.Delta + processItem2->IoOtherDelta.Delta
        );
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(PvtMemory)
{
    sortResult = uintptrcmp(processItem1->VmCounters.PagefileUsage, processItem2->VmCounters.PagefileUsage);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(UserName)
{
    sortResult = PhCompareStringWithNull(processItem1->UserName, processItem2->UserName, TRUE);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Description)
{
    sortResult = PhCompareStringWithNull(
        processItem1->VersionInfo.FileDescription,
        processItem2->VersionInfo.FileDescription,
        TRUE
        );
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(CompanyName)
{
    sortResult = PhCompareStringWithNull(
        processItem1->VersionInfo.CompanyName,
        processItem2->VersionInfo.CompanyName,
        TRUE
        );
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Version)
{
    sortResult = PhCompareStringWithNull(
        processItem1->VersionInfo.FileVersion,
        processItem2->VersionInfo.FileVersion,
        TRUE
        );
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(FileName)
{
    sortResult = PhCompareStringWithNull(
        processItem1->FileName,
        processItem2->FileName,
        TRUE
        );
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(CommandLine)
{
    sortResult = PhCompareStringWithNull(
        processItem1->CommandLine,
        processItem2->CommandLine,
        TRUE
        );
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(PeakPvtMemory)
{
    sortResult = uintptrcmp(processItem1->VmCounters.PeakPagefileUsage, processItem2->VmCounters.PeakPagefileUsage);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(WorkingSet)
{
    sortResult = uintptrcmp(processItem1->VmCounters.WorkingSetSize, processItem2->VmCounters.WorkingSetSize);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(PeakWorkingSet)
{
    sortResult = uintptrcmp(processItem1->VmCounters.PeakWorkingSetSize, processItem2->VmCounters.PeakWorkingSetSize);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(PrivateWs)
{
    PhpUpdateProcessNodeWsCounters(node1);
    PhpUpdateProcessNodeWsCounters(node2);
    sortResult = uintcmp(node1->WsCounters.NumberOfPrivatePages, node2->WsCounters.NumberOfPrivatePages);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(SharedWs)
{
    PhpUpdateProcessNodeWsCounters(node1);
    PhpUpdateProcessNodeWsCounters(node2);
    sortResult = uintcmp(node1->WsCounters.NumberOfSharedPages, node2->WsCounters.NumberOfSharedPages);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(ShareableWs)
{
    PhpUpdateProcessNodeWsCounters(node1);
    PhpUpdateProcessNodeWsCounters(node2);
    sortResult = uintcmp(node1->WsCounters.NumberOfShareablePages, node2->WsCounters.NumberOfShareablePages);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(VirtualSize)
{
    sortResult = uintptrcmp(processItem1->VmCounters.VirtualSize, processItem2->VmCounters.VirtualSize);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(PeakVirtualSize)
{
    sortResult = uintptrcmp(processItem1->VmCounters.PeakVirtualSize, processItem2->VmCounters.PeakVirtualSize);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(PageFaults)
{
    sortResult = uintcmp(processItem1->VmCounters.PageFaultCount, processItem2->VmCounters.PageFaultCount);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(SessionId)
{
    sortResult = uintcmp(processItem1->SessionId, processItem2->SessionId);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(PriorityClass)
{
    sortResult = intcmp(processItem1->BasePriority, processItem2->BasePriority);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(BasePriority)
{
    sortResult = intcmp(processItem1->BasePriority, processItem2->BasePriority);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Threads)
{
    sortResult = uintcmp(processItem1->NumberOfThreads, processItem2->NumberOfThreads);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Handles)
{
    sortResult = uintcmp(processItem1->NumberOfHandles, processItem2->NumberOfHandles);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(GdiHandles)
{
    sortResult = uintcmp(node1->GdiHandles, node2->GdiHandles);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(UserHandles)
{
    sortResult = uintcmp(node1->UserHandles, node2->UserHandles);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(IoRo)
{
    sortResult = uint64cmp(
        processItem1->IoReadDelta.Delta + processItem1->IoOtherDelta.Delta,
        processItem2->IoReadDelta.Delta + processItem2->IoOtherDelta.Delta
        );
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(IoW)
{
    sortResult = uint64cmp(
        processItem1->IoWriteDelta.Delta,
        processItem2->IoWriteDelta.Delta
        );
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Integrity)
{
    sortResult = uintcmp(processItem1->IntegrityLevel, processItem2->IntegrityLevel);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(IoPriority)
{
    sortResult = uintcmp(node1->IoPriority, node2->IoPriority);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(PagePriority)
{
    sortResult = uintcmp(node1->PagePriority, node2->PagePriority);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(StartTime)
{
    sortResult = int64cmp(processItem1->CreateTime.QuadPart, processItem2->CreateTime.QuadPart);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(TotalCpuTime)
{
    sortResult = uint64cmp(
        processItem1->KernelTime.QuadPart + processItem1->UserTime.QuadPart,
        processItem2->KernelTime.QuadPart + processItem2->UserTime.QuadPart
        );
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(KernelCpuTime)
{
    sortResult = uint64cmp(
        processItem1->KernelTime.QuadPart,
        processItem2->KernelTime.QuadPart
        );
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(UserCpuTime)
{
    sortResult = uint64cmp(
        processItem1->UserTime.QuadPart,
        processItem2->UserTime.QuadPart
        );
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(VerificationStatus)
{
    sortResult = intcmp(processItem1->VerifyResult == VrTrusted, processItem2->VerifyResult == VrTrusted);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(VerifiedSigner)
{
    sortResult = PhCompareStringWithNull(
        processItem1->VerifySignerName,
        processItem2->VerifySignerName,
        TRUE
        );
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Reserved1)
{
    sortResult = 0;
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(RelativeStartTime)
{
    sortResult = -int64cmp(processItem1->CreateTime.QuadPart, processItem2->CreateTime.QuadPart);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Bits)
{
    sortResult = intcmp(processItem1->IsWow64, processItem2->IsWow64);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Elevation)
{
    sortResult = intcmp(processItem1->ElevationType, processItem2->ElevationType);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(WindowTitle)
{
    PhpUpdateProcessNodeWindow(node1);
    PhpUpdateProcessNodeWindow(node2);
    sortResult = PhCompareStringWithNull(node1->WindowText, node2->WindowText, TRUE);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(WindowStatus)
{
    PhpUpdateProcessNodeWindow(node1);
    PhpUpdateProcessNodeWindow(node2);
    sortResult = intcmp(node1->WindowHung, node2->WindowHung);
}
END_SORT_FUNCTION

BOOLEAN NTAPI PhpProcessTreeListCallback(
    __in HWND hwnd,
    __in PH_TREELIST_MESSAGE Message,
    __in_opt PVOID Parameter1,
    __in_opt PVOID Parameter2,
    __in_opt PVOID Context
    )
{
    PPH_PROCESS_NODE node;

    switch (Message)
    {
    case TreeListGetChildren:
        {
            PPH_TREELIST_GET_CHILDREN getChildren = Parameter1;

            node = (PPH_PROCESS_NODE)getChildren->Node;

            if (ProcessTreeListSortOrder == NoSortOrder)
            {
                if (!node)
                {
                    getChildren->Children = (PPH_TREELIST_NODE *)ProcessNodeRootList->Items;
                    getChildren->NumberOfChildren = ProcessNodeRootList->Count;
                }
                else
                {
                    getChildren->Children = (PPH_TREELIST_NODE *)node->Children->Items;
                    getChildren->NumberOfChildren = node->Children->Count;
                }
            }
            else
            {
                if (!node)
                {
                    static PVOID sortFunctions[] =
                    {
                        SORT_FUNCTION(Name),
                        SORT_FUNCTION(Pid),
                        SORT_FUNCTION(Cpu),
                        SORT_FUNCTION(IoTotal),
                        SORT_FUNCTION(PvtMemory),
                        SORT_FUNCTION(UserName),
                        SORT_FUNCTION(Description),
                        SORT_FUNCTION(CompanyName),
                        SORT_FUNCTION(Version),
                        SORT_FUNCTION(FileName),
                        SORT_FUNCTION(CommandLine),
                        SORT_FUNCTION(PeakPvtMemory),
                        SORT_FUNCTION(WorkingSet),
                        SORT_FUNCTION(PeakWorkingSet),
                        SORT_FUNCTION(PrivateWs),
                        SORT_FUNCTION(SharedWs),
                        SORT_FUNCTION(ShareableWs),
                        SORT_FUNCTION(VirtualSize),
                        SORT_FUNCTION(PeakVirtualSize),
                        SORT_FUNCTION(PageFaults),
                        SORT_FUNCTION(SessionId),
                        SORT_FUNCTION(PriorityClass),
                        SORT_FUNCTION(BasePriority),
                        SORT_FUNCTION(Threads),
                        SORT_FUNCTION(Handles),
                        SORT_FUNCTION(GdiHandles),
                        SORT_FUNCTION(UserHandles),
                        SORT_FUNCTION(IoRo),
                        SORT_FUNCTION(IoW),
                        SORT_FUNCTION(Integrity),
                        SORT_FUNCTION(IoPriority),
                        SORT_FUNCTION(PagePriority),
                        SORT_FUNCTION(StartTime),
                        SORT_FUNCTION(TotalCpuTime),
                        SORT_FUNCTION(KernelCpuTime),
                        SORT_FUNCTION(UserCpuTime),
                        SORT_FUNCTION(VerificationStatus),
                        SORT_FUNCTION(VerifiedSigner),
                        SORT_FUNCTION(Reserved1),
                        SORT_FUNCTION(RelativeStartTime),
                        SORT_FUNCTION(Bits),
                        SORT_FUNCTION(Elevation),
                        SORT_FUNCTION(WindowTitle),
                        SORT_FUNCTION(WindowStatus)
                    };
                    int (__cdecl *sortFunction)(const void *, const void *);

                    if (ProcessTreeListSortColumn < PHTLC_MAXIMUM)
                        sortFunction = sortFunctions[ProcessTreeListSortColumn];

                    if (sortFunction)
                    {
                        // Don't use PhSortList to avoid overhead.
                        qsort(ProcessNodeList->Items, ProcessNodeList->Count, sizeof(PVOID), sortFunction);
                    }

                    getChildren->Children = (PPH_TREELIST_NODE *)ProcessNodeList->Items;
                    getChildren->NumberOfChildren = ProcessNodeList->Count;
                }
            }
        }
        return TRUE;
    case TreeListIsLeaf:
        {
            PPH_TREELIST_IS_LEAF isLeaf = Parameter1;

            node = (PPH_PROCESS_NODE)isLeaf->Node;

            if (ProcessTreeListSortOrder == NoSortOrder)
                isLeaf->IsLeaf = node->Children->Count == 0;
            else
                isLeaf->IsLeaf = TRUE;
        }
        return TRUE;
    case TreeListGetNodeText:
        {
            static PH_STRINGREF perSecondString = PH_STRINGREF_INIT(L"/s");

            PPH_TREELIST_GET_NODE_TEXT getNodeText = Parameter1;
            PPH_PROCESS_ITEM processItem;

            node = (PPH_PROCESS_NODE)getNodeText->Node;
            processItem = node->ProcessItem;

            switch (getNodeText->Id)
            {
            case PHTLC_NAME:
                getNodeText->Text = processItem->ProcessName->sr;
                break;
            case PHTLC_PID:
                PhInitializeStringRef(&getNodeText->Text, processItem->ProcessIdString);
                break;
            case PHTLC_CPU:
                {
                    FLOAT cpuUsage;

                    cpuUsage = processItem->CpuUsage * 100;

                    if (cpuUsage >= 0.01)
                    {
                        _snwprintf(node->CpuUsageText, PH_INT32_STR_LEN, L"%.2f", cpuUsage);
                        PhInitializeStringRef(&getNodeText->Text, node->CpuUsageText);
                    }
                    else
                    {
                        PhInitializeEmptyStringRef(&getNodeText->Text);
                    }
                }
                break;
            case PHTLC_IOTOTAL:
                {
                    ULONG64 number;

                    if (processItem->IoReadDelta.Delta != processItem->IoReadDelta.Value) // delta is wrong on first run of process provider
                    {
                        number = processItem->IoReadDelta.Delta + processItem->IoWriteDelta.Delta + processItem->IoOtherDelta.Delta;
                        number *= 1000;
                        number /= PhCsUpdateInterval;
                    }
                    else
                    {
                        number = 0;
                    }

                    if (number != 0)
                    {
                        PH_FORMAT format[2];

                        format[0].Type = SizeFormatType;
                        format[0].u.Size = number;
                        format[1].Type = StringFormatType;
                        format[1].u.String = perSecondString;
                        PhSwapReference2(&node->IoTotalText, PhFormat(format, 2, 0));
                        getNodeText->Text = node->IoTotalText->sr;
                    }
                    else
                    {
                        PhInitializeEmptyStringRef(&getNodeText->Text);
                    }
                }
                break;
            case PHTLC_PVTMEMORY:
                PhSwapReference2(&node->PrivateMemoryText, PhFormatSize(processItem->VmCounters.PagefileUsage, -1));
                getNodeText->Text = node->PrivateMemoryText->sr;
                break;
            case PHTLC_USERNAME:
                getNodeText->Text = PhGetStringRefOrEmpty(processItem->UserName);
                break;
            case PHTLC_DESCRIPTION:
                getNodeText->Text = PhGetStringRefOrEmpty(processItem->VersionInfo.FileDescription);
                break;
            case PHTLC_COMPANYNAME:
                getNodeText->Text = PhGetStringRefOrEmpty(processItem->VersionInfo.CompanyName);
                break;
            case PHTLC_VERSION:
                getNodeText->Text = PhGetStringRefOrEmpty(processItem->VersionInfo.FileVersion);
                break;
            case PHTLC_FILENAME:
                getNodeText->Text = PhGetStringRefOrEmpty(processItem->FileName);
                break;
            case PHTLC_COMMANDLINE:
                getNodeText->Text = PhGetStringRefOrEmpty(processItem->CommandLine);
                break;
            case PHTLC_PEAKPVTMEMORY:
                PhSwapReference2(&node->PeakPrivateMemoryText, PhFormatSize(processItem->VmCounters.PeakPagefileUsage, -1));
                getNodeText->Text = node->PeakPrivateMemoryText->sr;
                break;
            case PHTLC_WORKINGSET:
                PhSwapReference2(&node->WorkingSetText, PhFormatSize(processItem->VmCounters.WorkingSetSize, -1));
                getNodeText->Text = node->WorkingSetText->sr;
                break;
            case PHTLC_PEAKWORKINGSET:
                PhSwapReference2(&node->PeakWorkingSetText, PhFormatSize(processItem->VmCounters.PeakWorkingSetSize, -1));
                getNodeText->Text = node->PeakWorkingSetText->sr;
                break;
            case PHTLC_PRIVATEWS:
                PhpUpdateProcessNodeWsCounters(node);
                PhSwapReference2(&node->PrivateWsText, PhFormatSize(UInt32x32To64(node->WsCounters.NumberOfPrivatePages, PAGE_SIZE), -1));
                getNodeText->Text = node->PrivateWsText->sr;
                break;
            case PHTLC_SHAREDWS:
                PhpUpdateProcessNodeWsCounters(node);
                PhSwapReference2(&node->SharedWsText, PhFormatSize(UInt32x32To64(node->WsCounters.NumberOfSharedPages, PAGE_SIZE), -1));
                getNodeText->Text = node->SharedWsText->sr;
                break;
            case PHTLC_SHAREABLEWS:
                PhpUpdateProcessNodeWsCounters(node);
                PhSwapReference2(&node->ShareableWsText, PhFormatSize(UInt32x32To64(node->WsCounters.NumberOfShareablePages, PAGE_SIZE), -1));
                getNodeText->Text = node->ShareableWsText->sr;
                break;
            case PHTLC_VIRTUALSIZE:
                PhSwapReference2(&node->VirtualSizeText, PhFormatSize(processItem->VmCounters.VirtualSize, -1));
                getNodeText->Text = node->VirtualSizeText->sr;
                break;
            case PHTLC_PEAKVIRTUALSIZE:
                PhSwapReference2(&node->PeakVirtualSizeText, PhFormatSize(processItem->VmCounters.PeakVirtualSize, -1));
                getNodeText->Text = node->PeakVirtualSizeText->sr;
                break;
            case PHTLC_PAGEFAULTS:
                PhSwapReference2(&node->PageFaultsText, PhFormatUInt64(processItem->VmCounters.PageFaultCount, TRUE));
                getNodeText->Text = node->PageFaultsText->sr;
                break;
            case PHTLC_SESSIONID:
                PhInitializeStringRef(&getNodeText->Text, processItem->SessionIdString);
                break;
            case PHTLC_PRIORITYCLASS:
                PhInitializeStringRef(&getNodeText->Text, PhGetProcessPriorityClassWin32String(processItem->PriorityClassWin32));
                break;
            case PHTLC_BASEPRIORITY:
                PhPrintInt32(node->BasePriorityText, processItem->BasePriority);
                PhInitializeStringRef(&getNodeText->Text, node->BasePriorityText);
                break;
            case PHTLC_THREADS:
                PhPrintUInt32(node->ThreadsText, processItem->NumberOfThreads);
                PhInitializeStringRef(&getNodeText->Text, node->ThreadsText);
                break;
            case PHTLC_HANDLES:
                PhPrintUInt32(node->HandlesText, processItem->NumberOfHandles);
                PhInitializeStringRef(&getNodeText->Text, node->HandlesText);
                break;
            case PHTLC_GDIHANDLES:
                PhpUpdateProcessNodeGdiUserHandles(node);
                PhPrintUInt32(node->GdiHandlesText, node->GdiHandles);
                PhInitializeStringRef(&getNodeText->Text, node->GdiHandlesText);
                break;
            case PHTLC_USERHANDLES:
                PhpUpdateProcessNodeGdiUserHandles(node);
                PhPrintUInt32(node->UserHandlesText, node->UserHandles);
                PhInitializeStringRef(&getNodeText->Text, node->UserHandlesText);
                break;
            case PHTLC_IORO:
                {
                    ULONG64 number;

                    if (processItem->IoReadDelta.Delta != processItem->IoReadDelta.Value)
                    {
                        number = processItem->IoReadDelta.Delta + processItem->IoOtherDelta.Delta;
                        number *= 1000;
                        number /= PhCsUpdateInterval;
                    }
                    else
                    {
                        number = 0;
                    }

                    if (number != 0)
                    {
                        PH_FORMAT format[2];

                        format[0].Type = SizeFormatType;
                        format[0].u.Size = number;
                        format[1].Type = StringFormatType;
                        format[1].u.String = perSecondString;
                        PhSwapReference2(&node->IoRoText, PhFormat(format, 2, 0));
                        getNodeText->Text = node->IoRoText->sr;
                    }
                    else
                    {
                        PhInitializeEmptyStringRef(&getNodeText->Text);
                    }
                }
                break;
            case PHTLC_IOW:
                {
                    ULONG64 number;

                    if (processItem->IoReadDelta.Delta != processItem->IoReadDelta.Value)
                    {
                        number = processItem->IoWriteDelta.Delta;
                        number *= 1000;
                        number /= PhCsUpdateInterval;
                    }
                    else
                    {
                        number = 0;
                    }

                    if (number != 0)
                    {
                        PH_FORMAT format[2];

                        format[0].Type = SizeFormatType;
                        format[0].u.Size = number;
                        format[1].Type = StringFormatType;
                        format[1].u.String = perSecondString;
                        PhSwapReference2(&node->IoWText, PhFormat(format, 2, 0));
                        getNodeText->Text = node->IoWText->sr;
                    }
                    else
                    {
                        PhInitializeEmptyStringRef(&getNodeText->Text);
                    }
                }
                break;
            case PHTLC_INTEGRITY:
                PhInitializeStringRef(&getNodeText->Text, processItem->IntegrityString);
                break;
            case PHTLC_IOPRIORITY:
                PhpUpdateProcessNodeIoPagePriority(node);
                PhPrintUInt32(node->IoPriorityText, node->IoPriority);
                PhInitializeStringRef(&getNodeText->Text, node->IoPriorityText);
                break;
            case PHTLC_PAGEPRIORITY:
                PhpUpdateProcessNodeIoPagePriority(node);
                PhPrintUInt32(node->PagePriorityText, node->PagePriority);
                PhInitializeStringRef(&getNodeText->Text, node->PagePriorityText);
                break;
            case PHTLC_STARTTIME:
                {
                    SYSTEMTIME systemTime;

                    if (processItem->CreateTime.QuadPart != 0)
                    {
                        PhLargeIntegerToLocalSystemTime(&systemTime, &processItem->CreateTime);
                        PhSwapReference2(&node->StartTimeText, PhFormatDateTime(&systemTime));
                        getNodeText->Text = node->StartTimeText->sr;
                    }
                    else
                    {
                        PhInitializeEmptyStringRef(&getNodeText->Text);
                    }
                }
                break;
            case PHTLC_TOTALCPUTIME:
                PhPrintTimeSpan(node->TotalCpuTimeText,
                    processItem->KernelTime.QuadPart + processItem->UserTime.QuadPart,
                    PH_TIMESPAN_HMSM);
                PhInitializeStringRef(&getNodeText->Text, node->TotalCpuTimeText);
                break;
            case PHTLC_KERNELCPUTIME:
                PhPrintTimeSpan(node->KernelCpuTimeText, processItem->KernelTime.QuadPart, PH_TIMESPAN_HMSM);
                PhInitializeStringRef(&getNodeText->Text, node->KernelCpuTimeText);
                break;
            case PHTLC_USERCPUTIME:
                PhPrintTimeSpan(node->UserCpuTimeText, processItem->UserTime.QuadPart, PH_TIMESPAN_HMSM);
                PhInitializeStringRef(&getNodeText->Text, node->UserCpuTimeText);
                break;
            case PHTLC_VERIFICATIONSTATUS:
                PhInitializeStringRef(&getNodeText->Text,
                    processItem->VerifyResult == VrTrusted ? L"Trusted" : L"Not Trusted");
                break;
            case PHTLC_VERIFIEDSIGNER:
                getNodeText->Text = PhGetStringRefOrEmpty(processItem->VerifySignerName);
                break;
            case PHTLC_RELATIVESTARTTIME:
                {
                    if (processItem->CreateTime.QuadPart != 0)
                    {
                        LARGE_INTEGER currentTime;

                        PhQuerySystemTime(&currentTime);
                        PhSwapReference2(&node->RelativeStartTimeText,
                            PhFormatTimeSpanRelative(currentTime.QuadPart - processItem->CreateTime.QuadPart));
                        getNodeText->Text = node->RelativeStartTimeText->sr;
                    }
                    else
                    {
                        PhInitializeEmptyStringRef(&getNodeText->Text);
                    }
                }
                break;
            case PHTLC_BITS:
#ifdef _M_X64
                PhInitializeStringRef(&getNodeText->Text, processItem->IsWow64 ? L"32-bit" : L"64-bit");
#else
                PhInitializeStringRef(&getNodeText->Text, L"32-bit");
#endif
                break;
            case PHTLC_ELEVATION:
                {
                    PWSTR type;

                    if (WINDOWS_HAS_UAC)
                    {
                        switch (processItem->ElevationType)
                        {
                        case TokenElevationTypeDefault:
                            type = L"N/A";
                            break;
                        case TokenElevationTypeLimited:
                            type = L"Limited";
                            break;
                        case TokenElevationTypeFull:
                            type = L"Full";
                            break;
                        default:
                            type = L"N/A";
                            break;
                        }
                    }
                    else
                    {
                        type = L"";
                    }

                    PhInitializeStringRef(&getNodeText->Text, type);
                }
                break;
            case PHTLC_WINDOWTITLE:
                PhpUpdateProcessNodeWindow(node);
                PhSwapReference(&node->WindowTitleText, node->WindowText);
                getNodeText->Text = PhGetStringRef(node->WindowTitleText);
                break;
            case PHTLC_WINDOWSTATUS:
                PhpUpdateProcessNodeWindow(node);

                if (node->WindowHandle)
                    PhInitializeStringRef(&getNodeText->Text, node->WindowHung ? L"Not responding" : L"Running");
                else
                    PhInitializeEmptyStringRef(&getNodeText->Text);
                break;
            default:
                return FALSE;
            }

            getNodeText->Flags = TLC_CACHE;
        }
        return TRUE;
    case TreeListGetNodeColor:
        {
            PPH_TREELIST_GET_NODE_COLOR getNodeColor = Parameter1;
            PPH_PROCESS_ITEM processItem;

            node = (PPH_PROCESS_NODE)getNodeColor->Node;
            processItem = node->ProcessItem;

            if (PhPluginsEnabled)
            {
                PH_PLUGIN_GET_HIGHLIGHTING_COLOR getHighlightingColor;

                getHighlightingColor.Parameter = processItem;
                getHighlightingColor.BackColor = RGB(0xff, 0xff, 0xff);
                getHighlightingColor.Handled = FALSE;
                getHighlightingColor.Cache = FALSE;

                PhInvokeCallback(PhGetGeneralCallback(GeneralCallbackGetProcessHighlightingColor), &getHighlightingColor);

                if (getHighlightingColor.Handled)
                {
                    getNodeColor->BackColor = getHighlightingColor.BackColor;
                    getNodeColor->Flags = TLGNC_AUTO_FORECOLOR;

                    if (getHighlightingColor.Cache)
                        getNodeColor->Flags |= TLC_CACHE;

                    return TRUE;
                }
            }

            if (!processItem)
                ; // Dummy
            else if (PhCsUseColorDebuggedProcesses && processItem->IsBeingDebugged)
                getNodeColor->BackColor = PhCsColorDebuggedProcesses;
            else if (PhCsUseColorSuspended && processItem->IsSuspended)
                getNodeColor->BackColor = PhCsColorSuspended;
            else if (PhCsUseColorElevatedProcesses && processItem->IsElevated)
                getNodeColor->BackColor = PhCsColorElevatedProcesses;
            else if (PhCsUseColorPosixProcesses && processItem->IsPosix)
                getNodeColor->BackColor = PhCsColorPosixProcesses;
            else if (PhCsUseColorWow64Processes && processItem->IsWow64)
                getNodeColor->BackColor = PhCsColorWow64Processes;
            else if (PhCsUseColorJobProcesses && processItem->IsInSignificantJob)
                getNodeColor->BackColor = PhCsColorJobProcesses;
            //else if (
            //    PhCsUseColorPacked &&
            //    (processItem->VerifyResult != VrUnknown &&
            //    processItem->VerifyResult != VrNoSignature &&
            //    processItem->VerifyResult != VrTrusted
            //    ))
            //    getNodeColor->BackColor = PhCsColorPacked;
            else if (PhCsUseColorDotNet && processItem->IsDotNet)
                getNodeColor->BackColor = PhCsColorDotNet;
            else if (PhCsUseColorPacked && processItem->IsPacked)
                getNodeColor->BackColor = PhCsColorPacked;
            else if (PhCsUseColorServiceProcesses && processItem->ServiceList && processItem->ServiceList->Count != 0)
                getNodeColor->BackColor = PhCsColorServiceProcesses;
            else if (
                PhCsUseColorSystemProcesses &&
                processItem->UserName &&
                PhEqualString(processItem->UserName, PhLocalSystemName, TRUE)
                )
                getNodeColor->BackColor = PhCsColorSystemProcesses;
            else if (
                PhCsUseColorOwnProcesses &&
                processItem->UserName &&
                PhCurrentUserName &&
                PhEqualString(processItem->UserName, PhCurrentUserName, TRUE)
                )
                getNodeColor->BackColor = PhCsColorOwnProcesses;

            getNodeColor->Flags = TLC_CACHE | TLGNC_AUTO_FORECOLOR;
        }
        return TRUE;
    case TreeListGetNodeIcon:
        {
            PPH_TREELIST_GET_NODE_ICON getNodeIcon = Parameter1;

            node = (PPH_PROCESS_NODE)getNodeIcon->Node;

            if (node->ProcessItem->SmallIcon)
            {
                getNodeIcon->Icon = node->ProcessItem->SmallIcon;
            }
            else
            {
                getNodeIcon->Icon = PhGetStockAppIcon();
            }

            getNodeIcon->Flags = TLC_CACHE;
        }
        return TRUE;
    case TreeListGetNodeTooltip:
        {
            PPH_TREELIST_GET_NODE_TOOLTIP getNodeTooltip = Parameter1;

            node = (PPH_PROCESS_NODE)getNodeTooltip->Node;

            if (!node->TooltipText)
                node->TooltipText = PhGetProcessTooltipText(node->ProcessItem);

            if (node->TooltipText)
                getNodeTooltip->Text = node->TooltipText->sr;
            else
                return FALSE;
        }
        return TRUE;
    case TreeListSortChanged:
        {
            TreeList_GetSort(hwnd, &ProcessTreeListSortColumn, &ProcessTreeListSortOrder);
            // Force a rebuild to sort the items.
            TreeList_NodesStructured(hwnd);
        }
        return TRUE;
    case TreeListKeyDown:
        {
            switch ((SHORT)Parameter1)
            {
            case 'C':
                if (GetKeyState(VK_CONTROL) < 0)
                    SendMessage(PhMainWndHandle, WM_COMMAND, ID_PROCESS_COPY, 0);
                break;
            case VK_DELETE:
                if (GetKeyState(VK_SHIFT) >= 0)
                    SendMessage(PhMainWndHandle, WM_COMMAND, ID_PROCESS_TERMINATE, 0);
                else
                    SendMessage(PhMainWndHandle, WM_COMMAND, ID_PROCESS_TERMINATETREE, 0);

                break;
            case VK_RETURN:
                SendMessage(PhMainWndHandle, WM_COMMAND, ID_PROCESS_PROPERTIES, 0);
                break;
            }
        }
        return TRUE;
    case TreeListHeaderRightClick:
        {
            HMENU menu;
            HMENU subMenu;
            POINT point;

            menu = LoadMenu(PhInstanceHandle, MAKEINTRESOURCE(IDR_PROCESSHEADER));
            subMenu = GetSubMenu(menu, 0);
            GetCursorPos(&point);

            if ((UINT)TrackPopupMenu(
                subMenu,
                TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD,
                point.x,
                point.y,
                0,
                hwnd,
                NULL
                ) == ID_HEADER_CHOOSECOLUMNS)
            {
                PhShowChooseColumnsDialog(hwnd, hwnd);
            }

            DestroyMenu(menu);
        }
        return TRUE;
    case TreeListNodeRightClick:
        {
            PPH_TREELIST_MOUSE_EVENT mouseEvent = Parameter2;

            PhShowProcessContextMenu(mouseEvent->Location);
        }
        return TRUE;
    case TreeListNodeLeftDoubleClick:
        {
            SendMessage(PhMainWndHandle, WM_COMMAND, ID_PROCESS_PROPERTIES, 0);
        }
        return TRUE;
    }

    return FALSE;
}

PPH_PROCESS_ITEM PhGetSelectedProcessItem()
{
    PPH_PROCESS_ITEM processItem = NULL;
    ULONG i;

    for (i = 0; i < ProcessNodeList->Count; i++)
    {
        PPH_PROCESS_NODE node = ProcessNodeList->Items[i];

        if (node->Node.Selected)
        {
            processItem = node->ProcessItem;
            break;
        }
    }

    return processItem;
}

VOID PhGetSelectedProcessItems(
    __out PPH_PROCESS_ITEM **Processes,
    __out PULONG NumberOfProcesses
    )
{
    PPH_LIST list;
    ULONG i;

    list = PhCreateList(2);

    for (i = 0; i < ProcessNodeList->Count; i++)
    {
        PPH_PROCESS_NODE node = ProcessNodeList->Items[i];

        if (node->Node.Selected)
        {
            PhAddItemList(list, node->ProcessItem);
        }
    }

    *Processes = PhAllocateCopy(list->Items, sizeof(PVOID) * list->Count);
    *NumberOfProcesses = list->Count;

    PhDereferenceObject(list);
}

VOID PhDeselectAllProcessNodes()
{
    ULONG i;

    for (i = 0; i < ProcessNodeList->Count; i++)
    {
        PPH_PROCESS_NODE node = ProcessNodeList->Items[i];

        node->Node.Selected = FALSE;
        PhInvalidateTreeListNode(&node->Node, TLIN_STATE);
    }

    InvalidateRect(ProcessTreeListHandle, NULL, TRUE);
}

VOID PhInvalidateAllProcessNodes()
{
    ULONG i;

    for (i = 0; i < ProcessNodeList->Count; i++)
    {
        PPH_PROCESS_NODE node = ProcessNodeList->Items[i];

        memset(node->TextCache, 0, sizeof(PH_STRINGREF) * PHTLC_MAXIMUM);
        PhInvalidateTreeListNode(&node->Node, TLIN_COLOR);
        node->ValidMask = 0;
    }

    InvalidateRect(ProcessTreeListHandle, NULL, TRUE);
}

VOID PhSelectAndEnsureVisibleProcessNode(
    __in PPH_PROCESS_NODE ProcessNode
    )
{
    PPH_PROCESS_NODE processNode;
    BOOLEAN needsRestructure = FALSE;

    PhDeselectAllProcessNodes();

    if (!ProcessNode->Node.Visible)
        return;

    // Expand recursively, upwards.

    processNode = ProcessNode->Parent;

    while (processNode)
    {
        if (!processNode->Node.Expanded)
            needsRestructure = TRUE;

        processNode->Node.Expanded = TRUE;
        processNode = processNode->Parent;
    }

    // ListView_SetItemState is used as well.
    // To reproduce the bug:
    // 1. Select and then deselect an item.
    // 2. Call PhSelectAndEnsureVisibleProcessNode.
    // 3. Select another item (without Ctrl). The existing item doesn't get deselected.

    ProcessNode->Node.Selected = TRUE;
    ProcessNode->Node.Focused = TRUE;
    PhInvalidateTreeListNode(&ProcessNode->Node, TLIN_STATE);

    if (needsRestructure)
        TreeList_NodesStructured(ProcessTreeListHandle);

    ListView_SetItemState(TreeList_GetListView(ProcessTreeListHandle), ProcessNode->Node.s.ViewIndex,
        LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

    TreeList_EnsureVisible(ProcessTreeListHandle, &ProcessNode->Node, FALSE);
}

PPH_PROCESS_TREE_FILTER_ENTRY PhAddProcessTreeFilter(
    __in PPH_PROCESS_TREE_FILTER Filter,
    __in_opt PVOID Context
    )
{
    PPH_PROCESS_TREE_FILTER_ENTRY entry;

    entry = PhAllocate(sizeof(PH_PROCESS_TREE_FILTER_ENTRY));
    entry->Filter = Filter;
    entry->Context = Context;

    if (!ProcessTreeFilterList)
        ProcessTreeFilterList = PhCreateList(2);

    PhAddItemList(ProcessTreeFilterList, entry);

    return entry;
}

VOID PhRemoveProcessTreeFilter(
    __in PPH_PROCESS_TREE_FILTER_ENTRY Entry
    )
{
    ULONG index;

    if (!ProcessTreeFilterList)
        return;

    index = PhFindItemList(ProcessTreeFilterList, Entry);

    if (index != -1)
    {
        PhRemoveItemList(ProcessTreeFilterList, index);
        PhFree(Entry);
    }
}

BOOLEAN PhpApplyProcessTreeFiltersToNode(
    __in PPH_PROCESS_NODE Node
    )
{
    BOOLEAN show;
    ULONG i;

    show = TRUE;

    if (ProcessTreeFilterList)
    {
        for (i = 0; i < ProcessTreeFilterList->Count; i++)
        {
            PPH_PROCESS_TREE_FILTER_ENTRY entry;

            entry = ProcessTreeFilterList->Items[i];

            if (!entry->Filter(Node, entry->Context))
            {
                show = FALSE;
                break;
            }
        }
    }

    return show;
}

VOID PhApplyProcessTreeFilters()
{
    ULONG i;

    for (i = 0; i < ProcessNodeList->Count; i++)
    {
        PPH_PROCESS_NODE node;

        node = ProcessNodeList->Items[i];
        node->Node.Visible = PhpApplyProcessTreeFiltersToNode(node);

        if (!node->Node.Visible)
            node->Node.Selected = FALSE;

        PhInvalidateTreeListNode(&node->Node, TLIN_STATE);
    }

    TreeList_NodesStructured(ProcessTreeListHandle);
}

VOID PhCopyProcessTree()
{
    PPH_FULL_STRING text;

    text = PhGetProcessTreeListText(ProcessTreeListHandle);
    PhSetClipboardStringEx(ProcessTreeListHandle, text->Buffer, text->Length);
    PhDereferenceObject(text);
}

VOID PhWriteProcessTree(
    __inout PPH_FILE_STREAM FileStream,
    __in ULONG Mode
    )
{
    PPH_LIST lines;
    ULONG i;

    lines = PhGetProcessTreeListLines(
        ProcessTreeListHandle,
        ProcessNodeList->Count,
        ProcessNodeRootList,
        Mode
        );

    for (i = 0; i < lines->Count; i++)
    {
        PPH_STRING line;

        line = lines->Items[i];
        PhWriteStringAsAnsiFileStream(FileStream, &line->sr);
        PhDereferenceObject(line);
        PhWriteStringAsAnsiFileStream2(FileStream, L"\r\n");
    }

    PhDereferenceObject(lines);
}
