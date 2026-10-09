#include "BarcodeScannerHID.h"
#include "ConversionWchar.h"
#include "../../include/AddInDefBase.h"

#if defined(__ANDROID__)

#include "../include/IAndroidComponentHelper.h"
#include "Android_Proj/jni/jnienv.h"
#include <jni.h>
#include <time.h>
#include <vector>

std::wstring jstring2wstring_local(JNIEnv* jenv, jstring aStr)
{
    std::wstring result;
    if (aStr)
    {
        const jchar* pCh = jenv->GetStringChars(aStr, 0);
        jsize len = jenv->GetStringLength(aStr);
        result.assign(pCh, pCh + len);
        jenv->ReleaseStringChars(aStr, pCh);
    }
    return result;
}

jstring wstring2jstring_local(JNIEnv* jenv, const std::wstring& value)
{
    std::vector<jchar> chars;
    chars.reserve(value.size());
    for (std::wstring::const_iterator it = value.begin(); it != value.end(); ++it)
        chars.push_back((jchar)*it);

    if (chars.empty())
    {
        jchar empty = 0;
        return jenv->NewString(&empty, 0);
    }

    return jenv->NewString(&chars[0], (jsize)chars.size());
}

class CBarcodeScannerHIDRecorder
{
private:
    IAndroidComponentHelper* helper;
    jclass                   cc;
    jobject                  obj;
    IAddInDefBaseEx*         cnn;
    ClipboardData*           shrdData;
    bool                     isChangedData;
    jmethodID                methodID_start;
    jmethodID                methodID_stop;
    jmethodID                methodID_getBarcode;
    jmethodID                methodID_clearObjectsReference;
    jmethodID                methodID_getErrorDescr;
    jmethodID                methodID_setModeSPP;
    jmethodID                methodID_setSPPDeviceName;

public:
    CBarcodeScannerHIDRecorder(IAddInDefBaseEx* m_iConnect, ClipboardData* sharedData) :
        helper(0), cc(0), obj(0), cnn(m_iConnect), shrdData(sharedData), isChangedData(false),
        methodID_start(0), methodID_stop(0), methodID_getBarcode(0), methodID_clearObjectsReference(0), methodID_getErrorDescr(0),
        methodID_setModeSPP(0), methodID_setSPPDeviceName(0)
    {
        shrdData->isComplete = false;
        shrdData->text = L"";
        shrdData->lastChange = time(NULL);

        cnn->SetEventBufferDepth(10);
        helper = (IAndroidComponentHelper*)cnn->GetInterface(eIAndroidComponentHelper);
        if (helper)
        {
            WCHAR_T* className = 0;
            convToShortWchar(&className, L"com/barcodescannerhid/lib/BarcodeScannerHIDClass");
            jclass ccloc = helper->FindClass(className);
            delete[] className;

            if (ccloc)
            {
                JNIEnv* jenv = getJniEnv();
                cc = static_cast<jclass>(jenv->NewGlobalRef(ccloc));
                jenv->DeleteLocalRef(ccloc);
                jobject activity = helper->GetActivity();
                jmethodID ctorID = jenv->GetMethodID(cc, "<init>", "(Landroid/app/Activity;J)V");
                jobject objloc = jenv->NewObject(cc, ctorID, activity, (jlong)this);
                if (objloc)
                {
                    obj = jenv->NewGlobalRef(objloc);
                    jenv->DeleteLocalRef(objloc);
                }
                jenv->DeleteLocalRef(activity);

                methodID_start = jenv->GetMethodID(cc, "startWatch", "()Z");
                methodID_stop = jenv->GetMethodID(cc, "stopWatch", "()V");
                methodID_getBarcode = jenv->GetMethodID(cc, "getBarcodeText", "()Ljava/lang/String;");
                methodID_clearObjectsReference = jenv->GetMethodID(cc, "clearObjectsReference", "()V");
                methodID_getErrorDescr = jenv->GetMethodID(cc, "getErrorDescr", "()Ljava/lang/String;");
                methodID_setModeSPP = jenv->GetMethodID(cc, "setModeSPP", "(Z)V");
                methodID_setSPPDeviceName = jenv->GetMethodID(cc, "setSPPDeviceName", "(Ljava/lang/String;)V");
            }
        }
    }

    ~CBarcodeScannerHIDRecorder()
    {
        JNIEnv* jenv = getJniEnv();
        if (obj && methodID_clearObjectsReference)
            jenv->CallVoidMethod(obj, methodID_clearObjectsReference);
        stopWatching();
        if (obj)
            jenv->DeleteGlobalRef(obj);
        if (cc)
            jenv->DeleteGlobalRef(cc);
    }

    void updateData(const std::wstring& text)
    {
        shrdData->text = text;
        shrdData->isComplete = true;
        shrdData->lastChange = time(NULL);
        cnn->ExternalEvent(WcharWrapper(L"BarcodeScannerHID"), WcharWrapper(L"OnChange"), WcharWrapper(text.c_str()));
    }

    bool startWatching()
    {
        JNIEnv* jenv = getJniEnv();
        shrdData->isComplete = false;
        if (obj && methodID_start)
            return jenv->CallBooleanMethod(obj, methodID_start) == JNI_TRUE;
        return false;
    }

    void stopWatching()
    {
        JNIEnv* jenv = getJniEnv();
        if (obj && methodID_stop)
            jenv->CallVoidMethod(obj, methodID_stop);
    }

    void setModeSPP(bool mode)
    {
        JNIEnv* jenv = getJniEnv();
        if (obj && methodID_setModeSPP)
            jenv->CallVoidMethod(obj, methodID_setModeSPP, mode ? JNI_TRUE : JNI_FALSE);
    }

    void setSPPDeviceName(const std::wstring& deviceName)
    {
        JNIEnv* jenv = getJniEnv();
        if (obj && methodID_setSPPDeviceName)
        {
            jstring value = wstring2jstring_local(jenv, deviceName);
            jenv->CallVoidMethod(obj, methodID_setSPPDeviceName, value);
            if (value)
                jenv->DeleteLocalRef(value);
        }
    }

    std::wstring getBarcodeText()
    {
        JNIEnv* jenv = getJniEnv();
        jstring val = (jstring)(jenv->CallObjectMethod(obj, methodID_getBarcode));
        return jstring2wstring_local(jenv, val);
    }

    std::wstring getLastErrorDescr()
    {
        JNIEnv* jenv = getJniEnv();
        jstring err = (jstring)(jenv->CallObjectMethod(obj, methodID_getErrorDescr));
        return jstring2wstring_local(jenv, err);
    }
};

extern "C" JNIEXPORT void JNICALL Java_com_barcodescannerhid_lib_BarcodeScannerHIDClass_OnBarcodeScannerData(JNIEnv* jenv, jclass, jlong pObject, jstring content)
{
    CBarcodeScannerHIDRecorder* obj = (CBarcodeScannerHIDRecorder*)pObject;
    if (obj)
        obj->updateData(jstring2wstring_local(jenv, content));
}

#else

// Stubs for other platforms (not implemented)
class CBarcodeScannerHIDRecorder
{
public:
    CBarcodeScannerHIDRecorder(IAddInDefBaseEx*, ClipboardData*) {}
    ~CBarcodeScannerHIDRecorder() {}
    bool startWatching() { return false; }
    void stopWatching() {}
    void setModeSPP(bool) {}
    void setSPPDeviceName(const std::wstring&) {}
    std::wstring getBarcodeText() { return L""; }
    std::wstring getLastErrorDescr() { return L""; }
};

#endif // __ANDROID__

CBarcodeScannerHID::CBarcodeScannerHID() : piCnt(0), isRun(false), useSPP(false), recorder(0)
{
}

CBarcodeScannerHID::~CBarcodeScannerHID()
{
    if (recorder)
    {
        delete recorder;
        recorder = 0;
    }
}

void CBarcodeScannerHID::setIConnect(IAddInDefBaseEx* piConnect)
{
    piCnt = piConnect;
    if (piCnt)
    {
        recorder = new CBarcodeScannerHIDRecorder(piCnt, &sharedData);
        recorder->setModeSPP(useSPP);
        recorder->setSPPDeviceName(sppName);
    }
}

bool CBarcodeScannerHID::startWatching()
{
    if (recorder)
    {
        recorder->setModeSPP(useSPP);
        recorder->setSPPDeviceName(sppName);
        isRun = recorder->startWatching();
        lastErrorDescr = recorder->getLastErrorDescr();
        if (!isRun && lastErrorDescr.empty())
            lastErrorDescr = L"Barcode scanner start failed";
        return isRun;
    }
    isRun = false;
    lastErrorDescr = L"Barcode scanner recorder is unavailable";
    return false;
}

void CBarcodeScannerHID::stopWatching()
{
    isRun = false;
    if (recorder)
        recorder->stopWatching();
}

std::wstring CBarcodeScannerHID::barcodeText()
{
    if (sharedData.isComplete)
        return sharedData.text;
    if (recorder)
        return recorder->getBarcodeText();
    return L"";
}

std::wstring CBarcodeScannerHID::getLastErrorDescr()
{
    if (recorder)
        lastErrorDescr = recorder->getLastErrorDescr();
    return lastErrorDescr;
}

bool CBarcodeScannerHID::modeSPP() const
{
    return useSPP;
}

void CBarcodeScannerHID::setModeSPP(bool mode)
{
    useSPP = mode;
    if (recorder)
        recorder->setModeSPP(useSPP);
}

std::wstring CBarcodeScannerHID::sppDeviceName() const
{
    return sppName;
}

void CBarcodeScannerHID::setSPPDeviceName(const std::wstring& deviceName)
{
    sppName = deviceName;
    if (recorder)
        recorder->setSPPDeviceName(sppName);
}

void CBarcodeScannerHID::updateData(const std::wstring& text)
{
    sharedData.text = text;
    sharedData.lastChange = time(NULL);
    sharedData.isComplete = true;
}

void CBarcodeScannerHID::setError(const std::wstring& err)
{
    lastErrorDescr = err;
}
