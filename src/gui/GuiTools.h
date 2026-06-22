/*
 *  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef KEEPASSXC_GUITOOLS_H
#define KEEPASSXC_GUITOOLS_H

#include <QComboBox>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QList>
#include <QWidget>

class Entry;

namespace GuiTools
{
    bool confirmDeleteEntries(QWidget* parent, const QList<Entry*>& entries, bool permanent);
    bool confirmDeletePluginData(QWidget* parent, const QList<Entry*>& entries);
    size_t deleteEntriesResolveReferences(QWidget* parent, const QList<Entry*>& entries, bool permanent);
} // namespace GuiTools

/**
 * Helper class to ignore mouse wheel events on non-focused widgets
 * NOTE: The widget must NOT have a focus policy of "WHEEL"
 */
class MouseWheelEventFilter : public QObject
{
public:
    explicit MouseWheelEventFilter(QObject* parent)
        : QObject(parent){};

protected:
    bool eventFilter(QObject* obj, QEvent* event) override
    {
        const auto* widget = qobject_cast<QWidget*>(obj);
        if (event->type() == QEvent::Wheel && widget && !widget->hasFocus()) {
            event->ignore();
            return true;
        }
        return QObject::eventFilter(obj, event);
    }
};

/**
 * Helper class to make the Home/End keys move the cursor to the start/end of the
 * line in a QLineEdit. On macOS, Qt maps the bare Home/End keys to "move to
 * start/end of document", which a single-line edit ignores, so the keys appear
 * to do nothing. Holding Shift extends the selection, matching the behaviour on
 * the other platforms.
 */
class LineEditHomeEndEventFilter : public QObject
{
public:
    explicit LineEditHomeEndEventFilter(QObject* parent)
        : QObject(parent){};

protected:
    bool eventFilter(QObject* obj, QEvent* event) override
    {
        if (event->type() == QEvent::KeyPress) {
            // An editable QComboBox keeps the keyboard focus on the combo box itself
            // (its line edit's focus proxy points back to the combo), so the key event
            // is delivered to the QComboBox rather than to its QLineEdit. Resolve the
            // inner line edit in that case.
            auto* lineEdit = qobject_cast<QLineEdit*>(obj);
            if (!lineEdit) {
                if (auto* comboBox = qobject_cast<QComboBox*>(obj)) {
                    lineEdit = comboBox->lineEdit();
                }
            }
            // Skip read-only edits: there is nothing to navigate for editing, and
            // some read-only QLineEdit subclasses (e.g. the auto-type shortcut
            // recorder) repurpose key presses, so we must not swallow them.
            if (lineEdit && !lineEdit->isReadOnly()) {
                auto* keyEvent = static_cast<QKeyEvent*>(event);
                const bool select = keyEvent->modifiers().testFlag(Qt::ShiftModifier);
                // Only translate plain Home/End (optionally with Shift); leave
                // other modifier combinations (e.g. Cmd+Home) untouched.
                const auto otherModifiers =
                    keyEvent->modifiers() & ~Qt::ShiftModifier & ~Qt::KeypadModifier;
                if (otherModifiers == Qt::NoModifier) {
                    if (keyEvent->key() == Qt::Key_Home) {
                        lineEdit->home(select);
                        return true;
                    }
                    if (keyEvent->key() == Qt::Key_End) {
                        lineEdit->end(select);
                        return true;
                    }
                }
            }
        }
        return QObject::eventFilter(obj, event);
    }
};

#endif // KEEPASSXC_GUITOOLS_H
