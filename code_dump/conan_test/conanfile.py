
from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout
from conan.tools.files import copy

class MultiPackageProjectConan(ConanFile):
    name = "multi_package_project"
    version = "1.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    options = {
        "shared": [True, False],
        "cppstd": ["11", "14", "17", "20", "23"]
    }
    default_options = {
        "shared": False,
        "cppstd": "20"
    }

    def requirements(self):
        self.requires("fmt/9.1.0")
        self.requires("boost/1.81.0")
        self.requires("eigen/3.4.0")
        self.requires("nlohmann_json/3.11.2")

    def layout(self):
        cmake_layout(self)

    def configure(self):
        self.options["fmt"].shared = self.options.shared
        self.options["boost"].shared = self.options.shared
        self.settings.compiler.cppstd = self.options.cppstd

    def generate(self):
        deps = self.dependencies
        # Copy headers for each package
        copy(self, "*.h*", deps["fmt"].cpp_info.includedirs[0], self.source_folder + "/include")
        copy(self, "*.h*", deps["boost"].cpp_info.includedirs[0], self.source_folder + "/includet")
        copy(self, "*.h*", deps["eigen"].cpp_info.includedirs[0], self.source_folder + "/include")
        copy(self, "*.h*", deps["nlohmann_json"].cpp_info.includedirs[0], self.source_folder + "/include")

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

