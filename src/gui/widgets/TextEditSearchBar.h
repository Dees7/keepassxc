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

#ifndef KEEPASSXC_TEXTEDITSEARCHBAR_H
#define KEEPASSXC_TEXTEDITSEARCHBAR_H

#include <QPointer>
#include <QScopedPointer>
#include <QTextDocument>
#include <QWidget>

class QPlainTextEdit;

namespace Ui
{
    class TextEditSearchBar;
}

/**
 * An inline find bar that searches the text of an attached QPlainTextEdit.
 * Highlights all matches, shows a match counter and supports case sensitive and
 * whole-word matching. Hidden by default; show it with showBar() (e.g. bound to
 * the Find shortcut on the target edit).
 */
class TextEditSearchBar : public QWidget
{
    Q_OBJECT

public:
    explicit TextEditSearchBar(QWidget* parent = nullptr);
    ~TextEditSearchBar() override;

    void attachTextEdit(QPlainTextEdit* textEdit);

public slots:
    void showBar();
    void hideBar();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void updateResults();
    void findNext();
    void findPrevious();
    void textEditChanged();

private:
    QTextDocument::FindFlags findFlags() const;
    void find(bool backward);
    void updateHighlights();
    void clearHighlights();
    void updateMatchCounter();

    QScopedPointer<Ui::TextEditSearchBar> m_ui;
    QPointer<QPlainTextEdit> m_textEdit;
};

#endif // KEEPASSXC_TEXTEDITSEARCHBAR_H
