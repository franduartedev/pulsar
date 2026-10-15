#pragma once

#include <pulsar/application/project/project_snapshot.hpp>

namespace pulsar::application {

// Legacy v1-v5 migration and in-memory snapshots use the historical primary
// master rule. Never infer a canvas from auxiliary clips.
[[nodiscard]] std::optional<ProjectCanvas> legacy_project_canvas(const ProjectSnapshot& snapshot);
[[nodiscard]] std::optional<ProjectCanvas> effective_project_canvas(const ProjectSnapshot& snapshot);

} // namespace pulsar::application
