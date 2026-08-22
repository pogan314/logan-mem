/*
 * java_stdlib_data.c — Curated Java standard-library type/method registry.
 *
 * Strategy:
 *   - java.lang.* — fully covered (the implicit-import package).
 *     Object, String, StringBuilder, StringBuffer, CharSequence, Class,
 *     Throwable + the common subclass tree, Number + boxed primitives,
 *     Math, System, Thread, Iterable, Comparable, Cloneable, Enum, Record,
 *     AutoCloseable, the common Exception types.
 *   - java.util.* — collections + iterators + Optional + Date/Calendar +
 *     Arrays/Collections + Scanner/Random/UUID + Map.Entry.
 *   - java.io.* — streams, readers, writers, File, IOException family.
 *   - java.nio.file.* — Path, Paths, Files (often-used helpers).
 *   - java.util.function — the 21 functional interfaces.
 *   - java.util.stream  — Stream + Collectors entry points.
 *   - java.util.concurrent — ExecutorService, Future, CompletableFuture,
 *     ConcurrentHashMap, the concurrent collection set.
 *   - java.time — LocalDate/LocalTime/LocalDateTime/Duration/Instant.
 *
 * Method signatures use registry-level fidelity: receiver, short name,
 * return type. Param types are intentionally unmodeled (the resolver
 * chooses overloads by arity, with type compatibility scoring breaking
 * ties — see lsm_registry_lookup_method_by_args).
 *
 * This is the JLS-spec-aligned slice of the stdlib that 90%+ of real-world
 * Java code touches.
 */

#include "../type_rep.h"
#include "../type_registry.h"
#include "../../arena.h"
#include "../java_lsp.h"
#include <string.h>

#define REG_TYPE(qn_, short_, is_iface_, parents_)            \
    do {                                                      \
        memset(&rt, 0, sizeof(rt));                           \
        rt.qualified_name = (qn_);                            \
        rt.short_name = (short_);                             \
        rt.is_interface = (is_iface_);                        \
        rt.embedded_types = (parents_);                       \
        lsm_registry_add_type(reg, rt);                       \
    } while (0)

#define REG_METHOD(class_qn_, method_name_, ret_type_)                                          \
    do {                                                                                        \
        memset(&rf, 0, sizeof(rf));                                                             \
        rf.min_params = -1;                                                                     \
        rf.qualified_name =                                                                     \
            lsm_arena_sprintf(arena, "%s.%s", (class_qn_), (method_name_));                     \
        rf.short_name = (method_name_);                                                         \
        rf.receiver_type = (class_qn_);                                                         \
        {                                                                                       \
            const LSMType **rets =                                                              \
                (const LSMType **)lsm_arena_alloc(arena, 2 * sizeof(*rets));                    \
            rets[0] = (ret_type_);                                                              \
            rets[1] = NULL;                                                                     \
            rf.signature = lsm_type_func(arena, NULL, NULL, rets);                              \
        }                                                                                       \
        lsm_registry_add_func(reg, rf);                                                         \
    } while (0)

#define REG_CTOR(class_qn_, short_name_)                                              \
    do {                                                                              \
        memset(&rf, 0, sizeof(rf));                                                   \
        rf.min_params = -1;                                                           \
        rf.qualified_name =                                                           \
            lsm_arena_sprintf(arena, "%s.%s", (class_qn_), (short_name_));            \
        rf.short_name = (short_name_);                                                \
        rf.receiver_type = (class_qn_);                                               \
        {                                                                             \
            const LSMType **rets =                                                    \
                (const LSMType **)lsm_arena_alloc(arena, 2 * sizeof(*rets));          \
            rets[0] = lsm_type_named(arena, (class_qn_));                             \
            rets[1] = NULL;                                                           \
            rf.signature = lsm_type_func(arena, NULL, NULL, rets);                    \
        }                                                                             \
        lsm_registry_add_func(reg, rf);                                               \
    } while (0)

#define REG_FIELD(class_qn_, name_, type_)                                            \
    do {                                                                              \
        const LSMRegisteredType *_existing =                                          \
            lsm_registry_lookup_type(reg, (class_qn_));                               \
        (void)_existing;                                                              \
        /* Field append handled by REG_TYPE_FIELDS below. */                          \
        /* Placeholder for future per-field appends. */                               \
    } while (0)

void lsm_java_stdlib_register(LSMTypeRegistry *reg, LSMArena *arena) {
    LSMRegisteredType rt;
    LSMRegisteredFunc rf;

    /* ── Type-parent lists (must be static so addresses outlive the call) ── */
    static const char *no_parents[] = {NULL};
    static const char *parents_object[] = {"java.lang.Object", NULL};
    static const char *parents_throwable[] = {"java.lang.Object", NULL};
    static const char *parents_exception[] = {"java.lang.Throwable", NULL};
    static const char *parents_error[] = {"java.lang.Throwable", NULL};
    static const char *parents_runtime_exc[] = {"java.lang.Exception", NULL};
    static const char *parents_io_exc[] = {"java.lang.Exception", NULL};
    static const char *parents_number[] = {"java.lang.Object", NULL};
    static const char *parents_integer[] = {"java.lang.Number", NULL};
    static const char *parents_long[] = {"java.lang.Number", NULL};
    static const char *parents_double[] = {"java.lang.Number", NULL};
    static const char *parents_float[] = {"java.lang.Number", NULL};
    static const char *parents_short[] = {"java.lang.Number", NULL};
    static const char *parents_byte[] = {"java.lang.Number", NULL};
    static const char *parents_string[] = {"java.lang.Object", NULL};
    static const char *parents_charseq[] = {NULL};
    static const char *parents_iterable[] = {NULL};
    static const char *parents_collection[] = {"java.lang.Iterable", NULL};
    static const char *parents_list[] = {"java.util.Collection", NULL};
    static const char *parents_set[] = {"java.util.Collection", NULL};
    static const char *parents_queue[] = {"java.util.Collection", NULL};
    static const char *parents_deque[] = {"java.util.Queue", NULL};
    static const char *parents_map[] = {NULL};
    static const char *parents_map_entry[] = {NULL};
    static const char *parents_iterator[] = {NULL};
    static const char *parents_arraylist[] = {"java.util.List", NULL};
    static const char *parents_linkedlist[] = {"java.util.List", NULL};
    static const char *parents_hashset[] = {"java.util.Set", NULL};
    static const char *parents_treeset[] = {"java.util.Set", NULL};
    static const char *parents_linkedhashset[] = {"java.util.Set", NULL};
    static const char *parents_hashmap[] = {"java.util.Map", NULL};
    static const char *parents_treemap[] = {"java.util.Map", NULL};
    static const char *parents_linkedhashmap[] = {"java.util.Map", NULL};
    static const char *parents_concurrent_hashmap[] = {"java.util.Map", NULL};

    static const char *parents_inputstream[] = {"java.lang.AutoCloseable", NULL};
    static const char *parents_outputstream[] = {"java.lang.AutoCloseable", NULL};
    static const char *parents_reader[] = {"java.lang.AutoCloseable", NULL};
    static const char *parents_writer[] = {"java.lang.AutoCloseable", NULL};
    static const char *parents_buffered_reader[] = {"java.io.Reader", NULL};
    static const char *parents_buffered_writer[] = {"java.io.Writer", NULL};
    static const char *parents_print_stream[] = {"java.io.OutputStream", NULL};
    static const char *parents_print_writer[] = {"java.io.Writer", NULL};
    static const char *parents_file_input_stream[] = {"java.io.InputStream", NULL};
    static const char *parents_file_output_stream[] = {"java.io.OutputStream", NULL};
    static const char *parents_file_reader[] = {"java.io.Reader", NULL};
    static const char *parents_file_writer[] = {"java.io.Writer", NULL};
    static const char *parents_io_exception[] = {"java.lang.Exception", NULL};
    static const char *parents_runtime_exc_chain[] = {"java.lang.RuntimeException", NULL};
    /* Parent lists for types previously registered with inline compound
     * literals. A compound literal has automatic (block) storage duration,
     * so storing its address into the registry left a dangling stack pointer
     * once the REG_TYPE statement's block ended — an AddressSanitizer
     * stack-use-after-scope when the inheritance walk later read
     * rt->embedded_types[0]. These must be static so their addresses outlive
     * the call, exactly like the parent lists above. */
    static const char *parents_gregorian_calendar[] = {"java.util.Calendar", NULL};
    static const char *parents_file_not_found_exc[] = {"java.io.IOException", NULL};
    static const char *parents_closeable[] = {"java.lang.AutoCloseable", NULL};
    static const char *parents_unary_operator[] = {"java.util.function.Function", NULL};
    static const char *parents_binary_operator[] = {"java.util.function.BiFunction", NULL};
    static const char *parents_completable_future[] = {"java.util.concurrent.Future", NULL};
    static const char *parents_reentrant_lock[] = {"java.util.concurrent.locks.Lock", NULL};

    /* ── java.lang ─────────────────────────────────────────────── */
    REG_TYPE("java.lang.Object", "Object", false, no_parents);
    REG_TYPE("java.lang.Class", "Class", false, parents_object);
    REG_TYPE("java.lang.ClassLoader", "ClassLoader", false, parents_object);
    REG_TYPE("java.lang.CharSequence", "CharSequence", true, parents_charseq);
    REG_TYPE("java.lang.String", "String", false, parents_string);
    REG_TYPE("java.lang.StringBuilder", "StringBuilder", false, parents_object);
    REG_TYPE("java.lang.StringBuffer", "StringBuffer", false, parents_object);
    REG_TYPE("java.lang.Number", "Number", false, parents_number);
    REG_TYPE("java.lang.Integer", "Integer", false, parents_integer);
    REG_TYPE("java.lang.Long", "Long", false, parents_long);
    REG_TYPE("java.lang.Short", "Short", false, parents_short);
    REG_TYPE("java.lang.Byte", "Byte", false, parents_byte);
    REG_TYPE("java.lang.Float", "Float", false, parents_float);
    REG_TYPE("java.lang.Double", "Double", false, parents_double);
    REG_TYPE("java.lang.Boolean", "Boolean", false, parents_object);
    REG_TYPE("java.lang.Character", "Character", false, parents_object);
    REG_TYPE("java.lang.Void", "Void", false, parents_object);
    REG_TYPE("java.lang.Iterable", "Iterable", true, parents_iterable);
    REG_TYPE("java.lang.Comparable", "Comparable", true, no_parents);
    REG_TYPE("java.lang.Cloneable", "Cloneable", true, no_parents);
    REG_TYPE("java.lang.Runnable", "Runnable", true, no_parents);
    REG_TYPE("java.lang.AutoCloseable", "AutoCloseable", true, no_parents);
    REG_TYPE("java.lang.Math", "Math", false, parents_object);
    REG_TYPE("java.lang.System", "System", false, parents_object);
    REG_TYPE("java.lang.Thread", "Thread", false, parents_object);
    REG_TYPE("java.lang.Process", "Process", false, parents_object);
    REG_TYPE("java.lang.ProcessBuilder", "ProcessBuilder", false, parents_object);
    REG_TYPE("java.lang.StackTraceElement", "StackTraceElement", false, parents_object);
    REG_TYPE("java.lang.Enum", "Enum", false, parents_object);
    REG_TYPE("java.lang.Record", "Record", false, parents_object);
    REG_TYPE("java.lang.Throwable", "Throwable", false, parents_throwable);
    REG_TYPE("java.lang.Exception", "Exception", false, parents_exception);
    REG_TYPE("java.lang.Error", "Error", false, parents_error);
    REG_TYPE("java.lang.RuntimeException", "RuntimeException", false, parents_runtime_exc);
    REG_TYPE("java.lang.NullPointerException", "NullPointerException", false,
             parents_runtime_exc_chain);
    REG_TYPE("java.lang.IllegalArgumentException", "IllegalArgumentException", false,
             parents_runtime_exc_chain);
    REG_TYPE("java.lang.IllegalStateException", "IllegalStateException", false,
             parents_runtime_exc_chain);
    REG_TYPE("java.lang.IndexOutOfBoundsException", "IndexOutOfBoundsException", false,
             parents_runtime_exc_chain);
    REG_TYPE("java.lang.ArrayIndexOutOfBoundsException", "ArrayIndexOutOfBoundsException", false,
             parents_runtime_exc_chain);
    REG_TYPE("java.lang.ArithmeticException", "ArithmeticException", false,
             parents_runtime_exc_chain);
    REG_TYPE("java.lang.ClassCastException", "ClassCastException", false,
             parents_runtime_exc_chain);
    REG_TYPE("java.lang.ClassNotFoundException", "ClassNotFoundException", false,
             parents_exception);
    REG_TYPE("java.lang.NumberFormatException", "NumberFormatException", false,
             parents_runtime_exc_chain);
    REG_TYPE("java.lang.UnsupportedOperationException", "UnsupportedOperationException", false,
             parents_runtime_exc_chain);
    REG_TYPE("java.lang.InterruptedException", "InterruptedException", false, parents_exception);
    REG_TYPE("java.lang.SecurityException", "SecurityException", false,
             parents_runtime_exc_chain);
    REG_TYPE("java.lang.NoSuchMethodException", "NoSuchMethodException", false, parents_exception);
    REG_TYPE("java.lang.NoSuchFieldException", "NoSuchFieldException", false, parents_exception);

    /* Annotation-marker types. */
    REG_TYPE("java.lang.Override", "Override", true, no_parents);
    REG_TYPE("java.lang.Deprecated", "Deprecated", true, no_parents);
    REG_TYPE("java.lang.SuppressWarnings", "SuppressWarnings", true, no_parents);
    REG_TYPE("java.lang.FunctionalInterface", "FunctionalInterface", true, no_parents);

    /* ── Object methods ───────────────────────────────────────── */
    REG_METHOD("java.lang.Object", "toString", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.Object", "hashCode", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.lang.Object", "equals", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.lang.Object", "getClass", lsm_type_named(arena, "java.lang.Class"));
    REG_METHOD("java.lang.Object", "wait", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.lang.Object", "notify", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.lang.Object", "notifyAll", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.lang.Object", "clone", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.lang.Object", "finalize", lsm_type_builtin(arena, "void"));

    /* ── String methods ───────────────────────────────────────── */
    REG_METHOD("java.lang.String", "length", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.lang.String", "isEmpty", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.lang.String", "isBlank", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.lang.String", "charAt", lsm_type_builtin(arena, "char"));
    REG_METHOD("java.lang.String", "codePointAt", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.lang.String", "equals", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.lang.String", "equalsIgnoreCase", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.lang.String", "compareTo", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.lang.String", "compareToIgnoreCase", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.lang.String", "indexOf", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.lang.String", "lastIndexOf", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.lang.String", "contains", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.lang.String", "startsWith", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.lang.String", "endsWith", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.lang.String", "matches", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.lang.String", "concat", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.String", "substring", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.String", "trim", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.String", "strip", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.String", "stripLeading", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.String", "stripTrailing", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.String", "toLowerCase", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.String", "toUpperCase", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.String", "replace", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.String", "replaceAll", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.String", "replaceFirst", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.String", "split",
               lsm_type_slice(arena, lsm_type_named(arena, "java.lang.String")));
    REG_METHOD("java.lang.String", "toCharArray", lsm_type_slice(arena, lsm_type_builtin(arena, "char")));
    REG_METHOD("java.lang.String", "getBytes", lsm_type_slice(arena, lsm_type_builtin(arena, "byte")));
    REG_METHOD("java.lang.String", "intern", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.String", "format", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.String", "valueOf", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.String", "join", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.String", "repeat", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.String", "lines", lsm_type_named(arena, "java.util.stream.Stream"));
    REG_METHOD("java.lang.String", "chars", lsm_type_named(arena, "java.util.stream.IntStream"));
    REG_METHOD("java.lang.String", "codePoints",
               lsm_type_named(arena, "java.util.stream.IntStream"));
    REG_METHOD("java.lang.String", "hashCode", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.lang.String", "toString", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.String", "toCharArray", lsm_type_slice(arena, lsm_type_builtin(arena, "char")));
    REG_CTOR("java.lang.String", "String");

    /* ── StringBuilder / StringBuffer ─────────────────────────── */
    REG_METHOD("java.lang.StringBuilder", "append",
               lsm_type_named(arena, "java.lang.StringBuilder"));
    REG_METHOD("java.lang.StringBuilder", "insert",
               lsm_type_named(arena, "java.lang.StringBuilder"));
    REG_METHOD("java.lang.StringBuilder", "delete",
               lsm_type_named(arena, "java.lang.StringBuilder"));
    REG_METHOD("java.lang.StringBuilder", "deleteCharAt",
               lsm_type_named(arena, "java.lang.StringBuilder"));
    REG_METHOD("java.lang.StringBuilder", "replace",
               lsm_type_named(arena, "java.lang.StringBuilder"));
    REG_METHOD("java.lang.StringBuilder", "reverse",
               lsm_type_named(arena, "java.lang.StringBuilder"));
    REG_METHOD("java.lang.StringBuilder", "toString",
               lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.StringBuilder", "length", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.lang.StringBuilder", "charAt", lsm_type_builtin(arena, "char"));
    REG_METHOD("java.lang.StringBuilder", "setLength", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.lang.StringBuilder", "indexOf", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.lang.StringBuilder", "substring",
               lsm_type_named(arena, "java.lang.String"));
    REG_CTOR("java.lang.StringBuilder", "StringBuilder");

    REG_METHOD("java.lang.StringBuffer", "append",
               lsm_type_named(arena, "java.lang.StringBuffer"));
    REG_METHOD("java.lang.StringBuffer", "toString",
               lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.StringBuffer", "length", lsm_type_builtin(arena, "int"));
    REG_CTOR("java.lang.StringBuffer", "StringBuffer");

    /* ── CharSequence ─────────────────────────────────────────── */
    REG_METHOD("java.lang.CharSequence", "length", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.lang.CharSequence", "charAt", lsm_type_builtin(arena, "char"));
    REG_METHOD("java.lang.CharSequence", "subSequence",
               lsm_type_named(arena, "java.lang.CharSequence"));
    REG_METHOD("java.lang.CharSequence", "toString",
               lsm_type_named(arena, "java.lang.String"));

    /* ── Number + boxed types ─────────────────────────────────── */
    REG_METHOD("java.lang.Number", "intValue", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.lang.Number", "longValue", lsm_type_builtin(arena, "long"));
    REG_METHOD("java.lang.Number", "doubleValue", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.lang.Number", "floatValue", lsm_type_builtin(arena, "float"));
    REG_METHOD("java.lang.Number", "shortValue", lsm_type_builtin(arena, "short"));
    REG_METHOD("java.lang.Number", "byteValue", lsm_type_builtin(arena, "byte"));

    REG_METHOD("java.lang.Integer", "intValue", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.lang.Integer", "parseInt", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.lang.Integer", "valueOf", lsm_type_named(arena, "java.lang.Integer"));
    REG_METHOD("java.lang.Integer", "toString", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.Integer", "compare", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.lang.Integer", "compareTo", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.lang.Integer", "equals", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.lang.Integer", "hashCode", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.lang.Integer", "max", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.lang.Integer", "min", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.lang.Integer", "sum", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.lang.Integer", "bitCount", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.lang.Integer", "toBinaryString", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.Integer", "toHexString", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.Integer", "toOctalString", lsm_type_named(arena, "java.lang.String"));

    REG_METHOD("java.lang.Long", "longValue", lsm_type_builtin(arena, "long"));
    REG_METHOD("java.lang.Long", "parseLong", lsm_type_builtin(arena, "long"));
    REG_METHOD("java.lang.Long", "valueOf", lsm_type_named(arena, "java.lang.Long"));
    REG_METHOD("java.lang.Long", "toString", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.Long", "compareTo", lsm_type_builtin(arena, "int"));

    REG_METHOD("java.lang.Double", "doubleValue", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.lang.Double", "parseDouble", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.lang.Double", "valueOf", lsm_type_named(arena, "java.lang.Double"));
    REG_METHOD("java.lang.Double", "toString", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.Double", "isNaN", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.lang.Double", "isInfinite", lsm_type_builtin(arena, "boolean"));

    REG_METHOD("java.lang.Float", "floatValue", lsm_type_builtin(arena, "float"));
    REG_METHOD("java.lang.Float", "parseFloat", lsm_type_builtin(arena, "float"));
    REG_METHOD("java.lang.Float", "valueOf", lsm_type_named(arena, "java.lang.Float"));

    REG_METHOD("java.lang.Boolean", "booleanValue", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.lang.Boolean", "parseBoolean", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.lang.Boolean", "valueOf", lsm_type_named(arena, "java.lang.Boolean"));
    REG_METHOD("java.lang.Boolean", "toString", lsm_type_named(arena, "java.lang.String"));

    REG_METHOD("java.lang.Character", "charValue", lsm_type_builtin(arena, "char"));
    REG_METHOD("java.lang.Character", "isDigit", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.lang.Character", "isLetter", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.lang.Character", "isLetterOrDigit", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.lang.Character", "isWhitespace", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.lang.Character", "isUpperCase", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.lang.Character", "isLowerCase", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.lang.Character", "toUpperCase", lsm_type_builtin(arena, "char"));
    REG_METHOD("java.lang.Character", "toLowerCase", lsm_type_builtin(arena, "char"));
    REG_METHOD("java.lang.Character", "getNumericValue", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.lang.Character", "valueOf", lsm_type_named(arena, "java.lang.Character"));

    REG_METHOD("java.lang.Byte", "byteValue", lsm_type_builtin(arena, "byte"));
    REG_METHOD("java.lang.Byte", "parseByte", lsm_type_builtin(arena, "byte"));
    REG_METHOD("java.lang.Byte", "valueOf", lsm_type_named(arena, "java.lang.Byte"));

    REG_METHOD("java.lang.Short", "shortValue", lsm_type_builtin(arena, "short"));
    REG_METHOD("java.lang.Short", "parseShort", lsm_type_builtin(arena, "short"));
    REG_METHOD("java.lang.Short", "valueOf", lsm_type_named(arena, "java.lang.Short"));

    /* ── Math ─────────────────────────────────────────────────── */
    REG_METHOD("java.lang.Math", "abs", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.lang.Math", "min", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.lang.Math", "max", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.lang.Math", "sqrt", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.lang.Math", "cbrt", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.lang.Math", "pow", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.lang.Math", "exp", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.lang.Math", "log", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.lang.Math", "log10", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.lang.Math", "sin", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.lang.Math", "cos", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.lang.Math", "tan", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.lang.Math", "asin", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.lang.Math", "acos", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.lang.Math", "atan", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.lang.Math", "atan2", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.lang.Math", "floor", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.lang.Math", "ceil", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.lang.Math", "round", lsm_type_builtin(arena, "long"));
    REG_METHOD("java.lang.Math", "random", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.lang.Math", "signum", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.lang.Math", "hypot", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.lang.Math", "floorDiv", lsm_type_builtin(arena, "long"));
    REG_METHOD("java.lang.Math", "floorMod", lsm_type_builtin(arena, "long"));
    REG_METHOD("java.lang.Math", "addExact", lsm_type_builtin(arena, "long"));
    REG_METHOD("java.lang.Math", "subtractExact", lsm_type_builtin(arena, "long"));
    REG_METHOD("java.lang.Math", "multiplyExact", lsm_type_builtin(arena, "long"));
    REG_METHOD("java.lang.Math", "toRadians", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.lang.Math", "toDegrees", lsm_type_builtin(arena, "double"));

    /* ── System ───────────────────────────────────────────────── */
    REG_METHOD("java.lang.System", "currentTimeMillis", lsm_type_builtin(arena, "long"));
    REG_METHOD("java.lang.System", "nanoTime", lsm_type_builtin(arena, "long"));
    REG_METHOD("java.lang.System", "exit", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.lang.System", "getenv", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.System", "getProperty", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.System", "setProperty", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.System", "lineSeparator", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.System", "arraycopy", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.lang.System", "identityHashCode", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.lang.System", "gc", lsm_type_builtin(arena, "void"));

    /* ── Thread ───────────────────────────────────────────────── */
    REG_METHOD("java.lang.Thread", "start", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.lang.Thread", "run", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.lang.Thread", "join", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.lang.Thread", "interrupt", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.lang.Thread", "isAlive", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.lang.Thread", "sleep", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.lang.Thread", "currentThread", lsm_type_named(arena, "java.lang.Thread"));
    REG_METHOD("java.lang.Thread", "yield", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.lang.Thread", "getName", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.Thread", "setName", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.lang.Thread", "getId", lsm_type_builtin(arena, "long"));
    REG_METHOD("java.lang.Thread", "isInterrupted", lsm_type_builtin(arena, "boolean"));

    /* ── Class ────────────────────────────────────────────────── */
    REG_METHOD("java.lang.Class", "getName", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.Class", "getSimpleName", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.Class", "getCanonicalName", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.Class", "isInterface", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.lang.Class", "isArray", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.lang.Class", "isAssignableFrom", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.lang.Class", "isInstance", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.lang.Class", "newInstance", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.lang.Class", "forName", lsm_type_named(arena, "java.lang.Class"));
    REG_METHOD("java.lang.Class", "getSuperclass", lsm_type_named(arena, "java.lang.Class"));
    REG_METHOD("java.lang.Class", "getInterfaces",
               lsm_type_slice(arena, lsm_type_named(arena, "java.lang.Class")));

    /* ── Iterable / Iterator ──────────────────────────────────── */
    REG_METHOD("java.lang.Iterable", "iterator", lsm_type_named(arena, "java.util.Iterator"));
    REG_METHOD("java.lang.Iterable", "forEach", lsm_type_builtin(arena, "void"));

    /* ── Throwable methods ────────────────────────────────────── */
    REG_METHOD("java.lang.Throwable", "getMessage", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.Throwable", "getLocalizedMessage",
               lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.Throwable", "getCause", lsm_type_named(arena, "java.lang.Throwable"));
    REG_METHOD("java.lang.Throwable", "initCause",
               lsm_type_named(arena, "java.lang.Throwable"));
    REG_METHOD("java.lang.Throwable", "printStackTrace", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.lang.Throwable", "toString", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.lang.Throwable", "getStackTrace",
               lsm_type_slice(arena, lsm_type_named(arena, "java.lang.StackTraceElement")));

    /* ── AutoCloseable ────────────────────────────────────────── */
    REG_METHOD("java.lang.AutoCloseable", "close", lsm_type_builtin(arena, "void"));

    /* ── Comparable ───────────────────────────────────────────── */
    REG_METHOD("java.lang.Comparable", "compareTo", lsm_type_builtin(arena, "int"));

    /* ── Runnable ─────────────────────────────────────────────── */
    REG_METHOD("java.lang.Runnable", "run", lsm_type_builtin(arena, "void"));

    /* ── java.util ────────────────────────────────────────────── */
    REG_TYPE("java.util.Collection", "Collection", true, parents_collection);
    REG_TYPE("java.util.List", "List", true, parents_list);
    REG_TYPE("java.util.Set", "Set", true, parents_set);
    REG_TYPE("java.util.Queue", "Queue", true, parents_queue);
    REG_TYPE("java.util.Deque", "Deque", true, parents_deque);
    REG_TYPE("java.util.Map", "Map", true, parents_map);
    REG_TYPE("java.util.Map.Entry", "Entry", true, parents_map_entry);
    REG_TYPE("java.util.Iterator", "Iterator", true, parents_iterator);
    REG_TYPE("java.util.ListIterator", "ListIterator", true, parents_iterator);
    REG_TYPE("java.util.Spliterator", "Spliterator", true, no_parents);
    REG_TYPE("java.util.Comparator", "Comparator", true, no_parents);

    REG_TYPE("java.util.ArrayList", "ArrayList", false, parents_arraylist);
    REG_TYPE("java.util.LinkedList", "LinkedList", false, parents_linkedlist);
    REG_TYPE("java.util.Vector", "Vector", false, parents_arraylist);
    REG_TYPE("java.util.Stack", "Stack", false, parents_arraylist);
    REG_TYPE("java.util.HashSet", "HashSet", false, parents_hashset);
    REG_TYPE("java.util.TreeSet", "TreeSet", false, parents_treeset);
    REG_TYPE("java.util.LinkedHashSet", "LinkedHashSet", false, parents_linkedhashset);
    REG_TYPE("java.util.HashMap", "HashMap", false, parents_hashmap);
    REG_TYPE("java.util.TreeMap", "TreeMap", false, parents_treemap);
    REG_TYPE("java.util.LinkedHashMap", "LinkedHashMap", false, parents_linkedhashmap);
    REG_TYPE("java.util.ArrayDeque", "ArrayDeque", false, parents_deque);
    REG_TYPE("java.util.PriorityQueue", "PriorityQueue", false, parents_queue);

    REG_TYPE("java.util.Optional", "Optional", false, parents_object);
    REG_TYPE("java.util.OptionalInt", "OptionalInt", false, parents_object);
    REG_TYPE("java.util.OptionalLong", "OptionalLong", false, parents_object);
    REG_TYPE("java.util.OptionalDouble", "OptionalDouble", false, parents_object);
    REG_TYPE("java.util.Date", "Date", false, parents_object);
    REG_TYPE("java.util.Calendar", "Calendar", false, parents_object);
    REG_TYPE("java.util.GregorianCalendar", "GregorianCalendar", false,
             parents_gregorian_calendar);
    REG_TYPE("java.util.TimeZone", "TimeZone", false, parents_object);
    REG_TYPE("java.util.Locale", "Locale", false, parents_object);
    REG_TYPE("java.util.UUID", "UUID", false, parents_object);
    REG_TYPE("java.util.Random", "Random", false, parents_object);
    REG_TYPE("java.util.Scanner", "Scanner", false, parents_object);
    REG_TYPE("java.util.Arrays", "Arrays", false, parents_object);
    REG_TYPE("java.util.Collections", "Collections", false, parents_object);
    REG_TYPE("java.util.Objects", "Objects", false, parents_object);
    REG_TYPE("java.util.Properties", "Properties", false, parents_hashmap);
    REG_TYPE("java.util.regex.Pattern", "Pattern", false, parents_object);
    REG_TYPE("java.util.regex.Matcher", "Matcher", false, parents_object);

    /* ── Collection methods ───────────────────────────────────── */
    REG_METHOD("java.util.Collection", "size", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.Collection", "isEmpty", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Collection", "contains", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Collection", "containsAll", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Collection", "iterator", lsm_type_named(arena, "java.util.Iterator"));
    REG_METHOD("java.util.Collection", "toArray",
               lsm_type_slice(arena, lsm_type_named(arena, "java.lang.Object")));
    REG_METHOD("java.util.Collection", "add", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Collection", "addAll", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Collection", "remove", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Collection", "removeAll", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Collection", "retainAll", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Collection", "clear", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.Collection", "stream",
               lsm_type_named(arena, "java.util.stream.Stream"));
    REG_METHOD("java.util.Collection", "parallelStream",
               lsm_type_named(arena, "java.util.stream.Stream"));
    REG_METHOD("java.util.Collection", "forEach", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.Collection", "removeIf", lsm_type_builtin(arena, "boolean"));

    /* ── List methods ─────────────────────────────────────────── */
    REG_METHOD("java.util.List", "get", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.List", "set", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.List", "add", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.List", "remove", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.List", "indexOf", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.List", "lastIndexOf", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.List", "subList", lsm_type_named(arena, "java.util.List"));
    REG_METHOD("java.util.List", "of", lsm_type_named(arena, "java.util.List"));
    REG_METHOD("java.util.List", "copyOf", lsm_type_named(arena, "java.util.List"));
    REG_METHOD("java.util.List", "size", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.List", "isEmpty", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.List", "contains", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.List", "iterator", lsm_type_named(arena, "java.util.Iterator"));
    REG_METHOD("java.util.List", "stream",
               lsm_type_named(arena, "java.util.stream.Stream"));
    REG_METHOD("java.util.List", "forEach", lsm_type_builtin(arena, "void"));

    /* ── ArrayList ────────────────────────────────────────────── */
    REG_METHOD("java.util.ArrayList", "get", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.ArrayList", "set", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.ArrayList", "add", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.ArrayList", "remove", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.ArrayList", "size", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.ArrayList", "isEmpty", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.ArrayList", "indexOf", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.ArrayList", "iterator", lsm_type_named(arena, "java.util.Iterator"));
    REG_METHOD("java.util.ArrayList", "clear", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.ArrayList", "stream",
               lsm_type_named(arena, "java.util.stream.Stream"));
    REG_METHOD("java.util.ArrayList", "toArray",
               lsm_type_slice(arena, lsm_type_named(arena, "java.lang.Object")));
    REG_METHOD("java.util.ArrayList", "subList", lsm_type_named(arena, "java.util.List"));
    REG_METHOD("java.util.ArrayList", "trimToSize", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.ArrayList", "ensureCapacity", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.ArrayList", "forEach", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.ArrayList", "removeIf", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.List", "removeIf", lsm_type_builtin(arena, "boolean"));
    REG_CTOR("java.util.ArrayList", "ArrayList");

    REG_METHOD("java.util.LinkedList", "addFirst", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.LinkedList", "addLast", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.LinkedList", "removeFirst", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.LinkedList", "removeLast", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.LinkedList", "getFirst", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.LinkedList", "getLast", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.LinkedList", "peek", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.LinkedList", "poll", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.LinkedList", "offer", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.LinkedList", "size", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.LinkedList", "iterator", lsm_type_named(arena, "java.util.Iterator"));
    REG_CTOR("java.util.LinkedList", "LinkedList");

    /* ── Set methods ──────────────────────────────────────────── */
    REG_METHOD("java.util.Set", "size", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.Set", "isEmpty", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Set", "contains", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Set", "add", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Set", "remove", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Set", "iterator", lsm_type_named(arena, "java.util.Iterator"));
    REG_METHOD("java.util.Set", "of", lsm_type_named(arena, "java.util.Set"));
    REG_METHOD("java.util.Set", "copyOf", lsm_type_named(arena, "java.util.Set"));
    REG_METHOD("java.util.Set", "stream",
               lsm_type_named(arena, "java.util.stream.Stream"));
    REG_METHOD("java.util.Set", "forEach", lsm_type_builtin(arena, "void"));

    REG_METHOD("java.util.HashSet", "add", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.HashSet", "remove", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.HashSet", "contains", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.HashSet", "size", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.HashSet", "isEmpty", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.HashSet", "iterator", lsm_type_named(arena, "java.util.Iterator"));
    REG_METHOD("java.util.HashSet", "clear", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.HashSet", "stream",
               lsm_type_named(arena, "java.util.stream.Stream"));
    REG_CTOR("java.util.HashSet", "HashSet");

    REG_METHOD("java.util.TreeSet", "first", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.TreeSet", "last", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.TreeSet", "headSet", lsm_type_named(arena, "java.util.SortedSet"));
    REG_METHOD("java.util.TreeSet", "tailSet", lsm_type_named(arena, "java.util.SortedSet"));
    REG_CTOR("java.util.TreeSet", "TreeSet");

    REG_CTOR("java.util.LinkedHashSet", "LinkedHashSet");

    /* ── Map methods ──────────────────────────────────────────── */
    REG_METHOD("java.util.Map", "get", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.Map", "put", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.Map", "remove", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.Map", "containsKey", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Map", "containsValue", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Map", "keySet", lsm_type_named(arena, "java.util.Set"));
    REG_METHOD("java.util.Map", "values", lsm_type_named(arena, "java.util.Collection"));
    REG_METHOD("java.util.Map", "entrySet", lsm_type_named(arena, "java.util.Set"));
    REG_METHOD("java.util.Map", "size", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.Map", "isEmpty", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Map", "putAll", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.Map", "clear", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.Map", "getOrDefault", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.Map", "putIfAbsent", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.Map", "computeIfAbsent", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.Map", "compute", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.Map", "merge", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.Map", "of", lsm_type_named(arena, "java.util.Map"));
    REG_METHOD("java.util.Map", "copyOf", lsm_type_named(arena, "java.util.Map"));
    REG_METHOD("java.util.Map", "ofEntries", lsm_type_named(arena, "java.util.Map"));
    REG_METHOD("java.util.Map", "entry", lsm_type_named(arena, "java.util.Map.Entry"));
    REG_METHOD("java.util.Map", "forEach", lsm_type_builtin(arena, "void"));

    REG_METHOD("java.util.Map.Entry", "getKey", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.Map.Entry", "getValue", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.Map.Entry", "setValue", lsm_type_named(arena, "java.lang.Object"));

    REG_METHOD("java.util.HashMap", "get", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.HashMap", "put", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.HashMap", "remove", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.HashMap", "containsKey", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.HashMap", "containsValue", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.HashMap", "size", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.HashMap", "isEmpty", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.HashMap", "keySet", lsm_type_named(arena, "java.util.Set"));
    REG_METHOD("java.util.HashMap", "values", lsm_type_named(arena, "java.util.Collection"));
    REG_METHOD("java.util.HashMap", "entrySet", lsm_type_named(arena, "java.util.Set"));
    REG_METHOD("java.util.HashMap", "clear", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.HashMap", "getOrDefault", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.HashMap", "putIfAbsent", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.HashMap", "forEach", lsm_type_builtin(arena, "void"));
    REG_CTOR("java.util.HashMap", "HashMap");

    REG_METHOD("java.util.TreeMap", "firstKey", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.TreeMap", "lastKey", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.TreeMap", "headMap", lsm_type_named(arena, "java.util.SortedMap"));
    REG_METHOD("java.util.TreeMap", "tailMap", lsm_type_named(arena, "java.util.SortedMap"));
    REG_CTOR("java.util.TreeMap", "TreeMap");

    REG_CTOR("java.util.LinkedHashMap", "LinkedHashMap");

    /* ── Iterator methods ─────────────────────────────────────── */
    REG_METHOD("java.util.Iterator", "hasNext", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Iterator", "next", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.Iterator", "remove", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.Iterator", "forEachRemaining", lsm_type_builtin(arena, "void"));

    /* ── Optional ─────────────────────────────────────────────── */
    REG_METHOD("java.util.Optional", "get", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.Optional", "isPresent", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Optional", "isEmpty", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Optional", "orElse", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.Optional", "orElseGet", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.Optional", "orElseThrow", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.Optional", "ifPresent", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.Optional", "ifPresentOrElse", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.Optional", "map", lsm_type_named(arena, "java.util.Optional"));
    REG_METHOD("java.util.Optional", "flatMap", lsm_type_named(arena, "java.util.Optional"));
    REG_METHOD("java.util.Optional", "filter", lsm_type_named(arena, "java.util.Optional"));
    REG_METHOD("java.util.Optional", "of", lsm_type_named(arena, "java.util.Optional"));
    REG_METHOD("java.util.Optional", "ofNullable", lsm_type_named(arena, "java.util.Optional"));
    REG_METHOD("java.util.Optional", "empty", lsm_type_named(arena, "java.util.Optional"));
    REG_METHOD("java.util.Optional", "stream",
               lsm_type_named(arena, "java.util.stream.Stream"));

    /* ── Arrays / Collections / Objects helpers ───────────────── */
    REG_METHOD("java.util.Arrays", "asList", lsm_type_named(arena, "java.util.List"));
    REG_METHOD("java.util.Arrays", "stream",
               lsm_type_named(arena, "java.util.stream.Stream"));
    REG_METHOD("java.util.Arrays", "sort", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.Arrays", "binarySearch", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.Arrays", "fill", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.Arrays", "copyOf",
               lsm_type_slice(arena, lsm_type_named(arena, "java.lang.Object")));
    REG_METHOD("java.util.Arrays", "copyOfRange",
               lsm_type_slice(arena, lsm_type_named(arena, "java.lang.Object")));
    REG_METHOD("java.util.Arrays", "equals", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Arrays", "hashCode", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.Arrays", "toString", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.util.Arrays", "deepEquals", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Arrays", "deepToString", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.util.Arrays", "deepHashCode", lsm_type_builtin(arena, "int"));

    REG_METHOD("java.util.Collections", "sort", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.Collections", "reverse", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.Collections", "shuffle", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.Collections", "min", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.Collections", "max", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.Collections", "emptyList", lsm_type_named(arena, "java.util.List"));
    REG_METHOD("java.util.Collections", "emptySet", lsm_type_named(arena, "java.util.Set"));
    REG_METHOD("java.util.Collections", "emptyMap", lsm_type_named(arena, "java.util.Map"));
    REG_METHOD("java.util.Collections", "singletonList",
               lsm_type_named(arena, "java.util.List"));
    REG_METHOD("java.util.Collections", "singleton",
               lsm_type_named(arena, "java.util.Set"));
    REG_METHOD("java.util.Collections", "unmodifiableList",
               lsm_type_named(arena, "java.util.List"));
    REG_METHOD("java.util.Collections", "unmodifiableSet",
               lsm_type_named(arena, "java.util.Set"));
    REG_METHOD("java.util.Collections", "unmodifiableMap",
               lsm_type_named(arena, "java.util.Map"));
    REG_METHOD("java.util.Collections", "frequency", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.Collections", "binarySearch", lsm_type_builtin(arena, "int"));

    REG_METHOD("java.util.Objects", "equals", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Objects", "hashCode", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.Objects", "hash", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.Objects", "toString", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.util.Objects", "isNull", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Objects", "nonNull", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Objects", "requireNonNull", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.Objects", "requireNonNullElse",
               lsm_type_named(arena, "java.lang.Object"));

    /* ── UUID, Random, Scanner ────────────────────────────────── */
    REG_METHOD("java.util.UUID", "randomUUID", lsm_type_named(arena, "java.util.UUID"));
    REG_METHOD("java.util.UUID", "fromString", lsm_type_named(arena, "java.util.UUID"));
    REG_METHOD("java.util.UUID", "toString", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.util.UUID", "getMostSignificantBits", lsm_type_builtin(arena, "long"));
    REG_METHOD("java.util.UUID", "getLeastSignificantBits", lsm_type_builtin(arena, "long"));
    REG_CTOR("java.util.UUID", "UUID");

    REG_METHOD("java.util.Random", "nextInt", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.Random", "nextLong", lsm_type_builtin(arena, "long"));
    REG_METHOD("java.util.Random", "nextDouble", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.util.Random", "nextFloat", lsm_type_builtin(arena, "float"));
    REG_METHOD("java.util.Random", "nextBoolean", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Random", "nextGaussian", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.util.Random", "setSeed", lsm_type_builtin(arena, "void"));
    REG_CTOR("java.util.Random", "Random");

    REG_METHOD("java.util.Scanner", "next", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.util.Scanner", "nextLine", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.util.Scanner", "nextInt", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.Scanner", "nextLong", lsm_type_builtin(arena, "long"));
    REG_METHOD("java.util.Scanner", "nextDouble", lsm_type_builtin(arena, "double"));
    REG_METHOD("java.util.Scanner", "hasNext", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Scanner", "hasNextLine", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Scanner", "hasNextInt", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Scanner", "close", lsm_type_builtin(arena, "void"));
    REG_CTOR("java.util.Scanner", "Scanner");

    /* ── Locale / Date / Calendar / TimeZone ──────────────────── */
    REG_METHOD("java.util.Locale", "getLanguage", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.util.Locale", "getCountry", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.util.Locale", "toString", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.util.Locale", "getDefault", lsm_type_named(arena, "java.util.Locale"));
    REG_CTOR("java.util.Locale", "Locale");

    REG_METHOD("java.util.Date", "getTime", lsm_type_builtin(arena, "long"));
    REG_METHOD("java.util.Date", "setTime", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.Date", "before", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Date", "after", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.Date", "compareTo", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.Date", "toString", lsm_type_named(arena, "java.lang.String"));
    REG_CTOR("java.util.Date", "Date");

    REG_METHOD("java.util.Calendar", "getInstance", lsm_type_named(arena, "java.util.Calendar"));
    REG_METHOD("java.util.Calendar", "getTime", lsm_type_named(arena, "java.util.Date"));
    REG_METHOD("java.util.Calendar", "set", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.Calendar", "get", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.Calendar", "add", lsm_type_builtin(arena, "void"));

    REG_METHOD("java.util.TimeZone", "getDefault", lsm_type_named(arena, "java.util.TimeZone"));
    REG_METHOD("java.util.TimeZone", "getID", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.util.TimeZone", "getTimeZone", lsm_type_named(arena, "java.util.TimeZone"));

    /* ── regex ────────────────────────────────────────────────── */
    REG_METHOD("java.util.regex.Pattern", "compile",
               lsm_type_named(arena, "java.util.regex.Pattern"));
    REG_METHOD("java.util.regex.Pattern", "matcher",
               lsm_type_named(arena, "java.util.regex.Matcher"));
    REG_METHOD("java.util.regex.Pattern", "matches", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.regex.Pattern", "split",
               lsm_type_slice(arena, lsm_type_named(arena, "java.lang.String")));
    REG_METHOD("java.util.regex.Pattern", "pattern", lsm_type_named(arena, "java.lang.String"));

    REG_METHOD("java.util.regex.Matcher", "matches", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.regex.Matcher", "find", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.regex.Matcher", "group", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.util.regex.Matcher", "groupCount", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.regex.Matcher", "start", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.regex.Matcher", "end", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.regex.Matcher", "replaceAll", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.util.regex.Matcher", "replaceFirst",
               lsm_type_named(arena, "java.lang.String"));

    /* ── java.io ──────────────────────────────────────────────── */
    REG_TYPE("java.io.InputStream", "InputStream", false, parents_inputstream);
    REG_TYPE("java.io.OutputStream", "OutputStream", false, parents_outputstream);
    REG_TYPE("java.io.Reader", "Reader", false, parents_reader);
    REG_TYPE("java.io.Writer", "Writer", false, parents_writer);
    REG_TYPE("java.io.BufferedReader", "BufferedReader", false, parents_buffered_reader);
    REG_TYPE("java.io.BufferedWriter", "BufferedWriter", false, parents_buffered_writer);
    REG_TYPE("java.io.PrintStream", "PrintStream", false, parents_print_stream);
    REG_TYPE("java.io.PrintWriter", "PrintWriter", false, parents_print_writer);
    REG_TYPE("java.io.FileInputStream", "FileInputStream", false, parents_file_input_stream);
    REG_TYPE("java.io.FileOutputStream", "FileOutputStream", false, parents_file_output_stream);
    REG_TYPE("java.io.FileReader", "FileReader", false, parents_file_reader);
    REG_TYPE("java.io.FileWriter", "FileWriter", false, parents_file_writer);
    REG_TYPE("java.io.File", "File", false, parents_object);
    REG_TYPE("java.io.IOException", "IOException", false, parents_io_exception);
    REG_TYPE("java.io.FileNotFoundException", "FileNotFoundException", false,
             parents_file_not_found_exc);
    REG_TYPE("java.io.UncheckedIOException", "UncheckedIOException", false,
             parents_runtime_exc_chain);
    REG_TYPE("java.io.Serializable", "Serializable", true, no_parents);
    REG_TYPE("java.io.Closeable", "Closeable", true,
             parents_closeable);
    REG_TYPE("java.io.Flushable", "Flushable", true, no_parents);

    REG_METHOD("java.io.PrintStream", "println", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.io.PrintStream", "print", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.io.PrintStream", "printf", lsm_type_named(arena, "java.io.PrintStream"));
    REG_METHOD("java.io.PrintStream", "format", lsm_type_named(arena, "java.io.PrintStream"));
    REG_METHOD("java.io.PrintStream", "write", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.io.PrintStream", "flush", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.io.PrintStream", "close", lsm_type_builtin(arena, "void"));

    REG_METHOD("java.io.PrintWriter", "println", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.io.PrintWriter", "print", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.io.PrintWriter", "printf", lsm_type_named(arena, "java.io.PrintWriter"));
    REG_METHOD("java.io.PrintWriter", "flush", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.io.PrintWriter", "close", lsm_type_builtin(arena, "void"));
    REG_CTOR("java.io.PrintWriter", "PrintWriter");

    REG_METHOD("java.io.InputStream", "read", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.io.InputStream", "close", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.io.InputStream", "available", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.io.InputStream", "skip", lsm_type_builtin(arena, "long"));
    REG_METHOD("java.io.InputStream", "readAllBytes",
               lsm_type_slice(arena, lsm_type_builtin(arena, "byte")));

    REG_METHOD("java.io.OutputStream", "write", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.io.OutputStream", "flush", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.io.OutputStream", "close", lsm_type_builtin(arena, "void"));

    REG_METHOD("java.io.Reader", "read", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.io.Reader", "close", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.io.Reader", "ready", lsm_type_builtin(arena, "boolean"));

    REG_METHOD("java.io.Writer", "write", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.io.Writer", "flush", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.io.Writer", "close", lsm_type_builtin(arena, "void"));

    REG_METHOD("java.io.BufferedReader", "readLine", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.io.BufferedReader", "lines",
               lsm_type_named(arena, "java.util.stream.Stream"));
    REG_METHOD("java.io.BufferedReader", "close", lsm_type_builtin(arena, "void"));
    REG_CTOR("java.io.BufferedReader", "BufferedReader");

    REG_METHOD("java.io.BufferedWriter", "write", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.io.BufferedWriter", "newLine", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.io.BufferedWriter", "flush", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.io.BufferedWriter", "close", lsm_type_builtin(arena, "void"));
    REG_CTOR("java.io.BufferedWriter", "BufferedWriter");

    REG_METHOD("java.io.File", "exists", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.io.File", "isFile", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.io.File", "isDirectory", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.io.File", "canRead", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.io.File", "canWrite", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.io.File", "getName", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.io.File", "getPath", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.io.File", "getAbsolutePath", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.io.File", "getCanonicalPath", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.io.File", "getParent", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.io.File", "getParentFile", lsm_type_named(arena, "java.io.File"));
    REG_METHOD("java.io.File", "length", lsm_type_builtin(arena, "long"));
    REG_METHOD("java.io.File", "lastModified", lsm_type_builtin(arena, "long"));
    REG_METHOD("java.io.File", "mkdir", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.io.File", "mkdirs", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.io.File", "delete", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.io.File", "renameTo", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.io.File", "list",
               lsm_type_slice(arena, lsm_type_named(arena, "java.lang.String")));
    REG_METHOD("java.io.File", "listFiles",
               lsm_type_slice(arena, lsm_type_named(arena, "java.io.File")));
    REG_METHOD("java.io.File", "toPath", lsm_type_named(arena, "java.nio.file.Path"));
    REG_METHOD("java.io.File", "toURI", lsm_type_named(arena, "java.net.URI"));
    REG_CTOR("java.io.File", "File");

    /* ── java.nio.file ───────────────────────────────────────── */
    REG_TYPE("java.nio.file.Path", "Path", true, no_parents);
    REG_TYPE("java.nio.file.Paths", "Paths", false, parents_object);
    REG_TYPE("java.nio.file.Files", "Files", false, parents_object);

    REG_METHOD("java.nio.file.Path", "getFileName", lsm_type_named(arena, "java.nio.file.Path"));
    REG_METHOD("java.nio.file.Path", "getParent", lsm_type_named(arena, "java.nio.file.Path"));
    REG_METHOD("java.nio.file.Path", "getRoot", lsm_type_named(arena, "java.nio.file.Path"));
    REG_METHOD("java.nio.file.Path", "resolve", lsm_type_named(arena, "java.nio.file.Path"));
    REG_METHOD("java.nio.file.Path", "resolveSibling",
               lsm_type_named(arena, "java.nio.file.Path"));
    REG_METHOD("java.nio.file.Path", "relativize", lsm_type_named(arena, "java.nio.file.Path"));
    REG_METHOD("java.nio.file.Path", "normalize", lsm_type_named(arena, "java.nio.file.Path"));
    REG_METHOD("java.nio.file.Path", "toAbsolutePath",
               lsm_type_named(arena, "java.nio.file.Path"));
    REG_METHOD("java.nio.file.Path", "toString", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.nio.file.Path", "toFile", lsm_type_named(arena, "java.io.File"));
    REG_METHOD("java.nio.file.Path", "of", lsm_type_named(arena, "java.nio.file.Path"));

    REG_METHOD("java.nio.file.Paths", "get", lsm_type_named(arena, "java.nio.file.Path"));

    REG_METHOD("java.nio.file.Files", "exists", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.nio.file.Files", "isDirectory", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.nio.file.Files", "isRegularFile", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.nio.file.Files", "readString", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.nio.file.Files", "writeString", lsm_type_named(arena, "java.nio.file.Path"));
    REG_METHOD("java.nio.file.Files", "readAllLines", lsm_type_named(arena, "java.util.List"));
    REG_METHOD("java.nio.file.Files", "readAllBytes",
               lsm_type_slice(arena, lsm_type_builtin(arena, "byte")));
    REG_METHOD("java.nio.file.Files", "lines",
               lsm_type_named(arena, "java.util.stream.Stream"));
    REG_METHOD("java.nio.file.Files", "list",
               lsm_type_named(arena, "java.util.stream.Stream"));
    REG_METHOD("java.nio.file.Files", "walk",
               lsm_type_named(arena, "java.util.stream.Stream"));
    REG_METHOD("java.nio.file.Files", "createDirectory",
               lsm_type_named(arena, "java.nio.file.Path"));
    REG_METHOD("java.nio.file.Files", "createDirectories",
               lsm_type_named(arena, "java.nio.file.Path"));
    REG_METHOD("java.nio.file.Files", "createFile",
               lsm_type_named(arena, "java.nio.file.Path"));
    REG_METHOD("java.nio.file.Files", "delete", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.nio.file.Files", "deleteIfExists", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.nio.file.Files", "copy", lsm_type_named(arena, "java.nio.file.Path"));
    REG_METHOD("java.nio.file.Files", "move", lsm_type_named(arena, "java.nio.file.Path"));
    REG_METHOD("java.nio.file.Files", "size", lsm_type_builtin(arena, "long"));

    /* ── java.util.function (the 21 functional interfaces) ──── */
    REG_TYPE("java.util.function.Function", "Function", true, no_parents);
    REG_TYPE("java.util.function.BiFunction", "BiFunction", true, no_parents);
    REG_TYPE("java.util.function.Predicate", "Predicate", true, no_parents);
    REG_TYPE("java.util.function.BiPredicate", "BiPredicate", true, no_parents);
    REG_TYPE("java.util.function.Consumer", "Consumer", true, no_parents);
    REG_TYPE("java.util.function.BiConsumer", "BiConsumer", true, no_parents);
    REG_TYPE("java.util.function.Supplier", "Supplier", true, no_parents);
    REG_TYPE("java.util.function.UnaryOperator", "UnaryOperator", true,
             parents_unary_operator);
    REG_TYPE("java.util.function.BinaryOperator", "BinaryOperator", true,
             parents_binary_operator);
    REG_TYPE("java.util.function.IntFunction", "IntFunction", true, no_parents);
    REG_TYPE("java.util.function.LongFunction", "LongFunction", true, no_parents);
    REG_TYPE("java.util.function.DoubleFunction", "DoubleFunction", true, no_parents);
    REG_TYPE("java.util.function.IntPredicate", "IntPredicate", true, no_parents);
    REG_TYPE("java.util.function.LongPredicate", "LongPredicate", true, no_parents);
    REG_TYPE("java.util.function.DoublePredicate", "DoublePredicate", true, no_parents);
    REG_TYPE("java.util.function.IntConsumer", "IntConsumer", true, no_parents);
    REG_TYPE("java.util.function.LongConsumer", "LongConsumer", true, no_parents);
    REG_TYPE("java.util.function.DoubleConsumer", "DoubleConsumer", true, no_parents);
    REG_TYPE("java.util.function.IntSupplier", "IntSupplier", true, no_parents);
    REG_TYPE("java.util.function.LongSupplier", "LongSupplier", true, no_parents);
    REG_TYPE("java.util.function.DoubleSupplier", "DoubleSupplier", true, no_parents);
    REG_TYPE("java.util.function.BooleanSupplier", "BooleanSupplier", true, no_parents);
    REG_TYPE("java.util.function.ToIntFunction", "ToIntFunction", true, no_parents);
    REG_TYPE("java.util.function.ToLongFunction", "ToLongFunction", true, no_parents);
    REG_TYPE("java.util.function.ToDoubleFunction", "ToDoubleFunction", true, no_parents);

    REG_METHOD("java.util.function.Function", "apply", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.function.Function", "compose",
               lsm_type_named(arena, "java.util.function.Function"));
    REG_METHOD("java.util.function.Function", "andThen",
               lsm_type_named(arena, "java.util.function.Function"));
    REG_METHOD("java.util.function.Function", "identity",
               lsm_type_named(arena, "java.util.function.Function"));

    REG_METHOD("java.util.function.BiFunction", "apply",
               lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.function.BiFunction", "andThen",
               lsm_type_named(arena, "java.util.function.BiFunction"));

    REG_METHOD("java.util.function.Predicate", "test", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.function.Predicate", "and",
               lsm_type_named(arena, "java.util.function.Predicate"));
    REG_METHOD("java.util.function.Predicate", "or",
               lsm_type_named(arena, "java.util.function.Predicate"));
    REG_METHOD("java.util.function.Predicate", "negate",
               lsm_type_named(arena, "java.util.function.Predicate"));
    REG_METHOD("java.util.function.Predicate", "isEqual",
               lsm_type_named(arena, "java.util.function.Predicate"));

    REG_METHOD("java.util.function.Consumer", "accept", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.function.Consumer", "andThen",
               lsm_type_named(arena, "java.util.function.Consumer"));

    REG_METHOD("java.util.function.Supplier", "get", lsm_type_named(arena, "java.lang.Object"));

    REG_METHOD("java.util.function.UnaryOperator", "identity",
               lsm_type_named(arena, "java.util.function.UnaryOperator"));
    REG_METHOD("java.util.function.UnaryOperator", "apply",
               lsm_type_named(arena, "java.lang.Object"));

    /* ── java.util.stream ────────────────────────────────────── */
    REG_TYPE("java.util.stream.Stream", "Stream", true, no_parents);
    REG_TYPE("java.util.stream.IntStream", "IntStream", true, no_parents);
    REG_TYPE("java.util.stream.LongStream", "LongStream", true, no_parents);
    REG_TYPE("java.util.stream.DoubleStream", "DoubleStream", true, no_parents);
    REG_TYPE("java.util.stream.Collectors", "Collectors", false, parents_object);
    REG_TYPE("java.util.stream.Collector", "Collector", true, no_parents);

    REG_METHOD("java.util.stream.Stream", "filter",
               lsm_type_named(arena, "java.util.stream.Stream"));
    REG_METHOD("java.util.stream.Stream", "map",
               lsm_type_named(arena, "java.util.stream.Stream"));
    REG_METHOD("java.util.stream.Stream", "flatMap",
               lsm_type_named(arena, "java.util.stream.Stream"));
    REG_METHOD("java.util.stream.Stream", "mapToInt",
               lsm_type_named(arena, "java.util.stream.IntStream"));
    REG_METHOD("java.util.stream.Stream", "mapToLong",
               lsm_type_named(arena, "java.util.stream.LongStream"));
    REG_METHOD("java.util.stream.Stream", "mapToDouble",
               lsm_type_named(arena, "java.util.stream.DoubleStream"));
    REG_METHOD("java.util.stream.Stream", "sorted",
               lsm_type_named(arena, "java.util.stream.Stream"));
    REG_METHOD("java.util.stream.Stream", "distinct",
               lsm_type_named(arena, "java.util.stream.Stream"));
    REG_METHOD("java.util.stream.Stream", "limit",
               lsm_type_named(arena, "java.util.stream.Stream"));
    REG_METHOD("java.util.stream.Stream", "skip",
               lsm_type_named(arena, "java.util.stream.Stream"));
    REG_METHOD("java.util.stream.Stream", "peek",
               lsm_type_named(arena, "java.util.stream.Stream"));
    REG_METHOD("java.util.stream.Stream", "forEach", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.stream.Stream", "forEachOrdered", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.stream.Stream", "toArray",
               lsm_type_slice(arena, lsm_type_named(arena, "java.lang.Object")));
    REG_METHOD("java.util.stream.Stream", "toList", lsm_type_named(arena, "java.util.List"));
    REG_METHOD("java.util.stream.Stream", "reduce", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.stream.Stream", "collect", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.stream.Stream", "count", lsm_type_builtin(arena, "long"));
    REG_METHOD("java.util.stream.Stream", "anyMatch", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.stream.Stream", "allMatch", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.stream.Stream", "noneMatch", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.stream.Stream", "findFirst",
               lsm_type_named(arena, "java.util.Optional"));
    REG_METHOD("java.util.stream.Stream", "findAny",
               lsm_type_named(arena, "java.util.Optional"));
    REG_METHOD("java.util.stream.Stream", "min", lsm_type_named(arena, "java.util.Optional"));
    REG_METHOD("java.util.stream.Stream", "max", lsm_type_named(arena, "java.util.Optional"));
    REG_METHOD("java.util.stream.Stream", "of",
               lsm_type_named(arena, "java.util.stream.Stream"));
    REG_METHOD("java.util.stream.Stream", "empty",
               lsm_type_named(arena, "java.util.stream.Stream"));
    REG_METHOD("java.util.stream.Stream", "concat",
               lsm_type_named(arena, "java.util.stream.Stream"));
    REG_METHOD("java.util.stream.Stream", "iterate",
               lsm_type_named(arena, "java.util.stream.Stream"));
    REG_METHOD("java.util.stream.Stream", "generate",
               lsm_type_named(arena, "java.util.stream.Stream"));

    REG_METHOD("java.util.stream.IntStream", "sum", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.stream.IntStream", "average",
               lsm_type_named(arena, "java.util.OptionalDouble"));
    REG_METHOD("java.util.stream.IntStream", "max",
               lsm_type_named(arena, "java.util.OptionalInt"));
    REG_METHOD("java.util.stream.IntStream", "min",
               lsm_type_named(arena, "java.util.OptionalInt"));
    REG_METHOD("java.util.stream.IntStream", "count", lsm_type_builtin(arena, "long"));
    REG_METHOD("java.util.stream.IntStream", "boxed",
               lsm_type_named(arena, "java.util.stream.Stream"));
    REG_METHOD("java.util.stream.IntStream", "filter",
               lsm_type_named(arena, "java.util.stream.IntStream"));
    REG_METHOD("java.util.stream.IntStream", "map",
               lsm_type_named(arena, "java.util.stream.IntStream"));
    REG_METHOD("java.util.stream.IntStream", "range",
               lsm_type_named(arena, "java.util.stream.IntStream"));
    REG_METHOD("java.util.stream.IntStream", "rangeClosed",
               lsm_type_named(arena, "java.util.stream.IntStream"));
    REG_METHOD("java.util.stream.IntStream", "of",
               lsm_type_named(arena, "java.util.stream.IntStream"));

    REG_METHOD("java.util.stream.Collectors", "toList",
               lsm_type_named(arena, "java.util.stream.Collector"));
    REG_METHOD("java.util.stream.Collectors", "toSet",
               lsm_type_named(arena, "java.util.stream.Collector"));
    REG_METHOD("java.util.stream.Collectors", "toMap",
               lsm_type_named(arena, "java.util.stream.Collector"));
    REG_METHOD("java.util.stream.Collectors", "joining",
               lsm_type_named(arena, "java.util.stream.Collector"));
    REG_METHOD("java.util.stream.Collectors", "groupingBy",
               lsm_type_named(arena, "java.util.stream.Collector"));
    REG_METHOD("java.util.stream.Collectors", "partitioningBy",
               lsm_type_named(arena, "java.util.stream.Collector"));
    REG_METHOD("java.util.stream.Collectors", "counting",
               lsm_type_named(arena, "java.util.stream.Collector"));
    REG_METHOD("java.util.stream.Collectors", "summingInt",
               lsm_type_named(arena, "java.util.stream.Collector"));
    REG_METHOD("java.util.stream.Collectors", "averagingDouble",
               lsm_type_named(arena, "java.util.stream.Collector"));
    REG_METHOD("java.util.stream.Collectors", "mapping",
               lsm_type_named(arena, "java.util.stream.Collector"));
    REG_METHOD("java.util.stream.Collectors", "reducing",
               lsm_type_named(arena, "java.util.stream.Collector"));

    /* ── java.util.concurrent ────────────────────────────────── */
    REG_TYPE("java.util.concurrent.ExecutorService", "ExecutorService", true, no_parents);
    REG_TYPE("java.util.concurrent.Executors", "Executors", false, parents_object);
    REG_TYPE("java.util.concurrent.Future", "Future", true, no_parents);
    REG_TYPE("java.util.concurrent.CompletableFuture", "CompletableFuture", false,
             parents_completable_future);
    REG_TYPE("java.util.concurrent.ConcurrentHashMap", "ConcurrentHashMap", false,
             parents_concurrent_hashmap);
    REG_TYPE("java.util.concurrent.ConcurrentMap", "ConcurrentMap", true, parents_map);
    REG_TYPE("java.util.concurrent.TimeUnit", "TimeUnit", false, parents_object);
    REG_TYPE("java.util.concurrent.atomic.AtomicInteger", "AtomicInteger", false, parents_object);
    REG_TYPE("java.util.concurrent.atomic.AtomicLong", "AtomicLong", false, parents_object);
    REG_TYPE("java.util.concurrent.atomic.AtomicBoolean", "AtomicBoolean", false, parents_object);
    REG_TYPE("java.util.concurrent.atomic.AtomicReference", "AtomicReference", false,
             parents_object);
    REG_TYPE("java.util.concurrent.locks.Lock", "Lock", true, no_parents);
    REG_TYPE("java.util.concurrent.locks.ReentrantLock", "ReentrantLock", false,
             parents_reentrant_lock);
    REG_TYPE("java.util.concurrent.locks.ReadWriteLock", "ReadWriteLock", true, no_parents);

    REG_METHOD("java.util.concurrent.ExecutorService", "submit",
               lsm_type_named(arena, "java.util.concurrent.Future"));
    REG_METHOD("java.util.concurrent.ExecutorService", "execute", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.concurrent.ExecutorService", "shutdown", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.concurrent.ExecutorService", "shutdownNow",
               lsm_type_named(arena, "java.util.List"));
    REG_METHOD("java.util.concurrent.ExecutorService", "awaitTermination",
               lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.concurrent.ExecutorService", "isShutdown",
               lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.concurrent.ExecutorService", "isTerminated",
               lsm_type_builtin(arena, "boolean"));

    REG_METHOD("java.util.concurrent.Executors", "newFixedThreadPool",
               lsm_type_named(arena, "java.util.concurrent.ExecutorService"));
    REG_METHOD("java.util.concurrent.Executors", "newSingleThreadExecutor",
               lsm_type_named(arena, "java.util.concurrent.ExecutorService"));
    REG_METHOD("java.util.concurrent.Executors", "newCachedThreadPool",
               lsm_type_named(arena, "java.util.concurrent.ExecutorService"));
    REG_METHOD("java.util.concurrent.Executors", "newScheduledThreadPool",
               lsm_type_named(arena, "java.util.concurrent.ExecutorService"));

    REG_METHOD("java.util.concurrent.Future", "get", lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.concurrent.Future", "isDone", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.concurrent.Future", "cancel", lsm_type_builtin(arena, "boolean"));

    REG_METHOD("java.util.concurrent.CompletableFuture", "thenApply",
               lsm_type_named(arena, "java.util.concurrent.CompletableFuture"));
    REG_METHOD("java.util.concurrent.CompletableFuture", "thenAccept",
               lsm_type_named(arena, "java.util.concurrent.CompletableFuture"));
    REG_METHOD("java.util.concurrent.CompletableFuture", "thenCompose",
               lsm_type_named(arena, "java.util.concurrent.CompletableFuture"));
    REG_METHOD("java.util.concurrent.CompletableFuture", "thenCombine",
               lsm_type_named(arena, "java.util.concurrent.CompletableFuture"));
    REG_METHOD("java.util.concurrent.CompletableFuture", "exceptionally",
               lsm_type_named(arena, "java.util.concurrent.CompletableFuture"));
    REG_METHOD("java.util.concurrent.CompletableFuture", "join",
               lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.concurrent.CompletableFuture", "supplyAsync",
               lsm_type_named(arena, "java.util.concurrent.CompletableFuture"));
    REG_METHOD("java.util.concurrent.CompletableFuture", "runAsync",
               lsm_type_named(arena, "java.util.concurrent.CompletableFuture"));
    REG_METHOD("java.util.concurrent.CompletableFuture", "completedFuture",
               lsm_type_named(arena, "java.util.concurrent.CompletableFuture"));
    REG_METHOD("java.util.concurrent.CompletableFuture", "allOf",
               lsm_type_named(arena, "java.util.concurrent.CompletableFuture"));
    REG_METHOD("java.util.concurrent.CompletableFuture", "anyOf",
               lsm_type_named(arena, "java.util.concurrent.CompletableFuture"));

    REG_METHOD("java.util.concurrent.atomic.AtomicInteger", "get", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.concurrent.atomic.AtomicInteger", "set", lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.concurrent.atomic.AtomicInteger", "incrementAndGet",
               lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.concurrent.atomic.AtomicInteger", "decrementAndGet",
               lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.concurrent.atomic.AtomicInteger", "getAndIncrement",
               lsm_type_builtin(arena, "int"));
    REG_METHOD("java.util.concurrent.atomic.AtomicInteger", "compareAndSet",
               lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.util.concurrent.atomic.AtomicInteger", "addAndGet",
               lsm_type_builtin(arena, "int"));
    REG_CTOR("java.util.concurrent.atomic.AtomicInteger", "AtomicInteger");

    REG_METHOD("java.util.concurrent.atomic.AtomicLong", "get",
               lsm_type_builtin(arena, "long"));
    REG_METHOD("java.util.concurrent.atomic.AtomicLong", "incrementAndGet",
               lsm_type_builtin(arena, "long"));
    REG_CTOR("java.util.concurrent.atomic.AtomicLong", "AtomicLong");

    REG_METHOD("java.util.concurrent.atomic.AtomicReference", "get",
               lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.concurrent.atomic.AtomicReference", "set",
               lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.concurrent.atomic.AtomicReference", "compareAndSet",
               lsm_type_builtin(arena, "boolean"));
    REG_CTOR("java.util.concurrent.atomic.AtomicReference", "AtomicReference");

    REG_METHOD("java.util.concurrent.locks.ReentrantLock", "lock",
               lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.concurrent.locks.ReentrantLock", "unlock",
               lsm_type_builtin(arena, "void"));
    REG_METHOD("java.util.concurrent.locks.ReentrantLock", "tryLock",
               lsm_type_builtin(arena, "boolean"));
    REG_CTOR("java.util.concurrent.locks.ReentrantLock", "ReentrantLock");

    REG_METHOD("java.util.concurrent.ConcurrentHashMap", "put",
               lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.concurrent.ConcurrentHashMap", "get",
               lsm_type_named(arena, "java.lang.Object"));
    REG_METHOD("java.util.concurrent.ConcurrentHashMap", "putIfAbsent",
               lsm_type_named(arena, "java.lang.Object"));
    REG_CTOR("java.util.concurrent.ConcurrentHashMap", "ConcurrentHashMap");

    /* ── java.time ───────────────────────────────────────────── */
    REG_TYPE("java.time.LocalDate", "LocalDate", false, parents_object);
    REG_TYPE("java.time.LocalTime", "LocalTime", false, parents_object);
    REG_TYPE("java.time.LocalDateTime", "LocalDateTime", false, parents_object);
    REG_TYPE("java.time.ZonedDateTime", "ZonedDateTime", false, parents_object);
    REG_TYPE("java.time.OffsetDateTime", "OffsetDateTime", false, parents_object);
    REG_TYPE("java.time.Instant", "Instant", false, parents_object);
    REG_TYPE("java.time.Duration", "Duration", false, parents_object);
    REG_TYPE("java.time.Period", "Period", false, parents_object);
    REG_TYPE("java.time.ZoneId", "ZoneId", false, parents_object);
    REG_TYPE("java.time.format.DateTimeFormatter", "DateTimeFormatter", false, parents_object);

    REG_METHOD("java.time.LocalDate", "now", lsm_type_named(arena, "java.time.LocalDate"));
    REG_METHOD("java.time.LocalDate", "of", lsm_type_named(arena, "java.time.LocalDate"));
    REG_METHOD("java.time.LocalDate", "parse", lsm_type_named(arena, "java.time.LocalDate"));
    REG_METHOD("java.time.LocalDate", "plusDays", lsm_type_named(arena, "java.time.LocalDate"));
    REG_METHOD("java.time.LocalDate", "minusDays", lsm_type_named(arena, "java.time.LocalDate"));
    REG_METHOD("java.time.LocalDate", "getYear", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.time.LocalDate", "getMonth", lsm_type_named(arena, "java.time.Month"));
    REG_METHOD("java.time.LocalDate", "getDayOfMonth", lsm_type_builtin(arena, "int"));
    REG_METHOD("java.time.LocalDate", "getDayOfWeek",
               lsm_type_named(arena, "java.time.DayOfWeek"));
    REG_METHOD("java.time.LocalDate", "isAfter", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.time.LocalDate", "isBefore", lsm_type_builtin(arena, "boolean"));
    REG_METHOD("java.time.LocalDate", "format", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.time.LocalDate", "toString", lsm_type_named(arena, "java.lang.String"));

    REG_METHOD("java.time.LocalDateTime", "now",
               lsm_type_named(arena, "java.time.LocalDateTime"));
    REG_METHOD("java.time.LocalDateTime", "of",
               lsm_type_named(arena, "java.time.LocalDateTime"));
    REG_METHOD("java.time.LocalDateTime", "parse",
               lsm_type_named(arena, "java.time.LocalDateTime"));
    REG_METHOD("java.time.LocalDateTime", "plusHours",
               lsm_type_named(arena, "java.time.LocalDateTime"));
    REG_METHOD("java.time.LocalDateTime", "minusHours",
               lsm_type_named(arena, "java.time.LocalDateTime"));
    REG_METHOD("java.time.LocalDateTime", "format", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.time.LocalDateTime", "toString", lsm_type_named(arena, "java.lang.String"));

    REG_METHOD("java.time.Instant", "now", lsm_type_named(arena, "java.time.Instant"));
    REG_METHOD("java.time.Instant", "ofEpochMilli", lsm_type_named(arena, "java.time.Instant"));
    REG_METHOD("java.time.Instant", "ofEpochSecond", lsm_type_named(arena, "java.time.Instant"));
    REG_METHOD("java.time.Instant", "toEpochMilli", lsm_type_builtin(arena, "long"));
    REG_METHOD("java.time.Instant", "getEpochSecond", lsm_type_builtin(arena, "long"));
    REG_METHOD("java.time.Instant", "plus", lsm_type_named(arena, "java.time.Instant"));
    REG_METHOD("java.time.Instant", "minus", lsm_type_named(arena, "java.time.Instant"));

    REG_METHOD("java.time.Duration", "ofSeconds", lsm_type_named(arena, "java.time.Duration"));
    REG_METHOD("java.time.Duration", "ofMillis", lsm_type_named(arena, "java.time.Duration"));
    REG_METHOD("java.time.Duration", "ofMinutes", lsm_type_named(arena, "java.time.Duration"));
    REG_METHOD("java.time.Duration", "ofHours", lsm_type_named(arena, "java.time.Duration"));
    REG_METHOD("java.time.Duration", "ofDays", lsm_type_named(arena, "java.time.Duration"));
    REG_METHOD("java.time.Duration", "between", lsm_type_named(arena, "java.time.Duration"));
    REG_METHOD("java.time.Duration", "toMillis", lsm_type_builtin(arena, "long"));
    REG_METHOD("java.time.Duration", "toSeconds", lsm_type_builtin(arena, "long"));
    REG_METHOD("java.time.Duration", "toMinutes", lsm_type_builtin(arena, "long"));

    REG_METHOD("java.time.ZoneId", "of", lsm_type_named(arena, "java.time.ZoneId"));
    REG_METHOD("java.time.ZoneId", "systemDefault", lsm_type_named(arena, "java.time.ZoneId"));
    REG_METHOD("java.time.ZoneId", "getId", lsm_type_named(arena, "java.lang.String"));

    REG_METHOD("java.time.format.DateTimeFormatter", "ofPattern",
               lsm_type_named(arena, "java.time.format.DateTimeFormatter"));
    REG_METHOD("java.time.format.DateTimeFormatter", "format",
               lsm_type_named(arena, "java.lang.String"));

    /* ── java.net (minimal) ──────────────────────────────────── */
    REG_TYPE("java.net.URI", "URI", false, parents_object);
    REG_TYPE("java.net.URL", "URL", false, parents_object);
    REG_METHOD("java.net.URI", "create", lsm_type_named(arena, "java.net.URI"));
    REG_METHOD("java.net.URI", "toString", lsm_type_named(arena, "java.lang.String"));
    REG_METHOD("java.net.URI", "toURL", lsm_type_named(arena, "java.net.URL"));
    REG_METHOD("java.net.URL", "openStream", lsm_type_named(arena, "java.io.InputStream"));
    REG_METHOD("java.net.URL", "toString", lsm_type_named(arena, "java.lang.String"));
    REG_CTOR("java.net.URL", "URL");
    REG_CTOR("java.net.URI", "URI");
}
