from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMake, CMakeToolchain, CMakeDeps
from conan.tools.build import check_min_cppstd

class GreengrassProvisioningServiceConan(ConanFile):
    name = "greengrass_provisioning_service"
    version = "1.0.0"
    
    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False]
    }
    default_options = {
        "shared": False,
        "fPIC": True
    }
    
    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "CMakeLists.txt", "src/*", "tests/*", "scripts/*"
    
    def validate(self):
        check_min_cppstd(self, "17")
    
    def config_options(self):
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")
    
    def requirements(self):
        self.requires("cli11/2.3.2")
        self.requires("nlohmann_json/3.11.2")
        self.requires("spdlog/1.12.0")
        self.requires("fmt/10.2.1")
        self.requires("libcurl/8.4.0")
        self.requires("sqlite3/3.44.0")
        
    def build_requirements(self):
        self.test_requires("gtest/1.14.0")
    
    def layout(self):
        cmake_layout(self)
    
    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.variables["BUILD_TESTS"] = True
        tc.generate()
    
    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
    
    def package(self):
        cmake = CMake(self)
        cmake.install() 