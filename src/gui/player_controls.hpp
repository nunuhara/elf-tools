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

#ifndef GELF_PLAYER_CONTROLS_HPP
#define GELF_PLAYER_CONTROLS_HPP

#include <QWidget>
#include <QMediaPlayer>

class QAbstractButton;
class QLabel;
class QMediaPlayer;
class QSlider;

class PlayerControls : public QWidget
{
	Q_OBJECT
public:
	explicit PlayerControls(QMediaPlayer *player, QWidget *parent = nullptr);
	QMediaPlayer::State state() const;
	void setEnabled(bool enabled);
public slots:
	void setState(QMediaPlayer::State state, bool force = false);
	void durationChanged(qint64 duration);
	void positionChanged(qint64 progress);
signals:
	void play();
	void stop();
	void pause();
	void seek(int mseconds);
private:
	QMediaPlayer::State playerState = QMediaPlayer::StoppedState;
	QAbstractButton *playButton = nullptr;
	QAbstractButton *stopButton = nullptr;
	QAbstractButton *pauseButton = nullptr;
	QSlider *slider = nullptr;
	QLabel *durationLabel = nullptr;
	qint64 duration = 0;
};

#endif // GELF_PLAYER_CONTROLS_HPP
