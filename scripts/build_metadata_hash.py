import hashlib
import re
from datetime import datetime, timezone
from pathlib import Path


def build_id_now() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def compute_tree_checksum(root: Path, excluded_paths: set[Path]) -> str:
    digest = hashlib.sha256()
    for path in sorted(candidate for candidate in root.rglob("*") if candidate.is_file()):
        if path in excluded_paths or path.suffix == ".hash":
            continue
        relative_path = path.relative_to(root).as_posix().encode("utf-8")
        digest.update(relative_path)
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def read_firmware_build_id(path: Path) -> str:
    if not path.exists():
        return ""

    content = path.read_text(encoding="utf-8")
    build_id_match = re.search(r'RENOVENT_BUILD_ID\s+"([^"]+)"', content)
    return build_id_match.group(1).strip() if build_id_match else ""


def read_ui_build_id(path: Path) -> str:
    if not path.exists():
        return ""
    return path.read_text(encoding="utf-8").strip()


def get_hash_marker_path(root: Path, checksum: str) -> Path:
    return root / f"{checksum}.hash"


def has_hash_marker(root: Path, checksum: str) -> bool:
    return get_hash_marker_path(root, checksum).exists()


def replace_hash_marker(root: Path, checksum: str) -> None:
    marker_path = get_hash_marker_path(root, checksum)
    root.mkdir(parents=True, exist_ok=True)
    for candidate in root.glob("*.hash"):
        if candidate != marker_path:
            candidate.unlink()
    if not marker_path.exists():
        marker_path.touch()