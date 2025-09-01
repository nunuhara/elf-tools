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

#ifndef GELF_ARCHIVE_DIALOG_HPP
#define GELF_ARCHIVE_DIALOG_HPP

#include <QDialog>

extern "C" {
#include "arc.h"
}

class QCheckBox;
class QComboBox;
struct ai5_game;

class GameDialog : public QDialog
{
	Q_OBJECT
public:
	enum DialogType {
		GAME_SELECT,
		ARCHIVE_OPEN,
		ARCHIVE_EXTRACT
	};
	explicit GameDialog(const QString &path, DialogType type, QWidget *parent = nullptr);
	const struct ai5_game *game = nullptr;
	unsigned flags = 0;
	struct arc_extract_options opt;
public slots:
	void setGame(int i);
private:
	DialogType type;
	QByteArray path;
	QComboBox *gameSelector;
	QCheckBox *compressedCheckBox;
};

#endif // GELF_ARCHIVE_DIALOG_HPP
