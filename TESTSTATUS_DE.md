# Teststand – Performance-Test V1.4.0

## Änderungen in dieser Version
- Aktive OPSUCHT-Auktionen werden nur alle 30 Sekunden neu geladen (vorher 15 Sekunden).
- Kategorien werden nur alle 5 Minuten neu geladen (vorher 60 Sekunden).
- Das Inventar wird nur alle 2 Sekunden neu gelesen (vorher jede Sekunde).
- Der Preis-Cache bleibt 4 Sekunden gültig.

Diese Änderungen sollen die kurzen Hänger verringern. Ob sie sie vollständig beheben, muss im Spiel getestet werden.

## Wichtige Einschränkung
Diese Version ist ein Performance-Test und **noch nicht** die vollständige automatische Verkaufs-Synchronisierung nach Supabase. Die aktuelle C++-Quelle verwendet aktive Auktionspreise und zeichnet noch keine bestätigten `instant_bought`-/`sold`-Streamereignisse in `verarbeitete_verkaeufe` auf. Die vorhandene Edge Function `verkauf-verarbeiten` verlangt außerdem einen gültigen JWT. Es wurde absichtlich kein Service-Role-Schlüssel in die Mod eingebaut.

## Supabase-Tabellen vor dem Test
- `artikel`: 62 Zeilen
- `preise`: 74 Zeilen
- `verarbeitete_verkaeufe`: 74 Zeilen
- `mod_connection_test`: 1 Zeile

Die Tabelle `mod_connection_test` enthält den bisherigen Datensatz `android-mod-plan-a` mit 1.079 Auktionen, zuletzt aktualisiert am 9. Oktober 2026 um 23:02:20 UTC.
