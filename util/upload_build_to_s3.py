#!/usr/bin/env python3
"""
Upload ESPRelayBoard build artifacts to S3.

Uploads:
- build_info.json
- ESPRelayBoard.bin
- storage.bin

Destinations:
- s3://dist-repo-public/firmware/ESPRelayBoard/<IDF_TARGET>/latest/
- s3://dist-repo-public/firmware/ESPRelayBoard/<IDF_TARGET>/<DEVICE_SW_VERSION_NUM>/

Auth:
- Uses boto3 default credential chain (incl. local AWS profiles).
- Optionally pass --profile to force a specific AWS profile.

Requires:
  pip install boto3
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
from typing import Dict, Tuple

import mimetypes

import boto3
from botocore.exceptions import BotoCoreError, ClientError


BUCKET = "dist-repo-public"
# IMPORTANT: S3 keys do NOT start with "/" — treat this as a prefix.
BASE_PREFIX = "firmware/ESPPressureSensor/"


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Upload ESPPressureSensor build artifacts to S3.")
    p.add_argument(
        "source_root",
        help="Source folder root (project root).",
    )
    p.add_argument(
        "--build-info",
        default=None,
        help="Path to build_info.json (default: <source_root>/build/build_info.json).",
    )
    p.add_argument(
        "--profile",
        default=os.getenv("AWS_PROFILE", "ESPPressureSensor-repo"),
        help="AWS profile name to use (default: boto3 standard resolution).",
    )
    p.add_argument(
        "--region",
        default=None,
        help="AWS region (optional). If omitted, uses profile/env/default resolution.",
    )
    p.add_argument(
        "--dry-run",
        action="store_true",
        help="Print what would be uploaded without uploading.",
    )
    return p.parse_args()


def load_build_info(path: Path) -> Dict[str, str]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        raise SystemExit(f"ERROR: build info file not found: {path}")
    except json.JSONDecodeError as e:
        raise SystemExit(f"ERROR: invalid JSON in {path}: {e}")

    required = ["DEVICE_SW_VERSION_NUM", "IDF_TARGET"]
    for k in required:
        if k not in data or not str(data[k]).strip():
            raise SystemExit(f"ERROR: build info missing required key '{k}' in {path}")

    return data


def ensure_file(path: Path) -> None:
    if not path.exists() or not path.is_file():
        raise SystemExit(f"ERROR: required file not found: {path}")


def s3_key(*parts: str) -> str:
    # Join key fragments safely (no leading slash in S3 keys).
    cleaned = []
    for p in parts:
        p = p.replace("\\", "/")
        p = p.strip("/")
        if p:
            cleaned.append(p)
    return "/".join(cleaned)


def make_session(profile: str | None, region: str | None):
    # If profile is provided, force it; otherwise boto3 resolves env/role/default profile.
    if profile:
        return boto3.Session(profile_name=profile, region_name=region)
    return boto3.Session(region_name=region)


def upload_one(s3_client, local_path: Path, bucket: str, key: str, dry_run: bool) -> None:
    if dry_run:
        print(f"[DRY-RUN] upload: {local_path} -> s3://{bucket}/{key}")
        return

    try:
        content_type, _ = mimetypes.guess_type(str(local_path))
        if content_type is None:
            content_type = "application/octet-stream"

        s3_client.upload_file(
            str(local_path),
            bucket,
            key,
            ExtraArgs={
                "ContentType": content_type,
                "CacheControl": "no-cache"
            }
        )

        print(f"Uploaded: {local_path.name} -> s3://{bucket}/{key} ({content_type})")

    except (BotoCoreError, ClientError) as e:
        raise SystemExit(f"ERROR: upload failed for {local_path} -> s3://{bucket}/{key}: {e}")


def main() -> None:
    args = parse_args()

    source_root = Path(args.source_root).expanduser().resolve()
    build_dir = source_root / "build"

    build_info_path = (
        Path(args.build_info).expanduser().resolve()
        if args.build_info
        else (build_dir / "build_info.json")
    )

    # Local artifact paths
    artifact_build_info = build_dir / "build_info.json"
    artifact_firmware = build_dir / "ESPZPressureSensor.bin"
    artifact_storage = build_dir / "storage.bin"

    # Validate files exist
    ensure_file(build_info_path)
    ensure_file(artifact_build_info)
    ensure_file(artifact_firmware)
    ensure_file(artifact_storage)

    build_info = load_build_info(build_info_path)
    version = str(build_info["DEVICE_SW_VERSION_NUM"]).strip()
    target = str(build_info["IDF_TARGET"]).strip().lower()

    if not target:
        raise SystemExit(f"ERROR: build info missing valid 'IDF_TARGET' in {build_info_path}")

    # Determine destinations
    latest_prefix = s3_key(BASE_PREFIX, target, "latest")
    version_prefix = s3_key(BASE_PREFIX, target, version)

    # Create session + client
    session = make_session(args.profile, args.region)
    s3_client = session.client("s3")

    # Optional sanity check
    try:
        sts = session.client("sts")
        ident = sts.get_caller_identity()
        arn = ident.get("Arn", "unknown")
        acct = ident.get("Account", "unknown")
        print(f"AWS identity: {arn} (account {acct})")
    except Exception as e:
        print(f"Note: couldn't call STS get_caller_identity (continuing): {e}")

    uploads: Tuple[Tuple[Path, str], ...] = (
        (artifact_build_info, "build_info.json"),
        (artifact_firmware, "ESPZPressureSensor.bin"),
        (artifact_storage, "storage.bin"),
    )

    for local_path, filename in uploads:
        key_latest = s3_key(latest_prefix, filename)
        key_version = s3_key(version_prefix, filename)

        upload_one(s3_client, local_path, BUCKET, key_latest, args.dry_run)
        upload_one(s3_client, local_path, BUCKET, key_version, args.dry_run)

    print("Done.")


if __name__ == "__main__":
    main()