#ifndef __BARCODESCANNERHID_ADDIN_H__
#define __BARCODESCANNERHID_ADDIN_H__

#include "../../include/ComponentBase.h"
#include "../../include/AddInDefBase.h"
#include "../../include/IMemoryManager.h"
#include "ConversionWchar.h"
#include "BarcodeScannerHID.h"

class BarcodeScannerHIDAddIn : public IComponentBase
{
public:
    enum Props
    {
        ePropIsEnabled = 0,
        ePropForProcessing,
        ePropModeSPP,
        ePropSPPDeviceName,
        ePropLast
    };

    enum Methods
    {
        eMethEnable = 0,
        eMethDisable,
        eMethGetBarcode,
        eMethGetDescriptionLastError,
        eMethLast
    };

    BarcodeScannerHIDAddIn();
    virtual ~BarcodeScannerHIDAddIn();

    // IInitDoneBase
    virtual bool ADDIN_API Init(void* pConnection) override;
    virtual bool ADDIN_API setMemManager(void* mem) override;
    virtual long ADDIN_API GetInfo() override;
    virtual void ADDIN_API Done() override;

    // ILanguageExtenderBase
    virtual bool ADDIN_API RegisterExtensionAs(WCHAR_T** wsExtensionName) override;
    virtual long ADDIN_API GetNProps() override;
    virtual long ADDIN_API FindProp(const WCHAR_T* wsPropName) override;
    virtual const WCHAR_T* ADDIN_API GetPropName(long lPropNum, long lPropAlias) override;
    virtual bool ADDIN_API GetPropVal(const long lPropNum, tVariant* pvarPropVal) override;
    virtual bool ADDIN_API SetPropVal(const long lPropNum, tVariant* varPropVal) override;
    virtual bool ADDIN_API IsPropReadable(const long lPropNum) override;
    virtual bool ADDIN_API IsPropWritable(const long lPropNum) override;
    virtual long ADDIN_API GetNMethods() override;
    virtual long ADDIN_API FindMethod(const WCHAR_T* wsMethodName) override;
    virtual const WCHAR_T* ADDIN_API GetMethodName(const long lMethodNum, const long lMethodAlias) override;
    virtual long ADDIN_API GetNParams(const long lMethodNum) override;
    virtual bool ADDIN_API GetParamDefValue(const long lMethodNum, const long lParamNum, tVariant* pvarParamDefValue) override;
    virtual bool ADDIN_API HasRetVal(const long lMethodNum) override;
    virtual bool ADDIN_API CallAsProc(const long lMethodNum, tVariant* paParams, const long lSizeArray) override;
    virtual bool ADDIN_API CallAsFunc(const long lMethodNum, tVariant* pvarRetValue, tVariant* paParams, const long lSizeArray) override;

    // ILocaleBase
    virtual void ADDIN_API SetLocale(const WCHAR_T* loc) override;
    // UserLanguageBase
    virtual void ADDIN_API SetUserInterfaceLanguageCode(const WCHAR_T* lang) override;

private:
    long findName(const wchar_t* names[], const wchar_t* name, const uint32_t size) const;
    bool sendChangeEvent();

    IAddInDefBaseEx* m_iConnect;
    IMemoryManager*  m_iMemory;
    bool             isEnabled;
    bool             forProcessing;
    CBarcodeScannerHID watcher;
};

#endif // __BARCODESCANNERHID_ADDIN_H__
