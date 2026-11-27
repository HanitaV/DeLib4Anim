---
name: stem-video-scriptwriter
description: >-
  Writes, structures, reviews, or rewrites retention-optimized STEM educational video scripts
  and spoken voiceovers in Median and 3Blue1Brown style (Vietnamese "mình – bạn" or English).
  Use when asked to write a science, physics, math, chemistry, or CS explainer script, turn
  lecture notes or articles into a video script, diagnose viewer drop-off or rewrite a stiff
  script ("viết kịch bản video khoa học", "kịch bản kiểu Median / 3b1b", "thuyết minh STEM",
  "chuyển tài liệu thành kịch bản video", "review/sửa kịch bản"). Do NOT use for pixel
  coordinates, shader effects, or stem_engine SceneBuilder shot code — route visual layout
  and timing manifests to `stem-storyboard-director`.
---

# STEM Video Scriptwriter (Median / 3Blue1Brown Style)

Write spoken STEM scripts that earn every equation from physical intuition, hold viewer curiosity through explicit open loops, and sound like a sharp human explainer talking to an intelligent friend.

**Phase Boundary (Script First, Visuals Second):** Focus exclusively on narrative architecture, spoken prose, pacing, and high-level `Visual intent:` notes. Never pollute narrative drafting with pixel coordinates, frame numbers, or C++/Python scene code; hand off approved scripts to `stem-storyboard-director`.

## Mode Router

Route the request before drafting:

- **Mode A — New Script (Greenfield / From Topic):** User gives a topic, phenomenon, or short-demo concept. Run the Scope Check, then Steps 1–7.
- **Mode B — Source-to-Script Transposition:** User supplies textbook pages, PDFs, papers, blog posts, or code. Extract only what the video's **Core Promise** needs; discard document headings and background fluff, reorder claims along a strict dependency staircase, then run Steps 1–7.
- **Mode C — Script Diagnosis & Rewrite:** User brings an existing draft or drop-off timestamp. Audit using the **3-Cause Model** (Logic Break, Density Drop, Unspeakable/AI Line) in [./references/narrative-archetypes-and-hooks.md](./references/narrative-archetypes-and-hooks.md), grade findings (`HIGH`/`MEDIUM`/`LOW`), preserve user facts, then rewrite only after diagnosis.

## Pre-Writing Scope Check

Ask at most 3 short, multiple-choice questions only for details missing from the prompt. Refuse vague adjectives ("hấp dẫn", "chuyên sâu", "hay") and convert them into concrete decisions:

1. **Core Promise & Misconception:** What single misconception should the viewer unlearn, or what one thing should they truly understand by the end?
2. **Audience & Prerequisites:** General curious viewer (high-school intuition), STEM undergrad, or specialist?
3. **Video Shape, Runtime & Language:**
   - **Shape 1 — Concept Mystery Essay** (5–12 min, ~140 WPM): unpacks one deep question (e.g., *Electron thực sự trông như thế nào?*).
   - **Shape 2 — Foundational Course Pillar** (15–30 min, ~142 WPM): builds a whole domain from first principles (e.g., *Cơ Học Newton*).
   - **Shape 3 — Short `stem_engine` Demo** (20–90 s, ~135 WPM): tight 3–5 shot showcase around one crisp phenomenon.
   - **Language:** Vietnamese (`mình – bạn` / `chúng ta`, default) or English (`I – you` / `we`).

Stop interviewing as soon as Promise, Audience, Shape, and Source are locked.

## 7-Step Scriptwriting Workflow

Read [./references/median-style-dna.md](./references/median-style-dna.md) for verbatim patterns and [./references/vietnamese-voice-and-humanizer.md](./references/vietnamese-voice-and-humanizer.md) for spoken-language rules before drafting.

### Step 1: Lock the Narrative Blueprint
Write the opening blueprint block first (see [./templates/script-template.md](./templates/script-template.md)):
- **Core Promise** (1 sentence; if it needs "and" for two unrelated goals, split into two videos).
- **Target Misconception** (the specific mental movie the viewer starts with, e.g., *"electrons are tiny marbles orbiting a sun"*).
- **Open-Loop Contract** (1–3 crisp questions planted in the Cold Open, tagged with the exact chapter where each pays off).
- **Central Metaphor + Breakdown Boundary** (one everyday analogy that carries across chapters, plus where it stops being exact so the viewer is never misled).
- **Emotional Arc** (`Curiosity/Surprise -> Struggle/Paradox -> Partial Clarity -> Aha Moment -> Reframed Satisfaction`).

### Step 2: Engineer 3 Scored Candidate Hooks
Draft **3 candidate hooks** using distinct formulas from [./references/narrative-archetypes-and-hooks.md](./references/narrative-archetypes-and-hooks.md). Score each out of `/10` on specificity, misconception tension, visualizability, and zero-fluff speed. Pick the highest-scoring hook (>=7/10; otherwise rewrite) and keep the runner-ups under `Other Hooks`.
- Long-form (`Shape 1/2`): Cold open lands the hook + opens the loop inside `30–55s` (`~70–130` words).
- Short demo (`Shape 3`): Hook lands in `0–5s` (`<= 15` words).

### Step 3: Build the Dependency Staircase & Toy-System Callbacks
Order chapters so Concept $k$ uses **only** Ideas $1 \dots k-1$:
- Zero unintroduced jargon or forward references (*"sẽ học sau"*), unless explicitly parked as an Open Loop.
- Plant **2–3 recurring concrete toy systems** (e.g., an ant on a ruler, a car on a roundabout, a coffee cup on a braking train) and reuse them across later chapters to minimize cognitive reload.
- Tag each chapter `[CORE]` or `[CUTTABLE]` now so runtime trimming never happens ad-hoc.

### Step 4: Draft Word-for-Word Spoken Narration + Visual Intent
Write **100% verbatim spoken words** inside `>` blockquotes (never bullet outlines or `[giải thích phần này]`), paired with a concise `> Visual intent:` line after each beat. Follow Median's 6 craft rules:
1. **Phenomenon & Geometry Before Algebra:** Make the viewer feel the physical bottleneck or see the motion first; earn every symbol/equation afterward.
2. **Spoken Math Only:** Never put raw LaTeX (`\frac`, `\vec{a}`, `$x^2$`) in spoken narration. Write how a human actually speaks it aloud (*"bình phương độ lớn của hàm sóng"*, *"F bằng m nhân a"*).
3. **Bodily & Everyday Anchors:** Ground abstract quantities in felt sensation (pushed back into an airplane seat, leaning against a car door, pushing a heavy cabinet) and relatable local references.
4. **Pre-emptive Counter-Moves:** Anticipate the viewer's exact objection (*"Bạn có thể nghĩ: vậy thử dùng photon nhẹ hơn?"*) and answer it immediately.
5. **Epistemic Honesty & Visual Guardrails:** Flag analogies as intuition rather than dogma; before dazzling visuals confuse the viewer, list 2–3 clarifying rules (*"chấm sáng không phải nhiều electron..."*).
6. **Mandatory Last-Sentence Chapter Bridges:** Every chapter's final 1–2 sentences must expose a gap (*"mới trả lời khi nào thay đổi, chưa biết thay đổi bao nhiêu"*) that motivates the very next chapter.

### Step 5: Audio-Only & Humanizer Pass
Read the narration column aloud with the screen covered:
- **Zero Empty Deictics:** Ban *"như bạn thấy trên màn hình"*, *"nhìn vào đây"*, *"as you can see here"*. Integrate the visual fact into the spoken line.
- **Anti-AI-Slop Scrub:** Remove all tells in [./references/vietnamese-voice-and-humanizer.md](./references/vietnamese-voice-and-humanizer.md) (no *"không chỉ X mà còn Y"*, no *"hãy cùng khám phá"*, no em-dashes `—` in VO, no forced triads, no textbook passive voice).
- **Breath & Pacing Markers:** Keep sentences speakable in one breath (`<= 35` words). Add `[.]` (0.4s), `[..]` (0.8s), `[...]` (1.5s), or `[BEAT]` (2.5s) after key reveals, and **bold** only stressed words.

### Step 6: Run Deterministic Script Linter & Runtime Budget
Run the verification script on the saved Markdown file (default `140` WPM for Vietnamese Median cadence):

```powershell
.venv\Scripts\python.exe .agents/skills/stem-video-scriptwriter/scripts/lint_stem_script.py <path/to/script.md> --target-sec <seconds>
```

Fix every `ERROR` (raw LaTeX in VO, malformed structure) and address all `WARNING`s (AI-slop phrases, empty deictics, em-dashes, breath length, missing `Visual intent:`, runtime drift >15%, chapter <10s). Re-run until clean.

### Step 7: Finalize Cut Plan, Chapters & Handoff
Append:
- **Cut Plan:** Which `[CUTTABLE]` chapters/beats drop first if runtime runs long, while keeping every Open Loop closed.
- **YouTube Chapters:** Starting at `00:00`, ascending, each `>= 10s`, phrased as curiosity/search outcomes.
- **Pronunciation / Caption Glossary:** Foreign scientist names, units, symbols mapped to spoken vs. on-screen captions.
- **Next Step:** Offer handoff to `stem-storyboard-director` to build the shot-by-shot visual storyboard / `stem_engine` script.

## Pass-Threshold Gates (All Must Hold Before Delivery)

1. Core Promise is one sentence; Target Misconception and Open Loops are explicit.
2. Three distinct candidate hooks drafted and scored; winner >=7/10 and opens inside budget.
3. Dependency staircase has zero forward references; every Open Loop planted is explicitly closed.
4. Every chapter ends with a causal bridge motivating the next chapter (except the final outro).
5. Spoken narration is 100% verbatim with zero raw LaTeX, zero em-dashes, zero empty deictics, and zero AI-slop tells.
6. Every chapter includes a `Visual intent:` note and `[CORE]` / `[CUTTABLE]` tag.
7. `lint_stem_script.py` passes with `0` errors and budget within tolerance.
8. Reference worked example consulted when needed: [./examples/sample-script-con-lac-don.md](./examples/sample-script-con-lac-don.md).
