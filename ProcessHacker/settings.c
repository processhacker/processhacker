#include <settings.h>
#include <settingsp.h>

PPH_HASHTABLE PhSettingsHashtable;
PH_FAST_LOCK PhSettingsLock;

VOID PhSettingsInitialization()
{
    PhSettingsHashtable = PhCreateHashtable(
        sizeof(PH_SETTING),
        PhpSettingsHashtableCompareFunction,
        PhpSettingsHashtableHashFunction,
        20
        );
    PhInitializeFastLock(&PhSettingsLock);

    PhpAddIntegerPairSetting(L"MainWindowPosition", L"100,100");
    PhpAddIntegerPairSetting(L"MainWindowSize", L"800,600");
    PhpAddIntegerSetting(L"MainWindowState", L"1");
    PhpAddIntegerSetting(L"AllowMultipleInstances", L"0");
    PhpAddIntegerSetting(L"EnableWarnings", L"1");
    PhpAddStringSetting(L"DbgHelpPath", L"dbghelp.dll");
    PhpAddStringSetting(L"DbgHelpSearchPath", L"");
    PhpAddIntegerSetting(L"DbgHelpUndecorate", L"1");
}

BOOLEAN NTAPI PhpSettingsHashtableCompareFunction(
    __in PVOID Entry1,
    __in PVOID Entry2
    )
{
    PPH_SETTING setting1 = (PPH_SETTING)Entry1;
    PPH_SETTING setting2 = (PPH_SETTING)Entry2;

    return WSTR_EQUAL(setting1->Name, setting2->Name);
}

ULONG NTAPI PhpSettingsHashtableHashFunction(
    __in PVOID Entry
    )
{
    PPH_SETTING setting = (PPH_SETTING)Entry;

    return PhHashBytesSdbm((PBYTE)setting->Name, (ULONG)wcslen(setting->Name) * 2);
}

static VOID PhpAddStringSetting(
    __in PWSTR Name,
    __in PWSTR DefaultValue
    )
{
    PhpAddSetting(StringSettingType, Name, DefaultValue);
}

static VOID PhpAddIntegerSetting(
    __in PWSTR Name,
    __in PWSTR DefaultValue
    )
{
    PhpAddSetting(IntegerSettingType, Name, DefaultValue);
}

static VOID PhpAddIntegerPairSetting(
    __in PWSTR Name,
    __in PWSTR DefaultValue
    )
{
    PhpAddSetting(IntegerPairSettingType, Name, DefaultValue);
}

static VOID PhpAddSetting(
    __in PH_SETTING_TYPE Type,
    __in PWSTR Name,
    __in PWSTR DefaultValue
    )
{
    // No need to lock because we only add settings 
    // during initialization.

    PH_SETTING setting;

    setting.Type = Type;
    setting.Name = Name;
    setting.DefaultValue = PhCreateString(DefaultValue);
    setting.Value = NULL;

    PhpSettingFromString(Type, setting.DefaultValue, &setting.Value);

    PhAddHashtableEntry(PhSettingsHashtable, &setting);
}

static PPH_STRING PhpSettingToString(
    __in PH_SETTING_TYPE Type,
    __in PVOID Value
    )
{
    switch (Type)
    {
    case StringSettingType:
        {
            if (!Value)
                return PhCreateString(L"");

            PhReferenceObject(Value);

            return (PPH_STRING)Value;
        }
    case IntegerSettingType:
        {
            return PhFormatString(L"%u", (ULONG)Value);
        }
    case IntegerPairSettingType:
        {
            PPH_INTEGER_PAIR integerPair = (PPH_INTEGER_PAIR)Value;

            return PhFormatString(L"%u,%u", integerPair->X, integerPair->Y);
        }
    }

    return PhCreateString(L"");
}

static BOOLEAN PhpSettingFromString(
    __in PH_SETTING_TYPE Type,
    __in PPH_STRING String,
    __out PPVOID Value
    )
{
    switch (Type)
    {
    case StringSettingType:
        {
            PhReferenceObject(String);
            *Value = String;

            return TRUE;
        }
    case IntegerSettingType:
        {
            ULONG integer;

            if (swscanf(String->Buffer, L"%u", &integer) != EOF)
            {
                *Value = (PVOID)integer;
                return TRUE;
            }
            else
            {
                return FALSE;
            }
        }
    case IntegerPairSettingType:
        {
            PH_INTEGER_PAIR integerPair;

            if (swscanf(String->Buffer, L"%u,%u", &integerPair.X, &integerPair.Y) != EOF)
            {
                *Value = PhAllocateCopy(&integerPair, sizeof(PH_INTEGER_PAIR));
                return TRUE;
            }
            else
            {
                return FALSE;
            }
        }
    }

    return FALSE;
}

static VOID PhpFreeSettingValue(
    __in PH_SETTING_TYPE Type,
    __in PVOID Value
    )
{
    switch (Type)
    {
    case StringSettingType:
        if (Value)
            PhDereferenceObject(Value);
        break;
    case IntegerPairSettingType:
        if (Value)
            PhFree(Value);
        break;
    }
}

static PVOID PhpLookupSetting(
    __in PWSTR Name
    )
{
    PH_SETTING lookupSetting;
    PPH_SETTING setting;

    lookupSetting.Name = Name;
    setting = (PPH_SETTING)PhGetHashtableEntry(
        PhSettingsHashtable,
        &lookupSetting
        );

    return setting;
}

static PPH_STRING PhpJoinXmlTextNodes(
    __in mxml_node_t *node
    )
{
    PPH_STRING_BUILDER stringBuilder;
    PPH_STRING string;

    stringBuilder = PhCreateStringBuilder(10);

    while (node)
    {
        if (node->type == MXML_TEXT)
        {
            PPH_STRING textString;

            if (node->value.text.whitespace)
                PhStringBuilderAppend2(stringBuilder, L" ");

            textString = PhCreateStringFromAnsi(node->value.text.string);
            PhStringBuilderAppend(stringBuilder, textString);
            PhDereferenceObject(textString);
        }

        node = node->next;
    }

    string = PhReferenceStringBuilderString(stringBuilder);
    PhDereferenceObject(stringBuilder);

    return string;
}

ULONG PhGetIntegerSetting(
    __in PWSTR Name
    )
{
    PPH_SETTING setting;
    ULONG value;

    PhAcquireFastLockShared(&PhSettingsLock);

    setting = PhpLookupSetting(Name);

    if (setting && setting->Type == IntegerSettingType)
    {
        value = (ULONG)setting->Value;
    }
    else
    {
        setting = NULL;
    }

    PhReleaseFastLockShared(&PhSettingsLock);

    if (!setting)
        PhRaiseStatus(STATUS_NOT_FOUND);

    return value;
}

PH_INTEGER_PAIR PhGetIntegerPairSetting(
    __in PWSTR Name
    )
{
    PPH_SETTING setting;
    PH_INTEGER_PAIR value;

    PhAcquireFastLockShared(&PhSettingsLock);

    setting = PhpLookupSetting(Name);

    if (setting && setting->Type == IntegerPairSettingType)
    {
        if (setting->Value)
            value = *(PPH_INTEGER_PAIR)setting->Value;
        else
            memset(&value, 0, sizeof(PH_INTEGER_PAIR));
    }
    else
    {
        setting = NULL;
    }

    PhReleaseFastLockShared(&PhSettingsLock);

    if (!setting)
        PhRaiseStatus(STATUS_NOT_FOUND);

    return value;
}

PPH_STRING PhGetStringSetting(
    __in PWSTR Name
    )
{
    PPH_SETTING setting;
    PPH_STRING value;

    PhAcquireFastLockShared(&PhSettingsLock);

    setting = PhpLookupSetting(Name);

    if (setting && setting->Type == StringSettingType)
    {
        if (setting->Value)
        {
            value = (PPH_STRING)setting->Value;
            PhReferenceObject(value);
        }
        else
        {
            value = PhCreateString(L"");
        }
    }
    else
    {
        setting = NULL;
    }

    PhReleaseFastLockShared(&PhSettingsLock);

    if (!setting)
        PhRaiseStatus(STATUS_NOT_FOUND);

    return value;
}

VOID PhSetIntegerSetting(
    __in PWSTR Name,
    __in ULONG Value
    )
{
    PPH_SETTING setting;

    PhAcquireFastLockExclusive(&PhSettingsLock);

    setting = PhpLookupSetting(Name);

    if (setting && setting->Type == IntegerSettingType)
    {
        setting->Value = (PVOID)Value;
    }

    PhReleaseFastLockExclusive(&PhSettingsLock);

    if (!setting)
        PhRaiseStatus(STATUS_NOT_FOUND);
}

VOID PhSetIntegerPairSetting(
    __in PWSTR Name,
    __in PH_INTEGER_PAIR Value
    )
{
    PPH_SETTING setting;

    PhAcquireFastLockExclusive(&PhSettingsLock);

    setting = PhpLookupSetting(Name);

    if (setting && setting->Type == IntegerPairSettingType)
    {
        PhpFreeSettingValue(IntegerPairSettingType, setting->Value);
        setting->Value = PhAllocateCopy(&Value, sizeof(PH_INTEGER_PAIR));
    }

    PhReleaseFastLockExclusive(&PhSettingsLock);

    if (!setting)
        PhRaiseStatus(STATUS_NOT_FOUND);
}

VOID PhSetStringSetting(
    __in PWSTR Name,
    __in PWSTR Value
    )
{
    PPH_SETTING setting;

    PhAcquireFastLockExclusive(&PhSettingsLock);

    setting = PhpLookupSetting(Name);

    if (setting && setting->Type == StringSettingType)
    {
        PhpFreeSettingValue(StringSettingType, setting->Value);
        setting->Value = PhCreateString(Value);
    }

    PhReleaseFastLockExclusive(&PhSettingsLock);

    if (!setting)
        PhRaiseStatus(STATUS_NOT_FOUND);
}

BOOLEAN PhLoadSettings(
    __in PWSTR FileName
    )
{
    FILE *settingsFile;
    mxml_node_t *topNode;
    mxml_node_t *currentNode;

    settingsFile = _wfopen(FileName, L"r");

    if (!settingsFile)
        return FALSE;

    topNode = mxmlLoadFile(NULL, settingsFile, MXML_NO_CALLBACK);
    fclose(settingsFile);

    if (!topNode)
        return FALSE;

    if (!topNode->child)
    {
        mxmlDelete(topNode);
        return FALSE;
    }

    currentNode = topNode->child;

    while (currentNode)
    {
        PPH_STRING settingName = NULL;

        if (
            currentNode->type == MXML_ELEMENT &&
            currentNode->value.element.num_attrs >= 1 &&
            stricmp(currentNode->value.element.attrs[0].name, "name") == 0
            )
        {
            settingName = PhCreateStringFromAnsi(currentNode->value.element.attrs[0].value);
        }

        if (settingName)
        {
            PPH_STRING settingValue;

            settingValue = PhpJoinXmlTextNodes(currentNode->child);

            PhAcquireFastLockExclusive(&PhSettingsLock);

            {
                PPH_SETTING setting;

                setting = PhpLookupSetting(settingName->Buffer);

                if (setting)
                {
                    PhpFreeSettingValue(setting->Type, setting->Value);
                    setting->Value = NULL;

                    if (!PhpSettingFromString(
                        setting->Type,
                        settingValue,
                        &setting->Value
                        ))
                    {
                        PhpSettingFromString(
                            setting->Type,
                            setting->DefaultValue,
                            &setting->Value
                            );
                    }
                }
            }

            PhReleaseFastLockExclusive(&PhSettingsLock);

            PhDereferenceObject(settingValue);
            PhDereferenceObject(settingName);
        }

        currentNode = currentNode->next;
    }

    mxmlDelete(topNode);

    return TRUE;
}

BOOLEAN PhSaveSettings(
    __in PWSTR FileName
    )
{
    FILE *settingsFile;
    mxml_node_t *topNode;
    ULONG enumerationKey = 0;
    PPH_SETTING setting;

    topNode = mxmlNewElement(MXML_NO_PARENT, "settings");

    PhAcquireFastLockShared(&PhSettingsLock);

    while (PhEnumHashtable(PhSettingsHashtable, &setting, &enumerationKey))
    {
        mxml_node_t *settingNode;
        mxml_node_t *textNode;
        PPH_ANSI_STRING settingName;
        PPH_STRING settingValue;
        PPH_ANSI_STRING settingValueAnsi;

        // Create the setting element.

        settingNode = mxmlNewElement(topNode, "setting");

        settingName = PhCreateAnsiStringFromUnicode(setting->Name);
        mxmlElementSetAttr(settingNode, "name", settingName->Buffer);
        PhDereferenceObject(settingName);

        // Set the value.

        settingValue = PhpSettingToString(setting->Type, setting->Value);
        settingValueAnsi = PhCreateAnsiStringFromUnicodeEx(settingValue->Buffer, settingValue->Length);
        PhDereferenceObject(settingValue);

        textNode = mxmlNewText(settingNode, FALSE, settingValueAnsi->Buffer);
        PhDereferenceObject(settingValueAnsi);
    }

    PhReleaseFastLockShared(&PhSettingsLock);

    // Create the directory if it does not exist.
    {
        PPH_STRING fullPath;
        ULONG indexOfFileName;

        fullPath = PhGetFullPath(FileName, &indexOfFileName);

        if (fullPath)
        {
            PPH_STRING directoryName;

            directoryName = PhSubstring(fullPath, 0, indexOfFileName);

            SHCreateDirectoryEx(NULL, directoryName->Buffer, NULL);

            PhDereferenceObject(directoryName);
            PhDereferenceObject(fullPath);
        }
    }

    settingsFile = _wfopen(FileName, L"w");

    if (!settingsFile)
    {
        mxmlDelete(topNode);
        return FALSE;
    }

    mxmlSaveFile(topNode, settingsFile, MXML_NO_CALLBACK);
    mxmlDelete(topNode);
    fclose(settingsFile);

    return TRUE;
}
