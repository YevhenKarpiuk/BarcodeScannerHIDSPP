#include "stdafx.h"
#include "AddInNative.h"

static const wchar_t* g_PropNames[] =
    {L"IsEnabled",
     L"ForProcessing",
     L"ModeSPP",
     L"SPPDeviceName"};

static const wchar_t* g_MethodNames[] =
    {L"Enable",
     L"Disable",
     L"GetBarcode",
     L"GetDescriptionLastError"};

static const wchar_t* g_PropNamesRu[] =
    {L"IsEnabled",
     L"ForProcessing",
     L"ModeSPP",
     L"SPPDeviceName"};

static const wchar_t* g_MethodNamesRu[] =
    {L"Enable",
     L"Disable",
     L"GetBarcode",
     L"GetDescriptionLastError"};

static const wchar_t g_ComponentNameType[] = L"ka_soft_BarcodeScannerHID";
const long g_VersionAddIn = 2100;
static AppCapabilities g_capabilities = eAppCapabilitiesInvalid;

static WcharWrapper s_kClassNames(L"BarcodeScannerHID");
static WcharWrapper s_eventChangeName(L"OnChange");
static WcharWrapper s_paramEvent(L"_");

extern "C" long GetClassObject(const WCHAR_T* /*wsName*/, IComponentBase** pInterface)
{
    if (!*pInterface)
    {
        *pInterface = new BarcodeScannerHIDAddIn;
        return (long)*pInterface;
    }
    return 0;
}

extern "C" long DestroyObject(IComponentBase** pInterface)
{
    if (!*pInterface)
        return -1;

    delete *pInterface;
    *pInterface = 0;
    return 0;
}

extern "C" const WCHAR_T* GetClassNames()
{
    return s_kClassNames;
}

extern "C" AppCapabilities SetPlatformCapabilities(const AppCapabilities capabilities)
{
    g_capabilities = capabilities;
    return eAppCapabilitiesLast;
}

BarcodeScannerHIDAddIn::BarcodeScannerHIDAddIn() : m_iConnect(0), m_iMemory(0), isEnabled(false), forProcessing(false)
{
}

BarcodeScannerHIDAddIn::~BarcodeScannerHIDAddIn()
{
}

bool BarcodeScannerHIDAddIn::Init(void* pConnection)
{
    m_iConnect = (IAddInDefBaseEx*)pConnection;
    if (m_iConnect)
    {
        watcher.setIConnect(m_iConnect);
        return true;
    }
    return false;
}

bool BarcodeScannerHIDAddIn::setMemManager(void* mem)
{
    m_iMemory = (IMemoryManager*)mem;
    return m_iMemory != 0;
}

long BarcodeScannerHIDAddIn::GetInfo()
{
    return g_VersionAddIn;
}

void BarcodeScannerHIDAddIn::Done()
{
    m_iConnect = 0;
}

bool BarcodeScannerHIDAddIn::RegisterExtensionAs(WCHAR_T** wsExtensionName)
{
    const wchar_t* wsExtension = g_ComponentNameType;
    uint32_t iActualSize = static_cast<uint32_t>(::wcslen(wsExtension) + 1);

    if (m_iMemory)
    {
        if (m_iMemory->AllocMemory((void**)wsExtensionName, iActualSize * sizeof(WCHAR_T)))
        {
            convToShortWchar(wsExtensionName, wsExtension, iActualSize);
            return true;
        }
    }
    return false;
}

long BarcodeScannerHIDAddIn::GetNProps()
{
    return ePropLast;
}

long BarcodeScannerHIDAddIn::FindProp(const WCHAR_T* wsPropName)
{
    long propNum = -1;
    wchar_t* propName = 0;
    convFromShortWchar(&propName, wsPropName);

    propNum = findName(g_PropNames, propName, ePropLast);
    if (propNum == -1)
        propNum = findName(g_PropNamesRu, propName, ePropLast);

    delete[] propName;
    return propNum;
}

const WCHAR_T* BarcodeScannerHIDAddIn::GetPropName(long lPropNum, long lPropAlias)
{
    if (lPropNum >= ePropLast)
        return 0;

    wchar_t* wsCurrentName = 0;
    WCHAR_T* wsPropName = 0;
    switch (lPropAlias)
    {
    case 0:
        wsCurrentName = (wchar_t*)g_PropNames[lPropNum];
        break;
    case 1:
        wsCurrentName = (wchar_t*)g_PropNamesRu[lPropNum];
        break;
    default:
        return 0;
    }

    uint32_t iActualSize = static_cast<uint32_t>(wcslen(wsCurrentName) + 1);
    if (m_iMemory && wsCurrentName)
    {
        if (m_iMemory->AllocMemory((void**)&wsPropName, iActualSize * sizeof(WCHAR_T)))
            convToShortWchar(&wsPropName, wsCurrentName, iActualSize);
    }
    return wsPropName;
}

bool BarcodeScannerHIDAddIn::GetPropVal(const long lPropNum, tVariant* pvarPropVal)
{
    switch (lPropNum)
    {
    case ePropIsEnabled:
        TV_VT(pvarPropVal) = VTYPE_BOOL;
        TV_BOOL(pvarPropVal) = isEnabled;
        return true;
    case ePropForProcessing:
        TV_VT(pvarPropVal) = VTYPE_BOOL;
        TV_BOOL(pvarPropVal) = forProcessing;
        return true;
    case ePropModeSPP:
        TV_VT(pvarPropVal) = VTYPE_BOOL;
        TV_BOOL(pvarPropVal) = watcher.modeSPP();
        return true;
    case ePropSPPDeviceName:
    {
        std::wstring data = watcher.sppDeviceName();
        uint32_t iActualSize = static_cast<uint32_t>(data.size() + 1);
        if (m_iMemory->AllocMemory((void**)&pvarPropVal->pwstrVal, iActualSize * sizeof(WCHAR_T)))
        {
            convToShortWchar(&pvarPropVal->pwstrVal, data.c_str(), iActualSize);
            pvarPropVal->wstrLen = iActualSize - 1;
            TV_VT(pvarPropVal) = VTYPE_PWSTR;
            return true;
        }
        break;
    }
    default:
        break;
    }
    return false;
}

bool BarcodeScannerHIDAddIn::SetPropVal(const long lPropNum, tVariant* varPropVal)
{
    switch (lPropNum)
    {
    case ePropIsEnabled:
        isEnabled = TV_BOOL(varPropVal) != 0;
        return true;
    case ePropForProcessing:
        forProcessing = TV_BOOL(varPropVal) != 0;
        return true;
    case ePropModeSPP:
        watcher.setModeSPP(TV_BOOL(varPropVal) != 0);
        return true;
    case ePropSPPDeviceName:
    {
        if (TV_VT(varPropVal) != VTYPE_PWSTR || !varPropVal->pwstrVal)
            return false;
        wchar_t* value = 0;
        convFromShortWchar(&value, varPropVal->pwstrVal);
        watcher.setSPPDeviceName(value ? std::wstring(value) : std::wstring());
        delete[] value;
        return true;
    }
    default:
        break;
    }
    return false;
}

bool BarcodeScannerHIDAddIn::IsPropReadable(const long lPropNum)
{
    return lPropNum == ePropIsEnabled || lPropNum == ePropForProcessing ||
           lPropNum == ePropModeSPP || lPropNum == ePropSPPDeviceName;
}

bool BarcodeScannerHIDAddIn::IsPropWritable(const long lPropNum)
{
    return lPropNum == ePropIsEnabled || lPropNum == ePropForProcessing ||
           lPropNum == ePropModeSPP || lPropNum == ePropSPPDeviceName;
}

long BarcodeScannerHIDAddIn::GetNMethods()
{
    return eMethLast;
}

long BarcodeScannerHIDAddIn::FindMethod(const WCHAR_T* wsMethodName)
{
    wchar_t* name = 0;
    convFromShortWchar(&name, wsMethodName);
    long methodNum = findName(g_MethodNames, name, eMethLast);
    if (methodNum == -1)
        methodNum = findName(g_MethodNamesRu, name, eMethLast);
    delete[] name;
    return methodNum;
}

const WCHAR_T* BarcodeScannerHIDAddIn::GetMethodName(const long lMethodNum, const long lMethodAlias)
{
    if (lMethodNum >= eMethLast)
        return 0;

    wchar_t* wsCurrentName = 0;
    WCHAR_T* wsMethodName = 0;
    switch (lMethodAlias)
    {
    case 0:
        wsCurrentName = (wchar_t*)g_MethodNames[lMethodNum];
        break;
    case 1:
        wsCurrentName = (wchar_t*)g_MethodNamesRu[lMethodNum];
        break;
    default:
        return 0;
    }

    uint32_t iActualSize = static_cast<uint32_t>(wcslen(wsCurrentName) + 1);
    if (m_iMemory && wsCurrentName)
    {
        if (m_iMemory->AllocMemory((void**)&wsMethodName, iActualSize * sizeof(WCHAR_T)))
            convToShortWchar(&wsMethodName, wsCurrentName, iActualSize);
    }
    return wsMethodName;
}

long BarcodeScannerHIDAddIn::GetNParams(const long lMethodNum)
{
    switch (lMethodNum)
    {
    case eMethEnable:
    case eMethDisable:
    case eMethGetBarcode:
    case eMethGetDescriptionLastError:
        return 0;
    default:
        break;
    }
    return 0;
}

bool BarcodeScannerHIDAddIn::GetParamDefValue(const long /*lMethodNum*/, const long /*lParamNum*/, tVariant* /*pvarParamDefValue*/)
{
    return false;
}

bool BarcodeScannerHIDAddIn::HasRetVal(const long lMethodNum)
{
    switch (lMethodNum)
    {
    case eMethGetBarcode:
    case eMethGetDescriptionLastError:
        return true;
    default:
        return false;
    }
}

bool BarcodeScannerHIDAddIn::CallAsProc(const long lMethodNum, tVariant* /*paParams*/, const long /*lSizeArray*/)
{
    switch (lMethodNum)
    {
    case eMethEnable:
        isEnabled = watcher.startWatching();
        return true;
    case eMethDisable:
        watcher.stopWatching();
        isEnabled = false;
        return true;
    default:
        break;
    }
    return false;
}

bool BarcodeScannerHIDAddIn::CallAsFunc(const long lMethodNum, tVariant* pvarRetValue, tVariant* paParams, const long lSizeArray)
{
    switch (lMethodNum)
    {
    case eMethGetBarcode:
    {
        if (lSizeArray || paParams)
            return false;
        std::wstring data = watcher.barcodeText();
        uint32_t iActualSize = static_cast<uint32_t>(data.size() + 1);
        if (m_iMemory->AllocMemory((void**)&pvarRetValue->pwstrVal, iActualSize * sizeof(WCHAR_T)))
        {
            convToShortWchar(&pvarRetValue->pwstrVal, data.c_str(), iActualSize);
            pvarRetValue->wstrLen = iActualSize - 1;
            TV_VT(pvarRetValue) = VTYPE_PWSTR;
            return true;
        }
        break;
    }
    case eMethGetDescriptionLastError:
    {
        if (lSizeArray || paParams)
            return false;
        std::wstring errDescr = watcher.getLastErrorDescr();
        uint32_t iActualSize = static_cast<uint32_t>(errDescr.size() + 1);
        if (m_iMemory->AllocMemory((void**)&pvarRetValue->pwstrVal, iActualSize * sizeof(WCHAR_T)))
        {
            convToShortWchar(&pvarRetValue->pwstrVal, errDescr.c_str(), iActualSize);
            pvarRetValue->wstrLen = iActualSize - 1;
            TV_VT(pvarRetValue) = VTYPE_PWSTR;
            return true;
        }
        break;
    }
    default:
        break;
    }
    return false;
}

void BarcodeScannerHIDAddIn::SetLocale(const WCHAR_T* /*loc*/)
{
}

void BarcodeScannerHIDAddIn::SetUserInterfaceLanguageCode(const WCHAR_T* /*lang*/)
{
}

long BarcodeScannerHIDAddIn::findName(const wchar_t* names[], const wchar_t* name, const uint32_t size) const
{
    for (uint32_t i = 0; i < size; i++)
    {
        if (!wcscmp(names[i], name))
            return i;
    }
    return -1;
}

bool BarcodeScannerHIDAddIn::sendChangeEvent()
{
    if (m_iConnect)
    {
        m_iConnect->ExternalEvent(s_kClassNames, s_eventChangeName, s_paramEvent);
        return true;
    }
    return false;
}
