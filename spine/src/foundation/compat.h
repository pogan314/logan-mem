/*
 * compat.h — Cross-platform compatibility macros and shims.
 *
 * Provides portable TLS, sleep, strdup/strndup, and getline across
 * POSIX (macOS/Linux) and Windows. Include this instead of using
 * platform-specific macros directly.
 */
#ifndef LSM_COMPAT_H
#define LSM_COMPAT_H

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
/* stdlib.h declares getenv (lsm_tmpdir) and, on Windows, _putenv_s (lsm_setenv/
 * lsm_unsetenv). The x86-64 mingw toolchain pulled it in transitively, but the
 * aarch64 (CLANGARM64) include chain does not, so include it directly — without
 * it those calls become implicit declarations that conflict with the real
 * stdlib.h types and fail to compile on native ARM64 Windows. */
#include <stdlib.h>

/* ── Thread-local storage ─────────────────────────────────────── */
/* _Thread_local is C11 standard — works on GCC, Clang, and MSVC (2019+).
 * __declspec(thread) is MSVC-only and doesn't work on MinGW GCC. */
#define LSM_TLS _Thread_local

/* ── Sleep ────────────────────────────────────────────────────── */
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#define lsm_usleep(us) Sleep((DWORD)((us) / 1000))
#else
#include <unistd.h>
#define lsm_usleep(us) usleep((useconds_t)(us))
#endif

/* ── strdup / strndup ─────────────────────────────────────────── */
#ifdef _WIN32
#define lsm_strdup _strdup
/* Implemented in compat.c */
char *lsm_strndup(const char *s, size_t n);
#else
#define lsm_strdup strdup
#define lsm_strndup strndup
#endif

/* ── getline (Windows lacks it) ───────────────────────────────── */
#ifdef _WIN32
/* Implemented in compat.c */
ssize_t lsm_getline(char **lineptr, size_t *n, FILE *stream);
#else
#define lsm_getline getline
#endif

/* ── fileno ───────────────────────────────────────────────────── */
#ifdef _WIN32
#define lsm_fileno _fileno
#else
#define lsm_fileno fileno
#endif

/* ── strcasestr (Windows lacks it) ────────────────────────────── */
#ifdef _WIN32
/* Implemented in compat.c */
char *lsm_strcasestr(const char *haystack, const char *needle);
#else
#define lsm_strcasestr strcasestr
#endif

/* ── mkdir portability ───────────────────────────────────────── */
#ifdef _WIN32
#include <direct.h>
#define lsm_mkdir(path) _mkdir(path)
#else
#include <sys/stat.h>
#define lsm_mkdir(path) mkdir(path, 0755)
#endif

/* ── clock_gettime / nanosleep (Windows lacks them) ──────────── */
#include <time.h>
#ifdef _WIN32
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif
/* Implemented in compat.c */
int lsm_clock_gettime(int clk_id, struct timespec *tp);
static inline int lsm_nanosleep(const struct timespec *req, struct timespec *rem) {
    (void)rem;
    Sleep((DWORD)(req->tv_sec * 1000 + req->tv_nsec / 1000000));
    return 0;
}
#else
#define lsm_clock_gettime clock_gettime
#define lsm_nanosleep nanosleep
#endif

/* ── gmtime_r (Windows lacks it) ─────────────────────────────── */
#ifdef _WIN32
static inline struct tm *lsm_gmtime_r(const time_t *timep, struct tm *result) {
    return gmtime_s(result, timep) == 0 ? result : NULL;
}
#else
#define lsm_gmtime_r gmtime_r
#endif

/* ── mkdtemp (Windows lacks it) ──────────────────────────────── */
#ifdef _WIN32
/* Translates /tmp/ to %TEMP%\ and copies result back to tmpl.
 * Callers MUST use char buf[LSM_SZ_256] or larger. */
char *lsm_mkdtemp(char *tmpl);
#else
#define lsm_mkdtemp mkdtemp
#endif

/* ── mkstemp (Windows lacks it) ──────────────────────────────── */
#ifdef _WIN32
int lsm_mkstemp(char *tmpl);

#else
#define lsm_mkstemp mkstemp
#endif

/* Rewrite an absolute path into the form the platform's file APIs accept at
 * any length. On Windows, absolute drive paths beyond the legacy 260-char
 * limit are canonicalized and given the extended-length \\?\ prefix
 * (SQLite passes such UTF-8 paths through to CreateFileW unchanged). On
 * POSIX, the path is copied verbatim. Returns false if the buffer is too
 * small or the path cannot be canonicalized. */
bool lsm_path_for_file_api(const char *path, char *out, size_t out_size);

/* ── setenv / unsetenv (Windows lacks them) ──────────────────── */
#ifdef _WIN32
static inline int lsm_setenv(const char *name, const char *value, int overwrite) {
    (void)overwrite;
    if (!name || !value) {
        return EINVAL;
    }
    int name_chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, NULL, 0);
    int value_chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, NULL, 0);
    wchar_t *wide_name =
        name_chars > 0 ? (wchar_t *)malloc((size_t)name_chars * sizeof(*wide_name)) : NULL;
    wchar_t *wide_value =
        value_chars > 0 ? (wchar_t *)malloc((size_t)value_chars * sizeof(*wide_value)) : NULL;
    bool converted =
        wide_name && wide_value &&
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, wide_name, name_chars) > 0 &&
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, wide_value, value_chars) > 0;
    if (!converted) {
        free(wide_name);
        free(wide_value);
        return EINVAL;
    }
    /* Keep the CRT's narrow environment useful for legacy getenv callers,
     * then repair the process-wide Windows environment with the actual UTF-16
     * value. _putenv_s alone routes UTF-8 path bytes through the active ANSI
     * code page, which corrupts non-ASCII cache roots inherited by children. */
    int status = _putenv_s(name, value);
    if (status == 0 && !SetEnvironmentVariableW(wide_name, wide_value)) {
        status = EINVAL;
    }
    free(wide_name);
    free(wide_value);
    return status;
}
static inline int lsm_unsetenv(const char *name) {
    if (!name) {
        return EINVAL;
    }
    int name_chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, NULL, 0);
    wchar_t *wide_name =
        name_chars > 0 ? (wchar_t *)malloc((size_t)name_chars * sizeof(*wide_name)) : NULL;
    if (!wide_name ||
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, wide_name, name_chars) <= 0) {
        free(wide_name);
        return EINVAL;
    }
    int status = _putenv_s(name, "");
    if (status == 0) {
        SetLastError(ERROR_SUCCESS);
        if (!SetEnvironmentVariableW(wide_name, NULL) && GetLastError() != ERROR_ENVVAR_NOT_FOUND) {
            status = EINVAL;
        }
    }
    free(wide_name);
    return status;
}
#else
#define lsm_setenv setenv
#define lsm_unsetenv unsetenv
#endif

/* ── pipe (Windows uses _pipe) ───────────────────────────────── */
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#define lsm_pipe(fds) _pipe(fds, 4096, _O_BINARY)
#else
#define lsm_pipe(fds) pipe(fds)
#endif

/* ── Temp directory helper ───────────────────────────────────── */
static inline const char *lsm_tmpdir(void) {
#ifdef _WIN32
    const char *t = getenv("TEMP");
    if (!t)
        t = getenv("TMP");
    return t ? t : ".";
#else
    return "/tmp";
#endif
}

/* ── Signal handling ──────────────────────────────────────────── */
/* Windows doesn't have sigaction; provide macro to select signal API. */
#ifdef _WIN32
#define LSM_HAS_SIGACTION 0
#else
#define LSM_HAS_SIGACTION 1
#endif

#endif /* LSM_COMPAT_H */
