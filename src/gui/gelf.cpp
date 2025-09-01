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

#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#include <QFileDialog>

#include "game_dialog.hpp"
#include "gelf.hpp"
#include "mainwindow.hpp"
#include "version.h"

extern "C" {
#include "nulib/buffer.h"
#include "nulib/port.h"
#include "nulib/string.h"
#include "nulib/file.h"
#include "ai5/anim.h"
#include "ai5/arc.h"
#include "ai5/ccd.h"
#include "ai5/cg.h"
#include "ai5/game.h"
#include "a6.h"
#include "map.h"
#include "mes.h"
#include "mdd.h"
}

int main(int argc, char *argv[])
{
	ai5_set_game("isaku");
	QApplication app(argc, argv);
	QCoreApplication::setOrganizationName("nunuhara");
	QCoreApplication::setApplicationName("elf-tools");
	QCoreApplication::setApplicationVersion(ELF_TOOLS_VERSION);

	QCommandLineParser parser;
	parser.setApplicationDescription(QCoreApplication::applicationName());
	parser.addHelpOption();
	parser.addVersionOption();
	parser.addPositionalArgument("file", "The file to open.");
	parser.process(app);

	MainWindow w;
	w.setWindowTitle("elf-tools");
	if (!parser.positionalArguments().isEmpty())
		GElf::openFile(parser.positionalArguments().first());
	w.show();
	return app.exec();
}

FileFormat extensionToFileFormat(QString extension)
{
	if (!extension.compare("arc", Qt::CaseInsensitive))
		return FileFormat::ARC;
	if (!extension.compare("awd", Qt::CaseInsensitive))
		return FileFormat::AWD;
	if (!extension.compare("awf", Qt::CaseInsensitive))
		return FileFormat::AWF;
	if (!extension.compare("mes", Qt::CaseInsensitive))
		return FileFormat::MES;
	if (!extension.compare("smes", Qt::CaseInsensitive))
		return FileFormat::SMES;
	if (!extension.compare("akb", Qt::CaseInsensitive))
		return FileFormat::AKB;
	if (!extension.compare("GP4", Qt::CaseInsensitive))
		return FileFormat::GP4;
	if (!extension.compare("gp8", Qt::CaseInsensitive))
		return FileFormat::GP8;
	if (!extension.compare("g16", Qt::CaseInsensitive))
		return FileFormat::G16;
	if (!extension.compare("g24", Qt::CaseInsensitive))
		return FileFormat::G24;
	if (!extension.compare("g32", Qt::CaseInsensitive))
		return FileFormat::G32;
	if (!extension.compare("gcc", Qt::CaseInsensitive))
		return FileFormat::GCC;
	if (!extension.compare("gpr", Qt::CaseInsensitive))
		return FileFormat::GPR;
	if (!extension.compare("gpx", Qt::CaseInsensitive))
		return FileFormat::GPX;
	if (!extension.compare("png", Qt::CaseInsensitive))
		return FileFormat::PNG;
	if (!extension.compare("gif", Qt::CaseInsensitive))
		return FileFormat::GIF;
	if (!extension.compare("mdd", Qt::CaseInsensitive))
		return FileFormat::MDD;
	if (!extension.compare("a", Qt::CaseInsensitive))
		return FileFormat::A;
	if (!extension.compare("s4", Qt::CaseInsensitive))
		return FileFormat::S4;
	if (!extension.compare("sa", Qt::CaseInsensitive))
		return FileFormat::SA;
	if (!extension.compare("a6", Qt::CaseInsensitive))
		return FileFormat::A6;
	if (!extension.compare("ccd", Qt::CaseInsensitive))
		return FileFormat::CCD;
	if (!extension.compare("eve", Qt::CaseInsensitive))
		return FileFormat::EVE;
	if (!extension.compare("mp3", Qt::CaseInsensitive))
		return FileFormat::MP3;
	if (!extension.compare("wav", Qt::CaseInsensitive))
		return FileFormat::WAV;
	if (!extension.compare("ogg", Qt::CaseInsensitive))
		return FileFormat::OGG;
	if (!extension.compare("txt", Qt::CaseInsensitive))
		return FileFormat::TXT;
	if (!extension.compare("csv", Qt::CaseInsensitive))
		return FileFormat::CSV;
	return FileFormat::NONE;
}

QString fileFormatToExtension(FileFormat format)
{
	switch (format) {
	case FileFormat::NONE:
		return "";
	case FileFormat::ARC:
		return "ARC";
	case FileFormat::AWD:
		return "AWD";
	case FileFormat::AWF:
		return "AWF";
	case FileFormat::MES:
		return "MES";
	case FileFormat::SMES:
		return "SMES";
	case FileFormat::AKB:
		return "AKB";
	case FileFormat::GP4:
		return "GP4";
	case FileFormat::GP8:
		return "GP8";
	case FileFormat::G16:
		return "G16";
	case FileFormat::G24:
		return "G24";
	case FileFormat::G32:
		return "G32";
	case FileFormat::GCC:
		return "GCC";
	case FileFormat::GPR:
		return "GPR";
	case FileFormat::GPX:
		return "GPX";
	case FileFormat::PNG:
		return "PNG";
	case FileFormat::GIF:
		return "GIF";
	case FileFormat::MDD:
		return "MDD";
	case FileFormat::A:
		return "A";
	case FileFormat::S4:
		return "S4";
	case FileFormat::SA:
		return "SA";
	case FileFormat::A6:
		return "A6";
	case FileFormat::CCD:
		return "CCD";
	case FileFormat::EVE:
		return "EVE";
	case FileFormat::MP3:
		return "MP3";
	case FileFormat::WAV:
		return "WAV";
	case FileFormat::OGG:
		return "OGG";
	case FileFormat::TXT:
		return "TXT";
	case FileFormat::CSV:
		return "CSV";
	}
	return "";
}

bool isArchiveFormat(FileFormat format)
{
	switch (format) {
	case FileFormat::ARC:
	case FileFormat::AWD:
	case FileFormat::AWF:
		return true;
	default:
		return false;
	}
}

bool isImageFormat(FileFormat format)
{
	switch (format) {
	case FileFormat::AKB:
	case FileFormat::GP4:
	case FileFormat::GP8:
	case FileFormat::G16:
	case FileFormat::G24:
	case FileFormat::G32:
	case FileFormat::GCC:
	case FileFormat::GPR:
	case FileFormat::GPX:
	case FileFormat::PNG:
		return true;
	default:
		return false;
	}
}

GElf::GElf()
	: QObject()
{
}

GElf::~GElf()
{
}

QString getSaveFileName(QWidget *parent, QString caption, QVector<FileFormat> formats)
{
	if (!formats.size()) {
		return QFileDialog::getSaveFileName(parent, caption);
	}

	QString format_string = "Supported Formats: ";
	for (int i = 0; i < formats.size(); i++) {
		if (i > 0)
			format_string.append(" ");
		format_string.append(fileFormatToExtension(formats[i]));
	}
	format_string.append(" (");

	for (int i = 0; i < formats.size(); i++) {
		if (i > 0)
			format_string.append(" ");
		format_string.append("*.");
		format_string.append(fileFormatToExtension(formats[i]));
	}
	format_string.append(") ");

	return QFileDialog::getSaveFileName(parent, caption, "", format_string);
}

static std::shared_ptr<struct archive> openArchiveFile(const QString &path, FileFormat format,
		unsigned flags)
{
	struct archive *ar = nullptr;
	switch (format) {
	case FileFormat::ARC:
	case FileFormat::AWD:
	case FileFormat::AWF:
		ar = archive_open(path.toUtf8(), flags);
		break;
	default:
		break;
	}
	return std::shared_ptr<struct archive>(ar, archive_close);
}

void GElf::openArchive(const QString &path, FileFormat format)
{
	GameDialog dialog(path, GameDialog::DialogType::ARCHIVE_OPEN);
	if (dialog.exec() != QDialog::Accepted)
		return;
	ai5_set_game(dialog.game->name);

	QGuiApplication::setOverrideCursor(Qt::WaitCursor);

	std::shared_ptr<struct archive> ar = openArchiveFile(path, format, dialog.flags);
	if (!ar) {
		QGuiApplication::restoreOverrideCursor();
		fileError(path, tr("Failed to read .%1 file").arg(fileFormatToExtension(format)));
		return;
	}

	emit getInstance().openedArchive(path, ar, dialog.game, dialog.flags);
	QGuiApplication::restoreOverrideCursor();
}

int fileFormatToCGFormat(FileFormat format)
{
	switch (format) {
	case FileFormat::AKB: return CG_TYPE_AKB;
	case FileFormat::GP4: return CG_TYPE_GP8;
	case FileFormat::GP8: return CG_TYPE_GP4;
	case FileFormat::G16: return CG_TYPE_G16;
	case FileFormat::G24: return CG_TYPE_G24;
	case FileFormat::G32: return CG_TYPE_G32;
	case FileFormat::GCC: return CG_TYPE_GCC;
	case FileFormat::GPR: return CG_TYPE_GPR;
	case FileFormat::GPX: return CG_TYPE_GPX;
	case FileFormat::PNG: return CG_TYPE_PNG;
	default: break;
	}
	return -1;
}

static int fileFormatToOutputCGFormat(FileFormat format)
{
	switch (format) {
	case FileFormat::PNG: return CG_TYPE_PNG;
	case FileFormat::G16: return CG_TYPE_G16;
	case FileFormat::G24: return CG_TYPE_G24;
	case FileFormat::G32: return CG_TYPE_G32;
	default: break;
	}
	return -1;
}

static bool convertCG(struct port *port, struct cg *cg, FileFormat to)
{
	if (!cg)
		return false;
	int cg_type = fileFormatToOutputCGFormat(to);
	if (cg_type < 0) {
		cg_free(cg);
		return false;
	}
	if (port->type != PORT_TYPE_FILE) {
		cg_free(cg);
		return false;
	}
	cg_depalettize(cg);
	cg_write(cg, port->file, (enum cg_type)cg_type);
	cg_free(cg);
	return true;
}

QVector<FileFormat> GElf::getSupportedConversionFormats(FileFormat from)
{
	switch (from) {
	case FileFormat::NONE:
	case FileFormat::ARC:
	case FileFormat::AWD:
	case FileFormat::AWF:
	case FileFormat::SMES:
	case FileFormat::GIF:
	case FileFormat::SA:
	case FileFormat::WAV:
	case FileFormat::OGG:
	case FileFormat::TXT:
	case FileFormat::CSV:
		return QVector<FileFormat>();
	case FileFormat::MES:
		return QVector<FileFormat>({FileFormat::SMES});
	case FileFormat::A:
	case FileFormat::S4:
		return QVector<FileFormat>({FileFormat::SA});
	case FileFormat::A6:
		return QVector<FileFormat>({
			FileFormat::PNG,
			FileFormat::G16,
			FileFormat::G24,
			FileFormat::G32,
			FileFormat::TXT,
		});
	case FileFormat::AKB:
	case FileFormat::GP4:
	case FileFormat::GP8:
	case FileFormat::G16:
	case FileFormat::G24:
	case FileFormat::G32:
	case FileFormat::GCC:
	case FileFormat::GPR:
	case FileFormat::GPX:
	case FileFormat::PNG:
	case FileFormat::MP3:
		return QVector<FileFormat>({
			FileFormat::PNG,
			FileFormat::G16,
			FileFormat::G24,
			FileFormat::G32,
		});
	case FileFormat::MDD:
		return QVector<FileFormat>({FileFormat::GIF});
	case FileFormat::EVE:
		return QVector<FileFormat>({FileFormat::CSV});
	case FileFormat::CCD:
		return QVector<FileFormat>({FileFormat::TXT});
	}

	return QVector<FileFormat>();
}

bool GElf::convertFormat(struct port *port, const QString &name, uint8_t *data, size_t size,
		FileFormat from, FileFormat to,
		const struct ai5_game *game)
{
	if (from == to) {
		return port_write_bytes(port, data, size);
	}

	switch (from) {
	case FileFormat::MES:
		if (to == FileFormat::SMES) {
			ai5_set_game(game->name);
			mes_ast_block toplevel = vector_initializer;
			if (!mes_decompile(data, size, &toplevel)) {
				fileError(name, tr("Failed to decompile .MES file"));
				return false;
			}
			mes_ast_block_print(toplevel, -1, port);
			mes_ast_block_free(toplevel);
			return true;
		}
		break;
	case FileFormat::A:
	case FileFormat::S4:
		if (to == FileFormat::SA) {
			ai5_set_game(game->name);
			struct anim *anim = anim_parse(data, size);
			if (!anim) {
				fileError(name, tr("Failed to decompile animation file"));
				return false;
			}
			if (!anim_print(port, anim)) {
				fileError(name, tr("Failed to decompile animation file"));
				anim_free(anim);
				return false;
			}
			anim_free(anim);
			return true;
		}
		break;
	case FileFormat::A6:
		if (isImageFormat(to)) {
			a6_array a6 = a6_parse(data, size);
			bool r = convertCG(port, a6_to_image(a6), to);
			a6_free(a6);
			return r;
		} else if (to == FileFormat::TXT) {
			a6_array a6 = a6_parse(data, size);
			a6_print(port, a6);
			a6_free(a6);
			return true;
		}
		break;
	case FileFormat::AKB:
	case FileFormat::GP4:
	case FileFormat::GP8:
	case FileFormat::G16:
	case FileFormat::G24:
	case FileFormat::G32:
	case FileFormat::GCC:
	case FileFormat::GPR:
	case FileFormat::GPX:
	case FileFormat::PNG:
		return convertCG(port, cg_load(data, size, (enum cg_type)fileFormatToCGFormat(from)), to);
	case FileFormat::MP3:
		return convertCG(port, mp3_render(data, size), to);
	case FileFormat::MDD:
		if (to == FileFormat::GIF) {
			size_t gif_size;
			uint8_t *gif = mdd_render(data, size, &gif_size);
			if (!gif) {
				fileError(name, tr("Failed to render .MDD file"));
				return false;
			}
			port_write_bytes(port, gif, gif_size);
			return true;
		}
		break;
	case FileFormat::NONE:
	case FileFormat::ARC:
	case FileFormat::AWD:
	case FileFormat::AWF:
	case FileFormat::SMES:
	case FileFormat::GIF:
	case FileFormat::SA:
	case FileFormat::WAV:
	case FileFormat::OGG:
	case FileFormat::TXT:
	case FileFormat::CSV:
		break;
	case FileFormat::CCD:
		if (to == FileFormat::TXT) {
			struct ccd *ccd = ccd_parse(data, size);
			if (!ccd) {
				fileError(name, tr("Failed to unpack .CCD file"));
				return false;
			}
			ccd_print(port, ccd);
			ccd_free(ccd);
			return true;
		}
		break;
	case FileFormat::EVE:
		if (to == FileFormat::CSV) {
			eve_print(port, data, size);
			return true;
		}
		break;
	}
	error(QString(tr("Conversion from %1 to %2 is not supported").arg(
			fileFormatToExtension(from),
			fileFormatToExtension(to))));
	return false;
}

static void convertOpenText(const QString &name, uint8_t *data, size_t size, FileFormat from,
		FileFormat to, const struct ai5_game *game, bool newTab)
{
	struct port out;
	port_buffer_init(&out);
	if (GElf::convertFormat(&out, name, data, size, from, to, game)) {
		char *text = (char*)port_buffer_get(&out, NULL);
		GElf::openText(name, text, to, newTab);
		free(text);
	}
}

void GElf::openData(const QString &name, uint8_t *data, size_t size, const struct ai5_game *game,
		bool newTab)
{
	FileFormat format = extensionToFileFormat(QFileInfo(name).suffix());
	switch (format) {
	case FileFormat::AKB:
	case FileFormat::GP4:
	case FileFormat::GP8:
	case FileFormat::G16:
	case FileFormat::G24:
	case FileFormat::G32:
	case FileFormat::GCC:
	case FileFormat::GPR:
	case FileFormat::GPX:
	case FileFormat::PNG: {
		int cg_format = fileFormatToCGFormat(format);
		if (cg_format < 0)
			break;
		struct cg *cg = cg_load(data, size, (enum cg_type)cg_format);
		if (!cg) {
			fileError(name, tr("Failed to decode image"));
			break;
		}
		cg_depalettize(cg);
		emit getInstance().openedImageFile(name, std::shared_ptr<struct cg>(cg, cg_free), newTab);
		break;
	}
	case FileFormat::GIF:
		emit getInstance().openedGifFile(name, data, size, newTab);
		break;
	case FileFormat::MDD: {
		size_t gif_size;
		uint8_t *gif = mdd_render(data, size, &gif_size);
		if (!gif) {
			fileError(name, tr("Failed to render .MDD file"));
			break;
		}
		emit getInstance().openedGifFile(name, gif, gif_size, newTab);
		free(gif);
		break;
	}
	case FileFormat::MES:
		convertOpenText(name, data, size, format, FileFormat::SMES, game, newTab);
		break;
	case FileFormat::A:
	case FileFormat::S4:
		convertOpenText(name, data, size, format, FileFormat::SA, game, newTab);
		break;
	case FileFormat::A6: {
		a6_array a6 = a6_parse(data, size);
		struct cg *cg = a6_to_image(a6);
		a6_free(a6);
		if (!cg) {
			fileError(name, tr("Failed to render .A6 file"));
			break;
		}
		cg_depalettize(cg);
		emit getInstance().openedImageFile(name, std::shared_ptr<struct cg>(cg, cg_free), newTab);
		break;
	}
	case FileFormat::CCD:
		// TODO: SCCD format?
		convertOpenText(name, data, size, format, FileFormat::TXT, game, newTab);
		break;
	case FileFormat::EVE:
		convertOpenText(name, data, size, format, FileFormat::CSV, game, newTab);
		break;
	case FileFormat::MP3: {
		struct cg *cg = mp3_render(data, size);
		if (!cg) {
			fileError(name, tr("Failed to render .MP3 file"));
			break;
		}
		cg_depalettize(cg);
		emit getInstance().openedImageFile(name, std::shared_ptr<struct cg>(cg, cg_free), newTab);
		break;
	}
	case FileFormat::SMES:
	case FileFormat::SA:
	case FileFormat::TXT:
	case FileFormat::CSV:
		openText(name, (char*)data, format, newTab);
		break;
	case FileFormat::WAV:
	case FileFormat::OGG:
		openMedia(name, data, size, format, newTab);
		break;
	case FileFormat::NONE:
	case FileFormat::ARC:
	case FileFormat::AWD:
	case FileFormat::AWF:
		openBinary(name, data, size, newTab);
		break;
	}
}

void GElf::openFile(const QString &path, bool newTab)
{
	FileFormat format = extensionToFileFormat(QFileInfo(path).suffix());
	if (isArchiveFormat(format)) {
		openArchive(path, format);
		return;
	}
	size_t size;
	uint8_t *data = static_cast<uint8_t*>(file_read(path.toUtf8(), &size));
	if (!data) {
		fileError(path, tr("Failed to read file"));
		return;
	}

	const struct ai5_game *game = &ai5_games[0];
	// prompt for game if necessary
	switch (format) {
	case FileFormat::MES:
	case FileFormat::A:
	case FileFormat::S4: {
		GameDialog dialog(path, GameDialog::DialogType::GAME_SELECT);
		if (dialog.exec() != QDialog::Accepted) {
			free(data);
			return;
		}
		game = dialog.game;
		break;
	}
	default:
		break;
	}
	openData(path, data, size, game, newTab);
	free(data);
}

void GElf::openArchiveData(struct archive_data *file, const struct ai5_game *game, bool newTab)
{
	if (!archive_data_load(file)) {
		fileError(file->name, tr("Filed to load archived file"));
		return;
	}

	openData(file->name, file->data, file->size, game, newTab);
	archive_data_release(file);
}

void GElf::openText(const QString &name, char *text, FileFormat format, bool newTab)
{
	emit getInstance().openedText(name, text, format, newTab);
}

void GElf::openBinary(const QString &name, uint8_t *bytes, size_t size, bool newTab)
{
	struct buffer b;
	size_t hex_size = ((size / 16) + 3) * 76 + 1; // 76 chars per line, 16 bytes per line
	buffer_init(&b, (uint8_t*)xmalloc(hex_size), hex_size);

	buffer_write_cstring(&b, "Address  0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  ASCII             \n");
	buffer_write_cstring(&b, "-------- ----------------------------------------------- ------------------\n");

	for (unsigned addr = 0; addr < size; addr += 16) {
		// write address
		b.index += sprintf((char*)b.buf+b.index, "%08x ", addr);

		// write bytes (hex)
		unsigned i;
		for (i = addr; i < addr + 16 && i < size; i++) {
			b.index += sprintf((char*)b.buf+b.index, "%02hhx ", bytes[i]);
		}
		if (i == size) {
			unsigned remaining = 16 - (i - addr);
			memset(b.buf+b.index, ' ', remaining * 3);
			b.index += remaining * 3;
		}

		b.buf[b.index++] = '|';
		for (i = addr; i < addr + 16 && i < size; i++) {
			b.buf[b.index++] = isprint(bytes[i]) ? bytes[i] : '?';
		}
		if (i == size) {
			unsigned remaining = 16 - (i - addr);
			memset(b.buf+b.index, ' ', remaining);
			b.index += remaining;
		}
		b.buf[b.index++] = '|';
		b.buf[b.index++] = '\n';
	}

	b.buf[b.index++] = '\0';

	openText(name, (char*)b.buf, FileFormat::NONE, newTab);
	free(b.buf);
}

void GElf::openMedia(const QString &name, uint8_t *data, size_t size, FileFormat format,
		bool newTab)
{
	emit getInstance().openedMedia(name, data, size, format, newTab);
}

void GElf::fileError(const QString &filename, const QString &message)
{
	error(QString("%1: %2").arg(message).arg(filename));
}

void GElf::error(const QString &message)
{
	emit getInstance().errorMessage(message);
}

void GElf::status(const QString &message)
{
	emit getInstance().statusMessage(message);
}

void GElf::criticalError(const QString &message)
{
	QMessageBox msgBox;
	msgBox.setText(message);
	msgBox.setInformativeText("The program will now exit.");
	msgBox.setIcon(QMessageBox::Critical);
	msgBox.exec();
	sys_exit(1);
}
