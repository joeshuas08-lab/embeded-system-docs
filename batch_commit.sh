#!/bin/bash
# 使用 -z 让 git 以 null 分离文件名，配合 read -d '' 处理文件名中的空格
# 先收集所有未跟踪文件到一个临时文件中
git ls-files --others --exclude-standard -z > untracked_files.null

# 将 null 分隔的文件读取到数组中
file_array=()
while IFS= read -r -d '' file; do
    file_array+=("$file")
done < untracked_files.null

total=${#file_array[@]}
batch_size=10

echo "Total untracked files: $total"

for ((i=0; i<total; i+=batch_size)); do
  # 获取当前批次的文件
  batch=("${file_array[@]:i:batch_size}")
  echo "Processing batch $((i/batch_size + 1)): count ${#batch[@]}"
  
  # 添加文件
  for file in "${batch[@]}"; do
    git add "$file"
    echo "  Added: $file"
  done
  
  # 检查是否有文件被成功 add（防止空提交）
  if ! git diff --cached --quiet; then
    commit_msg="Add new files - batch $((i/batch_size + 1))"
    git commit -m "$commit_msg"
    echo "Committed with message: $commit_msg"
    
    # 推送，并处理可能的网络失败（重试一次）
    if ! git push origin main; then
        echo "Push failed, retrying once..."
        sleep 2
        git push origin main
    fi
    echo "Pushed to origin main"
  else
    echo "No files staged in this batch."
  fi
  echo "-----------------------------------"
done

# 清理临时文件
rm untracked_files.null
