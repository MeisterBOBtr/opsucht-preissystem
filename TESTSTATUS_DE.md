# OPVANTIS – Ingame-Test 1 (UI-Fokus)

Diese ZIP ist ein Zwischenschritt zum Testen in Minecraft Bedrock Tools v26.52.

## In dieser Runde geändert
- Projekt-/Buildname auf OPVANTIS umgestellt.
- Bestehende Panel-Zeichnung und `resources/frame.png` bleiben im Projekt enthalten.
- Die Heuristik, die den Inventarstatus nach vier Frames ohne erkannte Inventar-Textlabels zurücksetzte, wurde entschärft. Bedrock kann beim Öffnen des Inventars den UI-Text-Pass wechseln; diese Zeitüberschreitung konnte den Panelzustand zu früh zurücksetzen.

## Noch nicht erledigt
- `resources/frame.png` wird vom aktuellen Renderer noch nicht als Bild gezeichnet; die Panelgrafik ist deshalb noch nicht pixelgenau wie die gespeicherte Vorlage.
- Die direkte Opsucht-API-Abfrage ist in diesem UI-Test noch vorhanden und muss in einer späteren Runde durch reine Supabase-Lesezugriffe ersetzt werden.
- Die Supabase-Preiszuordnung und Itemnamen sind noch nicht korrigiert.
- Ein erfolgreicher Build/Ingame-Test wird nicht behauptet. Bitte den Build ausführen und im Spiel prüfen, ob das Panel beim Öffnen des Inventars sichtbar bleibt.

## Testschritte
1. GitHub Actions mit `.github/workflows/build.yml` starten.
2. Das erzeugte LeviPack auf Bedrock Tools v26.52 installieren.
3. Im Spiel prüfen: Panel im normalen HUD sichtbar? Inventar öffnen: bleibt es sichtbar? Inventar schließen: bleibt das Spiel flüssig?
4. Fehler oder Screenshot zurückschicken.
