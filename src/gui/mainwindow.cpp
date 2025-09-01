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

#include <iostream>
#include <QtWidgets>
#include "basic_text_view.hpp"
#include "gelf.hpp"
#include "mainwindow.hpp"
#include "navigator.hpp"
#include "player.hpp"
#include "smes_view.hpp"
#include "viewer.hpp"
#include "version.h"

extern "C" {
#include "ai5/cg.h"
}

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
{
	createActions();
	createStatusBar();
	createDockWindows();

	readSettings();

	setupViewer();
	setCentralWidget(tabWidget);

	setUnifiedTitleAndToolBarOnMac(true);

	connect(&GElf::getInstance(), &GElf::errorMessage, this, &MainWindow::error);
	connect(&GElf::getInstance(), &GElf::statusMessage, this, &MainWindow::status);
	connect(&GElf::getInstance(), &GElf::openedImageFile, this, &MainWindow::openImage);
	connect(&GElf::getInstance(), &GElf::openedGifFile, this, &MainWindow::openGif);
	connect(&GElf::getInstance(), &GElf::openedMedia, this, &MainWindow::openMedia);
	connect(&GElf::getInstance(), &GElf::openedText, this, &MainWindow::openTextFile);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	event->accept();
}

void MainWindow::open()
{
	QString fileName = QFileDialog::getOpenFileName(this);
	if (!fileName.isEmpty())
		GElf::openFile(fileName);
}

void MainWindow::about()
{
	QMessageBox::about(this, tr("About alice-tools"),
			"elf-tools version " ELF_TOOLS_VERSION);
}

void MainWindow::createActions()
{
	QMenu *fileMenu = menuBar()->addMenu(tr("&File"));

	const QIcon openIcon = QIcon::fromTheme("document-open");
	QAction *openAct = new QAction(openIcon, tr("&Open..."), this);
	openAct->setShortcuts(QKeySequence::Open);
	openAct->setStatusTip(tr("Open an existing file"));
	connect(openAct, &QAction::triggered, this, &MainWindow::open);
	fileMenu->addAction(openAct);

	fileMenu->addSeparator();

	const QIcon exitIcon = QIcon::fromTheme("application-exit");
	QAction *exitAct = new QAction(exitIcon, tr("E&xit"), this);
	exitAct->setShortcuts(QKeySequence::Quit);
	exitAct->setStatusTip(tr("Exit the application"));
	connect(exitAct, &QAction::triggered, this, &QWidget::close);
	fileMenu->addAction(exitAct);

	viewMenu = menuBar()->addMenu(tr("&View"));

	menuBar()->addSeparator();

	QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));

	const QIcon aboutIcon = QIcon::fromTheme("help-about");
	QAction *aboutAct = new QAction(aboutIcon, tr("&About"), this);
	aboutAct->setStatusTip(tr("About alice-tools"));
	connect(aboutAct, &QAction::triggered, this, &MainWindow::about);
	helpMenu->addAction(aboutAct);
}

void MainWindow::createStatusBar()
{
	status(tr("Ready"));
}

void MainWindow::createDockWindows()
{
	nav = new Navigator(this);
	nav->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	addDockWidget(Qt::LeftDockWidgetArea, nav);
	resizeDocks({nav}, {350}, Qt::Horizontal);
	viewMenu->addAction(nav->toggleViewAction());
}

void MainWindow::setupViewer()
{
	tabWidget = new QTabWidget;
	tabWidget->setMovable(true);
	tabWidget->setTabsClosable(true);

	connect(tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
	// TODO: display welcome page
}

void MainWindow::closeTab(int index)
{
	QWidget *w = tabWidget->widget(index);
	tabWidget->removeTab(index);
	delete w;
}

void MainWindow::readSettings()
{
	QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
	const QByteArray geometry = settings.value("geometry", QByteArray()).toByteArray();
	if (geometry.isEmpty()) {
		const QRect availableGeometry = screen()->availableGeometry();
		resize(availableGeometry.width() / 3, availableGeometry.height() / 2);
		move((availableGeometry.width() - width()) / 2,
			(availableGeometry.height() - height()) / 2);
	} else {
		restoreGeometry(geometry);
	}
}

void MainWindow::writeSettings()
{
	QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
	settings.setValue("geometry", saveGeometry());
}

void MainWindow::error(const QString &message)
{
	QMessageBox::critical(this, "elf-tools", message, QMessageBox::Ok);
}

void MainWindow::status(const QString &message)
{
	statusBar()->showMessage(message);
}

void MainWindow::openTextFile(const QString &name, char *text, FileFormat format, bool newTab)
{
	switch (format) {
	case FileFormat::SMES:
		openViewer(name, new SmesView(text), newTab);
		break;
	case FileFormat::SA:
	case FileFormat::TXT:
	case FileFormat::CSV:
		openViewer(name, new BasicTextView(text), newTab);
		break;
	default:
		openText(name, text, newTab);
		break;
	}
}

void MainWindow::openImage(const QString &name, std::shared_ptr<struct cg> cg, bool newTab)
{
	QImage image((uchar*)cg->pixels, cg->metrics.w, cg->metrics.h,
			cg->metrics.w*4, QImage::Format_RGBA8888);

	QLabel *imageLabel = new QLabel;
	imageLabel->setPixmap(QPixmap::fromImage(image));

	QScrollArea *scrollArea = new QScrollArea;
	scrollArea->setWidget(imageLabel);
	scrollArea->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

	openViewer(name, scrollArea, newTab);
}

void MainWindow::openGif(const QString &name, const uint8_t *data, size_t size, bool newTab)
{
	QByteArray *gif_bytes = new QByteArray((const char*)data, size);
	QBuffer *buffer = new QBuffer(gif_bytes);
	QMovie *movie = new QMovie(buffer);
	QLabel *label = new QLabel;
	label->setMovie(movie);
	label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

	connect(label, &QObject::destroyed, [movie, buffer, gif_bytes](QObject *obj) {
		delete movie;
		delete buffer;
		delete gif_bytes;
	});

	openViewer(name, label, newTab);
	movie->start();
}

void MainWindow::openMedia(const QString &name, const uint8_t *data, size_t size,
		FileFormat format, bool newTab)
{
	QByteArray *bytes = new QByteArray((const char*)data, size);
	QBuffer *buffer = new QBuffer(bytes);
	buffer->open(QIODevice::ReadOnly);
	buffer->seek(0);
	Player *player = new Player(name, buffer);
	connect(player, &QObject::destroyed, [bytes, buffer](QObject *obj) {
		delete buffer;
		delete bytes;
	});
	openViewer(name, player, newTab);
}

void MainWindow::openText(const QString &label, const QString &text, bool newTab)
{
	QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	font.setPointSize(10);

	QTextEdit *viewer = new QTextEdit;
	viewer->setFont(font);
	viewer->setPlainText(text);
	viewer->setReadOnly(true);

	openViewer(label, viewer, newTab);
}

void MainWindow::openViewer(const QString &label, QWidget *view, bool newTab)
{
	if (newTab || tabWidget->currentIndex() < 0) {
		Viewer *tabContent = new Viewer(view);
		int index = tabWidget->currentIndex()+1;
		tabWidget->insertTab(index, tabContent, label);
		tabWidget->setCurrentIndex(index);
		return;
	}

	Viewer *tabContent = static_cast<Viewer*>(tabWidget->currentWidget());
	tabContent->setChild(view);
	tabWidget->setTabText(tabWidget->currentIndex(), label);
}
