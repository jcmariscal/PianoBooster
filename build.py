#!/usr/bin/env python3
"""Convenience build wrapper for PianoBooster."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent


def add_on_off_arg(
    parser: argparse.ArgumentParser,
    *,
    dest: str,
    positive: str,
    negative: str,
    help_positive: str,
    help_negative: str,
) -> None:
    group = parser.add_mutually_exclusive_group()
    group.add_argument(positive, dest=dest, action="store_true", default=None, help=help_positive)
    group.add_argument(negative, dest=dest, action="store_false", help=help_negative)


def cmake_bool(value: bool) -> str:
    return "ON" if value else "OFF"


def append_bool_option(cmake_args: list[str], name: str, value: bool | None) -> None:
    if value is not None:
        cmake_args.append(f"-D{name}={cmake_bool(value)}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Configure and build PianoBooster with CMake.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "Examples:\n"
            "  ./build.py\n"
            "  ./build.py --qt Qt6 --build-type Debug\n"
            "  ./build.py --fresh --use-bundled-rtmidi --without-ftgl\n"
            "  ./build.py --target install-translations\n"
            "  ./build.py -- -DCMAKE_PREFIX_PATH=/opt/Qt/6.6.0/gcc_64\n"
        ),
    )

    parser.add_argument("--build-dir", default="build", help="CMake build directory (default: build).")
    parser.add_argument("--qt", choices=("auto", "Qt6", "Qt5"), default="auto", help="Qt package to use.")
    parser.add_argument("--build-type", choices=("Debug", "Release", "RelWithDebInfo", "MinSizeRel"))
    parser.add_argument("--generator", help="CMake generator, for example Ninja.")
    parser.add_argument("--cmake", default=os.environ.get("CMAKE", "cmake"), help="CMake executable.")
    parser.add_argument("--jobs", "-j", type=int, default=os.cpu_count() or 1, help="Parallel build jobs.")
    parser.add_argument("--target", action="append", help="Build target. Can be passed more than once.")
    parser.add_argument("--prefix", help="CMAKE_INSTALL_PREFIX.")
    parser.add_argument("--data-dir", help="PianoBooster DATA_DIR install path.")
    parser.add_argument("--fresh", action="store_true", help="Delete the build directory before configuring.")
    parser.add_argument("--build-only", action="store_true", help="Skip configure and only build.")
    parser.add_argument("--configure-only", action="store_true", help="Configure but do not build.")
    parser.add_argument("--install", action="store_true", help="Run cmake --install after building.")

    add_on_off_arg(
        parser,
        dest="with_internal_fluidsynth",
        positive="--with-internal-fluidsynth",
        negative="--without-internal-fluidsynth",
        help_positive="Build with internal FluidSynth.",
        help_negative="Build without internal FluidSynth.",
    )
    add_on_off_arg(
        parser,
        dest="use_ftgl",
        positive="--with-ftgl",
        negative="--without-ftgl",
        help_positive="Build with FTGL note-name rendering.",
        help_negative="Build without FTGL.",
    )
    add_on_off_arg(
        parser,
        dest="use_bundled_rtmidi",
        positive="--use-bundled-rtmidi",
        negative="--system-rtmidi",
        help_positive="Build with bundled RtMidi.",
        help_negative="Build with system RtMidi.",
    )

    parser.add_argument("--use-system-font", action="store_true", help="Set USE_SYSTEM_FONT=ON.")
    parser.add_argument("--with-jack", action="store_true", help="Set USE_JACK=ON.")
    parser.add_argument("--no-langs", action="store_true", help="Set NO_LANGS=ON.")
    parser.add_argument("--no-docs", action="store_true", help="Set NO_DOCS=ON.")
    parser.add_argument("--no-license", action="store_true", help="Set NO_LICENSE=ON.")
    parser.add_argument("--no-changelog", action="store_true", help="Set NO_CHANGELOG=ON.")
    parser.add_argument("--with-man", action="store_true", help="Set WITH_MAN=ON.")
    parser.add_argument("--enable-warnings", action="store_true", help="Set ENABLE_WARNINGS=ON.")
    parser.add_argument("--werror", action="store_true", help="Set TREAT_WARNINGS_AS_ERRORS=ON.")
    parser.add_argument("cmake_args", nargs=argparse.REMAINDER, help="Extra CMake args after '--'.")

    return parser


def normalize_remainder(args: list[str]) -> list[str]:
    if args and args[0] == "--":
        return args[1:]
    return args


def common_cmake_args(options: argparse.Namespace) -> list[str]:
    args: list[str] = []

    if options.build_type:
        args.append(f"-DCMAKE_BUILD_TYPE={options.build_type}")
    if options.prefix:
        args.append(f"-DCMAKE_INSTALL_PREFIX={options.prefix}")
    if options.data_dir:
        args.append(f"-DDATA_DIR={options.data_dir}")

    append_bool_option(args, "WITH_INTERNAL_FLUIDSYNTH", options.with_internal_fluidsynth)
    append_bool_option(args, "USE_FTGL", options.use_ftgl)
    append_bool_option(args, "USE_BUNDLED_RTMIDI", options.use_bundled_rtmidi)

    for cmake_name, enabled in (
        ("USE_SYSTEM_FONT", options.use_system_font),
        ("USE_JACK", options.with_jack),
        ("NO_LANGS", options.no_langs),
        ("NO_DOCS", options.no_docs),
        ("NO_LICENSE", options.no_license),
        ("NO_CHANGELOG", options.no_changelog),
        ("WITH_MAN", options.with_man),
        ("ENABLE_WARNINGS", options.enable_warnings),
        ("TREAT_WARNINGS_AS_ERRORS", options.werror),
    ):
        if enabled:
            args.append(f"-D{cmake_name}=ON")

    args.extend(normalize_remainder(options.cmake_args))
    return args


def run_command(command: list[str], *, cwd: Path = ROOT) -> int:
    print("+ " + " ".join(command), flush=True)
    return subprocess.run(command, cwd=str(cwd), check=False).returncode


def safe_remove_build_dir(build_dir: Path) -> None:
    resolved = build_dir.resolve()
    if resolved in (ROOT, ROOT.parent, Path("/")):
        raise SystemExit(f"Refusing to remove unsafe build directory: {resolved}")
    if resolved.exists():
        shutil.rmtree(resolved)


def configure(options: argparse.Namespace, build_dir: Path) -> int:
    base = [options.cmake, "-S", str(ROOT), "-B", str(build_dir)]
    if options.generator:
        base += ["-G", options.generator]

    cmake_options = common_cmake_args(options)
    qt_packages = ["Qt6", "Qt5"] if options.qt == "auto" else [options.qt]

    for index, qt_package in enumerate(qt_packages):
        command = base + [f"-DQT_PACKAGE_NAME={qt_package}"] + cmake_options
        result = run_command(command)
        if result == 0:
            return 0
        if options.qt == "auto" and index == 0:
            print(f"{qt_package} configure failed; trying {qt_packages[index + 1]}.", file=sys.stderr)

    return result


def build(options: argparse.Namespace, build_dir: Path) -> int:
    command = [options.cmake, "--build", str(build_dir), "--parallel", str(options.jobs)]
    if options.target:
        command += ["--target", *options.target]
    return run_command(command)


def install(options: argparse.Namespace, build_dir: Path) -> int:
    command = [options.cmake, "--install", str(build_dir)]
    if options.build_type:
        command += ["--config", options.build_type]
    return run_command(command)


def main(argv: list[str] | None = None) -> int:
    options = build_parser().parse_args(argv)
    build_dir = Path(options.build_dir)
    if not build_dir.is_absolute():
        build_dir = ROOT / build_dir

    if options.fresh and not options.build_only:
        safe_remove_build_dir(build_dir)

    if not options.build_only:
        result = configure(options, build_dir)
        if result != 0:
            return result

    if options.configure_only:
        return 0

    result = build(options, build_dir)
    if result != 0:
        return result

    if options.install:
        return install(options, build_dir)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
