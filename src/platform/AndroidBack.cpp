#include "platform/AndroidBack.h"

#ifdef Q_OS_ANDROID
#include "app/SpatialNavigation.h"

#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QMetaObject>
#include <QtCore/qcoreapplication_platform.h>

namespace {

// Called on Android's UI thread: handle it on the app's thread.
void nativeBack(JNIEnv *, jclass)
{
    QMetaObject::invokeMethod(qApp, [] {
        SpatialNavigation *navigation = SpatialNavigation::instance();
        if (navigation && navigation->back())
            return;
        // Nothing to go back to (Home): leave the app like Android does.
        QJniObject activity(QNativeInterface::QAndroidApplication::context());
        if (activity.isValid())
            activity.callMethod<jboolean>("moveTaskToBack", "(Z)Z", jboolean(true));
    }, Qt::QueuedConnection);
}

} // namespace

void AndroidBack::install()
{
    QJniEnvironment env;
    const JNINativeMethod methods[] = {
        { "nativeBack", "()V", reinterpret_cast<void *>(nativeBack) },
    };
    if (!env.registerNativeMethods("org/iptvplayer/IptvActivity", methods, 1))
        qWarning("Android back: could not connect to the activity");
}

#else

void AndroidBack::install() {}

#endif
