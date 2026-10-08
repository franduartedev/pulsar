#include <pulsar/application/project/project_codec.hpp>

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
  const std::size_t record_count = 1 + 1 + snapshot.timeline.tracks().size() + 1 +
                                   snapshot.assets.size() + snapshot.titles.size();
  std::size_t clip_count = 0;
  for (const auto& track : snapshot.timeline.tracks()) clip_count += track.clips().size();
  if (record_count + clip_count + 2 > max_records) {
    throw ProjectCodecError{"project exceeds record limit"};
  }
  std::ostringstream output;
  // Files without titles stay byte-compatible version 2; titles need version 3.
  const bool has_titles = !snapshot.titles.empty() || snapshot.allocator_state.next_title_id != 1;
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
  output << (needs_v5 ? "PULSAR_PROJECT 5\n" : needs_v4 ? "PULSAR_PROJECT 4\n" : has_titles ? "PULSAR_PROJECT 3\n" : "PULSAR_PROJECT 2\n");
  output << "TIMELINE " << snapshot.timeline.id().value() << "\n";
  const auto tracks = sorted_tracks(snapshot.timeline);
  for (const auto* track : tracks) {
    output << "TRACK " << track->id().value() << ' '
           << (track->kind() == domain::TrackKind::Video ? "VIDEO" : "AUDIO") << "\n";
  }
  output << "VIDEO_TRACK " << snapshot.video_track_id.value() << "\n";

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
      output << "\n";
    }
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
  bool allocator = false;
  bool ended = false;
  enum class Section { Header, Timeline, Tracks, VideoTrack, Assets, Clips, Titles, Allocator, End };
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
        if (header || tokens.size() != 2 || (tokens[1].text != "1" && tokens[1].text != "2" && tokens[1].text != "3" && tokens[1].text != "4" && tokens[1].text != "5")) {
          parse_fail(line_number, "invalid header/version");
        }
        header = true;
        section = Section::Timeline;
      } else if (!header) {
        parse_fail(line_number, "header required");
      } else if (tokens[0].text == "TIMELINE") {
        if (timeline || section != Section::Timeline || tokens.size() != 2) parse_fail(line_number, "invalid TIMELINE");
        state.timeline_id = domain::TimelineId{unsigned_number(tokens[1], line_number, "timeline id")};
        timeline = true;
        section = Section::Tracks;
      } else if (tokens[0].text == "TRACK") {
        if (!timeline || (section != Section::Tracks && section != Section::VideoTrack) || tokens.size() != 3) parse_fail(line_number, "invalid TRACK");
        const auto id = domain::TrackId{unsigned_number(tokens[1], line_number, "track id")};
        domain::TrackKind kind;
        if (tokens[2].text == "VIDEO") kind = domain::TrackKind::Video;
        else if (tokens[2].text == "AUDIO") kind = domain::TrackKind::Audio;
        else parse_fail(line_number, "invalid track kind");
        state.tracks.push_back(ProjectRestoreTrack{id, kind, {}});
        section = Section::Tracks;
      } else if (tokens[0].text == "VIDEO_TRACK") {
        if (video_track || state.tracks.empty() || (section != Section::Tracks && section != Section::VideoTrack) || tokens.size() != 2) parse_fail(line_number, "invalid VIDEO_TRACK");
        state.video_track_id = domain::TrackId{unsigned_number(tokens[1], line_number, "video track id")};
        video_track = true;
        section = Section::Assets;
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
            (tokens.size() != 13 && tokens.size() != 16 && tokens.size() != 23))
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
        if (tokens.size() >= 16) {
          if (tokens[i].quoted || tokens[i].text != "CLIP_AUDIO") {
            parse_fail(line_number, "invalid CLIP audio properties");
          }
          ++i;
          audio_gain_db = double_number(tokens[i++], line_number, "clip audio gain");
          const auto mute_value = unsigned_number(tokens[i++], line_number, "clip mute");
          if (mute_value > 1) parse_fail(line_number, "invalid clip mute");
          muted = mute_value != 0;
        }
        if (tokens.size() == 23) {
          if (tokens[i].quoted || tokens[i].text != "CLIP_FADE") parse_fail(line_number, "invalid CLIP fade properties");
          ++i;
          fade_in = media_time(tokens, i, line_number, "clip fade in");
          fade_out = media_time(tokens, i, line_number, "clip fade out");
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
                                   audio_gain_db, muted, fade_in, fade_out);
        } catch (const std::exception& error) {
          parse_fail(line_number, error.what());
        }
        section = Section::Clips;
      } else if (tokens[0].text == "TITLE") {
        if (!video_track || (section != Section::Assets && section != Section::Clips && section != Section::Titles) || tokens.size() != 14)
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
      } else if (tokens[0].text == "ALLOCATOR") {
        if (allocator || !video_track || (section != Section::Clips && section != Section::Assets && section != Section::Titles) ||
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
  return state;
}

} // namespace pulsar::application
