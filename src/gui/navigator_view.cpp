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

#include <QMenu>
#include <QContextMenuEvent>
#include <QFileDialog>
#include <QMessageBox>
#include "gelf.hpp"
#include "navigator_view.hpp"

extern "C" {
#include "nulib/port.h"
#include "nulib/string.h"
}

NavigatorView::NavigatorView(NavigatorModel *model, QWidget *parent)
	: QTreeView(parent)
	, model(model)
{
	setModel(model);
	connect(this, &QTreeView::activated, this, &NavigatorView::requestOpen);
}

NavigatorView::~NavigatorView()
{
	delete model;
}

static NavigatorNode *getNode(const QModelIndex &index)
{
	return static_cast<const NavigatorModel*>(index.model())->getNode(index);
}

void NavigatorView::requestOpen(const QModelIndex &index) const
{
	if (!index.isValid())
		return;

	NavigatorNode *node = getNode(index);
	if (!node)
		return;

	node->open(false);
}

void NavigatorView::exportNode(NavigatorNode *node)
{
	QVector<FileFormat> supportedFormats = node->getSupportedFormats();
	QString filename = getSaveFileName(this, tr("Export File"), supportedFormats);
	if (filename.isEmpty())
		return;

	QString ext = QFileInfo(filename).suffix();
	FileFormat format = extensionToFileFormat(ext);
	if (!supportedFormats.contains(format)) {
		QMessageBox::critical(this, "elf-tools",
				QString("%1 is not a supported export format for this file type").arg(ext),
				QMessageBox::Ok);
		return;
	}

	struct port port;
	QByteArray u = filename.toUtf8();
	if (!port_file_open(&port, u)) {
		QMessageBox::critical(this, "elf-tools",
				QString("Failed to open file '%1'").arg(filename),
				QMessageBox::Ok);
		return;
	}

	if (!node->write(&port, format)) {
		QMessageBox::critical(this, "elf-tools",
				QString("Failed to write to file '%1'").arg(filename),
				QMessageBox::Ok);
	}

	port_close(&port);
}

void NavigatorView::contextMenuEvent(QContextMenuEvent *event)
{
	// get selected index
	QModelIndexList selected = selectedIndexes();
	QModelIndex index = selected.isEmpty() ? QModelIndex() : selected.first();
	if (!index.isValid())
		return;

	NavigatorNode *node = getNode(index);
	if (!node)
		return;

	QMenu menu(this);
	menu.addAction(tr("Open"), [node]() -> void { node->open(false); });
	menu.addAction(tr("Open in New Tab"), [node]() -> void { node->open(true); });
	menu.addAction(tr("Export"), [this, node]() -> void { this->exportNode(node); });
	menu.exec(event->globalPos());
}
