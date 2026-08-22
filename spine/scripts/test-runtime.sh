#!/usr/bin/env bash

# Sourced by test harnesses. Host paths stay separate from paths exported to a
# native Windows product binary.
LSM_TEST_RUNTIME_ROOT=""
LSM_TEST_RUNTIME_DIR_HOST=""
LSM_TEST_CACHE_DIR_HOST=""
_LSM_TEST_RUNTIME_CREATED_ROOT=""
_LSM_TEST_RUNTIME_PRODUCT_RUNTIME=""
_LSM_TEST_RUNTIME_PRODUCT_CACHE=""

_lsm_test_runtime_error() {
    echo "test-runtime: $*" >&2
}

_lsm_test_runtime_discard_partial() {
    local root="$1" parent="$2" name="${1##*/}"
    [[ -n "$root" && "$root" != "$name" && "${root%/*}" == "$parent" && ! -L "$root" ]] || return 0
    case "$name" in lsmrt.?*|lsmrt-?*) rm -rf -- "$root" >/dev/null 2>&1 || true ;; esac
}

lsm_test_runtime_init() {
    [[ -z "$_LSM_TEST_RUNTIME_CREATED_ROOT" ]] ||
        { _lsm_test_runtime_error "runtime already initialized"; return 1; }
    local platform windows=0 parent root="" runtime_host cache_host
    local product_runtime product_cache helper_dir helper_native root_native
    platform="$(uname -s)" ||
        { _lsm_test_runtime_error "cannot identify the host platform"; return 1; }
    case "$platform" in
        MINGW*|MSYS*|CYGWIN*) windows=1 ;;
        Darwin) parent="/private/tmp" ;;
        *) parent="/tmp" ;;
    esac
    if (( windows )); then
        command -v cygpath >/dev/null 2>&1 ||
            { _lsm_test_runtime_error "cygpath is required on Windows"; return 1; }
        if [[ -n "${LSM_CI_TEMP_ROOT:-}" ]]; then
            parent="$(cygpath -u "$LSM_CI_TEMP_ROOT")" ||
                { _lsm_test_runtime_error "cannot convert LSM_CI_TEMP_ROOT"; return 1; }
            [[ -d "$parent" && ! -L "$parent" ]] ||
                { _lsm_test_runtime_error "LSM_CI_TEMP_ROOT is not a real directory"; return 1; }
            root="$(mktemp -d "${parent%/}/lsmrt.XXXXXX")" ||
                { _lsm_test_runtime_error "cannot create a private Windows CI root"; return 1; }
        else
            helper_dir="$(CDPATH= cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)" || return 1
            helper_native="$(cygpath -w "$helper_dir/ci/new-protected-temp-root.ps1")" ||
                { _lsm_test_runtime_error "cannot convert protected-root helper"; return 1; }
            root_native="$(MSYS2_ARG_CONV_EXCL='*' powershell.exe -NoProfile \
                -ExecutionPolicy Bypass -File "$helper_native" -Prefix 'lsmrt-')" ||
                { _lsm_test_runtime_error "protected Windows root creation failed"; return 1; }
            root_native="${root_native//$'\r'/}"
            [[ -n "$root_native" && "$root_native" != *$'\n'* ]] ||
                { _lsm_test_runtime_error "protected-root helper returned an invalid path"; return 1; }
            root="$(cygpath -u "$root_native")" ||
                { _lsm_test_runtime_error "cannot convert protected Windows root"; return 1; }
            parent="${root%/*}"
        fi
    else
        root="$(mktemp -d "${parent%/}/lsmrt.XXXXXX")" ||
            { _lsm_test_runtime_error "cannot create private root under $parent"; return 1; }
        chmod 700 "$root" || {
            _lsm_test_runtime_discard_partial "$root" "$parent"
            _lsm_test_runtime_error "cannot protect runtime root $root"
            return 1
        }
    fi
    runtime_host="$root/runtime"
    cache_host="$root/cache"
    mkdir "$runtime_host" "$cache_host" || {
        _lsm_test_runtime_discard_partial "$root" "$parent"
        _lsm_test_runtime_error "cannot create runtime/cache directories"
        return 1
    }
    if (( ! windows )); then
        chmod 700 "$runtime_host" "$cache_host" || {
            _lsm_test_runtime_discard_partial "$root" "$parent"
            _lsm_test_runtime_error "cannot protect runtime/cache directories"
            return 1
        }
        product_runtime="$runtime_host"
        product_cache="$cache_host"
    else
        product_runtime="$(cygpath -m "$runtime_host")" || {
            _lsm_test_runtime_discard_partial "$root" "$parent"
            _lsm_test_runtime_error "cannot convert the runtime directory"
            return 1
        }
        product_cache="$(cygpath -m "$cache_host")" || {
            _lsm_test_runtime_discard_partial "$root" "$parent"
            _lsm_test_runtime_error "cannot convert the cache directory"
            return 1
        }
    fi
    LSM_TEST_RUNTIME_ROOT="$root"
    LSM_TEST_RUNTIME_DIR_HOST="$runtime_host"
    LSM_TEST_CACHE_DIR_HOST="$cache_host"
    _LSM_TEST_RUNTIME_CREATED_ROOT="$root"
    _LSM_TEST_RUNTIME_PRODUCT_RUNTIME="$product_runtime"
    _LSM_TEST_RUNTIME_PRODUCT_CACHE="$product_cache"
    export LSM_RUNTIME_DIR="$product_runtime" LSM_CACHE_DIR="$product_cache"
}

_lsm_test_runtime_daemon() {
    LSM_RUNTIME_DIR="$_LSM_TEST_RUNTIME_PRODUCT_RUNTIME" \
        LSM_CACHE_DIR="$_LSM_TEST_RUNTIME_PRODUCT_CACHE" "$1" daemon "$2"
}

lsm_test_runtime_cleanup() {
    local binary="${1:-}" root="$_LSM_TEST_RUNTIME_CREATED_ROOT"
    local name="${_LSM_TEST_RUNTIME_CREATED_ROOT##*/}" runtime_entry="" active=0
    [[ -n "$root" ]] || return 0
    case "$name" in
        lsmrt.?*|lsmrt-?*) ;;
        *) _lsm_test_runtime_error "refusing cleanup for unexpected basename: $name"; return 0 ;;
    esac
    if [[ "$root" != "$LSM_TEST_RUNTIME_ROOT" || -L "$root" ]]; then
        _lsm_test_runtime_error "refusing cleanup for an unexpected runtime root: $root"
        return 0
    fi
    if [[ ! -e "$root" ]]; then
        _LSM_TEST_RUNTIME_CREATED_ROOT=""
        return 0
    fi
    if [[ ! -d "$root" || "$LSM_TEST_RUNTIME_DIR_HOST" != "$root/runtime" ||
          "$LSM_TEST_CACHE_DIR_HOST" != "$root/cache" ||
          -L "$LSM_TEST_RUNTIME_DIR_HOST" || -L "$LSM_TEST_CACHE_DIR_HOST" ]]; then
        _lsm_test_runtime_error "refusing cleanup after runtime path replacement: $root"
        return 0
    fi
    if [[ -d "$LSM_TEST_RUNTIME_DIR_HOST" ]] &&
       ! runtime_entry="$(find "$LSM_TEST_RUNTIME_DIR_HOST" -mindepth 1 -print -quit 2>/dev/null)"; then
        _lsm_test_runtime_error "cannot inspect runtime state; leaving $root"
        return 0
    fi
    if [[ -n "$runtime_entry" ]]; then
        [[ -x "$binary" ]] || {
            _lsm_test_runtime_error "cannot check the private daemon; leaving $root"
            return 0
        }
        if _lsm_test_runtime_daemon "$binary" status >/dev/null 2>&1; then
            active=1
            _lsm_test_runtime_daemon "$binary" stop >/dev/null 2>&1 || true
            for _attempt in {1..50}; do
                if ! _lsm_test_runtime_daemon "$binary" status >/dev/null 2>&1; then
                    active=0
                    break
                fi
                sleep 0.1
            done
        fi
    fi
    if (( active )); then
        _lsm_test_runtime_error "private daemon is still active; leaving $root"
        return 0
    fi
    if rm -rf -- "$root"; then
        _LSM_TEST_RUNTIME_CREATED_ROOT=""
    else
        _lsm_test_runtime_error "cleanup failed; leaving $root"
    fi
    return 0
}
