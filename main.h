#ifndef WDEPTRACK_MAIN_H__
#define WDEPTRACK_MAIN_H__

#include <string>
#include <vector>

enum {
	Debug,
	Warn,
	Error
};

void log(int level, const char *msg, ...);

/// Package class
/// reads a package archive, extracts information
class Package {
public:
	Package();
	Package(const std::string& path);

public:
	/// Represents an object file
	class Object {
	public:
		Object(const std::string& filepath);

	public:
		std::string              path;
		std::string              name;
		std::vector<std::string> need;
		std::string              rpath;
	};

public:
	inline operator bool()  { return !error; }
	inline bool operator!() { return error; }

	void show_needed();

private:
	bool add_entry  (struct archive *tar, struct archive_entry *entry);
	bool care_about (struct archive_entry *entry) const;
	bool read_object(struct archive *tar, const std::string &filename, size_t size);

public:
	std::string         name;
	std::vector<std::shared_ptr<Object> > objects;

private:
	bool error;
};

#endif
