/*
 * pass_semantic.c — Semantic edge passes: INHERITS, DECORATES, IMPLEMENTS.
 *
 * Operates on already-extracted nodes in the graph buffer:
 *   - INHERITS: Class/Interface → base class (from base_classes in extraction)
 *   - DECORATES: Function/Method → decorator function (from decorators in extraction)
 *   - IMPLEMENTS: Struct/Class → Interface (Go implicit + explicit base_classes + Rust impl)
 *
 * These passes re-extract files to access base_classes, decorators, and impl_traits data
 * since that information is in CBMDefinition fields, not stored in node properties JSON.
 *
 * Depends on: pass_definitions having populated the registry and graph buffer
 */
#include "yyjson/yyjson.h"
#include "foundation/constants.h"
#include "foundation/mem_core.h"
#include "foundation/str_util.h" // cbm_json_escape
#include "pipeline/pipeline.h"
#include <stdint.h>
#include "pipeline/pipeline_internal.h"
#include "graph_buffer/graph_buffer.h"
#include "foundation/log.h"
#include "foundation/compat.h"
#include "foundation/compat_fs.h"
#include "foundation/limits.h"
#include "cbm.h"
#include "callable_sig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* True for languages whose module QN derives from the CONTAINING DIRECTORY
 * (Java/Go package). MUST match cbm_lang_module_is_dir() (internal/cbm/helpers.c)
 * so base-class / same-module resolution keys against the directory-based
 * def-node QNs. */
static bool ps_module_is_dir(CBMLanguage lang) {
    return lang == CBM_LANG_JAVA || lang == CBM_LANG_GO;
}

static char *read_file(const char *path, int *out_len) {
    *out_len = -1; /* unknown/read failure; zero is a proven empty file */
    FILE *f = cbm_fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    (void)fseek(f, 0, SEEK_END);
    long size = ftell(f);
    (void)fseek(f, 0, SEEK_SET);
    if (size <= 0 || size > cbm_max_file_bytes()) { /* generous, env-configurable cap (B4) */
        if (size == 0) {
            *out_len = 0;
        }
        (void)fclose(f);
        return NULL;
    }
    /* +pad: tree-sitter lexer lookahead reads past EOF; keep it in-bounds */
    enum { CBM_TS_LOOKAHEAD_PAD = 16 };
    char *buf = cbm_alloc(CBM_MEM_CLASS_SEMANTIC, (size_t)size + CBM_TS_LOOKAHEAD_PAD);
    if (!buf) {
        (void)fclose(f);
        return NULL;
    }
    size_t nread = fread(buf, SKIP_ONE, size, f);
    (void)fclose(f);
    if (nread > (size_t)size) {
        nread = (size_t)size;
    }
    memset(buf + nread, 0, CBM_TS_LOOKAHEAD_PAD);
    *out_len = (int)nread;
    return buf;
}

static const char *itoa_log(int val) {
    enum { RING_BUF_COUNT = 4, RING_BUF_MASK = 3 };
    static CBM_TLS char bufs[RING_BUF_COUNT][CBM_SZ_32];
    static CBM_TLS int idx = 0;
    int i = idx;
    idx = (idx + SKIP_ONE) & RING_BUF_MASK;
    snprintf(bufs[i], sizeof(bufs[i]), "%d", val);
    return bufs[i];
}

/* Build per-file import map from cached extraction result or graph buffer edges. */
static int build_import_map(cbm_pipeline_ctx_t *ctx, const char *rel_path,
                            const CBMFileResult *result, const char ***out_keys,
                            const char ***out_vals, int *out_count) {
    *out_keys = NULL;
    *out_vals = NULL;
    *out_count = 0;

    /* Fast path: build from cached extraction result (no JSON parsing) */
    if (result && result->imports.count > 0) {
        const char **keys =
            cbm_calloc(CBM_MEM_CLASS_SEMANTIC,
                       (size_t)((size_t)result->imports.count) * (sizeof(const char *)));
        const char **vals =
            cbm_calloc(CBM_MEM_CLASS_SEMANTIC,
                       (size_t)((size_t)result->imports.count) * (sizeof(const char *)));
        int count = 0;

        for (int i = 0; i < result->imports.count; i++) {
            const CBMImport *imp = &result->imports.items[i];
            if (!imp->local_name || !imp->local_name[0] || !imp->module_path) {
                continue;
            }
            char *target_qn = cbm_pipeline_fqn_module(ctx->project_name, imp->module_path);
            const cbm_gbuf_node_t *target = cbm_gbuf_find_by_qn(ctx->gbuf, target_qn);
            free(target_qn);
            if (!target) {
                continue;
            }
            keys[count] = cbm_mem_strdup(CBM_MEM_CLASS_SEMANTIC, imp->local_name);
            vals[count] = target->qualified_name;
            count++;
        }

        *out_keys = keys;
        *out_vals = vals;
        *out_count = count;
        return 0;
    }

    /* Slow path: scan graph buffer IMPORTS edges + parse JSON properties */
    char *file_qn = cbm_pipeline_fqn_compute(ctx->project_name, rel_path, "__file__");
    const cbm_gbuf_node_t *file_node = cbm_gbuf_find_by_qn(ctx->gbuf, file_qn);
    free(file_qn);
    if (!file_node) {
        return 0;
    }

    const cbm_gbuf_edge_t **edges = NULL;
    int edge_count = 0;
    int rc = cbm_gbuf_find_edges_by_source_type(ctx->gbuf, file_node->id, "IMPORTS", &edges,
                                                &edge_count);
    if (rc != 0 || edge_count == 0) {
        return 0;
    }

    const char **keys =
        cbm_calloc(CBM_MEM_CLASS_SEMANTIC, (size_t)(edge_count) * (sizeof(const char *)));
    const char **vals =
        cbm_calloc(CBM_MEM_CLASS_SEMANTIC, (size_t)(edge_count) * (sizeof(const char *)));
    int count = 0;

    for (int i = 0; i < edge_count; i++) {
        const cbm_gbuf_edge_t *e = edges[i];
        const cbm_gbuf_node_t *target = cbm_gbuf_find_by_id(ctx->gbuf, e->target_id);
        if (!target || !e->properties_json) {
            continue;
        }

        const char *start = strstr(e->properties_json, "\"local_name\":\"");
        if (start) {
            start += strlen("\"local_name\":\"");
            const char *end = strchr(start, '"');
            if (end && end > start) {
                keys[count] = cbm_strndup(start, end - start);
                vals[count] = target->qualified_name;
                count++;
            }
        }
    }

    *out_keys = keys;
    *out_vals = vals;
    *out_count = count;
    return 0;
}

static void free_import_map(const char **keys, const char **vals, int count) {
    if (keys) {
        for (int i = 0; i < count; i++) {
            cbm_free(CBM_MEM_CLASS_SEMANTIC, (void *)keys[i]);
        }
        cbm_free(CBM_MEM_CLASS_SEMANTIC, (void *)keys);
    }
    if (vals) {
        cbm_free(CBM_MEM_CLASS_SEMANTIC, (void *)vals);
    }
}

/* Resolve a class/type name through the registry. Returns borrowed QN or NULL. */
static const char *resolve_as_class(const cbm_registry_t *reg, const char *name,
                                    const char *module_qn, const char **imp_keys,
                                    const char **imp_vals, int imp_count, CBMLanguage lang) {
    cbm_resolution_t res =
        cbm_registry_resolve_lang(reg, name, module_qn, imp_keys, imp_vals, imp_count, lang);
    if (!res.qualified_name || res.qualified_name[0] == '\0') {
        return NULL;
    }

    /* Verify it's a type-like container (Class/Struct/Interface/Enum/Type/Trait):
     * a base/embedded type, impl receiver, or inheritance target must resolve to
     * one of these. Struct included so Rust/Go/Swift/D `impl Trait for S` and Go
     * struct embedding resolve. */
    const char *label = cbm_registry_label_of(reg, res.qualified_name);
    if (!cbm_label_is_type_like(label)) {
        return NULL;
    }
    return res.qualified_name;
}

/* Extract decorator function name from the raw attribute/annotation text.
 * Handles the per-language surface syntaxes:
 *   Python/TS:  "@app.route('/api')"   → "app.route"
 *   Java/Kotlin:"@Override"             → "Override"
 *   C#:         "[Log]" / "[Log(...)]"  → "Log"
 *   Rust:       "#[derive(Debug)]"      → "derive"
 *               "#[allow(dead_code)]"   → "allow"
 *   PHP 8:      "#[Route('/users')]"    → "Route"
 *   Swift:      "@discardableResult"    → "discardableResult"
 *   Scala:      "@deprecated(...)"      → "deprecated"
 * Returns the leading identifier path, stopping at the first '(' or '['
 * argument list and trimming any wrapping bracket/`@`/`#` punctuation. */
static void extract_decorator_func(const char *dec, char *out, size_t outsz) {
    out[0] = '\0';
    if (!dec) {
        return;
    }
    const char *start = dec;
    /* Strip leading sigils: '@' (Py/TS/JVM/Swift/Scala), '#' + '[' (Rust/PHP8),
     * or a bare '[' (C# attribute). */
    while (*start == '@' || *start == '#' || *start == '[' || *start == ' ') {
        start++;
    }
    /* The name ends at the first argument list ('(' or '[') or wrapper ']'. */
    size_t len = 0;
    while (start[len] && start[len] != '(' && start[len] != '[' && start[len] != ']' &&
           start[len] != ' ' && start[len] != ',') {
        len++;
    }
    if (len == 0 || len >= outsz) {
        return;
    }
    memcpy(out, start, len);
    out[len] = '\0';
}

/* ── Go-style implicit interface satisfaction ─────────────────── */

/* Check if file_path ends with a suffix. */
static bool fp_ends_with(const char *fp, const char *suffix) {
    if (!fp || !suffix) {
        return false;
    }
    size_t fplen = strlen(fp);
    size_t sflen = strlen(suffix);
    return fplen >= sflen && strcmp(fp + fplen - sflen, suffix) == 0;
}

/* Info about one interface method (name + node ID). */
typedef struct {
    const char *name;
    int64_t id;
} go_imethod_t;

/* Check if class has all interface methods and create IMPLEMENTS + OVERRIDE edges. */
static int check_go_class_implements(cbm_pipeline_ctx_t *ctx, const cbm_gbuf_node_t *cls,
                                     const cbm_gbuf_node_t *iface, const go_imethod_t *imethods,
                                     int im_count) {
    if (!cls->file_path || !cls->qualified_name) {
        return 0;
    }
    if (!fp_ends_with(cls->file_path, ".go")) {
        return 0;
    }
    /* Resolve the struct's methods two ways and use whichever finds them:
     *   (a) the struct's DEFINES_METHOD edges, matched by method NAME — the real
     *       pipeline path, where Go receiver methods carry a flat QN (e.g.
     *       "pkg.Area" rather than "pkg.Circle.Area"), so QN-string
     *       reconstruction would miss; and
     *   (b) the reconstructed QN "<ClassQN>.<methodName>" — used by tests and any
     *       extractor that does emit class-qualified method QNs without
     *       DEFINES_METHOD edges from the class. */
    const cbm_gbuf_edge_t **cls_dm = NULL;
    int cls_dm_count = 0;
    cbm_gbuf_find_edges_by_source_type(ctx->gbuf, cls->id, "DEFINES_METHOD", &cls_dm,
                                       &cls_dm_count);

    char prefix[CBM_SZ_512];
    snprintf(prefix, sizeof(prefix), "%s.", cls->qualified_name);

    /* For each interface method, find the matching struct method node. */
    const cbm_gbuf_node_t *matched[CBM_SZ_128];
    for (int m = 0; m < im_count; m++) {
        const cbm_gbuf_node_t *found = NULL;
        /* (a) DEFINES_METHOD edge by name */
        for (int d = 0; d < cls_dm_count; d++) {
            const cbm_gbuf_node_t *cm = cbm_gbuf_find_by_id(ctx->gbuf, cls_dm[d]->target_id);
            if (cm && cm->name && strcmp(cm->name, imethods[m].name) == 0) {
                found = cm;
                break;
            }
        }
        /* (b) reconstructed "<ClassQN>.<methodName>" */
        if (!found) {
            char method_qn[CBM_SZ_512];
            snprintf(method_qn, sizeof(method_qn), "%s%s", prefix, imethods[m].name);
            found = cbm_gbuf_find_by_qn(ctx->gbuf, method_qn);
        }
        if (!found) {
            return 0; /* struct does not satisfy the interface */
        }
        matched[m] = found;
    }
    cbm_gbuf_insert_edge(ctx->gbuf, cls->id, iface->id, "IMPLEMENTS", "{}");
    int edges = SKIP_ONE;
    for (int m = 0; m < im_count; m++) {
        cbm_gbuf_insert_edge(ctx->gbuf, matched[m]->id, imethods[m].id, "OVERRIDE", "{}");
        edges++;
    }
    return edges;
}

/* ── Method-name index over the concrete types ────────────────────
 *
 * Checking every interface against every struct and named type is
 * interfaces x types work: the scaling lane measured it at 4.00x per 2x corpus
 * (2026-09-17). A type can only satisfy an interface if it supplies EVERY
 * method name, so it is in the candidate list of each of those names: checking
 * just the shortest such list, in the original order, runs exactly the checks
 * that can succeed, in exactly the order they ran before -- the edges and their
 * insertion order are unchanged. A type supplies a name by either lookup
 * check_go_class_implements uses: a DEFINES_METHOD edge to a node of that name,
 * or a node whose QN is "<TypeQN>.<name>". */
typedef struct {
    int *items; /* ascending type indices, no duplicates once sealed */
    int count;
    int cap;
} go_type_list_t;

typedef struct {
    CBMHashTable *by_name;    /* method name (borrowed) -> go_type_list_t* */
    CBMHashTable *type_by_qn; /* type QN (borrowed) -> (void *)(index + 1) */
} go_method_index_t;

static void go_index_add(go_method_index_t *ix, const char *name, int type_index) {
    go_type_list_t *list = cbm_ht_get(ix->by_name, name);
    if (!list) {
        list = cbm_calloc(CBM_MEM_CLASS_SEMANTIC, sizeof(*list));
        if (!list) {
            return;
        }
        cbm_ht_set(ix->by_name, name, list);
    }
    if (list->count > 0 && list->items[list->count - 1] == type_index) {
        return;
    }
    if (list->count == list->cap) {
        int cap = list->cap ? list->cap * PAIR_LEN : CBM_SZ_4;
        int *grown = cbm_realloc(CBM_MEM_CLASS_SEMANTIC, list->items, (size_t)cap * sizeof(int));
        if (!grown) {
            return;
        }
        list->items = grown;
        list->cap = cap;
    }
    list->items[list->count++] = type_index;
}

static void go_index_visit_node(const cbm_gbuf_node_t *node, void *userdata) {
    go_method_index_t *ix = userdata;
    const char *qn = node->qualified_name;
    /* Last '.' of the base QN (a callable identity suffix never holds one). */
    const char *dot = NULL;
    size_t base_len = cbm_qn_callable_base_len_named(qn, node->name);
    for (size_t i = 0; qn && i < base_len; i++) {
        if (qn[i] == '.') {
            dot = qn + i;
        }
    }
    if (!dot || dot == qn || !dot[SKIP_ONE]) {
        return;
    }
    char parent[CBM_SZ_512];
    size_t len = (size_t)(dot - qn);
    if (len >= sizeof(parent)) {
        return; /* longer than the lookup buffer check_go_class_implements builds */
    }
    memcpy(parent, qn, len);
    parent[len] = '\0';
    void *slot = cbm_ht_get(ix->type_by_qn, parent);
    if (slot) {
        go_index_add(ix, dot + SKIP_ONE, (int)((intptr_t)slot - SKIP_ONE));
    }
}

static int cmp_int_asc(const void *a, const void *b) {
    int x = *(const int *)a;
    int y = *(const int *)b;
    return (x > y) - (x < y);
}

static void go_index_seal_list(const char *key, void *val, void *userdata) {
    (void)key;
    (void)userdata;
    go_type_list_t *list = val;
    if (list->count < PAIR_LEN) {
        return;
    }
    qsort(list->items, (size_t)list->count, sizeof(int), cmp_int_asc);
    int out = SKIP_ONE;
    for (int i = SKIP_ONE; i < list->count; i++) {
        if (list->items[i] != list->items[out - SKIP_ONE]) {
            list->items[out++] = list->items[i];
        }
    }
    list->count = out;
}

static void go_index_free_list(const char *key, void *val, void *userdata) {
    (void)key;
    (void)userdata;
    go_type_list_t *list = val;
    cbm_free(CBM_MEM_CLASS_SEMANTIC, list->items);
    cbm_free(CBM_MEM_CLASS_SEMANTIC, list);
}

int cbm_pipeline_implements_go(cbm_pipeline_ctx_t *ctx) {
    int edge_count = 0;

    /* Find all Interface nodes */
    const cbm_gbuf_node_t **ifaces = NULL;
    int iface_count = 0;
    if (cbm_gbuf_find_by_label(ctx->gbuf, "Interface", &ifaces, &iface_count) != 0) {
        return 0;
    }

    /* Find candidate concrete types. In Go the type that satisfies an interface
     * is a struct (now labelled "Struct") or a named type (labelled "Class"); both
     * sets are checked. Each call returns a borrowed internal array (no free). */
    const cbm_gbuf_node_t **classes = NULL;
    int class_count = 0;
    cbm_gbuf_find_by_label(ctx->gbuf, "Class", &classes, &class_count);
    const cbm_gbuf_node_t **structs = NULL;
    int struct_count = 0;
    cbm_gbuf_find_by_label(ctx->gbuf, "Struct", &structs, &struct_count);
    if (class_count == 0 && struct_count == 0) {
        return 0;
    }

    /* Candidate index (see go_method_index_t). Type index t is the position in
     * the original iteration order: structs first, then classes. */
    int type_count = struct_count + class_count;
    const cbm_gbuf_node_t **types =
        cbm_alloc(CBM_MEM_CLASS_SEMANTIC, (size_t)type_count * sizeof(*types));
    go_method_index_t ix = {
        .by_name = cbm_ht_create_in(CBM_MEM_CLASS_SEMANTIC, (uint32_t)CBM_SZ_1K),
        .type_by_qn = cbm_ht_create_in(CBM_MEM_CLASS_SEMANTIC, (uint32_t)type_count),
    };
    bool indexed = types && ix.by_name && ix.type_by_qn;
    if (indexed) {
        for (int c = 0; c < struct_count; c++) {
            types[c] = structs[c];
        }
        for (int c = 0; c < class_count; c++) {
            types[struct_count + c] = classes[c];
        }
        for (int t = 0; t < type_count; t++) {
            if (types[t]->qualified_name) {
                cbm_ht_set(ix.type_by_qn, types[t]->qualified_name,
                           (void *)(intptr_t)(t + SKIP_ONE));
            }
            const cbm_gbuf_edge_t **tdm = NULL;
            int tdm_count = 0;
            cbm_gbuf_find_edges_by_source_type(ctx->gbuf, types[t]->id, "DEFINES_METHOD", &tdm,
                                               &tdm_count);
            for (int d = 0; d < tdm_count; d++) {
                const cbm_gbuf_node_t *cm = cbm_gbuf_find_by_id(ctx->gbuf, tdm[d]->target_id);
                if (cm && cm->name) {
                    go_index_add(&ix, cm->name, t);
                }
            }
        }
        cbm_gbuf_foreach_node(ctx->gbuf, go_index_visit_node, &ix);
        cbm_ht_foreach(ix.by_name, go_index_seal_list, NULL);
    }

    for (int i = 0; i < iface_count; i++) {
        const cbm_gbuf_node_t *iface = ifaces[i];
        if (!iface->file_path || !fp_ends_with(iface->file_path, ".go")) {
            continue;
        }

        /* Get interface methods via DEFINES_METHOD edges */
        const cbm_gbuf_edge_t **dm_edges = NULL;
        int dm_count = 0;
        if (cbm_gbuf_find_edges_by_source_type(ctx->gbuf, iface->id, "DEFINES_METHOD", &dm_edges,
                                               &dm_count) != 0 ||
            dm_count == 0) {
            continue;
        }

        /* Collect interface method info */
        go_imethod_t imethods[CBM_SZ_128];
        int im_count = 0;
        for (int j = 0; j < dm_count && im_count < CBM_SZ_128; j++) {
            const cbm_gbuf_node_t *m = cbm_gbuf_find_by_id(ctx->gbuf, dm_edges[j]->target_id);
            if (m && m->name) {
                imethods[im_count++] = (go_imethod_t){m->name, m->id};
            }
        }
        if (im_count == 0) {
            continue;
        }

        if (!indexed) {
            /* Index unavailable (allocation failure): the full scan. */
            for (int c = 0; c < struct_count; c++) {
                edge_count += check_go_class_implements(ctx, structs[c], iface, imethods, im_count);
            }
            for (int c = 0; c < class_count; c++) {
                edge_count += check_go_class_implements(ctx, classes[c], iface, imethods, im_count);
            }
            continue;
        }
        /* Only the types that supply the interface's rarest method name. */
        const go_type_list_t *shortest = NULL;
        bool missing = false;
        for (int m = 0; m < im_count; m++) {
            const go_type_list_t *list = cbm_ht_get(ix.by_name, imethods[m].name);
            if (!list || list->count == 0) {
                missing = true; /* no type supplies this name: none can satisfy */
                break;
            }
            if (!shortest || list->count < shortest->count) {
                shortest = list;
            }
        }
        if (missing || !shortest) {
            continue;
        }
        for (int k = 0; k < shortest->count; k++) {
            edge_count += check_go_class_implements(ctx, types[shortest->items[k]], iface, imethods,
                                                    im_count);
        }
    }
    if (ix.by_name) {
        cbm_ht_foreach(ix.by_name, go_index_free_list, NULL);
        cbm_ht_free(ix.by_name);
    }
    cbm_ht_free(ix.type_by_qn);
    cbm_free(CBM_MEM_CLASS_SEMANTIC, types);
    return edge_count;
}

/* Build the QN of the synthetic node that stands in for an external/stdlib
 * decorator whose target is not a local symbol (Rust `#[derive(Debug)]`, Swift
 * `@discardableResult`, Scala `@deprecated`, Python `@cache`, Java `@Override`,
 * ...).  Namespaced with `<decorator:` so it can never collide with a real
 * symbol and so all uses of the same decorator name across a project dedupe to
 * one node by QN. */
static void synth_decorator_qn(const char *func_name, char *out, size_t outsz) {
    snprintf(out, outsz, "<decorator:%s>", func_name);
}

/* Process INHERITS + DECORATES edges for one definition. */
/* Resolve one decorator and create DECORATES edge. */
static void resolve_decorator(cbm_pipeline_ctx_t *ctx, const cbm_gbuf_node_t *node,
                              const char *decorator, const char *module_qn, const char **imp_keys,
                              const char **imp_vals, int imp_count, int *count, CBMLanguage lang) {
    char func_name[CBM_SZ_256];
    extract_decorator_func(decorator, func_name, sizeof(func_name));
    if (func_name[0] == '\0') {
        return;
    }
    cbm_resolution_t res = cbm_registry_resolve_lang(ctx->registry, func_name, module_qn, imp_keys,
                                                     imp_vals, imp_count, lang);
    if ((!res.qualified_name || res.qualified_name[0] == '\0') && !strchr(func_name, '.')) {
        /* C# attributes are referenced by their short name (`[Log]`) but declared
         * with the conventional `Attribute` suffix (`class LogAttribute`).  Retry
         * with the suffix appended so the declaration resolves. */
        char with_suffix[CBM_SZ_256];
        int wn = snprintf(with_suffix, sizeof(with_suffix), "%sAttribute", func_name);
        if (wn > 0 && (size_t)wn < sizeof(with_suffix)) {
            res = cbm_registry_resolve_lang(ctx->registry, with_suffix, module_qn, imp_keys,
                                            imp_vals, imp_count, lang);
        }
    }
    const cbm_gbuf_node_t *dec = NULL;
    if (res.qualified_name && res.qualified_name[0] != '\0') {
        dec = cbm_gbuf_find_by_qn(ctx->gbuf, res.qualified_name);
    }
    /* A qualified Rust proc-macro path can collide with the decorated
     * function's own name: resolving `#[tokio::main]` from module `main`
     * may fall back from `tokio::main` to same-module `main`.  That is not a
     * local decorator target (and the self-edge is discarded below), so keep
     * the full external spelling by materialising the synthetic decorator. */
    if (dec && dec->id == node->id && decorator[0] == '#' && decorator[1] == '[' &&
        strstr(func_name, "::")) {
        dec = NULL;
    }
    if (!dec) {
        /* The decorator target is not a local symbol (external attribute /
         * stdlib annotation / proc-macro derive).  Materialise a synthetic
         * "Decorator" node so the DECORATES relation is still recorded.
         * Upsert dedupes by QN, so every use of the same decorator name shares
         * one node (one extra node per distinct external decorator, project-
         * wide).  Created in the single-threaded semantic pass, so direct
         * ctx->gbuf mutation is safe here (mirrors pass_route_nodes). */
        char syn_qn[CBM_SZ_512];
        synth_decorator_qn(func_name, syn_qn, sizeof(syn_qn));
        int64_t syn_id =
            cbm_gbuf_upsert_node(ctx->gbuf, "Decorator", func_name, syn_qn, "", 0, 0, "{}");
        if (syn_id != 0) {
            dec = cbm_gbuf_find_by_qn(ctx->gbuf, syn_qn);
        }
    }
    if (dec && node->id != dec->id) {
        /* Decorator source text can contain quotes and raw newlines — escape
         * it or the edge properties JSON is malformed (twin of the parallel
         * path in pass_parallel.c). */
        char esc_dec[CBM_SZ_256];
        cbm_json_escape(esc_dec, sizeof(esc_dec), decorator);
        char props[CBM_SZ_512];
        snprintf(props, sizeof(props), "{\"decorator\":\"%s\"}", esc_dec);
        cbm_gbuf_insert_edge(ctx->gbuf, node->id, dec->id, "DECORATES", props);
        /* Ensure a reference edge exists so the decorator appears in usage queries
         * without being misclassified as a real call by downstream passes. */
        cbm_gbuf_insert_edge(ctx->gbuf, node->id, dec->id, "USAGE", "{}");
        (*count)++;
    }
}

/* Caller initializes non-Python properties to an empty object. */
static bool sem_base_properties(CBMLanguage language, int index, char *properties) {
    if (language != CBM_LANG_PYTHON) {
        return true;
    }
    int written = snprintf(properties, CBM_SZ_64, "{\"base_index\":%d}", index);
    return written >= 0 && (size_t)written < CBM_SZ_64;
}

static void sem_process_def_edges(cbm_pipeline_ctx_t *ctx, const CBMDefinition *def,
                                  const cbm_file_info_t *fi, const CBMFileResult *result,
                                  const char *module_qn, const char **imp_keys,
                                  const char **imp_vals, int imp_count, int *inherits_count,
                                  int *decorates_count, CBMLanguage lang,
                                  const cbm_php_vendor_names_t *php_vendor) {
    if (!def->qualified_name) {
        return;
    }
    const cbm_gbuf_node_t *node = cbm_gbuf_find_by_qn(ctx->gbuf, def->qualified_name);
    if (!node) {
        return;
    }
    if (def->base_classes) {
        for (int b = 0; def->base_classes[b]; b++) {
            /* #1186: `extends Request` with `use Saloon\Http\Request` names the
             * vendor class, never a same-named project class. Mirrors
             * pass_parallel.c. */
            if (cbm_pipeline_php_vendor_bound(php_vendor, def->base_classes[b])) {
                continue;
            }
            const char *base_qn = resolve_as_class(ctx->registry, def->base_classes[b], module_qn,
                                                   imp_keys, imp_vals, imp_count, lang);
            if (!base_qn || cbm_python_external_base_contradicts(
                                fi->language, &result->imports, def->base_classes[b], base_qn,
                                ctx->gbuf, ctx->project_name, fi->rel_path)) {
                continue; /* unresolved, or an external base (`unittest.TestCase`) */
            }
            const cbm_gbuf_node_t *base_node = cbm_gbuf_find_by_qn(ctx->gbuf, base_qn);
            if (base_node && node->id != base_node->id) {
                /* A base that resolves to an Interface is an IMPLEMENTS relation
                 * (Java `implements`, C# `: IFace`, TS `implements`); a Class/
                 * Type/Enum base is plain INHERITS. Keyed off the target NODE's
                 * label — the graph truth the edge attaches to — so the
                 * sequential and parallel venues cannot diverge (the parallel
                 * path once hardcoded INHERITS and demoted every explicit
                 * implements at scale). */
                char base_props[CBM_SZ_64] = "{}";
                if (!sem_base_properties(fi->language, b, base_props)) {
                    continue;
                }
                cbm_gbuf_insert_edge(ctx->gbuf, node->id, base_node->id,
                                     cbm_semantic_base_edge_type(base_node), base_props);
                (*inherits_count)++;
            }
        }
    }
    if (def->decorators) {
        for (int dc = 0; def->decorators[dc]; dc++) {
            resolve_decorator(ctx, node, def->decorators[dc], module_qn, imp_keys, imp_vals,
                              imp_count, decorates_count, lang);
        }
    }
}

/* Get extraction result from cache or re-extract. Sets *owned=true if caller must free. */
static CBMFileResult *sem_get_or_extract(cbm_pipeline_ctx_t *ctx, int file_idx,
                                         const cbm_file_info_t *fi, bool *owned) {
    *owned = false;
    if (ctx->result_cache && ctx->result_cache[file_idx]) {
        return ctx->result_cache[file_idx];
    }
    int source_len = 0;
    char *source = read_file(fi->path, &source_len);
    if (!source) {
        if (source_len != 0) {
            (void)cbm_pipeline_test_extraction_ok(ctx, fi->language, NULL);
        }
        return NULL;
    }
    CBMFileResult *r = NULL;
#if defined(CBM_ENABLE_TEST_SEAMS) && CBM_ENABLE_TEST_SEAMS
    if (!cbm_pipeline_test_force_extract_null(ctx, fi->language))
#endif
    {
        r = cbm_extract_file_ex_with_tests(source, source_len, fi->language, ctx->project_name,
                                           fi->rel_path, CBM_EXTRACT_BUDGET, NULL, NULL, NULL, NULL,
                                           ctx->test_declarations);
    }
    cbm_free(CBM_MEM_CLASS_SEMANTIC, source);
    (void)cbm_pipeline_test_extraction_ok(ctx, fi->language, r);
    if (r) {
        *owned = true;
    }
    return r;
}

/* Resolve Rust impl traits for one file's extraction results. */
static int resolve_impl_traits(cbm_pipeline_ctx_t *ctx, const CBMFileResult *result,
                               const char *module_qn, const char **imp_keys, const char **imp_vals,
                               int imp_count) {
    int count = 0;
    for (int t = 0; t < result->impl_traits.count; t++) {
        CBMImplTrait *it = &result->impl_traits.items[t];
        if (!it->trait_name || !it->struct_name) {
            continue;
        }
        const char *trait_qn = resolve_as_class(ctx->registry, it->trait_name, module_qn, imp_keys,
                                                imp_vals, imp_count, CBM_LANG_RUST);
        if (!trait_qn) {
            continue;
        }
        const char *struct_qn = resolve_as_class(ctx->registry, it->struct_name, module_qn,
                                                 imp_keys, imp_vals, imp_count, CBM_LANG_RUST);
        if (!struct_qn) {
            continue;
        }
        const cbm_gbuf_node_t *tn = cbm_gbuf_find_by_qn(ctx->gbuf, trait_qn);
        const cbm_gbuf_node_t *sn = cbm_gbuf_find_by_qn(ctx->gbuf, struct_qn);
        if (tn && sn && tn->id != sn->id) {
            cbm_gbuf_insert_edge(ctx->gbuf, sn->id, tn->id, "IMPLEMENTS", "{}");
            count++;
        }
    }
    return count;
}

int cbm_pipeline_pass_semantic(cbm_pipeline_ctx_t *ctx, const cbm_file_info_t *files,
                               int file_count) {
    cbm_log_info("pass.start", "pass", "semantic", "files", itoa_log(file_count));

    int inherits_count = 0;
    int decorates_count = 0;
    int implements_count = 0;
    int errors = 0;

    for (int i = 0; i < file_count; i++) {
        if (cbm_pipeline_check_cancel(ctx)) {
            return CBM_NOT_FOUND;
        }

        const char *rel = files[i].rel_path;

        bool result_owned = false;
        CBMFileResult *result = sem_get_or_extract(ctx, i, &files[i], &result_owned);
        if (!result) {
            if (atomic_load(&ctx->test_declarations_failed)) {
                return CBM_PIPELINE_ABORT_PRESERVE_DB;
            }
            errors++;
            continue;
        }

        if (!cbm_pipeline_test_result_ok(ctx, result)) {
            if (result_owned) {
                cbm_free_result(result);
            }
            return CBM_PIPELINE_ABORT_PRESERVE_DB;
        }

        /* Build import map for resolution */
        const char **imp_keys = NULL;
        const char **imp_vals = NULL;
        int imp_count = 0;
        build_import_map(ctx, rel, result, &imp_keys, &imp_vals, &imp_count);
        cbm_php_vendor_names_t php_vendor;
        cbm_pipeline_php_vendor_names_build(ctx->gbuf, ctx->project_name, rel, result, &php_vendor);

        char *module_qn = cbm_pipeline_fqn_module_dir(ctx->project_name, rel,
                                                      ps_module_is_dir(files[i].language));

        /* ── INHERITS + DECORATES from definitions ──────────────── */
        for (int d = 0; d < result->defs.count; d++) {
            sem_process_def_edges(ctx, &result->defs.items[d], &files[i], result, module_qn,
                                  imp_keys, imp_vals, imp_count, &inherits_count, &decorates_count,
                                  files[i].language, &php_vendor);
        }

        /* ── IMPLEMENTS from impl_traits (Rust) ─────────────────── */
        implements_count +=
            resolve_impl_traits(ctx, result, module_qn, imp_keys, imp_vals, imp_count);

        free(module_qn);
        cbm_pipeline_php_vendor_names_free(&php_vendor);
        free_import_map(imp_keys, imp_vals, imp_count);
        if (result_owned) {
            cbm_free_result(result);
        }
    }

    /* ── Go-style implicit interface satisfaction ──────────────── */
    int go_impl = cbm_pipeline_implements_go(ctx);
    implements_count += go_impl;

    /* ── Explicit-language override detection (full graph, serial) ── */
    int overrides = cbm_pipeline_override_explicit(ctx);

    cbm_log_info("pass.done", "pass", "semantic", "inherits", itoa_log(inherits_count), "decorates",
                 itoa_log(decorates_count), "implements", itoa_log(implements_count), "overrides",
                 itoa_log(overrides), "errors", itoa_log(errors));
    return 0;
}

const char *cbm_semantic_base_edge_type(const cbm_gbuf_node_t *base_node) {
    return (base_node && base_node->label && strcmp(base_node->label, "Interface") == 0)
               ? "IMPLEMENTS"
               : "INHERITS";
}

/* ── Explicit OVERRIDE: nearest declaring ancestor (#1278) ──────────
 *
 * A method overrides the same-named method of its NEAREST ancestor that
 * declares one, not only of its direct base: `Leaf(Intermediate(Processor))`
 * with an Intermediate that redeclares nothing still binds
 * Leaf.process -> Processor.process.
 *
 * The walk is level-synchronous over explicit IMPLEMENTS/INHERITS edges:
 * level 1 is every direct base, level k+1 every not-yet-visited base of
 * level k. A method is bound to every declaring ancestor on the FIRST level
 * that has one and is not looked up further, so the edge set does not depend
 * on edge or node order (a diamond's two equally near declarations both
 * bind; its shared root binds once, via insert-edge dedup). Direct bases are
 * level 1, so every edge the direct-base-only matcher made is still made.
 * A visited set makes every ancestor enter the walk at most once: cycles and
 * self-loops in a malformed hierarchy terminate, and the level count can
 * never exceed the node count (the check below is only that invariant). */

typedef struct {
    int64_t *ids;
    int count;
    int cap;
} ovr_id_list_t;

static bool ovr_ids_push(ovr_id_list_t *list, int64_t id) {
    if (list->count == list->cap) {
        int cap = list->cap ? list->cap * 2 : 16;
        int64_t *grown =
            cbm_realloc(CBM_MEM_CLASS_SEMANTIC, list->ids, (size_t)cap * sizeof(int64_t));
        if (!grown) {
            return false;
        }
        list->ids = grown;
        list->cap = cap;
    }
    list->ids[list->count++] = id;
    return true;
}

static int ovr_cmp_id(const void *a, const void *b) {
    int64_t x = *(const int64_t *)a;
    int64_t y = *(const int64_t *)b;
    return (x > y) - (x < y);
}

/* Sort + unique in place. */
static void ovr_ids_normalize(ovr_id_list_t *list) {
    if (list->count < 2) {
        return;
    }
    qsort(list->ids, (size_t)list->count, sizeof(int64_t), ovr_cmp_id);
    int w = 1;
    for (int r = 1; r < list->count; r++) {
        if (list->ids[r] != list->ids[w - 1]) {
            list->ids[w++] = list->ids[r];
        }
    }
    list->count = w;
}

static bool ovr_ids_has(const ovr_id_list_t *sorted, int64_t id) {
    return sorted->count > 0 &&
           bsearch(&id, sorted->ids, (size_t)sorted->count, sizeof(int64_t), ovr_cmp_id) != NULL;
}

static const char *const ovr_base_edge_types[] = {"IMPLEMENTS", "INHERITS"};
enum { OVR_BASE_EDGE_TYPE_COUNT = 2 };

/* Append every explicit base of `node` to `out`. */
static bool ovr_push_bases(const cbm_gbuf_t *gb, int64_t node, ovr_id_list_t *out) {
    for (int t = 0; t < OVR_BASE_EDGE_TYPE_COUNT; t++) {
        const cbm_gbuf_edge_t **edges = NULL;
        int n = 0;
        cbm_gbuf_find_edges_by_source_type(gb, node, ovr_base_edge_types[t], &edges, &n);
        for (int e = 0; e < n; e++) {
            if (!ovr_ids_push(out, edges[e]->target_id)) {
                return false;
            }
        }
    }
    return true;
}

/* Bind each still-unbound method of the class to the same-named methods of
 * one ancestor. Marks nothing: a level's bindings are committed by the caller
 * after the whole level ran, so equally near declarations all bind. */
static int ovr_match_ancestor(cbm_gbuf_t *gb, const cbm_gbuf_edge_t **cls_dm, int cls_dm_count,
                              const bool *bound, bool *bound_now, int64_t ancestor) {
    const cbm_gbuf_edge_t **base_dm = NULL;
    int base_dm_count = 0;
    cbm_gbuf_find_edges_by_source_type(gb, ancestor, "DEFINES_METHOD", &base_dm, &base_dm_count);
    int created = 0;
    for (int c = 0; c < cls_dm_count; c++) {
        if (bound[c]) {
            continue;
        }
        const cbm_gbuf_node_t *cm = cbm_gbuf_find_by_id(gb, cls_dm[c]->target_id);
        if (!cm || !cm->name) {
            continue;
        }
        for (int b = 0; b < base_dm_count; b++) {
            const cbm_gbuf_node_t *bm = cbm_gbuf_find_by_id(gb, base_dm[b]->target_id);
            if (bm && bm->name && cm->id != bm->id && strcmp(cm->name, bm->name) == 0) {
                cbm_gbuf_insert_edge(gb, cm->id, bm->id, "OVERRIDE", "{}");
                bound_now[c] = true;
                created++;
                break;
            }
        }
    }
    return created;
}

typedef struct {
    ovr_id_list_t visited; /* sorted */
    ovr_id_list_t level;
    ovr_id_list_t next;
    bool *bound;
    bool *bound_now;
    int flags_cap;
} ovr_walk_t;

static void ovr_walk_free(ovr_walk_t *w) {
    cbm_free(CBM_MEM_CLASS_SEMANTIC, w->visited.ids);
    cbm_free(CBM_MEM_CLASS_SEMANTIC, w->level.ids);
    cbm_free(CBM_MEM_CLASS_SEMANTIC, w->next.ids);
    cbm_free(CBM_MEM_CLASS_SEMANTIC, w->bound);
    cbm_free(CBM_MEM_CLASS_SEMANTIC, w->bound_now);
}

static bool ovr_walk_reserve_flags(ovr_walk_t *w, int n) {
    if (n <= w->flags_cap) {
        return true;
    }
    bool *b = cbm_realloc(CBM_MEM_CLASS_SEMANTIC, w->bound, (size_t)n * sizeof(bool));
    if (!b) {
        return false;
    }
    w->bound = b;
    bool *bn = cbm_realloc(CBM_MEM_CLASS_SEMANTIC, w->bound_now, (size_t)n * sizeof(bool));
    if (!bn) {
        return false;
    }
    w->bound_now = bn;
    w->flags_cap = n;
    return true;
}

/* Drop from `next` every id already visited, then add the rest to visited. */
static bool ovr_walk_advance(ovr_walk_t *w) {
    ovr_ids_normalize(&w->next);
    w->level.count = 0;
    for (int i = 0; i < w->next.count; i++) {
        int64_t id = w->next.ids[i];
        if (!ovr_ids_has(&w->visited, id) && !ovr_ids_push(&w->level, id)) {
            return false;
        }
    }
    for (int i = 0; i < w->level.count; i++) {
        if (!ovr_ids_push(&w->visited, w->level.ids[i])) {
            return false;
        }
    }
    ovr_ids_normalize(&w->visited);
    w->next.count = 0;
    return true;
}

/* All OVERRIDE edges of one class: walk its ancestors level by level until
 * every method is bound or the ancestors run out. */
static int ovr_class_overrides(cbm_gbuf_t *gb, int64_t cls_id, ovr_walk_t *w) {
    const cbm_gbuf_edge_t **cls_dm = NULL;
    int cls_dm_count = 0;
    cbm_gbuf_find_edges_by_source_type(gb, cls_id, "DEFINES_METHOD", &cls_dm, &cls_dm_count);
    if (cls_dm_count == 0 || !ovr_walk_reserve_flags(w, cls_dm_count)) {
        return 0;
    }
    memset(w->bound, 0, (size_t)cls_dm_count * sizeof(bool));
    memset(w->bound_now, 0, (size_t)cls_dm_count * sizeof(bool));
    w->visited.count = 0;
    w->next.count = 0;
    if (!ovr_ids_push(&w->visited, cls_id) || !ovr_push_bases(gb, cls_id, &w->next) ||
        !ovr_walk_advance(w)) {
        return 0;
    }
    int created = 0;
    int unbound = cls_dm_count;
    const int max_levels = cbm_gbuf_node_count(gb); /* invariant, never reached */
    for (int depth = 0; w->level.count > 0 && unbound > 0 && depth < max_levels; depth++) {
        for (int i = 0; i < w->level.count; i++) {
            created += ovr_match_ancestor(gb, cls_dm, cls_dm_count, w->bound, w->bound_now,
                                          w->level.ids[i]);
            if (!ovr_push_bases(gb, w->level.ids[i], &w->next)) {
                return created;
            }
        }
        for (int c = 0; c < cls_dm_count; c++) {
            if (w->bound_now[c] && !w->bound[c]) {
                w->bound[c] = true;
                unbound--;
            }
        }
        if (!ovr_walk_advance(w)) {
            return created;
        }
    }
    return created;
}

/* Python assembly dispatch needs the ordered MRO, not pairwise sibling
 * matching: an abstract declaration earlier in the MRO still masks a later
 * concrete one. Bound the proof and omit an edge if C3 is inconsistent,
 * cyclic, or exceeds the bound; never use a truncated method set as proof. */
enum { PY_ASSEMBLY_LIMIT = 128, PY_ASSEMBLY_MIN_BASES = 2 };
typedef enum { PY_MRO_VISITING, PY_MRO_COMPLETE, PY_MRO_UNPROVABLE } py_assembly_state_t;
typedef struct {
    int64_t id;
    int64_t order[PY_ASSEMBLY_LIMIT];
    int count;
    py_assembly_state_t state;
} py_assembly_mro_t;
typedef struct {
    py_assembly_mro_t entries[PY_ASSEMBLY_LIMIT];
    int count;
} py_assembly_cache_t;
typedef struct {
    const int64_t *ids;
    int count;
    int position;
} py_assembly_sequence_t;

/* Stable ordering preserves legacy insertion order when base_index is absent. */
static int py_assembly_bases(cbm_gbuf_t *gb, int64_t id, int64_t *bases) {
    const cbm_gbuf_edge_t **edges = NULL;
    int count = 0;
    cbm_gbuf_find_edges_by_source_type(gb, id, "INHERITS", &edges, &count);
    if (count >= PY_ASSEMBLY_LIMIT) {
        return CBM_NOT_FOUND;
    }
    int ranks[PY_ASSEMBLY_LIMIT];
    for (int i = 0; i < count; i++) {
        int rank = i;
        const char *props = edges[i]->properties_json ? edges[i]->properties_json : "{}";
        yyjson_doc *doc = yyjson_read(props, strlen(props), 0);
        yyjson_val *value = doc ? yyjson_obj_get(yyjson_doc_get_root(doc), "base_index") : NULL;
        if (yyjson_is_int(value)) {
            rank = yyjson_get_int(value);
        }
        yyjson_doc_free(doc);
        int j = i;
        while (j > 0 && ranks[j - SKIP_ONE] > rank) {
            ranks[j] = ranks[j - SKIP_ONE];
            bases[j] = bases[j - SKIP_ONE];
            j--;
        }
        ranks[j] = rank;
        bases[j] = edges[i]->target_id;
    }
    return count;
}

static bool py_assembly_in_tail(const py_assembly_sequence_t *sequences, int count,
                                int64_t candidate) {
    for (int i = 0; i < count; i++) {
        const py_assembly_sequence_t *seq = &sequences[i];
        for (int j = seq->position + SKIP_ONE; j < seq->count; j++) {
            if (seq->ids[j] == candidate) {
                return true;
            }
        }
    }
    return false;
}

static bool py_assembly_merge(py_assembly_mro_t *mro, py_assembly_sequence_t *sequences,
                              int count) {
    for (;;) {
        bool pending = false;
        bool selected = false;
        int64_t candidate = 0;
        for (int i = 0; i < count; i++) {
            const py_assembly_sequence_t *seq = &sequences[i];
            if (seq->position == seq->count) {
                continue;
            }
            pending = true;
            candidate = seq->ids[seq->position];
            if (!py_assembly_in_tail(sequences, count, candidate)) {
                selected = true;
                break;
            }
        }
        if (!pending) {
            return true;
        }
        if (!selected || mro->count == PY_ASSEMBLY_LIMIT) {
            return false;
        }
        mro->order[mro->count++] = candidate;
        for (int i = 0; i < count; i++) {
            py_assembly_sequence_t *seq = &sequences[i];
            if (seq->position < seq->count && seq->ids[seq->position] == candidate) {
                seq->position++;
            }
        }
    }
}

static py_assembly_mro_t *py_assembly_mro( // NOLINT(misc-no-recursion)
    cbm_gbuf_t *gb, int64_t id, py_assembly_cache_t *cache) {
    for (int i = 0; i < cache->count; i++) {
        if (cache->entries[i].id == id) {
            return cache->entries[i].state == PY_MRO_COMPLETE ? &cache->entries[i] : NULL;
        }
    }
    if (cache->count == PY_ASSEMBLY_LIMIT) {
        return NULL;
    }
    py_assembly_mro_t *mro = &cache->entries[cache->count++];
    mro->id = id;
    mro->state = PY_MRO_VISITING;
    int64_t bases[PY_ASSEMBLY_LIMIT];
    int count = py_assembly_bases(gb, id, bases);
    if (count < 0) {
        goto unprovable;
    }
    py_assembly_sequence_t sequences[PY_ASSEMBLY_LIMIT + SKIP_ONE] = {0};
    for (int i = 0; i < count; i++) {
        py_assembly_mro_t *parent = py_assembly_mro(gb, bases[i], cache);
        if (!parent) {
            goto unprovable;
        }
        sequences[i] = (py_assembly_sequence_t){.ids = parent->order, .count = parent->count};
    }
    sequences[count] = (py_assembly_sequence_t){.ids = bases, .count = count};
    mro->order[mro->count++] = id;
    if (!py_assembly_merge(mro, sequences, count + SKIP_ONE)) {
        goto unprovable;
    }
    mro->state = PY_MRO_COMPLETE;
    return mro;
unprovable:
    mro->state = PY_MRO_UNPROVABLE;
    return NULL;
}

static bool py_assembly_abstract(const cbm_gbuf_node_t *method) {
    if (!method || !method->properties_json) {
        return false;
    }
    yyjson_doc *doc = yyjson_read(method->properties_json, strlen(method->properties_json), 0);
    yyjson_val *root = doc ? yyjson_doc_get_root(doc) : NULL;
    bool abstract = yyjson_is_true(yyjson_obj_get(root, "is_abstract"));
    yyjson_val *decorators = yyjson_obj_get(root, "decorators");
    size_t index;
    size_t max;
    yyjson_val *value;
    yyjson_arr_foreach(decorators, index, max, value) {
        const char *name = yyjson_get_str(value);
        if (name &&
            (strcmp(name, "abstractmethod") == 0 || strcmp(name, "abc.abstractmethod") == 0)) {
            abstract = true;
        }
    }
    yyjson_doc_free(doc);
    return abstract;
}

static const cbm_gbuf_node_t *py_assembly_method(cbm_gbuf_t *gb, int64_t owner, const char *name) {
    const cbm_gbuf_edge_t **methods = NULL;
    int count = 0;
    cbm_gbuf_find_edges_by_source_type(gb, owner, "DEFINES_METHOD", &methods, &count);
    for (int i = 0; i < count; i++) {
        const cbm_gbuf_node_t *method = cbm_gbuf_find_by_id(gb, methods[i]->target_id);
        if (method && method->name && strcmp(method->name, name) == 0) {
            return method;
        }
    }
    return NULL;
}

static void *assembly_json_alloc(void *ctx, size_t size) {
    (void)ctx;
    return cbm_alloc(CBM_MEM_CLASS_SEMANTIC, size);
}

static void *assembly_json_realloc(void *ctx, void *ptr, size_t old_size, size_t size) {
    (void)ctx;
    (void)old_size;
    return cbm_realloc(CBM_MEM_CLASS_SEMANTIC, ptr, size);
}

static void assembly_json_free(void *ctx, void *ptr) {
    (void)ctx;
    cbm_free(CBM_MEM_CLASS_SEMANTIC, ptr);
}

static bool py_assembly_contains(const py_assembly_mro_t *mro, int64_t id) {
    for (int i = 0; i < mro->count; i++) {
        if (mro->order[i] == id) {
            return true;
        }
    }
    return false;
}

static bool py_assembly_distinct_branches(cbm_gbuf_t *gb, const cbm_gbuf_node_t *assembly,
                                          py_assembly_cache_t *cache, int64_t implementation,
                                          int64_t contract) {
    const cbm_gbuf_edge_t **bases = NULL;
    int count = 0;
    cbm_gbuf_find_edges_by_source_type(gb, assembly->id, "INHERITS", &bases, &count);
    bool implementation_branch = false;
    bool contract_branch = false;
    for (int i = 0; i < count; i++) {
        const py_assembly_mro_t *branch = py_assembly_mro(gb, bases[i]->target_id, cache);
        if (!branch) {
            continue;
        }
        bool has_impl = py_assembly_contains(branch, implementation);
        bool has_contract = py_assembly_contains(branch, contract);
        implementation_branch |= has_impl && !has_contract;
        contract_branch |= has_contract && !has_impl;
    }
    return implementation_branch && contract_branch;
}

static int py_assembly_emit(cbm_gbuf_t *gb, const cbm_gbuf_node_t *assembly,
                            const cbm_gbuf_node_t *winner, const cbm_gbuf_node_t *contract) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *props = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, props);
    yyjson_mut_obj_add_str(doc, props, "dispatch", "python_mro_sibling");
    yyjson_mut_obj_add_str(doc, props, "assembly", assembly->qualified_name);
    const yyjson_alc allocator = {.malloc = assembly_json_alloc,
                                  .realloc = assembly_json_realloc,
                                  .free = assembly_json_free,
                                  .ctx = NULL};
    char *json = yyjson_mut_write_opts(doc, 0, &allocator, NULL, NULL);
    int created = 0;
    if (json) {
        cbm_gbuf_insert_edge(gb, winner->id, contract->id, "OVERRIDE", json);
        created++;
    }
    cbm_free(CBM_MEM_CLASS_SEMANTIC, json);
    yyjson_mut_doc_free(doc);
    return created;
}

static int py_assembly_match_contract(cbm_gbuf_t *gb, const cbm_gbuf_node_t *assembly,
                                      py_assembly_cache_t *cache, const py_assembly_mro_t *mro,
                                      int contract_index, const cbm_gbuf_node_t *contract) {
    if (!py_assembly_abstract(contract)) {
        return 0;
    }
    for (int i = 0; i < contract_index; i++) {
        const cbm_gbuf_node_t *winner = py_assembly_method(gb, mro->order[i], contract->name);
        if (!winner) {
            continue;
        }
        /* The first declaration wins, including an abstract mask. Assembly-owned
         * methods are already covered by the upstream ancestor pass. */
        if (i == 0 || py_assembly_abstract(winner) ||
            !py_assembly_distinct_branches(gb, assembly, cache, mro->order[i],
                                           mro->order[contract_index])) {
            return 0;
        }
        return py_assembly_emit(gb, assembly, winner, contract);
    }
    return 0;
}

static int override_python_sibling_mixins(cbm_pipeline_ctx_t *ctx,
                                          const cbm_gbuf_node_t *assembly) {
    if (!assembly->file_path || !fp_ends_with(assembly->file_path, ".py")) {
        return 0;
    }
    const cbm_gbuf_edge_t **bases = NULL;
    int base_count = 0;
    cbm_gbuf_find_edges_by_source_type(ctx->gbuf, assembly->id, "INHERITS", &bases, &base_count);
    if (base_count < PY_ASSEMBLY_MIN_BASES) {
        return 0;
    }
    py_assembly_cache_t *cache = cbm_calloc(CBM_MEM_CLASS_SEMANTIC, sizeof(*cache));
    if (!cache) {
        return 0;
    }
    py_assembly_mro_t *mro = py_assembly_mro(ctx->gbuf, assembly->id, cache);
    int created = 0;
    if (!mro) {
        goto done;
    }
    for (int i = SKIP_ONE; i < mro->count; i++) {
        const cbm_gbuf_edge_t **methods = NULL;
        int count = 0;
        cbm_gbuf_find_edges_by_source_type(ctx->gbuf, mro->order[i], "DEFINES_METHOD", &methods,
                                           &count);
        for (int j = 0; j < count; j++) {
            const cbm_gbuf_node_t *contract = cbm_gbuf_find_by_id(ctx->gbuf, methods[j]->target_id);
            created += py_assembly_match_contract(ctx->gbuf, assembly, cache, mro, i, contract);
        }
    }
done:
    cbm_free(CBM_MEM_CLASS_SEMANTIC, cache);
    return created;
}

int cbm_pipeline_override_explicit(cbm_pipeline_ctx_t *ctx) {
    if (!ctx || !ctx->gbuf) {
        return 0;
    }
    /* Every class with at least one explicit base, once, in id order. */
    ovr_id_list_t classes = {0};
    for (int t = 0; t < OVR_BASE_EDGE_TYPE_COUNT; t++) {
        const cbm_gbuf_edge_t **edges = NULL;
        int edge_count = 0;
        cbm_gbuf_find_edges_by_type(ctx->gbuf, ovr_base_edge_types[t], &edges, &edge_count);
        for (int e = 0; e < edge_count; e++) {
            if (!ovr_ids_push(&classes, edges[e]->source_id)) {
                cbm_free(CBM_MEM_CLASS_SEMANTIC, classes.ids);
                return 0;
            }
        }
    }
    ovr_ids_normalize(&classes);

    int created = 0;
    ovr_walk_t walk = {0};
    for (int i = 0; i < classes.count; i++) {
        const cbm_gbuf_node_t *cls = cbm_gbuf_find_by_id(ctx->gbuf, classes.ids[i]);
        /* Go's implicit satisfaction already emits OVERRIDE with interface
         * semantics; running both would double-cover .go sources. */
        if (!cls || (cls->file_path && fp_ends_with(cls->file_path, ".go"))) {
            continue;
        }
        created += ovr_class_overrides(ctx->gbuf, cls->id, &walk);
        created += override_python_sibling_mixins(ctx, cls);
    }
    ovr_walk_free(&walk);
    cbm_free(CBM_MEM_CLASS_SEMANTIC, classes.ids);
    return created;
}
