# Supabase-Verbindungstest (Plan A)

## Zweck
Diese Testversion prüft nur, ob die laufende Mod eine Verbindung zur Supabase Edge Function herstellen und einen Eintrag in einer separaten Testtabelle aktualisieren kann. Sie sendet **keine Artikelnamen, Artikelpreise oder Verkäufe**.

Alle 60 Sekunden wird `mod-connection-test` aufgerufen. Gespeichert werden nur:
- Testkennung `android-mod-plan-a` (eine feste Zeile, kein wachsendes Log)
- Zeitpunkt des letzten Kontakts
- Mod-Testversion
- Anzahl der von der vorhandenen OPSUCHT-API erkannten Auktionen

## Was wurde vorbereitet
- Supabase-Tabelle `public.mod_connection_test` mit aktivierter RLS und ohne Lese-/Schreibrechte für `anon` oder `authenticated`.
- Edge Function `mod-connection-test`, die per gültigem Supabase-Client-JWT aufgerufen wird und serverseitig mit `service_role` nur die feste Testzeile aktualisiert.
- Die Mod verwendet nur den öffentlichen `anon`-Client-Key. Der geheime `service_role`-Schlüssel ist **nicht** in der Mod enthalten.

## Bauen und testen
1. ZIP in ein eigenes GitHub-Repository hochladen bzw. die Dateien in das bestehende Mod-Repository übernehmen.
2. GitHub Actions manuell starten und auf einen erfolgreichen Build warten.
3. Die erzeugte Mod auf dem Handy installieren/aktivieren.
4. Minecraft starten und mindestens 2 Minuten normal spielen, während eine Internetverbindung besteht.
5. Danach in Supabase SQL Editor ausführen:

```sql
select test_id, last_seen, app_version, last_api_auction_count, updated_at
from public.mod_connection_test
where test_id = 'android-mod-plan-a';
```

## Erfolgskriterium
`last_seen` sollte ungefähr alle 60 Sekunden aktualisiert werden. `last_api_auction_count` sollte eine Zahl enthalten. Die Mod schreibt zusätzlich eine Erfolgsmeldung ins Mod-Log.

## Grenzen
- Dieser Test beweist nur die Verbindung und den Schreibweg. Er testet noch keine Artikelzuordnung und speichert keine echten Verkaufspreise.
- Die alte Preis-/HUD-Logik wurde nicht absichtlich umgebaut.
- Die ZIP enthält Quellcode; sie enthält kein fertig kompiliertes Android-Modul.
- Falls der Build fehlschlägt oder kein Testeintrag entsteht, nicht die bestehenden Supabase-Tabellen leeren. Erst den Fehlerbericht prüfen.
