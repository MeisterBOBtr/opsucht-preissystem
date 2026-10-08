# API, Datenzugriff und Erlaubnis

## Ziel

Das Add-on soll Preisangaben auf Grundlage einer offiziellen und erlaubten OPSUCHT-Datenquelle anzeigen.

## Aktueller Klärungsstand

Eine offizielle Schnittstelle für abgeschlossene Auktionsverkäufe ist noch nicht bestätigt. OPSUCHT wurde um Auskunft gebeten. Bis eine Antwort vorliegt, ist die Verwendung einer Verkaufshistorie als offene Frage zu behandeln.

## Benötigte Daten

Wenn OPSUCHT diese Daten offiziell zur Verfügung stellen kann, wären folgende Felder hilfreich:

- Item bzw. eindeutige Itemmerkmale
- verkaufte Menge
- tatsächlich erzielter Verkaufspreis
- Zeitpunkt des Verkaufs
- eindeutige Verkaufs- oder Auktionskennung, sofern bereitgestellt

Es werden nur die Daten benötigt, die zur Berechnung einer ungefähren Preisübersicht erforderlich sind.

## Abgrenzung

Das Projekt möchte:

- keine internen Datenbanken oder nicht öffentlichen Systeme auslesen;
- keine Authentifizierung oder Zugriffsbeschränkungen umgehen;
- keine aktiven Angebote fälschlich als Verkäufe zählen;
- keine Gebote oder Käufe automatisieren;
- die API nur im erlaubten Umfang verwenden.

## Fragen an OPSUCHT

1. Gibt es einen offiziellen Endpoint für tatsächlich abgeschlossene Verkäufe?
2. Welche Felder und Zeiträume sind verfügbar?
3. Ist die Nutzung für ein kostenloses Community-Addon erlaubt?
4. Welche Rate Limits, Cache-Regeln oder sonstigen Vorgaben gelten?
5. Gibt es Vorgaben zur Namensnennung, zum Logo oder zur Veröffentlichung?
6. Muss das Add-on vor der öffentlichen Veröffentlichung geprüft oder freigegeben werden?

## Umgang mit Zugangsdaten

Geheime Schlüssel, Passwörter, Tokens oder private Zugangsdaten dürfen nicht in öffentlichen Dateien, Screenshots oder im Repository veröffentlicht werden. Falls eine Authentifizierung erforderlich ist, muss eine geeignete sichere Lösung gewählt werden.

## Transparenz

Preiswerte sind Schätzwerte auf Basis der verfügbaren Verkaufsdaten und keine Garantie dafür, dass ein Item aktuell genau zu diesem Preis verkauft werden kann.
