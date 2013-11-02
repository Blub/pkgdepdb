#ifdef WITH_REGEX
# include <sys/types.h>
# include <regex.h>
#endif

#include "main.h"

namespace filter {

PackageFilter::PackageFilter(bool neg_)
: negate(neg_)
{}

PackageFilter::~PackageFilter()
{}

class PackageName : public PackageFilter {
public:
	std::string name;
	PackageName(bool neg, const std::string &name_)
	: PackageFilter(neg), name(name_) {}
	virtual bool visible(const Package &pkg) const;
};

unique_ptr<PackageFilter>
PackageFilter::name(const std::string &s, bool neg) {
	return unique_ptr<PackageFilter>(new PackageName(neg, s));
}

class PackageNameGlob : public PackageFilter {
public:
	std::string name;
	PackageNameGlob(bool neg, const std::string &name_)
	: PackageFilter(neg), name(name_) {}
	virtual bool visible(const Package &pkg) const;
};

unique_ptr<PackageFilter>
PackageFilter::nameglob(const std::string &s, bool neg) {
	return unique_ptr<PackageFilter>(new PackageNameGlob(neg, s));
}

bool
PackageName::visible(const Package &pkg) const
{
	return (pkg.name == this->name);
}

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

bool
PackageNameGlob::visible(const Package &pkg) const
{
	return match_glob(name, 0, pkg.name, 0);
}

#ifdef WITH_REGEX
class PackageNameRegex : public PackageFilter {
public:
	std::string         pattern;
	unique_ptr<regex_t> regex;
	PackageNameRegex(bool neg, const std::string &pattern_, unique_ptr<regex_t> &&regex_)
	: PackageFilter(neg), pattern(pattern_), regex(move(regex_)) {}
	virtual bool visible(const Package &pkg) const;

	~PackageNameRegex() {
		regfree(regex.get());
	}
};

unique_ptr<PackageFilter>
PackageFilter::nameregex(const std::string &pattern, bool ext, bool icase, bool neg) {
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

	return unique_ptr<PackageFilter>(new PackageNameRegex(neg, pattern, move(regex)));
}

bool
PackageNameRegex::visible(const Package &pkg) const
{
	regmatch_t rm;
	return 0 == regexec(regex.get(), pkg.name.c_str(), 0, &rm, 0);
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
