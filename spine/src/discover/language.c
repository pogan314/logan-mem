/*
 * language.c — Language detection from filename and extension.
 *
 * Maps file extensions and special filenames to LSMLanguage enum values.
 * Handles .m disambiguation (Objective-C vs Magma vs MATLAB).
 * Consults the process-global user config (set via lsm_set_user_lang_config)
 * before the built-in lookup table.
 */
#include "discover/discover.h"
#include "discover/userconfig.h"
#include "lsm.h" // LSMLanguage, LSM_LANG_*

#include "foundation/constants.h"
#include "foundation/compat_fs.h"

enum { LANG_SCAN_PASSES = 2 };
#define SLEN(s) (sizeof(s) - 1)
#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* ── Extension → Language lookup table ───────────────────────────── */

typedef struct {
    const char *ext; /* including dot, e.g. ".go" */
    LSMLanguage language;
} ext_entry_t;

/* Sorted by extension for binary search (but linear scan is fine for ~120 entries) */
static const ext_entry_t EXT_TABLE[] = {
    /* Bash */
    {".bash", LSM_LANG_BASH},
    {".sh", LSM_LANG_BASH},

    /* C */
    {".c", LSM_LANG_C},

    /* C++ */
    {".cc", LSM_LANG_CPP},
    {".ccm", LSM_LANG_CPP},
    {".cpp", LSM_LANG_CPP},
    {".cppm", LSM_LANG_CPP},
    {".cxx", LSM_LANG_CPP},
    {".h", LSM_LANG_CPP},
    {".hh", LSM_LANG_CPP},
    {".hpp", LSM_LANG_CPP},
    {".hxx", LSM_LANG_CPP},
    {".ixx", LSM_LANG_CPP},

    /* C# */
    {".cs", LSM_LANG_CSHARP},

    /* Clojure */
    {".clj", LSM_LANG_CLOJURE},
    {".cljc", LSM_LANG_CLOJURE},
    {".cljs", LSM_LANG_CLOJURE},

    /* CMake */
    {".cmake", LSM_LANG_CMAKE},

    /* COBOL */
    {".cbl", LSM_LANG_COBOL},
    {".cob", LSM_LANG_COBOL},

    /* Common Lisp */
    {".cl", LSM_LANG_COMMONLISP},
    {".lisp", LSM_LANG_COMMONLISP},
    {".lsp", LSM_LANG_COMMONLISP},

    /* CSS */
    {".css", LSM_LANG_CSS},

    /* CUDA */
    {".cu", LSM_LANG_CUDA},
    {".cuh", LSM_LANG_CUDA},

    /* Dart */
    {".dart", LSM_LANG_DART},

    /* Dockerfile */
    {".dockerfile", LSM_LANG_DOCKERFILE},

    /* Elixir */
    {".ex", LSM_LANG_ELIXIR},
    {".exs", LSM_LANG_ELIXIR},

    /* DotEnv */
    {".env", LSM_LANG_DOTENV},

    /* Elm */
    {".elm", LSM_LANG_ELM},

    /* Emacs Lisp */
    {".el", LSM_LANG_EMACSLISP},

    /* Erlang */
    {".erl", LSM_LANG_ERLANG},

    /* F# */
    {".fs", LSM_LANG_FSHARP},
    {".fsi", LSM_LANG_FSHARP},
    {".fsx", LSM_LANG_FSHARP},

    /* FORM */
    {".frm", LSM_LANG_FORM},
    {".prc", LSM_LANG_FORM},

    /* Fortran */
    {".f03", LSM_LANG_FORTRAN},
    {".f08", LSM_LANG_FORTRAN},
    {".f90", LSM_LANG_FORTRAN},
    {".f95", LSM_LANG_FORTRAN},

    /* GLSL */
    {".frag", LSM_LANG_GLSL},
    {".glsl", LSM_LANG_GLSL},
    {".vert", LSM_LANG_GLSL},

    /* Go */
    {".go", LSM_LANG_GO},

    /* GraphQL */
    {".gql", LSM_LANG_GRAPHQL},
    {".graphql", LSM_LANG_GRAPHQL},

    /* Groovy */
    {".gradle", LSM_LANG_GROOVY},
    {".groovy", LSM_LANG_GROOVY},

    /* Haskell */
    {".hs", LSM_LANG_HASKELL},

    /* HCL / Terraform */
    {".hcl", LSM_LANG_HCL},
    {".tf", LSM_LANG_HCL},

    /* HTML */
    {".htm", LSM_LANG_HTML},
    {".html", LSM_LANG_HTML},

    /* INI */
    {".cfg", LSM_LANG_INI},
    {".conf", LSM_LANG_INI},
    {".ini", LSM_LANG_INI},

    /* Java */
    {".java", LSM_LANG_JAVA},

    /* JavaScript */
    {".js", LSM_LANG_JAVASCRIPT},
    {".jsx", LSM_LANG_JAVASCRIPT},
    {".mjs", LSM_LANG_JAVASCRIPT}, /* ES modules (#197) */
    {".cjs", LSM_LANG_JAVASCRIPT}, /* CommonJS modules */

    /* JSON */
    {".json", LSM_LANG_JSON},

    /* Julia */
    {".jl", LSM_LANG_JULIA},

    /* Kotlin */
    {".kt", LSM_LANG_KOTLIN},
    {".kts", LSM_LANG_KOTLIN},

    /* Lean */
    {".lean", LSM_LANG_LEAN},

    /* Lua */
    {".lua", LSM_LANG_LUA},

    /* Magma */
    {".mag", LSM_LANG_MAGMA},
    {".magma", LSM_LANG_MAGMA},

    /* Makefile */
    {".mk", LSM_LANG_MAKEFILE},

    /* Markdown */
    {".md", LSM_LANG_MARKDOWN},
    {".mdx", LSM_LANG_MARKDOWN},

    /* MATLAB */
    {".m", LSM_LANG_MATLAB},
    {".matlab", LSM_LANG_MATLAB},
    {".mlx", LSM_LANG_MATLAB},

    /* Meson */
    {".meson", LSM_LANG_MESON},

    /* Mojo */
    {".mojo", LSM_LANG_MOJO},

    /* Nix */
    {".nix", LSM_LANG_NIX},

    /* OCaml */
    {".ml", LSM_LANG_OCAML},
    {".mli", LSM_LANG_OCAML},

    /* Perl */
    {".pl", LSM_LANG_PERL},
    {".pm", LSM_LANG_PERL},

    /* PHP */
    {".php", LSM_LANG_PHP},

    /* Protobuf */
    {".proto", LSM_LANG_PROTOBUF},

    /* Python */
    {".py", LSM_LANG_PYTHON},

    /* R — case insensitive handled separately */
    {".R", LSM_LANG_R},
    {".r", LSM_LANG_R},

    /* Ruby */
    {".gemspec", LSM_LANG_RUBY},
    {".rake", LSM_LANG_RUBY},
    {".rb", LSM_LANG_RUBY},

    /* Rust */
    {".rs", LSM_LANG_RUST},

    /* Scala */
    {".sc", LSM_LANG_SCALA},
    {".scala", LSM_LANG_SCALA},

    /* SCSS */
    {".scss", LSM_LANG_SCSS},

    /* SQL */
    {".sql", LSM_LANG_SQL},

    /* Svelte */
    {".svelte", LSM_LANG_SVELTE},

    /* Swift */
    {".swift", LSM_LANG_SWIFT},

    /* SystemVerilog + Verilog */
    {".sv", LSM_LANG_VERILOG},
    {".v", LSM_LANG_VERILOG},

    /* TOML */
    {".toml", LSM_LANG_TOML},

    /* TSX */
    {".tsx", LSM_LANG_TSX},

    /* TypeScript */
    {".ts", LSM_LANG_TYPESCRIPT},
    {".mts", LSM_LANG_TYPESCRIPT}, /* TS ES modules */
    {".cts", LSM_LANG_TYPESCRIPT}, /* TS CommonJS modules */

    /* VimScript */
    {".vim", LSM_LANG_VIMSCRIPT},
    {".vimrc", LSM_LANG_VIMSCRIPT},
    {"justfile", LSM_LANG_JUST},
    {"Justfile", LSM_LANG_JUST},
    {".justfile", LSM_LANG_JUST},
    {".just", LSM_LANG_JUST}, /* `import 'common.just'` target files */
    {"hyprland.conf", LSM_LANG_HYPRLANG},
    {"ssh_config", LSM_LANG_SSHCONFIG},
    {"sshd_config", LSM_LANG_SSHCONFIG},
    {"BUILD", LSM_LANG_STARLARK},
    {"BUILD.bazel", LSM_LANG_STARLARK},
    {"WORKSPACE", LSM_LANG_STARLARK},
    {"WORKSPACE.bazel", LSM_LANG_STARLARK},

    /* BitBake include fragments — `require/include foo.inc` target files.
     * NOTE: .inc is also used by ObjectScript include (macro) files; the
     * ambiguity is resolved by content in lsm_disambiguate_inc(). */
    {".inc", LSM_LANG_BITBAKE},

    /* InterSystems ObjectScript routines (.mac/.int/.rtn unambiguous; .cls is
     * shared with Apex and resolved by content in lsm_disambiguate_cls()). */
    {".mac", LSM_LANG_OBJECTSCRIPT_ROUTINE},
    {".int", LSM_LANG_OBJECTSCRIPT_ROUTINE},
    {".rtn", LSM_LANG_OBJECTSCRIPT_ROUTINE},

    /* Vue */
    {".vue", LSM_LANG_VUE},

    /* Wolfram */
    {".wl", LSM_LANG_WOLFRAM},
    {".wls", LSM_LANG_WOLFRAM},

    /* XML */
    {".xml", LSM_LANG_XML},
    {".xsd", LSM_LANG_XML},
    {".xsl", LSM_LANG_XML},
    {".svg", LSM_LANG_XML},

    /* YAML */
    {".yaml", LSM_LANG_YAML},
    {".yml", LSM_LANG_YAML},

    /* Ada */
    {".adb", LSM_LANG_ADA},

    /* Ada */
    {".ads", LSM_LANG_ADA},

    /* Agda */
    {".agda", LSM_LANG_AGDA},

    /* Astro */
    {".astro", LSM_LANG_ASTRO},

    /* AWK */
    {".awk", LSM_LANG_AWK},

    /* BitBake */
    {".bb", LSM_LANG_BITBAKE},

    /* BitBake */
    {".bbappend", LSM_LANG_BITBAKE},

    /* BitBake */
    {".bbclass", LSM_LANG_BITBAKE},

    /* Beancount */
    {".beancount", LSM_LANG_BEANCOUNT},

    /* BibTeX */
    {".bib", LSM_LANG_BIBTEX},

    /* Bicep */
    {".bicep", LSM_LANG_BICEP},

    /* Blade */
    /* .blade.php handled by userconfig compound extensions, not EXT_TABLE */

    /* Starlark */
    {".bzl", LSM_LANG_STARLARK},

    /* Cairo */
    {".cairo", LSM_LANG_CAIRO},

    /* Cap'n Proto */
    {".capnp", LSM_LANG_CAPNP},

    /* Apex */
    {".cls", LSM_LANG_APEX},

    /* Crystal */
    {".cr", LSM_LANG_CRYSTAL},

    /* CSV */
    {".csv", LSM_LANG_CSV},

    /* D */
    {".d", LSM_LANG_DLANG},

    /* Diff */
    {".diff", LSM_LANG_DIFF},

    /* Pascal */
    {".dpr", LSM_LANG_PASCAL},

    /* DeviceTree */
    {".dts", LSM_LANG_DEVICETREE},

    /* DeviceTree */
    {".dtsi", LSM_LANG_DEVICETREE},

    /* FunC */
    {".fc", LSM_LANG_FUNC},

    /* Fish */
    {".fish", LSM_LANG_FISH},

    /* Fennel */
    {".fnl", LSM_LANG_FENNEL},

    /* HLSL */
    {".fx", LSM_LANG_HLSL},

    /* GDScript */
    {".gd", LSM_LANG_GDSCRIPT},

    /* Gleam */
    {".gleam", LSM_LANG_GLEAM},

    /* GN */
    {".gn", LSM_LANG_GN},

    /* GN */
    {".gni", LSM_LANG_GN},

    /* Go Template */
    {".gotmpl", LSM_LANG_GOTEMPLATE},
    {".tpl", LSM_LANG_GOTEMPLATE}, /* Helm _helpers.tpl named-template definitions */

    /* Hare */
    {".ha", LSM_LANG_HARE},

    /* Hyprlang */
    {".hl", LSM_LANG_HYPRLANG},

    /* HLSL */
    {".hlsl", LSM_LANG_HLSL},

    /* HLSL */
    {".hlsli", LSM_LANG_HLSL},

    /* ISPC */
    {".ispc", LSM_LANG_ISPC},

    /* Jinja2 */
    {".j2", LSM_LANG_JINJA2},

    /* Janet */
    {".janet", LSM_LANG_JANET},

    /* Jinja2 */
    {".jinja", LSM_LANG_JINJA2},

    /* Jinja2 */
    {".jinja2", LSM_LANG_JINJA2},

    /* JSON5 */
    {".json5", LSM_LANG_JSON5},

    /* Jsonnet */
    {".jsonnet", LSM_LANG_JSONNET},

    /* KDL */
    {".kdl", LSM_LANG_KDL},

    /* Linker Script */
    {".ld", LSM_LANG_LINKERSCRIPT},

    /* Linker Script */
    {".lds", LSM_LANG_LINKERSCRIPT},

    /* Jsonnet */
    {".libsonnet", LSM_LANG_JSONNET},

    /* Liquid */
    {".liquid", LSM_LANG_LIQUID},

    /* LLVM IR */
    {".ll", LSM_LANG_LLVM_IR},

    /* Pascal */
    {".lpr", LSM_LANG_PASCAL},

    /* Luau */
    {".luau", LSM_LANG_LUAU},

    /* Qt QML */
    {".qml", LSM_LANG_QML},

    /* CFML / ColdFusion — .cfc components are script-dialect; .cfm are tag templates */
    {".cfc", LSM_LANG_CFSCRIPT},
    {".cfm", LSM_LANG_CFML},

    /* Mermaid */
    {".mermaid", LSM_LANG_MERMAID},

    /* Mermaid */
    {".mmd", LSM_LANG_MERMAID},

    /* Move */
    {".move", LSM_LANG_MOVE},

    /* NASM */
    {".nasm", LSM_LANG_NASM},

    /* Nickel */
    {".ncl", LSM_LANG_NICKEL},

    /* Nim */

    /* Nim */

    /* Squirrel */
    {".nut", LSM_LANG_SQUIRREL},

    /* Odin */
    {".odin", LSM_LANG_ODIN},

    /* DeviceTree */
    {".overlay", LSM_LANG_DEVICETREE},

    /* Pascal */
    {".pas", LSM_LANG_PASCAL},

    /* Diff */
    {".patch", LSM_LANG_DIFF},

    /* Pine Script */
    {".pine", LSM_LANG_PINE},

    /* Pkl */
    {".pkl", LSM_LANG_PKL},

    /* PO */
    {".po", LSM_LANG_PO},

    /* Pony */
    {".pony", LSM_LANG_PONY},

    /* PO */
    {".pot", LSM_LANG_PO},

    /* Puppet */
    {".pp", LSM_LANG_PUPPET},

    /* Prisma */
    {".prisma", LSM_LANG_PRISMA},

    /* Properties */
    {".properties", LSM_LANG_PROPERTIES},

    /* PowerShell */
    {".ps1", LSM_LANG_POWERSHELL},

    /* PowerShell */
    {".psd1", LSM_LANG_POWERSHELL},

    /* PowerShell */
    {".psm1", LSM_LANG_POWERSHELL},

    /* PureScript */
    {".purs", LSM_LANG_PURESCRIPT},

    /* ReScript */
    {".res", LSM_LANG_RESCRIPT},

    /* ReScript */
    {".resi", LSM_LANG_RESCRIPT},

    /* Regex */
    {".re", LSM_LANG_REGEX},

    /* Racket */
    {".rkt", LSM_LANG_RACKET},

    /* RON */
    {".ron", LSM_LANG_RON},

    /* reStructuredText */
    {".rst", LSM_LANG_RST},

    /* Assembly */
    {".s", LSM_LANG_ASSEMBLY},

    /* Assembly */
    {".S", LSM_LANG_ASSEMBLY},

    /* Scheme */
    {".scm", LSM_LANG_SCHEME},

    /* Slang */
    {".slang", LSM_LANG_SLANG},

    /* Smali */
    {".smali", LSM_LANG_SMALI},

    /* Smithy */
    {".smithy", LSM_LANG_SMITHY},

    /* Solidity */
    {".sol", LSM_LANG_SOLIDITY},

    /* SOQL */
    {".soql", LSM_LANG_SOQL},

    /* SOSL */
    {".sosl", LSM_LANG_SOSL},

    /* Scheme */
    {".ss", LSM_LANG_SCHEME},

    /* Starlark */
    {".star", LSM_LANG_STARLARK},

    /* SystemVerilog */

    /* SystemVerilog */

    /* Sway */
    {".sw", LSM_LANG_SWAY},

    /* Tcl */
    {".tcl", LSM_LANG_TCL},

    /* TableGen */
    {".td", LSM_LANG_TABLEGEN},

    /* Templ */
    {".templ", LSM_LANG_TEMPL},

    /* Thrift */
    {".thrift", LSM_LANG_THRIFT},

    /* Teal */
    {".tl", LSM_LANG_TEAL},

    /* TLA+ */
    {".tla", LSM_LANG_TLAPLUS},

    /* Go Template */
    {".tmpl", LSM_LANG_GOTEMPLATE},

    /* Apex */
    {".trigger", LSM_LANG_APEX},

    /* Typst */
    {".typ", LSM_LANG_TYPST},

    /* VHDL */
    {".vhd", LSM_LANG_VHDL},

    /* VHDL */
    {".vhdl", LSM_LANG_VHDL},

    /* WGSL */
    {".wgsl", LSM_LANG_WGSL},

    /* WIT */
    {".wit", LSM_LANG_WIT},

    /* Zsh */
    {".zsh", LSM_LANG_ZSH},

    /* Zig */
    {".zig", LSM_LANG_ZIG},
};

#define EXT_TABLE_SIZE (sizeof(EXT_TABLE) / sizeof(EXT_TABLE[0]))

/* ── Special filename → Language lookup ──────────────────────────── */

typedef struct {
    const char *filename;
    LSMLanguage language;
} filename_entry_t;

static const filename_entry_t FILENAME_TABLE[] = {
    {"CMakeLists.txt", LSM_LANG_CMAKE},
    {"Dockerfile", LSM_LANG_DOCKERFILE},
    {"GNUmakefile", LSM_LANG_MAKEFILE},
    {"Makefile", LSM_LANG_MAKEFILE},
    {"makefile", LSM_LANG_MAKEFILE},
    {"meson.build", LSM_LANG_MESON},
    {"meson.options", LSM_LANG_MESON},
    {"meson_options.txt", LSM_LANG_MESON},
    {"kustomization.yaml", LSM_LANG_KUSTOMIZE},
    {"kustomization.yml", LSM_LANG_KUSTOMIZE},
    /* Note: FILENAME_TABLE uses case-sensitive strcmp, so mixed-case variants
     * (e.g. "Kustomization.yaml") are not matched here.  They fall through to
     * LSM_LANG_YAML and are re-classified by lsm_is_kustomize_file() in
     * pass_k8s.c, which performs a case-insensitive comparison.  This is the
     * intended behaviour — no additional entries are needed. */
    {".vimrc", LSM_LANG_VIMSCRIPT},
    {".zshrc", LSM_LANG_ZSH},
    {".zshenv", LSM_LANG_ZSH},
    {".zprofile", LSM_LANG_ZSH},
    {"justfile", LSM_LANG_JUST},
    {"Justfile", LSM_LANG_JUST},
    {".justfile", LSM_LANG_JUST},
    {"hyprland.conf", LSM_LANG_HYPRLANG},
    {"ssh_config", LSM_LANG_SSHCONFIG},
    {"sshd_config", LSM_LANG_SSHCONFIG},
    {".ssh/config", LSM_LANG_SSHCONFIG},
    {"BUILD", LSM_LANG_STARLARK},
    {"BUILD.bazel", LSM_LANG_STARLARK},
    {"WORKSPACE", LSM_LANG_STARLARK},
    {"WORKSPACE.bazel", LSM_LANG_STARLARK},
    {"requirements.txt", LSM_LANG_REQUIREMENTS},
    {"requirements-dev.txt", LSM_LANG_REQUIREMENTS},
    {"requirements-test.txt", LSM_LANG_REQUIREMENTS},
    {"Kconfig", LSM_LANG_KCONFIG},
    {"go.mod", LSM_LANG_GOMOD},
    {".env", LSM_LANG_DOTENV},
    {".env.local", LSM_LANG_DOTENV},
    {".gitattributes", LSM_LANG_GITATTRIBUTES},

};

#define FILENAME_TABLE_SIZE (sizeof(FILENAME_TABLE) / sizeof(FILENAME_TABLE[0]))

/* ── Language names ──────────────────────────────────────────────── */

static const char *LANG_NAMES[LSM_LANG_COUNT] = {
    [LSM_LANG_GO] = "Go",
    [LSM_LANG_PYTHON] = "Python",
    [LSM_LANG_JAVASCRIPT] = "JavaScript",
    [LSM_LANG_TYPESCRIPT] = "TypeScript",
    [LSM_LANG_TSX] = "TSX",
    [LSM_LANG_RUST] = "Rust",
    [LSM_LANG_JAVA] = "Java",
    [LSM_LANG_CPP] = "C++",
    [LSM_LANG_CSHARP] = "C#",
    [LSM_LANG_PHP] = "PHP",
    [LSM_LANG_LUA] = "Lua",
    [LSM_LANG_SCALA] = "Scala",
    [LSM_LANG_KOTLIN] = "Kotlin",
    [LSM_LANG_RUBY] = "Ruby",
    [LSM_LANG_C] = "C",
    [LSM_LANG_BASH] = "Bash",
    [LSM_LANG_ZIG] = "Zig",
    [LSM_LANG_ELIXIR] = "Elixir",
    [LSM_LANG_HASKELL] = "Haskell",
    [LSM_LANG_OCAML] = "OCaml",
    [LSM_LANG_OBJC] = "Objective-C",
    [LSM_LANG_SWIFT] = "Swift",
    [LSM_LANG_DART] = "Dart",
    [LSM_LANG_PERL] = "Perl",
    [LSM_LANG_GROOVY] = "Groovy",
    [LSM_LANG_ERLANG] = "Erlang",
    [LSM_LANG_R] = "R",
    [LSM_LANG_HTML] = "HTML",
    [LSM_LANG_CSS] = "CSS",
    [LSM_LANG_SCSS] = "SCSS",
    [LSM_LANG_YAML] = "YAML",
    [LSM_LANG_TOML] = "TOML",
    [LSM_LANG_HCL] = "HCL",
    [LSM_LANG_SQL] = "SQL",
    [LSM_LANG_DOCKERFILE] = "Dockerfile",
    [LSM_LANG_CLOJURE] = "Clojure",
    [LSM_LANG_FSHARP] = "F#",
    [LSM_LANG_JULIA] = "Julia",
    [LSM_LANG_VIMSCRIPT] = "VimScript",
    [LSM_LANG_NIX] = "Nix",
    [LSM_LANG_COMMONLISP] = "Common Lisp",
    [LSM_LANG_ELM] = "Elm",
    [LSM_LANG_FORTRAN] = "Fortran",
    [LSM_LANG_CUDA] = "CUDA",
    [LSM_LANG_COBOL] = "COBOL",
    [LSM_LANG_VERILOG] = "Verilog",
    [LSM_LANG_EMACSLISP] = "Emacs Lisp",
    [LSM_LANG_JSON] = "JSON",
    [LSM_LANG_XML] = "XML",
    [LSM_LANG_MARKDOWN] = "Markdown",
    [LSM_LANG_MAKEFILE] = "Makefile",
    [LSM_LANG_CMAKE] = "CMake",
    [LSM_LANG_PROTOBUF] = "Protobuf",
    [LSM_LANG_GRAPHQL] = "GraphQL",
    [LSM_LANG_VUE] = "Vue",
    [LSM_LANG_SVELTE] = "Svelte",
    [LSM_LANG_MESON] = "Meson",
    [LSM_LANG_GLSL] = "GLSL",
    [LSM_LANG_INI] = "INI",
    [LSM_LANG_MATLAB] = "MATLAB",
    [LSM_LANG_LEAN] = "Lean",
    [LSM_LANG_FORM] = "FORM",
    [LSM_LANG_MAGMA] = "Magma",
    [LSM_LANG_WOLFRAM] = "Wolfram",
    [LSM_LANG_KUSTOMIZE] = "Kustomize",
    [LSM_LANG_K8S] = "Kubernetes",
    [LSM_LANG_PINE] = "PineScript",
    [LSM_LANG_SOLIDITY] = "Solidity",
    [LSM_LANG_TYPST] = "Typst",
    [LSM_LANG_GDSCRIPT] = "GDScript",
    [LSM_LANG_GLEAM] = "Gleam",
    [LSM_LANG_POWERSHELL] = "PowerShell",
    [LSM_LANG_PASCAL] = "Pascal",
    [LSM_LANG_DLANG] = "D",
    [LSM_LANG_NIM] = "Nim",
    [LSM_LANG_SCHEME] = "Scheme",
    [LSM_LANG_FENNEL] = "Fennel",
    [LSM_LANG_FISH] = "Fish",
    [LSM_LANG_AWK] = "AWK",
    [LSM_LANG_ZSH] = "Zsh",
    [LSM_LANG_TCL] = "Tcl",
    [LSM_LANG_ADA] = "Ada",
    [LSM_LANG_AGDA] = "Agda",
    [LSM_LANG_RACKET] = "Racket",
    [LSM_LANG_ODIN] = "Odin",
    [LSM_LANG_RESCRIPT] = "ReScript",
    [LSM_LANG_PURESCRIPT] = "PureScript",
    [LSM_LANG_NICKEL] = "Nickel",
    [LSM_LANG_CRYSTAL] = "Crystal",
    [LSM_LANG_TEAL] = "Teal",
    [LSM_LANG_HARE] = "Hare",
    [LSM_LANG_PONY] = "Pony",
    [LSM_LANG_LUAU] = "Luau",
    [LSM_LANG_QML] = "QML",
    [LSM_LANG_CFSCRIPT] = "CFML",
    [LSM_LANG_CFML] = "CFML",
    [LSM_LANG_JANET] = "Janet",
    [LSM_LANG_SWAY] = "Sway",
    [LSM_LANG_NASM] = "NASM",
    [LSM_LANG_ASSEMBLY] = "Assembly",
    [LSM_LANG_ASTRO] = "Astro",
    [LSM_LANG_BLADE] = "Blade",
    [LSM_LANG_JUST] = "Just",
    [LSM_LANG_GOTEMPLATE] = "Go Template",
    [LSM_LANG_TEMPL] = "Templ",
    [LSM_LANG_LIQUID] = "Liquid",
    [LSM_LANG_JINJA2] = "Jinja2",
    [LSM_LANG_PRISMA] = "Prisma",
    [LSM_LANG_HYPRLANG] = "Hyprlang",
    [LSM_LANG_DOTENV] = "DotEnv",
    [LSM_LANG_SYSTEMVERILOG] = "SystemVerilog",
    [LSM_LANG_DIFF] = "Diff",
    [LSM_LANG_WGSL] = "WGSL",
    [LSM_LANG_KDL] = "KDL",
    [LSM_LANG_JSON5] = "JSON5",
    [LSM_LANG_JSONNET] = "Jsonnet",
    [LSM_LANG_RON] = "RON",
    [LSM_LANG_THRIFT] = "Thrift",
    [LSM_LANG_CAPNP] = "Cap'n Proto",
    [LSM_LANG_PROPERTIES] = "Properties",
    [LSM_LANG_SSHCONFIG] = "SSH Config",
    [LSM_LANG_BIBTEX] = "BibTeX",
    [LSM_LANG_STARLARK] = "Starlark",
    [LSM_LANG_BICEP] = "Bicep",
    [LSM_LANG_CSV] = "CSV",
    [LSM_LANG_REQUIREMENTS] = "Requirements",
    [LSM_LANG_HLSL] = "HLSL",
    [LSM_LANG_VHDL] = "VHDL",
    [LSM_LANG_DEVICETREE] = "DeviceTree",
    [LSM_LANG_LINKERSCRIPT] = "Linker Script",
    [LSM_LANG_GN] = "GN",
    [LSM_LANG_KCONFIG] = "Kconfig",
    [LSM_LANG_BITBAKE] = "BitBake",
    [LSM_LANG_SMALI] = "Smali",
    [LSM_LANG_TABLEGEN] = "TableGen",
    [LSM_LANG_ISPC] = "ISPC",
    [LSM_LANG_CAIRO] = "Cairo",
    [LSM_LANG_MOVE] = "Move",
    [LSM_LANG_SQUIRREL] = "Squirrel",
    [LSM_LANG_FUNC] = "FunC",
    [LSM_LANG_REGEX] = "Regex",
    [LSM_LANG_JSDOC] = "JSDoc",
    [LSM_LANG_RST] = "reStructuredText",
    [LSM_LANG_BEANCOUNT] = "Beancount",
    [LSM_LANG_MERMAID] = "Mermaid",
    [LSM_LANG_PUPPET] = "Puppet",
    [LSM_LANG_PO] = "PO",
    [LSM_LANG_GITATTRIBUTES] = "gitattributes",
    [LSM_LANG_GITIGNORE] = "gitignore",
    [LSM_LANG_SLANG] = "Slang",
    [LSM_LANG_LLVM_IR] = "LLVM IR",
    [LSM_LANG_SMITHY] = "Smithy",
    [LSM_LANG_WIT] = "WIT",
    [LSM_LANG_TLAPLUS] = "TLA+",
    [LSM_LANG_PKL] = "Pkl",
    [LSM_LANG_GOMOD] = "Go Mod",
    [LSM_LANG_APEX] = "Apex",
    [LSM_LANG_SOQL] = "SOQL",
    [LSM_LANG_SOSL] = "SOSL",
    [LSM_LANG_MOJO] = "Mojo",
    [LSM_LANG_OBJECTSCRIPT_UDL] = "ObjectScript UDL",
    [LSM_LANG_OBJECTSCRIPT_ROUTINE] = "ObjectScript Routine",
    [LSM_LANG_OBJECTSCRIPT_EXPORT] = "ObjectScript Export XML",

};

/* ── Public API ──────────────────────────────────────────────────── */

LSMLanguage lsm_language_for_extension(const char *ext) {
    if (!ext || !ext[0]) {
        return LSM_LANG_COUNT;
    }

    /* Check user-defined overrides first */
    const lsm_userconfig_t *ucfg = lsm_get_user_lang_config();
    if (ucfg) {
        LSMLanguage ulang = lsm_userconfig_lookup(ucfg, ext);
        if (ulang != LSM_LANG_COUNT) {
            return ulang;
        }
    }

    for (size_t i = 0; i < EXT_TABLE_SIZE; i++) {
        if (strcmp(EXT_TABLE[i].ext, ext) == 0) {
            return EXT_TABLE[i].language;
        }
    }
    return LSM_LANG_COUNT;
}

LSMLanguage lsm_language_for_filename(const char *filename) {
    if (!filename || !filename[0]) {
        return LSM_LANG_COUNT;
    }

    /* Check special filenames first */
    for (size_t i = 0; i < FILENAME_TABLE_SIZE; i++) {
        if (strcmp(FILENAME_TABLE[i].filename, filename) == 0) {
            return FILENAME_TABLE[i].language;
        }
    }

    /* DotEnv variant filenames (".env.local", ".env.production", …): the
     * filename starts with ".env." but its last "extension" (e.g. ".local")
     * is not a real language extension.  Match the dotenv convention used by
     * pass_envscan/pass_infrascan (".env" exact, ".env." prefix, "*.env"
     * suffix) so file-index routing agrees with direct extraction. */
    if (strncmp(filename, ".env.", SLEN(".env.")) == 0) {
        return LSM_LANG_DOTENV;
    }

    /* Fall back to extension-based lookup.
     * For compound extensions (e.g. ".blade.php") defined in the user config,
     * scan from the first dot in the basename toward the last, checking user
     * config at each position.  Built-in extensions use the last dot only. */
    const char *last_dot = strrchr(filename, '.');
    if (!last_dot) {
        return LSM_LANG_COUNT;
    }

    /* Probe compound extensions (e.g. ".blade.php") from the first dot toward
     * the last. Built-in compounds are checked first so e.g. Laravel Blade
     * templates map to Blade rather than the single-extension fallback (PHP);
     * user config can still add more (#258). */
    static const struct {
        const char *ext;
        LSMLanguage lang;
    } COMPOUND_EXT_TABLE[] = {
        {".blade.php", LSM_LANG_BLADE},
    };
    const lsm_userconfig_t *ucfg = lsm_get_user_lang_config();
    const char *p = strchr(filename, '.');
    while (p && p < last_dot) {
        for (size_t i = 0; i < sizeof(COMPOUND_EXT_TABLE) / sizeof(COMPOUND_EXT_TABLE[0]); i++) {
            if (strcmp(p, COMPOUND_EXT_TABLE[i].ext) == 0) {
                return COMPOUND_EXT_TABLE[i].lang;
            }
        }
        if (ucfg) {
            LSMLanguage lang = lsm_userconfig_lookup(ucfg, p);
            if (lang != LSM_LANG_COUNT) {
                return lang;
            }
        }
        p = strchr(p + SKIP_ONE, '.');
    }

    /* Standard single-extension lookup (built-ins + user overrides). */
    return lsm_language_for_extension(last_dot);
}

const char *lsm_language_name(LSMLanguage lang) {
    if (lang < 0 || lang >= LSM_LANG_COUNT) {
        return "Unknown";
    }
    return LANG_NAMES[lang] ? LANG_NAMES[lang] : "Unknown";
}

/* ── Shebang interpreter detection (extensionless scripts) ────────── */

/* Basename of an interpreter path: the segment after the last '/'.  Shebangs
 * are a POSIX convention, so only '/' is treated as a separator. */
static const char *interp_basename(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + SKIP_ONE : path;
}

/* "python" optionally followed by an explicit numeric version (digits and dots
 * only, e.g. "python3", "python3.12").  Bounded and explicit so arbitrary
 * suffixes like "python-wrapper" are rejected. */
static bool is_python_interp(const char *base) {
    if (strncmp(base, "python", SLEN("python")) != 0) {
        return false;
    }
    const char *version = base + SLEN("python");
    if (*version == '\0') {
        return true;
    }

    /* Each numeric component must contain at least one digit. */
    bool need_digit = true;
    for (const char *v = version; *v; v++) {
        if (isdigit((unsigned char)*v)) {
            need_digit = false;
        } else if (*v == '.' && !need_digit) {
            need_digit = true;
        } else {
            return false;
        }
    }
    return !need_digit;
}

/* Map an interpreter basename to a language, or LSM_LANG_COUNT if unrecognized.
 * Non-python interpreters are matched exactly (no prefix/suffix logic). */
static LSMLanguage lang_for_interpreter(const char *base) {
    if (is_python_interp(base)) {
        return LSM_LANG_PYTHON;
    }
    static const struct {
        const char *name;
        LSMLanguage lang;
    } INTERP_TABLE[] = {
        {"sh", LSM_LANG_BASH},           {"bash", LSM_LANG_BASH}, {"dash", LSM_LANG_BASH},
        {"ksh", LSM_LANG_BASH},          {"zsh", LSM_LANG_BASH},  {"node", LSM_LANG_JAVASCRIPT},
        {"nodejs", LSM_LANG_JAVASCRIPT}, {"ruby", LSM_LANG_RUBY}, {"perl", LSM_LANG_PERL},
        {"php", LSM_LANG_PHP},           {"lua", LSM_LANG_LUA},
    };
    for (size_t i = 0; i < sizeof(INTERP_TABLE) / sizeof(INTERP_TABLE[0]); i++) {
        if (strcmp(base, INTERP_TABLE[i].name) == 0) {
            return INTERP_TABLE[i].lang;
        }
    }
    return LSM_LANG_COUNT;
}

/* Advance *cursor past leading blanks and return the next whitespace-delimited
 * token (NUL-terminated in place), or NULL when the line is exhausted. */
static char *shebang_next_token(char **cursor) {
    char *p = *cursor;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p == '\0') {
        *cursor = p;
        return NULL;
    }
    char *start = p;
    while (*p && *p != ' ' && *p != '\t') {
        p++;
    }
    if (*p) {
        *p = '\0';
        p++;
    }
    *cursor = p;
    return start;
}

LSMLanguage lsm_language_from_shebang(const char *path) {
    if (!path) {
        return LSM_LANG_COUNT;
    }

    FILE *f = lsm_fopen(path, "rb");
    if (!f) {
        return LSM_LANG_COUNT; /* fail closed on read error */
    }

    /* Read only a bounded first line. */
    char buf[LSM_SZ_256];
    size_t n = fread(buf, SKIP_ONE, sizeof(buf) - SKIP_ONE, f);

    /* Fail closed on any read error rather than parsing a partial buffer. */
    if (ferror(f)) {
        (void)fclose(f);
        return LSM_LANG_COUNT;
    }

    /* If the bounded buffer filled without containing a newline, the first
     * line may extend past our bound. Probe a single extra byte to tell an
     * exact EOF (the whole file is <= 255 bytes) from a truncated longer
     * line: any surviving byte -- including a newline just beyond the bound --
     * means the first line was cut off, so fail closed. A probe read error
     * fails closed too. This keeps the read bounded (no unbounded line read
     * or allocation). */
    bool have_newline = (memchr(buf, '\n', n) != NULL);
    if (!have_newline && n == sizeof(buf) - SKIP_ONE) {
        int probe = fgetc(f);
        if (probe != EOF || ferror(f)) {
            (void)fclose(f);
            return LSM_LANG_COUNT;
        }
    }
    (void)fclose(f);

    /* Must begin with "#!". */
    if (n < PAIR_LEN || buf[0] != '#' || buf[1] != '!') {
        return LSM_LANG_COUNT;
    }

    /* Isolate the first line; reject an embedded NUL before the newline. */
    size_t line_len = 0;
    while (line_len < n && buf[line_len] != '\n') {
        if (buf[line_len] == '\0') {
            return LSM_LANG_COUNT; /* embedded NUL — treat as binary */
        }
        line_len++;
    }
    /* Trim a trailing CR so CRLF first lines parse. */
    if (line_len > 0 && buf[line_len - SKIP_ONE] == '\r') {
        line_len--;
    }
    buf[line_len] = '\0';

    /* First token after "#!" is the interpreter (or env). */
    char *cursor = buf + PAIR_LEN;
    char *interp = shebang_next_token(&cursor);
    if (!interp) {
        return LSM_LANG_COUNT;
    }
    const char *base = interp_basename(interp);

    /* "env [-S] <interp> [args...]": the real interpreter is the next token.
     * Only the plain "env <interp>" and "env -S/--split-string <interp> [args]"
     * shapes are supported. After the optional -S, the interpreter token must
     * be a real command, so reject option tokens (leading '-') and NAME=value
     * assignments (containing '=') -- e.g. "env PYTHON=/usr/bin/python
     * python-wrapper", where env would treat the first token as an env-var
     * setting rather than the program to run. */
    if (strcmp(base, "env") == 0) {
        char *tok = shebang_next_token(&cursor);
        if (tok && (strcmp(tok, "-S") == 0 || strcmp(tok, "--split-string") == 0)) {
            tok = shebang_next_token(&cursor);
        }
        if (!tok || tok[0] == '-' || strchr(tok, '=') != NULL) {
            return LSM_LANG_COUNT;
        }
        base = interp_basename(tok);
    }

    return lang_for_interpreter(base);
}

/* ── .m file disambiguation ──────────────────────────────────────── */

/* Simple substring search helper */
static bool str_contains(const char *haystack, const char *needle) {
    return strstr(haystack, needle) != NULL;
}

static bool has_objc_markers(const char *buf) {
    return str_contains(buf, "@interface") || str_contains(buf, "@implementation") ||
           str_contains(buf, "@protocol") || str_contains(buf, "@property") ||
           str_contains(buf, "#import") || str_contains(buf, "@selector") ||
           str_contains(buf, "@encode") || str_contains(buf, "@synthesize") ||
           str_contains(buf, "@dynamic");
}

static bool has_magma_end_markers(const char *buf) {
    return str_contains(buf, "end function;") || str_contains(buf, "end procedure;") ||
           str_contains(buf, "end intrinsic;") || str_contains(buf, "end if;") ||
           str_contains(buf, "end for;") || str_contains(buf, "end while;");
}

/* Check for "intrinsic Name(" or "procedure Name(" patterns. */
static bool has_magma_callable_pattern(const char *buf) {
    const char *markers[] = {"intrinsic ", "procedure "};
    for (int i = 0; i < LANG_SCAN_PASSES; i++) {
        const char *p = strstr(buf, markers[i]);
        if (!p) {
            continue;
        }
        p += strlen(markers[i]);
        while (*p && isalpha((unsigned char)*p)) {
            p++;
        }
        if (*p == '(') {
            return true;
        }
    }
    return false;
}

/* Scan lines for MATLAB-specific markers (function/classdef/%%). */
static bool has_matlab_line_markers(const char *buf) {
    const char *line = buf;
    while (*line) {
        const char *p = line;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (strncmp(p, "function ", SLEN("function ")) == 0 ||
            strncmp(p, "function\t", SLEN("function\t")) == 0 ||
            strncmp(p, "classdef ", SLEN("classdef ")) == 0 ||
            strncmp(p, "classdef\t", SLEN("classdef\t")) == 0 || strncmp(p, "%%", PAIR_LEN) == 0 ||
            (*p == '%' && *(p + SKIP_ONE) != '{')) {
            return true;
        }
        const char *nl = strchr(line, '\n');
        if (!nl) {
            break;
        }
        line = nl + SKIP_ONE;
    }
    return false;
}

LSMLanguage lsm_disambiguate_m(const char *path) {
    if (!path) {
        return LSM_LANG_MATLAB;
    }

    FILE *f = lsm_fopen(path, "r");
    if (!f) {
        return LSM_LANG_MATLAB;
    }

    /* Read first 4KB */
    char buf[LSM_SZ_4K + SKIP_ONE];
    size_t n = fread(buf, SKIP_ONE, LSM_SZ_4K, f);
    buf[n] = '\0';
    (void)fclose(f);

    if (has_objc_markers(buf)) {
        return LSM_LANG_OBJC;
    }
    if (has_magma_end_markers(buf)) {
        return LSM_LANG_MAGMA;
    }
    if ((str_contains(buf, "intrinsic ") || str_contains(buf, "procedure ")) &&
        has_magma_callable_pattern(buf)) {
        return LSM_LANG_MAGMA;
    }
    if (has_matlab_line_markers(buf)) {
        return LSM_LANG_MATLAB;
    }

    return LSM_LANG_MATLAB;
}

/* Disambiguate .cls files: shared by InterSystems ObjectScript UDL and
 * Salesforce Apex. ObjectScript class files begin with a line of the form
 * "Class <UppercasePackage>...". Defaults to Apex on any doubt. */
LSMLanguage lsm_disambiguate_cls(const char *path) {
    if (!path) {
        return LSM_LANG_APEX;
    }

    FILE *f = lsm_fopen(path, "r");
    if (!f) {
        return LSM_LANG_APEX;
    }

    char buf[LSM_SZ_4K + SKIP_ONE];
    size_t n = fread(buf, SKIP_ONE, LSM_SZ_4K, f);
    buf[n] = '\0';
    (void)fclose(f);

    const char *line = buf;
    while (*line) {
        if (strncmp(line, "Class ", SLEN("Class ")) == 0 &&
            isupper((unsigned char)line[SLEN("Class ")])) {
            return LSM_LANG_OBJECTSCRIPT_UDL;
        }
        const char *nl = strchr(line, '\n');
        if (!nl) {
            break;
        }
        line = nl + SKIP_ONE;
    }
    return LSM_LANG_APEX;
}

/* Disambiguate .inc files: shared by BitBake include fragments and
 * InterSystems ObjectScript include (macro) files. ObjectScript .inc files are
 * predominantly macro definitions ("#define NAME ..." / "#def1arg NAME ...");
 * some also carry a "ROUTINE <Name>" header. The macro-preprocessor directives
 * are the strongest signal because that is the primary content of an .inc file,
 * whereas BitBake uses '#' only for "# comment" lines (always '#' + space).
 * We therefore match ObjectScript preprocessor directives ('#' immediately
 * followed by 'def'/';'), which BitBake never produces. Defaults to BitBake on
 * any doubt (preserves existing behaviour). */
LSMLanguage lsm_disambiguate_inc(const char *path) {
    if (!path) {
        return LSM_LANG_BITBAKE;
    }

    FILE *f = lsm_fopen(path, "r");
    if (!f) {
        return LSM_LANG_BITBAKE;
    }

    char buf[LSM_SZ_4K + SKIP_ONE];
    size_t n = fread(buf, SKIP_ONE, LSM_SZ_4K, f);
    buf[n] = '\0';
    (void)fclose(f);

    const char *line = buf;
    while (*line) {
        /* ObjectScript include header: a line beginning "ROUTINE <Uppercase>". */
        if (strncmp(line, "ROUTINE ", SLEN("ROUTINE ")) == 0 &&
            isupper((unsigned char)line[SLEN("ROUTINE ")])) {
            return LSM_LANG_OBJECTSCRIPT_ROUTINE;
        }
        /* ObjectScript macro directives — the primary content of .inc files.
         * "#define"/"#def1arg" (macro defs) and "#;" (line comment). BitBake's
         * only '#' use is "# comment" (hash + space), so these never collide. */
        if (strncmp(line, "#define", SLEN("#define")) == 0 ||
            strncmp(line, "#def1arg", SLEN("#def1arg")) == 0 ||
            strncmp(line, "#;", SLEN("#;")) == 0) {
            return LSM_LANG_OBJECTSCRIPT_ROUTINE;
        }
        const char *nl = strchr(line, '\n');
        if (!nl) {
            break;
        }
        line = nl + SKIP_ONE;
    }
    return LSM_LANG_BITBAKE;
}
