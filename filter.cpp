#ifdef WITH_REGEX
# include <sys/types.h>
# include <regex.h>
#endif

#include <algorithm>

#include "main.h"

namespace util {
	template<typename C, typename K>
	inline bool
	contains(const C &lst, const K &k) {
		return std::find(lst.begin(), lst.end(), k) != lst.end();
	}
}

namespace filter {

static bool
match_glob(const std::string &glob, size_t, const std::string &str, size_t);

PackageFilter::PackageFilter(bool neg_)
: negate(neg_)
{}

PackageFilter::~PackageFilter()
{}

ObjectFilter::ObjectFilter(bool neg_)
: negate(neg_)
{}

ObjectFilter::~ObjectFilter()
{}

// Package Name
class PackageName : public PackageFilter {
public:
	std::string name;
	PackageName(bool neg, const std::string &name_)
	: PackageFilter(neg), name(name_) {}
	virtual bool visible(const Package &pkg) const {
		return (pkg.name == this->name);
	}
};

unique_ptr<PackageFilter>
PackageFilter::name(const std::string &s, bool neg) {
	return unique_ptr<PackageFilter>(new PackageName(neg, s));
}

class PackageGroup : public PackageName {
public:
	PackageGroup(bool neg, const std::string &name_)
	: PackageName(neg, name_) {}
	virtual bool visible(const Package &pkg) const {
		return pkg.groups.find(this->name) != pkg.groups.cend();
	}
};

unique_ptr<PackageFilter>
PackageFilter::group(const std::string &s, bool neg) {
	return unique_ptr<PackageFilter>(new PackageGroup(neg, s));
}

// Object Name
class ObjectName : public ObjectFilter {
public:
	std::string name;
	ObjectName(bool neg, const std::string &name_)
	: ObjectFilter(neg), name(name_) {}
	virtual bool visible(const Elf &elf) const {
		return (elf.basename == this->name);
	}
};

unique_ptr<ObjectFilter>
ObjectFilter::name(const std::string &s, bool neg) {
	return unique_ptr<ObjectFilter>(new ObjectName(neg, s));
}

// Object Depends
class ObjectDepends : public ObjectFilter {
public:
	std::string name;
	ObjectDepends(bool neg, const std::string &name_)
	: ObjectFilter(neg), name(name_) {}
	virtual bool visible(const Elf &elf) const {
		return util::contains(elf.needed, this->name);
	}
};

unique_ptr<ObjectFilter>
ObjectFilter::depends(const std::string &s, bool neg) {
	return unique_ptr<ObjectFilter>(new ObjectDepends(neg, s));
}

// Package Name (Glob)
class PackageNameGlob : public PackageFilter {
public:
	std::string name;
	PackageNameGlob(bool neg, const std::string &name_)
	: PackageFilter(neg), name(name_) {}
	virtual bool visible(const Package &pkg) const {
		return match_glob(name, 0, pkg.name, 0);
	}
};

unique_ptr<PackageFilter>
PackageFilter::nameglob(const std::string &s, bool neg) {
	return unique_ptr<PackageFilter>(new PackageNameGlob(neg, s));
}

class PackageGroupGlob : public PackageNameGlob {
public:
	PackageGroupGlob(bool neg, const std::string &name_)
	: PackageNameGlob(neg, name_) {}
	virtual bool visible(const Package &pkg) const {
		for (auto &i : pkg.groups) {
			if (match_glob(name, 0, i, 0))
				return true;
		}
		return false;
	}
};

unique_ptr<PackageFilter>
PackageFilter::groupglob(const std::string &s, bool neg) {
	return unique_ptr<PackageFilter>(new PackageGroupGlob(neg, s));
}

// Object Name (Glob)
class ObjectNameGlob : public ObjectName {
public:
	ObjectNameGlob(bool neg, const std::string &name_)
	: ObjectName(neg, name_) {}
	virtual bool visible(const Elf &elf) const {
		return match_glob(name, 0, elf.basename, 0);
	}
};

unique_ptr<ObjectFilter>
ObjectFilter::nameglob(const std::string &s, bool neg) {
	return unique_ptr<ObjectFilter>(new ObjectNameGlob(neg, s));
}

class PackageBroken : public PackageFilter {
public:
	PackageBroken(bool neg)
	: PackageFilter(neg) {}
	virtual bool visible(const DB &db, const Package &pkg) const {
		return db.is_broken(&pkg);
	}
};

// Object Depends (Glob)
class ObjectDependsGlob : public ObjectDepends {
public:
	ObjectDependsGlob(bool neg, const std::string &name_)
	: ObjectDepends(neg, name_) {}
	virtual bool visible(const Elf &elf) const {
		for (auto &lib : elf.needed) {
			if (match_glob(name, 0, lib, 0))
				return true;
		}
		return false;
	}
};

unique_ptr<ObjectFilter>
ObjectFilter::dependsglob(const std::string &s, bool neg) {
	return unique_ptr<ObjectFilter>(new ObjectDependsGlob(neg, s));
}

// Broken Package
unique_ptr<PackageFilter>
PackageFilter::broken(bool neg) {
	return unique_ptr<PackageFilter>(new PackageBroken(neg));
}

// Utility functions:

static bool /* tail recursive */
match_glob(const std::string &glob, size_t g, const std::string &str, size_t s)
{
	size_t from, to;
	bool neg = false;

	auto parse_group = [&]() -> bool {
		from = ++g;
		neg = (g < glob.length() && glob[g] == '^');
		if (neg) ++from;
		if (glob[g] == ']') // if the group contains a ] it must come first
			++g;
		while (g < glob.length() && glob[g] != ']')
			++g;
		if (g >= glob.length()) {
			// glob syntax error, treat the [ as a regular [ character
			g = from-1;
			return false;
		}
		to = g-1;
		return true;
	};

	auto matches_group = [&](const char c) -> bool {
		for (size_t f = from; f != to+1; ++f) {
			if (f > from && f != to && glob[f] == '-') {
				++f;
				if (c >= glob[f-1] && c <= glob[f])
					return !neg;
			}
			if (c == glob[f]) {
				return !neg;
			}
		}
		return neg;
	};

	if (g >= glob.length()) // nothing else to match
		return s >= str.length(); // true if there are no contents to match either
	if (s >= str.length()) {
		if (glob[g] == '*')
			return match_glob(glob, g+1, str, s+1);
		return false;
	}
	switch (glob[g]) {
		default:
			if (glob[g] != str[s])
				return false;
			return match_glob(glob, g+1, str, s+1);
		case '?':
			return match_glob(glob, g+1, str, s+1);
		case '[':
			if (!parse_group()) {
				if (str[s] != '[')
					return false;
				return match_glob(glob, g+1, str, s+1);
			}
			if (!matches_group(str[s]))
				return false;
			return match_glob(glob, g+1, str, s+1);

		case '*':
		{
			bool wasgroup = false;
			while (g < glob.length() && (glob[g] == '*' || glob[g] == '?')) ++g;
			if (g >= glob.length()) // ended with a set of * and ?, so we match
				return true;
			if (glob[g] == '[')
				wasgroup = parse_group();
			while (s < str.length()) {
				if ( (wasgroup && matches_group(str[s])) ||
				     (!wasgroup && str[s] == glob[g]) )
				{
					// next character matches here, fork off:
					if (match_glob(glob, g+1, str, s+1))
						return true;
				}
				++s; // otherwise gobble it up into the *
			}
			return false;
		}
	}
}

#ifdef WITH_REGEX
static unique_ptr<regex_t>
make_regex(const std::string &pattern, bool ext, bool icase) {
	unique_ptr<regex_t> regex(new regex_t);
	int cflags = REG_NOSUB;
	if (ext)   cflags |= REG_EXTENDED;
	if (icase) cflags |= REG_ICASE;

	int err;
	if ( (err = ::regcomp(regex.get(), pattern.c_str(), cflags)) != 0) {
		char buf[4096];
		regerror(err, regex.get(), buf, sizeof(buf));
		log(Error, "failed to compile regex (flags: %s, %s): %s\n",
		    (ext ? "extended" : "basic"),
		    (icase ? "case insensitive" : "case sensitive"),
		    pattern.c_str());
		log(Error, "regex error: %s\n", buf);
		return nullptr;
	}

	return move(regex);
}

// Package Name (Regex)
class PackageNameRegex : public PackageFilter {
public:
	std::string         pattern;
	unique_ptr<regex_t> regex;
	bool                ext, icase;

	PackageNameRegex(bool neg, const std::string &pattern_,
	                 bool ext_, bool icase_,
	                 unique_ptr<regex_t> &&regex_)
	: PackageFilter(neg), pattern(pattern_)
	, regex(move(regex_))
	, ext(ext_), icase(icase_)
	{}
	virtual bool visible(const Package &pkg) const {
		regmatch_t rm;
		return 0 == regexec(regex.get(), pkg.name.c_str(), 0, &rm, 0);
	}

	~PackageNameRegex() {
		regfree(regex.get());
	}
};

unique_ptr<PackageFilter>
PackageFilter::nameregex(const std::string &pattern,
                         bool ext, bool icase, bool neg)
{
	unique_ptr<regex_t> regex(make_regex(pattern, ext, icase));
	if (!regex)
		return nullptr;

	return unique_ptr<PackageFilter>(
		new PackageNameRegex(neg, pattern, ext, icase, move(regex)));
}

class PackageGroupRegex : public PackageNameRegex {
public:
	PackageGroupRegex(bool neg, const std::string &pattern_,
	                  bool ext_, bool icase_,
	                  unique_ptr<regex_t> &&regex_)
	: PackageNameRegex(neg, pattern_, ext_, icase_, move(regex_)) {}
	virtual bool visible(const Package &pkg) const {
		regmatch_t rm;
		for (auto &i : pkg.groups) {
			if (0 == regexec(regex.get(), i.c_str(), 0, &rm, 0))
				return true;
		}
		return false;
	}
};

unique_ptr<PackageFilter>
PackageFilter::groupregex(const std::string &pattern,
                          bool ext, bool icase, bool neg)
{
	unique_ptr<regex_t> regex(make_regex(pattern, ext, icase));
	if (!regex)
		return nullptr;

	return unique_ptr<PackageFilter>(
		new PackageGroupRegex(neg, pattern, ext, icase, move(regex)));
}

// Object Name (Regex)
class ObjectNameRegex : public ObjectFilter {
public:
	std::string         pattern;
	unique_ptr<regex_t> regex;
	bool                ext, icase;

	ObjectNameRegex(bool neg, const std::string &pattern_,
	                bool ext_, bool icase_,
	                unique_ptr<regex_t> &&regex_)
	: ObjectFilter(neg), pattern(pattern_)
	, regex(move(regex_))
	, ext(ext_), icase(icase_)
	{}
	virtual bool visible(const Elf &elf) const {
		regmatch_t rm;
		return 0 == regexec(regex.get(), elf.basename.c_str(), 0, &rm, 0);
	}

	~ObjectNameRegex() {
		regfree(regex.get());
	}
};

unique_ptr<ObjectFilter>
ObjectFilter::nameregex(const std::string &pattern,
                        bool ext, bool icase, bool neg)
{
	unique_ptr<regex_t> regex(make_regex(pattern, ext, icase));
	if (!regex)
		return nullptr;

	return unique_ptr<ObjectFilter>(
		new ObjectNameRegex(neg, pattern, ext, icase, move(regex)));
}

// Object Depends (Regex)
class ObjectDependsRegex : public ObjectFilter {
public:
	std::string         pattern;
	unique_ptr<regex_t> regex;
	bool                ext, icase;

	ObjectDependsRegex(bool neg, const std::string &pattern_,
	                   bool ext_, bool icase_,
	                   unique_ptr<regex_t> &&regex_)
	: ObjectFilter(neg), pattern(pattern_)
	, regex(move(regex_))
	, ext(ext_), icase(icase_)
	{}
	virtual bool visible(const Elf &elf) const {
		regmatch_t rm;
		for (auto &lib : elf.needed) {
			if (0 == regexec(regex.get(), lib.c_str(), 0, &rm, 0))
				return true;
		}
		return false;
	}

	~ObjectDependsRegex() {
		regfree(regex.get());
	}
};

unique_ptr<ObjectFilter>
ObjectFilter::dependsregex(const std::string &pattern,
                           bool ext, bool icase, bool neg)
{
	unique_ptr<regex_t> regex(make_regex(pattern, ext, icase));
	if (!regex)
		return nullptr;

	return unique_ptr<ObjectFilter>(
		new ObjectDependsRegex(neg, pattern, ext, icase, move(regex)));
}


#endif

} // namespace filter

#ifdef TEST
#include <iostream>
using filter::match_glob;
int main() {
	std::string text("This is a stupid text.");
	int r=0;

	auto tryglob = [&](const char *c, bool expect) {
		if (match_glob(c, 0, text, 0) != expect) {
			std::cout << "FAIL: " << c << ": " << (expect ? "TRUE" : "FALSE") << " expected." << std::endl;
			r=1;
		} else {
			std::cout << "PASS: " << c << std::endl;
		}
	};

	tryglob("This*", true);
	tryglob("this*", false);
	tryglob("*this*", false);
	tryglob("*This*", true);
	tryglob("*This", false);
	tryglob("*?his*", true);
	tryglob("*?his?", false);
	tryglob("*.", true);
	tryglob("*.?", false);
	tryglob("[Tt]his*", true);
	tryglob("*[Tt]his*", true);
	tryglob("*T[hasdf]is*", true);
	tryglob("*T[hasdf]Xs*", false);
	tryglob("*T[^asdf]is*", true);
	tryglob("*T[^hsdf]is*", false);
	tryglob("*is*t*t*t*", true);
	tryglob("*is*t.", true);
	tryglob("*is*[asdf]*t.", true);
	tryglob("*is*[yz]*t.", false);
	text = "Fabcdbar";
	tryglob("Fabcdbar*", true);
	tryglob("Fabcdbar*?", false);
	tryglob("F[a-d]b*", true);
	tryglob("F[a-d][a-d]c*", true);
	tryglob("F[a-d][e-z]b*", false);
	tryglob("F[^a-d]b*", false);
	text = "foo-bar";
	tryglob("foo[-]bar", true);
	tryglob("foo[-x-z]bar", true);
	tryglob("fo[^-n]-bar", true);
	tryglob("fo[^n-]-bar", true);
	text = "Fa[bc]dbar";
	tryglob("Fa[[]bc*", true);
	tryglob("Fa[[]bc[]]db*", true);
	return r;
}
#endif
