# Entwicklungsstatus

**Stand:** 2026-10-09  
**Projektphase:** Frühe technische Entwicklung

Diese Datei soll ehrlich zeigen, was vorbereitet ist und was noch nicht als funktionsfähig bestätigt wurde.

## Vorhanden bzw. begonnen

- Öffentliches GitHub-Repository
- CMake-Projektgrundlage
- C++-Einstiegspunkt unter `src/`
- Ressourcenordner für das Rahmenbild
- Paketierungsskript vorgesehen
- GitHub-Actions-Workflow für Android-Builds
- Dokumentation der geplanten HUD-Anzeige

## Noch offen

- [ ] Nachweislich erfolgreicher Build des aktuellen Repository-Stands
- [ ] Erfolgreicher Test des Rahmens im Spiel auf dem Zielgerät
- [ ] Zuverlässige Erkennung des geöffneten Inventars
- [ ] Auslesen der Itemnamen und Stückzahlen
- [ ] Anbindung einer offiziell erlaubten OPSUCHT-Datenquelle
- [ ] Zuordnung von OPSUCHT-Items zu Minecraft-Inventar-Items
- [ ] Berechnung und Anzeige der Preise
- [ ] Berechnung des Gesamtinventarwerts
- [ ] Fehlerbehandlung und Leistungstests
- [ ] Release-Paket und Installationsanleitung

## Bekannte technische Risiken

1. **Minecraft-/Mod-Loader-Kompatibilität:** Änderungen an Minecraft oder am Loader können Schnittstellen verändern.
2. **Inventarzugriff:** Die verfügbaren Schnittstellen müssen das benötigte Inventar tatsächlich zugänglich machen.
3. **Item-Erkennung:** Anzeigenamen allein sind nicht immer eindeutig; benutzerdefinierte Items können zusätzliche Merkmale benötigen.
4. **API-Verfügbarkeit:** Eine offizielle Schnittstelle für tatsächliche abgeschlossene Verkäufe ist noch nicht bestätigt.
5. **Netzwerk und Limits:** Anfragen müssen effizient, sparsam und gemäß den erlaubten Nutzungsbedingungen umgesetzt werden.
6. **Preisgenauigkeit:** Wenige Verkäufe oder ungewöhnliche Stapelpreise können den Durchschnitt beeinflussen.

## Was als Nächstes geprüft wird

1. Den letzten GitHub-Actions-Build kontrollieren.
2. Den Rahmen im Spiel testen.
3. Die Antwort von OPSUCHT zur API und zu möglichen Nutzungsbedingungen abwarten.
4. Danach die technische Umsetzung der Inventarauslesung und Preisabfrage festlegen.

## Statusbegriffe

- **Vorbereitet:** Dateien oder Grundstruktur sind vorhanden.
- **Implementiert:** Code für die Funktion wurde geschrieben.
- **Getestet:** Funktion wurde auf der vorgesehenen Umgebung geprüft.
- **Fertig:** Funktion arbeitet zuverlässig und die relevanten Einschränkungen sind dokumentiert.

Diese Begriffe sollen nicht verwechselt werden.
