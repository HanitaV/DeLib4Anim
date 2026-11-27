#!/usr/bin/env python3
"""Download, SHA256-verify, and extract missing toolchains/SDKs under .stem_cache/toolchains/.

Installs in user-space (no UAC elevation required):
  1. Portable MSVC x64 C++20 compiler + Windows 11 SDK (from Microsoft VS17 release manifest)
  2. NVIDIA CUDA 13.5 Toolkit (nvcc, crt, cudart, cccl, libnvvm, compute-sanitizer, nsys, ncu)
  3. Prebuilt Skia Ganesh/OpenGL SDK (JetBrains/skia-pack m144-e2e6623374-4)
"""

from __future__ import annotations

import argparse
import contextlib
import hashlib
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import urllib.parse
import urllib.request
import zipfile

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
CACHE_DEPS = REPO_ROOT / ".stem_cache" / "deps"
TOOLCHAINS_DIR = REPO_ROOT / ".stem_cache" / "toolchains"

VS_CHANNEL_URL = "https://aka.ms/vs/17/release/channel"
CUDA_REDIST_URL = "https://developer.download.nvidia.com/compute/cuda/redist/redistrib_13.5.0.json"
CUDA_BASE_URL = "https://developer.download.nvidia.com/compute/cuda/redist/"

SKIA_URL = (
    "https://github.com/JetBrains/skia-pack/releases/download/"
    "m144-e2e6623374-4/Skia-m144-e2e6623374-4-windows-Release-x64.zip"
)
SKIA_SHA256 = "50869b098a4f72f5843687093a1a8fdd94e5d61d98397024bb4e521227fedcba"

CUDA_PACKAGES = [
    "cuda_nvcc",
    "cuda_crt",
    "cuda_cudart",
    "cccl",
    "libnvvm",
    "libnvptxcompiler",
    "libnvfatbin",
    "cuda_cuobjdump",
    "cuda_nvdisasm",
    "cuda_cupti",
    "cuda_profiler_api",
    "cuda_nvtx",
    "cuda_sanitizer_api",
    "nsight_systems",
    "nsight_compute",
]

WINSDK_MSIS = [
    "Installers/Universal CRT Headers Libraries and Sources-x86_en-us.msi",
    "Installers/Windows SDK Desktop Headers x86-x86_en-us.msi",
    "Installers/Windows SDK Desktop Headers x64-x86_en-us.msi",
    "Installers/Windows SDK OnecoreUap Headers x86-x86_en-us.msi",
    "Installers/Windows SDK OnecoreUap Headers x64-x86_en-us.msi",
    "Installers/Windows SDK for Windows Store Apps Headers-x86_en-us.msi",
    "Installers/Windows SDK for Windows Store Apps Headers OnecoreUap-x86_en-us.msi",
    "Installers/Windows SDK for Windows Store Apps Libs-x86_en-us.msi",
    "Installers/Windows SDK Desktop Libs x64-x86_en-us.msi",
    "Installers/Windows SDK Desktop Tools x64-x86_en-us.msi",
    "Installers/Windows SDK for Windows Store Apps Tools-x86_en-us.msi",
    "Installers/Windows SDK Signing Tools-x86_en-us.msi",
]


def sha256_file(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        while chunk := f.read(1024 * 1024):
            h.update(chunk)
    return h.hexdigest().lower()


def download_verified(url: str, dest: pathlib.Path, expected_sha256: str | None = None) -> pathlib.Path:
    dest.parent.mkdir(parents=True, exist_ok=True)
    expected = expected_sha256.strip().lower() if expected_sha256 else None
    if dest.exists():
        if expected is None or sha256_file(dest) == expected:
            return dest
        dest.unlink()

    tmp = dest.with_suffix(dest.suffix + ".part")
    print(f"[setup] Downloading {dest.name} ...", flush=True)
    req = urllib.request.Request(url, headers={"User-Agent": "stem-engine-setup/0.1"})
    with urllib.request.urlopen(req, timeout=300) as resp, tmp.open("wb") as out:
        while chunk := resp.read(512 * 1024):
            out.write(chunk)

    if expected is not None:
        actual = sha256_file(tmp)
        if actual != expected:
            tmp.unlink(missing_ok=True)
            msg = f"SHA256 mismatch for {url}: expected {expected}, got {actual}"
            raise RuntimeError(msg)

    tmp.replace(dest)
    return dest


def fetch_json(url: str) -> dict:
    req = urllib.request.Request(url, headers={"User-Agent": "stem-engine-setup/0.1"})
    with urllib.request.urlopen(req, timeout=60) as resp:
        return json.load(resp)


def extract_zip_strip_prefix(archive_path: pathlib.Path, dest_dir: pathlib.Path, strip_top: bool = False) -> None:
    dest_dir.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive_path) as zf:
        for info in zf.infolist():
            if info.is_dir():
                continue
            rel = info.filename.replace("\\", "/").lstrip("/")
            if strip_top:
                parts = rel.split("/", 1)
                if len(parts) < 2 or not parts[1]:
                    continue
                rel = parts[1]
            target = dest_dir / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            if target.exists():
                with contextlib.suppress(OSError):
                    os.chmod(target, 0o666)
                if target.stat().st_size == info.file_size:
                    continue
            with zf.open(info) as src, target.open("wb") as dst:
                shutil.copyfileobj(src, dst)


def extract_vsix_contents(vsix_path: pathlib.Path, dest_dir: pathlib.Path) -> None:
    with zipfile.ZipFile(vsix_path) as zf:
        for info in zf.infolist():
            if info.is_dir():
                continue
            raw_name = info.filename.replace("\\", "/")
            if not raw_name.startswith("Contents/"):
                continue
            rel = urllib.parse.unquote(raw_name[len("Contents/") :])
            target = dest_dir / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            if target.exists():
                with contextlib.suppress(OSError):
                    os.chmod(target, 0o666)
                if target.stat().st_size == info.file_size:
                    continue
            with zf.open(info) as src, target.open("wb") as dst:
                shutil.copyfileobj(src, dst)


def pick_vs_package(packages: list[dict], pkg_id: str) -> dict:
    candidates = [p for p in packages if p.get("id") == pkg_id]
    if not candidates:
        msg = f"Package not found in VS manifest: {pkg_id}"
        raise RuntimeError(msg)
    for pref in ("en-us", "neutral", ""):
        for c in candidates:
            if (c.get("language") or "").lower() == pref:
                return c
    return candidates[0]


def _write_msvc_wrappers(meta: dict) -> None:
    msvc_root = pathlib.Path(meta["msvc_root"])
    msvc_dir = pathlib.Path(meta["cl_dir"]).parent.parent.parent
    sdk_ver = meta["sdk_version"]
    path_prefix = ";".join(meta["PATH"])
    include_str = ";".join(meta["INCLUDE"])
    lib_str = ";".join(meta["LIB"])

    ps_lines = [
        "# Auto-generated portable MSVC x64 + Windows SDK activation script",
        f'$env:PATH = "{path_prefix};" + $env:PATH',
        f'$env:INCLUDE = "{include_str}"',
        f'$env:LIB = "{lib_str}"',
        '$env:VSCMD_VER = "17.14"',
        f'$env:VCINSTALLDIR = "{msvc_root}\\VC\\"',
        f'$env:WindowsSdkDir = "{msvc_root}\\Windows Kits\\10\\"',
        f'$env:UniversalCRTSdkDir = "{msvc_root}\\Windows Kits\\10\\"',
        f'$env:UCRTVersion = "{sdk_ver}"',
    ]
    (msvc_root / "devcmd.ps1").write_text("\n".join(ps_lines) + "\n", encoding="utf-8")

    bat_content = "\r\n".join(
        [
            "@echo off",
            f'set "VSINSTALLDIR={msvc_root}\\"',
            f'set "VCINSTALLDIR={msvc_root}\\VC\\"',
            f'set "VCToolsInstallDir={msvc_dir}\\"',
            f'set "WindowsSdkDir={msvc_root}\\Windows Kits\\10\\"',
            f'set "WindowsSdkVersion={sdk_ver}\\"',
            f'set "WindowsSdkLibVersion={sdk_ver}\\"',
            f'set "UniversalCRTSdkDir={msvc_root}\\Windows Kits\\10\\"',
            f'set "UCRTVersion={sdk_ver}"',
            'set "VSCMD_VER=17.14"',
            'set "VSCMD_ARG_TGT_ARCH=x64"',
            'set "Platform=x64"',
            f'set "PATH={path_prefix};%PATH%"',
            f'set "INCLUDE={include_str}"',
            f'set "LIB={lib_str}"',
            "exit /b 0",
            "",
        ]
    )
    aux_dir = msvc_root / "VC" / "Auxiliary" / "Build"
    aux_dir.mkdir(parents=True, exist_ok=True)
    for name in ("vcvars64.bat", "vcvars32.bat", "vcvarsall.bat"):
        (aux_dir / name).write_text(bat_content, encoding="utf-8")
    tools_dir = msvc_root / "Common7" / "Tools"
    tools_dir.mkdir(parents=True, exist_ok=True)
    (tools_dir / "VsDevCmd.bat").write_text(bat_content, encoding="utf-8")


def setup_msvc(force: bool = False) -> dict:
    msvc_root = TOOLCHAINS_DIR / "msvc"
    env_json = msvc_root / "msvc_env.json"
    if env_json.exists() and not force:
        data = json.loads(env_json.read_text(encoding="utf-8"))
        kernel32 = pathlib.Path(data["LIB"][2]) / "kernel32.lib"
        winapifamily = pathlib.Path(data["INCLUDE"][2]) / "winapifamily.h"
        if pathlib.Path(data["cl_exe"]).exists() and kernel32.exists() and winapifamily.exists():
            _write_msvc_wrappers(data)
            print(f"[setup] MSVC already ready at {msvc_root} (VC {data['msvc_version']}, SDK {data['sdk_version']})")
            return data

    print("[setup] Fetching Visual Studio 17 release channel manifest...", flush=True)
    channel = fetch_json(VS_CHANNEL_URL)
    vs_man_payload = next(
        i["payloads"][0]
        for i in channel.get("channelItems", [])
        if i.get("id") == "Microsoft.VisualStudio.Manifests.VisualStudio"
    )
    vs_man_path = download_verified(
        vs_man_payload["url"],
        CACHE_DEPS / "vs17" / "VisualStudio.vsman",
        None,
    )
    manifest = json.loads(vs_man_path.read_text(encoding="utf-8"))
    packages = manifest.get("packages", [])

    # Select latest standard HostX64.TargetX64 VC toolset
    vc_pattern = re.compile(r"^Microsoft\.VC\.(\d+\.\d+\.\d+\.\d+)\.Tools\.HostX64\.TargetX64\.base$")
    vc_versions: list[tuple[tuple[int, ...], str]] = []
    for p in packages:
        pid = p.get("id", "")
        m = vc_pattern.match(pid)
        if m:
            ver_str = m.group(1)
            vc_versions.append((tuple(int(x) for x in ver_str.split(".")), ver_str))
    if not vc_versions:
        raise RuntimeError("No VC HostX64.TargetX64 package found in VS manifest")
    vc_ver = sorted(vc_versions)[-1][1]

    vc_pkg_ids = [
        f"Microsoft.VC.{vc_ver}.Tools.HostX64.TargetX64.base",
        f"Microsoft.VC.{vc_ver}.Tools.HostX64.TargetX64.Res.base",
        f"Microsoft.VC.{vc_ver}.CRT.Headers.base",
        f"Microsoft.VC.{vc_ver}.CRT.x64.Desktop.base",
        f"Microsoft.VC.{vc_ver}.CRT.x64.Store.base",
    ]

    msvc_root.mkdir(parents=True, exist_ok=True)
    vc_cache = CACHE_DEPS / "vs17" / f"vc-{vc_ver}"
    for pid in vc_pkg_ids:
        pkg = pick_vs_package(packages, pid)
        for pl in pkg.get("payloads", []):
            fname = pl["fileName"]
            vsix = download_verified(pl["url"], vc_cache / fname, pl.get("sha256"))
            extract_vsix_contents(vsix, msvc_root)

    # Select latest Windows 11 SDK (or Windows 10 SDK)
    sdk_comp_ids = sorted(
        {
            p["id"]
            for p in packages
            if p.get("id", "").startswith("Microsoft.VisualStudio.Component.Windows11SDK.")
            and not p["id"].endswith(".Resources")
        }
    )
    if not sdk_comp_ids:
        raise RuntimeError("No Windows 11 SDK component found in VS manifest")
    sdk_comp = pick_vs_package(packages, sdk_comp_ids[-1])
    sdk_pkg_id = next(iter(sdk_comp["dependencies"].keys()))
    sdk_pkg = pick_vs_package(packages, sdk_pkg_id)

    payload_by_name = {pl["fileName"]: pl for pl in sdk_pkg.get("payloads", [])}
    sdk_cache = CACHE_DEPS / "vs17" / sdk_pkg_id
    msi_paths: list[pathlib.Path] = []
    needed_cabs: set[str] = set()

    all_cab_names = [name for name in payload_by_name if name.lower().endswith(".cab")]
    for msi_name in WINSDK_MSIS:
        pl = payload_by_name.get(msi_name)
        if pl is None:
            print(f"[setup] Warning: SDK MSI missing from manifest: {msi_name}", file=sys.stderr)
            continue
        msi_path = download_verified(pl["url"], sdk_cache / msi_name, pl.get("sha256"))
        msi_paths.append(msi_path)
        msi_bytes = msi_path.read_bytes()
        for cab_name in all_cab_names:
            cab_base = pathlib.Path(cab_name).name.encode("ascii")
            if cab_base in msi_bytes:
                needed_cabs.add(cab_name)

    for cab_name in sorted(needed_cabs):
        pl = payload_by_name[cab_name]
        download_verified(pl["url"], sdk_cache / cab_name, pl.get("sha256"))

    target_dir = str(msvc_root.resolve())
    for msi_path in msi_paths:
        print(f"[setup] Unpacking {msi_path.name} ...", flush=True)
        res = subprocess.run(
            ["msiexec.exe", "/a", str(msi_path.resolve()), "/qn", f"TARGETDIR={target_dir}"],
            capture_output=True,
            text=True,
        )
        if res.returncode != 0:
            msg = f"msiexec failed ({res.returncode}) on {msi_path.name}: {res.stderr}"
            raise RuntimeError(msg)

    # Locate installed versions
    vc_tools_dirs = sorted((msvc_root / "VC" / "Tools" / "MSVC").iterdir())
    if not vc_tools_dirs:
        raise RuntimeError("MSVC tools directory missing after extraction")
    msvc_dir = vc_tools_dirs[-1]
    actual_vc_ver = msvc_dir.name

    sdk_inc_dirs = sorted((msvc_root / "Windows Kits" / "10" / "Include").iterdir())
    if not sdk_inc_dirs:
        raise RuntimeError("Windows SDK Include directory missing after msiexec extraction")
    sdk_ver = sdk_inc_dirs[-1].name

    cl_dir = msvc_dir / "bin" / "Hostx64" / "x64"
    sdk_bin_dir = msvc_root / "Windows Kits" / "10" / "bin" / sdk_ver / "x64"
    if not sdk_bin_dir.is_dir():
        sdk_bin_dir = msvc_root / "Windows Kits" / "10" / "bin" / "x64"

    include_dirs = [
        str(msvc_dir / "include"),
        str(msvc_root / "Windows Kits" / "10" / "Include" / sdk_ver / "ucrt"),
        str(msvc_root / "Windows Kits" / "10" / "Include" / sdk_ver / "shared"),
        str(msvc_root / "Windows Kits" / "10" / "Include" / sdk_ver / "um"),
        str(msvc_root / "Windows Kits" / "10" / "Include" / sdk_ver / "winrt"),
        str(msvc_root / "Windows Kits" / "10" / "Include" / sdk_ver / "cppwinrt"),
    ]
    lib_dirs = [
        str(msvc_dir / "lib" / "x64"),
        str(msvc_root / "Windows Kits" / "10" / "Lib" / sdk_ver / "ucrt" / "x64"),
        str(msvc_root / "Windows Kits" / "10" / "Lib" / sdk_ver / "um" / "x64"),
    ]
    path_dirs = [str(cl_dir), str(sdk_bin_dir)]

    cl_exe = cl_dir / "cl.exe"
    if not cl_exe.exists():
        raise RuntimeError(f"cl.exe not found at {cl_exe}")

    meta = {
        "msvc_version": actual_vc_ver,
        "sdk_version": sdk_ver,
        "msvc_root": str(msvc_root.resolve()),
        "cl_dir": str(cl_dir.resolve()),
        "cl_exe": str(cl_exe.resolve()),
        "sdk_bin_dir": str(sdk_bin_dir.resolve()),
        "PATH": path_dirs,
        "INCLUDE": include_dirs,
        "LIB": lib_dirs,
    }
    env_json.write_text(json.dumps(meta, indent=2), encoding="utf-8")
    _write_msvc_wrappers(meta)
    print(f"[setup] MSVC ready: VC {actual_vc_ver}, SDK {sdk_ver}", flush=True)
    return meta


def setup_cuda(force: bool = False) -> pathlib.Path:
    cuda_root = TOOLCHAINS_DIR / "cuda"
    nvcc_exe = cuda_root / "bin" / "nvcc.exe"
    sanitizer_exe = cuda_root / "compute-sanitizer" / "compute-sanitizer.exe"
    nsys_matches = list(cuda_root.glob("nsight-systems/*/target-windows-x64/nsys.exe"))
    ncu_matches = list(cuda_root.glob("nsight-compute/*/target/windows-desktop-win7-x64/ncu.exe"))

    if not force and nvcc_exe.exists() and sanitizer_exe.exists() and nsys_matches and ncu_matches:
        print(f"[setup] CUDA 13.5 toolkit already ready at {cuda_root}")
        return cuda_root

    print("[setup] Fetching CUDA 13.5.0 redistributable manifest...", flush=True)
    manifest = fetch_json(CUDA_REDIST_URL)
    cuda_cache = CACHE_DEPS / "cuda-13.5.0"

    for pkg_name in CUDA_PACKAGES:
        entry = manifest.get(pkg_name, {}).get("windows-x86_64")
        if not entry:
            raise RuntimeError(f"CUDA package missing windows-x86_64 entry: {pkg_name}")
        rel_path = entry["relative_path"]
        url = urllib.parse.urljoin(CUDA_BASE_URL, rel_path)
        zip_name = pathlib.Path(rel_path).name
        archive = download_verified(url, cuda_cache / zip_name, entry.get("sha256"))
        print(f"[setup] Extracting {zip_name} ...", flush=True)
        extract_zip_strip_prefix(archive, cuda_root, strip_top=True)

    # Ensure nvcc.profile includes nvvm/bin and include/cccl cleanly
    profile_path = cuda_root / "bin" / "nvcc.profile"
    if profile_path.exists():
        profile_text = profile_path.read_text(encoding="utf-8")
        additions: list[str] = []
        if "include/cccl" not in profile_text and (cuda_root / "include" / "cccl").is_dir():
            additions.append('INCLUDES        += "-I$(TOP)/include/cccl" $(_SPACE_)')
        if "nvvm/bin" not in profile_text and (cuda_root / "nvvm" / "bin").is_dir():
            additions.append("PATH            += $(TOP)/nvvm/bin;$(_SPACE_)")
        if "NVVMIR_LIBRARY_DIR" not in profile_text and (cuda_root / "nvvm" / "libdevice").is_dir():
            additions.append("NVVMIR_LIBRARY_DIR = $(TOP)/nvvm/libdevice")
        if additions:
            profile_path.write_text(profile_text.rstrip() + "\n" + "\n".join(additions) + "\n", encoding="utf-8")

    if not nvcc_exe.exists():
        raise RuntimeError(f"nvcc.exe missing after extraction: {nvcc_exe}")
    print(f"[setup] CUDA 13.5 toolkit ready at {cuda_root}", flush=True)
    return cuda_root


def setup_skia(force: bool = False) -> pathlib.Path:
    skia_root = TOOLCHAINS_DIR / "skia"
    skia_lib = skia_root / "out" / "Release-windows-x64" / "skia.lib"
    if skia_lib.exists() and not force:
        print(f"[setup] Skia m144 already ready at {skia_root}")
        return skia_root

    archive = download_verified(
        SKIA_URL,
        CACHE_DEPS / "Skia-m144-e2e6623374-4-windows-Release-x64.zip",
        SKIA_SHA256,
    )
    print(f"[setup] Extracting {archive.name} ...", flush=True)
    extract_zip_strip_prefix(archive, skia_root, strip_top=False)
    if not skia_lib.exists():
        raise RuntimeError(f"skia.lib missing after extraction: {skia_lib}")
    print(f"[setup] Skia m144 ready at {skia_root}", flush=True)
    return skia_root


def main() -> None:
    parser = argparse.ArgumentParser(description="Setup missing MSVC, CUDA 13.5, and Skia toolchains")
    parser.add_argument("--only", choices=["all", "msvc", "cuda", "skia"], default="all")
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()

    if args.only in ("all", "msvc"):
        setup_msvc(force=args.force)
    if args.only in ("all", "cuda"):
        setup_cuda(force=args.force)
    if args.only in ("all", "skia"):
        setup_skia(force=args.force)


if __name__ == "__main__":
    main()
