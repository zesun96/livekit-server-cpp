# LiveKit Server C++ SDK 实现计划

更新时间：2026-10-07

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

- 本地 Protocol 当前全部 52 个自托管控制面 RPC：Room 和 Participant 管理（14 个，包括
  `PerformRpc`）、Egress（10 个）、Ingress（4 个）、SIP（16 个）、Agent Dispatch
  （3 个）以及 WhatsApp/Twilio Connector（5 个）。
- AccessToken HS256 签发。
- Webhook JWT、有效期、正文摘要、多 key 和动态 key 验证，同步事件回调以及完整
  `raw_body` 保留。
- Windows WinHTTP 传输、连接复用、自定义 `HttpTransport`。
- protobuf 无关的 SDK 服务模型，以及可选的 protobuf 生成类型 adapter。
- 每个公共头文件仅使用 SDK include 目录的独立编译测试。
- GTest 单元测试、可选 room/webhook 真实服务器集成测试，以及 protobuf adapter 启用和
  关闭两种安装包 consumer 测试。

主要缺口：

- 发布阻断项：Linux 和 macOS 没有默认 HTTP 实现，只能使用应用自行提供的 transport。
- SDK 自有服务模型是相互独立的规范 JSON 容器，但还没有类型化 C++ 字段、builder、集合
  访问器，也不能在调用服务前完成类型化校验。
- 调用均为同步调用且只有 client 级超时；没有单次请求超时、取消、调用方 Header、调用方
  request ID、异步 API 或重试策略。
- Transport 错误尚未区分 DNS、连接、TLS、发送、接收、超时和取消，WinHTTP 响应也没有
  可配置的最大尺寸限制。
- AccessToken 缺少当前 participant 权限、participant kind/detail、room
  configuration/preset/agents、SHA-256、Inference 和 Observability 声明。
- 缺少 Go SDK 对齐的便利能力：RoomService token 创建、保持输入顺序的 SIP 批量查询、结构化
  SIP 状态提取、请求校验，以及感知振铃时间的 SIP/WhatsApp 超时。
- `SendData` nonce 行为与重试/幂等语义尚未与 Go SDK 完全对齐和测试。
- 真实服务器集成测试仅覆盖 room 创建/删除和 `room_started` webhook；其他服务以及
  participant/track webhook 事件尚未覆盖。
- 尚无三平台 CI 矩阵、DLL 导出策略、ABI 检查、sanitizer 覆盖和完整的共享/静态安装消费矩阵。
- 缺少 Cloud Phone Number、Cloud Agent、Agent Simulation 共 30 个 RPC、Cloud Agent
  运维便利能力以及 LiveKit Cloud 区域发现/failover。

从当前基线开始的执行顺序：

1. S1：增加并强化跨平台 transport。
2. S2：增加单次请求控制、取消、诊断和可靠性语义。
3. S3：补齐 token、类型化模型易用性、便利接口、校验和 webhook 覆盖。
4. S4：通过三平台发布门槛，发布第一个自托管稳定版本。
5. C1-C4：先增加 Cloud 服务，再实现区域容灾和高级授权。

## 阶段 S0：稳定公共 API 边界

状态：已于 2026-10-07 完成。protobuf 无关的服务模型、转换层、可选兼容目标、公共头文件编译
测试和安装包 consumer fixture 均已落地；后续阶段可以在不改变模型边界的前提下增加类型化便捷
访问器。

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

状态：下一个实施阶段，也是自托管版本发布的首要阻断项。

目标：Windows、Linux、macOS 都能直接调用自托管 LiveKit Server。

实现项：

- 保留 WinHTTP 后端。
- 增加基于 libcurl 的非 Windows 默认 `HttpTransport`，支持 HTTP/HTTPS、系统代理、
  CA 验证、连接复用和响应大小限制。
- 新增 `USE_SYSTEM_CURL` 或等价选项，并固定 vendored 依赖版本与校验值。
- 统一 URL 规范化、IPv4/IPv6、代理、TLS、超时和错误映射行为。
- 为连接、请求发送、响应读取分别提供清晰的错误上下文。
- 所有后端以一致方式填充响应 Header。
- 增加可配置的请求和响应正文尺寸限制，所有窄化转换前检查范围，包括 WinHTTP 的
  `DWORD` 转换。
- 测试非法 URL、代理失败、TLS 失败、重定向、截断正文、超大正文、IPv6 literal 和 UTF-8
  host/path/header，且不访问物理媒体设备。

验收条件：

- Windows 使用 WinHTTP、Linux/macOS 使用 libcurl 的构建和单元测试通过。
- 三个平台都能完成 CreateRoom/ListRooms/DeleteRoom 集成测试。
- TLS 验证默认开启，禁止静默降级到不安全连接。
- Transport 错误能标识失败阶段，并保留底层平台错误上下文。
- 超大输入必须在不安全窄化转换或无界内存分配前失败。
- 自定义 `HttpTransport` 仍可替换默认实现。

## 阶段 S2：请求控制与可靠性

状态：计划在 S1 后实施。

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
- 保留 HTTP 响应 Header 和有用的 transport 错误细节，但异常消息不能包含凭据、token 或
  secret 响应正文。
- 明确哪些操作可以自动重试。默认重试仅适用于安全或明确幂等的调用；修改类调用必须具有
  稳定 request ID 和已记录的服务端去重约定。
- 增加线程安全和并发调用测试。
- 在同步 API 稳定后增加异步 API；优先提供可取消的 C++20 future/executor 适配，
  coroutine 接口作为可选层，不让库内部创建不可控的 detached thread。

验收条件：

- 取消可以中断 DNS/连接/发送/读取阶段，不等待完整默认超时。
- 每个调用可覆盖全局超时且不会修改其他并发调用。
- 自定义 Header 不得覆盖安全关键 Header，除非文档明确允许。
- 取消和超时使用不同且稳定的 `ErrorCode`，并保留有用诊断上下文。
- 重试不得重新生成 request ID、签名 token、请求正文或调用方提供的 nonce。
- TSAN 可用平台上的并发测试无数据竞争。

## 阶段 S3：自托管认证与核心服务完整性

状态：计划在请求控制稳定后实施。Protocol RPC 清单已经完整，本阶段集中补齐 token、类型化
易用性、校验和 Go SDK 行为一致性。

目标：补齐自托管场景中的令牌声明、便利接口和协议兼容性。

实现项：

- 为常用 SDK 自有 Room、Participant、Egress、Ingress、SIP、Agent Dispatch 和 Connector
  模型增加类型化 C++ builder 和只读访问器。规范 JSON 继续作为向前兼容的 escape hatch，
  主接口不得暴露 protobuf 类型。
- AccessToken 增加：
  - `canSubscribeMetrics`。
  - `canManageAgentSession`。
  - participant kind 和 kind detail。
  - room preset、room configuration、room agent dispatch。
- 增加 `SetSha256` 对齐和 room configuration 敏感凭据检查。默认拒绝签发包含存储或服务
  凭据的 token，除非显式启用名称清晰的 server-only override。
- RoomService 增加从当前 API key/secret 创建 AccessToken 的便利接口；预签名 token 模式
  必须明确返回不可签发错误。
- SIP 增加按 ID 批量获取 trunk/dispatch rule 的便利接口和稳定顺序语义。
- 增加 SIP 调用状态与 Twirp/HTTP 错误的结构化转换。
- 对齐 Go SDK 对 SIP create/update/list/delete 的请求校验，非法请求应在发送前返回
  `invalid_argument`。
- 为 SIP participant 创建、SIP transfer 和等待接听的 WhatsApp 调用增加感知振铃时间的
  超时行为，同时尊重调用方显式取消。
- 审核 Room、Egress、Ingress、SIP、Agent Dispatch、Connector 的所有当前公开 RPC；协议
  新增核心 RPC 时同步生成前置声明、实现和 route/grant 测试。
- SendData 自动 nonce 行为与 Go SDK 对齐，并验证幂等语义。
- Webhook 增加所有当前事件的便利字段和未知字段回归测试，继续保留完整 `raw_body`。多签名
  key 和动态 key 查询已经实现，必须继续保持覆盖。

验收条件：

- 自托管核心服务 route、grant、序列化和错误路径均有 GTest。
- AccessToken 声明与相同固定输入下的 Go SDK 输出语义一致。
- 类型化模型用户无需编写或解析 JSON 即可完成常用自托管工作流。
- 非法 SIP 输入在本地失败，等待接听的调用不会被普通短请求超时提前终止。
- Webhook 使用真实本地 LiveKit Server 完成至少 room、participant、track 三类事件验证。

## 阶段 S4：自托管发布门槛

状态：计划在 S1-S3 后实施。通过此门槛前不得开始 Cloud 工作。

目标：形成第一个可稳定发布的自托管版本，然后才进入 Cloud 扩展。

实现项：

- CI 覆盖 Windows、Linux、macOS，至少包含 Debug/Release 和共享/静态消费组合。
- 增加显式符号可见性/导出标注，并验证 Windows 动态库 consumer 能链接和运行，不能仅依赖
  静态库构建成功。
- 安装导出、版本兼容文件、pkg-config 或等价消费方式完整。
- 增加 ABI/API 检查和公开头独立编译检查。
- 在支持的平台运行 ASan/UBSan，并至少在一个平台使用 TSAN 覆盖并发 client、transport、
  token 和 webhook 路径。
- 完成服务示例、Webhook HTTP 框架接入示例、错误处理、取消指导和线程模型文档。
- 对凭据、日志脱敏、TLS、Webhook 验签和依赖供应链做发布前安全审计。
- 使用本地 LiveKit Server 和 CLI 运行完整自托管集成矩阵。

进入 Cloud 阶段的门槛：

- 自托管核心接口无已知阻断问题。
- 三个平台安装 consumer 通过。
- 静态和共享安装 consumer 在 protobuf adapter 开启与关闭两种配置下均通过。
- 单元测试和显式集成测试稳定，无偶发依赖时序测试。
- 公共 API/ABI 策略已经确定并记录。

## 阶段 C1：Cloud Phone Number

状态：推迟到自托管发布门槛通过之后。

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

状态：推迟到 C1 和自托管发布门槛完成之后。

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
- 管理调用使用当前 Protocol 的 Agent admin grant。不能自行添加 pinned Protocol 中不存在的
  grant 字段；仅当本地 Protocol 和 Go SDK 均提供新字段时再增加。
- Secret 相关请求和响应不得写入普通日志或异常全文。
- 完成 RPC 对齐后，单独评估 Go SDK 的 Cloud Agent 运维便利能力：源码上传与构建、部署日志
  流、registry/push target 处理和显式区域选择。
- 为上传和日志提供流式请求/响应扩展点，不能强制大体积或无界数据经过内存 protobuf 调用路径。

## 阶段 C3：Agent Simulation

状态：推迟到 C2 完成之后。

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

状态：推迟到 Cloud 服务 client 可用之后。

目标：达到 Go SDK 的 Cloud 运行可靠性和新增服务授权能力。

实现项：

- `/settings/regions` 区域发现、缓存和刷新。
- 默认最多 3 次尝试、指数退避、每次尝试独立超时预算。
- 只对 LiveKit Cloud 主机默认启用 failover；自托管默认不做跨区域猜测。
- 4xx 不重试，网络错误和 5xx 按策略重试；始终复用同一 request ID 和请求正文。
- 支持显式关闭 failover，并允许测试注入区域列表和退避时钟。
- AccessToken 增加与 pinned Protocol 一致的 Inference 和 Observability grant；grant 序列化
  测试必须独立于 Cloud 网络测试。

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

涉及公共边界或打包的变更还必须使用两个全新安装前缀构建 consumer：

- `LIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER=OFF`：不安装任何生成的 `*.pb.h` 或 adapter target，
  SDK 模型 consumer 构建通过。
- `LIVEKIT_SERVER_ENABLE_PROTOBUF_ADAPTER=ON`：生成头保留在隔离的 adapter include 目录，
  protobuf 兼容 consumer 构建通过。

Transport 变更必须先运行不访问网络的后端单元测试，再在每个支持平台运行显式本地服务器
integration 标签。取消、超时、重试和 failover 测试必须使用注入 transport/clock，并保持
确定性。

涉及真实服务行为时，在明确授权后使用本地 LiveKit Server 执行 `integration` 标签；
Cloud 阶段使用专用测试项目和短期凭据，不把真实凭据写入命令日志或仓库。
