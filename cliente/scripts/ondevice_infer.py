#!/usr/bin/env python3
import argparse
import io
import json
import os
import sys
from contextlib import redirect_stdout
from pathlib import Path


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser()
    p.add_argument("--file", required=True, help="Absolute path to image file")
    p.add_argument("--include-features", action="store_true", help="Include extracted feature vector in output")
    return p.parse_args()


def fail(message: str, code: int = 1) -> int:
    payload = {"ok": False, "error": message}
    print(json.dumps(payload, ensure_ascii=False))
    return code


def main() -> int:
    args = parse_args()
    file_path = Path(args.file).expanduser().resolve()
    if not file_path.exists() or not file_path.is_file():
        return fail("File not found.")

    backend_dir = Path(
        os.getenv("AI_AUTH_BACKEND_DIR", "/Users/macmini/tfg_ia_video/desarrollo/backend")
    ).expanduser().resolve()

    if not backend_dir.exists():
        return fail(f"Backend directory not found: {backend_dir}")

    sys.path.insert(0, str(backend_dir))

    try:
        # Suppress noisy model-loading prints to keep stdout as pure JSON.
        with redirect_stdout(io.StringIO()):
            from app.services.analysis_services import AnalysisService
            from app.services.feature_extraction import extract_features
            service = AnalysisService()
            result = service.analyze(file_path)
            features_res = extract_features(file_path) if args.include_features else None

        if hasattr(result, "model_dump"):
            data = result.model_dump()
        else:
            data = result.dict()

        if features_res is not None:
            data["debug_features"] = [float(x) for x in features_res.get("features", [])]
            data["debug_media_type"] = features_res.get("type", "")

        print(json.dumps(data, ensure_ascii=False))
        return 0
    except Exception as exc:
        return fail(str(exc))


if __name__ == "__main__":
    raise SystemExit(main())
