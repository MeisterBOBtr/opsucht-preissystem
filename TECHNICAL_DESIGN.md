# Technischer Aufbau

## Geplante Datenverarbeitung

```text
Minecraft Bedrock auf Android
            |
            v
   Mod-Loader / native Mod
            |
            v
   Inventar und Items erkennen
            |
            v
   Eindeutige Item-Zuordnung
            |
            v
   Offizielle OPSUCHT-Datenquelle
            |
            v
   Preis ermitteln und zwischenspeichern
            |
            v
   Itemzeilen und Gesamtwert anzeigen
```

Das Diagramm beschreibt das Zielbild, nicht eine bereits vollständig funktionierende Implementierung.

## Komponenten

### Native Android-Mod

Die native Komponente soll den HUD-Rahmen darstellen, das Inventar über eine verfügbare Mod-Loader-Schnittstelle auslesen und die Ergebnisse im Panel anzeigen.

Die konkreten APIs für Inventarzugriff und HUD-Zeichnung müssen zur tatsächlich eingesetzten Version des Mod-Loaders passen.

### Item-Erkennung

Für eine verlässliche Zuordnung können mehrere Merkmale nötig sein, zum Beispiel:

- Minecraft-Material bzw. interne Item-ID
- Anzeigename
- Lore oder zusätzliche Item-Metadaten
- Verzauberungen oder Varianten, soweit relevant

Stapelgröße, aktueller Besitzer, Auktions-ID und Preis sind normalerweise keine stabilen Identitätsmerkmale eines Itemtyps.

### Preisquelle

Bevorzugt soll eine von OPSUCHT offiziell bereitgestellte und erlaubte API verwendet werden. Vor der Umsetzung müssen Endpoints, Antwortformat, Limits, Authentifizierung und erlaubte Nutzung geklärt werden.

Falls eine direkte Abfrage aus dem Add-on nicht vorgesehen ist, muss geprüft werden, ob ein kleiner vermittelnder Dienst notwendig ist. Ein solcher Dienst sollte nur eingeführt werden, wenn es einen konkreten technischen oder sicherheitsbezogenen Grund gibt.

### Preisberechnung

Geplant ist ein Durchschnitt der fünf jüngsten tatsächlichen Verkäufe je erkanntem Item, sofern fünf gültige Verkäufe vorliegen. Die genaue Darstellung bei weniger Daten muss noch festgelegt werden.

Aktive Auktionen, abgelaufene Auktionen und abgebrochene Auktionen dürfen nicht automatisch als abgeschlossene Verkäufe gewertet werden.

### Benutzerdefinierte Items und Stapelpreise

Bei benutzerdefinierten OPSUCHT-Items muss geprüft werden, ob der gemeldete Verkaufspreis den gesamten Stapel oder eine einzelne Einheit bezeichnet. Der Wert darf nicht pauschal durch die Stapelgröße geteilt werden, wenn die API den Gesamtpreis des Stapels liefert.

### Anzeige

Geplante Felder pro Zeile:

- Itemname
- Stückzahl
- Preisangabe
- Status bei fehlendem Preis

Am unteren Rand soll der Gesamtwert aus den verfügbaren, korrekt zugeordneten Preisen berechnet werden. Fehlende Preise dürfen nicht stillschweigend als echte Nullpreise dargestellt werden.

## Effizienz und Fehlerbehandlung

- Wiederholte identische Preisabfragen nach Möglichkeit vermeiden.
- Zwischenspeicherung nur gemäß den Bedingungen der Datenquelle einsetzen.
- API-Fehler und fehlende Netzwerkverbindung sichtbar und kontrolliert behandeln.
- Keine geheimen API-Schlüssel in den nativen Code oder in ein öffentliches Repository einbauen.
- Bei veralteten Preisen kenntlich machen, dass die Daten möglicherweise nicht aktuell sind.

## Offene Fragen

- Welche Inventar- und HUD-Schnittstellen bietet die verwendete Mod-Loader-Version?
- Gibt es eine offizielle API für abgeschlossene OPSUCHT-Verkäufe?
- Sind direkte Anfragen aus dem Add-on gestattet?
- Welche Rate Limits und Cache-Regeln gelten?
- Welche Minecraft- und Mod-Loader-Versionen sollen unterstützt werden?
