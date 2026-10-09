# V7 – Korrektur der Preiszuordnung über Custom-IDs

Dieser Build behält die Listenlänge und Anzeigeänderungen aus V6 bei.

## Wichtigste Korrektur
Das Android-/Geyser-Inventar kann ein benutzerdefiniertes Item beispielsweise so bereitstellen:
`geyser_custom_main_misc_may26_golden_excalibur`

Die vorherige Zuordnungslogik versuchte, das Präfix `geyser custom` zu entfernen, bevor Unterstriche in Trennzeichen umgewandelt wurden. Dadurch konnte das tatsächliche Format `geyser_custom_...` das Präfix behalten und eine Übereinstimmung mit einer OPSUCHT-/API-Kennung wie `main_misc_may26_golden_excalibur` verfehlen.

V7 vereinheitlicht zuerst die Trennzeichen und entfernt anschließend das Geyser-Präfix. Dadurch wird die technische Kennung ohne Präfix als exakter API-Alias geprüft, ohne auf den Preis des Vanilla-Materials zurückzufallen.

## Wichtig
Diese Änderung verändert weder die Anzeige des End Shield noch das HUD mit 18 Zeilen. Sie verbessert ausschließlich die Preiszuordnung für benutzerdefinierte Items.
