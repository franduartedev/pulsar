#include <pulsar/application/project/project_codec.hpp>
#include <pulsar/application/composition/project_canvas.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pulsar::application {
namespace {

constexpr std::size_t max_line_size = 1024 * 1024;
constexpr std::size_t max_string_size = 64 * 1024;
constexpr std::size_t max_records = 100'000;

struct Token {
  std::string text;
  bool quoted{false};
};

[[noreturn]] void parse_fail(std::size_t line, std::string_view message) {
  throw ProjectCodecError{"line " + std::to_string(line) + ": " + std::string{message}};
}

int hex_digit(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  return -1;
}

std::vector<Token> tokenize(std::string_view line, std::size_t line_number) {
  if (line.size() > max_line_size) parse_fail(line_number, "line exceeds size limit");
  if (!line.empty() && line.back() == '\r') parse_fail(line_number, "CRLF is not canonical");
  std::vector<Token> tokens;
  std::size_t index = 0;
  while (index < line.size()) {
    while (index < line.size() && (line[index] == ' ' || line[index] == '\t')) ++index;
    if (index == line.size()) break;
    if (line[index] == '"') {
      ++index;
      std::string value;
      while (index < line.size() && line[index] != '"') {
        char current = line[index++];
        if (current != '\\') {
          value.push_back(current);
        } else {
          if (index == line.size()) parse_fail(line_number, "unterminated escape");
          const char escaped = line[index++];
          switch (escaped) {
          case '\\': value.push_back('\\'); break;
          case '"': value.push_back('"'); break;
          case 'n': value.push_back('\n'); break;
          case 'r': value.push_back('\r'); break;
          case 't': value.push_back('\t'); break;
          case 'x':
            if (index + 1 >= line.size()) parse_fail(line_number, "short hex escape");
            {
              const int high = hex_digit(line[index]);
              const int low = hex_digit(line[index + 1]);
              if (high < 0 || low < 0) parse_fail(line_number, "invalid hex escape");
              value.push_back(static_cast<char>((high << 4) | low));
              index += 2;
            }
            break;
          default: parse_fail(line_number, "unknown escape");
          }
        }
        if (value.size() > max_string_size) parse_fail(line_number, "quoted string exceeds size limit");
      }
      if (index == line.size()) parse_fail(line_number, "unterminated quoted string");
      ++index;
      if (index < line.size() && line[index] != ' ' && line[index] != '\t') {
        parse_fail(line_number, "quoted token must be separated");
      }
      tokens.push_back(Token{std::move(value), true});
    } else {
      const std::size_t start = index;
      while (index < line.size() && line[index] != ' ' && line[index] != '\t') ++index;
      tokens.push_back(Token{std::string{line.substr(start, index - start)}, false});
    }
  }
  return tokens;
}

bool is_unsigned_token(const Token& token) noexcept {
  if (token.quoted || token.text.empty()) return false;
  return std::all_of(token.text.begin(), token.text.end(),
                     [](const char value) { return value >= '0' && value <= '9'; });
}

std::int64_t signed_number(const Token& token, std::size_t line, std::string_view field) {
  if (token.quoted || token.text.empty() || token.text.front() == '+') {
    parse_fail(line, "invalid " + std::string{field});
  }
  std::int64_t value = 0;
  const auto result = std::from_chars(token.text.data(), token.text.data() + token.text.size(), value);
  if (result.ec != std::errc{} || result.ptr != token.text.data() + token.text.size()) {
    parse_fail(line, "invalid " + std::string{field});
  }
  return value;
}

std::uint64_t unsigned_number(const Token& token, std::size_t line, std::string_view field) {
  if (token.quoted || token.text.empty() || token.text.front() == '-') {
    parse_fail(line, "invalid " + std::string{field});
  }
  std::uint64_t value = 0;
  const auto result = std::from_chars(token.text.data(), token.text.data() + token.text.size(), value);
  if (result.ec != std::errc{} || result.ptr != token.text.data() + token.text.size()) {
    parse_fail(line, "invalid " + std::string{field});
  }
  return value;
}

int int_number(const Token& token, std::size_t line, std::string_view field) {
  const auto value = signed_number(token, line, field);
  if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max()) {
    parse_fail(line, "invalid " + std::string{field});
  }
  return static_cast<int>(value);
}

double double_number(const Token& token, std::size_t line, std::string_view field) {
  if (token.quoted || token.text.empty()) parse_fail(line, "invalid " + std::string{field});
  double value = 0.0;
  const auto result = std::from_chars(token.text.data(), token.text.data() + token.text.size(),
                                      value, std::chars_format::general);
  if (result.ec != std::errc{} || result.ptr != token.text.data() + token.text.size() ||
      !std::isfinite(value)) {
    parse_fail(line, "invalid " + std::string{field});
  }
  return value;
}

domain::MediaTime media_time(const std::vector<Token>& tokens, std::size_t& index,
                             std::size_t line, std::string_view field) {
  if (index + 2 >= tokens.size()) parse_fail(line, "missing " + std::string{field});
  const auto value = signed_number(tokens[index++], line, field);
  const auto numerator = signed_number(tokens[index++], line, std::string{field} + " numerator");
  const auto denominator = signed_number(tokens[index++], line, std::string{field} + " denominator");
  try {
    return domain::MediaTime{value, domain::Rational{numerator, denominator}};
  } catch (const std::exception& error) {
    parse_fail(line, error.what());
  }
}

std::string quote(std::string_view value) {
  if (value.size() > max_string_size) {
    throw ProjectCodecError{"quoted string exceeds size limit"};
  }
  static constexpr char hex[] = "0123456789ABCDEF";
  std::string result{"\""};
  for (const unsigned char byte : value) {
    switch (byte) {
    case '\\': result += "\\\\"; break;
    case '"': result += "\\\""; break;
    case '\n': result += "\\n"; break;
    case '\r': result += "\\r"; break;
    case '\t': result += "\\t"; break;
    default:
      if (byte < 0x20 || byte >= 0x7F) {
        result += "\\x";
        result.push_back(hex[byte >> 4]);
        result.push_back(hex[byte & 0xF]);
      } else {
        result.push_back(static_cast<char>(byte));
      }
    }
  }
  result.push_back('"');
  return result;
}

void append_time(std::ostringstream& output, domain::MediaTime value) {
  output << value.value() << ' ' << value.time_base().numerator() << ' '
         << value.time_base().denominator();
}

std::vector<const domain::Track*> sorted_tracks(const domain::Timeline& timeline) {
  std::vector<const domain::Track*> tracks;
  for (const auto& track : timeline.tracks()) tracks.push_back(&track);
  std::sort(tracks.begin(), tracks.end(), [](const auto* lhs, const auto* rhs) {
    return lhs->id().value() < rhs->id().value();
  });
  return tracks;
}

} // namespace

std::string ProjectCodec::encode(const ProjectSnapshot& snapshot) {
  try {
    validate_visual_track_ids(snapshot.timeline, snapshot.video_track_id, snapshot.visual_track_ids);
    if (snapshot.canvas && (snapshot.canvas->width <= 0 || snapshot.canvas->height <= 0 ||
                            snapshot.canvas->width > 32768 || snapshot.canvas->height > 32768 ||
                            static_cast<std::int64_t>(snapshot.canvas->width) * snapshot.canvas->height > 64'000'000)) {
      throw std::invalid_argument{"invalid project canvas dimensions"};
    }
  } catch (const std::invalid_argument& error) {
    throw ProjectCodecError{error.what()};
  }
  bool has_visual_transforms = false;
  bool has_audio_track_state = false;
  bool has_distant_music = false;
  bool has_distant_music_keyframes = false;
  bool has_telephone = false;
  bool has_underwater = false;
  bool has_underwater_keyframes = false;
  bool has_audio_effect_order = false;
  bool has_telephone_keyframes = false;
  for (const auto& track : snapshot.timeline.tracks()) {
    if (track.audio_state() != domain::AudioTrackState{}) has_audio_track_state = true;
    for (const auto& clip : track.clips()) {
      if (!clip.visual_transform().is_default()) has_visual_transforms = true;
      if (clip.distant_music().enabled) has_distant_music = true;
      if (clip.distant_music().enabled && !clip.distant_music().gain_keyframes.empty()) {
        has_distant_music_keyframes = true;
      }
      if (clip.telephone().enabled) has_telephone = true;
      if (clip.telephone().enabled && !clip.telephone().mix_keyframes.empty()) {
        has_telephone_keyframes = true;
      }
      if (clip.underwater().enabled) has_underwater = true;
      if (clip.underwater().enabled && !clip.underwater().depth_keyframes.empty()) {
        has_underwater_keyframes = true;
      }
      if (clip.audio_effect_order() != domain::canonical_audio_effect_order()) {
        has_audio_effect_order = true;
      }
    }
  }
  const bool needs_v6 = snapshot.visual_track_ids.size() > 1 || has_visual_transforms ||
      has_audio_track_state ||
      !snapshot.transitions.empty() ||
      (snapshot.canvas && snapshot.canvas != legacy_project_canvas(snapshot));
  const std::size_t record_count = 1 + 1 + snapshot.timeline.tracks().size() + 1 +
                                    snapshot.assets.size() + snapshot.titles.size() + snapshot.subtitles.size() + snapshot.transitions.size() +
                                    (needs_v6 ? 1 + (snapshot.canvas ? 1 : 0) : 0);
  std::size_t clip_count = 0;
  for (const auto& track : snapshot.timeline.tracks()) clip_count += track.clips().size();
  if (record_count + clip_count + 2 > max_records) {
    throw ProjectCodecError{"project exceeds record limit"};
  }
  std::ostringstream output;
  // Files without titles stay byte-compatible version 2; titles need version 3.
  const bool has_titles = !snapshot.titles.empty() || snapshot.allocator_state.next_title_id != 1;
  const bool has_subtitles = !snapshot.subtitles.empty();
  bool needs_v4 = false;
  bool needs_v5 = false;
  for (const auto& title : snapshot.titles) {
    if (title.position()) needs_v5 = true;   // free title placement
    if (title.background() != domain::TitleBackground::None || title.opacity_percent() != 100) needs_v4 = true;   // newer features share version 4
  }
  for (const auto& track : snapshot.timeline.tracks()) {
    for (const auto& clip : track.clips()) {
      if (clip.fade_in() > domain::MediaTime::zero() || clip.fade_out() > domain::MediaTime::zero()) needs_v4 = true;
    }
  }
  // Version 4 adds clip fades; every file that does not need a newer feature keeps its old version.
  output << (has_telephone_keyframes ? "PULSAR_PROJECT 14\n" : has_audio_effect_order ? "PULSAR_PROJECT 13\n" : has_underwater_keyframes ? "PULSAR_PROJECT 12\n" : has_underwater ? "PULSAR_PROJECT 11\n" : has_telephone ? "PULSAR_PROJECT 10\n" : has_subtitles ? "PULSAR_PROJECT 9\n" : has_distant_music_keyframes ? "PULSAR_PROJECT 8\n" : has_distant_music ? "PULSAR_PROJECT 7\n" : needs_v6 ? "PULSAR_PROJECT 6\n" : needs_v5 ? "PULSAR_PROJECT 5\n" : needs_v4 ? "PULSAR_PROJECT 4\n" : has_titles ? "PULSAR_PROJECT 3\n" : "PULSAR_PROJECT 2\n");
  output << "TIMELINE " << snapshot.timeline.id().value() << "\n";
  const auto tracks = sorted_tracks(snapshot.timeline);
  for (const auto* track : tracks) {
    output << "TRACK " << track->id().value() << ' '
           << (track->kind() == domain::TrackKind::Video ? "VIDEO" : "AUDIO");
    if (track->audio_state() != domain::AudioTrackState{}) {
      output << " TRACK_AUDIO " << std::setprecision(17) << track->audio_state().gain_db << ' '
             << (track->audio_state().muted ? 1 : 0) << ' ' << (track->audio_state().solo ? 1 : 0);
    }
    output << "\n";
  }
  output << "VIDEO_TRACK " << snapshot.video_track_id.value() << "\n";
  if (needs_v6) {
    output << "VISUAL_TRACKS";
    for (const auto id : snapshot.visual_track_ids) output << ' ' << id.value();
    output << '\n';
    if (snapshot.canvas) {
      output << "CANVAS " << snapshot.canvas->width << ' ' << snapshot.canvas->height << ' '
             << snapshot.canvas->frame_rate.rational().numerator() << ' '
             << snapshot.canvas->frame_rate.rational().denominator();
      if (snapshot.canvas->provisional_still) output << " STILL";
      output << '\n';
    }
  }

  std::vector<const MediaAsset*> assets;
  for (const auto& asset : snapshot.assets) assets.push_back(&asset);
  std::sort(assets.begin(), assets.end(), [](const auto* lhs, const auto* rhs) {
    return lhs->id().value() < rhs->id().value();
  });
  for (const auto* asset : assets) {
    if (!asset->has_video()) {
      if (!asset->audio().has_value()) {
        throw ProjectCodecError{"audio-only asset has no audio metadata"};
      }
      const auto& audio = *asset->audio();
      output << "ASSET " << asset->id().value() << " AUDIO_ONLY ";
      append_time(output, asset->playable_duration());
      output << ' ' << quote(asset->source_locator().string()) << " AUDIO "
             << audio.stream_index << ' ' << audio.sample_rate << ' '
             << audio.channel_count << ' ' << audio.time_base.numerator() << ' '
             << audio.time_base.denominator() << ' ';
      if (audio.duration.has_value()) append_time(output, *audio.duration);
      else output << '-';
      output << "\n";
      continue;
    }
    output << "ASSET " << asset->id().value() << ' ' << asset->video_stream_index() << ' ';
    append_time(output, asset->playable_duration());
    output << ' ' << quote(asset->source_locator().string()) << ' '
           << quote(asset->metadata().codec_name) << ' ' << asset->metadata().width << ' '
           << asset->metadata().height << ' ' << asset->metadata().time_base.numerator() << ' '
           << asset->metadata().time_base.denominator() << ' ';
    if (asset->metadata().nominal_frame_rate.has_value()) {
      output << asset->metadata().nominal_frame_rate->rational().numerator() << ' '
             << asset->metadata().nominal_frame_rate->rational().denominator();
    } else {
      output << '-';
    }
    output << ' ';
    if (asset->metadata().container_name.has_value()) {
      output << quote(*asset->metadata().container_name);
    } else {
      output << '-';
    }
    // A0 extension (Outcome A): trailing optional audio binding. Absence
    // means no audio; old v1 lines without the suffix decode unchanged.
    if (asset->audio().has_value()) {
      const auto& audio = *asset->audio();
      output << " AUDIO " << audio.stream_index << ' ' << audio.sample_rate << ' '
             << audio.channel_count << ' ' << audio.time_base.numerator() << ' '
             << audio.time_base.denominator() << ' ';
      if (audio.duration.has_value()) {
        append_time(output, *audio.duration);
      } else {
        output << '-';
      }
    }
    output << "\n";
  }

  for (const auto* track : tracks) {
    for (const auto& clip : track->clips()) {
      output << "CLIP " << clip.id().value() << ' ' << track->id().value() << ' '
             << clip.media_id().value() << ' ';
      append_time(output, clip.source_range().start());
      output << ' ';
      append_time(output, clip.source_range().end());
      output << ' ';
      append_time(output, clip.timeline_start());
      const bool clip_needs_v4 = clip.fade_in() > domain::MediaTime::zero() || clip.fade_out() > domain::MediaTime::zero();
      if (clip.audio_gain_db() != 0.0 || clip.muted() || clip_needs_v4) {
        output << " CLIP_AUDIO " << std::setprecision(17) << clip.audio_gain_db() << ' '
               << (clip.muted() ? 1 : 0);
      }
      if (clip_needs_v4) {
        output << " CLIP_FADE ";
        append_time(output, clip.fade_in());
        output << ' ';
        append_time(output, clip.fade_out());
      }
      if (!clip.visual_transform().is_default()) {
        const auto& transform = clip.visual_transform();
        output << " CLIP_TRANSFORM " << std::setprecision(17) << transform.position_x << ' '
               << transform.position_y << ' ' << transform.scale << ' '
               << transform.rotation_degrees << ' ' << transform.opacity;
      }
      if (clip.distant_music().enabled) {
        const auto& fx = clip.distant_music();
        output << " CLIP_DISTANT_MUSIC 1 " << std::setprecision(17)
               << fx.initial_gain_db << ' ' << fx.duration_seconds << ' '
               << fx.start_cutoff_hz << ' ' << fx.end_cutoff_hz << ' '
               << fx.reverb_amount << ' ' << static_cast<int>(fx.curve);
        if (!fx.gain_keyframes.empty()) {
          output << ' ' << fx.gain_keyframes.size();
          for (const auto& keyframe : fx.gain_keyframes) {
            output << ' ' << keyframe.time_seconds << ' ' << keyframe.value << ' '
                   << static_cast<int>(keyframe.interpolation);
          }
        }
      }
      if (clip.telephone().enabled) {
        const auto& fx = clip.telephone();
        output << " CLIP_TELEPHONE 1 " << std::setprecision(17) << fx.mix << ' ' << fx.drive;
        if (!fx.mix_keyframes.empty()) {
          output << ' ' << fx.mix_keyframes.size();
          for (const auto& keyframe : fx.mix_keyframes) {
            output << ' ' << keyframe.time_seconds << ' ' << keyframe.value << ' '
                   << static_cast<int>(keyframe.interpolation);
          }
        }
      }
      if (clip.underwater().enabled) {
        const auto& fx = clip.underwater();
        output << " CLIP_UNDERWATER 1 " << std::setprecision(17) << fx.mix << ' ' << fx.depth;
        if (!fx.depth_keyframes.empty()) {
          output << ' ' << fx.depth_keyframes.size();
          for (const auto& keyframe : fx.depth_keyframes) {
            output << ' ' << keyframe.time_seconds << ' ' << keyframe.value << ' '
                   << static_cast<int>(keyframe.interpolation);
          }
        }
      }
      if (clip.audio_effect_order() != domain::canonical_audio_effect_order()) {
        output << " CLIP_FX_ORDER";
        for (const auto type : clip.audio_effect_order()) output << ' ' << static_cast<int>(type);
      }
      output << "\n";
    }
  }

  for (const auto& transition : snapshot.transitions) {
    output << "TRANSITION CROSS_DISSOLVE " << transition.track_id.value() << ' '
           << transition.outgoing_clip_id.value() << ' ' << transition.incoming_clip_id.value() << ' ';
    append_time(output, transition.duration);
    output << "\n";
  }

  for (const auto& title : snapshot.titles) {
    output << "TITLE " << title.id().value() << ' ';
    append_time(output, title.start());
    output << ' ';
    append_time(output, title.end());
    output << ' ' << title.size_permille() << ' ' << title.color_rgba() << ' '
           << static_cast<int>(title.horizontal()) << ' ' << static_cast<int>(title.vertical()) << ' '
           << (title.shadow() ? 1 : 0) << ' ' << quote(title.text()) << "\n";
    if (title.background() != domain::TitleBackground::None || title.opacity_percent() != 100) {
      output << "TITLE_LOOK " << title.id().value() << ' ' << static_cast<int>(title.background()) << ' '
             << title.opacity_percent() << "\n";
    }
    if (title.position()) {
      output << "TITLE_POS " << title.id().value() << ' ' << title.position()->x_permille << ' '
             << title.position()->y_permille << "\n";
    }
  }

  for (const auto& subtitle : snapshot.subtitles) {
    output << "SUBTITLE " << subtitle.sequence << ' ';
    append_time(output, subtitle.start);
    output << ' ';
    append_time(output, subtitle.end);
    output << ' ' << quote(subtitle.text) << "\n";
  }

  output << "ALLOCATOR " << snapshot.allocator_state.next_media_id << ' '
         << snapshot.allocator_state.next_clip_id << ' '
         << snapshot.allocator_state.next_timeline_id << ' '
         << snapshot.allocator_state.next_track_id;
  if (has_titles) output << ' ' << snapshot.allocator_state.next_title_id;
  output << "\nEND\n";
  const std::string encoded = output.str();
  std::size_t offset = 0;
  while (offset < encoded.size()) {
    const auto newline = encoded.find('\n', offset);
    const auto length = (newline == std::string::npos ? encoded.size() : newline) - offset;
    if (length > max_line_size) throw ProjectCodecError{"encoded line exceeds size limit"};
    if (newline == std::string::npos) break;
    offset = newline + 1;
  }
  return encoded;
}

ProjectRestoreState ProjectCodec::decode(std::string_view bytes) {
  ProjectRestoreState state{domain::TimelineId{1}, {}, domain::TrackId{1}, {}, {}};
  bool header = false;
  bool timeline = false;
  bool video_track = false;
  bool version6 = false;
  bool version7 = false;
  bool version8 = false;
  bool version9 = false;
  bool version10 = false;
  bool version11 = false;
  bool version12 = false;
  bool version13 = false;
  bool version14 = false;
  bool allocator = false;
  bool ended = false;
  enum class Section { Header, Timeline, Tracks, VideoTrack, VisualTracks, Assets, Clips, Transitions, Titles, Subtitles, Allocator, End };
  Section section = Section::Header;
  std::size_t line_number = 0;
  std::size_t records = 0;
  std::size_t offset = 0;

  while (offset <= bytes.size()) {
    const std::size_t newline = bytes.find('\n', offset);
    const bool last = newline == std::string_view::npos;
    const std::string_view line = bytes.substr(offset, last ? bytes.size() - offset : newline - offset);
    offset = last ? bytes.size() + 1 : newline + 1;
    ++line_number;
    if (line.empty()) {
      if (ended) {
        if (last) break;
        continue;
      }
      parse_fail(line_number, "blank lines are not allowed");
    }
    if (++records > max_records) parse_fail(line_number, "record limit exceeded");
    const auto tokens = tokenize(line, line_number);
    if (tokens.empty()) {
      if (ended) {
        if (last) break;
        continue;
      }
      parse_fail(line_number, "empty record");
    }
    if (ended) parse_fail(line_number, "data after END");

    try {
      if (tokens[0].text == "PULSAR_PROJECT") {
        if (header || tokens.size() != 2 || (tokens[1].text != "1" && tokens[1].text != "2" && tokens[1].text != "3" && tokens[1].text != "4" && tokens[1].text != "5" && tokens[1].text != "6" && tokens[1].text != "7" && tokens[1].text != "8" && tokens[1].text != "9" && tokens[1].text != "10" && tokens[1].text != "11" && tokens[1].text != "12" && tokens[1].text != "13" && tokens[1].text != "14")) {
          parse_fail(line_number, "invalid header/version");
        }
        header = true;
        version14 = tokens[1].text == "14";
        version13 = tokens[1].text == "13" || version14;
        version7 = tokens[1].text == "7" || tokens[1].text == "8" || tokens[1].text == "9" || tokens[1].text == "10" || tokens[1].text == "11" || tokens[1].text == "12" || tokens[1].text == "13" || version14;
        version8 = tokens[1].text == "8" || tokens[1].text == "9" || tokens[1].text == "10" || tokens[1].text == "11" || tokens[1].text == "12" || tokens[1].text == "13" || version14;
        version9 = tokens[1].text == "9" || tokens[1].text == "10" || tokens[1].text == "11" || tokens[1].text == "12" || tokens[1].text == "13" || version14;
        version10 = tokens[1].text == "10" || tokens[1].text == "11" || tokens[1].text == "12" || tokens[1].text == "13" || version14;
        version11 = tokens[1].text == "11" || tokens[1].text == "12" || tokens[1].text == "13" || version14;
        version12 = tokens[1].text == "12" || tokens[1].text == "13" || version14;
        version6 = version7 || tokens[1].text == "6";
        section = Section::Timeline;
      } else if (!header) {
        parse_fail(line_number, "header required");
      } else if (tokens[0].text == "TIMELINE") {
        if (timeline || section != Section::Timeline || tokens.size() != 2) parse_fail(line_number, "invalid TIMELINE");
        state.timeline_id = domain::TimelineId{unsigned_number(tokens[1], line_number, "timeline id")};
        timeline = true;
        section = Section::Tracks;
      } else if (tokens[0].text == "TRACK") {
        if (!timeline || (section != Section::Tracks && section != Section::VideoTrack) ||
            (tokens.size() != 3 && (!version6 || tokens.size() != 7 || tokens[3].text != "TRACK_AUDIO"))) {
          parse_fail(line_number, "invalid TRACK");
        }
        const auto id = domain::TrackId{unsigned_number(tokens[1], line_number, "track id")};
        domain::TrackKind kind;
        if (tokens[2].text == "VIDEO") kind = domain::TrackKind::Video;
        else if (tokens[2].text == "AUDIO") kind = domain::TrackKind::Audio;
        else parse_fail(line_number, "invalid track kind");
        domain::AudioTrackState audio_state;
        if (tokens.size() == 7) {
          audio_state.gain_db = double_number(tokens[4], line_number, "track audio gain");
          const auto muted = unsigned_number(tokens[5], line_number, "track mute");
          const auto solo = unsigned_number(tokens[6], line_number, "track solo");
          if (muted > 1 || solo > 1) parse_fail(line_number, "invalid track audio flags");
          audio_state.muted = muted != 0;
          audio_state.solo = solo != 0;
          domain::validate_audio_track_state(audio_state);
        }
        state.tracks.push_back(ProjectRestoreTrack{id, kind, {}, audio_state});
        section = Section::Tracks;
      } else if (tokens[0].text == "VIDEO_TRACK") {
        if (video_track || state.tracks.empty() || (section != Section::Tracks && section != Section::VideoTrack) || tokens.size() != 2) parse_fail(line_number, "invalid VIDEO_TRACK");
        state.video_track_id = domain::TrackId{unsigned_number(tokens[1], line_number, "video track id")};
        video_track = true;
        state.visual_track_ids = {state.video_track_id};
        section = version6 ? Section::VisualTracks : Section::Assets;
      } else if (tokens[0].text == "VISUAL_TRACKS") {
        if (!version6 || section != Section::VisualTracks || tokens.size() < 2) {
          parse_fail(line_number, "invalid VISUAL_TRACKS");
        }
        state.visual_track_ids.clear();
        for (std::size_t i = 1; i < tokens.size(); ++i) {
          state.visual_track_ids.emplace_back(unsigned_number(tokens[i], line_number, "visual track id"));
        }
        section = Section::Assets;
      } else if (tokens[0].text == "CANVAS") {
        if (!version6 || section != Section::Assets || state.canvas ||
            (tokens.size() != 5 && tokens.size() != 6) ||
            (tokens.size() == 6 && tokens[5].text != "STILL")) {
          parse_fail(line_number, "invalid CANVAS");
        }
        const auto width = unsigned_number(tokens[1], line_number, "canvas width");
        const auto height = unsigned_number(tokens[2], line_number, "canvas height");
        const auto numerator = unsigned_number(tokens[3], line_number, "canvas fps numerator");
        const auto denominator = unsigned_number(tokens[4], line_number, "canvas fps denominator");
        if (width == 0 || height == 0 || width > 32768 || height > 32768 ||
            width * height > 64'000'000 ||
            numerator == 0 || denominator == 0 || numerator > INT32_MAX || denominator > INT32_MAX) {
          parse_fail(line_number, "invalid canvas geometry or frame rate");
        }
        state.canvas = ProjectCanvas{static_cast<int>(width), static_cast<int>(height),
            domain::FrameRate{static_cast<std::int64_t>(numerator), static_cast<std::int64_t>(denominator)},
            tokens.size() == 6};
      } else if (tokens[0].text == "ASSET") {
        if (!video_track || (section != Section::Assets) || tokens.size() < 4) parse_fail(line_number, "invalid ASSET");
        if (tokens.size() >= 3 && tokens[2].text == "AUDIO_ONLY") {
          std::size_t i = 1;
          const auto id = domain::MediaId{unsigned_number(tokens[i++], line_number, "media id")};
          ++i; // AUDIO_ONLY
          const auto duration = media_time(tokens, i, line_number, "duration");
          if (i >= tokens.size() || !tokens[i].quoted) parse_fail(line_number, "audio asset locator must be quoted");
          const std::filesystem::path locator{tokens[i++].text};
          if (i >= tokens.size() || tokens[i].quoted || tokens[i].text != "AUDIO") {
            parse_fail(line_number, "audio-only asset requires AUDIO metadata");
          }
          ++i;
          const int audio_stream = int_number(tokens[i++], line_number, "audio stream index");
          const int sample_rate = int_number(tokens[i++], line_number, "audio sample rate");
          const int channel_count = int_number(tokens[i++], line_number, "audio channel count");
          const auto audio_tb_num = signed_number(tokens[i++], line_number, "audio time base numerator");
          const auto audio_tb_den = signed_number(tokens[i++], line_number, "audio time base denominator");
          if (audio_stream < 0 || sample_rate <= 0 || channel_count <= 0) {
            parse_fail(line_number, "invalid audio-only asset metadata");
          }
          std::optional<domain::MediaTime> audio_duration;
          if (i < tokens.size() && !tokens[i].quoted && tokens[i].text == "-") ++i;
          else audio_duration = media_time(tokens, i, line_number, "audio duration");
          if (i != tokens.size()) parse_fail(line_number, "extra audio-only asset fields");
          try {
            state.assets.emplace_back(MediaAsset::audio_only(
                id, locator, duration,
                AudioAssetMetadata{audio_stream, sample_rate, channel_count,
                                   domain::Rational{audio_tb_num, audio_tb_den}, audio_duration}));
          } catch (const std::exception& error) {
            parse_fail(line_number, error.what());
          }
          section = Section::Assets;
          continue;
        }
        if (tokens.size() < 14) parse_fail(line_number, "invalid ASSET");
        std::size_t i = 1;
        const auto id = domain::MediaId{unsigned_number(tokens[i++], line_number, "media id")};
        const int stream = int_number(tokens[i++], line_number, "stream index");
        const auto duration = media_time(tokens, i, line_number, "duration");
        if (i + 7 >= tokens.size() || !tokens[i].quoted || !tokens[i + 1].quoted) parse_fail(line_number, "asset strings must be quoted");
        const std::filesystem::path locator{tokens[i++].text};
        const std::string codec = tokens[i++].text;
        const int width = int_number(tokens[i++], line_number, "width");
        const int height = int_number(tokens[i++], line_number, "height");
        const auto tb_num = signed_number(tokens[i++], line_number, "time base numerator");
        const auto tb_den = signed_number(tokens[i++], line_number, "time base denominator");
        domain::Rational time_base{tb_num, tb_den};
        std::optional<domain::FrameRate> fps;
        if (tokens[i].quoted || tokens[i].text != "-") {
          const auto fps_num = signed_number(tokens[i++], line_number, "fps numerator");
          if (i >= tokens.size()) parse_fail(line_number, "missing fps denominator");
          const auto fps_den = signed_number(tokens[i++], line_number, "fps denominator");
          fps = domain::FrameRate{fps_num, fps_den};
        } else {
          ++i;
        }
        std::optional<std::string> container;
        if (i >= tokens.size() || (tokens[i].text != "-" && !tokens[i].quoted)) parse_fail(line_number, "invalid container");
        if (tokens[i].quoted || tokens[i].text != "-") container = tokens[i].text;
        ++i;
        // A0 audio suffix: AUDIO <stream> <sample_rate> <channels> <tb_num>
        // <tb_den> (<dur_val> <dur_num> <dur_den> | -). Absent suffix means
        // no audio binding; old v1 lines end exactly here.
        std::optional<AudioAssetMetadata> audio;
        if (i != tokens.size()) {
          if (i + 6 >= tokens.size() || tokens[i].quoted || tokens[i].text != "AUDIO") {
            parse_fail(line_number, "invalid ASSET audio binding");
          }
          ++i;
          const int audio_stream = int_number(tokens[i++], line_number, "audio stream index");
          const int sample_rate = int_number(tokens[i++], line_number, "audio sample rate");
          const int channel_count = int_number(tokens[i++], line_number, "audio channel count");
          const auto audio_tb_num =
              signed_number(tokens[i++], line_number, "audio time base numerator");
          const auto audio_tb_den =
              signed_number(tokens[i++], line_number, "audio time base denominator");
          if (audio_stream < 0 || sample_rate <= 0 || channel_count <= 0) {
            parse_fail(line_number, "invalid ASSET audio binding");
          }
          domain::Rational audio_time_base{audio_tb_num, audio_tb_den};
          std::optional<domain::MediaTime> audio_duration;
          if (i < tokens.size() && !tokens[i].quoted && tokens[i].text == "-") {
            ++i;
          } else {
            audio_duration = media_time(tokens, i, line_number, "audio duration");
          }
          if (i != tokens.size()) parse_fail(line_number, "extra ASSET fields");
          audio = AudioAssetMetadata{audio_stream, sample_rate, channel_count, audio_time_base,
                                     audio_duration};
        }
        state.assets.emplace_back(id, locator, stream, duration,
                                  VideoAssetMetadata{codec, width, height, time_base, fps, container},
                                  MediaAvailability::Available, std::nullopt, std::move(audio));
        section = Section::Assets;
      } else if (tokens[0].text == "CLIP") {
        if (!video_track || (section != Section::Assets && section != Section::Clips) ||
            tokens.size() < 13)
          parse_fail(line_number, "invalid CLIP");
        std::size_t i = 1;
        const auto clip_id = domain::ClipId{unsigned_number(tokens[i++], line_number, "clip id")};
        const auto track_id = domain::TrackId{unsigned_number(tokens[i++], line_number, "clip track id")};
        const auto media_id = domain::MediaId{unsigned_number(tokens[i++], line_number, "clip media id")};
        const auto source_start = media_time(tokens, i, line_number, "source start");
        const auto source_end = media_time(tokens, i, line_number, "source end");
        const auto timeline_start = media_time(tokens, i, line_number, "timeline start");
        double audio_gain_db = 0.0;
        bool muted = false;
        auto fade_in = domain::MediaTime::zero();
        auto fade_out = domain::MediaTime::zero();
        domain::VisualTransform visual_transform;
        bool saw_audio = false;
        bool saw_fade = false;
        bool saw_transform = false;
        domain::DistantMusicSettings distant_music;
        bool saw_distant_music = false;
        domain::TelephoneSettings telephone;
        bool saw_telephone = false;
        domain::UnderwaterSettings underwater;
        bool saw_underwater = false;
        std::vector<domain::AudioEffectType> audio_effect_order;
        bool saw_audio_effect_order = false;
        while (i < tokens.size()) {
          if (!tokens[i].quoted && tokens[i].text == "CLIP_AUDIO" && !saw_audio) {
            saw_audio = true;
            if (i + 2 >= tokens.size()) parse_fail(line_number, "invalid CLIP audio properties");
            ++i;
            audio_gain_db = double_number(tokens[i++], line_number, "clip audio gain");
            const auto mute_value = unsigned_number(tokens[i++], line_number, "clip mute");
            if (mute_value > 1) parse_fail(line_number, "invalid clip mute");
            muted = mute_value != 0;
          } else if (!tokens[i].quoted && tokens[i].text == "CLIP_FADE" && !saw_fade) {
            saw_fade = true;
            ++i;
            fade_in = media_time(tokens, i, line_number, "clip fade in");
            fade_out = media_time(tokens, i, line_number, "clip fade out");
          } else if (version6 && !tokens[i].quoted && tokens[i].text == "CLIP_TRANSFORM" && !saw_transform) {
            saw_transform = true;
            if (i + 5 >= tokens.size()) parse_fail(line_number, "invalid CLIP transform");
            ++i;
            visual_transform.position_x = double_number(tokens[i++], line_number, "clip position x");
            visual_transform.position_y = double_number(tokens[i++], line_number, "clip position y");
            visual_transform.scale = double_number(tokens[i++], line_number, "clip scale");
            visual_transform.rotation_degrees = double_number(tokens[i++], line_number, "clip rotation");
            visual_transform.opacity = double_number(tokens[i++], line_number, "clip opacity");
          } else if (version7 && !tokens[i].quoted && tokens[i].text == "CLIP_DISTANT_MUSIC" && !saw_distant_music) {
            saw_distant_music = true;
            if (i + 6 >= tokens.size()) parse_fail(line_number, "invalid CLIP_DISTANT_MUSIC");
            ++i;
            const auto enabled = unsigned_number(tokens[i++], line_number, "Distant Music enabled");
            if (enabled > 1) parse_fail(line_number, "invalid Distant Music enabled");
            distant_music.enabled = enabled != 0;
            distant_music.initial_gain_db = double_number(tokens[i++], line_number, "Distant Music gain");
            distant_music.duration_seconds = double_number(tokens[i++], line_number, "Distant Music duration");
            distant_music.start_cutoff_hz = double_number(tokens[i++], line_number, "Distant Music start cutoff");
            distant_music.end_cutoff_hz = double_number(tokens[i++], line_number, "Distant Music end cutoff");
            distant_music.reverb_amount = double_number(tokens[i++], line_number, "Distant Music reverb");
            const auto curve = unsigned_number(tokens[i++], line_number, "Distant Music curve");
            if (curve > 2) parse_fail(line_number, "invalid Distant Music curve");
            distant_music.curve = static_cast<domain::DistantMusicCurve>(curve);
            if (version8 && i < tokens.size() && is_unsigned_token(tokens[i])) {
              const auto count = unsigned_number(tokens[i++], line_number,
                                                 "Distant Music keyframe count");
              if (count > 128 || i + count * 3 > tokens.size()) {
                parse_fail(line_number, "invalid Distant Music keyframes");
              }
              distant_music.gain_keyframes.reserve(static_cast<std::size_t>(count));
              for (std::uint64_t keyframe = 0; keyframe < count; ++keyframe) {
                domain::ScalarKeyframe value;
                value.time_seconds = double_number(tokens[i++], line_number,
                                                   "Distant Music keyframe time");
                value.value = double_number(tokens[i++], line_number,
                                            "Distant Music keyframe value");
                const auto interpolation = unsigned_number(
                    tokens[i++], line_number, "Distant Music keyframe interpolation");
                if (interpolation > 2) {
                  parse_fail(line_number, "invalid Distant Music keyframe interpolation");
                }
                value.interpolation = static_cast<domain::KeyframeInterpolation>(interpolation);
                distant_music.gain_keyframes.push_back(value);
              }
            }
          } else if (version10 && !tokens[i].quoted && tokens[i].text == "CLIP_TELEPHONE" && !saw_telephone) {
            saw_telephone = true;
            if (i + 3 >= tokens.size()) parse_fail(line_number, "invalid CLIP_TELEPHONE");
            ++i;
            const auto enabled = unsigned_number(tokens[i++], line_number, "Telephone enabled");
            if (enabled > 1) parse_fail(line_number, "invalid Telephone enabled");
            telephone.enabled = enabled != 0;
            telephone.mix = double_number(tokens[i++], line_number, "Telephone mix");
            telephone.drive = double_number(tokens[i++], line_number, "Telephone drive");
            if (version14 && i < tokens.size() && is_unsigned_token(tokens[i])) {
              const auto count = unsigned_number(tokens[i++], line_number, "Telephone mix keyframe count");
              if (count > 128 || i + count * 3 > tokens.size()) {
                parse_fail(line_number, "invalid Telephone mix keyframes");
              }
              telephone.mix_keyframes.reserve(static_cast<std::size_t>(count));
              for (std::uint64_t keyframe = 0; keyframe < count; ++keyframe) {
                domain::ScalarKeyframe value;
                value.time_seconds = double_number(tokens[i++], line_number, "Telephone mix keyframe time");
                value.value = double_number(tokens[i++], line_number, "Telephone mix keyframe value");
                const auto interpolation = unsigned_number(tokens[i++], line_number,
                                                            "Telephone mix keyframe interpolation");
                if (interpolation > 2) parse_fail(line_number, "invalid Telephone mix keyframe interpolation");
                value.interpolation = static_cast<domain::KeyframeInterpolation>(interpolation);
                telephone.mix_keyframes.push_back(value);
              }
            }
            try { telephone.validate(); } catch (const std::exception& error) { parse_fail(line_number, error.what()); }
          } else if (version11 && !tokens[i].quoted && tokens[i].text == "CLIP_UNDERWATER" && !saw_underwater) {
            saw_underwater = true;
            if (i + 3 >= tokens.size()) parse_fail(line_number, "invalid CLIP_UNDERWATER");
            ++i;
            const auto enabled = unsigned_number(tokens[i++], line_number, "Underwater enabled");
            if (enabled > 1) parse_fail(line_number, "invalid Underwater enabled");
            underwater.enabled = enabled != 0;
            underwater.mix = double_number(tokens[i++], line_number, "Underwater mix");
            underwater.depth = double_number(tokens[i++], line_number, "Underwater depth");
            if (version12 && i < tokens.size() && is_unsigned_token(tokens[i])) {
              const auto count = unsigned_number(tokens[i++], line_number,
                                                 "Underwater depth keyframe count");
              if (count > 128 || i + count * 3 > tokens.size()) {
                parse_fail(line_number, "invalid Underwater depth keyframes");
              }
              underwater.depth_keyframes.reserve(static_cast<std::size_t>(count));
              for (std::uint64_t keyframe = 0; keyframe < count; ++keyframe) {
                domain::ScalarKeyframe value;
                value.time_seconds = double_number(tokens[i++], line_number,
                                                   "Underwater depth keyframe time");
                value.value = double_number(tokens[i++], line_number,
                                            "Underwater depth keyframe value");
                const auto interpolation = unsigned_number(
                    tokens[i++], line_number, "Underwater depth keyframe interpolation");
                if (interpolation > 2) {
                  parse_fail(line_number, "invalid Underwater depth keyframe interpolation");
                }
                value.interpolation = static_cast<domain::KeyframeInterpolation>(interpolation);
                underwater.depth_keyframes.push_back(value);
              }
            }
            try { underwater.validate(); } catch (const std::exception& error) { parse_fail(line_number, error.what()); }
          } else if (version13 && !tokens[i].quoted && tokens[i].text == "CLIP_FX_ORDER" && !saw_audio_effect_order) {
            saw_audio_effect_order = true;
            if (i + 3 >= tokens.size()) parse_fail(line_number, "invalid CLIP_FX_ORDER");
            ++i;
            for (std::size_t effect = 0; effect < 3; ++effect) {
              const auto type = unsigned_number(tokens[i++], line_number, "audio effect type");
              if (type > 2) parse_fail(line_number, "invalid audio effect type");
              audio_effect_order.push_back(static_cast<domain::AudioEffectType>(type));
            }
            try {
              domain::validate_audio_effect_order(audio_effect_order);
            } catch (const std::exception& error) {
              parse_fail(line_number, error.what());
            }
          } else {
            parse_fail(line_number, "invalid or duplicate CLIP properties");
          }
        }
        if (i != tokens.size()) parse_fail(line_number, "extra CLIP fields");
        const auto track = std::find_if(state.tracks.begin(), state.tracks.end(),
                                        [track_id](const auto& item) { return item.id == track_id; });
        if (track == state.tracks.end()) parse_fail(line_number, "unknown clip track id");
        if (std::none_of(state.assets.begin(), state.assets.end(),
                         [media_id](const auto& item) { return item.id() == media_id; })) {
          parse_fail(line_number, "unknown clip media id");
        }
        try {
          track->clips.emplace_back(clip_id, media_id,
                                    domain::TimeRange{source_start, source_end}, timeline_start,
                                    audio_gain_db, muted, fade_in, fade_out, visual_transform, distant_music,
                                    telephone, underwater, std::move(audio_effect_order));
        } catch (const std::exception& error) {
          parse_fail(line_number, error.what());
        }
        section = Section::Clips;
      } else if (tokens[0].text == "TRANSITION") {
        if (!version6 || (section != Section::Clips && section != Section::Transitions) ||
            tokens.size() != 8 || tokens[1].text != "CROSS_DISSOLVE") {
          parse_fail(line_number, "invalid TRANSITION");
        }
        std::size_t i = 2;
        const auto track_id = domain::TrackId{unsigned_number(tokens[i++], line_number, "transition track id")};
        const auto outgoing = domain::ClipId{unsigned_number(tokens[i++], line_number, "transition outgoing clip id")};
        const auto incoming = domain::ClipId{unsigned_number(tokens[i++], line_number, "transition incoming clip id")};
        const auto duration = media_time(tokens, i, line_number, "transition duration");
        try {
          domain::Transition value{track_id, outgoing, incoming,
                                   domain::TransitionType::CrossDissolve, duration};
          domain::validate_transition_value(value);
          state.transitions.push_back(value);
        } catch (const std::exception& error) {
          parse_fail(line_number, error.what());
        }
        section = Section::Transitions;
      } else if (tokens[0].text == "TITLE") {
        if (!video_track || (section != Section::Assets && section != Section::Clips &&
                             section != Section::Transitions && section != Section::Titles) || tokens.size() != 14)
          parse_fail(line_number, "invalid TITLE");
        std::size_t i = 1;
        const auto title_id = domain::TitleId{unsigned_number(tokens[i++], line_number, "title id")};
        const auto start = media_time(tokens, i, line_number, "title start");
        const auto end = media_time(tokens, i, line_number, "title end");
        const auto size = int_number(tokens[i++], line_number, "title size");
        const auto color = unsigned_number(tokens[i++], line_number, "title colour");
        const auto horizontal = unsigned_number(tokens[i++], line_number, "title horizontal alignment");
        const auto vertical = unsigned_number(tokens[i++], line_number, "title vertical alignment");
        const auto shadow = unsigned_number(tokens[i++], line_number, "title shadow");
        if (color > 0xFFFFFFFFull || horizontal > 2 || vertical > 2 || shadow > 1) parse_fail(line_number, "invalid TITLE value");
        if (i + 1 != tokens.size() || !tokens[i].quoted) parse_fail(line_number, "title text must be quoted");
        try {
          state.titles.emplace_back(title_id, tokens[i].text, domain::TimeRange{start, end}, size,
                                    static_cast<std::uint32_t>(color), static_cast<domain::TitleHAlign>(horizontal),
                                    static_cast<domain::TitleVAlign>(vertical), shadow == 1);
        } catch (const std::exception& error) {
          parse_fail(line_number, error.what());
        }
        section = Section::Titles;
      } else if (tokens[0].text == "TITLE_LOOK") {
        if (section != Section::Titles || tokens.size() != 4) parse_fail(line_number, "invalid TITLE_LOOK");
        const auto title_id = domain::TitleId{unsigned_number(tokens[1], line_number, "title id")};
        const auto background = unsigned_number(tokens[2], line_number, "title background");
        const auto opacity = int_number(tokens[3], line_number, "title opacity");
        const auto found = std::find_if(state.titles.begin(), state.titles.end(),
                                        [title_id](const auto& item) { return item.id() == title_id; });
        if (found == state.titles.end() || background > 2) parse_fail(line_number, "invalid TITLE_LOOK value");
        try {
          *found = found->with_look(static_cast<domain::TitleBackground>(background), opacity);
        } catch (const std::exception& error) {
          parse_fail(line_number, error.what());
        }
      } else if (tokens[0].text == "TITLE_POS") {
        if (section != Section::Titles || tokens.size() != 4) parse_fail(line_number, "invalid TITLE_POS");
        const auto title_id = domain::TitleId{unsigned_number(tokens[1], line_number, "title id")};
        const auto x = int_number(tokens[2], line_number, "title x");
        const auto y = int_number(tokens[3], line_number, "title y");
        const auto found = std::find_if(state.titles.begin(), state.titles.end(),
                                        [title_id](const auto& item) { return item.id() == title_id; });
        if (found == state.titles.end()) parse_fail(line_number, "invalid TITLE_POS value");
        try {
          *found = found->with_position(domain::TitlePosition{x, y});
        } catch (const std::exception& error) {
          parse_fail(line_number, error.what());
        }
      } else if (tokens[0].text == "SUBTITLE") {
        if (!version9 || (section != Section::Titles && section != Section::Subtitles) || tokens.size() != 9 || !tokens[8].quoted) {
          parse_fail(line_number, "invalid SUBTITLE");
        }
        std::size_t i = 1;
        const auto sequence = unsigned_number(tokens[i++], line_number, "subtitle sequence");
        if (sequence == 0 || sequence > std::numeric_limits<std::uint32_t>::max()) {
          parse_fail(line_number, "invalid subtitle sequence");
        }
        const auto start = media_time(tokens, i, line_number, "subtitle start");
        const auto end = media_time(tokens, i, line_number, "subtitle end");
        try {
          state.subtitles.push_back(SubtitleCue{static_cast<std::uint32_t>(sequence), start, end, tokens[i].text});
        } catch (const std::exception& error) {
          parse_fail(line_number, error.what());
        }
        section = Section::Subtitles;
      } else if (tokens[0].text == "ALLOCATOR") {
        if (allocator || !video_track || (section != Section::Clips && section != Section::Assets &&
            section != Section::Transitions && section != Section::Titles && section != Section::Subtitles) ||
            (tokens.size() != 5 && tokens.size() != 6)) parse_fail(line_number, "invalid ALLOCATOR");
        if (tokens.size() == 6) state.allocator_state.next_title_id = unsigned_number(tokens[5], line_number, "next title id");
        state.allocator_state.next_media_id = unsigned_number(tokens[1], line_number, "next media id");
        state.allocator_state.next_clip_id = unsigned_number(tokens[2], line_number, "next clip id");
        state.allocator_state.next_timeline_id = unsigned_number(tokens[3], line_number, "next timeline id");
        state.allocator_state.next_track_id = unsigned_number(tokens[4], line_number, "next track id");
        allocator = true;
        section = Section::Allocator;
      } else if (tokens[0].text == "END") {
        if (!header || !timeline || !video_track || !allocator || tokens.size() != 1 || section != Section::Allocator) parse_fail(line_number, "incomplete project");
        ended = true;
        section = Section::End;
      } else {
        parse_fail(line_number, "unknown record");
      }
    } catch (const ProjectCodecError&) {
      throw;
    } catch (const std::exception& error) {
      parse_fail(line_number, error.what());
    }
    if (last) break;
  }

  if (!ended) throw ProjectCodecError{"missing END record"};
  domain::Timeline visual_timeline{state.timeline_id};
  try {
    for (const auto& track : state.tracks) {
      visual_timeline.add_track(domain::Track{track.id, track.kind});
    }
    validate_visual_track_ids(visual_timeline, state.video_track_id, state.visual_track_ids);
  } catch (const std::invalid_argument& error) {
    throw ProjectCodecError{error.what()};
  }
  return state;
}

} // namespace pulsar::application
