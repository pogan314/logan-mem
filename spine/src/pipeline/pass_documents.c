/*
 * pass_documents.c — DOCUMENTS edges from Markdown sections to the code they name.
 *
 * For every Section node, re-read its source lines (start_line..end_line) and
 * find `backticked` tokens. A path-like token links to File nodes whose path ends
 * with it; any other token links to Function/Method/type nodes whose simple name
 * matches its last `.`/`::` segment. A token with more than 5 candidates links to
 * none (ambiguity is not worth guessing). Runs in every mode, on both the
 * sequential and the parallel path, before the semantic pass.
 */
#include "foundation/constants.h"
#include "pipeline/pipeline.h"
#include "pipeline/pipeline_internal.h"
#include "graph_buffer/graph_buffer.h"
#include "discover/discover.h"
#include "lsm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

enum { DOC_MAX_TARGETS = 5, DOC_MAX_TOKEN = 200, DOC_MAX_TOKENS_PER_SECTION = 64 };

static bool is_code_label(const char *label) {
    static const char *ok[] = {"Function", "Method", "Class", "Struct", "Interface",
                               "Enum",     "Type",   "Trait", NULL};
    for (int i = 0; ok[i]; i++)
        if (strcmp(label, ok[i]) == 0)
            return true;
    return false;
}

static bool ends_with(const char *s, const char *suffix) {
    size_t ls = strlen(s), lx = strlen(suffix);
    return ls >= lx && strcmp(s + ls - lx, suffix) == 0;
}

/* Read whole file into a malloc'd NUL-terminated buffer; NULL on failure. */
static char *read_file(const char *repo, const char *rel) {
    char path[LSM_SZ_4K];
    snprintf(path, sizeof(path), "%s/%s", repo, rel);
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
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}

/* Pointer to the start of 1-based line `line` in buf, or NULL past EOF. */
static const char *line_start(const char *buf, int line) {
    const char *p = buf;
    for (int i = 1; i < line && p; i++) {
        p = strchr(p, '\n');
        if (p)
            p++;
    }
    return p;
}

static void link_token(lsm_gbuf_t *gb, int64_t section_id, const char *tok) {
    bool pathlike = strchr(tok, '/') != NULL || lsm_language_for_filename(tok) != LSM_LANG_COUNT;
    const char *key = tok;
    if (pathlike) {
        const char *slash = strrchr(tok, '/');
        key = slash ? slash + 1 : tok;
    } else {
        const char *dot = strrchr(tok, '.');
        const char *colons = strstr(tok, "::");
        while (colons) {
            const char *nx = strstr(colons + 2, "::");
            if (!nx)
                break;
            colons = nx;
        }
        if (colons && (!dot || colons > dot))
            key = colons + 2;
        else if (dot)
            key = dot + 1;
    }
    if (!key[0])
        return;

    const lsm_gbuf_node_t **nodes = NULL;
    int count = 0;
    if (lsm_gbuf_find_by_name(gb, key, &nodes, &count) != 0 || count == 0)
        return;

    int64_t targets[DOC_MAX_TARGETS];
    int nt = 0;
    for (int i = 0; i < count; i++) {
        const lsm_gbuf_node_t *n = nodes[i];
        bool ok = pathlike ? (strcmp(n->label, "File") == 0 && ends_with(n->file_path, tok))
                           : is_code_label(n->label);
        if (!ok)
            continue;
        if (nt == DOC_MAX_TARGETS)
            return; /* more than 5 candidates: link none */
        targets[nt++] = n->id;
    }
    for (int i = 0; i < nt; i++)
        lsm_gbuf_insert_edge(gb, section_id, targets[i], "DOCUMENTS", "{}");
}

static void scan_section(lsm_gbuf_t *gb, int64_t section_id, const char *text, size_t len) {
    static char seen[DOC_MAX_TOKENS_PER_SECTION][DOC_MAX_TOKEN + 1];
    int nseen = 0;
    size_t i = 0;
    while (i < len) {
        if (text[i] != '`') {
            i++;
            continue;
        }
        size_t j = i + 1;
        while (j < len && text[j] != '`' && text[j] != '\n')
            j++;
        if (j >= len || text[j] != '`') {
            i = j;
            continue;
        }
        size_t tl = j - i - 1;
        if (tl > 0 && tl <= DOC_MAX_TOKEN && !memchr(text + i + 1, ' ', tl)) {
            char tok[DOC_MAX_TOKEN + 1];
            memcpy(tok, text + i + 1, tl);
            tok[tl] = '\0';
            bool dup = false;
            for (int k = 0; k < nseen && !dup; k++)
                dup = strcmp(seen[k], tok) == 0;
            if (!dup) {
                if (nseen < DOC_MAX_TOKENS_PER_SECTION)
                    strcpy(seen[nseen++], tok);
                link_token(gb, section_id, tok);
            }
        }
        i = j + 1;
    }
}

void lsm_pipeline_pass_documents(lsm_pipeline_ctx_t *ctx) {
    const lsm_gbuf_node_t **secs = NULL;
    int n = 0;
    if (!ctx || !ctx->gbuf || !ctx->repo_path ||
        lsm_gbuf_find_by_label(ctx->gbuf, "Section", &secs, &n) != 0 || n == 0)
        return;

    typedef struct {
        int64_t id;
        char *file;
        int start, end;
    } sec_t;
    sec_t *list = calloc((size_t)n, sizeof(sec_t));
    if (!list)
        return;
    for (int i = 0; i < n; i++) {
        list[i].id = secs[i]->id;
        list[i].file = strdup(secs[i]->file_path ? secs[i]->file_path : "");
        list[i].start = secs[i]->start_line;
        list[i].end = secs[i]->end_line;
    }

    char *cur_file = NULL, *cur_buf = NULL;
    for (int i = 0; i < n; i++) {
        if (!list[i].file)
            continue;
        if (!cur_file || strcmp(cur_file, list[i].file) != 0) {
            free(cur_buf);
            cur_buf = read_file(ctx->repo_path, list[i].file);
            cur_file = list[i].file;
        }
        if (!cur_buf)
            continue;
        const char *a = line_start(cur_buf, list[i].start + 1); /* body starts after the heading */
        const char *b = line_start(cur_buf, list[i].end + 1);
        if (!a)
            continue;
        size_t len = b ? (size_t)(b - a) : strlen(a);
        scan_section(ctx->gbuf, list[i].id, a, len);
    }
    free(cur_buf);
    for (int i = 0; i < n; i++)
        free(list[i].file);
    free(list);
}
