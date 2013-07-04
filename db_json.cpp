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

bool
db_store_json(DB *db, const std::string& filename)
{
    FILE *out = fopen(filename.c_str(), "wb");
    guard close_out([out]() { fclose(out); });

    size_t id = 0;
    fprintf(out, "{\n"
                 "\t\"objects\": [");
    bool comma = false;
    for (auto &obj : db->objects) {
        if (comma) fputc(',', out);
        comma = true;
        json_obj(id++, out, obj);
    }
    fprintf(out, "\n\t]\n}\n");
    return true;
}
