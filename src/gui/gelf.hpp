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

#ifndef GELF_GELF_HPP
#define GELF_GELF_HPP

#include <QObject>

struct archive;
struct archive_data;
struct ai5_game;

enum class FileFormat {
	NONE,
	ARC,
	AWD,
	AWF,
	MES,
	SMES,
	AKB,
	GP4,
	GP8,
	G16,
	G24,
	G32,
	GCC,
	GPR,
	GPX,
	PNG,
	GIF,
	MDD,
	A,
	S4,
	SA,
	A6,
	CCD,
	EVE,
	MP3, // Isaku map file, not audio
	WAV,
	OGG,
	TXT,
	CSV,
};

FileFormat extensionToFileFormat(QString extension);
QString fileFormatToExtension(FileFormat format);
bool isArchiveFormat(FileFormat format);
bool isImageFormat(FileFormat format);
int fileFormatToCGFormat(FileFormat format);
QString getSaveFileName(QWidget *parent, QString caption, QVector<FileFormat> formats);

class GElf : public QObject
{
	Q_OBJECT
public:
	static GElf& getInstance()
	{
		static GElf instance;
		return instance;
	}
	GElf(GElf const&) = delete;
	void operator=(GElf const&) = delete;

	static void openFile(const QString &path, bool newTab = false);
	static void openArchiveData(struct archive_data *file, const struct ai5_game *game,
			bool newTab = false);
	static void openData(const QString &name, uint8_t *data, size_t size,
			const struct ai5_game *game, bool newTab = false);
	static void openText(const QString &name, char *text, FileFormat format,
			bool newTab = false);
	static void openBinary(const QString &name, uint8_t *bytes, size_t size,
			bool newTab = false);
	static void openMedia(const QString &name, uint8_t *bytes, size_t size,
			FileFormat format, bool newTab = false);
	static QVector<FileFormat> getSupportedConversionFormats(FileFormat from);
	static bool convertFormat(struct port *out, const QString &name, uint8_t *data,
			size_t size, FileFormat from, FileFormat to,
			const struct ai5_game *game);
	static void error(const QString &message);
	[[noreturn]] static void criticalError(const QString &message);
	static void status(const QString &message);
signals:
	void openedArchive(const QString &fileName, std::shared_ptr<struct archive> arc,
			const struct ai5_game *game, unsigned flags);
	void openedImageFile(const QString &fileName, std::shared_ptr<struct cg> cg,
			bool newTab);
	void openedGifFile(const QString &fileName, const uint8_t *data, size_t size,
			bool newTab);
	void openedMedia(const QString &fileName, const uint8_t *data, size_t size,
			FileFormat format, bool newTab);
	void openedText(const QString &name, char *text, FileFormat format, bool newTab);
	void errorMessage(const QString &message);
	void statusMessage(const QString &message);
private:
	GElf();
	~GElf();
	static void openArchive(const QString &path, FileFormat format);
	static void fileError(const QString &filename, const QString &message);
};

#endif // GELF_GELF_HPP
