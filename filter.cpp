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

Match::Match() {}
Match::~Match() {}

class ExactMatch : public Match {
 public:
  string text_;
  ExactMatch(string&&);
  bool operator()(const string&) const override;
};

class GlobMatch : public Match {
 public:
  string glob_;
  GlobMatch(string&&);
  bool operator()(const string&) const override;
};

ExactMatch::ExactMatch(string &&text)
: text_(move(text)) {}

GlobMatch::GlobMatch(string &&glob)
: glob_(move(glob)) {}

rptr<Match> Match::CreateExact(string &&text) {
  return new ExactMatch(move(text));
}

rptr<Match> Match::CreateGlob (string &&text) {
  return new GlobMatch(move(text));
}

#ifdef WITH_REGEX
class RegexMatch : public Match {
 public:
  string  pattern_;
  bool    icase_;
  regex_t regex_;
  bool    compiled_;
  RegexMatch(string&&, bool icase);
  ~RegexMatch();
  bool operator()(const string&) const override;
};

RegexMatch::RegexMatch(string &&pattern, bool icase)
: pattern_ (move(pattern)),
  icase_   (icase),
  compiled_(false)
{
  int cflags = REG_NOSUB | REG_EXTENDED;
  if (icase) cflags |= REG_ICASE;

  int err;
  if ( (err = ::regcomp(&regex_, pattern_.c_str(), cflags)) != 0) {
    char buf[4096];
    regerror(err, &regex_, buf, sizeof(buf));
    log(Error, "failed to compile regex (flags: %s): %s\n",
        (icase ? "case insensitive" : "case sensitive"),
        pattern.c_str());
    log(Error, "regex error: %s\n", buf);
    return;
  }
  compiled_ = true;
}

RegexMatch::~RegexMatch() {
  regfree(&regex_);
}

rptr<Match> Match::CreateRegex(string &&text, bool icase) {
  auto match = mk_rptr<RegexMatch>(move(text), icase);
  if (!match->compiled_)
    return nullptr;
  return match.release();
}
#endif

PackageFilter::PackageFilter(bool negate)
: negate_(negate)
{}

PackageFilter::~PackageFilter()
{}

ObjectFilter::ObjectFilter(bool negate)
: negate_(negate)
{}

ObjectFilter::~ObjectFilter()
{}

StringFilter::StringFilter(bool negate)
: negate_(negate)
{}

StringFilter::~StringFilter()
{}

// general purpose package filter
class PkgFilt : public PackageFilter {
 public:
  function<bool(const DB&, const Package&)> func;

  PkgFilt(bool neg, function<bool(const DB&, const Package&)> &&fn)
  : PackageFilter(neg), func(move(fn)) {}

  PkgFilt(bool neg, function<bool(const Package&)> &&fn)
  : PkgFilt(neg, [fn] (const DB &db, const Package &pkg) -> bool {
    (void)db;
    return fn(pkg);
  }) {}

  virtual bool visible(const DB &db, const Package &pkg) const {
    return func(db, pkg);
  }
};

// general purpose object filter
class ObjFilt : public ObjectFilter {
 public:
  function<bool(const Elf&)> func;

  ObjFilt(bool neg, function<bool(const Elf&)> &&fn)
  : ObjectFilter(neg), func(move(fn)) {}

  virtual bool visible(const Elf &elf) const {
    return func(elf);
  }
};

// general purpose string filter
class StrFilt : public StringFilter {
 public:
  function<bool(const string&)> func;

  StrFilt(bool neg, function<bool(const string&)> &&fn)
  : StringFilter(neg), func(move(fn)) {}

  virtual bool visible(const string &str) const {
    return func(str);
  }
};

// Utility functions:

static bool match_glob(const string &glob, size_t g,
                       const string &str,  size_t s)
// tail recursive
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

unique_ptr<PackageFilter> PackageFilter::name(rptr<Match> matcher, bool neg) {
  return mk_unique<PkgFilt>(neg, [matcher](const Package &pkg) {
    return (*matcher)(pkg.name_);
  });
}

template<typename CONT>
static unique_ptr<PackageFilter>
make_pkgfilter(rptr<Match> matcher, bool neg, CONT (Package::*member)) {
  return mk_unique<PkgFilt>(neg, [matcher,member](const Package &pkg) {
    for (auto &i : pkg.*member) {
      if ((*matcher)(i))
        return true;
    }
    return false;
  });
}
#define MAKE_PKGFILTER(NAME,VAR)                      \
unique_ptr<PackageFilter>                             \
PackageFilter::NAME(rptr<Match> matcher, bool neg) {  \
  return make_pkgfilter(matcher, neg, &Package::VAR##_); \
}

#define MAKE_PKGFILTER1(NAME) MAKE_PKGFILTER(NAME,NAME)

MAKE_PKGFILTER(group,groups)
MAKE_PKGFILTER1(depends)
MAKE_PKGFILTER1(optdepends)
MAKE_PKGFILTER1(provides)
MAKE_PKGFILTER1(conflicts)
MAKE_PKGFILTER1(replaces)
MAKE_PKGFILTER(contains,filelist)

#undef MAKE_PKGFILTER
#undef MAKE_PKGFILTER1

unique_ptr<PackageFilter>
PackageFilter::alldepends(rptr<Match> matcher, bool neg) {
  return mk_unique<PkgFilt>(neg, [matcher](const Package &pkg) {
    for (auto &i : pkg.depends_)
      if ((*matcher)(i))
        return true;
    for (auto &i : pkg.optdepends_)
      if ((*matcher)(i))
        return true;
    return false;
  });
}

static
unique_ptr<PackageFilter>
PkgLibFilter(unique_ptr<ObjectFilter> depfilter, bool neg) {
  if (!depfilter)
    return nullptr;
  rptr<ObjectFilter> libfilter(depfilter.release());
  if (!libfilter)
    return nullptr;
  return mk_unique<PkgFilt>(neg, [libfilter](const Package &pkg) {
    for (auto &e : pkg.objects_)
      if (libfilter->visible(*e))
        return true;
    return false;
  });
}

#define MAKE_PKGLIBFILTER(NAME)                                 \
unique_ptr<PackageFilter>                                       \
PackageFilter::pkglib##NAME(rptr<Match> matcher, bool neg) {    \
  return PkgLibFilter(ObjectFilter::NAME(matcher, false), neg); \
}

MAKE_PKGLIBFILTER(depends)
MAKE_PKGLIBFILTER(rpath)
MAKE_PKGLIBFILTER(runpath)
MAKE_PKGLIBFILTER(interp)

#undef MAKE_PKGLIBFILTER

unique_ptr<PackageFilter> PackageFilter::broken(bool neg) {
  return mk_unique<PkgFilt>(neg, [](const DB &db, const Package &pkg) {
    return db.IsBroken(&pkg);
  });
}

// general purpose object filter
unique_ptr<ObjectFilter> ObjectFilter::name(rptr<Match> matcher, bool neg) {
  return mk_unique<ObjFilt>(neg, [matcher](const Elf &elf) {
    return (*matcher)(elf.basename_);
  });
}

unique_ptr<ObjectFilter> ObjectFilter::path(rptr<Match> matcher, bool neg) {
  return mk_unique<ObjFilt>(neg, [matcher](const Elf &elf) {
    string p(elf.dirname_); p.append(1, '/'); p.append(elf.basename_);
    return (*matcher)(p);
  });
}

unique_ptr<ObjectFilter> ObjectFilter::depends(rptr<Match> matcher, bool neg) {
  return mk_unique<ObjFilt>(neg, [matcher](const Elf &elf) {
    for (auto &i : elf.needed_)
      if ((*matcher)(i))
        return true;
    return false;
  });
}

unique_ptr<ObjectFilter> ObjectFilter::rpath(rptr<Match> matcher, bool neg) {
  return mk_unique<ObjFilt>(neg, [matcher](const Elf &elf) {
    return elf.rpath_set_ && (*matcher)(elf.rpath_);
  });
}

unique_ptr<ObjectFilter> ObjectFilter::runpath(rptr<Match> matcher, bool neg) {
  return mk_unique<ObjFilt>(neg, [matcher](const Elf &elf) {
    return elf.runpath_set_ && (*matcher)(elf.runpath_);
  });
}

unique_ptr<ObjectFilter> ObjectFilter::interp(rptr<Match> matcher, bool neg) {
  return mk_unique<ObjFilt>(neg, [matcher](const Elf &elf) {
    return elf.interpreter_set_ && (*matcher)(elf.interpreter_);
  });
}

// string filter
unique_ptr<StringFilter> StringFilter::filter(rptr<Match> matcher, bool neg) {
  return mk_unique<StrFilt>(neg, [matcher](const string &str) {
    return (*matcher)(str);
  });
}

bool ExactMatch::operator()(const string &other) const {
  return text_ == other;
}

bool GlobMatch::operator()(const string &other) const {
  return match_glob(glob_, 0, other, 0);
}

#ifdef WITH_REGEX
bool RegexMatch::operator()(const string &other) const {
  regmatch_t rm;
  return 0 == regexec(&regex_, other.c_str(), 0, &rm, 0);
}
#endif

} // namespace filter

#ifdef TEST
#include <iostream>
using filter::match_glob;
int main() {
  string text("This is a stupid text.");
  int r=0;

  auto tryglob = [&](const char *c, bool expect) {
    if (match_glob(c, 0, text, 0) != expect) {
      std::cout << "FAIL: " << c << ": "
                << (expect ? "TRUE" : "FALSE") << " expected." << std::endl;
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
