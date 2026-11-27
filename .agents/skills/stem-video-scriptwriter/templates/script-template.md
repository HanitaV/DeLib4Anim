# Script: [Tiêu đề video]

## Metadata
- **Shape:** [Concept Mystery Essay / Foundational Course Pillar / Short stem_engine Demo]
- **Language:** [vi / en]
- **Target Audience:** [e.g., Curious general viewer, high-school physics intuition]
- **Target Runtime:** [e.g., 210s (3m30s) @ 140 WPM]

## 1. Narrative Blueprint
- **Core Promise:** [One sentence: what the viewer will fundamentally understand by the end.]
- **Target Misconception:** [The wrong intuition the viewer starts with.]
- **Open-Loop Contract:**
  1. [Loop 1 question] -> Paid off in Chapter [X]
  2. [Loop 2 question] -> Paid off in Chapter [Y]
- **Central Metaphor (+ Breakdown):** [Familiar analogy] -> Breaks down when [exact physical limit].
- **Emotional Arc:** Curiosity -> Paradox/Struggle -> Partial Clarity -> Aha Moment -> Reframed Satisfaction.
- **Dependency Staircase & Callbacks:**
  - Staircase: Ch.1 [A] -> Ch.2 [B uses A] -> Ch.3 [C uses A+B]
  - Recurring Toy Systems: [e.g., pendulum bob on string, playground swing]

## 2. Candidate Hooks (3 Scored Options)
- **Option A ([Formula Name] — Score: X/10):** "[Draft hook]" *(Note: why it works/fails)*
- **Option B ([Formula Name] — Score: Y/10):** "[Draft hook]" *(Note: why it works/fails)*
- **Option C ([Formula Name] — Score: Z/10):** "[Draft hook]" *(Chosen — highest visual tension & promise fit)*

---

## 3. Full Spoken Script

### Opening: [Title] [CORE]
> [Exact spoken narration. Zero raw LaTeX, zero em-dashes, <=35 words/sentence.]
>
> [Ending bridge leading into Chapter 1.]
>
> Visual intent: [1-2 sentences describing what appears on screen, no pixel coordinates.]

### Chapter 1: [Title] [CORE]
> [Exact spoken narration with [.], [..], [...], [BEAT] pause tags and **bold** stress.]
>
> [Last-sentence causal bridge motivating Chapter 2.]
>
> Visual intent: [1-2 sentences.]

### Chapter 2: [Title] [CORE]
> [Exact spoken narration.]
>
> [Last-sentence causal bridge motivating Chapter 3.]
>
> Visual intent: [1-2 sentences.]

### Closing: [Title] [CORE]
> [Explicitly close all Open Loops planted in the Opening, reframe the core takeaway, and end cleanly.]
>
> Visual intent: [1-2 sentences.]

---

## 4. Cut Plan & Packaging
- **Cut Plan:** If runtime exceeds budget, trim `[CUTTABLE]` sections in order: [list]. Never cut Opening loops, Core Aha beat, or Loop Closure.
- **YouTube Chapters:** *(Verify against `lint_stem_script.py` output)*
  - `00:00` [Opening title]
  - `MM:SS` [Chapter 1 title]
- **Pronunciation & Caption Glossary:**
  - `[Term]` -> Spoken: `[spoken form]` | On-screen: `[display form]`
- **Storyboard Handoff:** Ready for `stem-storyboard-director` to build shot table & `stem_engine` timeline.
