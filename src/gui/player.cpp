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

#include "player.hpp"
#include "player_controls.hpp"

// XXX: player->setMedia with a QIODevice gets stuck on LoadingMedia... apparently this is a
//      limit of the gstreamer integration in Qt5 (supposedly fixed in Qt6?). So we can only
//      play audio from files from disk.
Player::Player(const QString &path, QWidget *parent) : QWidget(parent)
{
	player = new QMediaPlayer(this);
	controls = new PlayerControls(player);
	connect(player, &QMediaPlayer::mediaStatusChanged, this, &Player::statusChanged);

	player->setMedia(QUrl::fromLocalFile(path));
	controls->setState(player->state());

	connect(controls, &PlayerControls::play, player, &QMediaPlayer::play);
	connect(controls, &PlayerControls::pause, player, &QMediaPlayer::pause);
	connect(controls, &PlayerControls::stop, player, &QMediaPlayer::stop);
	connect(controls, &PlayerControls::seek, this, &Player::seek);

	connect(player, &QMediaPlayer::durationChanged, controls, &PlayerControls::durationChanged);
	connect(player, &QMediaPlayer::positionChanged, controls, &PlayerControls::positionChanged);
	connect(player, &QMediaPlayer::stateChanged, controls,
			[this](QMediaPlayer::State arg) {
		controls->setState(arg);
	});

	QBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(20, 0, 20, 0);
	layout->addWidget(controls);
	setLayout(layout);
}

Player::~Player()
{
}

void Player::seek(int mseconds)
{
	player->setPosition(mseconds);
}

void Player::statusChanged(QMediaPlayer::MediaStatus status)
{
	if (status == QMediaPlayer::LoadingMedia) {
		if (controls)
			controls->setEnabled(false);
	} else if (status == QMediaPlayer::LoadedMedia) {
		if (controls)
			controls->setEnabled(true);
	}
}
