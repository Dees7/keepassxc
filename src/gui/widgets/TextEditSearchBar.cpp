/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
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

#include "TextEditSearchBar.h"
#include "ui_TextEditSearchBar.h"

#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QTextEdit>

#include "gui/Icons.h"

TextEditSearchBar::TextEditSearchBar(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::TextEditSearchBar())
{
    m_ui->setupUi(this);

    m_ui->searchEdit->addAction(icons()->icon("system-search"), QLineEdit::LeadingPosition);
    m_ui->previousButton->setIcon(icons()->icon("move-up"));
    m_ui->nextButton->setIcon(icons()->icon("move-down"));
    m_ui->closeButton->setIcon(icons()->icon("dialog-close"));

    connect(m_ui->searchEdit, &QLineEdit::textChanged, this, &TextEditSearchBar::updateResults);
    connect(m_ui->caseSensitiveButton, &QToolButton::toggled, this, &TextEditSearchBar::updateResults);
    connect(m_ui->wholeWordsButton, &QToolButton::toggled, this, &TextEditSearchBar::updateResults);
    connect(m_ui->nextButton, &QToolButton::clicked, this, &TextEditSearchBar::findNext);
    connect(m_ui->previousButton, &QToolButton::clicked, this, &TextEditSearchBar::findPrevious);
    connect(m_ui->closeButton, &QToolButton::clicked, this, &TextEditSearchBar::hideBar);

    // Enter/Shift+Enter/Escape are handled in eventFilter so we can react to the modifier.
    m_ui->searchEdit->installEventFilter(this);

    hide();
}

TextEditSearchBar::~TextEditSearchBar()
{
}

void TextEditSearchBar::attachTextEdit(QPlainTextEdit* textEdit)
{
    if (!m_textEdit.isNull()) {
        m_textEdit->removeEventFilter(this);
        m_textEdit->disconnect(this);
    }
    m_textEdit = textEdit;
    if (!m_textEdit.isNull()) {
        // Filter the edit so the Find shortcut can open this bar (see eventFilter).
        m_textEdit->installEventFilter(this);
        // Keep highlights in sync when the text changes while the bar is open.
        connect(m_textEdit, &QPlainTextEdit::textChanged, this, &TextEditSearchBar::textEditChanged);
    }
}

void TextEditSearchBar::showBar()
{
    if (m_textEdit.isNull() || !m_textEdit->isVisible()) {
        return;
    }

    // Override the edit's selection colours with a bright pair so the active
    // match stands out (the theme selection colour may be too dim on dark themes).
    if (!m_selectionColorsOverridden) {
        m_savedTextEditPalette = m_textEdit->palette();
        QPalette palette = m_savedTextEditPalette;
        palette.setColor(QPalette::Highlight, QColor(0xff, 0x8f, 0x00)); // orange
        palette.setColor(QPalette::HighlightedText, QColor(0x20, 0x20, 0x20));
        m_textEdit->setPalette(palette);
        m_selectionColorsOverridden = true;
    }

    // Prefill with the current single-line selection in the edit, if any.
    const QString selected = m_textEdit->textCursor().selectedText();
    if (!selected.isEmpty() && !selected.contains(QChar::ParagraphSeparator)) {
        const QSignalBlocker blocker(m_ui->searchEdit);
        m_ui->searchEdit->setText(selected);
    }

    show();
    m_ui->searchEdit->setFocus();
    m_ui->searchEdit->selectAll();
    updateResults();
}

void TextEditSearchBar::hideBar()
{
    const bool wasVisible = isVisible();
    clearHighlights();
    if (!m_textEdit.isNull()) {
        // Drop the selection left over from the last match.
        QTextCursor cursor = m_textEdit->textCursor();
        cursor.clearSelection();
        m_textEdit->setTextCursor(cursor);
        // Restore the original selection colours.
        if (m_selectionColorsOverridden) {
            m_textEdit->setPalette(m_savedTextEditPalette);
            m_selectionColorsOverridden = false;
        }
    }
    hide();
    if (wasVisible && !m_textEdit.isNull()) {
        m_textEdit->setFocus();
    }
}

QTextDocument::FindFlags TextEditSearchBar::findFlags() const
{
    QTextDocument::FindFlags flags;
    if (m_ui->caseSensitiveButton->isChecked()) {
        flags |= QTextDocument::FindCaseSensitively;
    }
    if (m_ui->wholeWordsButton->isChecked()) {
        flags |= QTextDocument::FindWholeWords;
    }
    return flags;
}

void TextEditSearchBar::clearHighlights()
{
    if (!m_textEdit.isNull()) {
        m_textEdit->setExtraSelections({});
    }
}

void TextEditSearchBar::updateHighlights()
{
    if (m_textEdit.isNull()) {
        return;
    }

    clearHighlights();

    const QString text = m_ui->searchEdit->text();
    if (text.isEmpty()) {
        m_ui->matchCountLabel->clear();
        return;
    }

    // Highlight every match in the document. Use a fixed, bright colour pair so
    // matches stay readable on both light and dark themes (the theme's selection
    // colour can be too dim to see, e.g. dark green on grey).
    auto* document = m_textEdit->document();
    QList<QTextEdit::ExtraSelection> selections;
    QTextCharFormat format;
    format.setBackground(QColor(0xff, 0xd5, 0x4f)); // amber
    format.setForeground(QColor(0x20, 0x20, 0x20));

    QTextCursor cursor(document);
    while (true) {
        cursor = document->find(text, cursor, findFlags());
        if (cursor.isNull()) {
            break;
        }
        QTextEdit::ExtraSelection selection;
        selection.cursor = cursor;
        selection.format = format;
        selections.append(selection);
    }
    m_textEdit->setExtraSelections(selections);

    updateMatchCounter();
}

void TextEditSearchBar::updateResults()
{
    if (m_textEdit.isNull()) {
        return;
    }

    updateHighlights();

    const QString text = m_ui->searchEdit->text();
    if (text.isEmpty()) {
        return;
    }

    // Move to the first match at or after the current position (live search).
    // Reset the cursor to the start of the current selection so it is considered
    // a candidate, then search forward, wrapping to the start if needed.
    QTextCursor from = m_textEdit->textCursor();
    from.setPosition(from.selectionStart());
    m_textEdit->setTextCursor(from);
    if (!m_textEdit->find(text, findFlags())) {
        QTextCursor start = m_textEdit->textCursor();
        start.movePosition(QTextCursor::Start);
        m_textEdit->setTextCursor(start);
        m_textEdit->find(text, findFlags());
    }

    updateMatchCounter();
}

void TextEditSearchBar::textEditChanged()
{
    // Refresh stale highlights when the edited text changes, but don't move the
    // cursor (the user is editing, not searching). Only relevant while open.
    if (isVisible()) {
        updateHighlights();
    }
}

void TextEditSearchBar::find(bool backward)
{
    if (m_textEdit.isNull()) {
        return;
    }

    const QString text = m_ui->searchEdit->text();
    if (text.isEmpty()) {
        return;
    }

    QTextDocument::FindFlags flags = findFlags();
    if (backward) {
        flags |= QTextDocument::FindBackward;
    }

    if (!m_textEdit->find(text, flags)) {
        // Wrap around to the other end of the document and retry.
        QTextCursor cursor = m_textEdit->textCursor();
        cursor.movePosition(backward ? QTextCursor::End : QTextCursor::Start);
        m_textEdit->setTextCursor(cursor);
        m_textEdit->find(text, flags);
    }

    updateMatchCounter();
}

void TextEditSearchBar::findNext()
{
    find(false);
}

void TextEditSearchBar::findPrevious()
{
    find(true);
}

void TextEditSearchBar::updateMatchCounter()
{
    const QString text = m_ui->searchEdit->text();
    if (m_textEdit.isNull() || text.isEmpty()) {
        m_ui->matchCountLabel->clear();
        return;
    }

    auto* document = m_textEdit->document();
    const QTextCursor current = m_textEdit->textCursor();
    const bool hasSelection = current.hasSelection();
    const int selectionStart = current.selectionStart();

    int total = 0;
    int index = 0;
    QTextCursor cursor(document);
    while (true) {
        cursor = document->find(text, cursor, findFlags());
        if (cursor.isNull()) {
            break;
        }
        ++total;
        if (hasSelection && cursor.selectionStart() == selectionStart) {
            index = total;
        }
    }

    if (total == 0) {
        m_ui->matchCountLabel->setText(tr("No results"));
    } else if (index == 0) {
        m_ui->matchCountLabel->setText(tr("%1 matches", "Number of search matches", total).arg(total));
    } else {
        m_ui->matchCountLabel->setText(tr("%1 of %2", "Current search match and total count").arg(index).arg(total));
    }
}

bool TextEditSearchBar::eventFilter(QObject* obj, QEvent* event)
{
    // Open the bar when the Find shortcut is pressed inside the attached edit.
    // Claiming the ShortcutOverride makes this take precedence over the
    // window-wide Find shortcut (the database search) while the edit has focus.
    if (obj == m_textEdit) {
        if (event->type() == QEvent::ShortcutOverride) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->matches(QKeySequence::Find)) {
                event->accept();
                return true;
            }
        } else if (event->type() == QEvent::KeyPress) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->matches(QKeySequence::Find)) {
                showBar();
                return true;
            }
        }
        return QWidget::eventFilter(obj, event);
    }

    if (obj == m_ui->searchEdit && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        switch (keyEvent->key()) {
        case Qt::Key_Escape:
            hideBar();
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (keyEvent->modifiers().testFlag(Qt::ShiftModifier)) {
                findPrevious();
            } else {
                findNext();
            }
            return true;
        default:
            break;
        }
    }
    return QWidget::eventFilter(obj, event);
}
