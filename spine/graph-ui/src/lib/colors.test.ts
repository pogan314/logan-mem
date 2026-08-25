import { describe, it, expect } from "vitest";
import { describeLabel } from "./colors";

describe("describeLabel", () => {
  it("describes the labels this indexer actually emits", () => {
    for (const label of ["Project", "Branch", "Folder", "File", "Module", "Package", "Section", "Class", "Function", "Method", "Variable", "Route", "EnvVar"]) {
      expect(describeLabel(label), label).toBeTruthy();
    }
  });

  it("returns undefined rather than a guess for an unknown label", () => {
    expect(describeLabel("Sprocket")).toBeUndefined();
  });
});
