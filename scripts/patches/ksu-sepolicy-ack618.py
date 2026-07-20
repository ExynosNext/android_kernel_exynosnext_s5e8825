#!/usr/bin/env python3
"""Patch KSU sepolicy.c: disable the Samsung-private config-fixup block.

The block references policydb.android_netlink_route and
POLICYDB_CONFIG_ANDROID_NETLINK_* which don't exist in ACK 6.18.

We wrap the entire if-block (including the 'static const size_t kConfigOff'
and its closing brace) in #if 0 / #endif.
"""
import sys, re

path = sys.argv[1]
with open(path, encoding='utf-8') as f:
    lines = f.readlines()

# Find the marker comment line
marker = '// https://android-review.googlesource.com/c/kernel/common/+/3009995'
start_idx = None
for i, line in enumerate(lines):
    if marker in line:
        start_idx = i
        break

if start_idx is None:
    print(f"Marker not found in {path} — may already be patched")
    sys.exit(0)

# The block to wrap is:
#   // comment...
#   // fixup config
#   // 4*2+8+4
#   static const size_t kConfigOff = 20;
#   if (len >= kConfigOff + sizeof(u32)) {
#       ...
#   }
#
# We need to find the closing '}' of this if block. The if block is
# indented with 4 spaces. We track brace depth.
brace_depth = 0
if_seen = False
end_idx = None
for i in range(start_idx, len(lines)):
    line = lines[i]
    if 'if (len >= kConfigOff' in line:
        if_seen = True
    if if_seen:
        brace_depth += line.count('{')
        brace_depth -= line.count('}')
        if brace_depth == 0 and '{' in lines[i-1] or (brace_depth == 0 and '}' in line and if_seen):
            end_idx = i
            break

if end_idx is None:
    # Fallback: find the line with just '    }' after the if block
    for i in range(start_idx + 10, min(start_idx + 30, len(lines))):
        if lines[i].strip() == '}':
            end_idx = i
            break

if end_idx is None:
    print(f"Could not find end of config-fixup block in {path}")
    sys.exit(1)

# Insert #if 0 before the marker and #endif after the closing brace
lines.insert(start_idx, '#if 0 // PATCHED for ACK 6.18: Samsung-private SELinux fields absent\n')
# end_idx shifted by +1 due to insertion above
lines.insert(end_idx + 2, '#endif\n')

with open(path, 'w', encoding='utf-8') as f:
    f.writelines(lines)
print(f"Patched {path}: wrapped lines {start_idx+1}-{end_idx+1} in #if 0")
