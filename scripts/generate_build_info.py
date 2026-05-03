import os
import re
from datetime import datetime, timezone
from pathlib import Path

Import("env")


def build_id_now() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def read_existing_build_id(header_path: Path, version_path: Path) -> str:
    if header_path.exists():
        header_content = header_path.read_text(encoding="utf-8")
        match = re.search(r'RENOVENT_BUILD_ID\s+"([^"]+)"', header_content)
        if match:
            return match.group(1).strip()

    if version_path.exists():
        return version_path.read_text(encoding="utf-8").strip()

    return ""


def write_if_changed(path: Path, content: str) -> None:
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def generate_build_metadata(*args, **kwargs) -> None:
    project_dir = Path(env["PROJECT_DIR"])
    header_path = project_dir / "src" / "build_info.generated.h"
    version_path = project_dir / "data" / "version.txt"
    build_id = os.environ.get("RENOVENT_BUILD_ID", "").strip()
    if not build_id:
        build_id = read_existing_build_id(header_path, version_path) or build_id_now()

    header_content = (
        "#pragma once\n\n"
        f'#define RENOVENT_BUILD_ID "{build_id}"\n'
    )
    write_if_changed(header_path, header_content)

    write_if_changed(version_path, f"{build_id}\n")


generate_build_metadata()
env.AddPreAction("buildfs", generate_build_metadata)