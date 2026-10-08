# OPSUCHT Inventarwert – Schritt 1

Diese Version ist absichtlich klein.

## Was ist enthalten?

- native C++ Android-Mod-Basis für LeviLauncher/LeviLaunchroid
- OPSUCHT-Rahmen als fester HUD-Rahmen
- Rahmen wird dauerhaft angezeigt, solange das Modul aktiviert ist
- Rahmen kann über das Mod-Menü deaktiviert werden
- noch KEINE Inventarauslesung
- noch KEINE Supabase-Abfrage
- noch KEINE Preisberechnung
- noch KEINE dynamische Tabelle

## Nächster Schritt

Wenn der Rahmen im Spiel korrekt erscheint, kommt die dynamische Tabelle darüber:

1. Itemname
2. Stückzahl
3. Durchschnittspreis
4. Gesamtwert

Unbekannte Artikel werden später als `Noch nicht erfasst` angezeigt.

## Zielplattform

Android / arm64-v8a / LeviLauncher Native Mod.

Minecraft 26.x ist im Manifest freigeschaltet. Die tatsächliche Kompatibilität hängt von der verwendeten Preloader-/Minecraft-Version ab.

## Build

Der GitHub-Workflow baut mit Android NDK 28.2 und CMake. Der Preloader wird automatisch über CMake geladen.
