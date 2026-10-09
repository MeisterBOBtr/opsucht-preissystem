# Build-Prüfung

Der GitHub-Actions-Build verwendet die offizielle Android-Vorlage von LeviLauncher. Die oberste `CMakeLists.txt` des Projekts ist deshalb nicht die CMake-Datei, mit der GitHub Actions den Build tatsächlich kompiliert.

Der vorherige Workflow kopierte BedrockTools außerhalb des Vorlagenziels und versuchte anschließend, einen globalen Include-Pfad über `CMAKE_CXX_FLAGS` einzufügen. Der Compiler meldete weiterhin:

`fatal error: 'bedrocktools/BedrockTools.hpp' file not found`

Dieser Workflow kopiert nun den vollständigen öffentlichen Header-Ordner `bedrocktools/include/bedrocktools` direkt nach `template/src/bedrocktools`. Die offizielle Vorlage fügt `template/src` bereits zu den Include-Verzeichnissen des Mod-Ziels hinzu. Dadurch wird `#include <bedrocktools/BedrockTools.hpp>` über denselben Include-Pfad wie der übrige Mod-Quellcode aufgelöst.

Eine Prüfung mit `test -f` stellt sicher, dass der Workflow sofort abbricht, falls der benötigte Header nicht kopiert wurde.
