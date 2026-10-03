"""aargh (argh.h) as a Conan 2 package, built from this repository:

    conan create .

then require "aargh/<version>" and link aargh::aargh (CMakeDeps) or use the
include path; your code keeps #include "argh.h". As before, define ARGH_IMPLEMENTATION in one source file.
"""
import os
import re

from conan import ConanFile
from conan.tools.files import copy, load


class AarghConan(ConanFile):
    name = "aargh"
    description = "Single-header command-line argument parser for C99"
    license = "MIT"
    url = "https://github.com/ilyabrin/aargh"
    homepage = "https://github.com/ilyabrin/aargh"
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
        copy(self, "argh.h", self.source_folder, os.path.join(self.package_folder, "include", "aargh"))

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
        self.cpp_info.includedirs = ["include/aargh"]
        self.cpp_info.set_property("cmake_file_name", "aargh")
        self.cpp_info.set_property("cmake_target_name", "aargh::aargh")
        self.cpp_info.set_property("pkg_config_name", "aargh")
