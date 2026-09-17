#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/media/media_probe.hpp>
#include <pulsar/domain/ids/ids.hpp>
#include <pulsar/domain/time/media_time.hpp>
#include <pulsar/domain/time/rational.hpp>
#include <pulsar/domain/time/time_range.hpp>

#include <cassert>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {
using namespace pulsar;
using application::CommandProcessor;
using domain::ClipId;
using domain::MediaId;
using domain::MediaTime;
using domain::Rational;
using domain::TimeRange;

MediaTime t(std::int64_t value) { return MediaTime{value, Rational{1, 1}}; }

class FakeProbe final : public application::MediaProbe {
public:
  mutable int calls{0};
  application::MediaProbeResult probe(const std::filesystem::path& path) const override {
    ++calls;
    application::MediaProbeResult result;
    result.path = path;
    result.video_streams.push_back(application::VideoStreamInfo{
        2, "h264", 1920, 1080, Rational{1, 1000}, MediaTime{100000, Rational{1, 1000}},
        std::nullopt});
    return result;
  }
};

MediaId import(application::ProjectDocument&, CommandProcessor& processor, FakeProbe& probe) {
  return processor.import_asset("media/source.mp4", probe);
}

bool same_range(TimeRange lhs, TimeRange rhs) {
  return lhs.start() == rhs.start() && lhs.end() == rhs.end();
}

void add_undo_redo() {
  application::ProjectDocument document;
  CommandProcessor processor{document};
  FakeProbe probe;
  const auto media = import(document, processor, probe);
  const auto clip = processor.add_clip(media, TimeRange{t(0), t(5)}, t(0));
  assert(document.timeline().find_clip(clip) != nullptr);
  assert(processor.undo());
  assert(document.timeline().find_clip(clip) == nullptr);
  assert(processor.redo());
  assert(document.timeline().find_clip(clip) != nullptr);
}

void remove_move_and_exact_restore() {
  application::ProjectDocument document;
  CommandProcessor processor{document};
  FakeProbe probe;
  const auto media = import(document, processor, probe);
  const auto clip = processor.add_clip(media, TimeRange{t(7), t(12)}, t(0));
  processor.move_clip(clip, document.video_track_id(), t(10));
  assert(document.timeline().find_clip(clip)->timeline_start() == t(10));
  assert(processor.undo());
  assert(document.timeline().find_clip(clip)->timeline_start() == t(0));
  processor.remove_clip(clip);
  assert(document.timeline().find_clip(clip) == nullptr);
  assert(processor.undo());
  const auto restored = document.timeline().find_clip(clip);
  assert(restored != nullptr && same_range(restored->source_range(), TimeRange{t(7), t(12)}) &&
         restored->timeline_start() == t(0));
  assert(processor.redo());
  assert(document.timeline().find_clip(clip) == nullptr);
}

void trims_restore_exact_values() {
  application::ProjectDocument document;
  CommandProcessor processor{document};
  FakeProbe probe;
  const auto media = import(document, processor, probe);
  const auto clip = processor.add_clip(media, TimeRange{t(7), t(20)}, t(10));
  processor.trim_clip_start(clip, t(13));
  auto current = document.timeline().find_clip(clip);
  assert(current != nullptr && current->timeline_start() == t(13) &&
         same_range(current->source_range(), TimeRange{t(10), t(20)}));
  assert(processor.undo());
  current = document.timeline().find_clip(clip);
  assert(current != nullptr && current->timeline_start() == t(10) &&
         same_range(current->source_range(), TimeRange{t(7), t(20)}));
  assert(processor.redo());
  processor.trim_clip_end(clip, t(18));
  current = document.timeline().find_clip(clip);
  assert(current != nullptr && same_range(current->source_range(), TimeRange{t(10), t(15)}));
  assert(processor.undo());
  current = document.timeline().find_clip(clip);
  assert(current != nullptr && same_range(current->source_range(), TimeRange{t(10), t(20)}));
}

void split_exact_identity_and_high_water() {
  application::ProjectDocument document;
  CommandProcessor processor{document};
  FakeProbe probe;
  const auto media = import(document, processor, probe);
  const auto clip = processor.add_clip(media, TimeRange{t(5), t(15)}, t(0));
  const auto right = processor.split_clip(clip, t(4));
  assert(right == ClipId{2});
  auto left_value = document.timeline().find_clip(clip);
  auto right_value = document.timeline().find_clip(right);
  assert(left_value != nullptr && right_value != nullptr);
  assert(same_range(left_value->source_range(), TimeRange{t(5), t(9)}));
  assert(same_range(right_value->source_range(), TimeRange{t(9), t(15)}));
  assert(processor.undo());
  assert(document.timeline().find_clip(clip) != nullptr);
  assert(document.timeline().find_clip(right) == nullptr);
  assert(document.next_clip_id() == ClipId{3});
  assert(processor.redo());
  assert(document.timeline().find_clip(right) != nullptr);
  assert(processor.undo());
  const auto branch_right = processor.split_clip(clip, t(6));
  assert(branch_right == ClipId{3});
  assert(document.timeline().find_clip(right) == nullptr);
}

void dirty_history_identity_and_generation() {
  application::ProjectDocument document;
  CommandProcessor processor{document};
  FakeProbe probe;
  assert(!document.is_dirty() && document.document_generation() == 0);
  import(document, processor, probe);
  document.mark_current_state_saved();
  const auto saved = document.history_state_id();
  const auto generation = document.document_generation();
  const auto second_media = import(document, processor, probe);
  const auto clip = processor.add_clip(second_media, TimeRange{t(0), t(1)}, t(0));
  assert(document.is_dirty() && document.history_state_id() != saved &&
         document.document_generation() == generation + 2);
  assert(processor.undo());
  assert(document.is_dirty());
  (void)clip;
  document.mark_current_state_saved();
  const auto saved_after_undo = document.history_state_id();
  (void)processor.add_clip(second_media, TimeRange{t(2), t(3)}, t(0));
  assert(document.is_dirty());
  assert(processor.undo());
  assert(document.history_state_id() == saved_after_undo);
  assert(!document.is_dirty());
  assert(processor.redo() && document.is_dirty());
}

void import_redo_does_not_probe() {
  application::ProjectDocument document;
  CommandProcessor processor{document};
  FakeProbe probe;
  const auto media = processor.import_asset("media/source.mp4", probe);
  assert(media == MediaId{1} && probe.calls == 1);
  assert(processor.undo());
  assert(!document.find_asset(media).has_value());
  assert(processor.redo());
  assert(document.find_asset(media).has_value() && probe.calls == 1);
}

void failures_are_atomic_and_preserve_redo() {
  application::ProjectDocument document;
  CommandProcessor processor{document};
  FakeProbe probe;
  const auto media = import(document, processor, probe);
  const auto first = processor.add_clip(media, TimeRange{t(0), t(5)}, t(0));
  assert(processor.undo() && processor.can_redo());
  const auto state = document.history_state_id();
  const auto generation = document.document_generation();
  const auto next = document.next_clip_id();
  bool failed = false;
  try {
    (void)processor.add_clip(MediaId{999}, TimeRange{t(0), t(1)}, t(0));
  } catch (const std::invalid_argument&) {
    failed = true;
  }
  assert(failed && processor.can_redo() && document.history_state_id() == state &&
         document.document_generation() == generation && document.next_clip_id() == next);
  assert(document.timeline().find_clip(first) == nullptr);
}

void capacity_zero_and_noncopyable() {
  static_assert(!std::is_copy_constructible_v<CommandProcessor>);
  static_assert(!std::is_move_constructible_v<CommandProcessor>);
  application::ProjectDocument document;
  CommandProcessor processor{document, 0};
  FakeProbe probe;
  const auto media = import(document, processor, probe);
  (void)processor.add_clip(media, TimeRange{t(0), t(1)}, t(0));
  assert(!processor.can_undo() && document.document_generation() == 2);
  assert(!processor.undo());
}
} // namespace

int main() {
  add_undo_redo();
  remove_move_and_exact_restore();
  trims_restore_exact_values();
  split_exact_identity_and_high_water();
  dirty_history_identity_and_generation();
  import_redo_does_not_probe();
  failures_are_atomic_and_preserve_redo();
  capacity_zero_and_noncopyable();
}
