#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

// Android picture-in-picture: keep watching in a small floating window.
// Android 8+; on Android 12+ the player also goes there by itself when the
// user leaves the app while a video plays (autoEnter). Not available on
// desktop (supported is false).
class PictureInPicture : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool supported READ isSupported CONSTANT)
    // The app is shown in the small window right now.
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
    // Go to picture-in-picture by itself when the user leaves the app.
    Q_PROPERTY(bool autoEnter READ autoEnter WRITE setAutoEnter NOTIFY autoEnterChanged)

public:
    explicit PictureInPicture(QObject *parent = nullptr);

    bool isSupported() const { return m_supported; }
    bool isActive() const { return m_active; }
    bool autoEnter() const { return m_autoEnter; }
    void setAutoEnter(bool on);

    Q_INVOKABLE void enter();
    // Re-reads whether the window is in picture-in-picture (call when the
    // window size changes; Android resizes it when entering and leaving).
    Q_INVOKABLE void refresh();

signals:
    void activeChanged();
    void autoEnterChanged();

private:
    void applyParams(bool enterNow);
    void setActive(bool active);

    bool m_supported = false;
    bool m_active = false;
    bool m_autoEnter = false;
};
