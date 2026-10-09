package com.barcodescannerhid.lib;

import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.content.pm.PackageManager;
import android.os.Build;
import android.text.TextUtils;
import android.util.Log;
import android.view.KeyCharacterMap;
import android.os.Looper;
import android.view.ActionMode;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.KeyboardShortcutGroup;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.SearchEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.view.accessibility.AccessibilityEvent;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.Charset;
import java.util.List;
import java.util.Set;
import java.util.UUID;

@android.support.annotation.Keep
public class BarcodeScannerHIDClass
{
    private static final String SCAN_PREFIX = "@@";
    private static final String TAG = "BarcodeScannerHID";
    private static final UUID SPP_UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");
    private static final Charset SPP_CHARSET = Charset.forName("UTF-8");
    private static final String PERMISSION_BLUETOOTH_CONNECT = "android.permission.BLUETOOTH_CONNECT";
    private static final int REQUEST_BLUETOOTH_CONNECT = 17042;
    private static final long SPP_START_TIMEOUT_MS = 8000L;

    private Activity mActivity;
    private long mCallObject;
    private final Object mScanLock = new Object();
    private final Object mSppLock = new Object();
    private final StringBuilder mScanBuffer = new StringBuilder();
    private String mLastScan = "";
    private volatile String mLastError = "";
    private volatile boolean mIsWatching = false;
    private volatile boolean mModeSPP = false;
    private volatile boolean mStopSppRequested = false;
    private String mSppDeviceName = "";
    private Thread mSppThread;
    private BluetoothSocket mSppSocket;
    private Window.Callback mOriginalWindowCallback;
    private Window.Callback mWrappedWindowCallback;

    private static native void OnBarcodeScannerData(long pObject, String content);

    public BarcodeScannerHIDClass(Activity activity, long owner)
    {
        mActivity = activity;
        mCallObject = owner;
        try
        {
            System.loadLibrary("ka_soft_BarcodeScannerHID");
        }
        catch (UnsatisfiedLinkError e)
        {
            mLastError = "Error loading library ka_soft_BarcodeScannerHID";
        }
    }

    public void setModeSPP(boolean mode)
    {
        Log.i(TAG, "setModeSPP: " + mode);
        if (mModeSPP == mode)
            return;

        boolean restart = mIsWatching;
        if (restart)
            stopWatch();

        mModeSPP = mode;

        if (restart)
            startWatch();
    }

    public void setSPPDeviceName(String deviceName)
    {
        String value = deviceName == null ? "" : deviceName;
        Log.i(TAG, "setSPPDeviceName: " + value);
        boolean restart = mIsWatching && mModeSPP && !value.equals(mSppDeviceName);

        if (restart)
            stopWatch();

        mSppDeviceName = value;

        if (restart)
            startWatch();
    }

    public boolean startWatch()
    {
        if (mModeSPP)
            return startSppWatch();

        return startHidWatch();
    }

    public void stopWatch()
    {
        stopSppWatch();
        stopHidWatch();
        mIsWatching = false;
    }

    public String getBarcodeText()
    {
        synchronized (mScanLock)
        {
            return mLastScan;
        }
    }

    public void clearObjectsReference()
    {
        stopWatch();
        mCallObject = 0;
        mActivity = null;
    }

    public String getErrorDescr()
    {
        return mLastError;
    }

    private boolean startHidWatch()
    {
        stopSppWatch();

        final Activity activity = mActivity;
        if (activity == null)
        {
            mLastError = "Activity is unavailable";
            return false;
        }

        runOnUiThread(activity, new Runnable()
        {
            @Override
            public void run()
            {
                Window window = activity.getWindow();
                if (window == null)
                {
                    mLastError = "Window is unavailable";
                    return;
                }

                Window.Callback currentCallback = window.getCallback();
                if (currentCallback == null)
                {
                    mLastError = "Window callback is unavailable";
                    return;
                }

                if (currentCallback == mWrappedWindowCallback)
                {
                    mIsWatching = true;
                    return;
                }

                mOriginalWindowCallback = currentCallback;
                mWrappedWindowCallback = new BarcodeWindowCallback(currentCallback);
                window.setCallback(mWrappedWindowCallback);
                clearBufferedScan();
                mIsWatching = true;
                mLastError = "";
            }
        });
        return true;
    }

    private void stopHidWatch()
    {
        final Activity activity = mActivity;
        runOnUiThread(activity, new Runnable()
        {
            @Override
            public void run()
            {
                if (activity == null)
                {
                    clearBufferedScan();
                    return;
                }

                Window window = activity.getWindow();
                if (window != null && mWrappedWindowCallback != null && window.getCallback() == mWrappedWindowCallback && mOriginalWindowCallback != null)
                    window.setCallback(mOriginalWindowCallback);

                mWrappedWindowCallback = null;
                mOriginalWindowCallback = null;
                clearBufferedScan();
            }
        });
    }

    private boolean startSppWatch()
    {
        Log.i(TAG, "startSppWatch");
        stopHidWatch();

        final Activity activity = mActivity;
        final String deviceName = mSppDeviceName;

        if (activity == null)
        {
            mLastError = "Activity is unavailable";
            return false;
        }

        if (TextUtils.isEmpty(deviceName))
        {
            mLastError = "SPPDeviceName is empty";
            Log.e(TAG, mLastError);
            return false;
        }

        if (!hasBluetoothConnectPermission(activity))
        {
            requestBluetoothConnectPermission(activity);
            mLastError = "Bluetooth connect permission is required. Allow it and call Enable again";
            Log.e(TAG, mLastError);
            return false;
        }

        final SppStartupState startupState = new SppStartupState();
        synchronized (mSppLock)
        {
            if (mSppThread != null && mSppThread.isAlive())
            {
                mIsWatching = true;
                return true;
            }

            mStopSppRequested = false;
            mSppThread = new Thread(new Runnable()
            {
                @Override
                public void run()
                {
                    runSppLoop(activity, deviceName, startupState);
                }
            }, "BarcodeScannerHID-SPP");
            mSppThread.setDaemon(true);
            clearBufferedScan();
            mIsWatching = true;
            mLastError = "";
            mSppThread.start();
        }

        return waitForSppStartup(startupState);
    }

    private void stopSppWatch()
    {
        Thread threadToStop;
        BluetoothSocket socketToClose;

        synchronized (mSppLock)
        {
            mStopSppRequested = true;
            threadToStop = mSppThread;
            socketToClose = mSppSocket;
            mSppSocket = null;
        }

        closeSocket(socketToClose);

        if (threadToStop != null && threadToStop != Thread.currentThread())
        {
            try
            {
                threadToStop.join(500);
            }
            catch (InterruptedException e)
            {
                Thread.currentThread().interrupt();
            }
        }

        synchronized (mSppLock)
        {
            if (mSppThread == threadToStop && (threadToStop == null || !threadToStop.isAlive()))
                mSppThread = null;
        }
    }

    private void runSppLoop(Activity activity, String deviceName, SppStartupState startupState)
    {
        BluetoothSocket socket = null;
        boolean currentThread = false;
        boolean startupReported = false;

        try
        {
            BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
            if (adapter == null)
            {
                setSppStartupError(startupState, "Bluetooth adapter is unavailable");
                startupReported = true;
                return;
            }

            if (!adapter.isEnabled())
            {
                setSppStartupError(startupState, "Bluetooth is disabled");
                startupReported = true;
                return;
            }

            BluetoothDevice device = findBondedDeviceByName(adapter, deviceName);
            if (device == null)
            {
                setSppStartupError(startupState, "Paired Bluetooth device not found: " + deviceName);
                startupReported = true;
                return;
            }
            Log.i(TAG, "SPP device found: " + device.getName());

            try
            {
                adapter.cancelDiscovery();
            }
            catch (SecurityException ignored)
            {
            }

            socket = device.createRfcommSocketToServiceRecord(SPP_UUID);
            synchronized (mSppLock)
            {
                if (mStopSppRequested)
                {
                    closeSocket(socket);
                    reportSppStartupResult(startupState, false);
                    startupReported = true;
                    return;
                }
                mSppSocket = socket;
            }

            socket.connect();
            Log.i(TAG, "SPP socket connected");
            InputStream input = socket.getInputStream();
            reportSppStartupResult(startupState, true);
            startupReported = true;
            readSppStream(input);
        }
        catch (SecurityException e)
        {
            if (!mStopSppRequested)
            {
                mLastError = "Bluetooth permission denied: " + safeMessage(e);
                Log.e(TAG, mLastError, e);
                if (!startupReported)
                    reportSppStartupResult(startupState, false);
            }
        }
        catch (IOException e)
        {
            if (!mStopSppRequested)
            {
                mLastError = "SPP connection error: " + safeMessage(e);
                Log.e(TAG, mLastError, e);
                if (!startupReported)
                    reportSppStartupResult(startupState, false);
            }
        }
        finally
        {
            closeSocket(socket);
            synchronized (mSppLock)
            {
                currentThread = mSppThread == Thread.currentThread();
                if (currentThread)
                {
                    mSppSocket = null;
                    mSppThread = null;
                }
            }
            if (currentThread && mModeSPP)
                mIsWatching = false;
        }
    }

    private boolean waitForSppStartup(SppStartupState startupState)
    {
        long deadline = System.currentTimeMillis() + SPP_START_TIMEOUT_MS;
        boolean interrupted = false;

        synchronized (startupState)
        {
            while (!startupState.completed)
            {
                long waitTime = deadline - System.currentTimeMillis();
                if (waitTime <= 0)
                    break;

                try
                {
                    startupState.wait(waitTime);
                }
                catch (InterruptedException e)
                {
                    interrupted = true;
                    Thread.currentThread().interrupt();
                    break;
                }
            }

            if (startupState.completed)
                return startupState.successful;
        }

        if (interrupted)
            mLastError = "SPP connection wait interrupted";
        else
            mLastError = "SPP connection timeout";

        Log.e(TAG, mLastError);
        reportSppStartupResult(startupState, false);
        stopSppWatch();
        return false;
    }

    private void setSppStartupError(SppStartupState startupState, String error)
    {
        mLastError = error;
        Log.e(TAG, mLastError);
        reportSppStartupResult(startupState, false);
    }

    private void reportSppStartupResult(SppStartupState startupState, boolean successful)
    {
        if (startupState == null)
            return;

        synchronized (startupState)
        {
            if (startupState.completed)
                return;

            startupState.successful = successful;
            startupState.completed = true;
            startupState.notifyAll();
        }
    }

    private BluetoothDevice findBondedDeviceByName(BluetoothAdapter adapter, String deviceName)
    {
        Set<BluetoothDevice> devices = adapter.getBondedDevices();
        BluetoothDevice caseInsensitiveMatch = null;
        BluetoothDevice containsMatch = null;
        String requestedLower = deviceName.toLowerCase();

        if (devices == null)
            return null;

        for (BluetoothDevice device : devices)
        {
            String currentName = device.getName();
            if (deviceName.equals(currentName))
                return device;

            if (caseInsensitiveMatch == null && currentName != null && deviceName.equalsIgnoreCase(currentName))
                caseInsensitiveMatch = device;

            if (containsMatch == null && currentName != null && currentName.toLowerCase().contains(requestedLower))
                containsMatch = device;
        }

        return caseInsensitiveMatch != null ? caseInsensitiveMatch : containsMatch;
    }

    private void readSppStream(InputStream input) throws IOException
    {
        ByteArrayOutputStream buffer = new ByteArrayOutputStream(128);
        boolean skipLfAfterCr = false;

        while (!mStopSppRequested)
        {
            int value = input.read();
            if (value < 0)
                break;

            if (skipLfAfterCr && value == '\n')
            {
                skipLfAfterCr = false;
                continue;
            }

            skipLfAfterCr = false;

            if (value == '\r')
            {
                submitSppScan(buffer);
                buffer.reset();
                skipLfAfterCr = true;
            }
            else
            {
                buffer.write(value);
            }
        }
    }

    private void submitSppScan(ByteArrayOutputStream buffer)
    {
        byte[] data = buffer.toByteArray();
        if (data.length == 0)
            return;

        String payload = new String(data, SPP_CHARSET);
        Log.i(TAG, "SPP scan received, length=" + payload.length());
        publishScan(payload);
    }

    private void publishScan(String payload)
    {
        final String content = payload == null ? "" : payload;
        synchronized (mScanLock)
        {
            mLastScan = content;
        }
        Log.i(TAG, "publishScan, length=" + content.length());

        final Activity activity = mActivity;
        if (activity != null)
        {
            runOnUiThread(activity, new Runnable()
            {
                @Override
                public void run()
                {
                    if (mCallObject != 0)
                        OnBarcodeScannerData(mCallObject, content);
                }
            });
        }
        else if (mCallObject != 0)
        {
            OnBarcodeScannerData(mCallObject, content);
        }
    }

    private boolean hasBluetoothConnectPermission(Activity activity)
    {
        if (Build.VERSION.SDK_INT < 31)
            return true;

        return activity.checkSelfPermission(PERMISSION_BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED;
    }

    private void requestBluetoothConnectPermission(Activity activity)
    {
        if (Build.VERSION.SDK_INT < 31)
            return;

        activity.requestPermissions(new String[] { PERMISSION_BLUETOOTH_CONNECT }, REQUEST_BLUETOOTH_CONNECT);
    }

    private void closeSocket(BluetoothSocket socket)
    {
        if (socket == null)
            return;

        try
        {
            socket.close();
        }
        catch (IOException ignored)
        {
        }
    }

    private String safeMessage(Exception e)
    {
        String message = e.getMessage();
        return message == null ? e.getClass().getSimpleName() : message;
    }

    private void handleKeyEvent(KeyEvent event)
    {
        if (!mIsWatching || mModeSPP || event == null || !isScannerInputEvent(event))
            return;

        if (event.getAction() == KeyEvent.ACTION_MULTIPLE)
        {
            processCharacters(event.getCharacters());
            return;
        }

        int keyCode = event.getKeyCode();
        if (keyCode == KeyEvent.KEYCODE_ENTER || keyCode == KeyEvent.KEYCODE_NUMPAD_ENTER)
        {
            submitBufferedScan();
            return;
        }

        if (keyCode == KeyEvent.KEYCODE_DEL)
        {
            removeLastBufferedChar();
            return;
        }

        int unicodeChar = event.getUnicodeChar(event.getMetaState());
        if (unicodeChar == 0)
            return;

        if (unicodeChar == '\r' || unicodeChar == '\n')
        {
            submitBufferedScan();
            return;
        }

        if (Character.isISOControl(unicodeChar))
            return;

        appendCharacter((char)unicodeChar);
    }

    private boolean isScannerInputEvent(KeyEvent event)
    {
        if (event.getAction() == KeyEvent.ACTION_MULTIPLE)
            return !TextUtils.isEmpty(event.getCharacters());

        if (event.getAction() != KeyEvent.ACTION_DOWN)
            return false;

        InputDevice device = event.getDevice();
        if (device == null)
            return true;

        if (!device.isVirtual())
            return true;

        if (event.getDeviceId() != KeyCharacterMap.VIRTUAL_KEYBOARD)
            return true;

        int source = event.getSource();
        return source == 0 || (source & InputDevice.SOURCE_KEYBOARD) == InputDevice.SOURCE_KEYBOARD;
    }

    private void processCharacters(CharSequence characters)
    {
        if (TextUtils.isEmpty(characters))
            return;

        for (int i = 0; i < characters.length(); i++)
        {
            char current = characters.charAt(i);
            if (current == '\r' || current == '\n')
                submitBufferedScan();
            else if (!Character.isISOControl(current))
                appendCharacter(current);
        }
    }

    private void appendCharacter(char value)
    {
        synchronized (mScanLock)
        {
            mScanBuffer.append(value);
        }
    }

    private void removeLastBufferedChar()
    {
        synchronized (mScanLock)
        {
            int length = mScanBuffer.length();
            if (length > 0)
                mScanBuffer.deleteCharAt(length - 1);
        }
    }

    private void clearBufferedScan()
    {
        synchronized (mScanLock)
        {
            mScanBuffer.setLength(0);
            mLastScan = "";
        }
    }

    private void submitBufferedScan()
    {
        String payload;
        synchronized (mScanLock)
        {
            payload = normalizeBufferedScan(mScanBuffer);
            mScanBuffer.setLength(0);

            if (!payload.startsWith(SCAN_PREFIX) || payload.length() <= SCAN_PREFIX.length())
                return;

            payload = payload.substring(SCAN_PREFIX.length());
            mLastScan = payload;
        }

        if (mCallObject != 0)
            OnBarcodeScannerData(mCallObject, payload);
    }

    private String normalizeBufferedScan(CharSequence input)
    {
        StringBuilder result = new StringBuilder(input.length());
        for (int i = 0; i < input.length(); i++)
        {
            char current = input.charAt(i);
            if (!Character.isWhitespace(current))
                result.append(current);
        }
        return result.toString();
    }

    private void runOnUiThread(Activity activity, Runnable action)
    {
        if (activity == null || action == null)
            return;

        if (Looper.myLooper() == Looper.getMainLooper())
            action.run();
        else
            activity.runOnUiThread(action);
    }

    private static final class SppStartupState
    {
        private boolean completed;
        private boolean successful;
    }

    private final class BarcodeWindowCallback implements Window.Callback
    {
        private final Window.Callback mDelegate;

        private BarcodeWindowCallback(Window.Callback delegate)
        {
            mDelegate = delegate;
        }

        @Override
        public boolean dispatchKeyEvent(KeyEvent event)
        {
            handleKeyEvent(event);
            return mDelegate != null && mDelegate.dispatchKeyEvent(event);
        }

        @Override
        public boolean dispatchKeyShortcutEvent(KeyEvent event)
        {
            handleKeyEvent(event);
            return mDelegate != null && mDelegate.dispatchKeyShortcutEvent(event);
        }

        @Override
        public boolean dispatchTouchEvent(MotionEvent event)
        {
            return mDelegate != null && mDelegate.dispatchTouchEvent(event);
        }

        @Override
        public boolean dispatchTrackballEvent(MotionEvent event)
        {
            return mDelegate != null && mDelegate.dispatchTrackballEvent(event);
        }

        @Override
        public boolean dispatchGenericMotionEvent(MotionEvent event)
        {
            return mDelegate != null && mDelegate.dispatchGenericMotionEvent(event);
        }

        @Override
        public boolean dispatchPopulateAccessibilityEvent(AccessibilityEvent event)
        {
            return mDelegate != null && mDelegate.dispatchPopulateAccessibilityEvent(event);
        }

        @Override
        public View onCreatePanelView(int featureId)
        {
            return mDelegate != null ? mDelegate.onCreatePanelView(featureId) : null;
        }

        @Override
        public boolean onCreatePanelMenu(int featureId, Menu menu)
        {
            return mDelegate != null && mDelegate.onCreatePanelMenu(featureId, menu);
        }

        @Override
        public boolean onPreparePanel(int featureId, View view, Menu menu)
        {
            return mDelegate != null && mDelegate.onPreparePanel(featureId, view, menu);
        }

        @Override
        public boolean onMenuOpened(int featureId, Menu menu)
        {
            return mDelegate != null && mDelegate.onMenuOpened(featureId, menu);
        }

        @Override
        public boolean onMenuItemSelected(int featureId, MenuItem item)
        {
            return mDelegate != null && mDelegate.onMenuItemSelected(featureId, item);
        }

        @Override
        public void onWindowAttributesChanged(WindowManager.LayoutParams attrs)
        {
            if (mDelegate != null)
                mDelegate.onWindowAttributesChanged(attrs);
        }

        @Override
        public void onContentChanged()
        {
            if (mDelegate != null)
                mDelegate.onContentChanged();
        }

        @Override
        public void onWindowFocusChanged(boolean hasFocus)
        {
            if (mDelegate != null)
                mDelegate.onWindowFocusChanged(hasFocus);
        }

        @Override
        public void onAttachedToWindow()
        {
            if (mDelegate != null)
                mDelegate.onAttachedToWindow();
        }

        @Override
        public void onDetachedFromWindow()
        {
            if (mDelegate != null)
                mDelegate.onDetachedFromWindow();
        }

        @Override
        public void onPanelClosed(int featureId, Menu menu)
        {
            if (mDelegate != null)
                mDelegate.onPanelClosed(featureId, menu);
        }

        @Override
        public boolean onSearchRequested()
        {
            return mDelegate != null && mDelegate.onSearchRequested();
        }

        @Override
        public boolean onSearchRequested(SearchEvent searchEvent)
        {
            return mDelegate != null && mDelegate.onSearchRequested(searchEvent);
        }

        @Override
        public ActionMode onWindowStartingActionMode(ActionMode.Callback callback)
        {
            return mDelegate != null ? mDelegate.onWindowStartingActionMode(callback) : null;
        }

        @Override
        public ActionMode onWindowStartingActionMode(ActionMode.Callback callback, int type)
        {
            return mDelegate != null ? mDelegate.onWindowStartingActionMode(callback, type) : null;
        }

        @Override
        public void onActionModeStarted(ActionMode mode)
        {
            if (mDelegate != null)
                mDelegate.onActionModeStarted(mode);
        }

        @Override
        public void onActionModeFinished(ActionMode mode)
        {
            if (mDelegate != null)
                mDelegate.onActionModeFinished(mode);
        }

        @Override
        public void onProvideKeyboardShortcuts(List<KeyboardShortcutGroup> data, Menu menu, int deviceId)
        {
            if (mDelegate != null)
                mDelegate.onProvideKeyboardShortcuts(data, menu, deviceId);
        }

        @Override
        public void onPointerCaptureChanged(boolean hasCapture)
        {
            if (mDelegate != null)
                mDelegate.onPointerCaptureChanged(hasCapture);
        }
    }
}
