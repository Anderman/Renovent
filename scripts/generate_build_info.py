from pathlib import Path
import sys

Import("env")

SCRIPT_DIR = Path(env["PROJECT_DIR"]) / "scripts"
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from build_metadata_hash import build_id_now
from build_metadata_hash import compute_tree_checksum
from build_metadata_hash import has_hash_marker
from build_metadata_hash import replace_hash_marker
from build_metadata_hash import read_firmware_build_id
from build_metadata_hash import read_ui_build_id
from build_metadata_release import copy_release_artifact
from build_metadata_release import resolve_release_base_url
from build_metadata_release import write_text

PROJECT_DIR = Path(env["PROJECT_DIR"])
SOURCE_DIR = PROJECT_DIR / "src"
UI_DIR = PROJECT_DIR / "data"
UI_HASH_DIR = PROJECT_DIR / "ui-hashes"
HEADER_PATH = PROJECT_DIR / "src" / "build_info.generated.h"
UI_VERSION_PATH = PROJECT_DIR / "data" / "version.txt"
LATEST_MANIFEST_PATH = PROJECT_DIR / "release" / "latest.json"
DEFAULT_RELEASE_BASE_URL = "https://raw.githubusercontent.com/Anderman/Renovent/main/release"

RELEASE_PLAN: dict[str, object] = {}


def resolve_next_build_id(firmware_changed: bool,
                          spiffs_changed: bool,
                          current_firmware_build_id: str,
                          current_spiffs_build_id: str) -> str:
    if firmware_changed or spiffs_changed:
        return build_id_now()

    if not current_firmware_build_id or not current_spiffs_build_id:
        return build_id_now()

    return ""


def build_component_state(current_build_id: str, checksum: str, changed: bool, next_build_id: str) -> dict:
    build_id = next_build_id if (changed or not current_build_id) else current_build_id
    return {
        "checksum": checksum,
        "buildId": build_id,
        "changed": changed,
    }


def create_release_plan() -> dict[str, object]:
    firmware_checksum = compute_tree_checksum(SOURCE_DIR, {HEADER_PATH})
    spiffs_checksum = compute_tree_checksum(UI_DIR, {UI_VERSION_PATH})
    firmware_changed = not has_hash_marker(SOURCE_DIR, firmware_checksum)
    spiffs_changed = not has_hash_marker(UI_HASH_DIR, spiffs_checksum)

    current_firmware_build_id = read_firmware_build_id(HEADER_PATH)
    current_spiffs_build_id = read_ui_build_id(UI_VERSION_PATH)
    next_build_id = resolve_next_build_id(
        firmware_changed,
        spiffs_changed,
        current_firmware_build_id,
        current_spiffs_build_id,
    )

    if firmware_changed:
        replace_hash_marker(SOURCE_DIR, firmware_checksum)
    if spiffs_changed:
        replace_hash_marker(UI_HASH_DIR, spiffs_checksum)

    return {
        "firmware": build_component_state(current_firmware_build_id, firmware_checksum, firmware_changed, next_build_id),
        "spiffs": build_component_state(current_spiffs_build_id, spiffs_checksum, spiffs_changed, next_build_id),
        "releaseBaseUrl": resolve_release_base_url(LATEST_MANIFEST_PATH, DEFAULT_RELEASE_BASE_URL),
    }


def write_generated_metadata(firmware: dict, spiffs: dict) -> None:
    firmware_build_id = str(firmware["buildId"])
    spiffs_build_id = str(spiffs["buildId"])

    header_content = (
        "#pragma once\n\n"
        f'#define RENOVENT_BUILD_ID "{firmware_build_id}"\n'
    )
    ui_version_content = f"{spiffs_build_id}\n"

    if (
        not firmware["changed"]
        and not spiffs["changed"]
        and HEADER_PATH.exists()
        and UI_VERSION_PATH.exists()
        and HEADER_PATH.read_text(encoding="utf-8") == header_content
        and UI_VERSION_PATH.read_text(encoding="utf-8") == ui_version_content
    ):
        return

    write_text(HEADER_PATH, header_content)
    write_text(UI_VERSION_PATH, ui_version_content)


def on_firmware_built(*args, **kwargs) -> None:
    copy_release_artifact(
        "firmware",
        Path(env.subst("$BUILD_DIR")) / f"{env.subst('$PROGNAME')}.bin",
        RELEASE_PLAN,
        PROJECT_DIR,
        LATEST_MANIFEST_PATH,
    )


def on_spiffs_built(*args, **kwargs) -> None:
    copy_release_artifact(
        "spiffs",
        Path(env.subst("$BUILD_DIR")) / "spiffs.bin",
        RELEASE_PLAN,
        PROJECT_DIR,
        LATEST_MANIFEST_PATH,
    )


def generate_build_metadata(*args, **kwargs) -> None:
    global RELEASE_PLAN
    RELEASE_PLAN = create_release_plan()
    write_generated_metadata(RELEASE_PLAN["firmware"], RELEASE_PLAN["spiffs"])


generate_build_metadata()
env.AddPreAction("buildfs", generate_build_metadata)
env.AddPreAction("uploadfs", generate_build_metadata)
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", on_firmware_built)
env.AddPostAction("buildfs", on_spiffs_built)
env.AddPostAction("uploadfs", on_spiffs_built)