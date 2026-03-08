# TFG Distribution Checklist

Use this before sharing the application outside the immediate development machine.

## Tutor review

1. Confirm whether the TFG software will remain private, tribunal-only, or publicly accessible.
2. Confirm whether an embargo or confidentiality request is needed.
3. Confirm whether model weights and sample datasets can be redistributed.

## Packaging decision

1. Ship the Qt native build, not the Python development path.
2. Freeze one native package version and one matching source checkpoint.
3. Remove development-only options from the public build if they are not needed.

## Legal material to include

1. Add the final approved license text.
2. Add the final EULA or terms of use.
3. Add third-party notices.
4. Show an in-app legal notice before first use.

## Abuse reduction

1. Disable or hide debug-only comparison features in the public build if unnecessary.
2. Limit repeated batch execution if abuse is a concern.
3. Validate model package hashes before loading.
4. Keep logs minimal and avoid shipping private sample data.

## Release hygiene

1. Build a release configuration.
2. Verify behavior on a clean machine without the Python environment.
3. Test with the exact packaged model used in the release.
4. Keep a versioned changelog for the release shown to the tutor or tribunal.
