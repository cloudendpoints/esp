# Endpoints Runtime Versioning

The committed Endpoints Runtime version uses format: `major.minor.revision`.
During release build, the version is enhanced with an additional suffix,
yielding:

    major.minor.revision~build.commitsha


The `built_using` record is similarly enhanced by build bots with
Bazel version information.
