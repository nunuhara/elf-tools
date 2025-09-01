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

#ifndef GELF_PLAYER_HPP
#define GELF_PLAYER_HPP

#include <QWidget>
#include <QMediaPlayer>

class QIODevice;
class PlayerControls;

class Player : public QWidget
{
	Q_OBJECT
public:
	explicit Player(const QString &path, QWidget *parent = nullptr);
	~Player();
private slots:
	void seek(int mseconds);
	void statusChanged(QMediaPlayer::MediaStatus status);
private:
	QMediaPlayer *player = nullptr;
	QIODevice *stream = nullptr;
	PlayerControls *controls = nullptr;
};

#endif // GELF_PLAYER_HPP
