set_languages("c++20")
add_rules("mode.debug", "mode.release")

local libs = { "fmt", "gtest", "prepucio", "tabulate" }

add_includedirs("include")
add_requires(table.unpack(libs))

target("fat-eater-lib")
  set_kind("static")
  add_files("source/**/*.cpp")
  add_packages(table.unpack(libs))

target("fat-eater")
  set_kind("binary")
  add_files("standalone/main.cpp")
  add_packages(table.unpack(libs))
  add_deps("fat-eater-lib")

  after_build(function (target)
    os.cp("assets/", target:targetdir())
  end)
