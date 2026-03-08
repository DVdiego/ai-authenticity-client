# Proposed License Position

This folder documents a proposed licensing position for distributed builds of the Qt client.
It is intentionally separated from the repository root so it can be reviewed with the TFG tutor
before any public release.

## Recommended approach

- Distributed application builds: proprietary distribution under the EULA in `EULA.md`
- Source code publication, if chosen: `PolyForm-Noncommercial-1.0.0`
- Model weights and packaged models: do not treat as open source by default; distribute only if
  dataset rights, model provenance, and redistribution rights have been reviewed

## Why this approach

- The project is intended for research, teaching, and TFG demonstration use
- A non-commercial restriction is incompatible with the Open Source Definition
- Creative Commons licenses are not recommended for software

## Official references to review before publishing

- PolyForm Noncommercial 1.0.0: `https://polyformproject.org/licenses/noncommercial/1.0.0`
- Open Source Definition: `https://opensource.org/osd`
- Creative Commons guidance for software: `https://creativecommons.org/faq/#can-i-apply-a-creative-commons-license-to-software`

## Pending decision

Before distributing outside the project team:

1. Confirm with the tutor whether the repository will remain private, shared only with the tribunal,
   or published publicly.
2. Confirm whether model weights can be redistributed.
3. If the repository is made public, replace this file with the final approved license text.
