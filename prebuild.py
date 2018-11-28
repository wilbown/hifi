#!python

import os
import sys

import hifi_singleton
import hifi_utils
import hifi_android
import hifi_vcpkg

import argparse
import concurrent
import hashlib
import importlib
import json
import os
import platform
import shutil
import ssl
import sys
import re
import tempfile
import time


def parse_args():
    # our custom ports, relative to the script location
    defaultPortsPath = hifi_utils.scriptRelative('cmake', 'ports')
    from argparse import ArgumentParser
    parser = ArgumentParser(description='Prepare build dependencies.')
    parser.add_argument('--android', type=str)
    parser.add_argument('--debug', action='store_true')
    parser.add_argument('--force-bootstrap', action='store_true')
    parser.add_argument('--force-build', action='store_true')
    parser.add_argument('--vcpkg-root', type=str, help='The location of the vcpkg distribution')
    parser.add_argument('--build-root', required=True, type=str, help='The location of the cmake build')
    parser.add_argument('--ports-path', type=str, default=defaultPortsPath)
    if True:
        args = parser.parse_args()
    else:
        args = parser.parse_args(['--android', 'questInterface', '--build-root', 'C:/git/hifi/android/apps/questInterface/.externalNativeBuild/cmake/release/arm64-v8a'])
    return args

def main():
    # Fixup env variables.  Leaving `USE_CCACHE` on will cause scribe to fail to build
    # VCPKG_ROOT seems to cause confusion on Windows systems that previously used it for 
    # building OpenSSL
    removeEnvVars = ['VCPKG_ROOT', 'USE_CCACHE']
    for var in removeEnvVars:
        if var in os.environ:
            del os.environ[var]

    args = parse_args()
    # Only allow one instance of the program to run at a time
    pm = hifi_vcpkg.VcpkgRepo(args)
    with hifi_singleton.Singleton(pm.lockFile) as lock:
        if not pm.upToDate():
            pm.bootstrap()
            pm.setupDependencies()

        if args.android:
            # Find the target location
            appPath = hifi_utils.scriptRelative('android/apps/' + args.android)
            # Copy the non-Qt libraries specified in the config in hifi_android.py
            hifi_android.copyAndroidLibs(pm.androidPackagePath, appPath)
            # Determine the Qt package path
            qtPath = os.path.join(pm.androidPackagePath, 'qt')
            hifi_android.QtPackager(appPath, qtPath).bundle()

        pm.writeTag()
        pm.writeConfig()

print(sys.argv)
main()
