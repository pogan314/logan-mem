/*
 * test_language.c — Tests for language detection (filename + extension).
 *
 * RED phase: These tests define the expected behavior for registered languages.
 */
#include "../src/foundation/compat.h"
#include "test_framework.h"
#include "discover/discover.h"

/* ── Extension-based detection ─────────────────────────────────── */

TEST(lang_ext_go) {
    ASSERT_EQ(lsm_language_for_extension(".go"), LSM_LANG_GO);
    PASS();
}
TEST(lang_ext_python) {
    ASSERT_EQ(lsm_language_for_extension(".py"), LSM_LANG_PYTHON);
    PASS();
}
TEST(lang_ext_javascript) {
    ASSERT_EQ(lsm_language_for_extension(".js"), LSM_LANG_JAVASCRIPT);
    PASS();
}
TEST(lang_ext_jsx) {
    ASSERT_EQ(lsm_language_for_extension(".jsx"), LSM_LANG_JAVASCRIPT);
    PASS();
}
/* Issue #197: .mjs (ES modules) / .cjs (CommonJS) were unmapped, so those
 * files were never indexed or searchable. */
TEST(lang_ext_mjs_cjs) {
    ASSERT_EQ(lsm_language_for_extension(".mjs"), LSM_LANG_JAVASCRIPT);
    ASSERT_EQ(lsm_language_for_extension(".cjs"), LSM_LANG_JAVASCRIPT);
    PASS();
}
TEST(lang_ext_mts_cts) {
    ASSERT_EQ(lsm_language_for_extension(".mts"), LSM_LANG_TYPESCRIPT);
    ASSERT_EQ(lsm_language_for_extension(".cts"), LSM_LANG_TYPESCRIPT);
    PASS();
}
TEST(lang_ext_typescript) {
    ASSERT_EQ(lsm_language_for_extension(".ts"), LSM_LANG_TYPESCRIPT);
    PASS();
}
TEST(lang_ext_tsx) {
    ASSERT_EQ(lsm_language_for_extension(".tsx"), LSM_LANG_TSX);
    PASS();
}
TEST(lang_ext_rust) {
    ASSERT_EQ(lsm_language_for_extension(".rs"), LSM_LANG_RUST);
    PASS();
}
TEST(lang_ext_java) {
    ASSERT_EQ(lsm_language_for_extension(".java"), LSM_LANG_JAVA);
    PASS();
}
TEST(lang_ext_cpp) {
    ASSERT_EQ(lsm_language_for_extension(".cpp"), LSM_LANG_CPP);
    PASS();
}
TEST(lang_ext_hpp) {
    ASSERT_EQ(lsm_language_for_extension(".hpp"), LSM_LANG_CPP);
    PASS();
}
TEST(lang_ext_cc) {
    ASSERT_EQ(lsm_language_for_extension(".cc"), LSM_LANG_CPP);
    PASS();
}
TEST(lang_ext_cxx) {
    ASSERT_EQ(lsm_language_for_extension(".cxx"), LSM_LANG_CPP);
    PASS();
}
TEST(lang_ext_hxx) {
    ASSERT_EQ(lsm_language_for_extension(".hxx"), LSM_LANG_CPP);
    PASS();
}
TEST(lang_ext_hh) {
    ASSERT_EQ(lsm_language_for_extension(".hh"), LSM_LANG_CPP);
    PASS();
}
TEST(lang_ext_h) {
    ASSERT_EQ(lsm_language_for_extension(".h"), LSM_LANG_CPP);
    PASS();
}
TEST(lang_ext_ixx) {
    ASSERT_EQ(lsm_language_for_extension(".ixx"), LSM_LANG_CPP);
    PASS();
}
TEST(lang_ext_csharp) {
    ASSERT_EQ(lsm_language_for_extension(".cs"), LSM_LANG_CSHARP);
    PASS();
}
TEST(lang_ext_php) {
    ASSERT_EQ(lsm_language_for_extension(".php"), LSM_LANG_PHP);
    PASS();
}
TEST(lang_ext_lua) {
    ASSERT_EQ(lsm_language_for_extension(".lua"), LSM_LANG_LUA);
    PASS();
}
TEST(lang_ext_scala) {
    ASSERT_EQ(lsm_language_for_extension(".scala"), LSM_LANG_SCALA);
    PASS();
}
TEST(lang_ext_sc) {
    ASSERT_EQ(lsm_language_for_extension(".sc"), LSM_LANG_SCALA);
    PASS();
}
TEST(lang_ext_kotlin) {
    ASSERT_EQ(lsm_language_for_extension(".kt"), LSM_LANG_KOTLIN);
    PASS();
}
TEST(lang_ext_kts) {
    ASSERT_EQ(lsm_language_for_extension(".kts"), LSM_LANG_KOTLIN);
    PASS();
}
TEST(lang_ext_ruby) {
    ASSERT_EQ(lsm_language_for_extension(".rb"), LSM_LANG_RUBY);
    PASS();
}
TEST(lang_ext_rake) {
    ASSERT_EQ(lsm_language_for_extension(".rake"), LSM_LANG_RUBY);
    PASS();
}
TEST(lang_ext_gemspec) {
    ASSERT_EQ(lsm_language_for_extension(".gemspec"), LSM_LANG_RUBY);
    PASS();
}
TEST(lang_ext_c) {
    ASSERT_EQ(lsm_language_for_extension(".c"), LSM_LANG_C);
    PASS();
}
TEST(lang_ext_bash) {
    ASSERT_EQ(lsm_language_for_extension(".sh"), LSM_LANG_BASH);
    PASS();
}
TEST(lang_ext_bash2) {
    ASSERT_EQ(lsm_language_for_extension(".bash"), LSM_LANG_BASH);
    PASS();
}
TEST(lang_ext_zig) {
    ASSERT_EQ(lsm_language_for_extension(".zig"), LSM_LANG_ZIG);
    PASS();
}
TEST(lang_ext_elixir) {
    ASSERT_EQ(lsm_language_for_extension(".ex"), LSM_LANG_ELIXIR);
    PASS();
}
TEST(lang_ext_exs) {
    ASSERT_EQ(lsm_language_for_extension(".exs"), LSM_LANG_ELIXIR);
    PASS();
}
TEST(lang_ext_haskell) {
    ASSERT_EQ(lsm_language_for_extension(".hs"), LSM_LANG_HASKELL);
    PASS();
}
TEST(lang_ext_ocaml) {
    ASSERT_EQ(lsm_language_for_extension(".ml"), LSM_LANG_OCAML);
    PASS();
}
TEST(lang_ext_mli) {
    ASSERT_EQ(lsm_language_for_extension(".mli"), LSM_LANG_OCAML);
    PASS();
}
TEST(lang_ext_swift) {
    ASSERT_EQ(lsm_language_for_extension(".swift"), LSM_LANG_SWIFT);
    PASS();
}
TEST(lang_ext_dart) {
    ASSERT_EQ(lsm_language_for_extension(".dart"), LSM_LANG_DART);
    PASS();
}
TEST(lang_ext_perl) {
    ASSERT_EQ(lsm_language_for_extension(".pl"), LSM_LANG_PERL);
    PASS();
}
TEST(lang_ext_pm) {
    ASSERT_EQ(lsm_language_for_extension(".pm"), LSM_LANG_PERL);
    PASS();
}
TEST(lang_ext_groovy) {
    ASSERT_EQ(lsm_language_for_extension(".groovy"), LSM_LANG_GROOVY);
    PASS();
}
TEST(lang_ext_gradle) {
    ASSERT_EQ(lsm_language_for_extension(".gradle"), LSM_LANG_GROOVY);
    PASS();
}
TEST(lang_ext_erlang) {
    ASSERT_EQ(lsm_language_for_extension(".erl"), LSM_LANG_ERLANG);
    PASS();
}
TEST(lang_ext_r) {
    ASSERT_EQ(lsm_language_for_extension(".r"), LSM_LANG_R);
    PASS();
}
TEST(lang_ext_R) {
    ASSERT_EQ(lsm_language_for_extension(".R"), LSM_LANG_R);
    PASS();
}

/* Tier 2 programming */
TEST(lang_ext_clojure) {
    ASSERT_EQ(lsm_language_for_extension(".clj"), LSM_LANG_CLOJURE);
    PASS();
}
TEST(lang_ext_cljs) {
    ASSERT_EQ(lsm_language_for_extension(".cljs"), LSM_LANG_CLOJURE);
    PASS();
}
TEST(lang_ext_cljc) {
    ASSERT_EQ(lsm_language_for_extension(".cljc"), LSM_LANG_CLOJURE);
    PASS();
}
TEST(lang_ext_fsharp) {
    ASSERT_EQ(lsm_language_for_extension(".fs"), LSM_LANG_FSHARP);
    PASS();
}
TEST(lang_ext_fsi) {
    ASSERT_EQ(lsm_language_for_extension(".fsi"), LSM_LANG_FSHARP);
    PASS();
}
TEST(lang_ext_fsx) {
    ASSERT_EQ(lsm_language_for_extension(".fsx"), LSM_LANG_FSHARP);
    PASS();
}
TEST(lang_ext_julia) {
    ASSERT_EQ(lsm_language_for_extension(".jl"), LSM_LANG_JULIA);
    PASS();
}
TEST(lang_ext_vim) {
    ASSERT_EQ(lsm_language_for_extension(".vim"), LSM_LANG_VIMSCRIPT);
    PASS();
}
TEST(lang_ext_nix) {
    ASSERT_EQ(lsm_language_for_extension(".nix"), LSM_LANG_NIX);
    PASS();
}
TEST(lang_ext_commonlisp) {
    ASSERT_EQ(lsm_language_for_extension(".lisp"), LSM_LANG_COMMONLISP);
    PASS();
}
TEST(lang_ext_lsp) {
    ASSERT_EQ(lsm_language_for_extension(".lsp"), LSM_LANG_COMMONLISP);
    PASS();
}
TEST(lang_ext_cl) {
    ASSERT_EQ(lsm_language_for_extension(".cl"), LSM_LANG_COMMONLISP);
    PASS();
}
TEST(lang_ext_elm) {
    ASSERT_EQ(lsm_language_for_extension(".elm"), LSM_LANG_ELM);
    PASS();
}
TEST(lang_ext_fortran) {
    ASSERT_EQ(lsm_language_for_extension(".f90"), LSM_LANG_FORTRAN);
    PASS();
}
TEST(lang_ext_f95) {
    ASSERT_EQ(lsm_language_for_extension(".f95"), LSM_LANG_FORTRAN);
    PASS();
}
TEST(lang_ext_f03) {
    ASSERT_EQ(lsm_language_for_extension(".f03"), LSM_LANG_FORTRAN);
    PASS();
}
TEST(lang_ext_f08) {
    ASSERT_EQ(lsm_language_for_extension(".f08"), LSM_LANG_FORTRAN);
    PASS();
}
TEST(lang_ext_cuda) {
    ASSERT_EQ(lsm_language_for_extension(".cu"), LSM_LANG_CUDA);
    PASS();
}
TEST(lang_ext_cuh) {
    ASSERT_EQ(lsm_language_for_extension(".cuh"), LSM_LANG_CUDA);
    PASS();
}
TEST(lang_ext_cobol) {
    ASSERT_EQ(lsm_language_for_extension(".cob"), LSM_LANG_COBOL);
    PASS();
}
TEST(lang_ext_cbl) {
    ASSERT_EQ(lsm_language_for_extension(".cbl"), LSM_LANG_COBOL);
    PASS();
}
TEST(lang_ext_verilog) {
    ASSERT_EQ(lsm_language_for_extension(".v"), LSM_LANG_VERILOG);
    PASS();
}
TEST(lang_ext_sv) {
    ASSERT_EQ(lsm_language_for_extension(".sv"), LSM_LANG_VERILOG);
    PASS();
}
TEST(lang_ext_emacslisp) {
    ASSERT_EQ(lsm_language_for_extension(".el"), LSM_LANG_EMACSLISP);
    PASS();
}

/* Scientific/math */
TEST(lang_ext_matlab) {
    ASSERT_EQ(lsm_language_for_extension(".matlab"), LSM_LANG_MATLAB);
    PASS();
}
TEST(lang_ext_mlx) {
    ASSERT_EQ(lsm_language_for_extension(".mlx"), LSM_LANG_MATLAB);
    PASS();
}
TEST(lang_ext_lean) {
    ASSERT_EQ(lsm_language_for_extension(".lean"), LSM_LANG_LEAN);
    PASS();
}
TEST(lang_ext_form) {
    ASSERT_EQ(lsm_language_for_extension(".frm"), LSM_LANG_FORM);
    PASS();
}
TEST(lang_ext_prc) {
    ASSERT_EQ(lsm_language_for_extension(".prc"), LSM_LANG_FORM);
    PASS();
}
TEST(lang_ext_magma) {
    ASSERT_EQ(lsm_language_for_extension(".mag"), LSM_LANG_MAGMA);
    PASS();
}
TEST(lang_ext_magma2) {
    ASSERT_EQ(lsm_language_for_extension(".magma"), LSM_LANG_MAGMA);
    PASS();
}
TEST(lang_ext_wolfram) {
    ASSERT_EQ(lsm_language_for_extension(".wl"), LSM_LANG_WOLFRAM);
    PASS();
}
TEST(lang_ext_wls) {
    ASSERT_EQ(lsm_language_for_extension(".wls"), LSM_LANG_WOLFRAM);
    PASS();
}

/* Helper languages */
TEST(lang_ext_html) {
    ASSERT_EQ(lsm_language_for_extension(".html"), LSM_LANG_HTML);
    PASS();
}
TEST(lang_ext_htm) {
    ASSERT_EQ(lsm_language_for_extension(".htm"), LSM_LANG_HTML);
    PASS();
}
TEST(lang_ext_css) {
    ASSERT_EQ(lsm_language_for_extension(".css"), LSM_LANG_CSS);
    PASS();
}
TEST(lang_ext_scss) {
    ASSERT_EQ(lsm_language_for_extension(".scss"), LSM_LANG_SCSS);
    PASS();
}
TEST(lang_ext_yaml) {
    ASSERT_EQ(lsm_language_for_extension(".yml"), LSM_LANG_YAML);
    PASS();
}
TEST(lang_ext_yaml2) {
    ASSERT_EQ(lsm_language_for_extension(".yaml"), LSM_LANG_YAML);
    PASS();
}
TEST(lang_ext_toml) {
    ASSERT_EQ(lsm_language_for_extension(".toml"), LSM_LANG_TOML);
    PASS();
}
TEST(lang_ext_hcl) {
    ASSERT_EQ(lsm_language_for_extension(".tf"), LSM_LANG_HCL);
    PASS();
}
TEST(lang_ext_hcl2) {
    ASSERT_EQ(lsm_language_for_extension(".hcl"), LSM_LANG_HCL);
    PASS();
}
TEST(lang_ext_sql) {
    ASSERT_EQ(lsm_language_for_extension(".sql"), LSM_LANG_SQL);
    PASS();
}
TEST(lang_ext_dockerfile) {
    ASSERT_EQ(lsm_language_for_extension(".dockerfile"), LSM_LANG_DOCKERFILE);
    PASS();
}
TEST(lang_ext_json) {
    ASSERT_EQ(lsm_language_for_extension(".json"), LSM_LANG_JSON);
    PASS();
}
TEST(lang_ext_xml) {
    ASSERT_EQ(lsm_language_for_extension(".xml"), LSM_LANG_XML);
    PASS();
}
TEST(lang_ext_xsl) {
    ASSERT_EQ(lsm_language_for_extension(".xsl"), LSM_LANG_XML);
    PASS();
}
TEST(lang_ext_xsd) {
    ASSERT_EQ(lsm_language_for_extension(".xsd"), LSM_LANG_XML);
    PASS();
}
TEST(lang_ext_svg) {
    ASSERT_EQ(lsm_language_for_extension(".svg"), LSM_LANG_XML);
    PASS();
}
TEST(lang_ext_markdown) {
    ASSERT_EQ(lsm_language_for_extension(".md"), LSM_LANG_MARKDOWN);
    PASS();
}
TEST(lang_ext_mdx) {
    ASSERT_EQ(lsm_language_for_extension(".mdx"), LSM_LANG_MARKDOWN);
    PASS();
}
TEST(lang_ext_makefile) {
    ASSERT_EQ(lsm_language_for_extension(".mk"), LSM_LANG_MAKEFILE);
    PASS();
}
TEST(lang_ext_cmake) {
    ASSERT_EQ(lsm_language_for_extension(".cmake"), LSM_LANG_CMAKE);
    PASS();
}
TEST(lang_ext_protobuf) {
    ASSERT_EQ(lsm_language_for_extension(".proto"), LSM_LANG_PROTOBUF);
    PASS();
}
TEST(lang_ext_graphql) {
    ASSERT_EQ(lsm_language_for_extension(".graphql"), LSM_LANG_GRAPHQL);
    PASS();
}
TEST(lang_ext_gql) {
    ASSERT_EQ(lsm_language_for_extension(".gql"), LSM_LANG_GRAPHQL);
    PASS();
}
TEST(lang_ext_vue) {
    ASSERT_EQ(lsm_language_for_extension(".vue"), LSM_LANG_VUE);
    PASS();
}
TEST(lang_ext_svelte) {
    ASSERT_EQ(lsm_language_for_extension(".svelte"), LSM_LANG_SVELTE);
    PASS();
}
TEST(lang_ext_meson) {
    ASSERT_EQ(lsm_language_for_extension(".meson"), LSM_LANG_MESON);
    PASS();
}
TEST(lang_ext_glsl) {
    ASSERT_EQ(lsm_language_for_extension(".glsl"), LSM_LANG_GLSL);
    PASS();
}
TEST(lang_ext_vert) {
    ASSERT_EQ(lsm_language_for_extension(".vert"), LSM_LANG_GLSL);
    PASS();
}
TEST(lang_ext_frag) {
    ASSERT_EQ(lsm_language_for_extension(".frag"), LSM_LANG_GLSL);
    PASS();
}
TEST(lang_ext_ini) {
    ASSERT_EQ(lsm_language_for_extension(".ini"), LSM_LANG_INI);
    PASS();
}
TEST(lang_ext_cfg) {
    ASSERT_EQ(lsm_language_for_extension(".cfg"), LSM_LANG_INI);
    PASS();
}
TEST(lang_ext_conf) {
    ASSERT_EQ(lsm_language_for_extension(".conf"), LSM_LANG_INI);
    PASS();
}

/* Unknown extension */
TEST(lang_ext_unknown) {
    ASSERT_EQ(lsm_language_for_extension(".xyz"), LSM_LANG_COUNT);
    PASS();
}
TEST(lang_ext_null) {
    ASSERT_EQ(lsm_language_for_extension(""), LSM_LANG_COUNT);
    PASS();
}

/* ── Filename-based detection ──────────────────────────────────── */

TEST(lang_fn_makefile) {
    ASSERT_EQ(lsm_language_for_filename("Makefile"), LSM_LANG_MAKEFILE);
    PASS();
}
TEST(lang_fn_gnumakefile) {
    ASSERT_EQ(lsm_language_for_filename("GNUmakefile"), LSM_LANG_MAKEFILE);
    PASS();
}
TEST(lang_fn_makefile_lower) {
    ASSERT_EQ(lsm_language_for_filename("makefile"), LSM_LANG_MAKEFILE);
    PASS();
}
TEST(lang_fn_cmake) {
    ASSERT_EQ(lsm_language_for_filename("CMakeLists.txt"), LSM_LANG_CMAKE);
    PASS();
}
TEST(lang_fn_dockerfile) {
    ASSERT_EQ(lsm_language_for_filename("Dockerfile"), LSM_LANG_DOCKERFILE);
    PASS();
}
TEST(lang_fn_meson_build) {
    ASSERT_EQ(lsm_language_for_filename("meson.build"), LSM_LANG_MESON);
    PASS();
}
TEST(lang_fn_meson_opts) {
    ASSERT_EQ(lsm_language_for_filename("meson.options"), LSM_LANG_MESON);
    PASS();
}
TEST(lang_fn_meson_opts_txt) {
    ASSERT_EQ(lsm_language_for_filename("meson_options.txt"), LSM_LANG_MESON);
    PASS();
}
TEST(lang_fn_vimrc) {
    ASSERT_EQ(lsm_language_for_filename(".vimrc"), LSM_LANG_VIMSCRIPT);
    PASS();
}

/* issue #258: .blade.php is a built-in compound extension → Blade by default
 * (previously fell through to the single-extension lookup and was mis-typed as
 * PHP). Plain .php still maps to PHP. */
TEST(lang_fn_blade_php_compound_issue258) {
    ASSERT_EQ(lsm_language_for_filename("login.blade.php"), LSM_LANG_BLADE);
    ASSERT_EQ(lsm_language_for_filename("alert.blade.php"), LSM_LANG_BLADE);
    ASSERT_EQ(lsm_language_for_filename("index.php"), LSM_LANG_PHP);
    PASS();
}

/* Filename with extension falls through to extension lookup */
TEST(lang_fn_main_go) {
    ASSERT_EQ(lsm_language_for_filename("main.go"), LSM_LANG_GO);
    PASS();
}
TEST(lang_fn_test_py) {
    ASSERT_EQ(lsm_language_for_filename("test.py"), LSM_LANG_PYTHON);
    PASS();
}
TEST(lang_fn_unknown) {
    ASSERT_EQ(lsm_language_for_filename("README"), LSM_LANG_COUNT);
    PASS();
}

/* ── Language name ─────────────────────────────────────────────── */

TEST(lang_name_go) {
    ASSERT_STR_EQ(lsm_language_name(LSM_LANG_GO), "Go");
    PASS();
}
TEST(lang_name_python) {
    ASSERT_STR_EQ(lsm_language_name(LSM_LANG_PYTHON), "Python");
    PASS();
}
TEST(lang_name_cpp) {
    ASSERT_STR_EQ(lsm_language_name(LSM_LANG_CPP), "C++");
    PASS();
}
TEST(lang_name_csharp) {
    ASSERT_STR_EQ(lsm_language_name(LSM_LANG_CSHARP), "C#");
    PASS();
}
TEST(lang_name_unknown) {
    ASSERT_STR_EQ(lsm_language_name(LSM_LANG_COUNT), "Unknown");
    PASS();
}

/* ── .m disambiguation ─────────────────────────────────────────── */

/* These tests need temp files with content markers */
TEST(lang_m_objc) {
    /* Write a temp file with Objective-C markers */
    char path[256];
    snprintf(path, sizeof(path), "%s/test_lang_objc.m", lsm_tmpdir());
    FILE *f = fopen(path, "w");
    ASSERT_NOT_NULL(f);
    fprintf(f, "#import <Foundation/Foundation.h>\n@interface Foo : NSObject\n@end\n");
    fclose(f);

    ASSERT_EQ(lsm_disambiguate_m(path), LSM_LANG_OBJC);
    remove(path);
    PASS();
}

TEST(lang_m_magma) {
    char path[256];
    snprintf(path, sizeof(path), "%s/test_lang_magma.m", lsm_tmpdir());
    FILE *f = fopen(path, "w");
    ASSERT_NOT_NULL(f);
    fprintf(f, "function MyFunc(x)\n  return x^2;\nend function;\n");
    fclose(f);

    ASSERT_EQ(lsm_disambiguate_m(path), LSM_LANG_MAGMA);
    remove(path);
    PASS();
}

TEST(lang_m_matlab) {
    char path[256];
    snprintf(path, sizeof(path), "%s/test_lang_matlab.m", lsm_tmpdir());
    FILE *f = fopen(path, "w");
    ASSERT_NOT_NULL(f);
    fprintf(f, "function y = square(x)\n  y = x.^2;\nend\n");
    fclose(f);

    ASSERT_EQ(lsm_disambiguate_m(path), LSM_LANG_MATLAB);
    remove(path);
    PASS();
}

TEST(lang_m_default_on_read_fail) {
    /* Non-existent file defaults to MATLAB */
    ASSERT_EQ(lsm_disambiguate_m("/tmp/nonexistent_file_12345.m"), LSM_LANG_MATLAB);
    PASS();
}

/* --- New languages (auto-generated) --- */
TEST(lang_ext_solidity) {
    ASSERT_EQ(lsm_language_for_extension(".sol"), LSM_LANG_SOLIDITY);
    PASS();
}

TEST(lang_ext_typst) {
    ASSERT_EQ(lsm_language_for_extension(".typ"), LSM_LANG_TYPST);
    PASS();
}

TEST(lang_ext_gdscript) {
    ASSERT_EQ(lsm_language_for_extension(".gd"), LSM_LANG_GDSCRIPT);
    PASS();
}

TEST(lang_ext_gleam) {
    ASSERT_EQ(lsm_language_for_extension(".gleam"), LSM_LANG_GLEAM);
    PASS();
}

TEST(lang_ext_powershell) {
    ASSERT_EQ(lsm_language_for_extension(".ps1"), LSM_LANG_POWERSHELL);
    ASSERT_EQ(lsm_language_for_extension(".psm1"), LSM_LANG_POWERSHELL);
    ASSERT_EQ(lsm_language_for_extension(".psd1"), LSM_LANG_POWERSHELL);
    PASS();
}

TEST(lang_ext_pascal) {
    ASSERT_EQ(lsm_language_for_extension(".pas"), LSM_LANG_PASCAL);
    ASSERT_EQ(lsm_language_for_extension(".lpr"), LSM_LANG_PASCAL);
    ASSERT_EQ(lsm_language_for_extension(".dpr"), LSM_LANG_PASCAL);
    PASS();
}

TEST(lang_ext_d) {
    ASSERT_EQ(lsm_language_for_extension(".d"), LSM_LANG_DLANG);
    PASS();
}

TEST(lang_ext_nim) {
    /* nim grammar removed — .nim/.nims no longer map to a language */
    ASSERT_EQ(lsm_language_for_extension(".nim"), LSM_LANG_COUNT);
    ASSERT_EQ(lsm_language_for_extension(".nims"), LSM_LANG_COUNT);
    PASS();
}

TEST(lang_ext_scheme) {
    ASSERT_EQ(lsm_language_for_extension(".scm"), LSM_LANG_SCHEME);
    ASSERT_EQ(lsm_language_for_extension(".ss"), LSM_LANG_SCHEME);
    PASS();
}

TEST(lang_ext_fennel) {
    ASSERT_EQ(lsm_language_for_extension(".fnl"), LSM_LANG_FENNEL);
    PASS();
}

TEST(lang_ext_fish) {
    ASSERT_EQ(lsm_language_for_extension(".fish"), LSM_LANG_FISH);
    PASS();
}

TEST(lang_ext_awk) {
    ASSERT_EQ(lsm_language_for_extension(".awk"), LSM_LANG_AWK);
    PASS();
}

TEST(lang_ext_zsh) {
    ASSERT_EQ(lsm_language_for_extension(".zsh"), LSM_LANG_ZSH);
    PASS();
}

TEST(lang_ext_tcl) {
    ASSERT_EQ(lsm_language_for_extension(".tcl"), LSM_LANG_TCL);
    PASS();
}

TEST(lang_ext_ada) {
    ASSERT_EQ(lsm_language_for_extension(".adb"), LSM_LANG_ADA);
    ASSERT_EQ(lsm_language_for_extension(".ads"), LSM_LANG_ADA);
    PASS();
}

TEST(lang_ext_agda) {
    ASSERT_EQ(lsm_language_for_extension(".agda"), LSM_LANG_AGDA);
    PASS();
}

TEST(lang_ext_racket) {
    ASSERT_EQ(lsm_language_for_extension(".rkt"), LSM_LANG_RACKET);
    PASS();
}

TEST(lang_ext_odin) {
    ASSERT_EQ(lsm_language_for_extension(".odin"), LSM_LANG_ODIN);
    PASS();
}

TEST(lang_ext_rescript) {
    ASSERT_EQ(lsm_language_for_extension(".res"), LSM_LANG_RESCRIPT);
    ASSERT_EQ(lsm_language_for_extension(".resi"), LSM_LANG_RESCRIPT);
    PASS();
}

TEST(lang_ext_purescript) {
    ASSERT_EQ(lsm_language_for_extension(".purs"), LSM_LANG_PURESCRIPT);
    PASS();
}

TEST(lang_ext_nickel) {
    ASSERT_EQ(lsm_language_for_extension(".ncl"), LSM_LANG_NICKEL);
    PASS();
}

TEST(lang_ext_crystal) {
    ASSERT_EQ(lsm_language_for_extension(".cr"), LSM_LANG_CRYSTAL);
    PASS();
}

TEST(lang_ext_teal) {
    ASSERT_EQ(lsm_language_for_extension(".tl"), LSM_LANG_TEAL);
    PASS();
}

TEST(lang_ext_hare) {
    ASSERT_EQ(lsm_language_for_extension(".ha"), LSM_LANG_HARE);
    PASS();
}

TEST(lang_ext_pony) {
    ASSERT_EQ(lsm_language_for_extension(".pony"), LSM_LANG_PONY);
    PASS();
}

TEST(lang_ext_luau) {
    ASSERT_EQ(lsm_language_for_extension(".luau"), LSM_LANG_LUAU);
    PASS();
}

TEST(lang_ext_qml) {
    ASSERT_EQ(lsm_language_for_extension(".qml"), LSM_LANG_QML);
    PASS();
}

TEST(lang_ext_cfml) {
    ASSERT_EQ(lsm_language_for_extension(".cfc"), LSM_LANG_CFSCRIPT);
    ASSERT_EQ(lsm_language_for_extension(".cfm"), LSM_LANG_CFML);
    PASS();
}

TEST(lang_ext_helm_tpl) {
    ASSERT_EQ(lsm_language_for_extension(".tpl"), LSM_LANG_GOTEMPLATE);
    PASS();
}

TEST(lang_ext_janet) {
    ASSERT_EQ(lsm_language_for_extension(".janet"), LSM_LANG_JANET);
    PASS();
}

TEST(lang_ext_sway) {
    ASSERT_EQ(lsm_language_for_extension(".sw"), LSM_LANG_SWAY);
    PASS();
}

TEST(lang_ext_nasm) {
    ASSERT_EQ(lsm_language_for_extension(".nasm"), LSM_LANG_NASM);
    PASS();
}

TEST(lang_ext_assembly) {
    ASSERT_EQ(lsm_language_for_extension(".s"), LSM_LANG_ASSEMBLY);
    ASSERT_EQ(lsm_language_for_extension(".S"), LSM_LANG_ASSEMBLY);
    PASS();
}

TEST(lang_ext_astro) {
    ASSERT_EQ(lsm_language_for_extension(".astro"), LSM_LANG_ASTRO);
    PASS();
}

TEST(lang_ext_gotemplate) {
    ASSERT_EQ(lsm_language_for_extension(".tmpl"), LSM_LANG_GOTEMPLATE);
    ASSERT_EQ(lsm_language_for_extension(".gotmpl"), LSM_LANG_GOTEMPLATE);
    PASS();
}

TEST(lang_ext_templ) {
    ASSERT_EQ(lsm_language_for_extension(".templ"), LSM_LANG_TEMPL);
    PASS();
}

TEST(lang_ext_liquid) {
    ASSERT_EQ(lsm_language_for_extension(".liquid"), LSM_LANG_LIQUID);
    PASS();
}

TEST(lang_ext_jinja2) {
    ASSERT_EQ(lsm_language_for_extension(".j2"), LSM_LANG_JINJA2);
    ASSERT_EQ(lsm_language_for_extension(".jinja2"), LSM_LANG_JINJA2);
    ASSERT_EQ(lsm_language_for_extension(".jinja"), LSM_LANG_JINJA2);
    PASS();
}

TEST(lang_ext_prisma) {
    ASSERT_EQ(lsm_language_for_extension(".prisma"), LSM_LANG_PRISMA);
    PASS();
}

TEST(lang_ext_hyprlang) {
    ASSERT_EQ(lsm_language_for_extension(".hl"), LSM_LANG_HYPRLANG);
    PASS();
}

TEST(lang_ext_diff) {
    ASSERT_EQ(lsm_language_for_extension(".diff"), LSM_LANG_DIFF);
    ASSERT_EQ(lsm_language_for_extension(".patch"), LSM_LANG_DIFF);
    PASS();
}

TEST(lang_ext_wgsl) {
    ASSERT_EQ(lsm_language_for_extension(".wgsl"), LSM_LANG_WGSL);
    PASS();
}

TEST(lang_ext_kdl) {
    ASSERT_EQ(lsm_language_for_extension(".kdl"), LSM_LANG_KDL);
    PASS();
}

TEST(lang_ext_json5) {
    ASSERT_EQ(lsm_language_for_extension(".json5"), LSM_LANG_JSON5);
    PASS();
}

TEST(lang_ext_jsonnet) {
    ASSERT_EQ(lsm_language_for_extension(".jsonnet"), LSM_LANG_JSONNET);
    ASSERT_EQ(lsm_language_for_extension(".libsonnet"), LSM_LANG_JSONNET);
    PASS();
}

TEST(lang_ext_ron) {
    ASSERT_EQ(lsm_language_for_extension(".ron"), LSM_LANG_RON);
    PASS();
}

TEST(lang_ext_thrift) {
    ASSERT_EQ(lsm_language_for_extension(".thrift"), LSM_LANG_THRIFT);
    PASS();
}

TEST(lang_ext_capnp) {
    ASSERT_EQ(lsm_language_for_extension(".capnp"), LSM_LANG_CAPNP);
    PASS();
}

TEST(lang_ext_properties) {
    ASSERT_EQ(lsm_language_for_extension(".properties"), LSM_LANG_PROPERTIES);
    PASS();
}

TEST(lang_ext_bibtex) {
    ASSERT_EQ(lsm_language_for_extension(".bib"), LSM_LANG_BIBTEX);
    PASS();
}

TEST(lang_ext_starlark) {
    ASSERT_EQ(lsm_language_for_extension(".star"), LSM_LANG_STARLARK);
    ASSERT_EQ(lsm_language_for_extension(".bzl"), LSM_LANG_STARLARK);
    PASS();
}

TEST(lang_ext_bicep) {
    ASSERT_EQ(lsm_language_for_extension(".bicep"), LSM_LANG_BICEP);
    PASS();
}

TEST(lang_ext_csv) {
    ASSERT_EQ(lsm_language_for_extension(".csv"), LSM_LANG_CSV);
    PASS();
}

TEST(lang_ext_hlsl) {
    ASSERT_EQ(lsm_language_for_extension(".hlsl"), LSM_LANG_HLSL);
    ASSERT_EQ(lsm_language_for_extension(".hlsli"), LSM_LANG_HLSL);
    ASSERT_EQ(lsm_language_for_extension(".fx"), LSM_LANG_HLSL);
    PASS();
}

TEST(lang_ext_vhdl) {
    ASSERT_EQ(lsm_language_for_extension(".vhd"), LSM_LANG_VHDL);
    ASSERT_EQ(lsm_language_for_extension(".vhdl"), LSM_LANG_VHDL);
    PASS();
}

TEST(lang_ext_devicetree) {
    ASSERT_EQ(lsm_language_for_extension(".dts"), LSM_LANG_DEVICETREE);
    ASSERT_EQ(lsm_language_for_extension(".dtsi"), LSM_LANG_DEVICETREE);
    ASSERT_EQ(lsm_language_for_extension(".overlay"), LSM_LANG_DEVICETREE);
    PASS();
}

TEST(lang_ext_linkerscript) {
    ASSERT_EQ(lsm_language_for_extension(".ld"), LSM_LANG_LINKERSCRIPT);
    ASSERT_EQ(lsm_language_for_extension(".lds"), LSM_LANG_LINKERSCRIPT);
    PASS();
}

TEST(lang_ext_gn) {
    ASSERT_EQ(lsm_language_for_extension(".gn"), LSM_LANG_GN);
    ASSERT_EQ(lsm_language_for_extension(".gni"), LSM_LANG_GN);
    PASS();
}

TEST(lang_ext_bitbake) {
    ASSERT_EQ(lsm_language_for_extension(".bb"), LSM_LANG_BITBAKE);
    ASSERT_EQ(lsm_language_for_extension(".bbclass"), LSM_LANG_BITBAKE);
    ASSERT_EQ(lsm_language_for_extension(".bbappend"), LSM_LANG_BITBAKE);
    PASS();
}

TEST(lang_ext_smali) {
    ASSERT_EQ(lsm_language_for_extension(".smali"), LSM_LANG_SMALI);
    PASS();
}

TEST(lang_ext_tablegen) {
    ASSERT_EQ(lsm_language_for_extension(".td"), LSM_LANG_TABLEGEN);
    PASS();
}

TEST(lang_ext_ispc) {
    ASSERT_EQ(lsm_language_for_extension(".ispc"), LSM_LANG_ISPC);
    PASS();
}

TEST(lang_ext_cairo) {
    ASSERT_EQ(lsm_language_for_extension(".cairo"), LSM_LANG_CAIRO);
    PASS();
}

TEST(lang_ext_move) {
    ASSERT_EQ(lsm_language_for_extension(".move"), LSM_LANG_MOVE);
    PASS();
}

TEST(lang_ext_mojo) {
    ASSERT_EQ(lsm_language_for_extension(".mojo"), LSM_LANG_MOJO);
    PASS();
}

TEST(lang_ext_squirrel) {
    ASSERT_EQ(lsm_language_for_extension(".nut"), LSM_LANG_SQUIRREL);
    PASS();
}

TEST(lang_ext_func) {
    ASSERT_EQ(lsm_language_for_extension(".fc"), LSM_LANG_FUNC);
    PASS();
}

TEST(lang_ext_rst) {
    ASSERT_EQ(lsm_language_for_extension(".rst"), LSM_LANG_RST);
    PASS();
}

TEST(lang_ext_beancount) {
    ASSERT_EQ(lsm_language_for_extension(".beancount"), LSM_LANG_BEANCOUNT);
    PASS();
}

TEST(lang_ext_mermaid) {
    ASSERT_EQ(lsm_language_for_extension(".mmd"), LSM_LANG_MERMAID);
    ASSERT_EQ(lsm_language_for_extension(".mermaid"), LSM_LANG_MERMAID);
    PASS();
}

TEST(lang_ext_puppet) {
    ASSERT_EQ(lsm_language_for_extension(".pp"), LSM_LANG_PUPPET);
    PASS();
}

TEST(lang_ext_po) {
    ASSERT_EQ(lsm_language_for_extension(".po"), LSM_LANG_PO);
    ASSERT_EQ(lsm_language_for_extension(".pot"), LSM_LANG_PO);
    PASS();
}

TEST(lang_ext_slang) {
    ASSERT_EQ(lsm_language_for_extension(".slang"), LSM_LANG_SLANG);
    PASS();
}

TEST(lang_ext_llvm) {
    ASSERT_EQ(lsm_language_for_extension(".ll"), LSM_LANG_LLVM_IR);
    PASS();
}

TEST(lang_ext_smithy) {
    ASSERT_EQ(lsm_language_for_extension(".smithy"), LSM_LANG_SMITHY);
    PASS();
}

TEST(lang_ext_wit) {
    ASSERT_EQ(lsm_language_for_extension(".wit"), LSM_LANG_WIT);
    PASS();
}

TEST(lang_ext_tlaplus) {
    ASSERT_EQ(lsm_language_for_extension(".tla"), LSM_LANG_TLAPLUS);
    PASS();
}

TEST(lang_ext_pkl) {
    ASSERT_EQ(lsm_language_for_extension(".pkl"), LSM_LANG_PKL);
    PASS();
}

TEST(lang_ext_apex) {
    ASSERT_EQ(lsm_language_for_extension(".cls"), LSM_LANG_APEX);
    ASSERT_EQ(lsm_language_for_extension(".trigger"), LSM_LANG_APEX);
    PASS();
}

TEST(lang_ext_soql) {
    ASSERT_EQ(lsm_language_for_extension(".soql"), LSM_LANG_SOQL);
    PASS();
}

TEST(lang_ext_sosl) {
    ASSERT_EQ(lsm_language_for_extension(".sosl"), LSM_LANG_SOSL);
    PASS();
}

/* --- Ported from lang_test.go: TestForLanguage --- */
TEST(lang_all_have_names) {
    /* Every language enum value from 0 to LSM_LANG_COUNT-1
     * should have a non-"Unknown" name. */
    for (int i = 0; i < LSM_LANG_COUNT; i++) {
        const char *name = lsm_language_name((LSMLanguage)i);
        ASSERT_NOT_NULL(name);
        ASSERT_TRUE(strcmp(name, "Unknown") != 0);
    }
    PASS();
}

/* ── Suite ─────────────────────────────────────────────────────── */

SUITE(language) {
    /* Extension: Tier 1 programming */
    RUN_TEST(lang_ext_go);
    RUN_TEST(lang_ext_python);
    RUN_TEST(lang_ext_javascript);
    RUN_TEST(lang_ext_jsx);
    RUN_TEST(lang_ext_mjs_cjs);
    RUN_TEST(lang_ext_mts_cts);
    RUN_TEST(lang_ext_typescript);
    RUN_TEST(lang_ext_tsx);
    RUN_TEST(lang_ext_rust);
    RUN_TEST(lang_ext_java);
    RUN_TEST(lang_ext_cpp);
    RUN_TEST(lang_ext_hpp);
    RUN_TEST(lang_ext_cc);
    RUN_TEST(lang_ext_cxx);
    RUN_TEST(lang_ext_hxx);
    RUN_TEST(lang_ext_hh);
    RUN_TEST(lang_ext_h);
    RUN_TEST(lang_ext_ixx);
    RUN_TEST(lang_ext_csharp);
    RUN_TEST(lang_ext_php);
    RUN_TEST(lang_ext_lua);
    RUN_TEST(lang_ext_scala);
    RUN_TEST(lang_ext_sc);
    RUN_TEST(lang_ext_kotlin);
    RUN_TEST(lang_ext_kts);
    RUN_TEST(lang_ext_ruby);
    RUN_TEST(lang_ext_rake);
    RUN_TEST(lang_ext_gemspec);
    RUN_TEST(lang_ext_c);
    RUN_TEST(lang_ext_bash);
    RUN_TEST(lang_ext_bash2);
    RUN_TEST(lang_ext_zig);
    RUN_TEST(lang_ext_elixir);
    RUN_TEST(lang_ext_exs);
    RUN_TEST(lang_ext_haskell);
    RUN_TEST(lang_ext_ocaml);
    RUN_TEST(lang_ext_mli);
    RUN_TEST(lang_ext_swift);
    RUN_TEST(lang_ext_dart);
    RUN_TEST(lang_ext_perl);
    RUN_TEST(lang_ext_pm);
    RUN_TEST(lang_ext_groovy);
    RUN_TEST(lang_ext_gradle);
    RUN_TEST(lang_ext_erlang);
    RUN_TEST(lang_ext_r);
    RUN_TEST(lang_ext_R);

    /* Extension: Tier 2 programming */
    RUN_TEST(lang_ext_clojure);
    RUN_TEST(lang_ext_cljs);
    RUN_TEST(lang_ext_cljc);
    RUN_TEST(lang_ext_fsharp);
    RUN_TEST(lang_ext_fsi);
    RUN_TEST(lang_ext_fsx);
    RUN_TEST(lang_ext_julia);
    RUN_TEST(lang_ext_vim);
    RUN_TEST(lang_ext_nix);
    RUN_TEST(lang_ext_commonlisp);
    RUN_TEST(lang_ext_lsp);
    RUN_TEST(lang_ext_cl);
    RUN_TEST(lang_ext_elm);
    RUN_TEST(lang_ext_fortran);
    RUN_TEST(lang_ext_f95);
    RUN_TEST(lang_ext_f03);
    RUN_TEST(lang_ext_f08);
    RUN_TEST(lang_ext_cuda);
    RUN_TEST(lang_ext_cuh);
    RUN_TEST(lang_ext_cobol);
    RUN_TEST(lang_ext_cbl);
    RUN_TEST(lang_ext_verilog);
    RUN_TEST(lang_ext_sv);
    RUN_TEST(lang_ext_emacslisp);

    /* Extension: Scientific/math */
    RUN_TEST(lang_ext_matlab);
    RUN_TEST(lang_ext_mlx);
    RUN_TEST(lang_ext_lean);
    RUN_TEST(lang_ext_form);
    RUN_TEST(lang_ext_prc);
    RUN_TEST(lang_ext_magma);
    RUN_TEST(lang_ext_magma2);
    RUN_TEST(lang_ext_wolfram);
    RUN_TEST(lang_ext_wls);

    /* Extension: Helper languages */
    RUN_TEST(lang_ext_html);
    RUN_TEST(lang_ext_htm);
    RUN_TEST(lang_ext_css);
    RUN_TEST(lang_ext_scss);
    RUN_TEST(lang_ext_yaml);
    RUN_TEST(lang_ext_yaml2);
    RUN_TEST(lang_ext_toml);
    RUN_TEST(lang_ext_hcl);
    RUN_TEST(lang_ext_hcl2);
    RUN_TEST(lang_ext_sql);
    RUN_TEST(lang_ext_dockerfile);
    RUN_TEST(lang_ext_json);
    RUN_TEST(lang_ext_xml);
    RUN_TEST(lang_ext_xsl);
    RUN_TEST(lang_ext_xsd);
    RUN_TEST(lang_ext_svg);
    RUN_TEST(lang_ext_markdown);
    RUN_TEST(lang_ext_mdx);
    RUN_TEST(lang_ext_makefile);
    RUN_TEST(lang_ext_cmake);
    RUN_TEST(lang_ext_protobuf);
    RUN_TEST(lang_ext_graphql);
    RUN_TEST(lang_ext_gql);
    RUN_TEST(lang_ext_vue);
    RUN_TEST(lang_ext_svelte);
    RUN_TEST(lang_ext_meson);
    RUN_TEST(lang_ext_glsl);
    RUN_TEST(lang_ext_vert);
    RUN_TEST(lang_ext_frag);
    RUN_TEST(lang_ext_ini);
    RUN_TEST(lang_ext_cfg);
    RUN_TEST(lang_ext_conf);

    /* Unknown/edge cases */
    RUN_TEST(lang_ext_unknown);
    RUN_TEST(lang_ext_null);

    /* Filename-based */
    RUN_TEST(lang_fn_makefile);
    RUN_TEST(lang_fn_gnumakefile);
    RUN_TEST(lang_fn_makefile_lower);
    RUN_TEST(lang_fn_cmake);
    RUN_TEST(lang_fn_dockerfile);
    RUN_TEST(lang_fn_meson_build);
    RUN_TEST(lang_fn_meson_opts);
    RUN_TEST(lang_fn_meson_opts_txt);
    RUN_TEST(lang_fn_vimrc);
    RUN_TEST(lang_fn_blade_php_compound_issue258);
    RUN_TEST(lang_fn_main_go);
    RUN_TEST(lang_fn_test_py);
    RUN_TEST(lang_fn_unknown);

    /* Language names */
    RUN_TEST(lang_name_go);
    RUN_TEST(lang_name_python);
    RUN_TEST(lang_name_cpp);
    RUN_TEST(lang_name_csharp);
    RUN_TEST(lang_name_unknown);

    /* .m disambiguation */
    RUN_TEST(lang_m_objc);
    RUN_TEST(lang_m_magma);
    RUN_TEST(lang_m_matlab);
    RUN_TEST(lang_m_default_on_read_fail);

    /* Go test ports */
    /* New languages */
    RUN_TEST(lang_ext_solidity);
    RUN_TEST(lang_ext_typst);
    RUN_TEST(lang_ext_gdscript);
    RUN_TEST(lang_ext_gleam);
    RUN_TEST(lang_ext_powershell);
    RUN_TEST(lang_ext_pascal);
    RUN_TEST(lang_ext_d);
    RUN_TEST(lang_ext_nim);
    RUN_TEST(lang_ext_scheme);
    RUN_TEST(lang_ext_fennel);
    RUN_TEST(lang_ext_fish);
    RUN_TEST(lang_ext_awk);
    RUN_TEST(lang_ext_zsh);
    RUN_TEST(lang_ext_tcl);
    RUN_TEST(lang_ext_ada);
    RUN_TEST(lang_ext_agda);
    RUN_TEST(lang_ext_racket);
    RUN_TEST(lang_ext_odin);
    RUN_TEST(lang_ext_rescript);
    RUN_TEST(lang_ext_purescript);
    RUN_TEST(lang_ext_nickel);
    RUN_TEST(lang_ext_crystal);
    RUN_TEST(lang_ext_teal);
    RUN_TEST(lang_ext_hare);
    RUN_TEST(lang_ext_pony);
    RUN_TEST(lang_ext_luau);
    RUN_TEST(lang_ext_qml);
    RUN_TEST(lang_ext_cfml);
    RUN_TEST(lang_ext_helm_tpl);
    RUN_TEST(lang_ext_janet);
    RUN_TEST(lang_ext_sway);
    RUN_TEST(lang_ext_nasm);
    RUN_TEST(lang_ext_assembly);
    RUN_TEST(lang_ext_astro);
    RUN_TEST(lang_ext_gotemplate);
    RUN_TEST(lang_ext_templ);
    RUN_TEST(lang_ext_liquid);
    RUN_TEST(lang_ext_jinja2);
    RUN_TEST(lang_ext_prisma);
    RUN_TEST(lang_ext_hyprlang);
    RUN_TEST(lang_ext_diff);
    RUN_TEST(lang_ext_wgsl);
    RUN_TEST(lang_ext_kdl);
    RUN_TEST(lang_ext_json5);
    RUN_TEST(lang_ext_jsonnet);
    RUN_TEST(lang_ext_ron);
    RUN_TEST(lang_ext_thrift);
    RUN_TEST(lang_ext_capnp);
    RUN_TEST(lang_ext_properties);
    RUN_TEST(lang_ext_bibtex);
    RUN_TEST(lang_ext_starlark);
    RUN_TEST(lang_ext_bicep);
    RUN_TEST(lang_ext_csv);
    RUN_TEST(lang_ext_hlsl);
    RUN_TEST(lang_ext_vhdl);
    RUN_TEST(lang_ext_devicetree);
    RUN_TEST(lang_ext_linkerscript);
    RUN_TEST(lang_ext_gn);
    RUN_TEST(lang_ext_bitbake);
    RUN_TEST(lang_ext_smali);
    RUN_TEST(lang_ext_tablegen);
    RUN_TEST(lang_ext_ispc);
    RUN_TEST(lang_ext_cairo);
    RUN_TEST(lang_ext_move);
    RUN_TEST(lang_ext_mojo);
    RUN_TEST(lang_ext_squirrel);
    RUN_TEST(lang_ext_func);
    RUN_TEST(lang_ext_rst);
    RUN_TEST(lang_ext_beancount);
    RUN_TEST(lang_ext_mermaid);
    RUN_TEST(lang_ext_puppet);
    RUN_TEST(lang_ext_po);
    RUN_TEST(lang_ext_slang);
    RUN_TEST(lang_ext_llvm);
    RUN_TEST(lang_ext_smithy);
    RUN_TEST(lang_ext_wit);
    RUN_TEST(lang_ext_tlaplus);
    RUN_TEST(lang_ext_pkl);
    RUN_TEST(lang_ext_apex);
    RUN_TEST(lang_ext_soql);
    RUN_TEST(lang_ext_sosl);

    RUN_TEST(lang_all_have_names);
}
