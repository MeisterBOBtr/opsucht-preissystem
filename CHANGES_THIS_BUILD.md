# Änderungen in diesem Build – Preiszuordnung V4

- Verwendet die ermittelte technische Geyser-Custom-Kennung als primären Schlüssel für benutzerdefinierte Items.
- Ergänzt eine stabile Vereinheitlichung technischer Schlüssel für API-Aliase und clientseitige ItemStack-Kandidaten.
- Entfernt unscharfe Zuordnungen benutzerdefinierter Items.
- Verhindert, dass benutzerdefinierte Items Preise des entsprechenden Vanilla-Materials übernehmen.
- Zeigt bei einer exakten Kennungs-/Alias-Übereinstimmung den Anzeigenamen aus der OPSUCHT-API.
- Verwendet für nicht aufgelöste benutzerdefinierte Kennungen einen kurzen Ersatznamen.
- Entfernt den Diagnose-Inspektor aus dem normalen HUD.
- Behält die Leistungsverbesserungen mit einer Inventaraktualisierung pro Sekunde bei.

## Aktualisierung von Preisen und Liste
- Die Preiszuordnung für Geyser-Custom-Items ermittelt nun zusätzlich den lesbaren technischen Namensrest (z. B. `..._golden_excalibur`) und vergleicht ihn exakt mit den Anzeigenamen-/Aliasdaten von OPSUCHT.
- Benutzerdefinierte Items greifen niemals auf den Preis ihres Vanilla-Materials zurück.
- Die HUD-Liste wurde auf 18 sichtbare Zeilen erweitert. Die kompakte Zeilenhöhe von 23 px lässt den Fußbereich frei.
