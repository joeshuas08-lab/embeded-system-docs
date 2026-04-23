# 开发者中心问答机器人开发文档

> 项目名：`dev-center-chat`
>
> 目标：在开发者中心网页内提供稳定的技术问答能力（当前通过 OpenClaw CLI 代理实现）。

---

## 1. 项目结构

```text
dev-center-chat/
├─ package.json
├─ package-lock.json
├─ server.js                 # 后端代理（Express）
├─ public/
│  └─ index.html            # 前端聊天页面
└─ DEVELOPMENT_GUIDE.md     # 本文档
```

---

## 2. 整体架构

当前采用“**前端页面 + Node 后端代理 + OpenClaw CLI**”架构：

1. 前端 `index.html` 发送用户问题到 `/api/chat`
2. 后端 `server.js` 调用：
   - `openclaw agent --to +8613800000000 --message "..." --json`
3. 后端解析 CLI 输出中的 JSON，提取回答文本
4. 后端将回答按字符拆分，模拟流式 SSE 输出给前端
5. 前端逐字渲染，形成打字机效果

---

## 3. 本地运行

### 3.1 安装依赖

```bash
cd /home/joes/.openclaw/workspace/dev-center-chat
npm install
```

### 3.2 启动服务

```bash
node server.js
```

默认监听：
- `0.0.0.0:8080`

浏览器访问（同机）：
- `http://127.0.0.1:8080`

局域网访问（跨设备）：
- `http://<服务器IP>:8080`

---

## 4. 核心实现说明

## 4.1 后端 `server.js`

### 请求入口
- `POST /api/chat`
- Body 结构：

```json
{
  "messages": [
    {"role":"system","content":"..."},
    {"role":"user","content":"..."}
  ]
}
```

### 命令执行

后端将最新 user 消息提取为 `userText`，执行：

```bash
openclaw agent --to +8613800000000 --message "<userText>" --json
```

说明：
- `--to` 用于绑定会话上下文（固定 session key）
- `--json` 返回结构化结果

### 返回解析

由于 CLI 输出可能包含日志，后端采用：
- `indexOf('{')` + `lastIndexOf('}')` 截取最外层 JSON
- `JSON.parse` 后多路径提取正文字段：
  - `reply`
  - `payloads[0].text`
  - `result.payloads[0].text`
  - `message`
  - `text`
  - `choices[0].message.content`

### 流式输出

后端使用 SSE：
- Header: `text/event-stream`
- 每个字符返回：`data: {"text":"x"}`
- 结束返回：`data: [DONE]`

---

## 4.2 前端 `public/index.html`

### 核心行为
- 输入框 + 发送按钮
- 调用 `/api/chat`
- 使用 `ReadableStream` + `TextDecoder` 按行解析 SSE
- 读取 `data.text` 逐步拼接显示

### 安全处理
- 使用 `textContent` 渲染 AI 输出，避免脚本注入

---

## 5. 常见问题与排查

## 5.1 页面提示“服务请求失败”

排查顺序：
1. 后端是否运行：
   ```bash
   lsof -i:8080
   ```
2. 看后端日志是否有命令执行错误
3. `openclaw` 是否可在当前 shell 直接调用

## 5.2 网页无回答 / 提示“找不到内容字段”

原因通常是 CLI 输出字段结构变化。

处理方法：
1. 在后端日志打印截取后的 JSON
2. 将提取链新增对应字段路径

## 5.3 端口被占用 (`EADDRINUSE`)

```bash
pkill -f "node server.js"
# 或
kill -9 $(lsof -t -i:8080)
```

再重启：
```bash
node server.js
```

---

## 6. 生产化建议

1. **后端进程托管**：建议用 PM2/systemd
2. **鉴权**：`/api/chat` 增加 Token 或登录态校验
3. **限流**：防刷（按 IP 或账号）
4. **日志分级**：info/warn/error 分离，避免控制台混乱
5. **超时控制**：CLI 调用增加超时、重试策略
6. **内容审查**：对输出加策略层（可选）
7. **结构化监控**：记录请求耗时、失败率、命中字段路径

---

## 7. 版本备份

已创建代码备份（不含 node_modules）：

- `/home/joes/.openclaw/workspace/backups/dev-center-chat-20260413-113307`

可直接用于回滚或打包迁移。

---

## 8. 后续可扩展方向

1. 将 CLI 代理升级为 OpenClaw 原生 RPC/WebSocket 直连
2. 增加多轮会话隔离（按用户 ID 映射不同 `--to`）
3. 支持 Markdown 渲染（代码高亮、表格）
4. 接入知识库检索（产品文档 / FAQ / PDF）
5. 增加“引用来源”展示，方便技术支持场景追溯

---

如需，我可以下一步继续给你补：
- PM2 一键部署脚本
- Nginx 反向代理配置
- 面向生产环境的 `server.js`（含重试、超时、限流、结构化日志）
