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

#include "player_controls.hpp"

PlayerControls::PlayerControls(QMediaPlayer *player, QWidget *parent) : QWidget(parent)
{
	duration = player->duration() / 1000;

	playButton = new QToolButton(this);
	playButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
	connect(playButton, &QAbstractButton::clicked, this, &PlayerControls::play);

	pauseButton = new QToolButton(this);
	pauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
	connect(pauseButton, &QAbstractButton::clicked, this, &PlayerControls::pause);

	stopButton = new QToolButton(this);
	stopButton->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
	connect(stopButton, &QAbstractButton::clicked, this, &PlayerControls::stop);

	slider = new QSlider(Qt::Horizontal, this);
	slider->setRange(0, player->duration());
	connect(slider, &QSlider::sliderMoved, this, &PlayerControls::seek);

	durationLabel = new QLabel();
	durationLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

	setState(QMediaPlayer::StoppedState, true);

	QBoxLayout *layout = new QHBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(playButton);
	layout->addWidget(pauseButton);
	layout->addWidget(stopButton);
	layout->addWidget(slider);
	layout->addWidget(durationLabel);
	setLayout(layout);
}

QMediaPlayer::State PlayerControls::state() const
{
	return playerState;
}

void PlayerControls::setEnabled(bool enabled)
{
	playButton->setEnabled(enabled);
	pauseButton->setEnabled(enabled);
	stopButton->setEnabled(enabled);
	slider->setEnabled(enabled);
}

void PlayerControls::setState(QMediaPlayer::State state, bool force)
{
	if (state == playerState && !force)
		return;

	playerState = state;
	QColor baseColor = palette().color(QPalette::Mid);
	QString inactiveStyleSheet = QStringLiteral("background-color: %1").arg(baseColor.name());
	QString defaultStyleSheet = QStringLiteral("");

	switch (state) {
	case QMediaPlayer::StoppedState:
		stopButton->setStyleSheet(inactiveStyleSheet);
		playButton->setStyleSheet(defaultStyleSheet);
		pauseButton->setStyleSheet(defaultStyleSheet);
		break;
	case QMediaPlayer::PlayingState:
		stopButton->setStyleSheet(defaultStyleSheet);
		playButton->setStyleSheet(inactiveStyleSheet);
		pauseButton->setStyleSheet(defaultStyleSheet);
		break;
	case QMediaPlayer::PausedState:
		stopButton->setStyleSheet(defaultStyleSheet);
		playButton->setStyleSheet(defaultStyleSheet);
		pauseButton->setStyleSheet(inactiveStyleSheet);
		break;
	}
}

void PlayerControls::durationChanged(qint64 dur)
{
	duration = dur / 1000;
	slider->setMaximum(dur);
}

void PlayerControls::positionChanged(qint64 progress)
{
	if (!slider->isSliderDown())
		slider->setValue(progress);

	QString str;
	progress /= 1000;
	QTime currentTime((progress / 3600) % 60, (progress / 60) % 60, progress % 60,
			(progress * 1000) % 1000);
	QTime totalTime((duration / 3600) % 60, (duration / 60) % 50, duration % 60,
			(duration * 1000) % 1000);
	QString format = "mm:ss";
	if (duration > 3600)
		format = "hh:mm:ss";
	str = currentTime.toString(format) + " / " + totalTime.toString(format);
	durationLabel->setText(str);
}
