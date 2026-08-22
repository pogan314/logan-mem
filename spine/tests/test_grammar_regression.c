/*
 * test_grammar_regression.c — Per-language extraction regression net.
 *
 * Guards against silent extraction breakage when vendored tree-sitter grammars
 * are refreshed. Every LSM_LANG_* enum is exercised: a minimal sample is
 * extracted and checked for (a) no crash (non-NULL result) and (b) a
 * catastrophic-break floor (defs >= min_defs) plus, for confident cases,
 * expected definition names. A future grammar upgrade that renames/removes the
 * node types extraction depends on (e.g. the fwcd kotlin `name`-field drop that
 * produced 0 defs) fails this suite loudly.
 *
 * min_defs convention: code languages use >=1 (a sample with a function/type
 * must yield at least one def); data/config/markup languages use 0 (the check
 * is "parses without crashing"). Tighten a row's min_defs to lock in a count.
 */
#include "test_framework.h"
#include "lsm.h"
#include "grammar_cases.h"

static int reg_has_def_any(LSMFileResult *r, const char *name) {
    for (int i = 0; i < r->defs.count; i++) {
        if (strcmp(r->defs.items[i].name, name) == 0)
            return 1;
    }
    return 0;
}

static LSMFileResult *extract(const char *src, LSMLanguage lang, const char *proj,
                              const char *path) {
    return lsm_extract_file(src, (int)strlen(src), lang, proj, path, 0, NULL, NULL);
}

const GrammarCase LSM_GRAMMAR_CASES[] = {
    /* ── LSP-backed / mainstream code languages (expect named defs) ── */
    {"go",
     LSM_LANG_GO,
     "a.go",
     "package m\nfunc Foo() {}\nfunc Bar() int { return 0 }\n",
     2,
     {"Foo", "Bar", NULL}},
    {"c",
     LSM_LANG_C,
     "a.c",
     "int foo(void){return 0;}\nint bar(void){return 1;}\n",
     2,
     {"foo", "bar", NULL}},
    {"cpp", LSM_LANG_CPP, "a.cpp", "struct A {};\nint foo(){return 0;}\n", 2, {"A", "foo", NULL}},
    {"cuda",
     LSM_LANG_CUDA,
     "a.cu",
     "__global__ void foo(){}\nint bar(){return 0;}\n",
     1,
     {"bar", NULL}},
    {"python",
     LSM_LANG_PYTHON,
     "a.py",
     "def foo():\n    pass\nclass A:\n    pass\n",
     2,
     {"foo", "A", NULL}},
    {"javascript",
     LSM_LANG_JAVASCRIPT,
     "a.js",
     "function foo(){}\nclass A{}\n",
     2,
     {"foo", "A", NULL}},
    {"typescript",
     LSM_LANG_TYPESCRIPT,
     "a.ts",
     "function foo(): number { return 1; }\nclass A {}\n",
     2,
     {"foo", "A", NULL}},
    {"tsx", LSM_LANG_TSX, "a.tsx", "function foo(): number { return 1; }\n", 1, {"foo", NULL}},
    {"java", LSM_LANG_JAVA, "A.java", "class A {\n    void foo() {}\n}\n", 2, {"A", "foo", NULL}},
    {"kotlin", LSM_LANG_KOTLIN, "a.kt", "fun foo() {}\nclass A\n", 2, {"foo", "A", NULL}},
    {"rust", LSM_LANG_RUST, "a.rs", "fn foo() {}\nstruct A;\n", 2, {"foo", "A", NULL}},
    {"ruby", LSM_LANG_RUBY, "a.rb", "def foo\nend\nclass A\nend\n", 2, {"foo", "A", NULL}},
    {"php", LSM_LANG_PHP, "a.php", "<?php\nfunction foo() {}\nclass A {}\n", 2, {"foo", "A", NULL}},
    {"c_sharp",
     LSM_LANG_CSHARP,
     "A.cs",
     "class A {\n    void Foo() {}\n}\n",
     2,
     {"A", "Foo", NULL}},
    {"bash",
     LSM_LANG_BASH,
     "a.sh",
     "foo() {\n  echo hi\n}\nbar() {\n  foo\n}\n",
     2,
     {"foo", "bar", NULL}},
    {"zsh", LSM_LANG_ZSH, "a.zsh", "foo() {\n  echo hi\n}\nbar() {\n  foo\n}\n", 1, {"foo", NULL}},
    {"lua",
     LSM_LANG_LUA,
     "a.lua",
     "function foo() end\nfunction bar() end\n",
     2,
     {"foo", "bar", NULL}},
    {"luau", LSM_LANG_LUAU, "a.luau", "function foo() end\nfunction bar() end\n", 1, {"foo", NULL}},
    {"perl", LSM_LANG_PERL, "a.pl", "sub foo {}\nsub bar {}\n", 2, {"foo", "bar", NULL}},
    {"dart", LSM_LANG_DART, "a.dart", "class A {}\nvoid foo() {}\n", 2, {"A", "foo", NULL}},
    {"swift", LSM_LANG_SWIFT, "a.swift", "func foo() {}\nclass A {}\n", 2, {"foo", "A", NULL}},
    {"scala", LSM_LANG_SCALA, "a.scala", "object A {\n  def foo() = 1\n}\n", 1, {"A", NULL}},
    {"gdscript", LSM_LANG_GDSCRIPT, "a.gd", "func foo():\n    pass\n", 1, {"foo", NULL}},
    {"groovy", LSM_LANG_GROOVY, "a.groovy", "class A {\n  def foo() {}\n}\n", 1, {"A", NULL}},
    {"zig", LSM_LANG_ZIG, "a.zig", "fn foo() void {}\nfn bar() void {}\n", 1, {"foo", NULL}},
    {"solidity",
     LSM_LANG_SOLIDITY,
     "a.sol",
     "contract A {\n  function foo() public {}\n}\n",
     1,
     {"A", NULL}},
    {"tcl", LSM_LANG_TCL, "a.tcl", "proc foo {} {}\nproc bar {} {}\n", 1, {"foo", NULL}},
    {"powershell",
     LSM_LANG_POWERSHELL,
     "a.ps1",
     "function Get-Foo {\n}\nfunction Get-Bar {\n}\n",
     1,
     {"Get-Foo", NULL}},
    {"r", LSM_LANG_R, "a.R", "foo <- function() {}\nbar <- function() {}\n", 1, {"foo", NULL}},
    {"julia", LSM_LANG_JULIA, "a.jl", "function foo() end\nstruct A end\n", 1, {"foo", NULL}},
    {"matlab",
     LSM_LANG_MATLAB,
     "a.m",
     "function foo()\nend\nfunction bar()\nend\n",
     1,
     {"foo", NULL}},

    /* ── code languages: catastrophic-break floor only (>=1), names vary by grammar ── */
    {"ada", LSM_LANG_ADA, "a.adb", "procedure Foo is\nbegin\n   null;\nend Foo;\n", 1, {NULL}},
    {"agda", LSM_LANG_AGDA, "a.agda", "module M where\nfoo : Set\nfoo = Set\n", 0, {NULL}},
    {"apex", LSM_LANG_APEX, "A.cls", "public class A {\n  void foo() {}\n}\n", 1, {NULL}},
    {"awk",
     LSM_LANG_AWK,
     "a.awk",
     "function foo() { print 1 }\nfunction bar() { print 2 }\n",
     1,
     {NULL}},
    {"cairo", LSM_LANG_CAIRO, "a.cairo", "fn foo() {}\nfn bar() {}\n", 1, {NULL}},
    {"clojure", LSM_LANG_CLOJURE, "a.clj", "(defn foo [] 1)\n(defn bar [] 2)\n", 1, {NULL}},
    {"commonlisp",
     LSM_LANG_COMMONLISP,
     "a.lisp",
     "(defun foo () 1)\n(defun bar () 2)\n",
     1,
     {NULL}},
    {"emacslisp", LSM_LANG_EMACSLISP, "a.el", "(defun foo () 1)\n(defun bar () 2)\n", 1, {NULL}},
    {"crystal", LSM_LANG_CRYSTAL, "a.cr", "def foo\nend\nclass A\nend\n", 1, {NULL}},
    {"d", LSM_LANG_DLANG, "a.d", "void foo() {}\nvoid bar() {}\n", 1, {NULL}},
    {"elixir", LSM_LANG_ELIXIR, "a.ex", "defmodule A do\n  def foo do\n  end\nend\n", 1, {NULL}},
    {"erlang", LSM_LANG_ERLANG, "a.erl", "-module(a).\nfoo() -> ok.\nbar() -> ok.\n", 1, {NULL}},
    {"fennel", LSM_LANG_FENNEL, "a.fnl", "(fn foo [] 1)\n(fn bar [] 2)\n", 1, {NULL}},
    {"fish", LSM_LANG_FISH, "a.fish", "function foo\n  echo hi\nend\n", 1, {NULL}},
    {"fortran", LSM_LANG_FORTRAN, "a.f90", "subroutine foo()\nend subroutine\n", 1, {NULL}},
    {"fsharp", LSM_LANG_FSHARP, "a.fs", "let foo () = 1\nlet bar () = 2\n", 1, {NULL}},
    {"gleam", LSM_LANG_GLEAM, "a.gleam", "pub fn foo() { 1 }\npub fn bar() { 2 }\n", 1, {NULL}},
    {"glsl", LSM_LANG_GLSL, "a.glsl", "void foo() {}\nvoid main() {}\n", 1, {NULL}},
    {"hare", LSM_LANG_HARE, "a.ha", "fn foo() void = void;\nfn bar() void = void;\n", 1, {NULL}},
    {"haskell", LSM_LANG_HASKELL, "a.hs", "foo :: Int\nfoo = 1\n", 1, {NULL}},
    {"hlsl", LSM_LANG_HLSL, "a.hlsl", "void foo() {}\nvoid bar() {}\n", 1, {NULL}},
    {"ispc", LSM_LANG_ISPC, "a.ispc", "void foo() {}\nvoid bar() {}\n", 1, {NULL}},
    {"objc", LSM_LANG_OBJC, "a.m", "@implementation A\n- (void)foo {}\n@end\n", 0, {NULL}},
    {"ocaml", LSM_LANG_OCAML, "a.ml", "let foo () = 1\nlet bar () = 2\n", 1, {NULL}},
    {"odin", LSM_LANG_ODIN, "a.odin", "foo :: proc() {}\nbar :: proc() {}\n", 1, {NULL}},
    {"pascal", LSM_LANG_PASCAL, "a.pas", "procedure Foo;\nbegin\nend;\n", 1, {NULL}},
    {"pony",
     LSM_LANG_PONY,
     "a.pony",
     "actor Main\n  fun foo(): U32 => 1\n",
     2,
     {"Main", "foo", NULL}},
    {"purescript", LSM_LANG_PURESCRIPT, "a.purs", "foo :: Int\nfoo = 1\n", 0, {NULL}},
    {"racket", LSM_LANG_RACKET, "a.rkt", "(define (foo) 1)\n(define (bar) 2)\n", 1, {NULL}},
    {"rescript", LSM_LANG_RESCRIPT, "a.res", "let foo = () => 1\nlet bar = () => 2\n", 1, {NULL}},
    {"scheme", LSM_LANG_SCHEME, "a.scm", "(define (foo) 1)\n(define (bar) 2)\n", 1, {NULL}},
    {"slang", LSM_LANG_SLANG, "a.slang", "void foo() {}\nvoid bar() {}\n", 1, {NULL}},
    {"squirrel", LSM_LANG_SQUIRREL, "a.nut", "function foo() {}\nfunction bar() {}\n", 1, {NULL}},
    {"starlark",
     LSM_LANG_STARLARK,
     "a.bzl",
     "def foo():\n    pass\ndef bar():\n    pass\n",
     1,
     {NULL}},
    {"sway", LSM_LANG_SWAY, "a.sw", "fn foo() {}\nfn bar() {}\n", 1, {NULL}},
    {"teal",
     LSM_LANG_TEAL,
     "a.tl",
     "local function foo() end\nlocal function bar() end\n",
     1,
     {NULL}},
    {"vimscript", LSM_LANG_VIMSCRIPT, "a.vim", "function! Foo()\nendfunction\n", 1, {NULL}},
    {"elm", LSM_LANG_ELM, "a.elm", "module M exposing (..)\nfoo = 1\n", 0, {NULL}},
    {"func", LSM_LANG_FUNC, "a.fc", "() foo() {\n}\n", 0, {NULL}},
    {"lean", LSM_LANG_LEAN, "a.lean", "def foo : Nat := 1\ndef bar : Nat := 2\n", 0, {NULL}},
    {"move",
     LSM_LANG_MOVE,
     "a.move",
     "module 0x1::m {\n  public fun foo() {}\n}\n",
     1,
     {"foo", NULL}},
    {"mojo",
     LSM_LANG_MOJO,
     "a.mojo",
     "fn foo() -> Int:\n    return 1\n\nstruct A:\n    fn bar(self) -> Int:\n        return foo()\n",
     2,
     {"foo", "A", NULL}},
    {"smali",
     LSM_LANG_SMALI,
     "A.smali",
     ".class public LA;\n.super Ljava/lang/Object;\n.method public foo()V\n.end method\n",
     2,
     {"foo", NULL}},
    {"systemverilog",
     LSM_LANG_SYSTEMVERILOG,
     "a.sv",
     "module m;\n  function int foo(); return 0; endfunction\nendmodule\n",
     0,
     {NULL}},
    {"verilog", LSM_LANG_VERILOG, "a.v", "module m;\nendmodule\n", 0, {NULL}},
    {"vhdl", LSM_LANG_VHDL, "a.vhd", "entity foo is\nend foo;\n", 0, {NULL}},
    {"wgsl", LSM_LANG_WGSL, "a.wgsl", "fn foo() {}\nfn bar() {}\n", 1, {NULL}},
    {"tlaplus", LSM_LANG_TLAPLUS, "a.tla", "---- MODULE M ----\nFoo == 1\n====\n", 0, {NULL}},
    {"llvm", LSM_LANG_LLVM_IR, "a.ll", "define void @foo() {\n  ret void\n}\n", 0, {NULL}},
    {"tablegen", LSM_LANG_TABLEGEN, "a.td", "def Foo {}\n", 0, {NULL}},
    {"puppet", LSM_LANG_PUPPET, "a.pp", "class foo {\n}\n", 0, {NULL}},

    /* ── first-party / self-maintained grammars ── */
    {"assembly", LSM_LANG_ASSEMBLY, "a.s", ".global foo\nfoo:\n  ret\n", 0, {NULL}},
    {"nasm", LSM_LANG_NASM, "a.asm", "global foo\nfoo:\n  ret\n", 0, {NULL}},
    {"cfml", LSM_LANG_CFML, "a.cfm", "<cffunction name=\"foo\"></cffunction>\n", 0, {NULL}},
    {"cfscript", LSM_LANG_CFSCRIPT, "a.cfc", "component {\n  function foo() {}\n}\n", 0, {NULL}},
    {"cobol",
     LSM_LANG_COBOL,
     "a.cob",
     "       IDENTIFICATION DIVISION.\n       PROGRAM-ID. FOO.\n       PROCEDURE DIVISION.\n       "
     "    MAIN-PARA.\n               STOP RUN.\n",
     1,
     {"FOO", NULL}},
    {"janet", LSM_LANG_JANET, "a.janet", "(defn foo [] 1)\n(defn bar [] 2)\n", 0, {NULL}},
    {"magma", LSM_LANG_MAGMA, "a.magma", "function foo()\n  return 1;\nend function;\n", 0, {NULL}},
    {"qml", LSM_LANG_QML, "a.qml", "import QtQuick\nItem {\n  function foo() {}\n}\n", 0, {NULL}},
    {"wolfram", LSM_LANG_WOLFRAM, "a.wl", "foo[x_] := x + 1\n", 0, {NULL}},
    {"pine", LSM_LANG_PINE, "a.pine", "foo() =>\n    1\n", 0, {NULL}},
    {"form", LSM_LANG_FORM, "a.frm", "Symbol x;\nLocal F = x;\n", 0, {NULL}},
    {"protobuf",
     LSM_LANG_PROTOBUF,
     "a.proto",
     "syntax = \"proto3\";\n\nmessage Foo {\n  int32 id = 1;\n}\n",
     1,
     {"Foo", NULL}},
    {"soql", LSM_LANG_SOQL, "a.soql", "SELECT Id FROM Account\n", 0, {NULL}},
    {"sosl", LSM_LANG_SOSL, "a.sosl", "FIND {test} IN ALL FIELDS\n", 0, {NULL}},
    {"dotenv", LSM_LANG_DOTENV, "a.env", "FOO=bar\nBAZ=qux\n", 0, {NULL}},

    /* ── data / config / markup / template languages (no-crash floor) ── */
    {"json", LSM_LANG_JSON, "a.json", "{\"a\": 1, \"b\": [2, 3]}\n", 0, {NULL}},
    {"json5", LSM_LANG_JSON5, "a.json5", "{a: 1, b: 2}\n", 0, {NULL}},
    {"jsonnet", LSM_LANG_JSONNET, "a.jsonnet", "{ a: 1, b: 2 }\n", 0, {NULL}},
    {"jsdoc", LSM_LANG_JSDOC, "a.jsdoc", "/** @param {number} x */\n", 0, {NULL}},
    {"yaml", LSM_LANG_YAML, "a.yaml", "a: 1\nb:\n  - 2\n  - 3\n", 0, {NULL}},
    {"k8s", LSM_LANG_K8S, "a.yaml", "apiVersion: v1\nkind: Pod\n", 0, {NULL}},
    {"kustomize", LSM_LANG_KUSTOMIZE, "kustomization.yaml", "resources:\n  - a.yaml\n", 0, {NULL}},
    {"toml", LSM_LANG_TOML, "a.toml", "[section]\nkey = 1\n", 0, {NULL}},
    {"ini", LSM_LANG_INI, "a.ini", "[section]\nkey=1\n", 0, {NULL}},
    {"csv", LSM_LANG_CSV, "a.csv", "a,b,c\n1,2,3\n", 0, {NULL}},
    {"sql", LSM_LANG_SQL, "a.sql", "CREATE TABLE t (id INT);\nSELECT * FROM t;\n", 0, {NULL}},
    {"xml", LSM_LANG_XML, "a.xml", "<root><child>x</child></root>\n", 0, {NULL}},
    {"html", LSM_LANG_HTML, "a.html", "<html><body><p>hi</p></body></html>\n", 0, {NULL}},
    {"css", LSM_LANG_CSS, "a.css", "a { color: red; }\n.x { width: 1px; }\n", 0, {NULL}},
    {"scss", LSM_LANG_SCSS, "a.scss", "$c: red;\na { color: $c; }\n", 0, {NULL}},
    {"markdown", LSM_LANG_MARKDOWN, "a.md", "# Title\n\nSome text.\n", 0, {NULL}},
    {"rst", LSM_LANG_RST, "a.rst", "Title\n=====\n\ntext\n", 0, {NULL}},
    {"dockerfile", LSM_LANG_DOCKERFILE, "Dockerfile", "FROM alpine\nRUN echo hi\n", 0, {NULL}},
    {"makefile", LSM_LANG_MAKEFILE, "Makefile", "all:\n\techo hi\n", 0, {NULL}},
    {"cmake", LSM_LANG_CMAKE, "CMakeLists.txt", "function(foo)\nendfunction()\n", 0, {NULL}},
    {"meson", LSM_LANG_MESON, "meson.build", "project('x', 'c')\n", 0, {NULL}},
    {"gn", LSM_LANG_GN, "a.gn", "executable(\"foo\") {\n}\n", 0, {NULL}},
    {"just", LSM_LANG_JUST, "justfile", "foo:\n\techo hi\n", 0, {NULL}},
    {"hcl", LSM_LANG_HCL, "a.hcl", "resource \"x\" \"y\" {\n  a = 1\n}\n", 0, {NULL}},
    {"nix", LSM_LANG_NIX, "a.nix", "{ foo = 1; bar = 2; }\n", 0, {NULL}},
    {"gomod", LSM_LANG_GOMOD, "go.mod", "module example.com/x\n\ngo 1.21\n", 0, {NULL}},
    {"gotemplate", LSM_LANG_GOTEMPLATE, "a.tmpl", "{{ if .X }}{{ .Y }}{{ end }}\n", 0, {NULL}},
    {"graphql", LSM_LANG_GRAPHQL, "a.graphql", "type Foo {\n  id: ID\n}\n", 0, {NULL}},
    {"prisma", LSM_LANG_PRISMA, "a.prisma", "model Foo {\n  id Int @id\n}\n", 0, {NULL}},
    {"thrift", LSM_LANG_THRIFT, "a.thrift", "service Foo {\n  void ping()\n}\n", 0, {NULL}},
    {"capnp", LSM_LANG_CAPNP, "a.capnp", "struct Foo {\n  id @0 :Int32;\n}\n", 0, {NULL}},
    {"smithy",
     LSM_LANG_SMITHY,
     "a.smithy",
     "namespace example\n\nstructure Foo {\n  id: Integer\n}\n",
     1,
     {"Foo", NULL}},
    {"wit",
     LSM_LANG_WIT,
     "a.wit",
     "interface i {\n  record r { x: u32 }\n  enum e { a, b }\n  f: func() -> u32;\n}\n",
     3,
     {"r", "e", "f", NULL}},
    {"kdl", LSM_LANG_KDL, "a.kdl", "node \"x\" {\n}\n", 0, {NULL}},
    {"ron", LSM_LANG_RON, "a.ron", "(a: 1, b: 2)\n", 0, {NULL}},
    {"nickel", LSM_LANG_NICKEL, "a.ncl", "{ foo = 1, bar = 2 }\n", 0, {NULL}},
    {"pkl", LSM_LANG_PKL, "a.pkl", "foo = 1\nbar = 2\n", 0, {NULL}},
    {"bicep", LSM_LANG_BICEP, "a.bicep", "param foo string\n", 0, {NULL}},
    {"bitbake", LSM_LANG_BITBAKE, "a.bb", "DESCRIPTION = \"x\"\n", 0, {NULL}},
    {"beancount", LSM_LANG_BEANCOUNT, "a.beancount", "2020-01-01 open Assets:Cash\n", 0, {NULL}},
    {"bibtex", LSM_LANG_BIBTEX, "a.bib", "@article{key,\n  title = {X}\n}\n", 0, {NULL}},
    {"po", LSM_LANG_PO, "a.po", "msgid \"x\"\nmsgstr \"y\"\n", 0, {NULL}},
    {"diff", LSM_LANG_DIFF, "a.diff", "--- a\n+++ b\n@@ -1 +1 @@\n-x\n+y\n", 0, {NULL}},
    {"regex", LSM_LANG_REGEX, "a.re", "(foo|bar)+\n", 0, {NULL}},
    {"requirements",
     LSM_LANG_REQUIREMENTS,
     "requirements.txt",
     "flask==1.0\nrequests>=2\n",
     0,
     {NULL}},
    {"properties", LSM_LANG_PROPERTIES, "a.properties", "foo=1\nbar=2\n", 0, {NULL}},
    {"gitignore", LSM_LANG_GITIGNORE, ".gitignore", "*.o\nbuild/\n", 0, {NULL}},
    {"gitattributes", LSM_LANG_GITATTRIBUTES, ".gitattributes", "*.c text\n", 0, {NULL}},
    {"sshconfig", LSM_LANG_SSHCONFIG, "config", "Host x\n  HostName y\n", 0, {NULL}},
    {"hyprlang", LSM_LANG_HYPRLANG, "a.conf", "general {\n  gaps_in = 5\n}\n", 0, {NULL}},
    {"kconfig", LSM_LANG_KCONFIG, "Kconfig", "config FOO\n  bool \"foo\"\n", 0, {NULL}},
    {"linkerscript", LSM_LANG_LINKERSCRIPT, "a.ld", "SECTIONS {\n  .text : {}\n}\n", 0, {NULL}},
    {"devicetree", LSM_LANG_DEVICETREE, "a.dts", "/dts-v1/;\n/ {\n};\n", 0, {NULL}},
    {"jinja2", LSM_LANG_JINJA2, "a.j2", "{% if x %}{{ y }}{% endif %}\n", 0, {NULL}},
    {"liquid", LSM_LANG_LIQUID, "a.liquid", "{% if x %}{{ y }}{% endif %}\n", 0, {NULL}},
    {"blade", LSM_LANG_BLADE, "a.blade.php", "@if($x)\n{{ $y }}\n@endif\n", 0, {NULL}},
    {"vue", LSM_LANG_VUE, "a.vue", "<template><div></div></template>\n", 0, {NULL}},
    {"svelte", LSM_LANG_SVELTE, "a.svelte", "<script>let x = 1;</script>\n<p>{x}</p>\n", 0, {NULL}},
    {"astro", LSM_LANG_ASTRO, "a.astro", "---\nconst x = 1;\n---\n<div>{x}</div>\n", 0, {NULL}},
    {"templ", LSM_LANG_TEMPL, "a.templ", "templ foo() {\n  <div></div>\n}\n", 0, {NULL}},
    {"typst", LSM_LANG_TYPST, "a.typ", "= Title\n\nsome text\n", 0, {NULL}},
    {"mermaid", LSM_LANG_MERMAID, "a.mmd", "graph TD\n  A --> B\n", 0, {NULL}},
};

const size_t LSM_GRAMMAR_CASES_COUNT = sizeof(LSM_GRAMMAR_CASES) / sizeof(LSM_GRAMMAR_CASES[0]);

TEST(grammar_regression_all) {
    int failures = 0;
    size_t n = LSM_GRAMMAR_CASES_COUNT;
    for (size_t i = 0; i < n; i++) {
        const GrammarCase *c = &LSM_GRAMMAR_CASES[i];
        LSMFileResult *r = extract(c->src, c->lang, "reg", c->path);
        if (!r) {
            fprintf(stderr, "  [REG] %-14s extract returned NULL\n", c->name);
            failures++;
            continue;
        }
        if (r->defs.count < c->min_defs) {
            fprintf(stderr, "  [REG] %-14s defs=%d < min=%d  (extraction regression?)\n", c->name,
                    r->defs.count, c->min_defs);
            failures++;
        }
        for (int e = 0; c->expect[e]; e++) {
            if (!reg_has_def_any(r, c->expect[e])) {
                fprintf(stderr, "  [REG] %-14s missing def '%s' (defs=%d)\n", c->name, c->expect[e],
                        r->defs.count);
                failures++;
            }
        }
        lsm_free_result(r);
    }
    if (failures > 0) {
        fprintf(stderr, "  [REG] %d grammar-regression check(s) failed across %zu languages\n",
                failures, n);
    }
    ASSERT_EQ(failures, 0);
    PASS();
}

void suite_grammar_regression(void) {
    RUN_TEST(grammar_regression_all);
}
