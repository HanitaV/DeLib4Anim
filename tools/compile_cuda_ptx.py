#!/usr/bin/env python3
"""Compile a CUDA .cu kernel file into a C++20 embedded PTX header.

Uses `nvcc -ptx -lineinfo` when CUDA Toolkit is installed, or falls back to
NVIDIA's official NVRTC (`nvrtc64_*.dll` / `libnvrtc.so`) from `nvidia-cuda-nvrtc`.
"""

from __future__ import annotations

import argparse
import ctypes
import json
import os
import pathlib
import shutil
import subprocess
import sys

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent


def find_nvcc() -> pathlib.Path | None:
    if nvcc := shutil.which("nvcc"):
        return pathlib.Path(nvcc)
    candidates: list[pathlib.Path] = []
    if cuda_path := os.environ.get("CUDA_PATH"):
        candidates.append(pathlib.Path(cuda_path) / "bin" / ("nvcc.exe" if sys.platform == "win32" else "nvcc"))
    candidates.append(REPO_ROOT / ".stem_cache" / "toolchains" / "cuda" / "bin" / ("nvcc.exe" if sys.platform == "win32" else "nvcc"))
    for cand in candidates:
        if cand.is_file():
            return cand
    return None


def _nvcc_env_and_ccbin() -> tuple[dict[str, str], list[str]]:
    env = os.environ.copy()
    extra_args: list[str] = []
    if sys.platform != "win32":
        return env, extra_args

    cl_exe = shutil.which("cl", path=env.get("PATH"))
    msvc_json = REPO_ROOT / ".stem_cache" / "toolchains" / "msvc" / "msvc_env.json"
    if (not cl_exe or not env.get("INCLUDE")) and msvc_json.is_file():
        meta = json.loads(msvc_json.read_text(encoding="utf-8"))
        env["PATH"] = os.pathsep.join(meta["PATH"]) + os.pathsep + env.get("PATH", "")
        env["INCLUDE"] = os.pathsep.join(meta["INCLUDE"])
        env["LIB"] = os.pathsep.join(meta["LIB"])
        env.setdefault("VSCMD_VER", "17.14")
        cl_exe = meta["cl_exe"]

    if cl_exe:
        extra_args.extend(["-ccbin", str(pathlib.Path(cl_exe).parent), "-allow-unsupported-compiler"])
    return env, extra_args


def compile_with_nvcc(cu_path: pathlib.Path, arch: str, nvcc_path: pathlib.Path | None = None) -> str:
    nvcc = str(nvcc_path or find_nvcc() or "")
    if not nvcc:
        msg = "nvcc not found on PATH or .stem_cache/toolchains/cuda"
        raise FileNotFoundError(msg)
    env, ccbin_args = _nvcc_env_and_ccbin()
    cuda_bin = str(pathlib.Path(nvcc).parent)
    cuda_nvvm_bin = str(pathlib.Path(nvcc).parent.parent / "nvvm" / "bin")
    env["PATH"] = cuda_bin + os.pathsep + cuda_nvvm_bin + os.pathsep + env.get("PATH", "")
    out_ptx = cu_path.with_suffix(".tmp.ptx")
    try:
        cmd = [
            nvcc,
            "-ptx",
            "-lineinfo",
            f"-arch={arch}",
            "-O3",
            *ccbin_args,
            str(cu_path),
            "-o",
            str(out_ptx),
        ]
        subprocess.run(cmd, env=env, check=True)
        return out_ptx.read_text(encoding="utf-8")
    finally:
        if out_ptx.exists():
            out_ptx.unlink()


def find_nvrtc_dll() -> pathlib.Path:
    candidates: list[pathlib.Path] = []
    for sp in sys.path:
        p = pathlib.Path(sp)
        if p.is_dir():
            for match in p.glob("nvidia/cu*/bin/**/nvrtc64_*.dll"):
                if "builtins" not in match.name:
                    candidates.append(match)
            for match in p.glob("nvidia/cuda_nvrtc/bin/**/nvrtc64_*.dll"):
                if "builtins" not in match.name:
                    candidates.append(match)
            for match in p.glob("nvidia/*/lib/libnvrtc.so*"):
                if "builtins" not in match.name:
                    candidates.append(match)
    cuda_path = os.environ.get("CUDA_PATH")
    if cuda_path:
        cp = pathlib.Path(cuda_path) / "bin"
        if cp.is_dir():
            for match in cp.glob("nvrtc64_*.dll"):
                if "builtins" not in match.name:
                    candidates.append(match)
    if not candidates:
        msg = (
            "Neither `nvcc` nor `nvrtc64_*.dll` (`nvidia-cuda-nvrtc`) was found. "
            "Install CUDA Toolkit or run `uv sync --group build`."
        )
        raise RuntimeError(msg)
    return sorted(candidates)[-1]


def compile_with_nvrtc(cu_path: pathlib.Path, arch: str) -> str:
    dll_path = find_nvrtc_dll()
    if sys.platform == "win32":
        os.environ["PATH"] = str(dll_path.parent) + os.pathsep + os.environ.get("PATH", "")
        os.add_dll_directory(str(dll_path.parent))
    nvrtc = ctypes.CDLL(str(dll_path))

    nvrtc.nvrtcCreateProgram.argtypes = [
        ctypes.POINTER(ctypes.c_void_p),
        ctypes.c_char_p,
        ctypes.c_char_p,
        ctypes.c_int,
        ctypes.POINTER(ctypes.c_char_p),
        ctypes.POINTER(ctypes.c_char_p),
    ]
    nvrtc.nvrtcCreateProgram.restype = ctypes.c_int
    nvrtc.nvrtcCompileProgram.argtypes = [
        ctypes.c_void_p,
        ctypes.c_int,
        ctypes.POINTER(ctypes.c_char_p),
    ]
    nvrtc.nvrtcCompileProgram.restype = ctypes.c_int
    nvrtc.nvrtcGetProgramLogSize.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_size_t)]
    nvrtc.nvrtcGetProgramLogSize.restype = ctypes.c_int
    nvrtc.nvrtcGetProgramLog.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    nvrtc.nvrtcGetProgramLog.restype = ctypes.c_int
    nvrtc.nvrtcGetPTXSize.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_size_t)]
    nvrtc.nvrtcGetPTXSize.restype = ctypes.c_int
    nvrtc.nvrtcGetPTX.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    nvrtc.nvrtcGetPTX.restype = ctypes.c_int
    nvrtc.nvrtcDestroyProgram.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
    nvrtc.nvrtcDestroyProgram.restype = ctypes.c_int

    src_bytes = cu_path.read_text(encoding="utf-8").encode("utf-8")
    name_bytes = cu_path.name.encode("utf-8")
    prog = ctypes.c_void_p()
    rc = nvrtc.nvrtcCreateProgram(ctypes.byref(prog), src_bytes, name_bytes, 0, None, None)
    if rc != 0:
        msg = f"nvrtcCreateProgram failed with code {rc}"
        raise RuntimeError(msg)

    try:
        opts = [
            f"--gpu-architecture={arch}".encode(),
            b"--generate-line-info",
            b"--std=c++20",
        ]
        opt_arr = (ctypes.c_char_p * len(opts))(*opts)
        compile_rc = nvrtc.nvrtcCompileProgram(prog, len(opts), opt_arr)

        log_size = ctypes.c_size_t(0)
        nvrtc.nvrtcGetProgramLogSize(prog, ctypes.byref(log_size))
        log_text = ""
        if log_size.value > 1:
            log_buf = ctypes.create_string_buffer(log_size.value)
            nvrtc.nvrtcGetProgramLog(prog, log_buf)
            log_text = log_buf.value.decode("utf-8", errors="replace")

        if compile_rc != 0:
            msg = f"NVRTC compilation failed (rc={compile_rc}):\n{log_text}"
            raise RuntimeError(msg)

        ptx_size = ctypes.c_size_t(0)
        rc = nvrtc.nvrtcGetPTXSize(prog, ctypes.byref(ptx_size))
        if rc != 0:
            msg = f"nvrtcGetPTXSize failed with code {rc}"
            raise RuntimeError(msg)
        ptx_buf = ctypes.create_string_buffer(ptx_size.value)
        rc = nvrtc.nvrtcGetPTX(prog, ptx_buf)
        if rc != 0:
            msg = f"nvrtcGetPTX failed with code {rc}"
            raise RuntimeError(msg)
        return ptx_buf.value.decode("utf-8")
    finally:
        nvrtc.nvrtcDestroyProgram(ctypes.byref(prog))


def main() -> None:
    parser = argparse.ArgumentParser(description="Compile CUDA .cu into PTX C++ header")
    parser.add_argument("--input", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--symbol", type=str, default="kColorConvertPtx")
    parser.add_argument("--arch", type=str, default="compute_75")
    args = parser.parse_args()

    if nvcc_path := find_nvcc():
        compiler_used = "nvcc"
        ptx = compile_with_nvcc(args.input, args.arch, nvcc_path=nvcc_path)
    else:
        compiler_used = "nvrtc"
        ptx = compile_with_nvrtc(args.input, args.arch)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    header = (
        "// Auto-generated from "
        f"{args.input.name} via {compiler_used} ({args.arch}, lineinfo enabled)\n"
        "#pragma once\n\n"
        "#include <string_view>\n\n"
        "namespace stem::interop {\n"
        f'inline constexpr std::string_view {args.symbol} = R"PTX(\n'
        f"{ptx}\n"
        ')PTX";\n'
        f'inline constexpr std::string_view {args.symbol}Compiler = "{compiler_used}";\n'
        "}  // namespace stem::interop\n"
    )
    args.output.write_text(header, encoding="utf-8")


if __name__ == "__main__":
    main()
