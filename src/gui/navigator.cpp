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

#include <QtWidgets>
#include "filesystem_view.hpp"
#include "gelf.hpp"
#include "mainwindow.hpp"
#include "navigator.hpp"
#include "navigator_model.hpp"
#include "navigator_view.hpp"

extern "C" {
#include "ai5/game.h"
}

Navigator::Navigator(MainWindow *parent)
	: QDockWidget(tr("Navigation"), parent)
	, window(parent)
{
	fileSelector = new QComboBox;
	stack = new QStackedWidget;

	connect(fileSelector, QOverload<int>::of(&QComboBox::activated),
		stack, &QStackedWidget::setCurrentIndex);

	QWidget *widget = new QWidget(this);
	QVBoxLayout *layout = new QVBoxLayout(widget);
	layout->addWidget(fileSelector);
	layout->addWidget(stack);
	layout->setContentsMargins(0, 0, 0, 0);
	setWidget(widget);

	addFilesystem();

	connect(&GElf::getInstance(), &GElf::openedArchive, this, &Navigator::addArchive);
}

Navigator::~Navigator()
{
}

void Navigator::addFilesystem()
{
	QFileSystemModel *model = new QFileSystemModel;
	model->setRootPath(QDir::rootPath());

	FileSystemView *tree = new FileSystemView(model);
	tree->setRootIndex(model->index(QDir::rootPath()));

	QModelIndex index = model->index(QDir::currentPath());
	while (index.isValid()) {
		tree->setExpanded(index, true);
		index = index.parent();
	}

	for (int i = 1; i < model->columnCount(); i++) {
		tree->setColumnHidden(i, true);
	}
	tree->setHeaderHidden(true);

	addFile(tr("Filesystem"), NULL, tree);
}

void Navigator::addArchive(const QString &fileName, std::shared_ptr<struct archive> ar,
		const struct ai5_game *game, unsigned flags)
{
	QWidget *widget = new QWidget;
	NavigatorModel *model = NavigatorModel::fromArchive(ar, game);
	NavigatorView *view = new NavigatorView(model);
	view->setColumnWidth(0, 500);

	QVBoxLayout *layout = new QVBoxLayout(widget);
	layout->addWidget(view);
	layout->setContentsMargins(0, 0, 0, 0);
	addFile(fileName, game->name, widget);
}

void Navigator::addFile(const QString &name, const char *description, QWidget *widget)
{
	int index = fileSelector->count();
	if (description)
		fileSelector->addItem(QString("%1 [%2]").arg(QFileInfo(name).fileName(), description));
	else
		fileSelector->addItem(QFileInfo(name).fileName());
	stack->addWidget(widget);

	fileSelector->setCurrentIndex(index);
	stack->setCurrentIndex(index);
}
