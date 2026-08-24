/* @vitest-environment jsdom */
import "@testing-library/jest-dom/vitest";
import { fireEvent, render, screen } from "@testing-library/react";
import { afterEach, beforeEach, describe, expect, it } from "vitest";
import { CollapsibleSection } from "./CollapsibleSection";

beforeEach(() => localStorage.clear());
afterEach(() => localStorage.clear());

describe("CollapsibleSection", () => {
  it("shows its children when open and hides them when collapsed", () => {
    render(
      <CollapsibleSection id="t1" title="Filters">
        <p>body content</p>
      </CollapsibleSection>,
    );
    expect(screen.getByText("body content")).toBeInTheDocument();
    fireEvent.click(screen.getByRole("button", { name: /Filters/ }));
    expect(screen.queryByText("body content")).not.toBeInTheDocument();
  });

  it("persists the collapsed state across remounts", () => {
    const { unmount } = render(
      <CollapsibleSection id="t2" title="Dead code"><p>rows</p></CollapsibleSection>,
    );
    fireEvent.click(screen.getByRole("button", { name: /Dead code/ }));
    expect(localStorage.getItem("lsm-sec-c:t2")).toBe("1");
    unmount();
    render(<CollapsibleSection id="t2" title="Dead code"><p>rows</p></CollapsibleSection>);
    expect(screen.queryByText("rows")).not.toBeInTheDocument();
  });

  /* The point of the feature: a section the user drags taller must stay taller. */
  it("applies a stored height and clamps a stored value that is out of range", () => {
    localStorage.setItem("lsm-sec-h:t3", "300");
    const { container, unmount } = render(
      <CollapsibleSection id="t3" title="Folders"><p>tree</p></CollapsibleSection>,
    );
    expect((container.firstChild as HTMLElement).style.height).toBe("300px");
    unmount();

    localStorage.setItem("lsm-sec-h:t3", "99999");
    const { container: c2 } = render(
      <CollapsibleSection id="t3" title="Folders"><p>tree</p></CollapsibleSection>,
    );
    expect((c2.firstChild as HTMLElement).style.height).toBe("900px");
  });

  /* A collapsed section must not keep reserving its height, or collapsing one would leave a hole instead of giving the space back. */
  it("drops its fixed height while collapsed", () => {
    const { container } = render(
      <CollapsibleSection id="t4" title="Missed files" defaultHeight={200}>
        <p>x</p>
      </CollapsibleSection>,
    );
    expect((container.firstChild as HTMLElement).style.height).toBe("200px");
    fireEvent.click(screen.getByRole("button", { name: /Missed files/ }));
    expect((container.firstChild as HTMLElement).style.height).toBe("");
  });

  /* The last section takes leftover space, so it has no height to drag. */
  it("gives a fill section no drag handle and no fixed height", () => {
    const { container } = render(
      <CollapsibleSection id="t5" title="Folders" fill><p>tree</p></CollapsibleSection>,
    );
    const root = container.firstChild as HTMLElement;
    expect(root.style.height).toBe("");
    expect(root.className).toContain("flex-1");
    expect(container.querySelector('[title^="Drag to resize"]')).toBeNull();
  });

  it("gives a sized section a drag handle", () => {
    const { container } = render(
      <CollapsibleSection id="t6" title="Filters"><p>x</p></CollapsibleSection>,
    );
    expect(container.querySelector('[title="Drag to resize Filters"]')).not.toBeNull();
  });

  it("renders header-right content outside the toggle button", () => {
    render(
      <CollapsibleSection id="t7" title="Filters" right={<button>All</button>}>
        <p>x</p>
      </CollapsibleSection>,
    );
    /* Nested buttons are invalid HTML and swallow the inner click, so "All" must be its own button, not a descendant of the toggle. */
    const all = screen.getByRole("button", { name: "All" });
    const toggle = screen.getByRole("button", { name: /Filters/ });
    expect(toggle.contains(all)).toBe(false);
  });

  it("reports its expanded state for assistive tech", () => {
    render(<CollapsibleSection id="t8" title="Filters"><p>x</p></CollapsibleSection>);
    const toggle = screen.getByRole("button", { name: /Filters/ });
    expect(toggle).toHaveAttribute("aria-expanded", "true");
    fireEvent.click(toggle);
    expect(toggle).toHaveAttribute("aria-expanded", "false");
  });
});
