#ifndef PH_MINIINFO_H
#define PH_MINIINFO_H

#include <procgrp.h>

typedef VOID (NTAPI *PPH_MINIINFO_SET_SECTION_TEXT)(
    _In_ struct _PH_MINIINFO_SECTION *Section,
    _In_opt_ PPH_STRING Text
    );

typedef struct _PH_MINIINFO_PARAMETERS
{
    HWND ContainerWindowHandle;
    HWND MiniInfoWindowHandle;

    HFONT Font;
    HFONT MediumFont;
    ULONG FontHeight;
    ULONG FontAverageWidth;
    ULONG MediumFontHeight;
    ULONG MediumFontAverageWidth;

    PPH_MINIINFO_SET_SECTION_TEXT SetSectionText;
} PH_MINIINFO_PARAMETERS, *PPH_MINIINFO_PARAMETERS;

typedef enum _PH_MINIINFO_SECTION_MESSAGE
{
    MiniInfoCreate,
    MiniInfoDestroy,
    MiniInfoTick,
    MiniInfoSectionChanging, // PPH_MINIINFO_SECTION Parameter1
    MiniInfoShowing, // BOOLEAN Parameter1 (Showing)
    MiniInfoCreateDialog, // PPH_MINIINFO_CREATE_DIALOG Parameter1
    MaxMiniInfoMessage
} PH_MINIINFO_SECTION_MESSAGE;

typedef BOOLEAN (NTAPI *PPH_MINIINFO_SECTION_CALLBACK)(
    _In_ struct _PH_MINIINFO_SECTION *Section,
    _In_ PH_MINIINFO_SECTION_MESSAGE Message,
    _In_opt_ PVOID Parameter1,
    _In_opt_ PVOID Parameter2
    );

typedef struct _PH_MINIINFO_CREATE_DIALOG
{
    BOOLEAN CustomCreate;

    // Parameters for default create
    PVOID Instance;
    PWSTR Template;
    DLGPROC DialogProc;
    PVOID Parameter;
} PH_MINIINFO_CREATE_DIALOG, *PPH_MINIINFO_CREATE_DIALOG;

#define PH_MINIINFO_SECTION_NO_UPPER_MARGINS 0x1

typedef struct _PH_MINIINFO_SECTION
{
    // Public

    // Initialization
    PH_STRINGREF Name;
    ULONG Flags;
    PPH_MINIINFO_SECTION_CALLBACK Callback;
    PVOID Context;
    PVOID Reserved1[3];

    PPH_MINIINFO_PARAMETERS Parameters;
    PVOID Reserved2[3];

    // Private

    struct
    {
        ULONG SpareFlags : 32;
    };
    HWND DialogHandle;
    PPH_STRING Text;
} PH_MINIINFO_SECTION, *PPH_MINIINFO_SECTION;

typedef enum _PH_MINIINFO_LIST_SECTION_MESSAGE
{
    MiListSectionCreate,
    MiListSectionDestroy,
    MiListSectionTick,
    MiListSectionShowing, // BOOLEAN Parameter1 (Showing)
    MiListSectionDialogCreated, // HWND Parameter1
    MiListSectionSortProcessList, // PPH_MINIINFO_LIST_SECTION_SORT_LIST Parameter1
    MiListSectionAssignSortData, // PPH_MINIINFO_LIST_SECTION_ASSIGN_SORT_DATA Parameter1
    MiListSectionSortGroupList, // PPH_MINIINFO_LIST_SECTION_SORT_LIST Parameter1
    MiListSectionGetTitleText, // PPH_MINIINFO_LIST_SECTION_GET_TITLE_TEXT Parameter1
    MiListSectionGetUsageText, // PPH_MINIINFO_LIST_SECTION_GET_USAGE_TEXT Parameter1
    MaxMiListSectionMessage
} PH_MINIINFO_LIST_SECTION_MESSAGE;

typedef BOOLEAN (NTAPI *PPH_MINIINFO_LIST_SECTION_CALLBACK)(
    _In_ struct _PH_MINIINFO_LIST_SECTION *ListSection,
    _In_ PH_MINIINFO_LIST_SECTION_MESSAGE Message,
    _In_opt_ PVOID Parameter1,
    _In_opt_ PVOID Parameter2
    );

// The list section performs the following steps when constructing the list of process groups:
// 1. MiListSectionSortProcessList is sent in order to sort the process list.
// 2. A small number of process groups is created from the first few processes in the sorted list (typically high
//    resource consumers).
// 3. MiListSectionAssignSortData is sent for each process group so that the user can assign custom sort data to
//    each process group.
// 4. MiListSectionSortGroupList is sent in order to ensure that the process groups are correctly sorted by resource
//    usage.
// The user also has access to the sort data when handling MiListSectionGetTitleText and MiListSectionGetUsageText.

typedef struct _PH_MINIINFO_LIST_SECTION_SORT_DATA
{
    PH_TREENEW_NODE DoNotModify;
    ULONGLONG UserData[4];
} PH_MINIINFO_LIST_SECTION_SORT_DATA, *PPH_MINIINFO_LIST_SECTION_SORT_DATA;

typedef struct _PH_MINIINFO_LIST_SECTION_ASSIGN_SORT_DATA
{
    PPH_PROCESS_GROUP ProcessGroup;
    PPH_MINIINFO_LIST_SECTION_SORT_DATA SortData;
} PH_MINIINFO_LIST_SECTION_ASSIGN_SORT_DATA, *PPH_MINIINFO_LIST_SECTION_ASSIGN_SORT_DATA;

typedef struct _PH_MINIINFO_LIST_SECTION_SORT_LIST
{
    // MiListSectionSortProcessList: List of PPH_PROCESS_NODE
    // MiListSectionSortGroupList: List of PPH_MINIINFO_LIST_SECTION_SORT_DATA
    PPH_LIST List;
} PH_MINIINFO_LIST_SECTION_SORT_LIST, *PPH_MINIINFO_LIST_SECTION_SORT_LIST;

typedef struct _PH_MINIINFO_LIST_SECTION_GET_TITLE_TEXT
{
    PPH_PROCESS_GROUP ProcessGroup;
    PPH_MINIINFO_LIST_SECTION_SORT_DATA SortData;
    PPH_STRING Title; // Top line (may already contain a string)
    PPH_STRING Subtitle; // Bottom line (may already contain a string)
    COLORREF TitleColor;
    COLORREF SubtitleColor;
} PH_MINIINFO_LIST_SECTION_GET_TITLE_TEXT, *PPH_MINIINFO_LIST_SECTION_GET_TITLE_TEXT;

typedef struct _PH_MINIINFO_LIST_SECTION_GET_USAGE_TEXT
{
    PPH_PROCESS_GROUP ProcessGroup;
    PPH_MINIINFO_LIST_SECTION_SORT_DATA SortData;
    PPH_STRING Line1; // Top line
    PPH_STRING Line2; // Bottom line
    COLORREF Line1Color;
    COLORREF Line2Color;
} PH_MINIINFO_LIST_SECTION_GET_USAGE_TEXT, *PPH_MINIINFO_LIST_SECTION_GET_USAGE_TEXT;

typedef struct _PH_MINIINFO_LIST_SECTION
{
    // Public

    PPH_MINIINFO_SECTION Section;
    HWND DialogHandle; // State
    HWND TreeNewHandle; // State
    PVOID Context;
    PPH_MINIINFO_LIST_SECTION_CALLBACK Callback;

    // Private

    PH_LAYOUT_MANAGER LayoutManager;
    ULONG RunCount;
    PPH_LIST ProcessGroupList;
    PPH_LIST NodeList;
    HANDLE SelectedRepresentativeProcessId;
    LARGE_INTEGER SelectedRepresentativeCreateTime;
} PH_MINIINFO_LIST_SECTION, *PPH_MINIINFO_LIST_SECTION;

typedef enum _PH_MINIINFO_PIN_TYPE
{
    MiniInfoManualPinType, // User pin
    MiniInfoIconPinType, // Notification icon
    MiniInfoActivePinType, // Window is active
    MiniInfoHoverPinType, // Cursor is over mini info window
    MiniInfoChildControlPinType, // Interacting with child control
    MaxMiniInfoPinType
} PH_MINIINFO_PIN_TYPE;

#define PH_MINIINFO_ACTIVATE_WINDOW 0x1
#define PH_MINIINFO_LOAD_POSITION 0x2
#define PH_MINIINFO_DONT_CHANGE_SECTION_IF_PINNED 0x4

VOID PhPinMiniInformation(
    _In_ PH_MINIINFO_PIN_TYPE PinType,
    _In_ LONG PinCount,
    _In_opt_ ULONG PinDelayMs,
    _In_ ULONG Flags,
    _In_opt_ PWSTR SectionName,
    _In_opt_ PPOINT SourcePoint
    );

#endif