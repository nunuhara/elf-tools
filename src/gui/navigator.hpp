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

#ifndef GELF_NAVIGATOR_HPP
#define GELF_NAVIGATOR_HPP

#include <QDockWidget>

class QComboBox;
class QStackedWidget;

struct archive;
struct ai5_game;
class MainWindow;

class Navigator : public QDockWidget
{
	Q_OBJECT
public:
	Navigator(MainWindow *parent = nullptr);
	~Navigator();
private slots:
	void addArchive(const QString &fileName, std::shared_ptr<struct archive> ar,
			const struct ai5_game *game, unsigned flags);
private:
	void addFilesystem();
	void addFile(const QString &name, const char *description, QWidget *widget);

	MainWindow *window;
	QComboBox *fileSelector;
	QStackedWidget *stack;
};

#endif // GELF_NAVIGATOR_HPP
