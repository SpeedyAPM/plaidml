message("Fetching gflags")
FetchContent_Declare(
  gflags
  URL      https://github.com/gflags/gflags/archive/e292e0452fcfd5a8ae055b59052fc041cbab4abf.zip
  URL_HASH SHA256=7d17922978692175c67ef5786a014df44bfbfe3b48b30937cca1413d4ff65f75
)

FetchContent_GetProperties(gflags)
if(NOT gflags_POPULATED)
  FetchContent_Populate(gflags)
  add_subdirectory(${gflags_SOURCE_DIR} ${gflags_BINARY_DIR})
endif()
