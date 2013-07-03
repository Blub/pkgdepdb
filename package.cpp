#include <memory>

#include <archive.h>
#include <archive_entry.h>

#include "main.h"

static bool
care_about(struct archive_entry *entry)
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

static bool
read_info(Package *pkg, struct archive *tar, size_t size)
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

	pkg->name = str.substr(pos + 10, str.find_first_of(" \n\r\t", pos+10) - pos - 10);
	return true;
}

static bool
read_object(Package *pkg, struct archive *tar, std::string &&filename, size_t size)
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
	rptr<Elf> object(Elf::open(&data[0], data.size(), &err));
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

	pkg->objects.push_back(object);

	return true;
}

static bool
add_entry(Package *pkg, struct archive *tar, struct archive_entry *entry)
{
	std::string filename(archive_entry_pathname(entry));
	bool isinfo = filename == ".PKGINFO";

	// for now we only care about files named lib.*\.so(\.|$)
	if (!isinfo && !care_about(entry))
	{
		archive_read_data_skip(tar);
		return true;
	}

	// Check the size
	size_t size = archive_entry_size(entry);
	if (!size)
		return true;

	if (isinfo)
		return read_info(pkg, tar, size);

	return read_object(pkg, tar, std::move(filename), size);
}


Package*
Package::open(const std::string& path)
{
	std::unique_ptr<Package> package(new Package);

	struct archive *tar = archive_read_new();
	archive_read_support_filter_all(tar);
	archive_read_support_format_all(tar);

	struct archive_entry *entry;
	if (ARCHIVE_OK != archive_read_open_filename(tar, path.c_str(), 10240)) {
		return 0;
	}

	while (ARCHIVE_OK == archive_read_next_header(tar, &entry)) {
		if (!add_entry(package.get(), tar, entry))
			return 0;
	}

	archive_read_free(tar);
	return package.release();
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
