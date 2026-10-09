#ifndef __BARCODESCANNERHID_H__
#define __BARCODESCANNERHID_H__

#include "ConversionWchar.h"
#include <string>
#include <time.h>
#include "../../include/mobile.h"

class IAddInDefBaseEx;

struct ClipboardData
{
    ClipboardData() : text(L""), lastChange(time(NULL)), isComplete(false) {}
    std::wstring text;
    time_t       lastChange;
    bool         isComplete;
};

class CBarcodeScannerHIDRecorder;

class CBarcodeScannerHID
{
public:
    CBarcodeScannerHID();
    ~CBarcodeScannerHID();

    void setIConnect(IAddInDefBaseEx* piConnect);
    bool startWatching();
    void stopWatching();
    std::wstring barcodeText();
    std::wstring getLastErrorDescr();
    bool modeSPP() const;
    void setModeSPP(bool mode);
    std::wstring sppDeviceName() const;
    void setSPPDeviceName(const std::wstring& deviceName);
    void updateData(const std::wstring& text);
    void setError(const std::wstring& err);

private:
    ClipboardData          sharedData;
    IAddInDefBaseEx*       piCnt;
    bool                   isRun;
    bool                   useSPP;
    std::wstring           sppName;
    std::wstring           lastErrorDescr;
    CBarcodeScannerHIDRecorder* recorder;
};

#endif // __BARCODESCANNERHID_H__
