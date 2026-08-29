# GameAnimationSampleCpp Development Instructions

- Read [README.md](README.md) and [the architecture notes](Docs/Architecture.md). Inspect Git status and the existing implementation before changing it.
- Preserve the PascalCase Unreal project, runtime module, target names, and asset paths. Use English in source, documentation, and concise commit summaries.
- Put reusable behavior in native C++ where practical. Document public API contracts and non-obvious lifecycle or ownership rules with Doxygen. Verify existing asset references before replacing Blueprint behavior.
- Keep generated files out of Git. Use LFS for Unreal packages. Coordinate shared binary edits and verify references, redirectors, and related external actor or object packages in the Editor.
- Build relevant targets and verify affected gameplay and assets. Record checks run and any limitations. Follow the Git Flow branch and release conventions in the README.
- Keep the project usable from Visual Studio. Regenerate and verify both game solution formats and both AutomationTool solution formats after changes to native files, modules, targets or build configuration.
