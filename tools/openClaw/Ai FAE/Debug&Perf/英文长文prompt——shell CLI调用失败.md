
## 调用链路

Copy

```
用户浏览器 (index.html)
    ↓ HTTP POST /api/chat
server.js (Express, 端口8080)
    ↓ 1. 拆解需求构建 prompt + 知识库检索结果（切片+RGA+过滤）(就是在這一步上可能會比較蠢,如果沒拆解好需求構建好prompt(本來prompt是agent構建的,結果這種架構變成脚本構建了?(待確認)))
    ↓ 2. exec() 执行 shell 命令
openclaw agent CLI (命令行工具形式调用openclaw)(就是在這一步上比較慢,每次要調用新的agent cli shell(待確認))
    ↓ 3. 解析 prompt，调用LLM大模型
AI Model (DeepSeek等)
    ↓ 4. 生成回复
openclaw agent CLI
    ↓ 5. 返回 JSON stdout
server.js
    ↓ 6. 解析回复，流式返回
用户浏览器
```

**失败点**：步骤2 → 3

具体说：`server.js` 调用了 `openclaw agent` 这个 **CLI 命令行工具**来发消息给大模型。它用 Node.js 的 `exec()` 执行了一段 shell 命令：

Copy

```bash
openclaw agent --to +8613800000000 --message "【很长的prompt】" --json --thinking off --timeout 90
```

**问题出在**：那个 `--message` 后面的 prompt 里包含了 **用户原始问题中的英文双引号（如 "Wi-Fi"、"4G"、"RS485"）**，这些双引号嵌在 shell 命令的字符串里，导致 shell 解析时引号嵌套混乱，最终报错：

Copy

```
/bin/sh: 1: Syntax error: end of file unexpected
```

所以是 **server.js → shell → openclaw agent CLI** 这一截调用链的 shell 引号解析出了问题，不是 AI 模型本身或者 openclaw 服务的问题。

**修复方式**：`exec()` 换成 `execFile()` 或 `spawn()`，把参数用数组传进去，避免 shell 引号解析。