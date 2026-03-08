Local distribution metadata is intentionally not tracked.

Keep recipient-specific files only on the machine that prepares the build:
- `build_info.json`
- `license.json`
- `license.sig`

Use these templates as the starting point:
- `build_info.template.json`
- `license.template.json`

Typical flow:
1. Copy the template files to `build_info.json` and `license.json`.
2. Fill in the real recipient, license id, dates, and channel.
3. Regenerate `license.sig` with `cliente/scripts/sign_distribution_license.sh`.
4. Package the app with `cliente/scripts/package_macos_local.sh`.
