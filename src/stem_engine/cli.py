"""CLI entrypoint for `python -m stem_engine`."""

from __future__ import annotations

import argparse
import importlib.util
import json
import pathlib
import sys
from typing import Any

from stem_engine import _stem_native as _nat


def _load_scene_from_script(script_path: pathlib.Path, width: int, height: int) -> Any:
    spec = importlib.util.spec_from_file_location("stem_user_scene", script_path)
    if spec is None or spec.loader is None:
        msg = f"Cannot import scene script: {script_path}"
        raise RuntimeError(msg)
    mod = importlib.util.module_from_spec(spec)
    sys.modules["stem_user_scene"] = mod
    spec.loader.exec_module(mod)
    if not hasattr(mod, "build_scene"):
        msg = f"Scene script {script_path} must define `build_scene(width=..., height=...)`"
        raise RuntimeError(msg)
    built = mod.build_scene(width=width, height=height)
    return built.build() if hasattr(built, "build") else built


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="stem_engine", description="Native GPU STEM animation engine CLI")
    sub = parser.add_subparsers(dest="command", required=True)

    p_doc = sub.add_parser("doctor", help="Check OpenGL, CUDA interop, and NVENC readiness")
    p_doc.add_argument("--json", action="store_true", help="Emit JSON report")

    p_ren = sub.add_parser("render", help="Render a Python scene script to MP4 via native GPU pipeline")
    p_ren.add_argument("script", type=pathlib.Path)
    p_ren.add_argument("--output", "-o", type=pathlib.Path, default=pathlib.Path("outputs/out.mp4"))
    p_ren.add_argument("--width", type=int, default=1920)
    p_ren.add_argument("--height", type=int, default=1080)
    p_ren.add_argument("--fps", type=int, default=60)
    p_ren.add_argument("--start", type=float, default=0.0)
    p_ren.add_argument("--end", type=float, default=-1.0)
    p_ren.add_argument("--no-audio", action="store_true")
    p_ren.add_argument("--preview", action="store_true", help="Open interactive native preview window instead of exporting")

    p_prev = sub.add_parser("preview", help="Open native VSync preview window for a Python scene script")
    p_prev.add_argument("script", type=pathlib.Path)
    p_prev.add_argument("--width", type=int, default=1920)
    p_prev.add_argument("--height", type=int, default=1080)
    p_prev.add_argument("--fps", type=int, default=60)
    p_prev.add_argument("--max-seconds", type=float, default=-1.0)

    p_bench = sub.add_parser("benchmark", help="Run native GPU benchmark suite (no raw CPU readbacks)")
    p_bench.add_argument("--suite", default="baseline")
    p_bench.add_argument("--label", default="baseline")
    p_bench.add_argument("--runs", type=int, default=3)
    p_bench.add_argument("--interop-batch", type=int, default=8)
    p_bench.add_argument("--include-4k", action="store_true")
    p_bench.add_argument("--demo-script", type=pathlib.Path, default=None)
    p_bench.add_argument("--output", "-o", type=pathlib.Path, default=pathlib.Path("outputs/benchmark.json"))
    p_bench.add_argument("--csv", type=pathlib.Path, default=None)

    args = parser.parse_args(argv)

    if args.command == "doctor":
        if args.json:
            print(_nat.doctor_json())
            rep = json.loads(_nat.doctor_json())
            return 0 if rep.get("ok") else 1
        print(_nat.doctor_text(), end="")
        rep = json.loads(_nat.doctor_json())
        return 0 if rep.get("ok") else 1

    if args.command == "preview":
        scene = _load_scene_from_script(args.script, args.width, args.height)
        if args.max_seconds <= 0.0:
            print(
                "Preview controls: Space=Play/Pause | Left/Right=Seek | ,/.=Frame Step | "
                "1-9=Jump Cue | R=Restart | M=Mute | Esc=Quit"
            )
        _nat.preview_scene(scene, _nat.RationalFPS(args.fps, 1), args.max_seconds)
        return 0

    if args.command == "render":
        scene = _load_scene_from_script(args.script, args.width, args.height)
        if args.preview:
            print(
                "Preview controls: Space=Play/Pause | Left/Right=Seek | ,/.=Frame Step | "
                "1-9=Jump Cue | R=Restart | M=Mute | Esc=Quit"
            )
            _nat.preview_scene(scene, _nat.RationalFPS(args.fps, 1), -1.0)
            return 0
        opts = _nat.ExportOptions()
        opts.output_path = str(args.output)
        opts.width = args.width
        opts.height = args.height
        opts.fps = _nat.RationalFPS(args.fps, 1)
        opts.start_sec = args.start
        opts.end_sec = args.end
        opts.enable_audio = not args.no_audio
        m = _nat.render_scene_to_mp4(scene, opts)
        print(
            f"Exported {args.output} | {m.frame_count} frames @ {m.width}x{m.height} | "
            f"Wall={m.total_wall_ms:.1f} ms | Throughput={m.export_fps:.2f} fps | "
            f"GL={m.gl_gpu_avg_ms:.3f} ms | CUDA={m.cuda_convert_avg_ms:.3f} ms | "
            f"RawReadbacks={m.raw_frame_readbacks} | Size={m.output_file_bytes} B"
        )
        return 0

    if args.command == "benchmark":
        demo_scene = None
        if args.demo_script is not None:
            demo_scene = _load_scene_from_script(args.demo_script, 1920, 1080)
        csv_path = args.csv if args.csv is not None else args.output.with_suffix(".csv")
        json_str = _nat.run_benchmark_suite(
            demo_scene,
            args.suite,
            args.label,
            args.runs,
            args.include_4k,
            str(args.output),
            str(csv_path),
            args.interop_batch,
        )
        data = json.loads(json_str)
        print(f"Benchmark ({data['label']}) on {data['gpu']} | {data['codec']} | preset={data['nvenc_preset']}")
        for w in data["workloads"]:
            print(
                f"  {w['name']:36s} {w['width']}x{w['height']} | "
                f"wall={w['total_wall_ms']:8.1f} ms | {w['export_fps']:6.2f} fps ({w['realtime_x']:.2f}x RT) | "
                f"GL={w['gl_gpu_avg_ms']:.3f}ms | CUDA={w['cuda_convert_avg_ms']:.3f}ms | "
                f"p95_frame={w['frame_wall_p95_ms']:.2f}ms | readbacks={w['raw_frame_readbacks']}"
            )
        return 0

    return 2
