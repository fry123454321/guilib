from conan import ConanFile
from conan.tools.cmake import cmake_layout

class libgui(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        self.requires("uwebsockets/20.71.0")