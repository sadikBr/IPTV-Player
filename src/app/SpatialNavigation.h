#pragma once

#include <QObject>
#include <QPointer>
#include <QRectF>

class QQuickItem;
class QQuickWindow;
class QKeyEvent;

// Arrow-key / TV remote navigation for the whole app.
//
// Pages handle keys first (a grid moves its own selection, the player zaps).
// When nobody uses an arrow key, focus moves to the nearest focusable item in
// that direction (anything with activeFocusOnTab: buttons, cards, chips,
// lists), scrolling it into view. Enter / OK clicks the focused button.
class SpatialNavigation : public QObject
{
    Q_OBJECT

public:
    enum Direction { Left, Right, Up, Down };

    explicit SpatialNavigation(QQuickWindow *window);

    // Moves focus from the current item; returns false if nothing is there.
    bool move(Direction direction);
    // Clicks the focused button; returns false if it isn't one.
    bool activate();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct Candidate
    {
        QQuickItem *item;
        QRectF rect; // in window coordinates
    };
    void collect(QQuickItem *item, const QRectF &clip, QList<Candidate> &out) const;
    QQuickItem *searchRoot(QQuickItem *focus) const;
    static void ensureVisible(QQuickItem *item);

    QPointer<QQuickWindow> m_window;
    bool m_delivering = false;
    // The last Back press was used (went back, or a page handled it): its release
    // must be swallowed too, or Android closes the app.
    bool m_backUsed = false;
};
