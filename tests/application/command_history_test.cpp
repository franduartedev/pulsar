#include <pulsar/application/commands/command.hpp>
#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/domain/ids/ids.hpp>
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/rational.hpp>
#include <pulsar/domain/time/time_range.hpp>
#include <pulsar/domain/timeline/clip.hpp>
#include <pulsar/domain/timeline/timeline.hpp>
#include <pulsar/domain/timeline/track.hpp>
#include <pulsar/domain/timeline/track_kind.hpp>

#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

using pulsar::application::AddClipCommand;
using pulsar::application::CommandProcessor;
using pulsar::application::MoveClipCommand;
using pulsar::application::RemoveClipCommand;
using pulsar::domain::Clip;
using pulsar::domain::ClipId;
using pulsar::domain::MediaId;
using pulsar::domain::MediaTime;
using pulsar::domain::Rational;
using pulsar::domain::TimeRange;
using pulsar::domain::Timeline;
using pulsar::domain::TimelineId;
using pulsar::domain::Track;
using pulsar::domain::TrackId;
using pulsar::domain::TrackKind;

int fail(std::string_view message) {
  std::cerr << "command history test failed: " << message << '\n';
  return 1;
}

template <typename Exception, typename Callable>
bool throws(Callable callable) {
  try {
    callable();
  } catch (const Exception&) {
    return true;
  } catch (...) {
    return false;
  }

  return false;
}

MediaTime seconds(std::int64_t value) {
  return MediaTime{value, Rational{1, 1}};
}

Clip clip(ClipId id, MediaId media_id, MediaTime timeline_start, MediaTime duration) {
  return Clip{id, media_id, TimeRange{MediaTime::zero(), duration}, timeline_start};
}

Timeline base_timeline() {
  Timeline timeline{TimelineId{1}};
  timeline.add_track(Track{TrackId{1}, TrackKind::Video});
  timeline.add_track(Track{TrackId{2}, TrackKind::Audio});
  return timeline;
}

bool add_undo_redo_tests() {
  Timeline timeline = base_timeline();
  CommandProcessor processor{timeline};

  processor.execute(AddClipCommand{TrackId{1}, clip(ClipId{1}, MediaId{1}, seconds(0), seconds(5))});
  if (timeline.find_clip(ClipId{1}) == nullptr || !processor.can_undo()) {
    return false;
  }
  if (!processor.undo() || timeline.find_clip(ClipId{1}) != nullptr || !processor.can_redo()) {
    return false;
  }
  if (!processor.redo() || timeline.find_clip(ClipId{1}) == nullptr) {
    return false;
  }

  return true;
}

bool remove_undo_redo_tests() {
  Timeline timeline = base_timeline();
  timeline.insert_clip(TrackId{1}, clip(ClipId{1}, MediaId{1}, seconds(5), seconds(5)));
  CommandProcessor processor{timeline};

  processor.execute(RemoveClipCommand{ClipId{1}});
  if (timeline.find_clip(ClipId{1}) != nullptr) {
    return false;
  }
  if (!processor.undo()) {
    return false;
  }
  const auto* restored = timeline.find_clip(ClipId{1});
  if (restored == nullptr || restored->timeline_start() != seconds(5) ||
      timeline.find_track(TrackId{1})->find_clip(ClipId{1}) == nullptr) {
    return false;
  }
  if (!processor.redo() || timeline.find_clip(ClipId{1}) != nullptr) {
    return false;
  }

  return true;
}

bool move_undo_redo_tests() {
  Timeline timeline = base_timeline();
  timeline.insert_clip(TrackId{1}, clip(ClipId{1}, MediaId{1}, seconds(0), seconds(5)));
  CommandProcessor processor{timeline};

  processor.execute(MoveClipCommand{ClipId{1}, TrackId{2}, seconds(10)});
  if (timeline.find_track(TrackId{1})->find_clip(ClipId{1}) != nullptr ||
      timeline.find_track(TrackId{2})->find_clip(ClipId{1}) == nullptr ||
      timeline.find_clip(ClipId{1})->timeline_start() != seconds(10)) {
    return false;
  }
  if (!processor.undo() || timeline.find_track(TrackId{1})->find_clip(ClipId{1}) == nullptr ||
      timeline.find_clip(ClipId{1})->timeline_start() != seconds(0)) {
    return false;
  }
  if (!processor.redo() || timeline.find_track(TrackId{2})->find_clip(ClipId{1}) == nullptr ||
      timeline.find_clip(ClipId{1})->timeline_start() != seconds(10)) {
    return false;
  }

  return true;
}

bool redo_invalidation_tests() {
  Timeline timeline = base_timeline();
  CommandProcessor processor{timeline};

  processor.execute(AddClipCommand{TrackId{1}, clip(ClipId{1}, MediaId{1}, seconds(0), seconds(5))});
  if (!processor.undo() || !processor.can_redo()) {
    return false;
  }
  processor.execute(AddClipCommand{TrackId{1}, clip(ClipId{2}, MediaId{2}, seconds(5), seconds(5))});
  if (processor.can_redo()) {
    return false;
  }

  return true;
}

bool failed_command_tests() {
  Timeline timeline = base_timeline();
  CommandProcessor processor{timeline};

  processor.execute(AddClipCommand{TrackId{1}, clip(ClipId{1}, MediaId{1}, seconds(0), seconds(5))});
  processor.undo();
  if (!processor.can_redo()) {
    return false;
  }
  const std::size_t undo_before = processor.undo_size();
  const std::size_t redo_before = processor.redo_size();

  if (!throws<std::invalid_argument>([&processor] {
        processor.execute(AddClipCommand{TrackId{99},
                                         clip(ClipId{2}, MediaId{2}, seconds(0), seconds(5))});
      })) {
    return false;
  }
  if (processor.undo_size() != undo_before || processor.redo_size() != redo_before ||
      timeline.find_clip(ClipId{2}) != nullptr) {
    return false;
  }

  return true;
}

bool capacity_tests() {
  Timeline timeline = base_timeline();
  CommandProcessor processor{timeline, 3};

  processor.execute(AddClipCommand{TrackId{1}, clip(ClipId{1}, MediaId{1}, seconds(0), seconds(1))});
  processor.execute(AddClipCommand{TrackId{1}, clip(ClipId{2}, MediaId{2}, seconds(1), seconds(1))});
  processor.execute(AddClipCommand{TrackId{1}, clip(ClipId{3}, MediaId{3}, seconds(2), seconds(1))});
  processor.execute(AddClipCommand{TrackId{1}, clip(ClipId{4}, MediaId{4}, seconds(3), seconds(1))});

  if (processor.undo_size() != 3) {
    return false;
  }
  if (!processor.undo() || !processor.undo() || !processor.undo()) {
    return false;
  }
  if (processor.can_undo()) {
    return false;
  }
  if (timeline.find_clip(ClipId{1}) == nullptr || timeline.find_clip(ClipId{2}) != nullptr ||
      timeline.find_clip(ClipId{3}) != nullptr || timeline.find_clip(ClipId{4}) != nullptr) {
    return false;
  }

  return true;
}

bool capacity_edge_tests() {
  Timeline disabled_timeline = base_timeline();
  CommandProcessor disabled{disabled_timeline, 0};
  disabled.execute(
      AddClipCommand{TrackId{1}, clip(ClipId{10}, MediaId{10}, seconds(0), seconds(1))});
  if (disabled_timeline.find_clip(ClipId{10}) == nullptr || disabled.can_undo() ||
      disabled.undo_size() != 0) {
    return false;
  }

  Timeline single_timeline = base_timeline();
  CommandProcessor single{single_timeline, 1};
  single.execute(AddClipCommand{TrackId{1}, clip(ClipId{20}, MediaId{20}, seconds(0), seconds(1))});
  single.execute(AddClipCommand{TrackId{1}, clip(ClipId{21}, MediaId{21}, seconds(1), seconds(1))});
  if (single.undo_size() != 1) {
    return false;
  }
  if (!single.undo()) {
    return false;
  }
  if (single_timeline.find_clip(ClipId{20}) == nullptr ||
      single_timeline.find_clip(ClipId{21}) != nullptr) {
    return false;
  }

  return true;
}

bool empty_history_tests() {
  Timeline timeline = base_timeline();
  CommandProcessor processor{timeline};

  if (processor.can_undo() || processor.can_redo()) {
    return false;
  }
  if (processor.undo() || processor.redo()) {
    return false;
  }

  return true;
}

} // namespace

int main() {
  if (!add_undo_redo_tests()) {
    return fail("add undo/redo tests");
  }
  if (!remove_undo_redo_tests()) {
    return fail("remove undo/redo tests");
  }
  if (!move_undo_redo_tests()) {
    return fail("move undo/redo tests");
  }
  if (!redo_invalidation_tests()) {
    return fail("redo invalidation tests");
  }
  if (!failed_command_tests()) {
    return fail("failed command tests");
  }
  if (!capacity_tests()) {
    return fail("capacity tests");
  }
  if (!capacity_edge_tests()) {
    return fail("capacity edge tests");
  }
  if (!empty_history_tests()) {
    return fail("empty history tests");
  }

  return 0;
}
