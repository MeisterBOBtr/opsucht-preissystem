# OPSUCHT Inventarwert für Minecraft Bedrock

Ein kostenlos geplantes Add-on für Minecraft Bedrock auf Android, das Handyspielern eine übersichtliche Preisanzeige für Items aus dem OPSUCHT-Auktionshaus ermöglichen soll.

> **Entwicklungsstatus:** Frühe Entwicklungsphase. Der feste HUD-Rahmen ist vorbereitet. Inventarauslesung, API-Anbindung und Preisberechnung sind noch nicht als vollständig funktionierende Funktionen bestätigt.

## Ziel

Das Projekt soll Handy-/Bedrock-Spielern helfen, den ungefähren Marktwert ihres Inventars besser einzuschätzen. Das Add-on soll Itemnamen, Stückzahlen, Preise und den Gesamtwert übersichtlich anzeigen.

## Geplante Anzeige

- Itemname
- Anzahl der Items
- Preis pro Item bzw. passende Preisangabe für das erkannte Item
- Gesamtwert des Inventars
- Kennzeichnung unbekannter Items mit `Noch nicht erfasst`

Die Preisangaben sollen möglichst auf tatsächlichen Verkäufen beruhen und nicht einfach aus einer einzelnen aktiven Auktion abgeleitet werden.

## Aktueller Stand

- Android-/ARM64-Projektgrundlage für einen nativen Bedrock-Mod-Loader ist angelegt.
- Ein festes HUD-Panel mit einem eigenen Rahmen ist vorbereitet.
- Die GitHub-Actions-Buildpipeline ist eingerichtet und wird weiter getestet.
- Inventarauslesung ist noch nicht fertig.
- Eine bestätigte, zuverlässige Preisquelle ist noch nicht angebunden.
- Dynamische Tabellenzeilen und Gesamtwertberechnung sind noch nicht fertig.

Details: [`STATUS.md`](STATUS.md) und [`ROADMAP.md`](ROADMAP.md).

## Plattform und technische Grundlage

- Zielplattform: Minecraft Bedrock auf Android
- Zielarchitektur: `arm64-v8a`
- Mod-Loader: LeviLauncher / LeviLauchroid – die genaue Kompatibilität muss mit der verwendeten Version geprüft werden
- Sprache: C++
- Build: CMake und Android NDK über GitHub Actions

Die tatsächliche Kompatibilität hängt unter anderem von der Minecraft-Version, dem Mod-Loader und den verfügbaren Schnittstellen ab.

## OPSUCHT-API

Das Projekt soll ausschließlich eine von OPSUCHT offiziell erlaubte und dokumentierte Schnittstelle verwenden. Ob eine offizielle Schnittstelle für abgeschlossene Verkäufe verfügbar ist und unter welchen Bedingungen sie genutzt werden darf, ist noch mit OPSUCHT zu klären.

Siehe [`API_AND_PERMISSIONS.md`](API_AND_PERMISSIONS.md).

## Grundsätze

- Kostenlos für Spieler geplant
- Keine erfundenen Preise: nicht erkannte oder nicht verfügbare Preise sollen klar gekennzeichnet werden
- API-Zugriffe möglichst sparsam und effizient gestalten
- Keine internen oder nicht autorisierten Datenquellen verwenden
- Projektstatus und bekannte Einschränkungen transparent dokumentieren

## Mitwirkung und Rückmeldungen

Fehlerberichte, technische Hinweise und Verbesserungsvorschläge sind willkommen. Bitte keine Zugangsschlüssel, Passwörter oder privaten Tokens in Issues oder im Repository veröffentlichen.

## Hinweis

Dieses Projekt ist ein unabhängiges Vorhaben. Es ist keine offizielle OPSUCHT-Software, sofern OPSUCHT nicht ausdrücklich etwas anderes bestätigt. Namen und Marken bleiben ihren jeweiligen Inhabern zugeordnet.
