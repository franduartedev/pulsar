#pragma once

extern "C" {
struct AVFormatContext;
}

namespace pulsar::media::ffmpeg::detail {

using ExportWriteHeaderHook = int (*)(AVFormatContext*);

// Test-only fault seam. The default is null and the production path calls
// avformat_write_header() directly; tests may replace it temporarily.
[[nodiscard]] ExportWriteHeaderHook& export_write_header_hook() noexcept;

} // namespace pulsar::media::ffmpeg::detail
