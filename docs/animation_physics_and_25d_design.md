# Animation Physics & Layered 2D / 2.5D Design Specification

This document standardizes DeLib4Anim's **presentation motion physics** (stopping inertia, harmonious entrances, and micro-staggered overlapping action) and its **Orthographic Layered 2D / 2.5D-First Design System** (reserving true 3D strictly for spatially irreducible phenomena).

---

## 1. Animation Physics Principles for STEM

### 1.1 Scientific vs. Presentation Invariance Contract
Scientific integrity is strictly isolated from visual acting:
- **Scientific state**: Physics trajectories (`Trajectory2D` with fixed $\Delta t = 1/240\,\text{s}$), free-body force vector magnitudes/angles, plot curves, and `LayoutBox` / `SafeArea` / `check_overlaps()` bounds read *only* scientific tracks (`center`, `position`, `scale`, `rotation_rad`, `delta`).
- **Presentation state**: Acting primitives (`pop_in`, `pop_out`, `inertia_stop`) write *only* `presentation_offset`, `presentation_scale`, `presentation_rotation_rad`, `elevation`, and `visible`.
- Because `LayoutBox` and `Trajectory2D` never inspect `presentation_*` tracks, adding stopping recoil or pop overshoot never corrupts physical measurements or triggers spurious layout shifts.
- All easings and impulse responses are **purely stateless functions of timestamp $t$**, preserving exact seek determinism (`0 -> 5 -> 2 -> 5`).

### 1.2 Stopping Inertia & Follow-Through (`EasingType::Impulse`)
When a moving rigid body arrests at contact ($t = t_{\text{stop}}$), an abrupt clamp to $v = 0$ feels robotic and weightless. Instead, `Track<T>` evaluates a normalized, closed-form damped harmonic velocity-impulse response $w(u)$ over $u \in [0, 1]$ with $w(0) = 0$, $\max_u w(u) = 1$, and $w(1) = 0$:

For underdamped oscillatory settle ($\zeta < 0.999$, $\omega_d = \omega\sqrt{1 - \zeta^2}$):
$$
f(u) = e^{-\zeta \omega u}\sin(\omega_d u)\,(1 - u)^2, \qquad w(u) = \frac{f(u)}{f(u_{\text{peak}})}
$$
where $u_{\text{peak}} = \operatorname{clamp}\!\left(\frac{\operatorname{atan2}(\sqrt{1-\zeta^2},\, \zeta)}{\omega_d},\; 0.02,\; 0.45\right)$.

For critically/overdamped surge-and-settle ($\zeta \ge 0.999$):
$$
g(u) = (\omega u)\,e^{1 - \omega u}\,(1 - u)^2, \qquad w(u) = \frac{g(u)}{g(u_{\text{peak}})}, \qquad u_{\text{peak}} = \operatorname{clamp}\!\left(\frac{1}{\omega + 2},\; 0.02,\; 0.35\right)
$$

Because $w(1) = 0$, `Track<T>::evaluate(t)` returns `v0` (rest state) for $t \ge t_1$ when `easing == EasingType::Impulse`.

Three coupled channels express physical weight on stop:
1. **Translational inertia excursion (`presentation_offset`)**: Peak displacement $\Delta\vec{p} = \hat{d}\,d_{\max}$ along the incoming velocity direction $\hat{d}$, with $d_{\max} \in [5, 11]\,\text{px}$ (or inferred from `Trajectory2D::evaluate_vel(t_stop - 0.025)`).
2. **Center-of-Mass (COM) braking pitch (`presentation_rotation_rad`)**: Forward torque tilt proportional to horizontal velocity component:
   $$\Delta\theta = -\operatorname{sign}(d_x)\,\theta_{\max}, \qquad \theta_{\max} \in [0.02, 0.05]\,\text{rad}$$
3. **Volume/Area-preserving contact squash-and-stretch (`presentation_scale`)**: Compressing along the impact axis by $\varepsilon \in [0.04, 0.10]$ expands the perpendicular axis:
   $$s_\parallel = 1 - \varepsilon, \qquad s_\perp = \frac{1}{1 - \varepsilon}$$

### 1.3 Harmonious Element Entrance (`pop_in` / `float_in`)
Elements appearing out of nowhere at constant scale feel sterile. Harmonious STEM entrances combine four coordinated channels:
- **Non-zero initial scale + gentle overshoot**:
  - **Hero cards & formulas (`POP_HERO`)**: `duration = 0.42 s`, `initial_scale = 0.84 -> 1.0`, `rise_px = 14 px`, `omega = 16.5`, `zeta = 0.62` (`Spring`, ~4–5% crisp overshoot), alpha fade completing in the first **45%** of the span (`QuinticOut`), and dynamic shadow elevation rising from `0 -> target_elevation`.
  - **Badges, tracer dots & markers (`POP_SNAPPY`)**: `duration = 0.34 s`, `initial_scale = 0.45 -> 1.0`, `rise_px = 6 px`, `omega = 19.0`, `zeta = 0.56` (`Spring`).
  - **Titles & secondary labels (`POP_SOFT`)**: `duration = 0.46 s`, `initial_scale = 0.92 -> 1.0`, `rise_px = 10 px`, `omega = 14.0`, `zeta = 0.74` (`Spring`, minimal overshoot).
- **Available Easing Curves**:
  - `BackOut` / `BackIn`: Cubic overshoot/anticipation parameterized via `spring_zeta` ($s = \operatorname{clamp}(2.6\,\zeta, 0.35, 3.5)$).
  - `ExpoOut`: Normalized $1 - 2^{-10u}$ fast snap-in for responsive UI reveals.
  - `Spring`: C0-continuous normalized damped harmonic oscillator settling cleanly to `1.0` at $u = 1$.

### 1.4 Secondary Overlapping Action (Micro-Stagger)
Compound STEM widgets separate primary structure from inner ink by `30–45 ms`:
- **`FormulaCard.pop_in()`**: Card container pops first at $t_0$; inner MathJax `Formula` follows at $t_0 + 0.04\,\text{s}$ with slightly softer initial scale.
- **`CalloutBadge.activate()`**: Circular badge pops at $t_0$; number text settles at $t_0 + 0.025\,\text{s}$; label text glides in at $t_0 + 0.04\,\text{s}$.
- **`VectorLabelGroup.pop_in()`**: Bold symbol + over-arrow pop at $t_0$; subscript and muted subtitle follow at $t_0 + 0.035\,\text{s}$.
- **`ViewfinderBrackets.pop_in()`**: Four corner brackets settle inward from a radial outward offset (`+18 px`) using `ExpoOut` / `Spring`.

---

## 2. Layered 2D / 2.5D-First Design System (3D by Exception)

### 2.1 Core Aesthetic Rules
By default, all STEM scenes are authored in **Orthographic Layered 2D / 2.5D** — inspired by Median, Kurzgesagt, and modern motion UI — **without** fake 3D perspective floor grids or Blinn-Phong sphere highlights on flat diagram shapes:
1. **Analytical SDF Soft Elevation Shadows (`elevation`)**:
   - `RectItem` and `CircleItem` accept `ScalarTrack elevation` (in world/screen px, default `0.0`).
   - Rendered single-pass in the SDF fragment shader (`kRectFs`, `kCircleFs`) with screen-space downward offset `(0, -0.52 * elev)` (rotated back into local space for tilted rects!) and Porter-Duff `over` compositing. Zero blur FBOs, zero extra draw calls.
   - Standard elevation tiers:
     - `0 px`: Flush guides, axes, calibration swatches.
     - `6–10 px`: Physical props (inclined beams, sliding blocks, callout badges).
     - `14–18 px`: Elevated hero formula cards and foreground UI panels.
2. **Layered 2.5D Materials**:
   - **`card25d` (`RectItem`, `material_mode = 4`)**: Subtle vertical micro-gradient, 1 px top inner specular rim highlight, and soft 2 px bottom lip bevel.
   - **`disc25d` (`CircleItem`, `material_mode = 3`)**: Crisp layered flat disc with upper crescent inner rim sheen and subtle bottom lip shade — replacing `sphere3d` in non-3D diagrams.
3. **Multi-Plane Orthographic Parallax (`parallax`)**:
   - All world-space primitives (`RectItem`, `CircleItem`, `VectorItem`, `GraphItem`, `PathInstanceItem`, `TextItem`) expose `float parallax = 1.0f`.
   - Evaluated deterministically in `Scene::evaluate` as `world_p + cam_pos * (1.0f - parallax)`.
   - Depth planes:
     - Background guides / subtle coordinate ticks: `0.35 – 0.55`
     - Midground context layers: `0.65 – 0.80`
     - Primary physics / diagram stage: `1.00`
     - Foreground framing accents: `1.10 – 1.15`

### 2.2 Decision Matrix: When 2D vs. 2.5D vs. True 3D
| Visual Subject | Default Mode | Why |
| :--- | :--- | :--- |
| Force diagrams, inclined planes, blocks, pendulums, circuits | **2D / 2.5D** (`card25d`, `disc25d`, `elevation`) | Orthographic clarity keeps vector lengths, angles ($\theta$), and projections strictly readable without foreshortening distortion. |
| Formulas, derivation cards, callout badges, plots ($v(t), s(t)$) | **2D / 2.5D** (`card25d`, `elevation=8..16`) | Layered depth cues separate focal cards from background space cleanly. |
| Atomic shell diagrams, classical Bohr orbits, nucleon clusters | **2D / 2.5D** (`disc25d`, concentric parallax rings) | Pedagogically a schematic diagram; fake 3D grids add visual clutter without conveying new spatial invariants. |
| Quantum 3D orbitals ($2p, 3d_{m=2}, 4f_{m=3}$), 3D chiral molecules, $\vec{v}\times\vec{B}$ | **True 3D** (`OrbitalCloud3DItem`, camera yaw/pitch orbit) | Nodal surfaces, lobes, and azimuthal probability currents are inherently 3D and cannot be understood from a single 2D slice. |
