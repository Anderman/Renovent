import json
import os
import shutil
from pathlib import Path


def read_build_state(path: Path) -> dict:
    if not path.exists():
        return {}

    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return {}


def write_text(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def resolve_release_base_url(manifest_path: Path, default_release_base_url: str) -> str:
    configured = os.environ.get("RENOVENT_RELEASE_BASE_URL", "").strip()
    if configured:
        return configured.rstrip("/")

    manifest = read_build_state(manifest_path)
    for artifact_name in ("firmware", "spiffs"):
        artifact = manifest.get(artifact_name, {})
        if not isinstance(artifact, dict):
            continue
        download_url = str(artifact.get("downloadUrl", "")).strip()
        marker = f"/{artifact_name}/"
        if marker in download_url:
            return download_url.split(marker, 1)[0].rstrip("/")

    return default_release_base_url


def write_manifest(build_state: dict, manifest_path: Path) -> None:
    firmware = build_state["firmware"]
    spiffs = build_state["spiffs"]
    firmware_build_id = str(firmware["buildId"])
    spiffs_build_id = str(spiffs["buildId"])

    base_url = build_state["releaseBaseUrl"]
    manifest_content = json.dumps(
        {
            "firmware": {
                "buildId": firmware_build_id,
                "downloadUrl": f"{base_url}/firmware/{firmware_build_id}.bin",
            },
            "spiffs": {
                "buildId": spiffs_build_id,
                "downloadUrl": f"{base_url}/spiffs/{spiffs_build_id}.bin",
            },
        },
        indent=2,
    ) + "\n"
    write_text(manifest_path, manifest_content)


def copy_release_artifact(component: str,
                          source_path: Path,
                          build_state: dict,
                          project_dir: Path,
                          manifest_path: Path) -> None:

    component_state = build_state[component]
    target_path = project_dir / "release" / component / f"{component_state['buildId']}.bin"
    if component_state["changed"] or not target_path.exists():
        target_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source_path, target_path)

    write_manifest(build_state, manifest_path)