"""STEM Engine: Native GPU STEM animation engine (Python authoring -> C++20 -> OpenGL -> CUDA -> NVENC)."""

from __future__ import annotations

import os
import pathlib
import shutil
import sys
import sysconfig

# Must be set BEFORE opengl32.dll loads in python.exe on Hybrid/Optimus laptops so OpenGL binds to the NVIDIA dGPU.
if sys.platform == "win32":
    os.environ.setdefault("SHIM_MCCOMPAT", "0x800000001")
    _pkg_dir = pathlib.Path(__file__).resolve().parent
    _platlib_pkg = pathlib.Path(sysconfig.get_path("platlib")) / "stem_engine"
    _repo_root = _pkg_dir.parent.parent
    _dll_dirs = [
        _pkg_dir,
        _platlib_pkg,
        _repo_root / "build" / "release",
        _repo_root / "build" / "release" / "_deps" / "ffmpeg_win64-src" / "bin",
    ]
    _gxx = shutil.which("g++")
    if _gxx:
        _dll_dirs.append(pathlib.Path(_gxx).resolve().parent)
    for _cand in _dll_dirs:
        if _cand.is_dir():
            os.add_dll_directory(str(_cand))

from stem_engine import _stem_native  # ty: ignore[missing-module-member]
from stem_engine.authoring import (
    AudioCue,
    Body,
    CalloutBadge,
    Camera,
    Cue,
    Formula,
    FormulaCard,
    Graph,
    LayoutBox,
    OrbitalCloud,
    OverlapWarning,
    SafeArea,
    SceneBuilder,
    Stickman,
    Text,
    Vector,
    VectorLabelGroup,
    ViewfinderBrackets,
    load_excalidraw_anchors,
    resolve_font,
)
from stem_engine.formula import compile_tex_to_svg
from stem_engine.motion import (
    POP_HERO,
    POP_SNAPPY,
    POP_SOFT,
    InertiaSpec,
    PopSpec,
    compute_stop_inertia_from_trajectory,
)

__all__ = [
    "POP_HERO",
    "POP_SNAPPY",
    "POP_SOFT",
    "AudioCue",
    "Body",
    "CalloutBadge",
    "Camera",
    "Cue",
    "Formula",
    "FormulaCard",
    "Graph",
    "InertiaSpec",
    "LayoutBox",
    "OrbitalCloud",
    "OverlapWarning",
    "PopSpec",
    "SafeArea",
    "SceneBuilder",
    "Stickman",
    "Text",
    "Vector",
    "VectorLabelGroup",
    "ViewfinderBrackets",
    "_stem_native",
    "compile_tex_to_svg",
    "compute_stop_inertia_from_trajectory",
    "load_excalidraw_anchors",
    "resolve_font",
]
