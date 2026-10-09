# Entwicklungsplan

Die Reihenfolge kann sich ändern, wenn technische Tests oder die Rückmeldung von OPSUCHT neue Anforderungen ergeben.

## Schritt 1 – Projektgrundlage und HUD-Rahmen

- [x] GitHub-Repository angelegt
- [x] CMake-/Android-NDK-Buildpipeline begonnen
- [x] Ressourcenordner und Rahmenbild vorgesehen
- [ ] Build auf der tatsächlichen Zielumgebung erfolgreich abschließen
- [ ] Rahmen im Spiel korrekt positionieren und skalieren
- [ ] Aktivieren und Deaktivieren des Panels testen

## Schritt 2 – Inventar erkennen und auslesen

- [ ] Prüfen, welche offizielle Mod-Loader-Schnittstelle dafür verfügbar ist
- [ ] Erkennen, wann das Inventar geöffnet ist
- [ ] Itemname und Anzahl auslesen
- [ ] Mit verschiedenen Inventarplätzen, Stapelgrößen und Items testen
- [ ] Verhalten bei geschlossenem Inventar und bei Spielmenüs testen

## Schritt 3 – OPSUCHT-Datenquelle klären

- [ ] Rückmeldung von OPSUCHT zur API abwarten
- [ ] Erlaubte Endpoints und Nutzungsbedingungen dokumentieren
- [ ] Verfügbarkeit tatsächlicher Verkaufsdaten prüfen
- [ ] Datenformat, Rate Limits und Cache-Regeln klären
- [ ] Festlegen, ob direkte API-Aufrufe aus dem Add-on erlaubt und technisch möglich sind

## Schritt 4 – Item-Erkennung und Preislogik

- [ ] Eindeutige Erkennungsschlüssel für Items definieren
- [ ] Benutzerdefinierte Items und Varianten berücksichtigen
- [ ] Menge und Stapelpreis korrekt interpretieren
- [ ] Nur tatsächliche Verkäufe in die Verkaufshistorie aufnehmen
- [ ] Durchschnitt aus den fünf jüngsten tatsächlichen Verkäufen berechnen, wenn ausreichend Daten vorhanden sind
- [ ] Unbekannte Items ohne erfundenen Preis anzeigen

## Schritt 5 – Dynamische Anzeige

- [ ] Zeilen für erkannte Inventar-Items dynamisch zeichnen
- [ ] Name, Stückzahl und Preis anzeigen
- [ ] Lange Namen und viele Items sinnvoll behandeln
- [ ] Gesamtwert korrekt berechnen
- [ ] `Noch nicht erfasst` bei fehlenden Preisdaten anzeigen
- [ ] Lesbarkeit und Skalierung auf unterschiedlichen Handybildschirmen testen

## Schritt 6 – Stabilität und Effizienz

- [ ] API-Anfragen begrenzen und Ergebnisse zwischenspeichern, soweit erlaubt
- [ ] Netzwerkfehler und fehlende Daten sauber behandeln
- [ ] Ladezustände und veraltete Daten kenntlich machen
- [ ] Abstürze, Speicherverbrauch und Performance testen
- [ ] Minecraft-Updates und Mod-Loader-Kompatibilität prüfen

## Schritt 7 – Veröffentlichung

- [ ] Zustimmung und Bedingungen von OPSUCHT dokumentieren
- [ ] Lizenz und Nutzungsbedingungen festlegen
- [ ] Installationsanleitung erstellen
- [ ] Versionierung und Änderungsprotokoll einrichten
- [ ] Release-Paket erstellen und auf einem Testgerät prüfen
- [ ] Download- und Supportkanal einrichten

## Versionsprinzip

Eine Funktion gilt erst dann als erledigt, wenn sie gebaut und auf der Zielumgebung getestet wurde. Ein grüner Build allein beweist nicht, dass die Funktion im Spiel korrekt arbeitet.
