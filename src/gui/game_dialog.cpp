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

#include "game_dialog.hpp"

extern "C" {
#include "ai5/game.h"
#include "arc.h"
}

static int last_selected_index = 0;

GameDialog::GameDialog(const QString &arc_path, DialogType type, QWidget *parent)
	: QDialog(parent), type(type)
{
	opt = ARC_EXTRACT_DEFAULT;
	path = arc_path.toUtf8();
	flags = ARCHIVE_RAW;
	setMinimumWidth(400);

	gameSelector = new QComboBox;

	for (int i = 0; i < AI5_NR_GAME_IDS; i++) {
		gameSelector->addItem(QString("%1 [%2]").arg(ai5_games[i].description, ai5_games[i].name));
	}

	QFormLayout *formLayout = new QFormLayout;
	formLayout->addRow(tr("Game:"), gameSelector);

	if (type == DialogType::ARCHIVE_OPEN || type == DialogType::ARCHIVE_EXTRACT) {
		compressedCheckBox = new QCheckBox(tr("Compressed"));
		connect(compressedCheckBox, &QCheckBox::stateChanged, [this](int state) {
			if (state == Qt::Checked)
				flags &= ~ARCHIVE_RAW;
			else
				flags |= ARCHIVE_RAW;
		});
		formLayout->addRow(compressedCheckBox);
	}

	if (type == DialogType::ARCHIVE_EXTRACT) {
		QCheckBox *rawCheckBox = new QCheckBox(tr("Raw"));

		QGroupBox *mesTypeBox = new QGroupBox(tr("MES Output Mode"));
		QRadioButton *mesStructuredButton = new QRadioButton(tr("Structured"));
		QRadioButton *mesFlatButton = new QRadioButton(tr("Flat"));
		QRadioButton *mesTextButton = new QRadioButton(tr("Text"));
		mesStructuredButton->setChecked(true);

		connect(rawCheckBox, &QAbstractButton::clicked, [this](bool checked) {
			opt.raw = checked;
		});
		connect(mesStructuredButton, &QAbstractButton::clicked, [this](bool checked) {
			if (checked) {
				opt.mes_flat = false;
				opt.mes_text = false;
			}
		});
		connect(mesFlatButton, &QAbstractButton::clicked, [this](bool checked) {
			if (checked) {
				opt.mes_flat = true;
				opt.mes_text = false;
			}
		});
		connect(mesTextButton, &QAbstractButton::clicked, [this](bool checked) {
			if (checked) {
				opt.mes_flat = false;
				opt.mes_text = true;
			}
		});

		QVBoxLayout *layout = new QVBoxLayout;
		layout->addWidget(mesStructuredButton);
		layout->addWidget(mesFlatButton);
		layout->addWidget(mesTextButton);
		mesTypeBox->setLayout(layout);

		formLayout->addRow(rawCheckBox);
		formLayout->addRow(mesTypeBox);
	}

	QWidget *form = new QWidget;
	form->setLayout(formLayout);

	QDialogButtonBox *buttonBox = new QDialogButtonBox;
	QPushButton *open_button = buttonBox->addButton("Open", QDialogButtonBox::ActionRole);
	connect(open_button, &QAbstractButton::clicked, [this](bool checked) {
		game = &ai5_games[gameSelector->currentIndex()];
		accept();
	});
	QPushButton *cancel_button = buttonBox->addButton("Cancel", QDialogButtonBox::ActionRole);
	connect(cancel_button, &QAbstractButton::clicked, [this](bool checked) {
		reject();
	});

	QVBoxLayout *layout = new QVBoxLayout;
	layout->addWidget(form);
	layout->addWidget(buttonBox);
	setLayout(layout);

	connect(gameSelector, QOverload<int>::of(&QComboBox::activated),
			this, &GameDialog::setGame);
	gameSelector->setCurrentIndex(last_selected_index);
	setGame(last_selected_index);

	setWindowTitle(QString("Open Archive: %1").arg(QFileInfo(path).fileName()));
}

void GameDialog::setGame(int i)
{
	if (type == DialogType::ARCHIVE_OPEN || type == DialogType::ARCHIVE_EXTRACT) {
		if (arc_is_compressed(path, ai5_games[i].id)) {
			compressedCheckBox->setCheckState(Qt::Checked);
		} else {
			compressedCheckBox->setCheckState(Qt::Unchecked);
		}
	
		if (ai5_games[i].id == GAME_KAWARAZAKIKE)
			flags |= ARCHIVE_STEREO;
		else
			flags &= ~ARCHIVE_STEREO;
	}
	last_selected_index = i;
}
