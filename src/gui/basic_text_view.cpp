/* Copyright (C) 2025 Nunuhara Cabbage <nunuhara@haniwa.technology>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://gnu.org/licenses/>.
 */

#include <QFont>
#include <QRegularExpression>
#include <QTextCharFormat>
#include "basic_text_view.hpp"

BasicTextView::BasicTextView(QWidget *parent)
	: QTextEdit(parent)
{
	QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	font.setFixedPitch(true);
	font.setPointSize(10);
	setFont(font);

	highlighter = new SyntaxHighlighter(document());

	QTextCharFormat fmt;

	// numbers
	fmt.setForeground(Qt::darkCyan);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\b0x[a-fA-F0-9]+\\b")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\b[1-9][0-9]*\\b")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\b0[0-7]*\\b")), fmt);
	highlighter->addRule(QRegularExpression(QStringLiteral("\\b[0-9]+\\.[0-9]+\\b")), fmt);

	// strings
	fmt.setForeground(Qt::red);
	highlighter->addRule(QRegularExpression(QStringLiteral("\"(\\\\.|[^\"\\\\])*\"")), fmt);

	setReadOnly(true);
}

BasicTextView::BasicTextView(const QString &text, QWidget *parent)
	: BasicTextView(parent)
{
	setPlainText(text);
}
