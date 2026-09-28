# Pyhton script to remove the ARM platform - courtesy of Claude.

import re
from pathlib import Path

patterns = [
    # Multi-line: <PropertyGroup|ItemDefinitionGroup|ImportGroup|ItemGroup Condition="...|ARM'"> ... </same tag>
    re.compile(
        r"""^[ \t]*<(PropertyGroup|ItemDefinitionGroup|ImportGroup|ItemGroup)\b[^>]*\bCondition="[^"]*\|ARM'"[^>]*>
            .*?
            </\1>[ \t]*\r?\n
        """,
        re.MULTILINE | re.DOTALL | re.VERBOSE,
    ),
    # Multi-line: <ProjectConfiguration Include="Debug|ARM"> ... </ProjectConfiguration>
    re.compile(
        r"""^[ \t]*<ProjectConfiguration\b[^>]*\bInclude="[^"]*\|ARM"[^>]*>
            .*?
            </ProjectConfiguration>[ \t]*\r?\n
        """,
        re.MULTILINE | re.DOTALL | re.VERBOSE,
    ),
    # Single-line: <ExcludedFromBuild Condition="...|ARM'">true</ExcludedFromBuild> (any element)
    re.compile(
        r"""^[ \t]*<(\w+)\b[^>]*\bCondition="[^"]*\|ARM'"[^>]*>[^<\r\n]*</\1>[ \t]*\r?\n""",
        re.MULTILINE | re.VERBOSE,
    ),
]

files = list(Path(".").rglob("*.vcxproj")) + list(Path(".").rglob("*.vcxproj.user"))

for f in files:
    text = f.read_bytes().decode("utf-8")
    total = 0
    for p in patterns:
        text, n = p.subn("", text)
        total += n
    if total:
        f.write_bytes(text.encode("utf-8"))
        print(f"{f}: removed {total} block(s)")