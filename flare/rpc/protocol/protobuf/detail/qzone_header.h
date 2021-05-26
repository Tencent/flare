// Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of the
// License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#ifndef FLARE_RPC_PROTOCOL_PROTOBUF_DETAIL_QZONE_HEADER_H_
#define FLARE_RPC_PROTOCOL_PROTOBUF_DETAIL_QZONE_HEADER_H_

#include <arpa/inet.h>

// Copied from `base_class_old/comm/qzone_protocol.h`, with minimal changes.
namespace flare::protobuf::qzone {

typedef enum {
  QzoneServerSucc = 0,         // 0 - [正常数据, 处理成功]
  QzoneServerFailed = 1,       // 1 - [正常数据, 处理失败]
  QzoneServerExc = 2,          // 2 - [异常数据, 服务器拒绝处理]
  QzoneServerBusy = 3,         // 3 - [正常数据, 服务器忙, 可重试]
  QzoneServerRedirected = 10,  // 10 - [服务器重定向]
  QzoneServerAck = 20,         // 20 - [回执包]
  QzoneClient = 100,           // 100 - [client请求, 非server回应]
} QzoneServerResponse;

#define DefaultServResInfo 0

// +-----------------------------------------------------------------+
// | 版本(1byte) | 命令字(4 bytes) | 效验和(2 bytes) | 序列号(4 bytes) |
// |-----------------------------------------------------------------|
// | 序列号(4bytes) | 染色信息(4 bytes) | server回应标识(1 byte)       |
// |-----------------------------------------------------------------|
// | server回应信息(2 bytes) | 协议总长度(4bytes) | 协议体             |
// +-----------------------------------------------------------------+

#pragma pack(1)
typedef struct _QzoneProtocolHead_ {
  unsigned char version;
  unsigned int cmd;
  unsigned short checksum;

  unsigned int serialNo;  // 4 bytes, Protocol Serial Number, 由client生成,
                          // client效验
  unsigned int colorration;          // 4 bytes, 染色信息
  unsigned char serverResponseFlag;  // 1 byte, Server端回应标识 :
                                     // 0 - [正常数据, 处理成功],
                                     // 1 - [正常数据, 处理失败]
                                     // 2 - [异常数据, 服务器拒绝处理]
                                     // 3 - [正常数据, 服务器忙, 可重试]
                                     // 10 - [服务器重定向]
                                     // 20 - [回执包],
                                     // 100 - [client请求, 非server回应]
                                     //
  unsigned short serverResponseInfo;  // 2 bytes, Server端回应附加信息
  // 对于处理失败(1):  表示处理失败的错误号errcode
  //  对于服务器忙(3):  表示重试时间(网络字节序)
  //  对于服务器拒绝服务(2): 表示拒绝原因(网络字节序)
  //  其中, 服务器拒绝服务原因定义如下:
  //  使用的每bit表示不同的拒绝理由, 由低位字节至高分别定义为:
  //      0x1: 当前协议版本
  //      0x2: 当前协议命令字
  //      0x4: 当前client类型
  //      0x8: 当前client版本
  //      0x10: 当前client子系统
  //      相应的位置1表示拒绝, 置0表示不拒绝, 如5位全为0表示无理由拒绝.
  //  例如, 服务器拒绝当前client类型的当前client版本,
  // 则ServerResponseInfo的取值为0x12.
  char reserved[1];  // 预留
  unsigned int len;  // 协议总长度

  _QzoneProtocolHead_() {
    version = 0x0;
    cmd = 0;
    checksum = 0;
    serialNo = 0;
    colorration = 0;
    serverResponseFlag = 0;
    serverResponseInfo = 0;
    len = 0;
  }

  void Encode() {
    // version = version;
    cmd = htonl(cmd);
    serialNo = htonl(serialNo);
    colorration = htonl(colorration);
    // serverResponseFlag = serverResponseFlag;
    serverResponseInfo = htons(serverResponseInfo);
    len = htonl(len);
  }

  void Decode() {
    // version = version;
    cmd = ntohl(cmd);
    serialNo = ntohl(serialNo);
    colorration = ntohl(colorration);
    // serverResponseFlag = serverResponseFlag;
    serverResponseInfo = ntohs(serverResponseInfo);
    len = ntohl(len);
  }

  /*
  @brief:	效验合
  @param:	协议头+协议体的sendbuf, sendbuf长度
  */
  inline unsigned short CheckSum(const void* buf, unsigned int bufLen) {
    unsigned int sum = 0;
    unsigned short* data = (unsigned short*)buf;

    int len = bufLen / 2;
    int mod = bufLen % 2;
    for (int i = 0; i < len; i++) sum += data[i];

    unsigned short nshort = 0;
    if (mod == 1) {
      /* bugfix, 2008-05-28, ianyang modified, char* => unsigned char* */
      nshort = (unsigned short)((unsigned char*)buf)[bufLen - 1];
      sum += nshort;
    }
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    nshort = ~((unsigned short)sum);

    return nshort;
  }
} QzoneProtocolHead, *QzoneProtocolHeadPtr;
#pragma pack()

// -----------------------------------------------------------------------------
//
// 协议结构
//
// -----------------------------------------------------------------------------

/*
 * 数据包的头尾标识
 */
#define QzoneProtocolSOH 0x04
#define QzoneProtocolEOT 0x05

/*
 *  protocol packet
 */
#pragma pack(1)

typedef struct {
  char soh;
  QzoneProtocolHead head;
  char body[];
  // ... char eot; 包结束
} QzoneProtocol, *QzoneProtocolPtr;

#pragma pack()
}  // namespace flare::protobuf::qzone

#endif  // FLARE_RPC_PROTOCOL_PROTOBUF_DETAIL_QZONE_HEADER_H_
