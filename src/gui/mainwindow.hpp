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

#ifndef GELF_MAINWINDOW_HPP
#define GELF_MAINWINDOW_HPP

#include <QMainWindow>
#include <QFileInfo>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QTabWidget>
#include "gelf.hpp"

class Navigator;

class MainWindow : public QMainWindow
{
	Q_OBJECT
public:
	MainWindow(QWidget *parent = nullptr);
protected:
	void closeEvent(QCloseEvent *event) override;
public slots:
	void openTextFile(const QString &name, char *text, FileFormat format, bool newTab);
	void openImage(const QString &name, std::shared_ptr<struct cg> cg, bool newTab);
	void openGif(const QString &name, const uint8_t *data, size_t size, bool newTab);
	void openMedia(const QString &name, const uint8_t *data, size_t size,
			FileFormat format, bool newTab);
private slots:
	void open();
	void about();
	void error(const QString &message);
	void status(const QString &message);
	void closeTab(int index);
private:
	void createActions();
	void createStatusBar();
	void createDockWindows();
	void readSettings();
	void writeSettings();
	void setupViewer();

	void openText(const QString &label, const QString &text, bool newTab);
	void openViewer(const QString &label, QWidget *view, bool newTab);

	QMenu *viewMenu;

	QTabWidget *tabWidget;
	Navigator *nav;
};

#endif // GELF_MAINWINDOW_HPP
