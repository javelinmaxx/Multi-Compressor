#!/usr/bin/env python3
"""Build and run the deterministic research tests; no plugin installation or CI dispatch."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
from academic_names import internal_project, public_project, PUBLIC_PROJECTS

ROOT = Path(__file__).resolve().parents[1]
PROJECTS = {"KuoStereo": "KUO_STEREO_BUILD_TESTS", "ZaoDrive": "ZAO_DRIVE_BUILD_TESTS", "XuanPhase": "XUAN_PHASE_BUILD_TESTS"}


def run(command):
    print("+", " ".join(map(str, command)), flush=True)
    subprocess.run(list(map(str, command)), check=True, cwd=ROOT)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-root", type=Path, default=ROOT / "build/academic")
    parser.add_argument("--output", type=Path, default=ROOT / "build/academic-results")
    parser.add_argument("--build-dir", action="append", default=[], metavar="PROJECT=PATH", help="Reuse a configured project build directory; repeat for multiple projects")
    parser.add_argument("--only", nargs="+", type=public_project, choices=PUBLIC_PROJECTS, default=PUBLIC_PROJECTS)
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--jobs", type=int, default=3)
    args = parser.parse_args()
    builds = {}
    for entry in args.build_dir:
        project, path = entry.split("=", 1)
        project = internal_project(project)
        if project not in PROJECTS:
            parser.error(f"Unknown project: {project}")
        builds[project] = Path(path).resolve()
    output = args.output.resolve()
    for selected in args.only:
        project = internal_project(selected)
        build = builds.get(project, args.build_root.resolve()/project)
        if not args.skip_build:
            run(["cmake", "-S", ROOT/"projects"/project, "-B", build, "-DCMAKE_BUILD_TYPE=Release", f"-D{PROJECTS[project]}=ON"] if project not in builds else ["cmake", "-S", ROOT/"projects"/project, "-B", build, f"-D{PROJECTS[project]}=ON"])
            run(["cmake", "--build", build, "--config", "Release", "--target", project+"AblationTests", "--parallel", args.jobs])
        executable = build/(project+"AblationTests"+(".exe" if os.name == "nt" else ""))
        if not executable.exists():
            executable = build/"Release"/executable.name
        destination = output/project
        destination.mkdir(parents=True, exist_ok=True)
        sources = [ROOT/"projects/academic_names.py", ROOT/"projects"/project/"CMakeLists.txt", ROOT/"projects/AcademicAblations.h", ROOT/"projects/AcademicTestUtils.h", ROOT/"projects/AcademicTests.cmake", Path(__file__).resolve(), ROOT/"projects/generate_academic_reports.py", ROOT/"projects/generate_audio_spectrograms.py", ROOT/"projects/generate_apricot_fir_report.py", ROOT/"projects/test_apricot_fir_report.py", ROOT/"projects/test_audio_spectrograms.py", ROOT/"projects/KuoStereo/Measurements/SynthPad.h"]
        sources += sorted((ROOT/"projects"/project/"Source").glob("*"))
        sources += [ROOT/"projects"/project/"Tests/AblationTests.cpp"]
        # Include transitive shared DSP headers as well as the direct project.
        sources += sorted((ROOT/"projects/YinyangCompressor/Source").glob("*.h"))
        sources += sorted((ROOT/"projects/KuoStereo/Source").glob("*.h"))
        cache = build/"CMakeCache.txt"
        cache_values = dict(line.split("=", 1) for line in cache.read_text().splitlines() if "=" in line and not line.startswith(("#", "//"))) if cache.exists() else {}
        compiler = cache_values.get("CMAKE_CXX_COMPILER:FILEPATH", "c++")
        metadata = {
            "source_revision": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
            "source_dirty": bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT, text=True).strip()),
            "platform": platform.platform(), "python": sys.version.split()[0],
            "cmake": subprocess.check_output(["cmake", "--version"], text=True).splitlines()[0],
            "compiler": subprocess.check_output([compiler, "--version"], text=True).splitlines()[0] if os.name != "nt" else compiler,
            "compile_flags": {key: value for key, value in cache_values.items() if key.startswith("CMAKE_CXX_FLAGS")},
            "source_sha256": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources if p.is_file()},
            "cmake_cache_sha256": hashlib.sha256(cache.read_bytes()).hexdigest() if cache.exists() else None,
            "build_config": next((line.split("=", 1)[1] for line in cache.read_text().splitlines() if line.startswith("CMAKE_BUILD_TYPE:")), "multi-config") if cache.exists() else "unknown",
        }
        (destination/"provenance.json").write_text(json.dumps(metadata, indent=2)+"\n")
        run([executable, destination])
    print(f"Results: {output}")
    print(f"Generate reports: python3 projects/generate_academic_reports.py {output}")


if __name__ == "__main__":
    main()
