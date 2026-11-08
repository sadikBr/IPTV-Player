#include "platform/PictureInPicture.h"

#include <QPointer>

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>
#endif

PictureInPicture::PictureInPicture(QObject *parent) : QObject(parent)
{
#ifdef Q_OS_ANDROID
    if (QNativeInterface::QAndroidApplication::sdkVersion() >= 26) {
        QJniObject context(QNativeInterface::QAndroidApplication::context());
        QJniObject pm = context.callObjectMethod("getPackageManager",
                                                 "()Landroid/content/pm/PackageManager;");
        if (pm.isValid()) {
            m_supported = pm.callMethod<jboolean>(
                    "hasSystemFeature", "(Ljava/lang/String;)Z",
                    QJniObject::fromString(QStringLiteral("android.software.picture_in_picture"))
                            .object<jstring>());
        }
        QJniEnvironment env;
        env.checkAndClearExceptions();
    }
#endif
}

void PictureInPicture::setAutoEnter(bool on)
{
    if (on == m_autoEnter)
        return;
    m_autoEnter = on;
    emit autoEnterChanged();
    applyParams(false);
}

void PictureInPicture::enter()
{
    applyParams(true);
}

void PictureInPicture::applyParams(bool enterNow)
{
#ifdef Q_OS_ANDROID
    if (!m_supported)
        return;
    const bool autoEnter = m_autoEnter;
    QPointer<PictureInPicture> self(this);
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([autoEnter, enterNow, self] {
        QJniObject activity(QNativeInterface::QAndroidApplication::context());
        if (!activity.isValid())
            return;
        QJniObject builder("android/app/PictureInPictureParams$Builder");
        QJniObject ratio("android/util/Rational", "(II)V", jint(16), jint(9));
        builder.callObjectMethod("setAspectRatio",
                                 "(Landroid/util/Rational;)Landroid/app/PictureInPictureParams$Builder;",
                                 ratio.object());
        if (QNativeInterface::QAndroidApplication::sdkVersion() >= 31) {
            builder.callObjectMethod("setAutoEnterEnabled",
                                     "(Z)Landroid/app/PictureInPictureParams$Builder;",
                                     jboolean(autoEnter));
        }
        QJniObject params = builder.callObjectMethod("build",
                                                     "()Landroid/app/PictureInPictureParams;");
        activity.callMethod<void>("setPictureInPictureParams",
                                  "(Landroid/app/PictureInPictureParams;)V", params.object());
        if (enterNow) {
            activity.callMethod<jboolean>("enterPictureInPictureMode",
                                          "(Landroid/app/PictureInPictureParams;)Z",
                                          params.object());
        }
        QJniEnvironment env;
        env.checkAndClearExceptions();
    });
#else
    Q_UNUSED(enterNow)
#endif
}

void PictureInPicture::refresh()
{
#ifdef Q_OS_ANDROID
    if (!m_supported)
        return;
    QJniObject activity(QNativeInterface::QAndroidApplication::context());
    if (!activity.isValid())
        return;
    const bool active = activity.callMethod<jboolean>("isInPictureInPictureMode", "()Z");
    QJniEnvironment env;
    env.checkAndClearExceptions();
    setActive(active);
#endif
}

void PictureInPicture::setActive(bool active)
{
    if (active == m_active)
        return;
    m_active = active;
    emit activeChanged();
}
