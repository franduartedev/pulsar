#include <pulsar/application/commands/command_processor.hpp>
#include <pulsar/application/project/project_codec.hpp>
#include <pulsar/application/project/project_document.hpp>
#include <pulsar/application/export/export_plan.hpp>
#include <pulsar/project_file/file_project_repository.hpp>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace pulsar;
using application::CommandProcessor;
using application::MediaProbe;
using application::MediaProbeResult;
using application::ProjectCodec;
using application::ProjectCodecError;
using application::ProjectDocument;
using application::ProjectSaveStatus;
using application::VideoStreamInfo;
using domain::MediaId;
using domain::MediaTime;
using domain::Rational;
using domain::TimeRange;

class FakeProbe final : public MediaProbe {
public:
  MediaProbeResult probe(const std::filesystem::path& path) const override {
    MediaProbeResult result;
    result.path = path;
    result.container_name = "matroska";
    result.video_streams.push_back(VideoStreamInfo{2, "h264", 1920, 1080, Rational{1, 90000},
                                                   MediaTime{120, Rational{1, 1}},
                                                   domain::FrameRate{30000, 1001}});
    return result;
  }
};

class BuiltinSoundProbe final : public MediaProbe {
public:
  MediaProbeResult probe(const std::filesystem::path& path) const override {
    const auto duration = MediaTime{4800, Rational{1, 4800}};
    return {path, std::string{"WAV"}, duration, {},
            {{0, "pcm_s16le", 48000, 1, Rational{1, 48000},
              MediaTime{48000, Rational{1, 48000}}}}};
  }
};

MediaTime t(std::int64_t value, Rational base = Rational{1, 1}) { return MediaTime{value, base}; }

ProjectDocument make_document() {
  ProjectDocument document;
  CommandProcessor processor{document};
  const auto first = processor.import_asset("media dir/clip \\\"one\\\"\\n.mp4", FakeProbe{});
  const auto second = processor.import_asset("second.mp4", FakeProbe{});
  (void)processor.add_clip(first, TimeRange{t(1001, Rational{1, 30000}), t(2002, Rational{1, 30000})}, t(0));
  (void)processor.add_clip(second, TimeRange{t(3, Rational{1, 2}), t(5, Rational{1, 2})}, t(4));
  return document;
}

void codec_roundtrip_and_canonical_output() {
  auto document = make_document();
  const auto snapshot = document.snapshot();
  const auto bytes = ProjectCodec::encode(snapshot);
  const auto state = ProjectCodec::decode(bytes);
  auto restored = ProjectDocument::restore(state);
  assert(ProjectCodec::encode(restored.snapshot()) == bytes);
  assert(!restored.is_dirty());
  assert(restored.history_state_id() == 1);
  assert(restored.saved_history_state_id() == 1);
  assert(restored.document_generation() == 0);
  assert(bytes.find("VIDEO_TRACK") != std::string::npos);
  assert(bytes.find("CLIP ") != std::string::npos);
}

void distant_music_roundtrips_as_project_version_seven() {
  auto document = make_document();
  CommandProcessor processor{document};
  domain::DistantMusicSettings settings;
  settings.enabled = true;
  settings.initial_gain_db = -24.0;
  settings.duration_seconds = 3.5;
  settings.start_cutoff_hz = 700.0;
  settings.end_cutoff_hz = 16'000.0;
  settings.reverb_amount = 0.35;
  settings.curve = domain::DistantMusicCurve::Smooth;
  processor.set_clip_distant_music(domain::ClipId{1}, settings);
  const auto bytes = ProjectCodec::encode(document.snapshot());
  assert(bytes.find("PULSAR_PROJECT 7") == 0);
  assert(bytes.find("CLIP_DISTANT_MUSIC 1") != std::string::npos);
  const auto restored = ProjectDocument::restore(ProjectCodec::decode(bytes));
  const auto* clip = restored.timeline().find_clip(domain::ClipId{1});
  assert(clip != nullptr && clip->distant_music() == settings);
  assert(ProjectCodec::encode(restored.snapshot()) == bytes);
}

void distant_music_gain_keyframes_roundtrip_as_project_version_eight() {
  auto document = make_document();
  CommandProcessor processor{document};
  domain::DistantMusicSettings settings;
  settings.enabled = true;
  settings.gain_keyframes = {{0.0, -42.0, domain::KeyframeInterpolation::Hold},
                             {1.5, -12.0, domain::KeyframeInterpolation::Smooth},
                             {3.0, 0.0, domain::KeyframeInterpolation::Linear}};
  processor.set_clip_distant_music(domain::ClipId{1}, settings);
  const auto bytes = ProjectCodec::encode(document.snapshot());
  assert(bytes.find("PULSAR_PROJECT 8") == 0);
  assert(bytes.find(" 3 0 -42 0 1.5 -12 2 3 0 1") != std::string::npos);
  const auto restored = ProjectDocument::restore(ProjectCodec::decode(bytes));
  const auto* clip = restored.timeline().find_clip(domain::ClipId{1});
  assert(clip != nullptr && clip->distant_music() == settings);
  assert(ProjectCodec::encode(restored.snapshot()) == bytes);
}

void subtitles_roundtrip_and_undo_as_project_version_nine() {
  auto document = make_document();
  CommandProcessor processor{document};
  const std::vector<application::SubtitleCue> cues{
      {1, t(0, Rational{1, 1000}), t(1500, Rational{1, 1000}), "Hola\nPULSAR"},
      {2, t(2000, Rational{1, 1000}), t(3500, Rational{1, 1000}), "Una nueva escena"},
  };
  processor.replace_subtitles(cues);
  assert(document.subtitles() == cues);
  assert(processor.undo());
  assert(document.subtitles().empty());
  assert(processor.redo());
  assert(document.subtitles() == cues);
  const auto bytes = ProjectCodec::encode(document.snapshot());
  assert(bytes.find("PULSAR_PROJECT 9") == 0);
  assert(bytes.find("SUBTITLE 1") != std::string::npos);
  const auto restored = ProjectDocument::restore(ProjectCodec::decode(bytes));
  assert(restored.subtitles() == cues);
  assert(ProjectCodec::encode(restored.snapshot()) == bytes);
}

void relinking_asset_is_undoable_and_preserves_clip_identity() {
  auto document = make_document();
  CommandProcessor processor{document};
  processor.relink_asset(domain::MediaId{1}, "relinked/clip.mp4", FakeProbe{});
  const auto replacement = document.find_asset(domain::MediaId{1});
  assert(replacement.has_value());
  assert(replacement->source_locator() == std::filesystem::path{"relinked/clip.mp4"});
  assert(document.timeline().find_clip(domain::ClipId{1}) != nullptr);
  assert(processor.undo());
  assert(document.find_asset(domain::MediaId{1})->source_locator() ==
         std::filesystem::path{"media dir/clip \"one\"\n.mp4"});
  assert(processor.redo());
  assert(document.find_asset(domain::MediaId{1})->source_locator() ==
         std::filesystem::path{"relinked/clip.mp4"});
}

void canvas_preset_is_undoable_and_persisted() {
  auto document = make_document();
  CommandProcessor processor{document};
  const auto original = *document.canvas();
  processor.set_project_canvas(application::ProjectCanvas{1080, 1920, original.frame_rate, false});
  assert(document.canvas()->width == 1080 && document.canvas()->height == 1920);
  assert(processor.undo());
  assert(*document.canvas() == original);
  assert(processor.redo());
  const auto bytes = ProjectCodec::encode(document.snapshot());
  assert(bytes.find("CANVAS 1080 1920") != std::string::npos);
  assert(ProjectCodec::encode(ProjectDocument::restore(ProjectCodec::decode(bytes)).snapshot()) == bytes);
}

void visual_stack_versions_and_validation() {
  using domain::TrackId;
  using domain::TrackKind;
  const auto legacy = ProjectCodec::encode(make_document().snapshot());
  const auto legacy_plan = application::exporting::ExportPlan::build(make_document().snapshot());
  for (int version = 1; version <= 5; ++version) {
    auto bytes = legacy;
    bytes.replace(15, 1, std::to_string(version));
    const auto state = ProjectCodec::decode(bytes);
    const auto document = ProjectDocument::restore(state);
    assert(state.visual_track_ids == std::vector<TrackId>{state.video_track_id});
    assert(document.visual_track_ids() == state.visual_track_ids);
    assert(document.canvas() == make_document().canvas());
    const auto migrated_plan = application::exporting::ExportPlan::build(document.snapshot());
    assert(migrated_plan.output_width() == legacy_plan.output_width());
    assert(migrated_plan.output_height() == legacy_plan.output_height());
    assert(migrated_plan.frame_rate().rational() == legacy_plan.frame_rate().rational());
    assert(migrated_plan.frame_count() == legacy_plan.frame_count());
    assert(ProjectCodec::encode(document.snapshot()) == legacy);
  }

  auto snapshot = make_document().snapshot();
  snapshot.timeline.add_track(domain::Track{TrackId{9}, TrackKind::Video});
  snapshot.timeline.add_track(domain::Track{TrackId{3}, TrackKind::Video});
  snapshot.timeline.add_track(domain::Track{TrackId{4}, TrackKind::Audio});
  snapshot.allocator_state.next_track_id = 10;
  snapshot.visual_track_ids = {TrackId{9}, TrackId{1}, TrackId{3}};
  const auto bytes = ProjectCodec::encode(snapshot);
  assert(bytes.starts_with("PULSAR_PROJECT 6\n"));
  assert(bytes.find("VISUAL_TRACKS 9 1 3\n") != std::string::npos);
  const auto restored = ProjectDocument::restore(ProjectCodec::decode(bytes));
  assert(restored.video_track_id() == TrackId{1});
  assert(restored.visual_track_ids() == snapshot.visual_track_ids);
  assert(restored.snapshot().visual_track_ids == snapshot.visual_track_ids);
  assert(ProjectCodec::encode(restored.snapshot()) == bytes);
  // Earlier unreleased v6 records without CANVAS still migrate from VIDEO 1.
  auto early_v6 = bytes;
  const auto canvas_record = early_v6.find("CANVAS ");
  early_v6.erase(canvas_record, early_v6.find('\n', canvas_record) - canvas_record + 1);
  assert(ProjectDocument::restore(ProjectCodec::decode(early_v6)).canvas() == snapshot.canvas);

  for (const auto& invalid : std::vector<std::vector<TrackId>>{
           {}, {TrackId{9}, TrackId{3}}, {TrackId{1}, TrackId{9}},
           {TrackId{9}, TrackId{1}, TrackId{3}, TrackId{3}},
           {TrackId{9}, TrackId{1}, TrackId{3}, TrackId{4}},
           {TrackId{9}, TrackId{1}, TrackId{3}, TrackId{99}}}) {
    auto state = ProjectCodec::decode(bytes);
    state.visual_track_ids = invalid;
    bool rejected = false;
    try { (void)ProjectDocument::restore(state); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    auto bad_snapshot = snapshot;
    bad_snapshot.visual_track_ids = invalid;
    rejected = false;
    try { (void)ProjectCodec::encode(bad_snapshot); }
    catch (const ProjectCodecError&) { rejected = true; }
    assert(rejected);
  }
  for (const auto replacement : {"", "VISUAL_TRACKS\n", "VISUAL_TRACKS 9 3\n",
                                 "VISUAL_TRACKS 9 1 3 3\n", "VISUAL_TRACKS 9 1 4\n",
                                 "VISUAL_TRACKS 9 1 99\n",
                                 "VISUAL_TRACKS 9 1 3\nVISUAL_TRACKS 9 1 3\n"}) {
    auto bad = bytes;
    bad.replace(bad.find("VISUAL_TRACKS"), std::string{"VISUAL_TRACKS 9 1 3\n"}.size(), replacement);
    bool rejected = false;
    try { (void)ProjectCodec::decode(bad); }
    catch (const ProjectCodecError&) { rejected = true; }
    assert(rejected);
  }
  for (const auto primary : {TrackId{4}, TrackId{99}}) {
    auto state = ProjectCodec::decode(bytes);
    state.video_track_id = primary;
    bool rejected = false;
    try { (void)ProjectDocument::restore(state); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
  }
  // Older formats cannot smuggle in an unrepresented auxiliary visual track.
  auto old_multivideo = bytes;
  old_multivideo.replace(15, 1, "5");
  old_multivideo.erase(old_multivideo.find("VISUAL_TRACKS"), std::string{"VISUAL_TRACKS 9 1 3\n"}.size());
  bool rejected = false;
  try { (void)ProjectCodec::decode(old_multivideo); }
  catch (const ProjectCodecError&) { rejected = true; }
  assert(rejected);
}

void canvas_survives_primary_move_and_auxiliary_edits() {
  ProjectDocument initial;
  CommandProcessor setup{initial};
  const auto media = setup.import_asset("visual.mp4", FakeProbe{});
  const auto clip = setup.add_clip(media, TimeRange{t(0), t(4)}, t(0));
  const auto canvas = initial.canvas();
  assert(canvas && canvas->width == 1920 && canvas->height == 1080);
  auto state = ProjectCodec::decode(ProjectCodec::encode(initial.snapshot()));
  state.tracks.push_back({domain::TrackId{9}, domain::TrackKind::Video, {}});
  state.visual_track_ids.push_back(domain::TrackId{9});
  state.allocator_state.next_track_id = 10;
  auto document = ProjectDocument::restore(state);
  CommandProcessor processor{document};
  processor.move_clip(clip, domain::TrackId{9}, t(0));
  assert(document.canvas() == canvas);
  auto plan = application::exporting::ExportPlan::build(document.snapshot());
  assert(plan.output_width() == 1920 && plan.output_height() == 1080);
  assert(plan.frame_rate().rational() == canvas->frame_rate.rational());
  assert(!plan.master_video_asset_id());
  const auto persisted = ProjectCodec::encode(document.snapshot());
  assert(persisted.find("CANVAS 1920 1080 30000 1001\n") != std::string::npos);
  assert(ProjectDocument::restore(ProjectCodec::decode(persisted)).canvas() == canvas);
  for (const auto invalid : {"CANVAS 0 1080 30000 1001\n", "CANVAS 1920 1080 0 1\n",
                             "CANVAS 1920 1080 30000 1001\nCANVAS 1920 1080 30000 1001\n"}) {
    auto damaged = persisted;
    damaged.replace(damaged.find("CANVAS "), std::string{"CANVAS 1920 1080 30000 1001\n"}.size(), invalid);
    bool rejected = false;
    try { (void)ProjectCodec::decode(damaged); }
    catch (const ProjectCodecError&) { rejected = true; }
    assert(rejected);
  }
  processor.move_clip(clip, domain::TrackId{9}, t(2));
  assert(document.canvas() == canvas);
  auto reordered = document.snapshot();
  reordered.timeline.add_track(domain::Track{domain::TrackId{10}, domain::TrackKind::Video});
  reordered.visual_track_ids = {document.video_track_id(), domain::TrackId{10}, domain::TrackId{9}};
  reordered.allocator_state.next_track_id = 11;
  assert(ProjectDocument::restore(ProjectCodec::decode(ProjectCodec::encode(reordered))).canvas() == canvas);
  processor.remove_clip(clip);
  assert(document.canvas() == canvas);
  auto empty_singleton = document.snapshot();
  empty_singleton.timeline.remove_track(domain::TrackId{9});
  empty_singleton.visual_track_ids = {document.video_track_id()};
  const auto empty_bytes = ProjectCodec::encode(empty_singleton);
  assert(empty_bytes.starts_with("PULSAR_PROJECT 6\n"));
  assert(ProjectDocument::restore(ProjectCodec::decode(empty_bytes)).canvas() == canvas);
  assert(processor.undo() && document.canvas() == canvas);
  assert(processor.undo() && document.canvas() == canvas);

  // An initially empty VIDEO 1 does not prevent the first VIDEO 2 picture
  // from establishing a durable canvas. Undo restores the previous state.
  ProjectDocument fresh;
  CommandProcessor fresh_commands{fresh};
  const auto aux_media = fresh_commands.import_asset("visual.mp4", FakeProbe{});
  auto aux_state = ProjectCodec::decode(ProjectCodec::encode(fresh.snapshot()));
  aux_state.tracks.push_back({domain::TrackId{9}, domain::TrackKind::Video, {}});
  aux_state.visual_track_ids.push_back(domain::TrackId{9});
  aux_state.allocator_state.next_track_id = 10;
  auto aux_document = ProjectDocument::restore(aux_state);
  CommandProcessor aux_commands{aux_document};
  assert(!aux_document.canvas());
  (void)aux_commands.add_clip(domain::TrackId{9}, aux_media, TimeRange{t(0), t(4)}, t(0));
  assert(aux_document.canvas() == canvas);
  assert(application::exporting::ExportPlan::build(aux_document.snapshot()).output_width() == 1920);
  assert(aux_commands.undo() && !aux_document.canvas());
  assert(aux_commands.redo() && aux_document.canvas() == canvas);
}

void visual_transform_v6_roundtrip_and_validation() {
  auto snapshot = make_document().snapshot();
  const auto* original = snapshot.timeline.find_clip(domain::ClipId{1});
  assert(original);
  const auto clip_id = original->id();
  snapshot.timeline.restore_clip(snapshot.video_track_id,
      domain::Clip{original->id(), original->media_id(), original->source_range(),
                   original->timeline_start(), original->audio_gain_db(), original->muted(),
                   original->fade_in(), original->fade_out(), {0.125, -0.25, 0.75, 32.5, 0.4}});
  const auto encoded = ProjectCodec::encode(snapshot);
  assert(encoded.starts_with("PULSAR_PROJECT 6\n"));
  assert(encoded.find("CLIP_TRANSFORM 0.125 -0.25 0.75 32.5 0.40000000000000002") != std::string::npos);
  const auto restored = ProjectDocument::restore(ProjectCodec::decode(encoded));
  assert((restored.timeline().find_clip(clip_id)->visual_transform() ==
          domain::VisualTransform{0.125, -0.25, 0.75, 32.5, 0.4}));

  const auto default_bytes = ProjectCodec::encode(make_document().snapshot());
  assert(default_bytes.find("CLIP_TRANSFORM") == std::string::npos);
  assert(ProjectDocument::restore(ProjectCodec::decode(default_bytes))
             .timeline().find_clip(domain::ClipId{1})->visual_transform().is_default());

  for (const auto* bad : {"nan 0 1 0 1", "0 0 0 0 1", "0 0 1 0 1.1", "11 0 1 0 1"}) {
    auto damaged = encoded;
    const auto begin = damaged.find("CLIP_TRANSFORM ") + std::string{"CLIP_TRANSFORM "}.size();
    damaged.replace(begin, damaged.find('\n', begin) - begin, bad);
    bool rejected = false;
    try { (void)ProjectCodec::decode(damaged); }
    catch (const ProjectCodecError&) { rejected = true; }
    assert(rejected);
  }
}

void builtin_locator_roundtrip() {
  ProjectDocument document;
  CommandProcessor processor{document};
  const auto media = processor.import_asset("pulsar-sound:click_soft", BuiltinSoundProbe{});
  const auto track = processor.add_track(domain::TrackKind::Audio);
  const auto clip = processor.add_clip(track, media,
                                       TimeRange{t(0, Rational{1, 48000}),
                                                 t(4800, Rational{1, 48000})}, t(2));
  processor.set_clip_audio_gain(clip, -6.0);
  processor.set_clip_muted(clip, true);
  const auto bytes = ProjectCodec::encode(document.snapshot());
  const auto restored = ProjectDocument::restore(ProjectCodec::decode(bytes));
  assert(restored.find_asset(media)->source_locator() ==
         std::filesystem::path{"pulsar-sound:click_soft"});
  assert(restored.timeline().find_clip(clip)->timeline_start() == t(2));
  assert(restored.timeline().find_clip(clip)->audio_gain_db() == -6.0);
  assert(restored.timeline().find_clip(clip)->muted());
}

void golden_fixture_decodes() {
  std::ifstream input{std::filesystem::path{PULSAR_SOURCE_DIR} / "tests" / "fixtures" /
                      "project_v1_golden.pulsar"};
  const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
  assert(!bytes.empty());
  auto restored = ProjectDocument::restore(ProjectCodec::decode(bytes));
  assert(restored.assets().size() == 2);
  assert(restored.timeline().tracks().front().clips().size() == 2);
  const auto canonical = ProjectCodec::encode(restored.snapshot());
  assert(canonical.find("PULSAR_PROJECT 2\n") == 0);
  assert(ProjectDocument::restore(ProjectCodec::decode(canonical)).assets().size() == 2);
}

void strict_decode_rejects_malformed_input() {
  const auto good = ProjectCodec::encode(ProjectDocument{}.snapshot());
  const std::vector<std::string> malformed{
      "PULSAR_PROJECT 6\n", "PULSAR_PROJECT 1\nTIMELINE 1\nEND\n",
      good.substr(0, good.size() - 4),
      good + "JUNK\n",
  };
  for (const auto& text : malformed) {
    bool rejected = false;
    try {
      (void)ProjectCodec::decode(text);
    } catch (const ProjectCodecError&) {
      rejected = true;
    }
    assert(rejected);
  }
  auto invalid_track_kind = good;
  const auto track_line = invalid_track_kind.find("TRACK ");
  assert(track_line != std::string::npos);
  const auto kind = invalid_track_kind.find("VIDEO", track_line);
  assert(kind != std::string::npos);
  invalid_track_kind.replace(kind, 5, "MYSTERY");
  bool track_rejected = false;
  try {
    (void)ProjectCodec::decode(invalid_track_kind);
  } catch (const ProjectCodecError&) {
    track_rejected = true;
  }
  assert(track_rejected);
}

void strict_m9_5_regressions() {
  const auto good = ProjectCodec::encode(make_document().snapshot());
  auto expect_decode_reject = [](const std::string& text) {
    bool rejected = false;
    try {
      (void)ProjectCodec::decode(text);
    } catch (const ProjectCodecError&) {
      rejected = true;
    }
    assert(rejected);
  };
  auto expect_restore_reject = [](const std::string& text) {
    bool rejected = false;
    try {
      (void)ProjectDocument::restore(ProjectCodec::decode(text));
    } catch (const std::exception&) {
      rejected = true;
    }
    assert(rejected);
  };

  expect_decode_reject(good + "\nGARBAGE\n");
  expect_decode_reject(good + "\n\nGARBAGE\n");
  (void)ProjectCodec::decode(good + "\n\n\n");
  const auto first_asset = good.find("ASSET ");
  assert(first_asset != std::string::npos);
  const auto first_asset_end = good.find('\n', first_asset);
  assert(first_asset_end != std::string::npos);
  const auto clip_line = good.find("CLIP ");
  assert(clip_line != std::string::npos);
  const auto clip_newline = good.find('\n', clip_line);
  assert(clip_newline != std::string::npos);
  const auto asset_after_clip = good.substr(0, first_asset) +
                                good.substr(first_asset_end + 1, clip_newline - first_asset_end - 1) +
                                good.substr(first_asset, first_asset_end - first_asset + 1) +
                                good.substr(clip_newline + 1);
  expect_decode_reject(asset_after_clip);

  auto duplicate_media = good;
  const auto second_asset = duplicate_media.find("ASSET ", duplicate_media.find("ASSET ") + 1);
  assert(second_asset != std::string::npos);
  duplicate_media.replace(second_asset + 6, 1, "1");
  expect_restore_reject(duplicate_media);

  auto duplicate_clip = good;
  const auto second_clip = duplicate_clip.find("CLIP ", duplicate_clip.find("CLIP ") + 1);
  assert(second_clip != std::string::npos);
  duplicate_clip.replace(second_clip + 5, 1, "1");
  expect_restore_reject(duplicate_clip);

  auto overlap = good;
  const auto second_clip_line = overlap.find("CLIP ", overlap.find("CLIP ") + 1);
  const auto second_clip_end = overlap.find('\n', second_clip_line);
  const auto timeline_start = overlap.rfind(" 4 1 1", second_clip_end);
  assert(timeline_start != std::string::npos);
  overlap.replace(timeline_start, 6, " 0 1 1");
  expect_restore_reject(overlap);

  auto unknown_track = good;
  const auto first_clip = unknown_track.find("CLIP ");
  assert(first_clip != std::string::npos);
  unknown_track.replace(first_clip + 7, 1, "9");
  expect_restore_reject(unknown_track);

  auto unknown_media = good;
  unknown_media.replace(first_clip + 9, 1, "9");
  expect_restore_reject(unknown_media);

  auto low_allocator = good;
  const auto allocator_line = low_allocator.find("ALLOCATOR ");
  assert(allocator_line != std::string::npos);
  low_allocator.replace(allocator_line + 10, 1, "2");
  expect_restore_reject(low_allocator);

  auto quoted_dash = make_document().snapshot();
  auto metadata = quoted_dash.assets.front().metadata();
  metadata.container_name = "-";
  const auto& original = quoted_dash.assets.front();
  quoted_dash.assets.front() = application::MediaAsset{
      original.id(), original.source_locator(), original.video_stream_index(),
      original.playable_duration(), metadata, original.availability(), original.availability_reason()};
  const auto dash_bytes = ProjectCodec::encode(quoted_dash);
  assert(dash_bytes.find(" \"-\"") != std::string::npos);
  assert(ProjectCodec::encode(ProjectDocument::restore(ProjectCodec::decode(dash_bytes)).snapshot()) ==
         dash_bytes);

  auto oversized = make_document().snapshot();
  auto oversized_metadata = oversized.assets.front().metadata();
  oversized.assets.front() = application::MediaAsset{
      oversized.assets.front().id(), std::string(65 * 1024, 'x'),
      oversized.assets.front().video_stream_index(), oversized.assets.front().playable_duration(),
      oversized_metadata};
  bool limit_rejected = false;
  try {
    (void)ProjectCodec::encode(oversized);
  } catch (const ProjectCodecError&) {
    limit_rejected = true;
  }
  assert(limit_rejected);
}

void edited_state_and_saved_snapshot_regressions() {
  ProjectDocument document;
  CommandProcessor processor{document};
  FakeProbe probe;
  const auto media = processor.import_asset("source.mp4", probe);
  const auto clip = processor.add_clip(media, TimeRange{t(0), t(10)}, t(0));
  processor.move_clip(clip, document.video_track_id(), t(2));
  processor.trim_clip_start(clip, t(4));
  const auto right = processor.split_clip(clip, t(8));
  assert(right == domain::ClipId{2});
  const auto snapshot = document.snapshot();
  const auto roundtrip = ProjectDocument::restore(ProjectCodec::decode(ProjectCodec::encode(snapshot)));
  assert(ProjectCodec::encode(roundtrip.snapshot()) == ProjectCodec::encode(snapshot));
  document.mark_history_state_saved(snapshot.history_state_id);
  processor.move_clip(clip, document.video_track_id(), t(20));
  assert(document.is_dirty());
  document.mark_history_state_saved(snapshot.history_state_id);
  assert(document.is_dirty());
}

void allocator_high_water_and_restore_validation() {
  auto document = make_document();
  auto snapshot = document.snapshot();
  snapshot.allocator_state.next_clip_id = 100;
  snapshot.allocator_state.next_media_id = 100;
  auto restored = ProjectDocument::restore(ProjectCodec::decode(ProjectCodec::encode(snapshot)));
  CommandProcessor processor{restored};
  const auto id = processor.add_clip(MediaId{1}, TimeRange{t(6), t(7)}, t(8));
  assert(id == domain::ClipId{100});

  snapshot.allocator_state.next_clip_id = 0;
  bool rejected = false;
  try {
    (void)ProjectDocument::restore(ProjectCodec::decode(ProjectCodec::encode(snapshot)));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

void repository_atomic_save_and_missing_media_refresh() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("pulsar-project-test-" + std::to_string(
                        std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  const auto media = root / "source clip.mp4";
  const auto project = root / "session.pulsar";
  { std::ofstream output{media}; output << "media"; }

  ProjectDocument document;
  CommandProcessor processor{document};
  const auto media_id = processor.import_asset(media, FakeProbe{});
  (void)processor.add_clip(media_id, TimeRange{t(0), t(2)}, t(0));
  pulsar::project_file::FileProjectRepository repository;
  const auto result = repository.save(project, document.snapshot());
  assert(result.status == ProjectSaveStatus::Durable);
  const auto canonical = ProjectCodec::encode(document.snapshot());
  {
    std::ifstream input{project, std::ios::binary};
    const std::string installed{std::istreambuf_iterator<char>{input}, {}};
    assert(installed == canonical);
  }
  const auto missing_parent = root / "does-not-exist" / "failed.pulsar";
  bool save_failed = false;
  try {
    (void)repository.save(missing_parent, document.snapshot());
  } catch (const application::ProjectRepositoryError&) {
    save_failed = true;
  }
  assert(save_failed);
  assert(!std::filesystem::exists(missing_parent));
  const auto state = repository.load(project);
  assert(state.assets.size() == 1);
  assert(state.assets.front().availability() == application::MediaAvailability::Available);

  std::filesystem::remove(media);
  const auto missing = repository.load(project);
  assert(missing.assets.front().availability() == application::MediaAvailability::Unavailable);
  auto restored = ProjectDocument::restore(missing);
  assert(restored.timeline().find_clip(domain::ClipId{1}) != nullptr);
  assert(restored.timeline().duration() == t(2));
  const auto relative_root = root / "relative";
  std::filesystem::create_directories(relative_root / "media");
  const auto relative_media = relative_root / "media" / "clip.mp4";
  const auto relative_project = relative_root / "relative.pulsar";
  { std::ofstream output{relative_media}; output << "media"; }
  ProjectDocument relative_document;
  CommandProcessor relative_processor{relative_document};
  const auto relative_id = relative_processor.import_asset("media/clip.mp4", FakeProbe{});
  (void)relative_processor.add_clip(relative_id, TimeRange{t(0), t(2)}, t(0));
  assert(repository.save(relative_project, relative_document.snapshot()).status == ProjectSaveStatus::Durable);
  const auto relative_loaded = repository.load(relative_project);
  assert(relative_loaded.assets.front().source_locator() == std::filesystem::path{"media/clip.mp4"});
  assert(relative_loaded.assets.front().availability() == application::MediaAvailability::Available);
  std::filesystem::remove(relative_media);
  const auto relative_missing = repository.load(relative_project);
  assert(relative_missing.assets.front().source_locator() == std::filesystem::path{"media/clip.mp4"});
  assert(relative_missing.assets.front().availability() == application::MediaAvailability::Unavailable);

  std::filesystem::remove_all(root);
}

} // namespace

void repository_normalizes_codec_failures() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("pulsar-project-codec-failures-" + std::to_string(
                        std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  pulsar::project_file::FileProjectRepository repository;

  // Malformed bytes through the real repository surface as repository errors.
  const auto malformed = root / "malformed.pulsar";
  { std::ofstream output{malformed}; output << "PULSAR_PROJECT 9\n"; }
  bool load_rejected = false;
  try {
    (void)repository.load(malformed);
  } catch (const application::ProjectRepositoryError& error) {
    load_rejected = true;
    const std::string message{error.what()};
    assert(message.find("cannot decode project file") != std::string::npos);
    assert(message.find("invalid header/version") != std::string::npos);
  }
  assert(load_rejected);

  // An unencodable snapshot fails before touching an existing destination.
  auto oversized = make_document().snapshot();
  oversized.assets.front() = application::MediaAsset{
      oversized.assets.front().id(), std::string(65 * 1024, 'x'),
      oversized.assets.front().video_stream_index(), oversized.assets.front().playable_duration(),
      oversized.assets.front().metadata()};
  const auto destination = root / "existing.pulsar";
  const std::string sentinel{"sentinel-bytes"};
  { std::ofstream output{destination, std::ios::binary}; output << sentinel; }
  bool save_rejected = false;
  try {
    (void)repository.save(destination, oversized);
  } catch (const application::ProjectRepositoryError& error) {
    save_rejected = true;
    const std::string message{error.what()};
    assert(message.find("cannot encode project") != std::string::npos);
    assert(message.find("quoted string exceeds size limit") != std::string::npos);
  }
  assert(save_rejected);
  {
    std::ifstream input{destination, std::ios::binary};
    const std::string installed{std::istreambuf_iterator<char>{input}, {}};
    assert(installed == sentinel);
  }
}

int main() {
  visual_transform_v6_roundtrip_and_validation();
  canvas_survives_primary_move_and_auxiliary_edits();
  visual_stack_versions_and_validation();
  codec_roundtrip_and_canonical_output();
  distant_music_roundtrips_as_project_version_seven();
  distant_music_gain_keyframes_roundtrip_as_project_version_eight();
  subtitles_roundtrip_and_undo_as_project_version_nine();
  relinking_asset_is_undoable_and_preserves_clip_identity();
  canvas_preset_is_undoable_and_persisted();
  builtin_locator_roundtrip();
  golden_fixture_decodes();
  strict_decode_rejects_malformed_input();
  strict_m9_5_regressions();
  edited_state_and_saved_snapshot_regressions();
  allocator_high_water_and_restore_validation();
  repository_atomic_save_and_missing_media_refresh();
  repository_normalizes_codec_failures();
}
