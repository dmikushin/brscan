#include <gtest/gtest.h>

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace {

constexpr int kWidth = 1648;
constexpr int kExpectedTriples = 2290;

std::vector<uint8_t> ReadFile(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return {};
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), {});
}

uint16_t Le16(const std::vector<uint8_t>& data, size_t pos) {
  return static_cast<uint16_t>(data[pos] | (data[pos + 1] << 8));
}

int Clamp(int v) {
  if (v < 0) return 0;
  if (v > 255) return 255;
  return v;
}

struct Frame {
  uint8_t header;
  std::vector<uint8_t> payload;
};

std::vector<uint8_t> ReplayPayloads(const std::vector<uint8_t>& capture) {
  std::vector<uint8_t> stream;
  for (size_t pos = 0; pos + 4 <= capture.size();) {
    int32_t rc = static_cast<int32_t>(capture[pos] |
                                      (capture[pos + 1] << 8) |
                                      (capture[pos + 2] << 16) |
                                      (capture[pos + 3] << 24));
    pos += 4;
    if (rc > 0) {
      if (pos + static_cast<size_t>(rc) > capture.size()) break;
      stream.insert(stream.end(), capture.begin() + pos, capture.begin() + pos + rc);
      pos += static_cast<size_t>(rc);
    }
  }
  return stream;
}

std::vector<Frame> ParseColorFrames(const std::vector<uint8_t>& stream) {
  std::vector<Frame> frames;
  size_t pos = 0;
  for (; pos + 3 <= stream.size(); pos++) {
    if (stream[pos] == 0x44 && stream[pos + 1] == 0x07 && stream[pos + 2] == 0x00)
      break;
  }
  while (pos + 3 <= stream.size()) {
    uint8_t header = stream[pos];
    if (header == 0x80 || header > 0xC1) {
      pos++;
      continue;
    }

    uint16_t wrapper_len = Le16(stream, pos + 1);
    size_t length_pos = pos + 3 + wrapper_len;
    if (length_pos + 2 > stream.size()) break;

    uint16_t payload_len = Le16(stream, length_pos);
    size_t total = 3 + wrapper_len + 2 + payload_len;
    if (total < 5 || pos + total > stream.size()) break;

    if ((header == 0x44 || header == 0x48 || header == 0x4c) && payload_len >= kWidth) {
      frames.push_back({header, std::vector<uint8_t>(stream.begin() + length_pos + 2,
                                                     stream.begin() + length_pos + 2 + payload_len)});
    }
    pos += total;
  }
  return frames;
}

}  // namespace

TEST(Brscan4ColorReference, CapturedColorStreamUsesCrYCbPlaneTriples) {
  const auto capture = ReadFile(std::string(BRSCAN_SOURCE_DIR) + "/tests/data/dcp1510_a4_color_200dpi.bin");
  ASSERT_FALSE(capture.empty());
  EXPECT_EQ(capture.size(), 11410881u);

  const auto frames = ParseColorFrames(ReplayPayloads(capture));
  ASSERT_GE(frames.size(), static_cast<size_t>(kExpectedTriples * 3));

  int triples = 0;
  uint64_t r_sum = 0;
  uint64_t g_sum = 0;
  uint64_t b_sum = 0;
  uint8_t r_min = 255, g_min = 255, b_min = 255;
  uint8_t r_max = 0, g_max = 0, b_max = 0;

  for (size_t i = 0; i + 2 < frames.size();) {
    if (frames[i].header != 0x44 || frames[i + 1].header != 0x48 || frames[i + 2].header != 0x4c) {
      i++;
      continue;
    }

    const auto& cr = frames[i].payload;
    const auto& y = frames[i + 1].payload;
    const auto& cb = frames[i + 2].payload;
    ASSERT_GE(cr.size(), static_cast<size_t>(kWidth));
    ASSERT_GE(y.size(), static_cast<size_t>(kWidth));
    ASSERT_GE(cb.size(), static_cast<size_t>(kWidth));

    for (int x = 0; x < kWidth; x++) {
      const int yy = y[x];
      const int cbb = cb[x] - 128;
      const int crr = cr[x] - 128;
      const int r = Clamp(static_cast<int>(yy + 1.402 * crr));
      const int g = Clamp(static_cast<int>(yy - 0.34414 * cbb - 0.71414 * crr));
      const int b = Clamp(static_cast<int>(yy + 1.772 * cbb));
      r_sum += static_cast<uint8_t>(r);
      g_sum += static_cast<uint8_t>(g);
      b_sum += static_cast<uint8_t>(b);
      r_min = std::min<uint8_t>(r_min, r);
      g_min = std::min<uint8_t>(g_min, g);
      b_min = std::min<uint8_t>(b_min, b);
      r_max = std::max<uint8_t>(r_max, r);
      g_max = std::max<uint8_t>(g_max, g);
      b_max = std::max<uint8_t>(b_max, b);
    }

    triples++;
    i += 3;
  }

  EXPECT_EQ(triples, kExpectedTriples);

  const double pixels = static_cast<double>(triples) * kWidth;
  EXPECT_NEAR(r_sum / pixels, 229.36, 0.05);
  EXPECT_NEAR(g_sum / pixels, 173.11, 0.05);
  EXPECT_NEAR(b_sum / pixels, 134.67, 0.05);

  EXPECT_LT(r_min, 10);
  EXPECT_LT(g_min, 10);
  EXPECT_LT(b_min, 10);
  EXPECT_EQ(r_max, 255);
  EXPECT_EQ(g_max, 255);
  EXPECT_EQ(b_max, 255);
}
