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
		// let's group them...
		size_t i = 0;
		for (i = 0; i != pkg->objects.size()-1; ++i) {
			if ((i & 0xF) == 0)
				fprintf(out, "\n\t\t\t\t");
			fprintf(out, "%lu, ", pkg->objects[i]->json.id);
		}
		if ((i & 0xF) == 0)
			fprintf(out, "%s\n\t\t\t\t", (i ? "" : ","));
		fprintf(out, "%lu\n\t\t\t]", pkg->objects[i]->json.id);
		sep = ",\n";
	}

	fprintf(out, "\n\t\t}");
}

template<typename T, typename ITER>
static inline void
ForOff(T &t, size_t start, std::function<bool(ITER)> op) {
	if (start >= t.size())
		return;
	for (auto iter = t.begin() + start; iter != t.end(); ++iter) {
		if (!op(iter))
			break;
	}
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

	fprintf(out, "\n\t]\n}\n");
	return true;
}
