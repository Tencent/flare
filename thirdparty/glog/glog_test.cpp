#include "glog/logging.h"
#include "glog/raw_logging.h"

#include "gflags/gflags.h"
#include "gtest/gtest.h"

TEST(GLog, Test) {
  VLOG(1) << "VLOG(1)";
  VLOG(2) << "VLOG(2)";
  LOG(INFO) << "INFO";
  LOG(WARNING) << "WARNING";
  LOG(ERROR) << "ERROR";
  RAW_LOG(ERROR, "RAW_LOG(ERROR)");
  RAW_LOG(INFO, "sssssssss %d", 256);
}

TEST(GLogDeathTest, Fatal) { ASSERT_DEATH((LOG(FATAL) << "sssssssss"), ""); }

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  return RUN_ALL_TESTS();
}
