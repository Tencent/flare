# Protocol Buffers RPC状态码

pb的RPC状态码使用`int`定义。其中小于1000的状态码由框架保留，并定义于[rpc_meta.proto](../rpc/protocol/protobuf/rpc_meta.proto)中的`Status`。

业务代码可自行定义大于1000的状态码，并通过`RpcServerController::SetFailed`、`RpcClientController::ErrorCode`在服务端至客户端之间传递。

框架本身只关注特定的一些错误码及成功（即`0`），其余状态码均被框架视为失败。业务方可根据需求自行添加。

（FIXME: 需要允许业务自行定义“成功”类型的错误码吗？[NTSTATUS](https://docs.microsoft.com/en-us/windows-hardware/drivers/kernel/using-ntstatus-values)支持这种定义，但是pb本身对这种大数字编码的时空效率不高。）

---
[返回目录](README.md)
