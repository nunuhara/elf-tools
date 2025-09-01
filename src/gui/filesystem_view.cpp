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

#include <QContextMenuEvent>
#include <QFileDialog>
#include <QGuiApplication>
#include <QMenu>
#include <QMessageBox>

#include "game_dialog.hpp"
#include "gelf.hpp"
#include "filesystem_view.hpp"

extern "C" {
#include "nulib/file.h"
#include "nulib/port.h"
#include "ai5/arc.h"
#include "arc.h"
}

FileSystemView::FileSystemView(QFileSystemModel *model, QWidget *parent)
	: QTreeView(parent)
	, model(model)
{
	setModel(model);
	connect(this, &QTreeView::activated, this, &FileSystemView::openFile);
}

FileSystemView::~FileSystemView()
{
	delete model;
}

void FileSystemView::open(const QModelIndex &index, bool newTab)
{
	if (model->isDir(index))
		return;
	GElf::openFile(model->filePath(index), newTab);
}

void FileSystemView::openFile(const QModelIndex &index)
{
	open(index, false);
}

void FileSystemView::extract(const QModelIndex &index)
{
	GameDialog dialog(model->filePath(index), GameDialog::DialogType::ARCHIVE_EXTRACT);
	if (dialog.exec() != QDialog::Accepted)
		return;
	ai5_set_game(dialog.game->name);

	QByteArray u = model->filePath(index).toUtf8();

	QGuiApplication::setOverrideCursor(Qt::WaitCursor);
	struct archive *ar = archive_open(u, dialog.flags);
	QGuiApplication::restoreOverrideCursor();

	if (!ar) {
		QMessageBox::critical(this, "elf-tools", tr("Failed to load archive"),
				QMessageBox::Ok);
		return;
	}

	QString dir = QFileDialog::getExistingDirectory(this, tr("Select output directory"), "",
			QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	if (!dir.isEmpty()) {
		QGuiApplication::setOverrideCursor(Qt::WaitCursor);
		QByteArray u = dir.toUtf8();
		arc_extract_all(ar, u, &dialog.opt);
		QGuiApplication::restoreOverrideCursor();
	}

	archive_close(ar);
}

void FileSystemView::convert(const QModelIndex &index, FileFormat format)
{
	QVector<FileFormat> supportedFormats = GElf::getSupportedConversionFormats(format);
	QString filename = getSaveFileName(this, tr("Convert Image"), supportedFormats);
	if (filename.isEmpty())
		return;

	QString ext = QFileInfo(filename).suffix();
	FileFormat to = extensionToFileFormat(ext);
	if (!supportedFormats.contains(to)) {
		GElf::error(QString("%1 is not a supported conversion format for this file type").arg(ext));
		return;
	}

	struct port port;
	QByteArray u = filename.toUtf8();
	if (!port_file_open(&port, u)) {
		GElf::error(QString("Failed to open file '%1'").arg(filename));
		return;
	}

	size_t size;
	uint8_t *data = static_cast<uint8_t*>(file_read(model->filePath(index).toUtf8(), &size));
	if (!data) {
		GElf::error(QString("Failed to read file '%1'").arg(model->filePath(index)));
		return;
	}
	QByteArray src_name = model->filePath(index).toUtf8();
	GElf::convertFormat(&port, src_name, data, size, format, to, &ai5_games[0]);
	port_close(&port);
}

void FileSystemView::contextMenuEvent(QContextMenuEvent *event)
{
	// get selected index
	QModelIndexList selected = selectedIndexes();
	QModelIndex index = selected.isEmpty() ? QModelIndex() : selected.first();
	if (!index.isValid())
		return;

	FileFormat format = extensionToFileFormat(model->fileInfo(index).suffix());
	if (format == FileFormat::NONE)
		return;

	QMenu menu(this);
	menu.addAction(tr("Open"), [this, index]() -> void { this->open(index, false); });
	if (isImageFormat(format) || format == FileFormat::ARC) {
		menu.addAction(tr("Open in New Tab"), [this, index]() -> void { this->open(index, true); });
	}
	if (isArchiveFormat(format)) {
		menu.addAction(tr("Extract"), [this, index]() -> void { this->extract(index); });
	}
	if (isImageFormat(format)) {
		menu.addAction(tr("Convert"), [this, index, format]() -> void { this->convert(index, format); });
	}

	menu.exec(event->globalPos());
}
