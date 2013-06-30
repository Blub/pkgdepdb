#include <stdio.h>
#include <stdarg.h>

#include <memory>
#include <tuple>

#include <archive.h>
#include <archive_entry.h>

#include <llvm/Support/system_error.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Object/Binary.h>

#include "main.h"

enum {
	Debug,
	Warn,
	Error
};
static int LogLevel = Warn;

void
log(int level, const char *msg, ...)
{
	if (level < LogLevel)
		return;
	va_list ap;
	va_start(ap, msg);
	vfprintf(stderr, msg, ap);
	va_end(ap);
}

Package::Object::Object(const std::string &filepath)
{
	size_t dot = filepath.find_last_of('.');
	if (dot == std::string::npos) {
		// contains no dot
		name = filepath;
	}
	else {
		path = filepath.substr(0, dot);
		name = filepath.substr(dot+1, std::string::npos);
	}
}

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

	// for now we only care about files named lib.*\.so(\.|$)
	if (!care_about(entry))
	{
		log(Debug, "skip: %s\n", filename.c_str());
		archive_read_data_skip(tar);
		return true;
	}

	// Check the size
	size_t size = archive_entry_size(entry);
	if (!size) {
		log(Error, "invalid size: %lu for file %s\n",
		    (unsigned long)size,
		    filename.c_str());
		return false;
	}

	// Read into an LLVM MemoryBuffer
	std::unique_ptr<llvm::MemoryBuffer> buffer(llvm::MemoryBuffer::getNewUninitMemBuffer(size));
	char *data = const_cast<char*>(buffer->getBufferStart());

	ssize_t rc = archive_read_data(tar, data, size);
	if (rc < 0) {
		log(Error, "failed to read from archive stream\n");
		return false;
	}

	// Create an ELF Object
	// in llvm memorybuffers are moved, create a new one:
	llvm::MemoryBuffer *bufref = llvm::MemoryBuffer::getMemBuffer(buffer->getBuffer(), "", false);

	llvm::OwningPtr<llvm::object::Binary> binary;
	llvm::error_code ec = llvm::object::createBinary(bufref, binary);
	if (ec) {
		log(Debug, "nope: %s\n", filename.c_str());
		return true;
	}
	if (!binary->isObject()) {
		log(Debug, "not an object: %s\n", filename.c_str());
		return true;
	}
	if (!binary->isELF()) {
		// for now...
		log(Debug, "not an ELF object: %s\n", filename.c_str());
		return true;
	}

	std::unique_ptr<llvm::object::ObjectFile> obj(llvm::object::ObjectFile::createELFObjectFile(buffer.release()));
	log(Debug, "ELF: %s\n", filename.c_str());

	llvm::StringRef path;
	for (auto need = obj->begin_libraries_needed();
	     !ec && need != obj->end_libraries_needed();
	     need.increment(ec) )
	{
		ec = need->getPath(path);
		if (ec) {
			log(Error, "error reading library ref\n");
			return false;
		}
		printf("%s NEEDS %s\n", filename.c_str(), path.str().c_str());
	}

	return true;
}

int main(int argc, char **argv) {
	(void)argc; (void)argv;
	if (argc != 2) {
		log(Error, "usage error\n");
		return 1;
	}

	Package package(argv[1]);
	if (!package)
		log(Error, "there was an error\n");
	return 0;
}
