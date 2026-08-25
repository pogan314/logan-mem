import { useCallback, useRef, useState, type ReactNode } from "react";

/* A left-sidebar section that collapses to its header and, while open, can be dragged taller or shorter by its bottom edge. Both the collapsed flag and the height persist per section id, so the panel comes back the way it was left.
 *
 * Why a fixed pixel height rather than a flex share: the sections hold lists of unrelated lengths (filter chips, four checkboxes, a whole folder tree), so a proportional split starves whichever one the user actually wants. A height they set themselves is the only arrangement that survives changing projects.
 *
 * The last section in the column passes `fill`, which makes it take the leftover space instead of a stored height — without it, a short column leaves dead space under the final section and a tall one clips it. */

const MIN_H = 64;
const MAX_H = 900;

function loadNum(key: string, fallback: number): number {
  try {
    const v = localStorage.getItem(key);
    if (v !== null) return Math.max(MIN_H, Math.min(MAX_H, parseInt(v, 10)));
  } catch { /* ignore */ }
  return fallback;
}
function saveNum(key: string, value: number) {
  try { localStorage.setItem(key, String(Math.round(value))); } catch { /* ignore */ }
}
function loadBool(key: string, fallback: boolean): boolean {
  try {
    const v = localStorage.getItem(key);
    if (v !== null) return v === "1";
  } catch { /* ignore */ }
  return fallback;
}
function saveBool(key: string, value: boolean) {
  try { localStorage.setItem(key, value ? "1" : "0"); } catch { /* ignore */ }
}

interface CollapsibleSectionProps {
  id: string;               /* localStorage key suffix; must be stable */
  title: string;
  right?: ReactNode;        /* header-right content: counts, All|None buttons */
  defaultHeight?: number;
  defaultCollapsed?: boolean;
  fill?: boolean;           /* take the remaining column space, no drag handle */
  children: ReactNode;
}

export function CollapsibleSection({
  id,
  title,
  right,
  defaultHeight = 180,
  defaultCollapsed = false,
  fill = false,
  children,
}: CollapsibleSectionProps) {
  const hKey = `lsm-sec-h:${id}`;
  const cKey = `lsm-sec-c:${id}`;
  const [collapsed, setCollapsed] = useState(() => loadBool(cKey, defaultCollapsed));
  const [height, setHeight] = useState(() => loadNum(hKey, defaultHeight));

  const dragging = useRef(false);
  const lastY = useRef(0);

  const onPointerDown = useCallback((e: React.PointerEvent) => {
    dragging.current = true;
    lastY.current = e.clientY;
    (e.target as HTMLElement).setPointerCapture(e.pointerId);
  }, []);

  const onPointerMove = useCallback((e: React.PointerEvent) => {
    if (!dragging.current) return;
    const delta = e.clientY - lastY.current;
    lastY.current = e.clientY;
    setHeight((h) => {
      const nh = Math.max(MIN_H, Math.min(MAX_H, h + delta));
      saveNum(hKey, nh);
      return nh;
    });
  }, [hKey]);

  const onPointerUp = useCallback(() => { dragging.current = false; }, []);

  const toggle = useCallback(() => {
    setCollapsed((c) => { saveBool(cKey, !c); return !c; });
  }, [cKey]);

  /* A collapsed section is header-only regardless of `fill`, so collapsing the last one hands its space back to the others rather than leaving a gap. */
  const style = collapsed || fill ? undefined : { height };
  const outer = collapsed
    ? "shrink-0"
    : fill
      ? "flex-1 min-h-0"
      : "shrink-0";

  return (
    <div className={`flex flex-col border-b border-border/30 ${outer}`} style={style}>
      <div className="flex items-center justify-between px-4 pt-2.5 pb-1.5 shrink-0">
        <button
          onClick={toggle}
          aria-expanded={!collapsed}
          title={collapsed ? `Expand ${title}` : `Collapse ${title}`}
          className="flex items-center gap-1.5 text-[11px] font-medium text-foreground/50 hover:text-foreground/80 uppercase tracking-widest transition-colors"
        >
          <span className="text-[8px] text-foreground/30 w-2 text-center">
            {collapsed ? "▶" : "▼"}
          </span>
          {title}
        </button>
        {right && <div className="flex items-center gap-2">{right}</div>}
      </div>

      {!collapsed && <div className="flex-1 min-h-0 flex flex-col">{children}</div>}

      {/* Drag the bottom edge. A `fill` section has no handle: its height is whatever the others leave, so there is nothing to set. */}
      {!collapsed && !fill && (
        <div
          onPointerDown={onPointerDown}
          onPointerMove={onPointerMove}
          onPointerUp={onPointerUp}
          title={`Drag to resize ${title}`}
          className="h-1 cursor-row-resize hover:bg-primary/30 active:bg-primary/50 transition-colors shrink-0"
        />
      )}
    </div>
  );
}
