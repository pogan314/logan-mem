/* Node label → color mapping for sidebar/tooltips (structural meaning) */

const LABEL_COLORS: Record<string, string> = {
  Project: "#e11d48",
  Package: "#f97316",
  Module: "#f97316",
  Folder: "#22c55e",
  File: "#3b82f6",
  Class: "#a855f7",
  Interface: "#a855f7",
  Function: "#06b6d4",
  Method: "#06b6d4",
  Route: "#eab308",
  Variable: "#64748b",
};

const DEFAULT_COLOR = "#94a3b8";

export function colorForLabel(label: string): string {
  return LABEL_COLORS[label] ?? DEFAULT_COLOR;
}

/* Dead-code status → color (matches layout3d.c status strings).
 *   dead     zero callers + zero usages, not entry/test/exported
 *   single   exactly one caller
 *   entry    entry points / routes
 *   test     test code
 *   normal   healthy (>=2 callers)
 *   exported/structural → dimmed grey (not dead-code candidates) */
const STATUS_COLORS: Record<string, string> = {
  dead: "#ef4444",
  single: "#f97316",
  entry: "#3b82f6",
  test: "#a855f7",
  normal: "#22c55e",
  exported: "#475569",
  structural: "#334155",
};

const STATUS_DEFAULT = "#334155";

export function colorForStatus(status?: string): string {
  return status ? (STATUS_COLORS[status] ?? STATUS_DEFAULT) : STATUS_DEFAULT;
}

export const STATUS_LEGEND: { status: string; label: string; color: string }[] = [
  { status: "dead", label: "Dead (0 callers)", color: STATUS_COLORS.dead },
  { status: "single", label: "One caller", color: STATUS_COLORS.single },
  { status: "entry", label: "Entry / route", color: STATUS_COLORS.entry },
  { status: "test", label: "Test", color: STATUS_COLORS.test },
  { status: "normal", label: "Normal", color: STATUS_COLORS.normal },
];

/* Stellar spectral type legend (for the graph view) */
export const STELLAR_LEGEND = [
  { type: "O (Blue Giant)", color: "#80a0ff", description: "50+ connections" },
  { type: "B (Blue-White)", color: "#c0d0ff", description: "26-50 connections" },
  { type: "A (White)", color: "#e8e8ff", description: "13-25 connections" },
  { type: "F (Yellow-White)", color: "#fff0c0", description: "7-12 connections" },
  { type: "G (Yellow/Sun)", color: "#ffe080", description: "4-6 connections" },
  { type: "K (Orange)", color: "#ffa060", description: "2-3 connections" },
  { type: "M (Red Dwarf)", color: "#ff6050", description: "0-1 connections" },
];

/* Node label → what the indexer means by it. Each line is verified against the code that creates the node, named beside it. A label with no entry here gets no tooltip rather than a guessed one. */
const LABEL_DESCRIPTIONS: Record<string, string> = {
  Project: "The indexed repository itself — one per project (pipeline.c).",
  Branch: "The git branch the index was built from (pipeline.c).",
  Folder: "A directory in the repository (pipeline.c).",
  File: "A discovered file, indexed or not (pipeline.c).",
  Module: "A parsed source file's top-level scope — one per file — or a TypeScript namespace (extract_defs.c).",
  Package: "An external dependency declared in a manifest or Helm chart. Not code in this repo (pass_k8s.c).",
  Section: "A Markdown heading, spanning to the next heading of equal or higher level (extract_defs.c).",
  Class: "A class, or a Go/Rust/Swift named type (extract_defs.c).",
  Struct: "A Go/Rust/Swift/D struct, kept distinct from Class (extract_defs.c).",
  Interface: "An interface declaration (extract_defs.c).",
  Protocol: "A Swift protocol — treated as an interface (pass_lsp_cross.c).",
  Enum: "An enum declaration (extract_defs.c).",
  Type: "A type alias or named type declaration (extract_defs.c).",
  Function: "A free function — one not attached to a class (extract_defs.c).",
  Method: "A function that belongs to a class (extract_defs.c).",
  Variable: "A top-level or member variable or constant (extract_defs.c).",
  Route: "An HTTP route path found in a web framework (pass_route_nodes.c).",
  EnvVar: "An environment variable the code reads (pass_definitions.c).",
};

export function describeLabel(label: string): string | undefined {
  return LABEL_DESCRIPTIONS[label];
}
