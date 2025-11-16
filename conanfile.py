from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeDeps, CMakeToolchain


class TwsRedisBridgeConan(ConanFile):
    name = "tws-redis-bridge"
    version = "0.1.0"
    
    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}
    
    # Sources are located in the project root
    exports_sources = "CMakeLists.txt", "src/*", "include/*", "tests/*"
    
    def requirements(self):
        """Production dependencies"""
        self.requires("redis-plus-plus/1.3.11")  # type: ignore[misc]
        self.requires("rapidjson/cci.20230929")  # type: ignore[misc]
        self.requires("concurrentqueue/1.0.4")  # type: ignore[misc]
        # NOTE: Using system protobuf 3.12.4 instead of Conan (matches TWS API)
    
    def build_requirements(self):
        """Test dependencies"""
        self.test_requires("catch2/3.5.0")  # type: ignore[misc]
    
    def config_options(self):
        if self.settings.os == "Windows":  # type: ignore[attr-defined]
            del self.options.fPIC  # type: ignore[attr-defined]
    
    def layout(self):
        cmake_layout(self)
    
    def generate(self):
        # REASON: Generate CMake find_package files for dependencies
        deps = CMakeDeps(self)
        deps.generate()
        
        # REASON: Generate CMake toolchain with build settings
        tc = CMakeToolchain(self)
        tc.generate()
    
    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
    
    def package(self):
        cmake = CMake(self)
        cmake.install()
