#pragma once

#include <filesystem>

namespace pulsar::project_file {

// Location of the shipped read-only assets (the Core Sounds pack lives in
// <assets>/sounds). Nothing here is baked in at build time: a release must run
// from any directory. Search order:
//   1. $PULSAR_ASSETS_DIR                      (explicit override, e.g. for testers)
//   2. <exe dir>/assets, <exe dir>/../assets   (build tree and portable bundle)
//   3. <exe dir>/../share/pulsar/assets        (installed under a prefix)
//   4. <cwd>/assets
// Returns the first directory that contains sounds/index.json, or an empty path.
[[nodiscard]] std::filesystem::path assets_directory();

} // namespace pulsar::project_file
