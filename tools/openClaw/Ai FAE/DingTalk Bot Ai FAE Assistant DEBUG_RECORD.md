


# 钉钉机器人专业化实施笔记（实战版）

## 0. 目标定义（先统一口径）
你要的“专业”不是会聊天，而是：
1. 能稳定收消息并秒级 ACK（不丢回调）
2. 技术问题回答有结构（结论→依据→下一步→Sources）
3. 参数类问题不胡报（证据不足就明确不下结论）
4. 优先复用真实助理能力（runtime），而不是模板拼接

---

## 1. 推荐架构（两层就够）

### 第一层：收发桥接层（必须轻）
职责只做 4 件事：
- 接收钉钉 HTTP 回调
- 立刻 ACK（`msgtype: empty`）
- 异步调用回答引擎
- 把结果回发 sessionWebhook

不要在这一层做复杂检索/推理。

### 第二层：回答引擎层（必须可演进）
优先顺序：
1. **主路径：真实 assistant runtime**（`openclaw agent --session-id ... --json`）
2. **兜底路径：本地 router/RAG**（runtime 失败时保障可用）

这样可同时保证：
- 智能上限（runtime）
- 可用下限（fallback）

---

## 2. 专业回答标准（强约束）

### 固定输出模板
- 结论：一句话直接回答
- 关键依据：3~6 条
- 下一步：可执行动作
- Sources：可核查来源

### 参数题硬规则（主频/DDR/容量/版本）
- 必须优先引用 Product Manual / Datasheet / Hardware Guide
- 无可靠证据：写“暂不建议报确定值”
- 禁止经验值直接外发

---

## 3. 运行稳定性 checklist

### 必做
- 进程常驻（nohup/systemd/pm2 任一）
- 隧道常驻（cloudflared）
- 健康检查：`http://127.0.0.1:3000/`
- 日志落盘：`state/dingtalk_http_bot.log`

### 监控关键日志
- `recv sender=...`（是否收到）
- `answered by assistant runtime`（是否走到真实助理）
- `fallback answered by local router`（是否发生降级）
- `replied status=200`（是否发回成功）

---

## 4. 你当前系统的推荐配置

### 路由策略
- 默认全部技术问题走 `dev-center-assistant`
- 先 runtime，再 fallback

### 质量策略
- 参数题强证据
- Sources 强制输出
- 回答风格保持工程师口径（直接、可执行、少空话）

---

## 5. 常见故障与处理

### 故障 A：收到消息但没回
排查顺序：
1. 桥接是否在线（3000）
2. sessionWebhook 是否有效
3. 是否 `replied status=200`

### 故障 B：回了但不够智能
1. 看是否 `answered by assistant runtime`
2. 如果总是 fallback，修 runtime 调用
3. 检查 prompt 是否把业务约束写清楚

### 故障 C：参数题答非所问
1. 检查 sources 是否来自可靠文档
2. 加强型号锁定（SoC/板卡/BSP 三要素）
3. 无证据时必须保守回复

---

## 6. 一句话落地策略
“桥接层只做传输，智能都放在助理；参数题必须证据优先；runtime 失败自动降级但不掉线。”

---

## 7. 你可以每周复盘的 4 个指标
1. Runtime 命中率（非 fallback 占比）
2. 回包成功率（status=200 占比）
3. 参数题证据合规率（有 Sources 且可核查）
4. 用户追问率（一次答清的比例）

如果这 4 个指标持续提升，机器人就会越来越“专业”。







# DingTalk Bot Debug Record

- Generated: 2026-04-14 14:51:09 CST
- Workspace: /home/joes/.openclaw/workspace

## Runtime Status
```
 240359  191876 /home/joes/.openclaw/workspace/cloudflared tunnel --url http://127.0.0.1:3000
 580375    1055 node /home/joes/.openclaw/workspace/skills/dingtalk-notifier/scripts/ding_http_listener_smart.js
```

## Health Check
```
智能客服 HTTP smart bridge is online.
```

## Recent Bridge Logs (tail -n 80)
```
🚀 智能客服 HTTP SMART bridge listening on :3000 (default -> dev-center-assistant)
[HTTP-SMART] recv sender=joeshua text=全志t113i和t113s3的区别
[HTTP-SMART] assistant runtime error: Command failed: openclaw agent --session-id dingtalk-dev-center-cidq9vt-yVdsE5MOujiniIP9Q-- --message 你是dev-center-assistant能力在钉钉侧的智能客服实例，请直接回答技术问题。
这是一条真实用户提问，不是 heartbeat 轮询，禁止回复 HEARTBEAT_OK。
回答要求：结论先行、关键依据、下一步、Sources；术语准确，不要空话。
参数类问题（主频/DDR/容量）若证据不足，明确写“暂不建议报确定值”。
提问人: joeshua
会话类型: 2
会话标题: A-robotic(MYIR智能客服测试)
用户问题: 全志t113i和t113s3的区别 --json --timeout 120

[HTTP-SMART] fallback answered by local router
[HTTP-SMART] replied status=200
[HTTP-SMART] recv sender=joeshua text=全志t113i和t113s3的区别
[HTTP-SMART] answered by assistant runtime
[HTTP-SMART] replied status=200
[HTTP-SMART] recv sender=joeshua text=rk3576如何适配新的lvds屏幕
[HTTP-SMART] assistant runtime error: Command failed: openclaw agent --session-id dingtalk-dev-center-cidq9vt-yVdsE5MOujiniIP9Q-- --message 你是dev-center-assistant能力在钉钉侧的智能客服实例，请直接回答技术问题。
这是一条真实用户提问，不是 heartbeat 轮询，禁止回复 HEARTBEAT_OK。
回答要求：结论先行、关键依据、下一步、Sources；术语准确，不要空话。
参数类问题（主频/DDR/容量）若证据不足，明确写“暂不建议报确定值”。
提问人: joeshua
会话类型: 2
会话标题: A-robotic(MYIR智能客服测试)
用户问题: rk3576如何适配新的lvds屏幕 --json --timeout 120

[HTTP-SMART] fallback answered by local router
[HTTP-SMART] replied status=200
[HTTP-SMART] recv sender=joeshua text=rk3576如何适配新的lvds屏幕
[HTTP-SMART] answered by assistant runtime
[HTTP-SMART] replied status=200
[HTTP-SMART] recv sender=joeshua text=st257以太网phy调试
[HTTP-SMART] assistant runtime error: Command failed: openclaw agent --session-id dingtalk-dev-center-cidq9vt-yVdsE5MOujiniIP9Q-- --message 你是dev-center-assistant能力在钉钉侧的智能客服实例，请直接回答技术问题。
这是一条真实用户提问，不是 heartbeat 轮询，禁止回复 HEARTBEAT_OK。
回答要求：结论先行、关键依据、下一步、Sources；术语准确，不要空话。
参数类问题（主频/DDR/容量）若证据不足，明确写“暂不建议报确定值”。
提问人: joeshua
会话类型: 2
会话标题: A-robotic(MYIR智能客服测试)
用户问题: st257以太网phy调试 --json --timeout 120

[HTTP-SMART] fallback answered by local router
[HTTP-SMART] replied status=200
[HTTP-SMART] recv sender=joeshua text=st257以太网phy调试
[HTTP-SMART] answered by assistant runtime
[HTTP-SMART] replied status=200
```

## Git History (related files, last 20)
```
ce2f5e5 feat(dingtalk): call openclaw assistant runtime before local router fallback
981be60 feat(dingtalk): force dev-center routing and tighten KB-backed parameter answers
b80f949 feat(dingtalk): default bot routing to dev-center and auto-switch email requests
cee9b54 chore(dingtalk): remove route tag from bot replies
4a05cdb feat(dingtalk): upgrade bridge router to local RAG evidence extraction
9db1a4d chore(dingtalk): route bridge to email-kb-tech-reply-pro
ecde05f feat(dingtalk): add dingtalk-bridge-pro skill and route HTTP bot to dev-center-assistant
3b6e43b refactor(dingtalk): default to professional skill-style local KB answers
ba6183a feat(dingtalk): wire smart HTTP bot to local KB + brain fallback
162a466 fix(dingtalk): improve smart HTTP replies with local SoC knowledge
e51788f feat(dingtalk): add HTTP smart listener with AI fallback
```
已加强完成，且**已做 git 提交**。✅
####   最新Git 提交
- commit: `12a0a7f`
- message: `chore(backup): add hardened dingtalk bot source snapshot with checksums`
####   加强备份结果
- 备份目录：`/home/joes/.openclaw/workspace/backups/dingtalk-bot-source-20260414-145935-hardened`
- 压缩包：`/home/joes/.openclaw/workspace/backups/dingtalk-bot-source-20260414-145935-hardened.tar.gz`
- 压缩包校验：`/home/joes/.openclaw/workspace/backups/dingtalk-bot-source-20260414-145935-hardened.tar.gz.sha256`
- 快速指针：`/home/joes/.openclaw/workspace/backups/dingtalk-bot-source-latest`（软链接到最新备份）
####   包含内容
- 核心源码快照（bridge/router/skill 文件）
- 调试日志快照
- 运行健康检查快照
- git head / 相关提交历史 / 状态信息
- 目录内文件 `SHA256SUMS` 完整校验清单



## Current Key Behavior
- Bridge layer: HTTP listener on port 3000 with fast ACK.
- Primary answer path: OpenClaw assistant runtime via 'openclaw agent --session-id ... --json'.
- Fallback path: local router (answer_router.py) when runtime call fails.
- Current target skill: dev-center-assistant.

