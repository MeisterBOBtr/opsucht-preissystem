# Projektziele

## Warum dieses Projekt entsteht

Das Ziel ist, eine kostenlose Preisübersicht für Minecraft-Bedrock-Spieler auf Android zu entwickeln. Besonders Handyspieler sollen Items aus ihrem Inventar leichter preislich einschätzen können.

Die Idee ist eine kleine, übersichtliche Anzeige im Spiel, die sich am OPSUCHT-Auktionshaus orientiert.

## Hauptziele

1. Eine feste, gut lesbare Anzeige im HUD bereitstellen.
2. Das Inventar auslesen, sofern die verwendete Modding-Schnittstelle dies zuverlässig und regelkonform ermöglicht.
3. Inventar-Items möglichst eindeutig erkennen.
4. Preisangaben aus einer offiziellen und erlaubten Datenquelle beziehen.
5. Den Gesamtwert des Inventars berechnen.
6. Unbekannte Items deutlich kennzeichnen, statt einen Preis zu erfinden.
7. Die Anwendung möglichst kostenlos und einfach installierbar halten.

## Zielgruppe

Minecraft-Bedrock-Spieler auf Android, insbesondere Spieler, die eine praktische Preisübersicht im Spiel nutzen möchten.

## Geplante Nutzererfahrung

Wenn das Inventar geöffnet ist, soll eine Übersicht erscheinen. Sie soll Itemname, Stückzahl und Preis zeigen und am unteren Rand den berechneten Gesamtwert ausgeben.

Die konkrete Erkennung des geöffneten Inventars und die dauerhafte Darstellung müssen technisch getestet werden. Solange das nicht zuverlässig funktioniert, wird die Funktion nicht als fertig bezeichnet.

## Preisgrundsätze

- Bevorzugt werden tatsächlich abgeschlossene Verkäufe.
- Aktive Auktionen allein sollen nicht als tatsächliche Verkäufe behandelt werden.
- Der geplante Durchschnitt soll auf den fünf jüngsten tatsächlichen Verkäufen je eindeutig erkanntem Item beruhen, sofern genügend Daten vorhanden sind.
- Neue Verkaufsdaten sollen ältere Daten nach dem festgelegten Verfahren ersetzen.
- Wenn keine ausreichenden Preisdaten vorliegen, soll `Noch nicht erfasst` oder eine andere eindeutige Statusmeldung erscheinen.
- Für benutzerdefinierte Items muss geklärt sein, ob ein Auktionspreis den ganzen Stapel oder eine einzelne Einheit bezeichnet.

## Nicht-Ziele

- Keine Items verändern oder manipulieren.
- Keine Gebote abgeben und keine Auktionen automatisch kaufen.
- Keine Spielaktionen automatisieren.
- Keine nicht öffentlichen OPSUCHT-Daten ohne ausdrückliche Erlaubnis abrufen.
- Keine falsche Genauigkeit bei Schätzpreisen vortäuschen.

## Erfolgskriterien

Das Projekt gilt erst dann als einsatzbereit, wenn die Anzeige stabil dargestellt wird, die Item-Erkennung ausreichend zuverlässig ist, Preise korrekt zugeordnet werden, der Gesamtwert stimmt und die Nutzung der Datenquelle geklärt ist.
