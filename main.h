#ifndef WDEPTRACK_MAIN_H__
#define WDEPTRACK_MAIN_H__

#include <string>
#include <vector>

/// Package class
/// reads a package archive, extracts information
/// about libraries and updates a database with new
/// information.
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

private:
	bool add_entry(struct archive *tar, struct archive_entry *entry);
	bool care_about(struct archive_entry *entry) const;

public:
	std::string         name;
	std::vector<Object> objects;

private:
	bool error;
};

#endif
