#include "platform/ScreenWakeLock.h"

#include <QtGlobal>

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>
#endif

namespace ScreenWakeLock {

#ifdef Q_OS_ANDROID
namespace {
bool g_active = false;
}

void setActive(bool active)
{
    if (active == g_active)
        return;
    g_active = active;

    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([active] {
        QJniObject activity(QNativeInterface::QAndroidApplication::context());
        if (!activity.isValid())
            return;
        QJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
        if (!window.isValid())
            return;
        constexpr jint FLAG_KEEP_SCREEN_ON = 0x00000080; // WindowManager.LayoutParams
        if (active)
            window.callMethod<void>("addFlags", "(I)V", FLAG_KEEP_SCREEN_ON);
        else
            window.callMethod<void>("clearFlags", "(I)V", FLAG_KEEP_SCREEN_ON);
    });
}
#else
void setActive(bool active)
{
    Q_UNUSED(active);
}
#endif

} // namespace ScreenWakeLock
