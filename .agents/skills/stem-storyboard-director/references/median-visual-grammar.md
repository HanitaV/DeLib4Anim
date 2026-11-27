# Median Visual Grammar & Design Tokens

Distilled from the two Median reference videos in `docs/videos/` and aligned with `examples/incline.py` and `examples/chemistry_electron.py`.

---

## 1. Backdrop Modes

| Mode | Use Case | `SceneBuilder.set_background(...)` Tokens |
| :--- | :--- | :--- |
| **Classical Starfield** | Mechanics, orbits, projectiles, vectors, everyday physics | `top_color=(0.014, 0.015, 0.018, 1.0)`, `bottom_color=(0.008, 0.009, 0.011, 1.0)`, `starfield=1.50`, `vignette=0.22`, `grain=0.004`, `show_grid=False` |
| **Quantum Void Studio** | Wavefunctions, orbitals, particles, slit experiments, energy ladders | `top_color=(0.008, 0.009, 0.011, 1.0)`, `bottom_color=(0.042, 0.044, 0.050, 1.0)`, `starfield=0.0`, `vignette=0.28`, `grain=0.004`, `show_grid=False` (plus optional 3D perspective floor grid lines) |

---

## 2. Semantic Color Lock (Never Reassign Meaning Mid-Video)

| Token Name | Hex | Normalized RGBA | Assigned Physical Meaning |
| :--- | :--- | :--- | :--- |
| **Primary Ink / White** | `#F3F5F8` | `(0.953, 0.961, 0.973, 1.0)` | Primary text, normal force $\vec{N}$, velocity $\vec{v}$, probability curves, axes |
| **Emerald** | `#59C975` | `(0.349, 0.788, 0.459, 1.0)` | Gravity $\vec{P}$, acceleration $\vec{a}$, active driving force |
| **Coral** | `#EB5E55` | `(0.922, 0.369, 0.333, 1.0)` | Friction $\vec{F}_{\text{mst}}$, drag, opposing/damping forces, warning limits |
| **Gold** | `#F2C94C` | `(0.949, 0.788, 0.298, 1.0)` | Displacement $s$, angle $\theta$, stroboscopic markers, highlight accents |
| **Muted Slate** | `#999FAB` | `(0.600, 0.624, 0.671, 1.0)` | Vietnamese vector subtitles, inactive energy levels, secondary annotations |
| **Structural Gray** | `#575B64` | `(0.340, 0.355, 0.390, 0.72)` | Perspective floor grid lines, split-screen divider, background geometry |
| **Orbital Cloud** | `#DCE0E8` | `(0.860, 0.878, 0.908, 1.0)` | 3D quantum probability point-cloud particles |

---

## 3. Opacity & Visual Salience Hierarchy

Direct the viewer's eye strictly by luminance and alpha:
- **Primary Active Element:** `1.0` opacity (full crisp white or semantic accent).
- **Supporting Dashed Guides / Projection Lines:** `0.55–0.72` opacity (`dashed=(7.0, 6.0)` or `(8.0, 6.0)`).
- **Previous Derivation Steps / Inactive Callouts:** `0.34–0.40` opacity via `Formula.dim_to(t0, t1, 0.35, 1.0)` or `CalloutBadge.dim(t0, t1)` — never delete previous equations prematurely if the viewer still needs visual context!

---

## 4. Typography & Canvas Coordinate System

- **Canvas:** `1920 × 1080`, origin `(0.0, 0.0)` at screen center; `+X` right (`[-960, +960]`), `+Y` up (`[-540, +540]`).
- **SafeArea:** `margin_x=72.0`, `margin_y=54.0` -> usable bounds `x in [-888, +888]`, `y in [-486, +486]`.
- **Typography Scale:**
  - Section Title / Top Header: `40 px`, `semibold` (e.g., `y = +325`)
  - Corner State Tag (top-left): `34 px`, `semibold`, `align_h=-1` at `(-850, 460)` (e.g., `"2p, m = 1"`)
  - Vector Label Symbol + Subtitle: `29–31 px` bold-italic symbol + over-arrow + `0.72x` muted Vietnamese name
  - Callouts, Axes & Captions: `22–26 px`
  - MathJax Formula `target_height_px`: `62–72 px` centerpiece, `42–46 px` inside `FormulaCard` or right-side derivation column

---

## 5. Eight Signature Median Visual Idioms

1. **Minimalist Centerpiece + Perspective Floor Grid:** Single hero object centered (`y ≈ +28`) over subtle perspective grid lines (`Electron` Shot 1) or flat dark ground (`Newton` paper & ball).
2. **Top-Center Formula Card + Lower Live Diagram:** Rounded dark card at `(0, +398)` with `1.6 px` border (`\textcolor{#HEX}{...}` color-matched to vectors below), leaving `y in [-300, +250]` clean for physics motion.
3. **Right-Column Focus-Dimmed Derivation Stack:** Equations stacked vertically on the right (`x ≈ +455`, `y = +225, +115, 0`). As each new line reveals, the previous line dims to `0.35` opacity.
4. **Paired Vector Badge (`VectorLabelGroup`):** Colored bold-italic symbol with crisp over-arrow + optional subscript + muted Vietnamese name right beside the vector (e.g., $\vec{P}$ *trọng lực*, $\vec{N}$ *lực pháp tuyến*).
5. **Stroboscopic Equal-Time Dots + Quadratic Dimension Arrow:** Small gold dots dropped along a path at fixed `Δt` intervals so acceleration ($s \propto t^2$) is visible in one glance, paired with a parallel dimension arrow.
6. **Split-Screen Motion + Synchronized Plot (`x = 0` Divider):** Camera shifts the physical system cleanly into the left column (`x < -80`) while revealing a thin vertical divider at `x = 0` and live `v(t)`, `s(t)`, or `θ(t)` plots with moving tracer dots + dashed projection guides on the right (`x > +120`).
7. **Sequential Numbered Callout Badges (`CalloutBadge`):** Circular `1`, `2`, `3` badges stacked vertically on the right (`y = +118, 0, -118`), illuminating active circle white-on-dark (`activate`) and dimming previous items (`dim`) as narration walks through 3 rules.
8. **Viewfinder Brackets (`ViewfinderBrackets`) + Measurement Collapse:** Four crisp L-corner brackets framing a 3D orbital or key simulation during synthesis, followed by dramatic collapse to a single glowing dot + wave re-spread.
