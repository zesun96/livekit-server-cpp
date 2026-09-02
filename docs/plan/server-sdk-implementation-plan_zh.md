# LiveKit Server C++ SDK 实现计划

更新时间：2026-09-02

## 目标与排序原则

实现顺序固定为：

1. 先完成自托管 LiveKit 所需的跨平台、稳定、可发布 SDK。
2. 再补齐 LiveKit Cloud 专属服务和区域容灾能力。
3. 每个里程碑都必须包含公开 API、实现、GTest、安装消费测试和文档，不能只增加
   protobuf RPC 转发代码。

优先级判断标准：

- 自托管部署能否在 Windows、Linux 和 macOS 后端直接使用。
- 公共头文件是否保持稳定并避免泄露内部生成依赖。
- 请求能否取消、超时、诊断和安全重试。
- 是否与本地 `server-sdk-go-main` 的公开服务能力一致。
- Cloud 专属功能不能阻塞自托管版本发布。

## 当前基线

已经实现：

- Room 和 Participant 管理，包括 `PerformRpc`。
- Egress、Ingress、SIP、Agent Dispatch、WhatsApp/Twilio Connector。
- AccessToken HS256 签发。
- Webhook JWT、有效期和正文摘要验证，以及事件回调。
- Windows WinHTTP 传输、连接复用、自定义 `HttpTransport`。
- GTest 单元测试、可选真实服务器集成测试、安装包 consumer 测试。

主要缺口：

- 非 Windows 平台没有默认 HTTP 实现。
- 服务公开签名仍使用 protobuf 类型，虽然公共头已不直接包含生成头。
- 没有每次请求的取消、超时、额外 Header 和异步接口。
- AccessToken 尚未覆盖最新授权和房间配置声明。
- 缺少 Cloud Phone Number、Cloud Agent、Agent Simulation 共 30 个接口。
- 缺少 LiveKit Cloud 区域发现和 failover。

## 阶段 S0：稳定公共 API 边界

目标：先确定自托管版本可以长期维护的 API 和 ABI，避免后续服务扩展反复破坏调用方。

实现项：

- 新增 SDK 自有的请求、响应和领域模型目录，例如 `include/livekit/server/model/`。
- 公共服务接口不再以生成的 protobuf 类型作为必须使用的参数或返回值。
- 在 `src/detail/proto/` 实现 SDK 模型与 protobuf 的双向转换。
- 保留一个显式、可选的 protobuf adapter 层，为已有调用方提供迁移路径。
- 明确静态库与动态库的依赖策略：
  - 公共源码不要求 protobuf include 路径。
  - CMake/package manager 自动处理二进制链接依赖。
  - 不把 protobuf 静态对象强行合并进 SDK，避免宿主进程发生重复符号或 ABI 冲突。
- 为所有公开头增加仅使用 SDK `include/` 路径的编译测试。
- 为公开枚举和错误类型定义未知值策略，保证协议向前兼容。

验收条件：

- `include/livekit/server/` 中没有 `*.pb.h` 或 protobuf include。
- 不包含协议生成头的 consumer 可以使用 AccessToken、Webhook 和 SDK 自有服务模型。
- protobuf adapter 的启用与关闭都有独立构建测试。
- 现有 API 如需变更，提供兼容 overload、adapter 或明确的主版本迁移说明。

## 阶段 S1：跨平台自托管基础能力

目标：Windows、Linux、macOS 都能直接调用自托管 LiveKit Server。

实现项：

- 保留 WinHTTP 后端。
- 增加基于 libcurl 的非 Windows 默认 `HttpTransport`，支持 HTTP/HTTPS、系统代理、
  CA 验证、连接复用和响应大小限制。
- 新增 `USE_SYSTEM_CURL` 或等价选项，并固定 vendored 依赖版本与校验值。
- 统一 URL 规范化、IPv4/IPv6、代理、TLS、超时和错误映射行为。
- 为连接、请求发送、响应读取分别提供清晰的错误上下文。
- 限制请求和响应正文尺寸，所有窄化转换前检查范围。

验收条件：

- Windows 使用 WinHTTP、Linux/macOS 使用 libcurl 的构建和单元测试通过。
- 三个平台都能完成 CreateRoom/ListRooms/DeleteRoom 集成测试。
- TLS 验证默认开启，禁止静默降级到不安全连接。
- 自定义 `HttpTransport` 仍可替换默认实现。

## 阶段 S2：请求控制与可靠性

目标：让 SDK 适合长期运行的服务进程，而不只是同步示例。

实现项：

- 新增 `RequestOptions`：
  - 单次请求超时。
  - `std::stop_token` 取消。
  - 额外 HTTP Header。
  - 调用方提供或 SDK 生成的 request ID。
- 所有服务方法增加兼容 overload，允许传入 `RequestOptions`。
- 保证同一次逻辑请求重试时复用 `X-Livekit-Request-Id`。
- 区分连接失败、超时、取消、HTTP、Twirp 和协议解析错误。
- 增加线程安全和并发调用测试。
- 在同步 API 稳定后增加异步 API；优先提供可取消的 C++20 future/executor 适配，
  coroutine 接口作为可选层，不让库内部创建不可控的 detached thread。

验收条件：

- 取消可以中断 DNS/连接/发送/读取阶段，不等待完整默认超时。
- 每个调用可覆盖全局超时且不会修改其他并发调用。
- 自定义 Header 不得覆盖安全关键 Header，除非文档明确允许。
- TSAN 可用平台上的并发测试无数据竞争。

## 阶段 S3：自托管认证与核心服务完整性

目标：补齐自托管场景中的令牌声明、便利接口和协议兼容性。

实现项：

- AccessToken 增加：
  - `canSubscribeMetrics`。
  - `canManageAgentSession`。
  - participant kind 和 kind detail。
  - room preset、room configuration、room agent dispatch。
- RoomService 增加从当前 API key/secret 创建 AccessToken 的便利接口；预签名 token 模式
  必须明确返回不可签发错误。
- SIP 增加按 ID 批量获取 trunk/dispatch rule 的便利接口和稳定顺序语义。
- 增加 SIP 调用状态与 Twirp/HTTP 错误的结构化转换。
- 审核 Room、Egress、Ingress、SIP、Agent Dispatch、Connector 的所有当前公开 RPC；协议
  新增核心 RPC 时同步生成前置声明、实现和 route/grant 测试。
- SendData 自动 nonce 行为与 Go SDK 对齐，并验证幂等语义。
- Webhook 增加签名 key 轮换、所有事件便利字段和未知字段回归测试，继续保留完整
  `raw_body`。

验收条件：

- 自托管核心服务 route、grant、序列化和错误路径均有 GTest。
- AccessToken 声明与相同固定输入下的 Go SDK 输出语义一致。
- Webhook 使用真实本地 LiveKit Server 完成至少 room、participant、track 三类事件验证。

## 阶段 S4：自托管发布门槛

目标：形成第一个可稳定发布的自托管版本，然后才进入 Cloud 扩展。

实现项：

- CI 覆盖 Windows、Linux、macOS，至少包含 Debug/Release 和共享/静态消费组合。
- 安装导出、版本兼容文件、pkg-config 或等价消费方式完整。
- 增加 ABI/API 检查和公开头独立编译检查。
- 完成服务示例、Webhook HTTP 框架接入示例、错误处理和线程模型文档。
- 对凭据、日志脱敏、TLS、Webhook 验签和依赖供应链做发布前安全审计。
- 使用本地 LiveKit Server 和 CLI 运行完整自托管集成矩阵。

进入 Cloud 阶段的门槛：

- 自托管核心接口无已知阻断问题。
- 三个平台安装 consumer 通过。
- 单元测试和显式集成测试稳定，无偶发依赖时序测试。
- 公共 API/ABI 策略已经确定并记录。

## 阶段 C1：Cloud Phone Number

目标：补齐 LiveKit Cloud 电话号码管理的 6 个接口。

接口：

- `SearchPhoneNumbers`
- `PurchasePhoneNumber`
- `ListPhoneNumbers`
- `GetPhoneNumber`
- `UpdatePhoneNumber`
- `ReleasePhoneNumbers`

实现要求：

- 独立 `PhoneNumberClient`，并评估是否加入 `LiveKitApi` accessor。
- 使用 SIP admin grant。
- 搜索接口采用独立的较长超时，但仍允许调用方覆盖或取消。
- Cloud-only 构建和测试不能影响纯自托管 consumer。

## 阶段 C2：Cloud Agent 管理

目标：补齐 Cloud Agent 的 18 个接口。

接口组：

- 生命周期：`CreateAgent`、`CreateAgentV2`、`UpdateAgent`、`DeleteAgent`、
  `RestartAgent`、`RollbackAgent`。
- 部署：`DeployAgent`、`DeployAgentV2`、`PromoteAgent`。
- 查询：`ListAgents`、`ListAgentVersions`、`GetClientSettings`。
- Secret：`ListAgentSecrets`、`UpdateAgentSecrets`。
- Private Link：`CreatePrivateLink`、`DestroyPrivateLink`、`ListPrivateLinks`、
  `GetPrivateLinkStatus`。

实现要求：

- 支持 Cloud Agent endpoint 推导和显式覆盖，不把 Cloud 域名规则写入通用 transport。
- AccessToken 增加 agent `databaseAdmin`，并补齐 Cloud Agent admin grant。
- Secret 相关请求和响应不得写入普通日志或异常全文。

## 阶段 C3：Agent Simulation

目标：补齐 Agent Simulation 的 6 个接口。

接口：

- `CreateSimulationRun`
- `ConfirmSimulationSourceUpload`
- `GetSimulationRun`
- `ListSimulationRuns`
- `CancelSimulationRun`
- `CreateScenarioFromSession`

实现要求：

- 使用 `simulationAdmin` grant。
- 上传确认流程必须测试过期 URL、重复确认和取消。
- 大请求或上传数据不经过普通 protobuf 内存缓冲路径时，应提供流式扩展点。

## 阶段 C4：Cloud 区域容灾与高级授权

目标：达到 Go SDK 的 Cloud 运行可靠性和新增服务授权能力。

实现项：

- `/settings/regions` 区域发现、缓存和刷新。
- 默认最多 3 次尝试、指数退避、每次尝试独立超时预算。
- 只对 LiveKit Cloud 主机默认启用 failover；自托管默认不做跨区域猜测。
- 4xx 不重试，网络错误和 5xx 按策略重试；始终复用同一 request ID 和请求正文。
- 支持显式关闭 failover，并允许测试注入区域列表和退避时钟。
- AccessToken 增加 Inference、Observability 和其他 Cloud-only grant。

验收条件：

- 主区域不可用、两个区域不可用、全部不可用、4xx、5xx、超时、取消均有确定性测试。
- 非 Cloud URL 不触发区域发现。
- 重试不会重复生成 token、nonce 或 request ID。

## 测试与提交策略

每个阶段拆为可独立审查的提交：

1. 协议模型/前置声明和 CMake。
2. client 实现与 grant/route。
3. GTest 单元测试。
4. opt-in 集成测试与示例。
5. 安装导出和文档。

每个提交前至少运行：

```powershell
git diff --check
cmake --build <build-dir> --config Release --parallel
ctest --test-dir <build-dir> -C Release -L unit --output-on-failure
```

涉及真实服务行为时，在明确授权后使用本地 LiveKit Server 执行 `integration` 标签；
Cloud 阶段使用专用测试项目和短期凭据，不把真实凭据写入命令日志或仓库。
