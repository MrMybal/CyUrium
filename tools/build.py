# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Build the native interface without changing the machine's environment."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent


def newest(directory):
    candidates = [p for p in directory.iterdir() if p.is_dir() and all(x.isdigit() for x in p.name.split('.'))]
    return max(candidates, key=lambda p: tuple(map(int, p.name.split('.'))))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--configuration', choices=('Release', 'Debug'), default='Release')
    parser.add_argument('--run', action='store_true')
    parser.add_argument('--core-only', action='store_true')
    args = parser.parse_args()
    env = dict(os.environ)
    cmake = shutil.which('cmake')
    configure = []
    if sys.platform == 'win32':
        env = {key.upper(): value for key, value in env.items()}
        env['VSLANG'] = '1033'
        base = Path(env.get('PROGRAMFILES', r'C:\Program Files')) / 'Microsoft Visual Studio' / '2022'
        installations = [p for p in base.glob('*') if (p / 'VC/Tools/MSVC').exists()]
        if not installations:
            raise RuntimeError('Visual Studio 2022 C++ Build Tools are required.')
        vs = installations[0]
        toolset = newest(vs / 'VC/Tools/MSVC')
        sdk = Path(env.get('PROGRAMFILES(X86)', r'C:\Program Files (x86)')) / 'Windows Kits/10'
        sdk_version = newest(sdk / 'Lib').name
        tools = toolset / 'bin/Hostx64/x64'
        sdk_bin = sdk / 'bin' / sdk_version / 'x64'
        cmake_tools = vs / 'Common7/IDE/CommonExtensions/Microsoft/CMake'
        cmake = cmake or str(cmake_tools / 'CMake/bin/cmake.exe')
        ninja = shutil.which('ninja') or str(cmake_tools / 'Ninja/ninja.exe')
        env['PATH'] = os.pathsep.join(map(str, (tools, sdk_bin, env.get('PATH', ''))))
        env['INCLUDE'] = os.pathsep.join(map(str, (toolset / 'include', sdk / 'Include' / sdk_version / 'ucrt',
                                                  sdk / 'Include' / sdk_version / 'shared', sdk / 'Include' / sdk_version / 'um',
                                                  sdk / 'Include' / sdk_version / 'winrt')))
        env['LIB'] = os.pathsep.join(map(str, (toolset / 'lib/x64', sdk / 'Lib' / sdk_version / 'ucrt/x64',
                                              sdk / 'Lib' / sdk_version / 'um/x64')))
        configure = ['-G', 'Ninja', '-DCMAKE_MAKE_PROGRAM=' + Path(ninja).as_posix(),
                     '-DCMAKE_C_COMPILER=' + (tools / 'cl.exe').as_posix(),
                     '-DCMAKE_CXX_COMPILER=' + (tools / 'cl.exe').as_posix(),
                     '-DCMAKE_RC_COMPILER=' + (sdk_bin / 'rc.exe').as_posix()]
    if not cmake or not Path(cmake).is_file():
        raise RuntimeError('CMake 3.24+ is required.')
    suffix = '-core' if args.core_only else ''
    build = ROOT / 'build' / (sys.platform + '-' + args.configuration.lower() + suffix)
    command = [cmake, '-S', str(ROOT), '-B', str(build), *configure,
               '-DCMAKE_BUILD_TYPE=' + args.configuration,
               '-DCYURIUM_BUILD_APP=' + ('OFF' if args.core_only else 'ON')]
    subprocess.run(command, env=env, check=True)
    subprocess.run([cmake, '--build', str(build), '--config', args.configuration, '--parallel', '4'], env=env, check=True)
    ctest = Path(cmake).with_name('ctest.exe' if sys.platform == 'win32' else 'ctest')
    subprocess.run([str(ctest), '--test-dir', str(build), '-C', args.configuration, '--output-on-failure'], env=env, check=True)
    executable = build / ('CyUrium.exe' if sys.platform == 'win32' else 'CyUrium.app/Contents/MacOS/CyUrium' if sys.platform == 'darwin' else 'CyUrium')
    if not args.core_only:
        platform_name = 'windows' if sys.platform == 'win32' else 'macos' if sys.platform == 'darwin' else 'linux'
        destination = ROOT / 'dist' / (platform_name + ('-debug' if args.configuration == 'Debug' else ''))
        if sys.platform == 'darwin':
            bundle = build / 'CyUrium.app'
            shutil.copytree(bundle, destination / 'CyUrium.app', dirs_exist_ok=True)
            packaged = destination / 'CyUrium.app/Contents/MacOS/CyUrium'
        else:
            destination /= 'CyUrium'
            destination.mkdir(parents=True, exist_ok=True)
            shutil.copy2(executable, destination / executable.name)
            for folder in ('assets', 'licenses'):
                shutil.copytree(build / folder, destination / folder, dirs_exist_ok=True)
            packaged = destination / executable.name
        notice_destination = destination/'CyUrium.app/Contents/Resources' if sys.platform == 'darwin' else destination
        notice_destination.mkdir(parents=True, exist_ok=True)
        for name in ('LICENSE', 'NOTICE', 'THIRD_PARTY_NOTICES.md'):
            shutil.copy2(ROOT/name, notice_destination/name)
        print('Executable:', packaged, flush=True)
        if sys.platform == 'win32':
            from prepare_gecko import package
            package(packaged.parent / 'engine')
            from prepare_sdk import prepare
            prepare(packaged.parent)
        if args.configuration == 'Release':
            from check_package_paths import check_package_paths
            check_package_paths(destination/'CyUrium.app' if sys.platform == 'darwin' else destination)
        if args.run:
            subprocess.Popen([str(packaged), '--profile', str(ROOT / '.runtime/profiles/default')], cwd=ROOT, env=env)


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print('Build failed:', error, file=sys.stderr)
        sys.exit(1)
