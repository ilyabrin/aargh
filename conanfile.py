"""argh.h as a Conan 2 package, built from this repository:

    conan create .

then require "argh/<version>" and link argh::argh (CMakeDeps) or use the
include path. As before, define ARGH_IMPLEMENTATION in one source file.
"""
import os
import re

from conan import ConanFile
from conan.tools.files import copy, load


class ArghConan(ConanFile):
    name = "argh"
    description = "Single-header command-line argument parser for C99"
    license = "MIT"
    url = "https://github.com/ilyabrin/argh"
    homepage = "https://github.com/ilyabrin/argh"
    topics = ("cli", "argument-parser", "command-line", "header-only", "embedded")
    package_type = "header-library"
    settings = "os", "arch", "compiler", "build_type"
    exports_sources = "argh.h", "LICENSE"
    no_copy_source = True

    def set_version(self):
        # The version lives in argh.h only
        text = load(self, os.path.join(self.recipe_folder, "argh.h"))
        self.version = re.search(r'#define ARGH_VERSION "([^"]+)"', text).group(1)

    def package_id(self):
        self.info.clear()

    def package(self):
        copy(self, "LICENSE", self.source_folder, os.path.join(self.package_folder, "licenses"))
        copy(self, "argh.h", self.source_folder, os.path.join(self.package_folder, "include"))

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
        self.cpp_info.set_property("cmake_file_name", "argh")
        self.cpp_info.set_property("cmake_target_name", "argh::argh")
        self.cpp_info.set_property("pkg_config_name", "argh")
