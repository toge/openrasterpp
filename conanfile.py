from conan import ConanFile

required_conan_version = ">=1.53.0"

class LocalConan(ConanFile):
  name = "openrasterpp"
  version = "1.0"
  settings = "os", "arch", "compiler", "build_type"
  generators = "CMakeToolchain", "CMakeDeps", "VirtualBuildEnv"

  def configure(self):
    pass

  def requirements(self):
    self.requires("catch2/3.4.0")
    self.requires("minizip-ng/4.0.6")
    self.requires("lodepng/cci.20200615")
