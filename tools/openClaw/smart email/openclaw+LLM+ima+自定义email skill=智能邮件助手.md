好 — 我把把从 IMA 拉取资料作为“回邮件来源”的完整可执行流程写清楚了：包括准备、验证、查询、取文、引用格式、注意事项与示例命令。你可以直接按步骤操作或授权我代执行。

一、准备（只做一次）
- 凭证：在主机上设置任一方法（优先级 环境变量 → 配置文件）
  - 环境变量（临时会话）：
    export IMA_OPENAPI_CLIENTID="你的ClientID"
    export IMA_OPENAPI_APIKEY="你的APIKey"
  - 或永久文件（推荐）：
    mkdir -p ~/.config/ima
    echo "your_client_id" > ~/.config/ima/client_id
    echo "your_api_key" > ~/.config/ima/api_key
- 安全：凭证仅发往官方域名 ima.qq.com（技能说明中已约束，不写日志、不外传）。

二、验证连通性（快速检查）
- 列出笔记本（示例，limit≤20）：
  curl -s -X POST "https://ima.qq.com/openapi/note/v1/list_note_folder_by_cursor" \
    -H "ima-openapi-clientid: $CLIENTID" \
    -H "ima-openapi-apikey: $APIKEY" \
    -H "Content-Type: application/json" \
    -d '{"cursor":"0","limit":20}'
- 成功返回 code=0 即认证与网络可用。若返回 apiKey鉴权失败 或 频控，请检查凭证/频率。

三、检索并定位要引用的文档（交互式流程）
1. 列出笔记本（见上）。
2. 在某个笔记本内列笔记（分页）：
   POST /openapi/note/v1/list_note_by_folder_id
   body: {"folder_id":"<folder_id>","cursor":"","limit":20}
3. 从列表中选择目标文档，记下 doc_id 与 title。

示例（获取 folder → 列出 notes → 取 doc_id）可用 jq 自动化处理（我已为你做过脚本）。

四、获取笔记正文（用于引用或插入邮件）
- 接口：POST /openapi/note/v1/get_doc_content
  body 示例：
  {"doc_id":"<doc_id>","target_content_format":0}
- 返回的 content 就是纯文本（或 JSON/Markdown 选项），把需要的段落摘出来作为邮件引用。

命令示例：
curl -s -X POST "https://ima.qq.com/openapi/note/v1/get_doc_content" \
  -H "ima-openapi-clientid: $CLIENTID" \
  -H "ima-openapi-apikey: $APIKEY" \
  -H "Content-Type: application/json" \
  -d '{"doc_id":"'"$DOCID"'","target_content_format":0}' | jq -r '.data.content'

五、把 IMA 内容作为邮件来源写入邮件的建议格式（模板）
- 简洁引用 + 来源元数据（便于核验）：
  引用示例段落（直接粘入邮件正文）…

  — 来源：IMA 笔记 “<标题>” (doc_id: <doc_id>), 笔记本: “<folder_name>”, 抓取时间: 2026-04-08T12:34:56Z  
  (检索接口：POST https://ima.qq.com/openapi/note/v1/get_doc_content, 请求者：your_account)

- 说明为什么信任该资料（例如公司内部笔记、已确认作者等），并在必要时附上完整内容或附件（需用户允许）。

六、如果要把 IMA 数据作为“回复邮件的证据链/引用库”长期使用（可选项）
- 建议建立本地缓存/索引：
  - 定期拉取并缓存 metadata（title, doc_id, folder, summary, modify_time）。
  - 为常用查询做向量索引（FAISS/Annoy）或关键词索引，加速匹配。
- 使用缓存索引做检索 → 得到 doc_id → 实时从 IMA 拉取最新正文并插入邮件（保证内容是最新且有权访问）。
- 优点：快速、可记录来源时间；缺点：需周期性同步并注意隐私/存储策略。

七、写操作与注意事项（重要）
- 读操作（get_doc_content、list）通常安全；写操作（import_doc、append_doc）会直接修改你 IMA 帐号数据，执行前必须获得你明确许可。写操作的接口：
  - import_doc: /openapi/note/v1/import_doc（从 Markdown 新建）
  - append_doc: /openapi/note/v1/append_doc（追加到已有 doc_id）
- 强制编码检查（非常重要，来自 SKILL.md）：
  - notes 写入类接口前必须确保 title/content 为合法 UTF‑8（否则会乱码且无法修复）。
  - 在 PowerShell 5.1 上发送写请求需把 JSON 转为 UTF‑8 字节数组后发送（技能文档有示例）。
- 速控/配额：注意 API 可能有频率限制（错误码如 20002 表示频率超限），批量操作请做速率控制与重试策略。

八、自动化/脚本化示例（读取流程一键化）
- 列出所有笔记并把检索结果保存到本地 JSON（伪命令）：
  1) list folders (cursor=0 → page)
  2) for each folder list notes (cursor iterate)
  3) for each note call get_doc_content → 保存文件 / 记录元数据（title/doc_id/time）
- 我可以为你写好脚本，按你允许的频率执行并把结果存到 /home/joes/.openclaw/workspace/ima_cache/，供生成邮件时引用。

九、合规与隐私（必须确认）
- 在邮件中引用私人或敏感笔记前，请确认邮件收件人有相应访问权限；不要未经许可把私人笔记泄露到公共邮件/群组。
- 我不会把你的 API Key 发送到其他域名；写入操作前我会再次确认并展示将要写入的内容供你批准。

十、我可以现在帮你做的事（选项）
- A. 帮你建立“检索→插入邮件”的自动脚本（我来写并你批准运行）。  
- B. 现在按你指示列出/导出所有现有笔记并生成可搜索的本地索引。  
- C. 给你一个最小化的交互流程（shell 命令 + jq）让你在本地自己执行。  
- D. 现在做一次演示：用当前凭证列出笔记本 → 选一篇取正文 → 生成邮件引用模板并回传给你（演示会真实读 IMA，但不会写）。

请选择你要我做的下一步（A/B/C/D），或告诉我你希望的引用格式，我立即开始。