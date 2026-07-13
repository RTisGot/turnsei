#!/usr/bin/env bash
# vcpkg installを実行し、SSL失効チェック起因のダウンロード失敗が出たら
# system curl (--ssl-no-revoke) で該当ファイルを取得してリトライする
set -u
cd /d/Turnsei/vcpkg

MAX_ITER=40
for i in $(seq 1 $MAX_ITER); do
  echo "=== attempt $i ==="
  LOG=/d/Turnsei/vcpkg_install_retry_$i.log
  ./vcpkg.exe install vulkan:x64-windows "ggml[vulkan]:x64-windows" llama-cpp:x64-windows > "$LOG" 2>&1
  code=$?
  if [ $code -eq 0 ]; then
    echo "SUCCESS"
    cat "$LOG"
    exit 0
  fi

  # look for a failed download URL/filename pair
  line=$(grep -B1 "curl operation failed" "$LOG" | grep "Downloading" | tail -1)
  if [ -n "$line" ]; then
    url=$(echo "$line" | sed -E 's/Downloading (\S+) ->.*/\1/')
    fname=$(echo "$line" | sed -E 's/.*-> (\S+)$/\1/')
    echo "Recovering: $fname  <-  $url"
    curl -L --ssl-no-revoke --max-time 120 -o "downloads/$fname" "$url" -w "HTTP:%{http_code} size:%{size_download}\n"
    continue
  fi

  # transient Windows file-lock (AV scanning newly created try_compile exe) - just retry
  if grep -q "could not be removed" "$LOG" || grep -q "プロセスはファイルにアクセスできません" "$LOG"; then
    echo "Transient file-lock during CMake try_compile detected, retrying..."
    continue
  fi

  echo "No recoverable error found. Dumping log tail:"
  tail -60 "$LOG"
  exit 1
done

echo "Exceeded max iterations"
exit 1
