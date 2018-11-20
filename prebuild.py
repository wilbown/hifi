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
import re
import zipfile
import tarfile
import tempfile
import time
import urllib.request
import functools
import configparser
import xml.etree.ElementTree as ET

try:
    import fcntl
except ImportError:
    fcntl = None

print = functools.partial(print, flush=True)

def recursiveFileList(startPath):
    result = []
    if os.path.isfile(startPath):
        result.append(startPath)
    elif os.path.isdir(startPath):
        for dirName, subdirList, fileList in os.walk(startPath):
            for fname in fileList:
                result.append(os.path.realpath(os.path.join(startPath, dirName, fname)))
    result.sort()
    return result


def executeSubprocessCapture(processArgs):
    processResult = subprocess.run(processArgs, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if (0 != processResult.returncode):
        raise RuntimeError('Call to "{}" failed.\n\narguments:\n{}\n\nstdout:\n{}\n\nstderr:\n{}'.format(
            processArgs[0],
            ' '.join(processArgs[1:]), 
            processResult.stdout.decode('utf-8'),
            processResult.stderr.decode('utf-8')))
    return processResult.stdout.decode('utf-8')

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


def getGradleHifiDir():
    p = re.compile(r'\s*HIFI_ANDROID_PRECOMPILED\s*=\s*(\S*)\s*')
    gradlePropertiesFile = os.path.expanduser('~/.gradle/gradle.properties') 
    if os.path.exists(gradlePropertiesFile):
        with open(gradlePropertiesFile) as f:
            for line in f.readlines():
                m = p.match(line)
                if m:
                    return m.group(1)
    return None
        

# gradlePropertiesFile = os.path.expanduser('~/.gradle/gradle.properties') 
# if os.path.exists(gradlePropertiesFile):
#     p = re.compile(r'\s*HIFI_ANDROID_PRECOMPILED\s*=\s*(\S*)\s*')
#     with open(gradlePropertiesFile) as f:
#         for line in f.readlines():
#             m = p.match(line)
#             if m:
#                 destBaseDir = m.group(1)
#                 break

# gradlePropertiesFile = os.path.expanduser('~/.gradle/gradle.properties') 
# if os.path.exists(gradlePropertiesFile):
#     p = re.compile(r'\s*HIFI_ANDROID_PRECOMPILED\s*=\s*(\S*)\s*')
#     with open(gradlePropertiesFile) as f:
#         for line in f.readlines():
#             m = p.match(line)
#             if m:
#                 destBaseDir = m.group(1)
#                 break

# gradlePropertiesFile = os.path.expanduser('~/.gradle/gradle.properties') 
# if os.path.exists(gradlePropertiesFile):
#     p = re.compile(r'\s*HIFI_ANDROID_PRECOMPILED\s*=\s*(\S*)\s*')
#     with open(gradlePropertiesFile) as f:
#         for line in f.readlines():
#             m = p.match(line)
#             if m:
#                 destBaseDir = m.group(1)
#                 break



def hashFile(file, hasher = hashlib.sha512()):
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
    filenames = recursiveFileList(folder)
    return hashFiles(filenames)

def downloadAndExtract(url, destPath, hash=None, hasher = hashlib.sha512(), isZip=False):
    tempFileDescriptor, tempFileName = tempfile.mkstemp()
    # OSX Python doesn't support SSL, so we need to bypass it.  
    # However, we still validate the downloaded file's sha512 hash
    context = ssl._create_unverified_context()
    with urllib.request.urlopen(url, context=context) as response, open(tempFileDescriptor, 'wb') as tempFile:
        shutil.copyfileobj(response, tempFile)

    # Verify the hash
    if hash and hash != hashFile(tempFileName, hasher):
        raise RuntimeError("Downloaded file does not match hash")
        
    if isZip:
        with zipfile.ZipFile(tempFileName) as zip:
            zip.extractall(destPath)
    else:
        # Extract the archive
        with tarfile.open(tempFileName, 'r:gz') as tgz:
            tgz.extractall(destPath)
    os.remove(tempFileName)

# Used to ensure only one instance of the script runs at a time
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

# Encapsulates the vcpkg system 
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

        self.configFilePath = os.path.join(args.build_root, 'vcpkg.cmake')
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
            if args.android:
                self.path = os.path.join(basePath, 'android', self.id)
            else:
                self.path = os.path.join(basePath, self.id)

        print("Using vcpkg path {}".format(self.path))
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
            self.androidPackagePath = os.path.join(self.path, 'android')
            self.projectRoot = os.path.realpath(os.path.join(__file__, os.path.pardir))
            self.androidAppPath = os.path.join(self.projectRoot, 'android/apps', args.android)
            self.androidJniPath = os.path.join(self.androidAppPath, 'src/main/jniLibs/arm64-v8a')
            self.androidAssetPath = os.path.join(self.androidAppPath, 'src/main/assets')
        else:
            self.triplet = self.hostTriplet

    def outOfDate(self):
        return False
        global args
        # Prevent doing a clean if we've explcitly set a directory for vcpkg
        if args.vcpkg_root is not None:
            return False
        if args.force_build:
            return True
        if args.skip_bootstrap:
            return False

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

    # Make sure the VCPKG prerequisites are all there.
    def bootstrap(self):
        global args
        if not self.outOfDate():
            return
        
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

    def run(self, commands):
        actualCommands = [self.exe, '--vcpkg-root', self.path]
        actualCommands.extend(commands)
        print("Running command")
        print(actualCommands)
        executeSubprocess(actualCommands, folder=self.path)

    def setupDependencies(self):
        global args
        print("Installing host tools")
        #self.run(['install', '--triplet', self.hostTriplet, 'hifi-host-tools'])

        # Special case for android, grab a bunch of binaries
        # Migrate the gradle work here
        if args.android:
            print("Installing Android binaries")
            self.setupAndroidDependencies()
            return

        print("Installing build dependencies")
        self.run(['install', '--triplet', self.triplet, 'hifi-client-deps'])
        # Remove temporary build artifacts
        builddir = os.path.join(self.path, 'buildtrees')
        if os.path.isdir(builddir):
            print("Wiping build trees")
            shutil.rmtree(builddir, ignore_errors=True)


    def downloadAndroidPackages(self, androidPackages):
        for packageName in androidPackages:
            package = androidPackages[packageName]
            dest = os.path.join(self.androidPackagePath, packageName)
            if os.path.isdir(dest):
                continue
            url = 'https://hifi-public.s3.amazonaws.com/dependencies/android/'
            if 'baseUrl' in package:
                url = package['baseUrl']
            url += package['file']
            if 'versionId' in package:
                url += '?versionId=' + package['versionId']
            zipFile = package['file'].endswith('.zip')
            downloadAndExtract(url, dest, isZip=zipFile, hash=package['checksum'], hasher=hashlib.md5())


    def copyAndroidLibs(self, androidPackages):
        if not os.path.isdir(self.androidJniPath):
            os.makedirs(self.androidJniPath)
        for packageName in androidPackages:
            package = androidPackages[packageName]
            if 'sharedLibFolder' in package:
                sharedLibFolder = os.path.join(self.androidPackagePath, packageName, package['sharedLibFolder'])
                if 'includeLibs' in package:
                    for lib in package['includeLibs']:
                        print("Copying {}".format(lib))
                        shutil.copy(os.path.join(sharedLibFolder, lib), self.androidJniPath)
                else:
                    print("Copying {}".format(os.path.join(sharedLibFolder, '*')))
                    shutil.copy(os.path.join(sharedLibFolder, '*'), self.androidJniPath)

    def androidQtBundle(self):
        QT5_DEPS = [
            'Qt5Concurrent',
            'Qt5Core',
            'Qt5Gui',
            'Qt5Multimedia',
            'Qt5Network',
            'Qt5OpenGL',
            'Qt5Qml',
            'Qt5Quick',
            'Qt5QuickControls2',
            'Qt5QuickTemplates2',
            'Qt5Script',
            'Qt5ScriptTools',
            'Qt5Svg',
            'Qt5WebChannel',
            'Qt5WebSockets',
            'Qt5Widgets',
            'Qt5XmlPatterns',
            # Android specific
            'Qt5AndroidExtras',
            'Qt5WebView',
        ]

        qtRoot = os.path.join(self.androidPackagePath, 'qt')
        files = []
        permissions = []
        features = []

        for lib in QT5_DEPS:
            libfile = os.path.join(qtRoot, "lib/lib{}.so".format(lib))
            if not os.path.exists(libfile):
                continue
            files.append(libfile)
            androidDeps = os.path.join(qtRoot, "lib/{}-android-dependencies.xml".format(lib))
            if not os.path.exists(androidDeps):
                continue

            tree = ET.parse(androidDeps)
            root = tree.getroot()                
            for item in root.findall('./dependencies/lib/depends/*'):
                if (item.tag == 'lib') or (item.tag == 'bundled'):
                    relativeFilename = item.attrib['file']
                    if (relativeFilename.startswith('qml')):
                        continue
                    filename = os.path.join(qtRoot, relativeFilename)
                    files.extend(recursiveFileList(filename))
                elif item.tag == 'jar' and 'bundling' in item.attrib and item.attrib['bundling'] == "1":
                    files.append(os.path.join(qtRoot, item.attrib['file']))
                elif item.tag == 'permission':
                    permissions.append(item.attrib['name'])
                elif item.tag == 'feature':
                    features.append(item.attrib['name'])

        qmlImportCommandFile = os.path.join(qtRoot, 'bin/qmlimportscanner')
        system = platform.system()
        if 'Windows' == system:
            qmlImportCommandFile += ".exe"
        if not os.path.isfile(qmlImportCommandFile):
            raise RuntimeError("Couldn't find qml import scanner")

        qmlRootPath = os.path.join(self.projectRoot, 'interface/resources/qml')
        qmlRootPath = os.path.normcase(os.path.realpath(qmlRootPath))
        qmlImportPath = os.path.join(qtRoot, 'qml')
        commandResult = executeSubprocessCapture([
            qmlImportCommandFile, 
            '-rootPath', qmlRootPath, 
            '-importPath', qmlImportPath
        ])
        qmlImportResults = json.loads(commandResult)
        for item in qmlImportResults:
            if 'path' not in item:
                print("Warning: QML import could not be resolved in any of the import paths: {}".format(item['name']))
                continue
            path = os.path.normcase(os.path.realpath(item['path']))
            if not os.path.exists(path):
                continue
            if path.startswith(qmlRootPath):
                continue
            files.extend(recursiveFileList(path))


        libDestinationDirectory = self.androidJniPath
        jarDestinationDirectory = os.path.join(self.androidAppPath, 'libs')
        assetDestinationDirectory = self.androidAssetPath
        assetDestinationDirectory = self.androidAssetPath
        libsXmlFile = os.path.join(self.androidAppPath, 'src/main/res/values/libs.xml')

        libsXmlRoot = ET.Element('resources')
        qtLibsNode = ET.SubElement(libsXmlRoot, 'array', {'name':'qt_libs'})
        bundledLibsNode = ET.SubElement(libsXmlRoot, 'array', {'name':'bundled_in_lib'})
        bundledAssetsNode = ET.SubElement(libsXmlRoot, 'array', {'name':'bundled_in_assets'})
        libPrefix = 'lib'
        for sourceFile in files:
            if not os.path.isfile(sourceFile):
                raise RuntimeError("Unable to find dependency file " + sourceFile)
            relativePath = os.path.relpath(sourceFile, qtRoot)
            destinationFile = None
            if relativePath.endswith('.so'):
                garbledFileName = None
                if relativePath.startswith(libPrefix):
                    garbledFileName = relativePath[4:]
                    p = re.compile(r'lib(Qt5.*).so')
                    m = p.search(garbledFileName)
                    if not m:
                        raise RuntimeError("Huh?")
                    libName = m.group(1)
                    ET.SubElement(qtLibsNode, 'item').text = libName
                else:
                    garbledFileName = 'lib' + relativePath.replace('\\', '_'[0])
                    value = "{}:{}".format(garbledFileName, relativePath).replace('\\', '/')
                    ET.SubElement(bundledLibsNode, 'item').text = value
                destinationFile = os.path.join(libDestinationDirectory, garbledFileName)
            elif relativePath.startswith('jar'):
                destinationFile = os.path.join(jarDestinationDirectory, relativePath[4:])
            else:
                value = "--Added-by-androiddeployqt--/{}:{}".format(relativePath,relativePath).replace('\\', '/')
                ET.SubElement(bundledAssetsNode, 'item').text = value
                destinationFile = os.path.join(assetDestinationDirectory, relativePath)

            destinationParent = os.path.realpath(os.path.join(destinationFile, os.path.pardir))
            if not os.path.isdir(destinationParent):
                os.makedirs(destinationParent)
            #print("{} -> {}".format(sourceFile, destinationFile))
            #shutil.copy(sourceFile, destinationFile)

        tree = ET.ElementTree(libsXmlRoot)
        tree.write(libsXmlFile, 'UTF-8', True)
        #with open(, 'w') as f:

    def setupAndroidDependencies(self):
        # vcpkg prebuilt
        if not os.path.isdir(os.path.join(self.path, 'installed', 'arm64-android')):
            url = "https://hifi-public.s3.amazonaws.com/dependencies/vcpkg/vcpkg-arm64-android.tar.gz"
            hash = "832f82a4d090046bdec25d313e20f56ead45b54dd06eee3798c5c8cbdd64cce4067692b1c3f26a89afe6ff9917c10e4b601c118bea06d23f8adbfe5c0ec12bc3"
            dest = os.path.join(self.path, 'installed')
            downloadAndExtract(url, dest, hash)

        androidPackages = {
            'qt': {
                'file': 'qt-5.11.1_linux_armv8-libcpp_openssl_patched.tgz',
                'versionId': '3S97HBM5G5Xw9EfE52sikmgdN3t6C2MN',
                'checksum': 'aa449d4bfa963f3bc9a9dfe558ba29df',
            },
            'bullet': {
                'file': 'bullet-2.88_armv8-libcpp.tgz',
                'versionId': 'S8YaoED0Cl8sSb8fSV7Q2G1lQJSNDxqg',
                'checksum': '81642779ccb110f8c7338e8739ac38a0',
            },            
            'draco': {
                'file': 'draco_armv8-libcpp.tgz',
                'versionId': '3.B.uBj31kWlgND3_R2xwQzT_TP6Dz_8',
                'checksum': '617a80d213a5ec69fbfa21a1f2f738cd',
            },
            'glad': {
                'file': 'glad_armv8-libcpp.zip',
                'versionId': 'r5Zran.JSCtvrrB6Q4KaqfIoALPw3lYY',
                'checksum': 'a8ee8584cf1ccd34766c7ddd9d5e5449',
            },
            'gvr': {
                'file': 'gvrsdk_v1.101.0.tgz',
                'versionId': 'nqBV_j81Uc31rC7bKIrlya_Hah4v3y5r',
                'checksum': '57fd02baa069176ba18597a29b6b4fc7',
            },
            'nvtt': {
                'file': 'nvtt_armv8-libcpp.zip',
                'versionId': 'lmkBVR5t4UF1UUwMwEirnk9H_8Nt90IO',
                'checksum': 'eb46d0b683e66987190ed124aabf8910',
                'sharedLibFolder': 'lib',
                'includeLibs': ['libnvtt.so', 'libnvmath.so', 'libnvimage.so', 'libnvcore.so']
            },
            'oculus': {
                'file': 'ovr_sdk_mobile_1.19.0.zip',
                'versionId': 's_RN1vlEvUi3pnT7WPxUC4pQ0RJBs27y',
                'checksum': '98f0afb62861f1f02dd8110b31ed30eb',
                'sharedLibFolder': 'VrApi/Libs/Android/arm64-v8a/Release',
                'includeLibs': ['libvrapi.so']
            },
            'openssl': {
                'file': 'openssl-1.1.0g_armv8.tgz',
                'versionId': 'AiiPjmgUZTgNj7YV1EEx2lL47aDvvvAW',
                'checksum': 'cabb681fbccd79594f65fcc266e02f32'
            },
            'polyvox': {
                'file': 'polyvox_armv8-libcpp.tgz',
                'versionId': 'A2kbKiNhpIenGq23bKRRzg7IMAI5BI92',
                'checksum': 'dba88b3a098747af4bb169e9eb9af57e',
                'sharedLibFolder': 'lib',
                'includeLibs': ['Release/libPolyVoxCore.so', 'libPolyVoxUtil.so'],
            },
            'tbb': {
                'file': 'tbb-2018_U1_armv8_libcpp.tgz',
                'versionId': 'mrRbWnv4O4evcM1quRH43RJqimlRtaKB',
                'checksum': '20768f298f53b195e71b414b0ae240c4',
                'sharedLibFolder': 'lib/release',
                'includeLibs': ['libtbb.so', 'libtbbmalloc.so'],
            },
            'hifiAC': {
                'baseUrl': 'http://s3.amazonaws.com/hifi-public/dependencies/',
                'file': 'codecSDK-android_armv8-2.0.zip',
                'checksum': '1cbef929675818fc64c4101b72f84a6a'
            },
            'etc2comp': {
                'file': 'etc2comp-patched-armv8-libcpp.tgz',
                'versionId': 'bHhGECRAQR1vkpshBcK6ByNc1BQIM8gU',
                'checksum': '14b02795d774457a33bbc60e00a786bc'
            },
            'breakpad': {
                'file': 'breakpad.tgz',
                'versionId': '8VrYXz7oyc.QBxNia0BVJOUBvrFO61jI',
                'checksum': 'ddcb23df336b08017042ba4786db1d9e',
                'sharedLibFolder': 'lib',
                'includeLibs': {'libbreakpad_client.a'}
            }
        }
        system = platform.system()
        if 'Windows' == system:
            androidPackages['qt'] = {
                'file': 'qt-5.11.1_win_armv8-libcpp_openssl_patched.tgz',
                'versionId': 'JfWM0P_Mz5Qp0LwpzhrsRwN3fqlLSFeT',
                'checksum': '0582191cc55431aa4f660848a542883e'
            }
        elif 'Darwin' == system:
            androidPackages['qt'] = {
                'file': 'qt-5.11.1_osx_armv8-libcpp_openssl_patched.tgz',
                'versionId': 'OxBD7iKINv1HbyOXmAmDrBb8AF3N.Kup',
                'checksum': 'c83cc477c08a892e00c71764dca051a0'
            }

        self.downloadAndroidPackages(androidPackages)
        self.copyAndroidLibs(androidPackages)
        self.androidQtBundle()

    def writeConfig(self):
        global args
        print("Writing cmake config to {}".format(self.configFilePath))
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
        with open(self.configFilePath, 'w') as f:
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
        if not args.skip_bootstrap:
            vcpkg.bootstrap()
        vcpkg.setupDependencies()
        vcpkg.writeConfig()
        vcpkg.writeTag()


from argparse import ArgumentParser
parser = ArgumentParser(description='Prepare build dependencies.')
parser.add_argument('--android', type=str)
parser.add_argument('--debug', action='store_true')
parser.add_argument('--force-bootstrap', action='store_true')
parser.add_argument('--skip-bootstrap', action='store_true')
parser.add_argument('--force-build', action='store_true')
parser.add_argument('--vcpkg-root', type=str, help='The location of the vcpkg distribution')
parser.add_argument('--build-root', required=True, type=str, help='The location of the cmake build')

if True:
    args = parser.parse_args()
else:
    args = parser.parse_args(['--android', 'interface', '--skip-bootstrap', '--build-root', os.path.expanduser('~/build_test')])

# Fixup env variables.  Leaving `USE_CCACHE` on will cause scribe to fail to build
# VCPKG_ROOT seems to cause confusion on Windows systems that previously used it for 
# building OpenSSL
removeEnvVars = ['VCPKG_ROOT', 'USE_CCACHE']
for var in removeEnvVars:
    if var in os.environ:
        del os.environ[var]


main()
