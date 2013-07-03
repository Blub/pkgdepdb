#include <memory>

#include <archive.h>
#include <archive_entry.h>

#include "main.h"

Package::Package(const std::string& path)
{
	error = false;

	struct archive *tar = archive_read_new();
	archive_read_support_filter_all(tar);
	archive_read_support_format_all(tar);

	struct archive_entry *entry;
	if (ARCHIVE_OK != archive_read_open_filename(tar, path.c_str(), 10240)) {
		error = true;
		return;
	}

	while (ARCHIVE_OK == archive_read_next_header(tar, &entry)) {
		//log(Log, "-> %s\n", archive_entry_pathname(entry));

		if (!add_entry(tar, entry))
			break;
	}

	archive_read_free(tar);
}

Package::Package()
{
	error = false;
}

bool
Package::care_about(struct archive_entry *entry) const
{
	mode_t mode = archive_entry_mode(entry);

	if (AE_IFLNK == (mode & AE_IFLNK)) {
		// ignoring symlinks for now
		return false;
	}

	if (AE_IFREG != (mode & AE_IFREG)) {
		// not a regular file
		return false;
	}

	if (!archive_entry_size_is_set(entry)) {
		// ignore files which have no size...
		return false;
	}

	return true;
}

bool
Package::add_entry(struct archive *tar, struct archive_entry *entry)
{
	std::string filename(archive_entry_pathname(entry));
	bool isinfo = filename == ".PKGINFO";

	// for now we only care about files named lib.*\.so(\.|$)
	if (!isinfo && !care_about(entry))
	{
		log(Debug, "skip: %s\n", filename.c_str());
		archive_read_data_skip(tar);
		return true;
	}

	// Check the size
	size_t size = archive_entry_size(entry);
	if (!size) {
#if 0
		log(Error, "invalid size: %lu for file %s\n",
		    (unsigned long)size,
		    filename.c_str());
		return false;
#else
		return true;
#endif
	}

	if (isinfo)
		return read_info(tar, size);

	return read_object(tar, std::move(filename), size);
}

bool
Package::read_info(struct archive *tar, size_t size)
{
	std::unique_ptr<char> data(new char[size]);
	ssize_t rc = archive_read_data(tar, data.get(), size);
	if ((size_t)rc != size) {
		log(Error, "failed to read .PKGINFO");
		return false;
	}

	std::string str(data.get());
	size_t pos = str.find("pkgname = ");
	if (pos == std::string::npos) {
		log(Error, "missing pkgname entry in .PKGINFO");
		return false;
	}

	if (pos != 0 && data.get()[pos-1] != '\n') {
		log(Error, "corrupted .PKGINFO");
		return false;
	}

	this->name = str.substr(pos + 10, str.find_first_of(" \n\r\t", pos+10) - pos - 10);
	return true;
}

bool
Package::read_object(struct archive *tar, std::string &&filename, size_t size)
{
	std::vector<char> data;
	data.resize(size);

	ssize_t rc = archive_read_data(tar, &data[0], size);
	if (rc < 0) {
		log(Error, "failed to read from archive stream\n");
		return false;
	}
	else if ((size_t)rc != size) {
		log(Error, "file was short: %s\n", filename.c_str());
		return false;
	}

	bool err = false;
	std::shared_ptr<Elf> object(Elf::open(&data[0], data.size(), &err));
	if (!object.get()) {
		if (err)
			log(Error, "error in: %s\n", filename.c_str());
		return !err;
	}

	size_t slash = filename.find_last_of('/');
	if (slash == std::string::npos)
		object->basename = std::move(filename);
	else {
		object->dirname  = filename.substr(0, slash);
		object->basename = filename.substr(slash+1, std::string::npos);
	}

	objects.push_back(object);

	return true;
}

void
Package::show_needed()
{
	const char *name = this->name.c_str();
	for (auto &obj : objects) {
		std::string path = obj->dirname + "/" + obj->basename;
		const char *objname = path.c_str();
		for (auto &need : obj->needed) {
			printf("%s: %s NEEDS %s\n", name, objname, need.c_str());
		}
	}
}
