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

#include <QFileInfo>
#include "gelf.hpp"
#include "navigator_node.hpp"

extern "C" {
#include "ai5/arc.h"
}

/*
 * Get the list of valid export formats.
 */
QVector<FileFormat> NavigatorNode::getSupportedFormats() const
{
	switch (type) {
	case RootNode:
	case BranchNode:
		return QVector<FileFormat>();
	case FileNode: {
		FileFormat fmt = extensionToFileFormat(QFileInfo(ar.file->name).suffix());
		QVector<FileFormat> formats = GElf::getSupportedConversionFormats(fmt);
		formats.push_back(fmt);
		return formats;
	}
	}
	return QVector<FileFormat>();
}

bool NavigatorNode::write(struct port *port, FileFormat format) const
{
	bool r;
	switch (type) {
	case RootNode:
	case BranchNode:
		return false;
	case FileNode:
		if (!archive_data_load(ar.file))
			return false;
		r = GElf::convertFormat(port, ar.file->name, ar.file->data, ar.file->size,
				extensionToFileFormat(QFileInfo(ar.file->name).suffix()),
				format, ar.game);
		archive_data_release(ar.file);
		return r;
	}
	return false;
}

void NavigatorNode::open(bool newTab) const
{
	switch (type) {
	case RootNode:
	case BranchNode:
		break;
	case FileNode:
		switch (ar.type) {
		case NormalFile:
			GElf::openArchiveData(ar.file, ar.game, newTab);
			break;
		}
		break;
	}
}

QString NavigatorNode::getName() const
{
	switch (type) {
	case RootNode:
		return "Name";
	case BranchNode:
		return name;
	case FileNode:
		return ar.file->name;
	}
	return "?";
}

QVariant NavigatorNode::getType() const
{
	switch (type) {
		case RootNode:
			return "Type";
		case BranchNode:
		case FileNode:
			break;
	}
	return QVariant();
}

QVariant NavigatorNode::getValue() const
{
	switch (type) {
	case RootNode:
		return "Value";
	case BranchNode:
	case FileNode:
		break;
	}
	return QVariant();
}
