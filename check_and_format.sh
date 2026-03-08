#!/usr/bin/env bash
set -euo pipefail

# 获取脚本所在目录，默认使用项目根目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
root_dir="${1:-$PROJECT_ROOT}"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 检查 clang-format 是否安装
if ! command -v clang-format &> /dev/null; then
    echo -e "${RED}Error: clang-format is not installed${NC}"
    exit 1
fi

# 检查 clang-format 版本 (需要 >= 14)
CLANG_FORMAT_VERSION=$(clang-format --version | grep -oP '\d+' | head -1)
if [[ "$CLANG_FORMAT_VERSION" -lt 14 ]]; then
    echo -e "${YELLOW}Warning: clang-format version $CLANG_FORMAT_VERSION detected, version >= 14 recommended${NC}"
fi

echo "Scanning directory: $root_dir"
echo "Using clang-format version: $(clang-format --version | head -1)"

# 查找所有 C++ 文件
mapfile -t files < <(
    find "$root_dir" -type f \( -name "*.h" -o -name "*.hpp" -o -name "*.cpp" -o -name "*.cc" -o -name "*.cxx" -o -name "*.ipp" \) \
        -not -path "*/third_party/*" \
        -not -path "*/build*/*" \
        -not -path "*/.git/*" \
        -print 2>/dev/null | sort
)

if [[ ${#files[@]} -eq 0 ]]; then
    echo -e "${YELLOW}No C++ files found under: $root_dir${NC}"
    exit 0
fi

echo "Found ${#files[@]} C++ files to check"

# 检查不符合格式的文件
noncompliant=()
for file in "${files[@]}"; do
    if ! clang-format --dry-run --Werror "$file" >/dev/null 2>&1; then
        noncompliant+=("$file")
    fi
done

if [[ ${#noncompliant[@]} -eq 0 ]]; then
    echo -e "${GREEN}All files are clang-format compliant.${NC}"
else
    echo -e "${YELLOW}Files not matching clang-format (${#noncompliant[@]} files):${NC}"
    for file in "${noncompliant[@]}"; do
        echo "  $file"
    done

    echo ""
    echo "Formatting noncompliant files..."
    for file in "${noncompliant[@]}"; do
        clang-format -i "$file"
        echo -e "  ${GREEN}Formatted:${NC} $file"
    done
    echo -e "${GREEN}Formatting done.${NC}"
fi

# 检测超过 150 字符的行（clang-format 无法自动修复的超长行）
over_limit=()
for file in "${files[@]}"; do
    if awk 'length > 150 { exit 1 }' "$file"; then
        : # 没有超长行
    else
        over_limit+=("$file")
    fi
done

if [[ ${#over_limit[@]} -gt 0 ]]; then
    echo ""
    echo -e "${RED}WARNING: Files with lines exceeding 150 characters (cannot be auto-fixed):${NC}"
    for file in "${over_limit[@]}"; do
        echo -e "  ${YELLOW}$file${NC}"
        awk 'length > 150 { printf "    Line %d: %d chars\n", NR, length }' "$file"
    done
    exit 1
fi

echo ""
echo -e "${GREEN}Done. All checks passed.${NC}"
