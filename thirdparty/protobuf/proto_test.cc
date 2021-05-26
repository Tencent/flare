// Copyright (c) 2017 Tencent Inc.
// Author: Sun Naicai (naicaisun@tencent.com)

#include "thirdparty/protobuf/proto_test.pb.h"

#include <utility>

#include "thirdparty/googletest/gtest/gtest.h"

namespace gdt {

using namespace protoc;  // NOLINT

class ProtocTest : public ::testing::Test {
 protected:
  ProtocTest() {
    original_msg_.set_msg_id(5);
    original_msg_.set_msg("msg");
    NestedMsg* original_nested_msg = original_msg_.mutable_nest_msg();
    original_nested_msg->set_id(6);
    original_nested_msg->set_title("nested");
    original_msg_.add_msg_ids(6);
    original_msg_.add_msg_ids(7);
    original_msg_.add_msgs("hello");
    original_msg_.add_msgs("world");
    NestedMsg* first_added_nested_msg = original_msg_.add_nest_msgs();
    first_added_nested_msg->set_id(1);
    first_added_nested_msg->set_title("first");
    NestedMsg* second_added_nested_msg = original_msg_.add_nest_msgs();
    second_added_nested_msg->set_id(2);
    second_added_nested_msg->set_title("second");
  }

 protected:
  TestMessage original_msg_;
};

TEST_F(ProtocTest, TestMove) {
  TestMessage original_msg_;
  original_msg_.set_msg_id(5);
  original_msg_.set_msg("msg");
  NestedMsg* original_nested_msg = original_msg_.mutable_nest_msg();
  original_nested_msg->set_id(6);
  original_nested_msg->set_title("nested");
  original_msg_.add_msg_ids(6);
  original_msg_.add_msg_ids(7);
  original_msg_.add_msgs("hello");
  original_msg_.add_msgs("world");
  NestedMsg* first_added_nested_msg = original_msg_.add_nest_msgs();
  first_added_nested_msg->set_id(1);
  first_added_nested_msg->set_title("first");
  NestedMsg* second_added_nested_msg = original_msg_.add_nest_msgs();
  second_added_nested_msg->set_id(2);
  second_added_nested_msg->set_title("second");

  TestMessage rvalue_cp_msg = std::move(original_msg_);
  EXPECT_FALSE(original_msg_.has_msg_id());
  EXPECT_FALSE(original_msg_.has_msg());
  EXPECT_FALSE(original_msg_.has_nest_msg());
  EXPECT_EQ(0, original_msg_.msg_ids_size());
  EXPECT_EQ(0, original_msg_.msgs_size());
  EXPECT_EQ(0, original_msg_.nest_msgs_size());

  EXPECT_EQ(5, rvalue_cp_msg.msg_id());
  EXPECT_EQ("msg", rvalue_cp_msg.msg());
  EXPECT_EQ(6, rvalue_cp_msg.nest_msg().id());
  EXPECT_EQ("nested", rvalue_cp_msg.nest_msg().title());

  ASSERT_EQ(2, rvalue_cp_msg.msg_ids_size());
  EXPECT_EQ(6, rvalue_cp_msg.msg_ids(0));
  EXPECT_EQ(7, rvalue_cp_msg.msg_ids(1));

  ASSERT_EQ(2, rvalue_cp_msg.msgs_size());
  EXPECT_EQ("hello", rvalue_cp_msg.msgs(0));
  EXPECT_EQ("world", rvalue_cp_msg.msgs(1));

  ASSERT_EQ(2, rvalue_cp_msg.nest_msgs_size());
  EXPECT_EQ(1, rvalue_cp_msg.nest_msgs(0).id());
  EXPECT_EQ("first", rvalue_cp_msg.nest_msgs(0).title());
  EXPECT_EQ(2, rvalue_cp_msg.nest_msgs(1).id());
  EXPECT_EQ("second", rvalue_cp_msg.nest_msgs(1).title());
}

TEST_F(ProtocTest, TestLazy) {
  std::string serialize_result;
  original_msg_.SerializeToString(&serialize_result);

  TestMessage first_parsed_msg;
  // quicker than normal parse
  ASSERT_TRUE(first_parsed_msg.ParseFromString(serialize_result));
  // though not parsed, we have this field
  EXPECT_TRUE(first_parsed_msg.has_nest_msg());
  EXPECT_EQ("nested", first_parsed_msg.nest_msg().title());

  // change some no-lazy fileds and serialize again
  TestMessage second_parsed_msg;
  ASSERT_TRUE(second_parsed_msg.ParseFromString(serialize_result));
  second_parsed_msg.set_msg("another msg");
  std::string second_serialize_result;
  // quicker than normal serialization
  second_parsed_msg.SerializeToString(&second_serialize_result);

  // though modified, the lazy field not parsed yet
  TestMessage third_parsed_msg;
  ASSERT_TRUE(third_parsed_msg.ParseFromString(second_serialize_result));
  EXPECT_EQ("another msg", third_parsed_msg.msg());
  // modify the lazy field
  third_parsed_msg.mutable_nest_msg()->set_id(8);
  EXPECT_EQ("nested", third_parsed_msg.nest_msg().title());

  third_parsed_msg.clear_nest_msg();
  std::string third_serialize_result;
  third_parsed_msg.SerializeToString(&third_serialize_result);
  TestMessage fourth_parsed_msg;
  ASSERT_TRUE(fourth_parsed_msg.ParseFromString(third_serialize_result));
  EXPECT_FALSE(fourth_parsed_msg.has_nest_msg());

  // test DebugString
  TestMessage fifth_parsed_msg;
  fifth_parsed_msg.ParseFromString(serialize_result);
  std::string expect_debug_string =
      "msg_id: 5\n"
      "msg: \"msg\"\n"
      "nest_msg {\n"
      "  id: 6\n"
      "  title: \"nested\"\n"
      "}\n"
      "msg_ids: 6\n"
      "msg_ids: 7\n"
      "msgs: \"hello\"\n"
      "msgs: \"world\"\n"
      "nest_msgs {\n"
      "  id: 1\n"
      "  title: \"first\"\n"
      "}\n"
      "nest_msgs {\n"
      "  id: 2\n"
      "  title: \"second\"\n"
      "}\n";
  EXPECT_EQ(expect_debug_string, fifth_parsed_msg.Utf8DebugString());
}

TEST_F(ProtocTest, TestReflectionForLazyField) {
  std::string serialize_result;
  original_msg_.SerializeToString(&serialize_result);

  // test GetMessage
  TestMessage first_parsed_msg;
  ASSERT_TRUE(first_parsed_msg.ParseFromString(serialize_result));
  EXPECT_TRUE(first_parsed_msg.has_nest_msg());
  auto& nested_msg = first_parsed_msg.GetReflection()->GetMessage(
      first_parsed_msg,
      TestMessage::descriptor()->FindFieldByName("nest_msg"));
  EXPECT_EQ("id: 6\ntitle: \"nested\"\n", nested_msg.Utf8DebugString());

  // test MutableMessage
  TestMessage second_parsed_msg;
  second_parsed_msg.ParseFromString(serialize_result);
  auto* ptr_nested_msg = second_parsed_msg.GetReflection()->MutableMessage(
      &second_parsed_msg,
      TestMessage::descriptor()->FindFieldByName("nest_msg"));
  EXPECT_EQ("id: 6\ntitle: \"nested\"\n", ptr_nested_msg->Utf8DebugString());
}

}  // namespace gdt
