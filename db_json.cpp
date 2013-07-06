#include <stdio.h>

#include "main.h"

static void
json_quote(FILE *out, const std::string& str)
{
	fprintf(out, "\"");
	for (size_t i = 0; i != str.length(); ++i) {
		if (str[i] == '\n') {
			fputc('\\', out);
			fputc('n', out);
			continue;
		}
		if (str[i] == '\r') {
			fputc('\\', out);
			fputc('r', out);
			continue;
		}
		if (str[i] == '\"' || str[i] == '\\')
			fputc('\\', out);
		fputc(str[i], out);
	}
	fprintf(out, "\"");
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

	fprintf(out, "\n}\n");
	return true;
}
