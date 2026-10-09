# Build check

The GitHub Actions build uses the official LeviLauncher Android template.
The project's own top-level CMakeLists.txt is therefore not the CMake file
that compiles the GitHub Actions build.

The previous workflow copied BedrockTools outside the template target and
then tried to inject a global `CMAKE_CXX_FLAGS` include path. The compiler
still reported:

`fatal error: 'bedrocktools/BedrockTools.hpp' file not found`

This workflow now copies the complete public
`bedrocktools/include/bedrocktools` header tree directly into
`template/src/bedrocktools`. The official template already adds `template/src`
to the mod target's include directories, so
`#include <bedrocktools/BedrockTools.hpp>` resolves through the same include
path as the rest of the mod source.

A `test -f` check is included so the workflow stops immediately if the
required header was not copied.
