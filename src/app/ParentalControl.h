#pragma once

#include <QObject>
#include <QSettings>
#include <QtQml/qqmlregistration.h>

// Parental control: a PIN that hides chosen ("locked") categories, and
// Settings, until it is entered. Locked again when the app restarts.
class ParentalControl : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool hasPin READ hasPin NOTIFY changed)
    // Locked content and Settings are hidden right now.
    Q_PROPERTY(bool locked READ isLocked NOTIFY changed)

public:
    explicit ParentalControl(QObject *parent = nullptr);

    bool hasPin() const;
    bool isLocked() const { return m_locked; }

    Q_INVOKABLE bool isValidPin(const QString &pin) const;
    // Sets the first PIN (current empty) or changes it. Leaves it unlocked.
    Q_INVOKABLE bool setPin(const QString &currentPin, const QString &newPin);
    Q_INVOKABLE bool removePin(const QString &currentPin);
    Q_INVOKABLE bool checkPin(const QString &pin) const;
    Q_INVOKABLE bool unlock(const QString &pin);
    Q_INVOKABLE void lock();

    Q_INVOKABLE void setCategoryLocked(int sourceId, const QString &kind, const QString &extId,
                                       bool locked);
    Q_INVOKABLE int lockedCategoryCount(int sourceId) const;
    // Categories whose names look like adult content, not locked yet.
    Q_INVOKABLE int adultCategoryCount(int sourceId) const;
    Q_INVOKABLE int lockAdultCategories(int sourceId);

signals:
    void changed();

private:
    void setLocked(bool locked);
    void notifyContent(int sourceId = -1);
    QList<QPair<QString, QString>> adultCategories(int sourceId) const; // (kind, extId)

    QSettings m_settings;
    bool m_locked = false;
};
