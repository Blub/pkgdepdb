#include <memory>

#include <archive.h>
#include <archive_entry.h>

#include "main.h"

static bool
care_about(struct archive_entry *entry)
{
	mode_t mode = archive_entry_mode(entry);

#if 0
	if (AE_IFLNK == (mode & AE_IFLNK)) {
		// ignoring symlinks for now
		return false;
	}
#endif

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

    pos = str.find("pkgver = ");
	if (pos == std::string::npos) {
		log(Warn, "missing pkgname entry in .PKGINFO");
		return true;
	}

	if (pos != 0 && data.get()[pos-1] != '\n') {
		log(Warn, "corrupted .PKGINFO; skipping pkgver");
		return true;
	}

	pkg->version = str.substr(pos + 9, str.find_first_of(" \n\r\t", pos+9) - pos - 9);
	return true;
}

static inline std::tuple<std::string, std::string>
splitpath(const std::string& path)
{
	size_t slash = path.find_last_of('/');
	if (slash == std::string::npos)
		return std::make_tuple("/", path);
	if (path[0] != '/')
		return std::make_tuple(std::move(std::string("/") + path.substr(0, slash)), path.substr(slash+1));
	return std::make_tuple(path.substr(0, slash), path.substr(slash+1));
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

	auto split(std::move(splitpath(filename)));
	object->dirname  = std::move(std::get<0>(split));
	object->basename = std::move(std::get<1>(split));

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

	mode_t mode = archive_entry_mode(entry);
	if (AE_IFLNK == (mode & AE_IFLNK)) {
		// it's a symlink...
		const char *link = archive_entry_symlink(entry);
		if (!link) {
			log(Error, "error reading symlink");
			return false;
		}
		archive_read_data_skip(tar);
		pkg->symlinks[filename] = link;
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

Elf*
Package::find(const std::string& dirname, const std::string& basename) const
{
	for (auto &obj : objects) {
		if (obj->dirname == dirname && obj->basename == basename)
			return const_cast<Elf*>(obj.get());
	}
	return nullptr;
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

	bool changed;
	do {
		changed = false;
		for (auto link = package->symlinks.begin(); link != package->symlinks.end();)
		{
			auto linkfrom = splitpath(link->first);
			decltype(linkfrom) linkto;

			// handle relative as well as absolute symlinks
			if (!link->second.length()) {
				// illegal
				++link;
				continue;
			}
			if (link->second[0] == '/') // absolute
				linkto = splitpath(link->second);
			else // relative
			{
				std::string fullpath = std::get<0>(linkfrom) + "/" + link->second;
				linkto = splitpath(fullpath);
			}

			Elf *obj = package->find(std::get<0>(linkto), std::get<1>(linkto));
			if (!obj) {
				++link;
				continue;
			}
			changed = true;

			Elf *copy = new Elf(*obj);
			copy->dirname  = std::move(std::get<0>(linkfrom));
			copy->basename = std::move(std::get<1>(linkfrom));

			package->objects.push_back(copy);
			package->symlinks.erase(link++);
		}
	} while (changed);
	package->symlinks.clear();

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
