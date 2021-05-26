# HBase protocol

这一目录保存了我们用于和HBase客户端/服务端通信的底层协议。

## 线上格式

HBase的线上格式可参考[hbase/rpc.adoc](https://github.com/apache/hbase/blob/master/src/main/asciidoc/_chapters/rpc.adoc)。

## IDL

协议定义均来源于[hbase/hbase-protocol](https://github.com/apache/hbase/blob/master/hbase-protocol/src/main/protobuf/)。

我们对`*.proto`做了一定的修改：

- 为避免符号冲突，我们对`*.proto`中包名做了修改。
- 我们对文件名大小写也做了修改以使其符合我们的编码规范。

*我们所做的所有修改均以不改变其线上格式为前提。这保证了我们可以正常和原版的HBase服务端/客户端正常通信。*

另外此处只引入了HBase的底层RPC（即接口无关的）协议，flare框架并不关心HBase所提供的各个服务及其接口。

HBase的服务接口相关IDL，我们建议业务自行引入（如将整个`hbase-protocol`引入至`thirdparty/`下）。

## 常量定义

[`constants.h`](constants.h)中定义了部分公用常量。

**`constants.h`中定义的常量仅用于框架内部使用，普通用户不应引用这一文件中的内容。**

这些常量的原始定义来源于多处，包括但不限于：

- [`hbase/HConstants.java`](https://github.com/apache/hbase/blob/master/hbase-common/src/main/java/org/apache/hadoop/hbase/HConstants.java)
- [`hbase/AuthMethod.java`](https://github.com/apache/hbase/blob/master/hbase-client/src/main/java/org/apache/hadoop/hbase/security/AuthMethod.java)
- [`org.apache.hadoop.hbase.ipc`](https://hbase.apache.org/apidocs/org/apache/hadoop/hbase/ipc/package-summary.html)

为符合我们的代码规范，定义常量时我们可能会做一些代码结构、命名风格等不影响二进制交互性的编码调整。

`constants.h`并不是一个完整的常量列表，目前我们只定义了被我们框架所使用的常量。

## 备注

这儿列出了我们编码期间遇到的一些细节问题。

- `ConnectionHeader.service_name`只包含类名，而不是fully-qualified的全名。可参考[org.apache.hadoop.hbase.ipc.AbstractRpcClient.callMethod(...)](https://github.com/apache/hbase/blob/master/hbase-client/src/main/java/org/apache/hadoop/hbase/ipc/AbstractRpcClient.java)。
- [hbase/rpc.adoc](https://github.com/apache/hbase/blob/master/src/main/asciidoc/_chapters/rpc.adoc)中对`ConnectionHeader`的格式描述不正确，实际的线上格式为`[size(4-byte BE integer)][ConnectionHeader::SerializeToXxx]`，而非文档所说的通过`writeDelimitedTo`生成。可以参考[org.apache.hadoop.hbase.ipc.BlockingRpcConnection.BlockingRpcConnection(...)](https://github.com/apache/hbase/blob/master/hbase-client/src/main/java/org/apache/hadoop/hbase/ipc/BlockingRpcConnection.java)。
