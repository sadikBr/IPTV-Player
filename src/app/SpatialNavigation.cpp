#include "SpatialNavigation.h"

#include <QCoreApplication>
#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickWindow>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

bool isFlickable(const QQuickItem *item)
{
    return item->inherits("QQuickFlickable");
}

// Grids and lists that move their own selection with the arrow keys: focus
// goes to the view, not to one of its delegates.
bool isKeyNavigatingView(const QQuickItem *item)
{
    return item->inherits("QQuickItemView") && item->activeFocusOnTab();
}

bool isOverlay(const QQuickItem *item)
{
    return item->inherits("QQuickOverlay");
}

// The part of the window an item belongs to: the side bar, the page stack,
// the bottom bar, or a popup.
const QQuickItem *region(const QQuickItem *item, const QQuickItem *root)
{
    const QQuickItem *last = item;
    for (const QQuickItem *i = item; i && i != root; i = i->parentItem())
        last = i;
    return last;
}

QRectF windowRect(const QQuickItem *item)
{
    return item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
}

} // namespace

SpatialNavigation::SpatialNavigation(QQuickWindow *window) : QObject(window), m_window(window)
{
    window->installEventFilter(this);
}

bool SpatialNavigation::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_window || m_delivering)
        return false;
    if (event->type() == QEvent::KeyRelease) {
        // Android quits when the Back release is not accepted, even if its press
        // took us back a screen. Swallow it unless Back is meant to close the app.
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Back && m_backUsed) {
            if (!key->isAutoRepeat())
                m_backUsed = false;
            key->accept();
            return true;
        }
        return false;
    }
    if (event->type() != QEvent::KeyPress)
        return false;
    auto *key = static_cast<QKeyEvent *>(event);
    if (key->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
        return false;

    int direction = -1;
    bool activateKey = false;
    switch (key->key()) {
    case Qt::Key_Left: direction = Left; break;
    case Qt::Key_Right: direction = Right; break;
    case Qt::Key_Up: direction = Up; break;
    case Qt::Key_Down: direction = Down; break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Select: activateKey = true; break;
    case Qt::Key_Escape:
    case Qt::Key_Back: {
        // Back one screen when nothing on the page used the key.
        m_delivering = true;
        key->setAccepted(true);
        QCoreApplication::sendEvent(m_window, key);
        m_delivering = false;
        if (key->isAccepted() && m_window->activeFocusItem()
            && m_window->activeFocusItem() != m_window->contentItem()) {
            m_backUsed = key->key() == Qt::Key_Back;
            return true;
        }
        QVariant result;
        QMetaObject::invokeMethod(m_window, "goBack", Q_RETURN_ARG(QVariant, result));
        const bool wentBack = result.toBool();
        if (key->key() == Qt::Key_Back)
            m_backUsed = wentBack;
        // Nothing to go back to: Android closes the app (Back key).
        return wentBack || key->key() == Qt::Key_Escape;
    }
    default: return false;
    }

    // In a grid, Left in the first column (Right in the last) leaves the grid
    // instead of wrapping to the previous / next row.
    if (direction == Left || direction == Right) {
        for (QQuickItem *item = m_window->activeFocusItem(); item; item = item->parentItem()) {
            if (!item->inherits("QQuickGridView"))
                continue;
            const qreal cellWidth = item->property("cellWidth").toReal();
            const int columns = cellWidth > 0 ? qMax(1, int(item->width() / cellWidth)) : 1;
            const int index = item->property("currentIndex").toInt();
            const int count = item->property("count").toInt();
            const bool atEdge = direction == Left ? index % columns == 0
                                                  : (index % columns == columns - 1 || index == count - 1);
            if (atEdge && move(static_cast<Direction>(direction)))
                return true;
            break;
        }
    }

    // Let the page have it first.
    m_delivering = true;
    QQuickItem *before = m_window->activeFocusItem();
    QQuickItem *popup = nullptr;
    for (QQuickItem *item = before; item; item = item->parentItem()) {
        if (item->inherits("QQuickPopupItem")) {
            popup = item;
            break;
        }
    }
    if (popup && before != popup) {
        // Inside a dialog: the dialog itself takes every key, so offer it to
        // the focused control and its parents up to (not including) the
        // dialog, like Qt does for the window.
        key->setAccepted(false);
        for (QQuickItem *item = before; item && item != popup && !key->isAccepted();
             item = item->parentItem()) {
            key->setAccepted(true);
            QCoreApplication::sendEvent(item, key);
        }
    } else {
        key->setAccepted(true);
        QCoreApplication::sendEvent(m_window, key);
    }
    m_delivering = false;
    QQuickItem *focus = m_window->activeFocusItem();
    // A dialog that has focus itself (not one of its buttons) swallows keys.
    const bool nothingFocused = !focus || focus == m_window->contentItem()
            || focus->inherits("QQuickPopupItem");
    if (key->isAccepted() && !nothingFocused)
        return true;

    if (activateKey)
        activate();
    else
        move(static_cast<Direction>(direction));
    return true;
}

QQuickItem *SpatialNavigation::searchRoot(QQuickItem *focus) const
{
    // Inside a popup or dialog: stay in it.
    for (QQuickItem *item = focus; item; item = item->parentItem()) {
        QQuickItem *parent = item->parentItem();
        if (parent && isOverlay(parent))
            return item;
    }
    return m_window->contentItem();
}

void SpatialNavigation::collect(QQuickItem *item, const QRectF &clip, QList<Candidate> &out) const
{
    if (!item->isVisible() || !item->isEnabled() || item->opacity() < 0.01)
        return;
    if (item != m_window->contentItem() && isOverlay(item))
        return; // popups are searched on their own

    const QRectF rect = windowRect(item);
    QRectF childClip = clip;
    if (item->clip()) {
        // A scrolling area may hold items that are scrolled out of view
        // along its scroll direction; they become visible when focused.
        if (isFlickable(item)) {
            const bool vertical = item->property("contentHeight").toReal() > item->height() + 1;
            const bool horizontal = item->property("contentWidth").toReal() > item->width() + 1;
            // Visible part of the area, unlimited along the axis it scrolls.
            childClip = clip.intersected(rect);
            if (vertical) {
                childClip.setTop(-1e7);
                childClip.setBottom(1e7);
            }
            if (horizontal) {
                childClip.setLeft(-1e7);
                childClip.setRight(1e7);
            }
        } else {
            childClip = clip.intersected(rect);
        }
    }

    if (qEnvironmentVariableIsSet("IPTV_NAV_DEBUG") && item->inherits("QQuickFlickable"))
        qInfo("nav flickable %s rect %.0f,%.0f %.0fx%.0f clip %.0f,%.0f %.0fx%.0f content %.0fx%.0f",
              item->metaObject()->className(), rect.x(), rect.y(), rect.width(), rect.height(),
              childClip.x(), childClip.y(), childClip.width(), childClip.height(),
              item->property("contentWidth").toReal(), item->property("contentHeight").toReal());
    if (item->activeFocusOnTab() && rect.width() > 1 && rect.height() > 1
        && clip.intersects(rect)) {
        out.append({ item, rect });
        if (isKeyNavigatingView(item))
            return; // the view handles its delegates itself
    }
    const QList<QQuickItem *> children = item->childItems();
    for (QQuickItem *child : children)
        collect(child, childClip, out);
}

bool SpatialNavigation::move(Direction direction)
{
    QQuickItem *focus = m_window->activeFocusItem();
    QQuickItem *root = searchRoot(focus);

    // Everything visible in the window (plus what scrolling areas can reveal)
    QList<Candidate> candidates;
    const QRectF windowArea(0, 0, m_window->width(), m_window->height());
    collect(root, windowArea, candidates);
    if (candidates.isEmpty())
        return false;
    if (qEnvironmentVariableIsSet("IPTV_NAV_DEBUG")) {
        for (const Candidate &c : std::as_const(candidates))
            qInfo("nav candidate %s \"%s\" %.0f,%.0f %.0fx%.0f", c.item->metaObject()->className(),
                  qPrintable(c.item->property("text").toString().left(20)), c.rect.x(), c.rect.y(),
                  c.rect.width(), c.rect.height());
    }

    // The focused item may be part of a control (the text inside a search
    // box); use the focusable item that contains it.
    QQuickItem *focusable = nullptr;
    for (QQuickItem *item = focus; item; item = item->parentItem()) {
        if (item->activeFocusOnTab()) {
            focusable = item;
            break;
        }
    }
    // Nothing focusable has focus (a page that was just opened, or nothing at
    // all): start at the top left of what has focus.
    const bool hasFocus = focusable != nullptr;
    QQuickItem *scope = nullptr;
    QRectF from;
    if (hasFocus) {
        focus = focusable;
        from = windowRect(focus);
    } else {
        if (focus && focus != m_window->contentItem() && focus != root) {
            scope = focus;
            // Prefer what's inside the page that has focus.
            const bool anyInside = std::any_of(candidates.cbegin(), candidates.cend(),
                                               [scope](const Candidate &c) {
                                                   return scope->isAncestorOf(c.item);
                                               });
            if (!anyInside)
                scope = nullptr;
        }
        const QRectF area = scope ? windowRect(scope) : windowArea;
        from = QRectF(area.topLeft(), QSizeF(0, 0));
        // A page can name where focus starts (e.g. its Play button).
        if (focus) {
            auto *initial = qvariant_cast<QQuickItem *>(focus->property("initialFocusItem"));
            if (initial && initial->isVisible() && initial->isEnabled()) {
                initial->forceActiveFocus(Qt::TabFocusReason);
                ensureVisible(initial);
                return true;
            }
        }
    }

    QQuickItem *best = nullptr;
    qreal bestScore = std::numeric_limits<qreal>::max();
    for (const Candidate &c : std::as_const(candidates)) {
        if (hasFocus && (c.item == focus || focus->isAncestorOf(c.item) || c.item->isAncestorOf(focus)))
            continue;
        if (scope && !scope->isAncestorOf(c.item))
            continue;
        const QRectF &r = c.rect;
        qreal gap = 0;     // distance along the direction
        qreal offset = 0;  // distance across it (0 when they overlap)
        constexpr qreal tolerance = 4;
        if (!hasFocus) {
            gap = r.top() - from.top();
            offset = (r.left() - from.left()) * 0.25;
        } else {
            switch (direction) {
            case Right:
                if (r.left() < from.right() - tolerance)
                    continue;
                gap = r.left() - from.right();
                break;
            case Left:
                if (r.right() > from.left() + tolerance)
                    continue;
                gap = from.left() - r.right();
                break;
            case Down:
                if (r.top() < from.bottom() - tolerance)
                    continue;
                gap = r.top() - from.bottom();
                break;
            case Up:
                if (r.bottom() > from.top() + tolerance)
                    continue;
                gap = from.top() - r.bottom();
                break;
            }
            if (direction == Left || direction == Right)
                offset = qMax<qreal>(0, qMax(r.top() - from.bottom(), from.top() - r.bottom()));
            else
                offset = qMax<qreal>(0, qMax(r.left() - from.right(), from.left() - r.right()));
        }
        // Don't jump far sideways (e.g. from the side bar up to the page
        // header on the other side of the window).
        if (hasFocus && offset > 3 * qMax<qreal>(gap, 0) + 160)
            continue;
        // Prefer items in line with the current one; then the closest.
        const qreal centerOffset = (direction == Left || direction == Right)
                ? std::abs(r.center().y() - from.center().y())
                : std::abs(r.center().x() - from.center().x());
        // Stay in the same part of the window when something fits there, and
        // only cross to another part in the direction it lies (the side bar
        // is to the left, the bottom bar below).
        qreal regionPenalty = 0;
        if (hasFocus) {
            const QQuickItem *fromRegion = region(focus, root);
            const QQuickItem *toRegion = region(c.item, root);
            if (fromRegion != toRegion) {
                const QRectF a = windowRect(fromRegion);
                const QRectF b = windowRect(toRegion);
                const bool lies = direction == Left ? b.right() <= a.left() + tolerance
                                : direction == Right ? b.left() >= a.right() - tolerance
                                : direction == Up ? b.bottom() <= a.top() + tolerance
                                                  : b.top() >= a.bottom() - tolerance;
                if (!lies)
                    continue;
                regionPenalty = 2000;
            }
        }
        const qreal score = qMax<qreal>(0, gap) + offset * 4 + centerOffset * 0.1 + regionPenalty;
        if (score < bestScore) {
            bestScore = score;
            best = c.item;
        }
    }
    if (!best)
        return false;
    best->forceActiveFocus(Qt::TabFocusReason);
    ensureVisible(best);
    return true;
}

void SpatialNavigation::ensureVisible(QQuickItem *item)
{
    constexpr qreal margin = 24;
    for (QQuickItem *parent = item->parentItem(); parent; parent = parent->parentItem()) {
        // Areas that the user can't scroll follow something else (e.g. the
        // guide's channel column follows the program grid); leave them.
        if (!isFlickable(parent) || !parent->property("interactive").toBool())
            continue;
        const QRectF r = item->mapRectToItem(parent, QRectF(0, 0, item->width(), item->height()));
        const qreal contentX = parent->property("contentX").toReal();
        const qreal contentY = parent->property("contentY").toReal();
        const qreal maxX = parent->property("contentWidth").toReal() - parent->width()
                + parent->property("originX").toReal();
        const qreal maxY = parent->property("contentHeight").toReal() - parent->height()
                + parent->property("originY").toReal();
        qreal x = contentX;
        qreal y = contentY;
        if (r.top() < margin)
            y += r.top() - margin;
        else if (r.bottom() > parent->height() - margin)
            y += r.bottom() - parent->height() + margin;
        if (r.left() < margin)
            x += r.left() - margin;
        else if (r.right() > parent->width() - margin)
            x += r.right() - parent->width() + margin;
        y = qBound(parent->property("originY").toReal(), y, qMax(maxY, parent->property("originY").toReal()));
        x = qBound(parent->property("originX").toReal(), x, qMax(maxX, parent->property("originX").toReal()));
        if (!qFuzzyCompare(y, contentY))
            parent->setProperty("contentY", y);
        if (!qFuzzyCompare(x, contentX))
            parent->setProperty("contentX", x);
    }
}

bool SpatialNavigation::activate()
{
    QQuickItem *focus = m_window->activeFocusItem();
    for (QQuickItem *item = focus; item; item = item->parentItem()) {
        if (item->inherits("QQuickAbstractButton")) {
            if (!QMetaObject::invokeMethod(item, "click"))
                QMetaObject::invokeMethod(item, "clicked");
            return true;
        }
        if (item->inherits("QQuickPopupItem")) {
            // Enter on a field in a dialog (e.g. a number box): OK.
            QObject *popup = item->parent();
            if (popup && popup->inherits("QQuickDialog"))
                return QMetaObject::invokeMethod(popup, "accept");
            break;
        }
    }
    return false;
}
