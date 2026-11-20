#include <gtest/gtest.h>

extern "C" {
#include "brother_modelinf.h"
}

TEST(ScanProtocol, ExplicitLegacyOverridesSeriesThreshold) {
  MODELINF model{};
  model.seriesNo = 13;
  model.scanProtocol = SCAN_PROTOCOL_LEGACY;

  EXPECT_FALSE(model_uses_brscan4(&model));
}

TEST(ScanProtocol, ExplicitBrscan4OverridesSeriesThreshold) {
  MODELINF model{};
  model.seriesNo = 1;
  model.scanProtocol = SCAN_PROTOCOL_BRSCAN4;

  EXPECT_TRUE(model_uses_brscan4(&model));
}

TEST(ScanProtocol, UnannotatedModelsRetainThresholdRouting) {
  MODELINF legacy{};
  legacy.seriesNo = 9;
  legacy.scanProtocol = SCAN_PROTOCOL_COMPAT;

  MODELINF brscan4{};
  brscan4.seriesNo = 10;
  brscan4.scanProtocol = SCAN_PROTOCOL_COMPAT;

  EXPECT_FALSE(model_uses_brscan4(&legacy));
  EXPECT_TRUE(model_uses_brscan4(&brscan4));
}

TEST(ScanProtocol, ParsesKnownTokensAndDefaultsUnknownValues) {
  EXPECT_EQ(scan_protocol_from_token("legacy"), SCAN_PROTOCOL_LEGACY);
  EXPECT_EQ(scan_protocol_from_token("brscan4"), SCAN_PROTOCOL_BRSCAN4);
  EXPECT_EQ(scan_protocol_from_token("unknown"), SCAN_PROTOCOL_COMPAT);
}
