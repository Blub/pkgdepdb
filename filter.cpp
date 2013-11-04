#ifdef WITH_REGEX
# include <sys/types.h>
# include <regex.h>
#endif

#include <algorithm>

#include "main.h"

using std::shared_ptr;

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

// general purpose package filter
class PkgFilt : public PackageFilter {
public:
	std::function<bool(const DB&, const Package&)> func;

	PkgFilt(bool neg, std::function<bool(const DB&, const Package&)> &&fn)
	: PackageFilter(neg), func(move(fn)) {}

	PkgFilt(bool neg, std::function<bool(const Package&)> &&fn)
	: PkgFilt(neg, [fn] (const DB &db, const Package &pkg) -> bool {
		(void)db;
		return fn(pkg);
	}) {}

	virtual bool visible(const DB &db, const Package &pkg) const {
		return func(db, pkg);
	}
};

unique_ptr<PackageFilter>
PackageFilter::name(const std::string &s, bool neg) {
	return mk_unique<PkgFilt>(neg, [s](const Package &pkg) {
		return pkg.name == s;
	});
}

unique_ptr<PackageFilter>
PackageFilter::nameglob(const std::string &s, bool neg) {
	return mk_unique<PkgFilt>(neg, [s](const Package &pkg) {
		return match_glob(s, 0, pkg.name, 0);
	});
}

unique_ptr<PackageFilter>
PackageFilter::group(const std::string &s, bool neg) {
	return mk_unique<PkgFilt>(neg, [s](const Package &pkg) {
		return pkg.groups.find(s) != pkg.groups.cend();
	});
}

unique_ptr<PackageFilter>
PackageFilter::groupglob(const std::string &s, bool neg) {
	return mk_unique<PkgFilt>(neg, [s](const Package &pkg) {
		for (auto &i : pkg.groups) {
			if (match_glob(s, 0, i, 0))
				return true;
		}
		return false;
	});
}

unique_ptr<PackageFilter>
PackageFilter::depends(const std::string &s, bool neg) {
	return mk_unique<PkgFilt>(neg, [s](const Package &pkg) {
		return util::contains(pkg.depends, s);
	});
}

unique_ptr<PackageFilter>
PackageFilter::dependsglob(const std::string &s, bool neg) {
	return mk_unique<PkgFilt>(neg, [s](const Package &pkg) {
		for (auto &i : pkg.depends) {
			if (match_glob(s, 0, i, 0))
				return true;
		}
		return false;
	});
}

unique_ptr<PackageFilter>
PackageFilter::broken(bool neg) {
	return mk_unique<PkgFilt>(neg, [](const DB &db, const Package &pkg) {
		return db.is_broken(&pkg);
	});
}

// general purpose object filter
class ObjFilt : public ObjectFilter {
public:
	std::function<bool(const Elf&)> func;

	ObjFilt(bool neg, std::function<bool(const Elf&)> &&fn)
	: ObjectFilter(neg), func(move(fn)) {}

	virtual bool visible(const Elf &elf) const {
		return func(elf);
	}
};

unique_ptr<ObjectFilter>
ObjectFilter::name(const std::string &s, bool neg) {
	return mk_unique<ObjFilt>(neg, [s](const Elf &elf) {
		return elf.basename == s;
	});
}

unique_ptr<ObjectFilter>
ObjectFilter::nameglob(const std::string &s, bool neg) {
	return mk_unique<ObjFilt>(neg, [s](const Elf &elf) {
		return match_glob(s, 0, elf.basename, 0);
	});
}

unique_ptr<ObjectFilter>
ObjectFilter::depends(const std::string &s, bool neg) {
	return mk_unique<ObjFilt>(neg, [s](const Elf &elf) {
		return util::contains(elf.needed, s);
	});
}

unique_ptr<ObjectFilter>
ObjectFilter::dependsglob(const std::string &s, bool neg) {
	return mk_unique<ObjFilt>(neg, [s](const Elf &elf) {
		for (auto &i : elf.needed) {
			if (match_glob(s, 0, i, 0))
				return true;
		}
		return false;
	});
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
static shared_ptr<dtor_ptr<regex_t>>
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

	return shared_ptr<dtor_ptr<regex_t>>(new dtor_ptr<regex_t>(regex.release(), [](regex_t *r) {
		regfree(r);
	}));
}

// Package Name (Regex)
unique_ptr<PackageFilter>
PackageFilter::nameregex(const std::string &pattern,
                         bool ext, bool icase, bool neg)
{
	auto regex = make_regex(pattern, ext, icase);
	if (!regex)
		return nullptr;
	return mk_unique<PkgFilt>(neg, [regex](const Package &pkg) {
		regmatch_t rm;
		return 0 == regexec(regex->get(), pkg.name.c_str(), 0, &rm, 0);
	});
}

unique_ptr<PackageFilter>
PackageFilter::groupregex(const std::string &pattern,
                          bool ext, bool icase, bool neg)
{
	auto regex = make_regex(pattern, ext, icase);
	if (!regex)
		return nullptr;
	return mk_unique<PkgFilt>(neg, [regex](const Package &pkg) {
		regmatch_t rm;
		for (auto &i : pkg.groups) {
			if (0 == regexec(regex->get(), i.c_str(), 0, &rm, 0))
				return true;
		}
		return false;
	});
}

unique_ptr<PackageFilter>
PackageFilter::dependsregex(const std::string &pattern,
                          bool ext, bool icase, bool neg)
{
	auto regex = make_regex(pattern, ext, icase);
	if (!regex)
		return nullptr;
	return mk_unique<PkgFilt>(neg, [regex](const Package &pkg) {
		regmatch_t rm;
		for (auto &i : pkg.depends) {
			if (0 == regexec(regex->get(), i.c_str(), 0, &rm, 0))
				return true;
		}
		return false;
	});
}

unique_ptr<ObjectFilter>
ObjectFilter::nameregex(const std::string &pattern,
                        bool ext, bool icase, bool neg)
{
	auto regex = make_regex(pattern, ext, icase);
	if (!regex)
		return nullptr;
	return mk_unique<ObjFilt>(neg, [regex](const Elf &elf) {
		regmatch_t rm;
		return 0 == regexec(regex->get(), elf.basename.c_str(), 0, &rm, 0);
	});
}

unique_ptr<ObjectFilter>
ObjectFilter::dependsregex(const std::string &pattern,
                           bool ext, bool icase, bool neg)
{
	auto regex = make_regex(pattern, ext, icase);
	if (!regex)
		return nullptr;
	return mk_unique<ObjFilt>(neg, [regex](const Elf &elf) {
		regmatch_t rm;
		for (auto &lib : elf.needed) {
			if (0 == regexec(regex->get(), lib.c_str(), 0, &rm, 0))
				return true;
		}
		return false;
	});
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
