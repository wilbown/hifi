#!python

import argparse
import concurrent
import hashlib
import importlib
import json
import os
import platform
import shutil
import ssl
import subprocess
import sys
import tarfile
import tempfile
import time
import urllib.request
import functools
try:
    import fcntl
except ImportError:
    fcntl = None

print = functools.partial(print, flush=True)

def executeSubprocess(processArgs, folder=None, env=None):
    restoreDir = None
    if folder != None:
        restoreDir = os.getcwd()
        os.chdir(folder)

    process = subprocess.Popen(
        processArgs, stdout=sys.stdout, stderr=sys.stderr, env=env)
    process.wait()

    if (0 != process.returncode):
        raise RuntimeError('Call to "{}" failed.\n\narguments:\n{}\n'.format(
            processArgs[0],
            ' '.join(processArgs[1:]),
            ))

    if restoreDir != None:
        os.chdir(restoreDir)


def hashFile(file):
    hasher = hashlib.sha512()
    with open(file, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hasher.update(chunk)
    return hasher.hexdigest()

# Assumes input files are in deterministic order
def hashFiles(filenames):
    hasher = hashlib.sha256()
    for filename in filenames:
        with open(filename, "rb") as f:
            for chunk in iter(lambda: f.read(4096), b""):
                hasher.update(chunk)
    return hasher.hexdigest()

def hashFolder(folder):
    filenames = []
    for dirName, subdirList, fileList in os.walk(folder):
        for fname in fileList:
            filenames.append(os.path.join(folder, dirName, fname))
    # Make sure we hash the files in a determinisitic order, 
    # regardless of how the os.walk function ordered them
    filenames.sort()
    return hashFiles(filenames)

def downloadAndExtract(url, destPath, hash=None):
    tempFileDescriptor, tempFileName = tempfile.mkstemp()
    # OSX Python doesn't support SSL, so we need to bypass it.  
    # However, we still validate the downloaded file's sha512 hash
    context = ssl._create_unverified_context()
    with urllib.request.urlopen(url, context=context) as response, open(tempFileDescriptor, 'wb') as tempFile:
        shutil.copyfileobj(response, tempFile)

    # Verify the hash
    if hash and hash != hashFile(tempFileName):
        raise RuntimeError("Downloaded file does not match hash")
        
    # Extract the archive
    with tarfile.open(tempFileName, 'r:gz') as tgz:
        tgz.extractall(destPath)
    os.remove(tempFileName)

           
class Singleton:
    def __init__(self, path):
        self.fh = None
        self.windows = 'Windows' == platform.system()
        self.path = path

    def __enter__(self):
        success = False
        while not success:
            try:
                if self.windows:
                    if os.path.exists(self.path):
                        os.unlink(self.path)
                    self.fh = os.open(self.path, os.O_CREAT | os.O_EXCL | os.O_RDWR)
                else:
                    self.fh = open(self.path, 'x')
                    fcntl.lockf(self.fh, fcntl.LOCK_EX | fcntl.LOCK_NB)
                success = True
            except EnvironmentError as err:
                if self.fh is not None:
                    if self.windows:
                        os.close(self.fh)
                    else:
                        self.fh.close()
                    self.fh = None
                print("Couldn't aquire lock, retrying in 10 seconds")
                time.sleep(10)
        return self

    def __exit__(self, type, value, traceback):
        if self.windows:
            os.close(self.fh)
        else:
            fcntl.lockf(self.fh, fcntl.LOCK_UN)
            self.fh.close()
        os.unlink(self.path)

class VcpkgRepo:
    def __init__(self):
        global args
        scriptPath = os.path.dirname(os.path.realpath(sys.argv[0]))
        # our custom ports, relative to the script location
        self.sourcePortsPath = os.path.join(scriptPath, 'cmake', 'ports')
        # FIXME Revert to ports hash before release
        self.id = hashFolder(self.sourcePortsPath)[:8]
        # OS dependent information
        system = platform.system()

        if args.vcpkg_root is not None:
            print("override vcpkg path with " + args.vcpkg_root)
            self.path = args.vcpkg_root
        else:
            if 'Darwin' == system:
                defaultBasePath = os.path.expanduser('~/hifi/vcpkg')
            else:
                defaultBasePath = os.path.join(tempfile.gettempdir(), 'hifi', 'vcpkg')
            basePath = os.getenv('HIFI_VCPKG_BASE', defaultBasePath)
            if basePath == defaultBasePath:
                print("Warning: Environment variable HIFI_VCPKG_BASE not set, using {}".format(defaultBasePath))
            if (not os.path.isdir(basePath)):
                os.makedirs(basePath)
            self.path = os.path.join(basePath, self.id)

        lockDir, lockName = os.path.split(self.path)
        lockName += '.lock'
        if not os.path.isdir(lockDir):
            os.makedirs(lockDir)

        self.lockFile = os.path.join(lockDir, lockName)
        self.tagFile = os.path.join(self.path, '.id')
        # A format version attached to the tag file... increment when you want to force the build systems to rebuild 
        # without the contents of the ports changing
        self.version = 1
        self.tagContents = "{}_{}".format(self.id, self.version)

        print("prebuild path: " + self.path)
        if 'Windows' == system:
            self.exe = os.path.join(self.path, 'vcpkg.exe')
            self.vcpkgUrl = 'https://hifi-public.s3.amazonaws.com/dependencies/vcpkg/vcpkg-win32.tar.gz?versionId=YZYkDejDRk7L_hrK_WVFthWvisAhbDzZ'
            self.vcpkgHash = '3e0ff829a74956491d57666109b3e6b5ce4ed0735c24093884317102387b2cb1b2cd1ff38af9ed9173501f6e32ffa05cc6fe6d470b77a71ca1ffc3e0aa46ab9e'
            self.hostTriplet = 'x64-windows'
        elif 'Darwin' == system:
            self.exe = os.path.join(self.path, 'vcpkg')
            self.vcpkgUrl = 'https://hifi-public.s3.amazonaws.com/dependencies/vcpkg/vcpkg-osx.tar.gz?versionId=_fhqSxjfrtDJBvEsQ8L_ODcdUjlpX9cc'
            self.vcpkgHash = '519d666d02ef22b87c793f016ca412e70f92e1d55953c8f9bd4ee40f6d9f78c1df01a6ee293907718f3bbf24075cc35492fb216326dfc50712a95858e9cbcb4d'
            self.hostTriplet = 'x64-osx'
        else:
            self.exe = os.path.join(self.path, 'vcpkg')
            self.vcpkgUrl = 'https://hifi-public.s3.amazonaws.com/dependencies/vcpkg/vcpkg-linux.tar.gz?versionId=97Nazh24etEVKWz33XwgLY0bvxEfZgMU'
            self.vcpkgHash = '6a1ce47ef6621e699a4627e8821ad32528c82fce62a6939d35b205da2d299aaa405b5f392df4a9e5343dd6a296516e341105fbb2dd8b48864781d129d7fba10d'
            self.hostTriplet = 'x64-linux'

        if args.android:
            self.triplet = 'arm64-android'
        else:
            self.triplet = self.hostTriplet

    def outOfDate(self):
        global args
        # Prevent doing a clean if we've explcitly set a directory for vcpkg
        if args.vcpkg_root is not None:
            return False
        if args.force_build:
            return True
        print("Looking for tag file {}".format(self.tagFile))
        if not os.path.isfile(self.tagFile):
            return True
        with open(self.tagFile, 'r') as f:
            storedTag = f.read()
        print("Found stored tag {}".format(storedTag))
        if storedTag != self.tagContents:
            print("Doesn't match computed tag {}".format(self.tagContents))
            return True
        return False

    def clean(self):
        cleanPath = self.path
        print("Cleaning vcpkg installation at {}".format(cleanPath))
        if os.path.isdir(self.path):
            print("Removing {}".format(cleanPath))
            shutil.rmtree(cleanPath, ignore_errors=True)

    def bootstrap(self):
        global args
        if self.outOfDate():
            self.clean()

        # don't download the vcpkg binaries if we're working with an explicit 
        # vcpkg directory (possibly a git checkout)
        if args.vcpkg_root is None:
            downloadVcpkg = False
            if args.force_bootstrap:
                print("Forcing bootstrap")
                downloadVcpkg = True

            if not downloadVcpkg and not os.path.isfile(self.exe):
                print("Missing executable, boostrapping")
                downloadVcpkg = True
            
            # Make sure we have a vcpkg executable
            testFile = os.path.join(self.path, '.vcpkg-root')
            if not downloadVcpkg and not os.path.isfile(testFile):
                print("Missing {}, bootstrapping".format(testFile))
                downloadVcpkg = True

            if downloadVcpkg:
                print("Fetching vcpkg from {} to {}".format(self.vcpkgUrl, self.path))
                downloadAndExtract(self.vcpkgUrl, self.path, self.vcpkgHash)

        print("Replacing port files")
        portsPath = os.path.join(self.path, 'ports')
        if (os.path.islink(portsPath)):
            os.unlink(portsPath)
        if (os.path.isdir(portsPath)):
            shutil.rmtree(portsPath, ignore_errors=True)
        shutil.copytree(self.sourcePortsPath, portsPath)

    def downloadAndroidDependencies(self):
        url = "https://hifi-public.s3.amazonaws.com/dependencies/vcpkg/vcpkg-arm64-android.tar.gz"
        hash = "832f82a4d090046bdec25d313e20f56ead45b54dd06eee3798c5c8cbdd64cce4067692b1c3f26a89afe6ff9917c10e4b601c118bea06d23f8adbfe5c0ec12bc3"
        dest = os.path.join(self.path, 'installed')
        downloadAndExtract(url, dest, hash)

    def run(self, commands):
        actualCommands = [self.exe, '--vcpkg-root', self.path]
        actualCommands.extend(commands)
        print("Running command")
        print(actualCommands)
        executeSubprocess(actualCommands, folder=self.path)

    def buildDependencies(self):
        global args
        print("Installing host tools")
        self.run(['install', '--triplet', self.hostTriplet, 'hifi-host-tools'])
        # Special case for android, grab a bunch of binaries
        if args.android:
            self.downloadAndroidDependencies()
            return

        print("Installing build dependencies")
        self.run(['install', '--triplet', self.triplet, 'hifi-client-deps'])
        # Remove temporary build artifacts
        builddir = os.path.join(self.path, 'buildtrees')
        if os.path.isdir(builddir):
            print("Wiping build trees")
            shutil.rmtree(builddir, ignore_errors=True)

    def writeConfig(self):
        global args
        configFilePath = os.path.join(args.build_root, 'vcpkg.cmake')
        print("Writing cmake config to {}".format(configFilePath))
        # Write out the configuration for use by CMake
        cmakeScript = os.path.join(self.path, 'scripts/buildsystems/vcpkg.cmake')
        installPath = os.path.join(self.path, 'installed', self.triplet)
        toolsPath = os.path.join(self.path, 'installed', self.hostTriplet, 'tools')
        cmakeTemplate = """
set(CMAKE_TOOLCHAIN_FILE "{}" CACHE FILEPATH "Toolchain file")
set(CMAKE_TOOLCHAIN_FILE_UNCACHED "{}")
set(VCPKG_INSTALL_ROOT "{}")
set(VCPKG_TOOLS_DIR "{}")
"""
        if not args.android:
            cmakeTemplate += """
# If the cached cmake toolchain path is different from the computed one, exit
if(NOT (CMAKE_TOOLCHAIN_FILE_UNCACHED STREQUAL CMAKE_TOOLCHAIN_FILE))
    message(FATAL_ERROR "CMAKE_TOOLCHAIN_FILE has changed, please wipe the build directory and rerun cmake")
endif()
"""
        cmakeConfig = cmakeTemplate.format(cmakeScript, cmakeScript, installPath, toolsPath).replace('\\', '/')
        with open(configFilePath, 'w') as f:
            f.write(cmakeConfig)

    def writeTag(self):
        print("Writing tag {} to {}".format(self.tagContents, self.tagFile))
        with open(self.tagFile, 'w') as f:
            f.write(self.tagContents)

def main():
    # Only allow one instance of the program to run at a time
    global args
    vcpkg = VcpkgRepo()
    with Singleton(vcpkg.lockFile) as lock:
        vcpkg.bootstrap()
        vcpkg.buildDependencies()
        vcpkg.writeConfig()
        vcpkg.writeTag()


from argparse import ArgumentParser
parser = ArgumentParser(description='Prepare build dependencies.')
parser.add_argument('--android', action='store_true')
parser.add_argument('--debug', action='store_true')
parser.add_argument('--force-bootstrap', action='store_true')
parser.add_argument('--force-build', action='store_true')
parser.add_argument('--vcpkg-root', type=str, help='The location of the vcpkg distribution')
parser.add_argument('--build-root', required=True, type=str, help='The location of the cmake build')
args = parser.parse_args()

# Fixup env variables.  Leaving `USE_CCACHE` on will cause scribe to fail to build
# VCPKG_ROOT seems to cause confusion on Windows systems that previously used it for 
# building OpenSSL
removeEnvVars = ['VCPKG_ROOT', 'USE_CCACHE']
for var in removeEnvVars:
    if var in os.environ:
        del os.environ[var]


main()

