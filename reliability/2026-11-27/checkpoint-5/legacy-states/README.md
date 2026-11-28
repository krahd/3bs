# Prior-executable compatibility fixtures

These files were newly generated from the audited `06066967b243e250d97da37ea0195b31f5c9521c` executable in CI run 46086793103. They are not historical user sessions. The capture harness and native recall test remain in `tests/`.

The portable `.3bs` files are preserved byte for byte. Host `.bin` bytes are preserved as base64 text for repository transport. Decode each `.bin.base64` file with Python `base64.decodebytes`; the decoded filename and SHA-256 must match `PROVENANCE.json`. No plug-in executable is contained in these fixtures.
