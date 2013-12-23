#include <stdio.h>

#include "main.h"

static void
json_in_quote(FILE *out, const std::string& str)
{
	for (size_t i = 0; i != str.length(); ++i) {
		switch (str[i]) {
			case '"':  fputc('\\', out); fputc('"', out); break;
			case '\\': fputc('\\', out); fputc('\\', out); break;
			case '\b': fputc('\\', out); fputc('b', out); break;
			case '\f': fputc('\\', out); fputc('f', out); break;
			case '\n': fputc('\\', out); fputc('n', out); break;
			case '\r': fputc('\\', out); fputc('r', out); break;
			case '\t': fputc('\\', out); fputc('t', out); break;
			default:
				fputc(str[i], out);
				break;
		}
	}
}

static void
json_quote(FILE *out, const std::string& str)
{
	fprintf(out, "\"");
	json_in_quote(out, str);
	fprintf(out, "\"");
}

static void
print_objname(const Elf *obj)
{
	putchar('"');
	json_in_quote(stdout, obj->dirname);
	putchar('/');
	json_in_quote(stdout, obj->basename);
	putchar('"');
}

void
DB::show_packages_json(bool filter_broken,
                       bool filter_notempty,
                       const FilterList &pkg_filters,
                       const ObjFilterList &obj_filters)
{
	printf("{");
	if (filter_broken)
		printf("\n\t\"filters\": [ \"broken\" ],");
	else
		printf("\n\t\"filters\": [],");

	if (!packages.size()) {
		printf("\n\t\"packages\": []\n}\n");
		return;
	}

	printf("\n\t\"packages\": [");

	const char *mainsep = "\n\t\t";
	for (auto &pkg : packages) {
		if (!util::all(pkg_filters, *this, *pkg))
			continue;
		if (filter_broken && !is_broken(pkg))
			continue;
		if (filter_notempty && is_empty(pkg, obj_filters))
			continue;
		printf("%s{", mainsep); mainsep = ",\n\t\t";
		printf("\n\t\t\t\"name\": ");
		json_quote(stdout, pkg->name);
		printf(",\n\t\t\t\"version\": ");
		json_quote(stdout, pkg->version);
		if (opt_verbosity >= 1) {
			if (pkg->groups.size()) {
				printf(",\n\t\t\t\"groups\": [");
				const char *sep = "\n\t\t\t\t";
				for (auto &grp : pkg->groups) {
					printf("%s", sep); sep = ",\n\t\t\t\t";
					json_quote(stdout, grp);
				}
				printf("\n\t\t\t]");
			}
			if (pkg->depends.size()) {
				printf(",\n\t\t\t\"depends\": [");
				const char *sep = "\n\t\t\t\t";
				for (auto &dep : pkg->depends) {
					printf("%s", sep); sep = ",\n\t\t\t\t";
					json_quote(stdout, dep);
				}
				printf("\n\t\t\t]");
			}
			if (pkg->optdepends.size()) {
				printf(",\n\t\t\t\"optdepends\": [");
				const char *sep = "\n\t\t\t\t";
				for (auto &dep : pkg->optdepends) {
					printf("%s", sep); sep = ",\n\t\t\t\t";
					json_quote(stdout, dep);
				}
				printf("\n\t\t\t]");
			}
			if (filter_broken) {
				printf(",\n\t\t\t\"broken\": [");
				const char *sep = "\n\t\t\t\t";
				for (auto &obj : pkg->objects) {
					if (!util::all(obj_filters, *this, *obj))
						continue;
					if (!is_broken(obj))
						continue;
					if (opt_verbosity >= 2) {
						printf("%s{", sep); sep = ",\n\t\t\t\t";
						printf("\n\t\t\t\t\t\"object\": ");
						print_objname(obj);
						auto& list = obj->req_missing;
						if (!list.empty()) {
							printf(",\n\t\t\t\t\t\"misses\": [");
							const char *missep = "\n\t\t\t\t\t\t";
							for (auto &missing : list) {
								printf("%s", missep);
								missep = ",\n\t\t\t\t\t\t";
								json_quote(stdout, missing);
							}
							printf("\n\t\t\t\t\t]");
						}
						printf("\n\t\t\t\t}");
					} else {
						printf("%s", sep); sep = ",\n\t\t\t\t";
						print_objname(obj);
					}
				}
				printf("\n\t\t\t]");
			}
			else {
				if (!pkg->objects.size())
					printf(",\n\t\t\t\"contains\": []");
				else {
					printf(",\n\t\t\t\"contains\": [");
					const char *sep = "\n\t\t\t\t";
					for (auto &obj : pkg->objects) {
						if (!util::all(obj_filters, *this, *obj))
							continue;
						printf("%s", sep); sep = ",\n\t\t\t\t";
						print_objname(obj);
					}
					printf("\n\t\t\t]");
				}
			}
		}
		printf("\n\t\t}");
	}

	printf("\n\t]\n}\n");
}

void
DB::show_info_json()
{
	printf("{");
	printf( "\n\t\"db_version\": %u", (unsigned)loaded_version);
	printf(",\n\t\"db_name\": "); json_quote(stdout, name);
	printf(",\n\t\"strict\": %s", (strict_linking ? "true" : "false"));
	printf(",\n\t\"library_path\": [");
	if (!library_path.size()) {
		printf("]\n}\n");
		return;
	}
	size_t i = 0;
	while (i != library_path.size()) {
		printf("\n\t\t");
		json_quote(stdout, library_path[i]);
		if (++i != library_path.size()) {
			printf(" // %u", (unsigned)i);
			break;
		}
		printf(", // %u", (unsigned)(i-1));
	}
	printf("\n\t]");

	unsigned id;
	if (ignore_file_rules.size()) {
		printf(",\n\t\"ignore_files\": [");
		id = 0;
		for (auto &p : ignore_file_rules) {
			printf("\n\t\t");
			json_quote(stdout, p);
			if (id+1 == ignore_file_rules.size())
				printf(" // %u", id++);
			else
				printf(", // %u", id++);
		}
		printf("\n\t]");
	}
	if (assume_found_rules.size()) {
		printf(",\n\t\"assume_found\": [");
		id = 0;
		for (auto &p : assume_found_rules) {
			printf("\n\t\t");
			json_quote(stdout, p);
			if (id+1 == assume_found_rules.size())
				printf(" // %u", id++);
			else
				printf(", // %u", id++);
		}
		printf("\n\t]");
	}
	const char *sep;
	if (package_library_path.size()) {
		printf(",\n\t\"package_libray_paths\": {");
		sep = "\n\t\t";
		for (auto &iter : package_library_path) {
			printf("%s", sep); sep = ",\n\t\t";
			json_quote(stdout, iter.first);
			printf(": [");
			const char *psep = "\n\t\t\t";
			for (auto &path : iter.second) {
				printf("%s", psep); psep = ",\n\t\t\t";
				json_quote(stdout, path);
			}
			printf("\n\t\t]");
		}
		printf("\n\t}");
	}
	if (base_packages.size()) {
		printf(",\n\t\"base_packages\": [");
		id = 0;
		for (auto &p : base_packages) {
			printf("\n\t\t");
			json_quote(stdout, p);
			if (id+1 == base_packages.size())
				printf(" // %u", id++);
			else
				printf(", // %u", id++);
		}
		printf("\n\t]");
	}

	printf("\n}\n");
}

void
DB::show_objects_json(const FilterList &pkg_filters, const ObjFilterList &obj_filters)
{
	if (!objects.size()) {
		printf("{ \"objects\": [] }\n");
		return;
	}

	printf("{ \"objects\": [");
	const char *mainsep = "\n\t";
	for (auto &obj : objects) {
		if (!util::all(obj_filters, *this, *obj))
			continue;
		if (pkg_filters.size() && (!obj->owner || !util::all(pkg_filters, *this, *obj->owner)))
			continue;
		printf("%s{\n\t\t\"file\":  ", mainsep); mainsep = ",\n\t";
		print_objname(obj);
		if (opt_verbosity < 1) {
			printf("\n\t}");
			continue;
		}
		do {
			printf("\n\t\t\"class\": %u, // %s", (unsigned)obj->ei_class, obj->classString());
			printf("\n\t\t\"data\":  %u, // %s", (unsigned)obj->ei_data,  obj->dataString());
			if (opt_verbosity >= 2 || obj->rpath_set || obj->runpath_set)
				printf("\n\t\t\"osabi\": %u, // %s", (unsigned)obj->ei_osabi, obj->osabiString());
			else
				printf("\n\t\t\"osabi\": %u  // %s", (unsigned)obj->ei_osabi, obj->osabiString());
			if (obj->rpath_set) {
				printf(",\n\t\t\"rpath\": ");
				json_quote(stdout, obj->rpath);
			}
			if (obj->runpath_set) {
				printf(",\n\t\t\"runpath\": ");
				json_quote(stdout, obj->runpath);
			}
			if (opt_verbosity < 2) {
				printf("\n\t}");
				break;
			}
			printf(",\n\t\t\"finds\": ["); {
				auto &set = obj->req_found;
				const char *sep = "\n\t\t\t";
				for (auto &found : set) {
					printf("%s", sep); sep = ",\n\t\t\t";
					print_objname(found);
				}
			}
			printf("\n\t\t],\n\t\t\"misses\": ["); {
				auto &set = obj->req_missing;
				const char *sep = "\n\t\t\t";
				for (auto &miss : set) {
					printf("%s", sep); sep = ",\n\t\t\t";
					json_quote(stdout, miss);
				}
			}

			printf("\n\t\t]\n\t}");
		} while(0);
	}
	printf("\n] }\n");
}

void
DB::show_found_json()
{
	printf("{ \"found_objects\": {");
	const char *mainsep = "\n\t";
	for (const Elf *obj : objects) {
		if (obj->req_found.empty())
			continue;
		printf("%s", mainsep); mainsep = ",\n\t";
		print_objname(obj);
		printf(": [");

		const char *sep = "\n\t\t";
		for (auto &s : obj->req_found) {
			printf("%s", sep); sep = ",\n\t\t";
			json_quote(stdout, s->basename);
		}
		printf("\n\t]");
	}
	printf("\n} }\n");
}

void
DB::show_filelist_json(const FilterList &pkg_filters,
                       const StrFilterList &str_filters)
{
	printf("{ \"filelist\": [");
	const char *mainsep = "\n\t";
	for (auto &pkg : packages) {
		if (!util::all(pkg_filters, *this, *pkg))
			continue;
		if (!opt_quiet) {
			printf("%s", mainsep); mainsep = ",\n\t";
			json_quote(stdout, pkg->name);
			printf(": [");
		}

		const char *sep = "\n\t\t";
		for (auto &file : pkg->filelist) {
			if (!util::all(str_filters, file))
				continue;
			if (!opt_quiet) {
				printf("%s", sep); sep = ",\n\t\t";
				json_quote(stdout, file);
			} else {
				printf("%s", mainsep); mainsep = ",\n\t";
				json_quote(stdout, file);
			}
		}
		if (!opt_quiet)
			printf("\n\t]");
	}
	printf("\n] }\n");
}

void
DB::show_missing_json()
{
	printf("{ \"missing_objects\": {");
	const char *mainsep = "\n\t";
	for (const Elf *obj : objects) {
		if (obj->req_missing.empty())
			continue;
		printf("%s", mainsep); mainsep = ",\n\t";
		print_objname(obj);
		printf(": [");

		const char *sep = "\n\t\t";
		for (auto &s : obj->req_missing) {
			printf("%s", sep); sep = ",\n\t\t";
			json_quote(stdout, s);
		}
		printf("\n\t]");
	}
	printf("\n} }\n");
}

static void
json_obj(size_t id, FILE *out, const Elf *obj)
{
	fprintf(out, "\n\t\t{\n"
	             "\t\t\t\"id\": %lu", (unsigned long)id);

	fprintf(out, ",\n\t\t\t\"dirname\": ");
	json_quote(out, obj->dirname);
	fprintf(out, ",\n\t\t\t\"basename\": ");
	json_quote(out, obj->basename);
	fprintf(out, ",\n\t\t\t\"ei_class\": %u", (unsigned)obj->ei_class);
	fprintf(out, ",\n\t\t\t\"ei_data\":  %u", (unsigned)obj->ei_data);
	fprintf(out, ",\n\t\t\t\"ei_osabi\": %u", (unsigned)obj->ei_osabi);
	if (obj->rpath_set) {
		fprintf(out, ",\n\t\t\t\"rpath\": ");
		json_quote(out, obj->rpath);
	}
	if (obj->runpath_set) {
		fprintf(out, ",\n\t\t\t\"runpath\": ");
		json_quote(out, obj->runpath);
	}
	if (obj->needed.size()) {
		fprintf(out, ",\n\t\t\t\"needed\": [");
		bool comma = false;
		for (auto &need : obj->needed) {
			if (comma) fputc(',', out);
			comma = true;
			fprintf(out, "\n\t\t\t\t");
			json_quote(out, need);
		}
		fprintf(out, "\n\t\t\t]");
	}

	fprintf(out, "\n\t\t}");
}

template<class OBJLIST>
static void
json_objlist(FILE *out, const OBJLIST &list)
{
	if (!list.size())
		return;
	// let's group them...
	size_t i = 0;
	size_t count = list.size()-1;
	auto iter = list.begin();
	for (; i != count; ++i, ++iter) {
		if ((i & 0xF) == 0)
			fprintf(out, "\n\t\t\t\t");
		fprintf(out, "%lu, ", (*iter)->json.id);
	}
	if ((i & 0xF) == 0)
		fprintf(out, "%s\n\t\t\t\t", (i ? "" : ","));
	fprintf(out, "%lu", (*iter)->json.id);
}

template<class STRLIST>
static void
json_strlist(FILE *out, const STRLIST &list)
{
	bool comma = false;
	for (auto &i : list) {
		if (comma) fputc(',', out);
		comma = true;
		fprintf(out, "\n\t\t\t\t");
		json_quote(out, i);
	}
}

static void
json_pkg(FILE *out, const Package *pkg)
{
	fprintf(out, "\n\t\t{");
	const char *sep = "\n";
	if (pkg->name.size()) {
		fprintf(out, "%s\t\t\t\"name\": ", sep);
		json_quote(out, pkg->name);
		sep = ",\n";
	}
	if (pkg->version.size()) {
		fprintf(out, "%s\t\t\t\"version\": ", sep);
		json_quote(out, pkg->version);
		sep = ",\n";
	}
	if (pkg->objects.size()) {
		fprintf(out, "%s\t\t\t\"objects\": [", sep);
		json_objlist(out, pkg->objects);
		fprintf(out, "\n\t\t\t]");
		sep = ",\n";
	}

	fprintf(out, "\n\t\t}");
}

#if 0
static void
json_obj_found(FILE *out, const Elf *obj, const ObjectSet &found)
{
	fprintf(out, "\n\t\t{"
	             "\n\t\t\t\"obj\": %lu"
	             "\n\t\t\t\"found\": [",
	        (unsigned long)obj->json.id);
	json_objlist(out, found);
	fprintf(out, "\n\t\t\t]"
	             "\n\t\t}");
}

static void
json_obj_missing(FILE *out, const Elf *obj, const StringSet &missing)
{
	fprintf(out, "\n\t\t{"
	             "\n\t\t\t\"obj\": %lu"
	             "\n\t\t\t\"missing\": [",
	        (unsigned long)obj->json.id);
	json_strlist(out, missing);
	fprintf(out, "\n\t\t\t]"
	             "\n\t\t}");
}
#endif

bool
db_store_json(DB *db, const std::string& filename)
{
	FILE *out = fopen(filename.c_str(), "wb");
	if (!out) {
		log(Error, "failed to open file `%s' for reading\n", filename.c_str());
		return false;
	}

	log(Message, "writing json database file\n");

	guard close_out([out]() { fclose(out); });

	// we put the objects first as they don't directly depend on anything
	size_t id = 0;
	fprintf(out, "{\n"
	             "\t\"objects\": [");
	bool comma = false;
	for (auto &obj : db->objects) {
		if (comma) fputc(',', out);
		comma = true;
		obj->json.id = id;
		json_obj(id, out, obj);
		++id;
	}
	fprintf(out, "\n\t],\n"
	             "\t\"packages\": [");

	// packages have a list of objects
	// above we numbered them with IDs to reuse here now
	comma = false;
	for (auto &pkg : db->packages) {
		if (comma) fputc(',', out);
		comma = true;
		json_pkg(out, pkg);
	}

	fprintf(out, "\n\t]");

#if 0
	if (!db->required_found.empty()) {
		fprintf(out, ",\n\t\"found\": [");
		comma = false;
		for (auto &found : db->required_found) {
			if (comma) fputc(',', out);
			comma = true;
			json_obj_found(out, found.first, found.second);
		}
		fprintf(out, "\n\t]");
	}


	if (!db->required_missing.empty()) {
		fprintf(out, ",\n\t\"missing\": [");
		comma = false;
		for (auto &mis : db->required_missing) {
			if (comma) fputc(',', out);
			comma = true;
			json_obj_missing(out, mis.first, mis.second);
		}
		fprintf(out, "\n\t]");
	}
#endif

	fprintf(out, "\n}\n");
	return true;
}
