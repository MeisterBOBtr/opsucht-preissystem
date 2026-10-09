# Datenschutz und Sicherheit

## Grundsatz

Das Add-on soll nur die Daten verarbeiten, die für die Item-Erkennung und die Anzeige einer Preisübersicht erforderlich sind.

## Geplante Datenverarbeitung

- Inventar-Itemdaten werden lokal ausgelesen, sofern der Mod-Loader dies unterstützt und erlaubt.
- Für die Preiszuordnung werden nur erforderliche Itemmerkmale an die vorgesehene Datenquelle übermittelt.
- Ob Daten an einen vermittelnden Server gesendet werden müssen, ist noch offen und wird erst nach Klärung der API entschieden.
- Es sollen keine persönlichen Kontodaten oder Zugangsdaten unnötig gesammelt werden.

## Sicherheit

- Keine geheimen Schlüssel in GitHub committen.
- Keine privaten Zugangsdaten in Logs ausgeben.
- Netzwerkfehler sicher behandeln.
- API-Nutzung und Speicherung an die offiziellen Nutzungsbedingungen anpassen.
- Wenn ein Backend erforderlich wird, nur notwendige Daten verarbeiten und Zugriffe absichern.

## Transparenz gegenüber Nutzern

Vor einer Veröffentlichung soll erklärt werden, welche Daten das Add-on verarbeitet, ob Netzwerkverbindungen hergestellt werden und ob Daten gespeichert werden.

Diese Datei beschreibt die geplanten Grundsätze. Die tatsächliche Umsetzung muss vor der Veröffentlichung geprüft und bei Bedarf angepasst werden.
