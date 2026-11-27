# Storyboard: Con Lắc Đơn & Bí Ẩn Chu Kỳ Độc Lập Khối Lượng (26s Showcase)

## Metadata
- **Tier:** Tier 2 (`stem_engine` Production Shot Spec)
- **Backdrop Mode:** Classical Starfield (`starfield=1.50`, `vignette=0.22`)
- **Resolution & FPS:** 1920x1080 @ 60fps
- **Source Script:** `sample-script-con-lac-don.md` (condensed 26s visual demo)

## Semantic Color Lock
- `#F3F5F8` Primary Ink / Velocity `v` / Tension `T` / Plot Axes
- `#59C975` Gravity `P` / Acceleration `a`
- `#EB5E55` Restoring Force `P_t` / Small-Angle Error Zone
- `#F2C94C` Angle `theta` / Period `T` Highlight / Strobe Dots
- `#999FAB` Muted Vietnamese Vector Subtitles & Reference Arcs
- `#575B64` Structural Split-Screen Divider & Reference Guides

---

## Shot & Beat Table

| Shot | Time (s) | Visual & Layout (1920x1080) | Motion & Camera | Spoken VO | Sync & SFX |
| :--- | :--- | :--- | :--- | :--- | :--- |
| Shot 1 | 0.0-6.0 | Center pivot at `(0, 260)` on starfield; two side-by-side pendulums (`x = -240` mass `1m`, `x = +240` mass `3m`) with glowing `sphere3d` bobs; minimal label `"1m"` / `"3m"` (`#999FAB`). | Camera static `(0, 0)` zoom `1.05`; both bobs pull to `+12 deg` over `0.6-1.4s`, release at `1.8s` and swing in lockstep (`dt = 1/240s`). | "Treo hai quả nặng lệch nhau ba lần. Hai con lắc vẫn chạm đáy cùng lúc." | Sync: "cùng lúc" -> both bobs cross `theta=0` at `t=3.4s`; SFX: `Click` @ `1.80s`, `Chime` @ `3.40s` |
| Shot 2 | 6.0-12.8 | Single pendulum centered at `(0, 260)` held at `+12 deg`; `vec_p` (`#59C975`, `P trọng lực`), `vec_t` (`#F3F5F8`, `T lực căng`), dashed orthogonal projection (`#59C975` @ `0.58`), tangential `vec_pt` (`#EB5E55`, `P_t kéo về`). Top `FormulaCard` at `(0, 398)`: `m a = -m g sin(theta)`. | Camera pans smoothly `6.0-6.9s` to zoom `1.22` centered on bob; vectors reveal `6.8-8.8s` (geometry before formula); `FormulaCard` reveals `9.0-9.6s`; `1.2s` hold. | "Phần tiếp tuyến của trọng lực kéo quả nặng về giữa, tỷ lệ đúng theo khối lượng." | Sync: "kéo quả nặng" -> `vec_pt` reveal; SFX: `Click` @ `7.40s` |
| Shot 3 | 12.8-19.6 | Right-side derivation stack (`x = +430`, `y = 225, 115, 0`): line 1 highlights `m` on both sides then cancels/dims (`dim_to(0.35)`), line 2 `a = -g sin(theta) ≈ -g theta`, line 3 `FormulaCard` `T = 2 pi sqrt(L/g)` (`#F2C94C` border). Left stage shows pendulum swing with gold `theta` arc. | Camera moves `12.8-13.6s` to `pos=(220, 0)` zoom `1.0`; step-by-step equation reveals with `dim_to(0.35)` on prior steps; `1.5s` breathing hold at end. | "Khối lượng vừa tăng lực kéo, vừa tăng sức ì. Chúng tự triệt tiêu hoàn toàn." | Sync: "tự triệt tiêu" -> `m` cross-fade out; SFX: `Chime` @ `15.40s` |
| Shot 4 | 19.6-26.0 | Split screen with `x = 0` vertical divider (`#575B64`). Left (`x = -460`): pendulum swinging at `6 deg` and `12 deg` ghost overlay with `#F2C94C` strobe dots. Right (`x = +140..780`): live `theta(t)` sinusoid plot with `#F2C94C` tracer dot + dashed sync guide. | Camera shifts `19.6-20.4s` cleanly into left column; `Graph.reveal` draws `theta(t)` synchronously from `t=20.6s` to `25.0s`; hold final frame `1.0s`. | "Góc rộng hơn có đường dài hơn, nhưng độ dốc gắt đẩy vật chạy nhanh hơn để bù lại." | Sync: "chạy nhanh hơn" -> dual zero-crossing flash; SFX: `Contact` @ `22.50s` |

---

## `stem_engine` Implementation Notes (Tier 2)
- **Physics / Trajectory Precompute:** Integrate exact nonlinear pendulum ODE $\ddot{\theta} = -(g/L)\sin\theta$ via RK4 at `dt = 1/240.0` for `L = 1.8m` (`pixel_len = 460 px`).
- **Active Windows & SafeArea:** Every `add_text`, `add_vector_label`, `add_formula`, and `add_formula_card` passes explicit `active_window=(t0, t1)` inside `[-888..888] x [-486..486]`.
- **Verification:** Validated with `validate_storyboard.py --mode shot --strict`; confirm `sb.check_overlaps() == []` in Python before `python -m stem_engine render`.
