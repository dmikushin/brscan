#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include "brother_brscan4.h"
}

namespace {

std::vector<uint8_t> Hex(std::initializer_list<uint8_t> bytes) {
  return std::vector<uint8_t>(bytes);
}

struct FakeReadStep {
  int rc;
  std::vector<uint8_t> bytes;
};

struct FakeReader {
  std::vector<FakeReadStep> steps;
  std::vector<int> requested_sizes;
  size_t index = 0;
};

int FakeRead(void* ctx, unsigned char* dst, int size) {
  auto* reader = static_cast<FakeReader*>(ctx);
  reader->requested_sizes.push_back(size);
  if (reader->index >= reader->steps.size()) return 0;

  const auto& step = reader->steps[reader->index++];
  if (step.rc > 0) {
    const int copy = std::min<int>(step.rc, static_cast<int>(step.bytes.size()));
    std::memcpy(dst, step.bytes.data(), static_cast<size_t>(copy));
  }
  return step.rc;
}

}  // namespace

TEST(Brscan4Reference, FirstStageBoundaryStatusMatchesMakeCacheBlock) {
  struct Case { uint8_t header; bool boundary; };
  const Case cases[] = {
      {0x00, false}, {0x40, false}, {0x42, false}, {0x7F, false},
      {0x80, true},  {0x81, false}, {0x83, false}, {0x84, false},
      {0x85, false}, {0x86, false}, {0xC1, false}, {0xC2, true},
      {0xD0, true},  {0xE3, true},  {0xE5, true},  {0xEF, true},
      {0xFF, true},
  };

  for (const auto& c : cases) {
    EXPECT_EQ(brscan4_is_boundary_status(c.header), c.boundary)
        << "header=0x" << std::hex << static_cast<int>(c.header);
  }
}

TEST(Brscan4Reference, ParsesWrapperAndPayloadLengthsFromReferenceOffsets) {
  const auto frame = Hex({
      0x42,             // header
      0x07, 0x00,       // wrapper_len = 7
      0x01, 0x00, 0x84, 0x00, 0x00, 0x00, 0x00,
      0x03, 0x00,       // data_len at offsets 10..11
      0xAA, 0xBB, 0xCC,
  });

  unsigned int record_len = 0;
  unsigned int payload_offset = 0;
  unsigned int payload_len = 0;
  ASSERT_EQ(brscan4_record_length(frame.data(), frame.size(),
                                  &record_len, &payload_offset, &payload_len), 1);
  EXPECT_EQ(record_len, 15u);
  EXPECT_EQ(payload_offset, 12u);
  EXPECT_EQ(payload_len, 3u);
}

TEST(Brscan4Reference, BoundaryStatusIsOneByteRecord) {
  const auto record = Hex({0x80, 0x42, 0x07, 0x00});
  unsigned int record_len = 0;
  unsigned int payload_offset = 123;
  unsigned int payload_len = 456;

  ASSERT_EQ(brscan4_record_length(record.data(), record.size(),
                                  &record_len, &payload_offset, &payload_len), 1);
  EXPECT_EQ(record_len, 1u);
  EXPECT_EQ(payload_offset, 0u);
  EXPECT_EQ(payload_len, 0u);
}

TEST(Brscan4Reference, IncompleteFramesAreNotRecords) {
  const auto prefix = Hex({0x42, 0x07, 0x00, 0x01, 0x00});
  unsigned int record_len = 0;
  EXPECT_EQ(brscan4_record_length(prefix.data(), prefix.size(), &record_len, nullptr, nullptr), 0);
}

TEST(Brscan4Cache, ExactReadKeepsResidualBytes) {
  Brscan4ReadCache cache{};
  brscan4_cache_reset(&cache);
  FakeReader reader{{{10, Hex({0, 1, 2, 3, 4, 5, 6, 7, 8, 9})}}};
  unsigned char out[16] = {};

  EXPECT_EQ(brscan4_cache_read(&cache, FakeRead, &reader, out, 4), 4);
  EXPECT_EQ(std::vector<uint8_t>(out, out + 4), Hex({0, 1, 2, 3}));
  EXPECT_EQ(cache.len, 6u);

  EXPECT_EQ(brscan4_cache_read(&cache, FakeRead, &reader, out, 6), 6);
  EXPECT_EQ(std::vector<uint8_t>(out, out + 6), Hex({4, 5, 6, 7, 8, 9}));
  EXPECT_EQ(cache.len, 0u);
  EXPECT_EQ(reader.index, 1u);
}

TEST(Brscan4Cache, CapsUsbReadsAtReferenceMaximum) {
  Brscan4ReadCache cache{};
  brscan4_cache_reset(&cache);
  FakeReader reader{{{0, {}}}};
  unsigned char out[16] = {};

  EXPECT_EQ(brscan4_cache_read(&cache, FakeRead, &reader, out, 0x20200), 0);
  ASSERT_FALSE(reader.requested_sizes.empty());
  EXPECT_EQ(reader.requested_sizes.front(), BRSCAN4_MAX_USB_READ_SIZE);
}

TEST(Brscan4Cache, RejectsRequestsAboveReferencePublicCap) {
  Brscan4ReadCache cache{};
  brscan4_cache_reset(&cache);
  FakeReader reader;
  unsigned char out[1] = {};

  EXPECT_EQ(brscan4_cache_read(&cache, FakeRead, &reader, out,
                               BRSCAN4_MAX_REQUEST_SIZE + 1), -1);
  EXPECT_TRUE(reader.requested_sizes.empty());
}

TEST(Brscan4Cache, PartialReadReturnsCachedBytesAndClearsCache) {
  Brscan4ReadCache cache{};
  brscan4_cache_reset(&cache);
  FakeReader reader{{
      {5, Hex({10, 11, 12, 13, 14})},
      {0, {}},
  }};
  unsigned char out[16] = {};

  EXPECT_EQ(brscan4_cache_read(&cache, FakeRead, &reader, out, 10), 5);
  EXPECT_EQ(std::vector<uint8_t>(out, out + 5), Hex({10, 11, 12, 13, 14}));
  EXPECT_EQ(cache.len, 0u);
}

TEST(Brscan4Cache, EmptyTimeoutReturnsZero) {
  Brscan4ReadCache cache{};
  brscan4_cache_reset(&cache);
  FakeReader reader{{{0, {}}}};
  unsigned char out[1] = {};

  EXPECT_EQ(brscan4_cache_read(&cache, FakeRead, &reader, out, 1), 0);
  EXPECT_EQ(cache.len, 0u);
}

TEST(Brscan4ReadNextRecord, ReturnsStatusAsOneByteRecord) {
  Brscan4ReadCache cache{};
  brscan4_cache_reset(&cache);
  FakeReader reader{{{4, Hex({0x80, 0x42, 0x07, 0x00})}}};
  unsigned char out[16] = {};

  EXPECT_EQ(brscan4_read_next_record(&cache, FakeRead, &reader, out, sizeof(out)), 1);
  EXPECT_EQ(out[0], 0x80);
  EXPECT_EQ(cache.len, 3u);
}

TEST(Brscan4ReadNextRecord, ReturnsFullFrameAtomically) {
  Brscan4ReadCache cache{};
  brscan4_cache_reset(&cache);
  const auto frame = Hex({
      0x42, 0x07, 0x00,
      0x01, 0x00, 0x84, 0x00, 0x00, 0x00, 0x00,
      0x03, 0x00,
      0xAA, 0xBB, 0xCC,
      0x80,
  });
  FakeReader reader{{{static_cast<int>(frame.size()), frame}}};
  unsigned char out[32] = {};

  EXPECT_EQ(brscan4_read_next_record(&cache, FakeRead, &reader, out, sizeof(out)), 15);
  EXPECT_EQ(std::vector<uint8_t>(out, out + 15),
            Hex({0x42, 0x07, 0x00, 0x01, 0x00, 0x84, 0x00, 0x00,
                 0x00, 0x00, 0x03, 0x00, 0xAA, 0xBB, 0xCC}));
  EXPECT_EQ(cache.len, 1u);

  EXPECT_EQ(brscan4_read_next_record(&cache, FakeRead, &reader, out, sizeof(out)), 1);
  EXPECT_EQ(out[0], 0x80);
}

TEST(Brscan4ReadNextRecord, DoesNotExposePartialFrame) {
  Brscan4ReadCache cache{};
  brscan4_cache_reset(&cache);
  FakeReader reader{{
      {5, Hex({0x42, 0x07, 0x00, 0x01, 0x00})},
      {0, {}},
  }};
  unsigned char out[32] = {};

  EXPECT_EQ(brscan4_read_next_record(&cache, FakeRead, &reader, out, sizeof(out)), 0);
}

TEST(Brscan4ReadNextRecord, NonBoundaryHighBytesCanBeFrameHeaders) {
  Brscan4ReadCache cache{};
  brscan4_cache_reset(&cache);
  const auto frame = Hex({
      0x81, 0x00, 0x00,
      0x00, 0x00,
  });
  FakeReader reader{{{static_cast<int>(frame.size()), frame}}};
  unsigned char out[16] = {};

  EXPECT_EQ(brscan4_read_next_record(&cache, FakeRead, &reader, out, sizeof(out)), 5);
  EXPECT_EQ(out[0], 0x81);
}
