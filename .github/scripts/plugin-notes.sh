#!/usr/bin/env bash
# Generate the per-plugin update-status section of the release notes.
#
# Usage: plugin-notes.sh <base-ref>
#   <base-ref>  git ref to compare against (normally the latest stable
#               release tag); pass ROOT to compare against the first commit.
#
# Emits Markdown on stdout:
#   - one table row per plugin: its version and whether it changed since
#     <base-ref>. The version is the plugin's last-update date (generated
#     into <plugin>/version.h by build.bat; the same value the DLL file
#     properties and the about dialog show),
#   - one commit list per updated plugin (max 10),
#   - an "other changes" list for commits that touched no plugin directory.
#
# Dates are CST (UTC+8) computed manually: the runners' Git Bash has no
# zoneinfo, so TZ=Asia/Shanghai silently fails there.

set -euo pipefail

repo="${GITHUB_REPOSITORY:-dsqm/CLaunch-Plugins}"
base=${1:?usage: plugin-notes.sh <base-ref>}
if [ "$base" = "ROOT" ]; then
  base=$(git rev-list --max-parents=0 HEAD)
fi

# Plugin directories are self-describing: each holds <name>.def.
plugins=$(for d in */; do
  p=${d%/}
  if [ -f "$p/$p.def" ]; then echo "$p"; fi
done)

cst_date() {
  date -u -d "@$(( $1 + 8*3600 ))" +%Y-%m-%d
}

version_of() {
  sed -n 's/^#define PLUGIN_VERSION_A[[:space:]]*"\([^"]*\)".*/\1/p' "$1/version.h"
}

echo "插件版本号 = 该插件最后一次更新的日期（与 DLL 文件属性、「关于此插件」弹窗一致）。"
echo ""
echo "| 插件 | 版本 | 状态 |"
echo "| --- | --- | --- |"
updated=""
for p in $plugins; do
  ts=$(git log -1 --format=%ct "$base..HEAD" -- "$p" || true)
  if [ -n "$ts" ]; then
    updated="$updated $p"
    status="✅ 本次更新"
    last_ts=$ts
  else
    status="－ 未更新"
    last_ts=$(git log -1 --format=%ct HEAD -- "$p" || true)
  fi
  ver=$(version_of "$p")
  if [ -z "$ver" ]; then
    ver=$(cst_date "${last_ts:-0}")   # version.h not generated yet -> fall back to the commit date
  fi
  printf '| %s | %s | %s |\n' "$p" "$ver" "$status"
done

for p in $updated; do
  echo ""
  echo "#### $p 更新内容"
  echo ""
  git log --oneline -10 "$base..HEAD" -- "$p" | while read -r line; do
    s=${line%% *}; m=${line#* }
    echo "- [\`$s\`](https://github.com/$repo/commit/$s) $m"
  done
done

# Commits that touched no plugin directory (CI, build scripts, READMEs);
# commits touching a plugin dir already appear in that plugin's list.
# Diff lists are captured before matching: grep -q in a pipeline can kill
# git via SIGPIPE, which set -o pipefail turns into a fatal error.
pat=$(echo "$plugins" | tr '\n' '|' | sed 's/|$//')
n=0
for c in $(git rev-list "$base..HEAD"); do
  files=$(git diff-tree -r --no-commit-id --name-only "$c")
  if [ -z "$files" ]; then
    continue
  fi
  if echo "$files" | grep -qE "^($pat)/"; then
    continue
  fi
  subject=$(git log -1 --format='%h %s' "$c")
  echo "- [\`$c\`](https://github.com/$repo/commit/$c) $subject"
  n=$((n + 1))
  if [ "$n" -ge 10 ]; then break; fi
done > other.md
if [ -s other.md ]; then
  echo ""
  echo "#### 其他变更"
  echo ""
  cat other.md
fi
rm -f other.md
