# OPVANTIS – Opsucht Inventarwert-Mod

Projektbasis für Bedrock Tools v26.52 / Android ARM64. Das vorhandene Rahmenbild unter `resources/frame.png` bleibt erhalten.

## Zielverhalten
- Panel soll auch bei geöffnetem Minecraft-Inventar sichtbar bleiben und über der Inventaroberfläche liegen.
- Inventar-Items und Stückzahlen lesen.
- Items anhand ihrer technischen Merkmale eindeutig mit `artikel` in Supabase abgleichen.
- Pro Artikel die bis zu fünf neuesten Werte aus `preise` verwenden und deren Durchschnitt berechnen.
- Stückwert, Wert je Stapel und Gesamtwert anzeigen.
- Keine Spielitems verändern; Panel soll manuell deaktivierbar sein.

## Wichtiger Stand
Diese ZIP benennt das Projekt als OPVANTIS um und enthält den bisherigen C++-Stand samt Rahmen und Build-Workflow. **Die aktuelle `main.cpp` bezieht Preise weiterhin aus der OPSUCHT-Auktions-API; die Supabase-Tabellen `artikel` und `preise` sind noch nicht als Datenquelle angebunden.** Außerdem ist die dauerhafte Darstellung über dem geöffneten Inventar auf dem Zielgerät noch nicht bestätigt. Ein erfolgreicher GitHub-Build allein beweist diese Ingame-Funktionen nicht.

## Build
1. ZIP entpacken bzw. Inhalt ins GitHub-Repository hochladen.
2. GitHub → Actions → Build-Workflow → Run workflow.
3. Das Artefakt `opvantis-levipack` herunterladen und auf dem Handy testen.

Bitte die ZIP als Projektquelle verwenden, nicht als bereits fertig getestetes Release.
