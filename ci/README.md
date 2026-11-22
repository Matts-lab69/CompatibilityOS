# CI design

Future GitHub Actions builds pinned Wine sources and a rebaseable patch series, stages target-specific binaries, and publishes versioned artifacts with checksums, signatures and required source/notices. Verify build options and runtime dependencies on supported targets before publication. Pin actions and inputs, limit build concurrency by observed memory, and separate release publication from validation. No workflows, builds or publishes are configured.
