package org.iptvplayer;

import android.os.Build;
import android.os.Bundle;
import android.window.OnBackInvokedCallback;
import android.window.OnBackInvokedDispatcher;

import org.qtproject.qt.android.bindings.QtActivity;

// Qt's activity plus our own handling of Android's back gesture / button.
//
// On Android 13+ the app registers a back callback, so back (gesture or
// button) always reaches the app: it closes a dialog, leaves the player or goes
// back one screen. Only when there is nothing to go back to does the app move
// to the background, like any Android app. Older versions send the Back key,
// which the app handles the same way.
public class IptvActivity extends QtActivity
{
    private Object m_backCallback; // OnBackInvokedCallback (API 33+)

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        if (Build.VERSION.SDK_INT >= 33) {
            OnBackInvokedCallback callback = new OnBackInvokedCallback() {
                @Override
                public void onBackInvoked()
                {
                    try {
                        nativeBack();
                    } catch (UnsatisfiedLinkError e) {
                        // The app hasn't finished starting: behave like Android.
                        moveTaskToBack(true);
                    }
                }
            };
            getOnBackInvokedDispatcher().registerOnBackInvokedCallback(
                    OnBackInvokedDispatcher.PRIORITY_DEFAULT, callback);
            m_backCallback = callback;
        }
    }

    @Override
    protected void onDestroy()
    {
        if (Build.VERSION.SDK_INT >= 33 && m_backCallback != null) {
            getOnBackInvokedDispatcher().unregisterOnBackInvokedCallback(
                    (OnBackInvokedCallback) m_backCallback);
            m_backCallback = null;
        }
        super.onDestroy();
    }

    // Implemented in C++ (src/platform/AndroidBack.cpp).
    private static native void nativeBack();
}
