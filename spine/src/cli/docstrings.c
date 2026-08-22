/*
 * docstrings.c — `logan-spine-mcp docstrings [--all] <file>...`
 *
 * Parses each file with the single-file extractor and prints one line per
 * missing docstring: "path:line kind name". Kinds: file, function, method,
 * class, struct, interface, enum, type, trait. Default reports exported
 * symbols only (what the extractor records as exported — see --help); --all
 * reports every symbol. Exit 0 = nothing missing, 1 = something missing,
 * 2 = usage error or unreadable file. No index, no daemon, no database.
 */

#include "cli/cli.h"
#include "discover/discover.h"
#include "lsm.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static const char *USAGE =
    "Usage: logan-spine-mcp docstrings [--all] <file>...\n"
    "  Prints 'path:line kind name' for each missing docstring; exit 1 if any, 0 if none.\n"
    "  Checks Python, Go, JavaScript, TypeScript, TSX, Java, C#, Kotlin, Rust, C, C++;\n"
    "  other files are skipped. Default = exported symbols only: Go/Java/C#/Kotlin by an\n"
    "  uppercase first letter (a name heuristic, not the language's visibility modifiers),\n"
    "  Python by a name not starting with `_`; JS/TS functions with `export` (classes always);\n"
    "  Rust/C/C++ report all symbols. --all reports all symbols in every language.\n";

static bool reportable_label(const char *label) {
    static const char *ok[] = {"Function", "Method", "Class", "Struct", "Interface",
                               "Enum",     "Type",   "Trait", NULL};
    for (int i = 0; ok[i]; i++)
        if (strcmp(label, ok[i]) == 0)
            return true;
    return false;
}

static bool is_js(LSMLanguage l) {
    return l == LSM_LANG_JAVASCRIPT || l == LSM_LANG_TYPESCRIPT || l == LSM_LANG_TSX;
}

/* Languages with a file-level docstring rule and a per-symbol docstring concept. */
static bool checked_language(LSMLanguage l) {
    return l == LSM_LANG_PYTHON || l == LSM_LANG_GO || is_js(l) || l == LSM_LANG_JAVA ||
           l == LSM_LANG_CSHARP || l == LSM_LANG_KOTLIN || l == LSM_LANG_RUST || l == LSM_LANG_C ||
           l == LSM_LANG_CPP;
}

/* JS/TS record `export` on functions as is_entry_point (also set for a function named
 * `main`); classes carry no export signal there and are always considered exported. */
static bool considered_exported(const LSMDefinition *d, LSMLanguage lang) {
    if (is_js(lang) && strcmp(d->label, "Function") == 0)
        return d->is_entry_point;
    return d->is_exported;
}

static void print_kind(const char *label) {
    for (const char *p = label; *p; p++)
        putchar(tolower((unsigned char)*p));
}

static char *slurp(const char *path, long *len) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) {
        fclose(f);
        return NULL;
    }
    char *buf = malloc((size_t)n + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    *len = (long)fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[*len] = '\0';
    return buf;
}

/* Returns 0 = complete or skipped, 1 = findings, 2 = unreadable. */
static int check_file(const char *path, bool all) {
    LSMLanguage lang = lsm_language_for_filename(path);
    if (lang == LSM_LANG_COUNT || !checked_language(lang))
        return 0;
    long len = 0;
    char *src = slurp(path, &len);
    if (!src) {
        fprintf(stderr, "docstrings: cannot read %s\n", path);
        return 2;
    }
    LSMFileResult *r = lsm_extract_file(src, (int)len, lang, "docstrings", path, 0, NULL, NULL);
    if (!r) {
        printf("%s:0 error extractor returned nothing\n", path);
        free(src);
        return 0;
    }
    int rc = 0;
    if (r->has_error) {
        printf("%s:0 error %s\n", path, r->error_msg ? r->error_msg : "parse failed");
    }
    if (!r->file_docstring) {
        printf("%s:1 file %s\n", path, path);
        rc = 1;
    }
    for (int i = 0; i < r->defs.count; i++) {
        const LSMDefinition *d = &r->defs.items[i];
        if (!reportable_label(d->label) || d->docstring)
            continue;
        if (!all && !considered_exported(d, lang))
            continue;
        printf("%s:%d ", path, (int)d->start_line);
        print_kind(d->label);
        printf(" %s\n", d->name ? d->name : "?");
        rc = 1;
    }
    lsm_free_result(r);
    free(src);
    return rc;
}

int lsm_cmd_docstrings(int argc, char **argv) {
    bool all = false;
    int first = 0;
    for (; first < argc; first++) {
        if (strcmp(argv[first], "--all") == 0) {
            all = true;
        } else if (strcmp(argv[first], "--help") == 0 || strcmp(argv[first], "-h") == 0) {
            fputs(USAGE, stdout);
            return 0;
        } else if (argv[first][0] == '-') {
            fprintf(stderr, "docstrings: unknown option %s\n%s", argv[first], USAGE);
            return 2;
        } else {
            break;
        }
    }
    if (first >= argc) {
        fputs(USAGE, stderr);
        return 2;
    }
    int worst = 0;
    for (int i = first; i < argc; i++) {
        int rc = check_file(argv[i], all);
        if (rc == 2)
            return 2;
        if (rc > worst)
            worst = rc;
    }
    return worst;
}
