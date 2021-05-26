import unittest

from thirdparty.protobuf.proto_test_pb2 import TestMessage


class ProtocTest(unittest.TestCase):
    def testIt(self):
        m = TestMessage()
        m.msg_id = 1
        self.assertEquals(1, m.msg_id)


if __name__ == '__main__':
    unittest.main()
