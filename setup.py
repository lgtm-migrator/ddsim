import os
import sys
import re
import subprocess

from setuptools import setup, Extension, find_namespace_packages
from setuptools.command.build_ext import build_ext
from setuptools_scm import get_version


class CMakeExtension(Extension):
    def __init__(self, name, sourcedir='', namespace=''):
        Extension.__init__(self, name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)
        self.namespace = namespace


class CMakeBuild(build_ext):
    def build_extension(self, ext):
        version = get_version(root='.', relative_to=__file__)

        self.package = ext.namespace
        extdir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext.name)))
        # required for auto-detection of auxiliary "native" libs
        if not extdir.endswith(os.path.sep):
            extdir += os.path.sep

        cmake_generator = os.environ.get("CMAKE_GENERATOR", "")
        cfg = 'Debug' if self.debug else 'Release'

        cmake_args = [
            f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={extdir}",
            f"-DPYTHON_EXECUTABLE={sys.executable}",
            f"-DDDSIM_VERSION_INFO={version}",
            f"-DCMAKE_BUILD_TYPE={cfg}",
            f"-DBINDINGS=ON"
        ]
        build_args = []

        if self.compiler.compiler_type != "msvc":
            if not cmake_generator:
                cmake_args += ["-GNinja"]
        else:
            # Single config generators are handled "normally"
            single_config = any(x in cmake_generator for x in {"NMake", "Ninja"})
            # CMake allows an arch-in-generator style for backward compatibility
            contains_arch = any(x in cmake_generator for x in {"ARM", "Win64"})
            # Convert distutils Windows platform specifiers to CMake -A arguments
            plat_to_cmake = {
                "win32": "Win32",
                "win-amd64": "x64",
                "win-arm32": "ARM",
                "win-arm64": "ARM64",
            }
            # Specify the arch if using MSVC generator, but only if it doesn't
            # contain a backward-compatibility arch spec already in the
            # generator name.
            if not single_config and not contains_arch:
                cmake_args += ["-A", plat_to_cmake[self.plat_name]]
            # Multi-config generators have a different way to specify configs
            if not single_config:
                cmake_args += ["-DCMAKE_LIBRARY_OUTPUT_DIRECTORY_{}={}".format(cfg.upper(), extdir)]
                build_args += ["--config", cfg]

        # cross-compile support for macOS - respect ARCHFLAGS if set
        if sys.platform.startswith("darwin"):
            archs = re.findall(r"-arch (\S+)", os.environ.get("ARCHFLAGS", ""))
            if archs:
                cmake_args += ["-DCMAKE_OSX_ARCHITECTURES={}".format(";".join(archs))]

        # Set CMAKE_BUILD_PARALLEL_LEVEL to control the parallel build level across all generators.
        if "CMAKE_BUILD_PARALLEL_LEVEL" not in os.environ:
            if hasattr(self, "parallel") and self.parallel:
                build_args += ["-j{}".format(self.parallel)]

        if sys.platform == "win32":
            cmake_args += ['-T', 'ClangCl']

        if not os.path.exists(self.build_temp):
            os.makedirs(self.build_temp)
        else:
            os.remove(os.path.join(self.build_temp, 'CMakeCache.txt'))
        subprocess.check_call(['cmake', ext.sourcedir] + cmake_args, cwd=self.build_temp)
        subprocess.check_call(['cmake', '--build', '.', '--target', ext.name] + build_args, cwd=self.build_temp)


README_PATH = os.path.join(os.path.abspath(os.path.dirname(__file__)), 'README.md')
with open(README_PATH, encoding="utf8") as readme_file:
    README = readme_file.read()

setup(
    name='mqt.ddsim',
    author='Stefan Hillmich',
    author_email='stefan.hillmich@jku.at',
    description='MQT DDSIM - A quantum simulator based on decision diagrams written in C++',
    long_description=README,
    long_description_content_type='text/markdown',
    license='MIT',
    url='https://www.cda.cit.tum.de/research/quantum_simulation/',
    ext_modules=[CMakeExtension('pyddsim', namespace='mqt.ddsim')],
    cmdclass={"build_ext": CMakeBuild},
    zip_safe=False,
    packages=find_namespace_packages(include=['mqt.*']),
    python_requires=">=3.7",
    classifiers=[
        'Development Status :: 4 - Beta',
        'Programming Language :: Python :: 3',
        'Programming Language :: C++',
        'License :: OSI Approved :: MIT License',
        'Operating System :: Microsoft :: Windows',
        'Operating System :: MacOS',
        'Operating System :: POSIX :: Linux',
        'Intended Audience :: Science/Research',
        'Natural Language :: English',
        'Topic :: Scientific/Engineering :: Electronic Design Automation (EDA)',
    ],
    keywords='mqt quantum simulation',
    project_urls={
        'Source': 'https://github.com/cda-tum/ddsim/',
        'Tracker': 'https://github.com/cda-tum/ddsim/issues',
        'Research': 'https://www.cda.cit.tum.de/research/quantum_simulation/',
    },
    extras_require={
        "tnflow": ["sparse", "opt-einsum", "quimb", "pandas", "numpy"],
        "tests": ["pytest", "qiskit-terra>=0.19.2,<0.23.0"],
        "docs": ["Sphinx==5.3.0", "sphinx-rtd-theme==1.1.1", "sphinxcontrib-bibtex==2.5.0", "sphinx-copybutton==0.5.1"],
    }
)
