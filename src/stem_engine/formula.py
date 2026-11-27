"""Offline TeX formula -> MathJax SVG compiler & disk cache (no TeX Live required)."""

from __future__ import annotations

import hashlib
import json
import pathlib
import shutil
import subprocess

COMPILER_VERSION = "mathjax-full@3.2.2+stem-svg-v1"
MATHJAX_PINNED_VERSION = "3.2.2"


def _repo_root() -> pathlib.Path:
    return pathlib.Path(__file__).resolve().parent.parent.parent


def _cache_dir() -> pathlib.Path:
    d = _repo_root() / ".stem_cache" / "formulas"
    d.mkdir(parents=True, exist_ok=True)
    return d


def _mathjax_tool_dir() -> pathlib.Path:
    return _repo_root() / ".stem_cache" / "mathjax_tool"


def _ensure_mathjax_runner() -> pathlib.Path | None:
    """Ensures a local offline Node + mathjax-full@3.2.2 converter script exists."""
    node = shutil.which("node")
    npm = shutil.which("npm")
    if not node or not npm:
        return None

    tool_dir = _mathjax_tool_dir()
    runner = tool_dir / "tex2svg.mjs"
    mj_pkg = tool_dir / "node_modules" / "mathjax-full" / "package.json"

    if not mj_pkg.exists():
        tool_dir.mkdir(parents=True, exist_ok=True)
        pkg_json = {
            "name": "stem-mathjax-offline",
            "private": True,
            "type": "module",
            "dependencies": {"mathjax-full": MATHJAX_PINNED_VERSION},
        }
        (tool_dir / "package.json").write_text(json.dumps(pkg_json, indent=2), encoding="utf-8")
        subprocess.run(
            [npm, "install", "--no-audit", "--no-fund", "--silent"],
            cwd=str(tool_dir),
            check=True,
        )

    if not runner.exists():
        runner.write_text(
            """import { mathjax } from 'mathjax-full/js/mathjax.js';
import { TeX } from 'mathjax-full/js/input/tex.js';
import { SVG } from 'mathjax-full/js/output/svg.js';
import { liteAdaptor } from 'mathjax-full/js/adaptors/liteAdaptor.js';
import { RegisterHTMLHandler } from 'mathjax-full/js/handlers/html.js';
import { AllPackages } from 'mathjax-full/js/input/tex/AllPackages.js';

const chunks = [];
for await (const c of process.stdin) chunks.push(c);
const req = JSON.parse(Buffer.concat(chunks).toString('utf8'));

const adaptor = liteAdaptor();
RegisterHTMLHandler(adaptor);
const tex = new TeX({ packages: AllPackages, macros: req.macros || {} });
const svg = new SVG({ fontCache: 'local' });
const html = mathjax.document('', { InputJax: tex, OutputJax: svg });
const node = html.convert(req.tex, { display: req.display !== false });
const inner = adaptor.innerHTML(node);
process.stdout.write(inner);
""",
            encoding="utf-8",
        )
    return runner


def compile_tex_to_svg(
    tex: str,
    *,
    macros: dict[str, str] | None = None,
    font: str = "MathJax-TeX",
    display: bool = True,
) -> tuple[str, str]:
    """Compile TeX math string to SVG via cached MathJax SVG pipeline.

    Returns `(cache_key, svg_xml)`. Cache key hashes `(tex, macros, font, compiler_version)`.
    """
    payload = {
        "tex": tex,
        "macros": macros or {},
        "font": font,
        "display": display,
        "version": COMPILER_VERSION,
    }
    cache_key = hashlib.sha256(json.dumps(payload, sort_keys=True).encode("utf-8")).hexdigest()[:24]
    cached_svg = _cache_dir() / f"{cache_key}.svg"
    if cached_svg.exists():
        return cache_key, cached_svg.read_text(encoding="utf-8")

    runner = _ensure_mathjax_runner()
    node = shutil.which("node")
    if runner is None or node is None:
        msg = (
            "Node.js / mathjax-full offline formula compiler unavailable and formula not in cache: "
            f"{tex}"
        )
        raise RuntimeError(msg)

    proc = subprocess.run(
        [node, str(runner)],
        input=json.dumps(payload).encode("utf-8"),
        capture_output=True,
        check=True,
    )
    svg_xml = proc.stdout.decode("utf-8").strip()
    if "<svg" not in svg_xml:
        msg = f"MathJax SVG compiler produced invalid output for `{tex}`: {svg_xml[:200]}"
        raise RuntimeError(msg)

    cached_svg.write_text(svg_xml, encoding="utf-8")
    return cache_key, svg_xml
