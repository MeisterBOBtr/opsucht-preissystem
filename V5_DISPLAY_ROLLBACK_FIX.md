# V5 – Korrektur der Namensanzeige

Die Änderung der Anzeigenamen im vorherigen V5-Build war zu aggressiv: Auf diesem Client kann `getCustomName()` die technische Geyser-Kennung zurückgeben. Dadurch verlor das HUD die lesbaren Namen aus der API.

Dieser Korrektur-Build stellt die bewährte Preiszuordnung aus V4 wieder her und verwendet `getCustomName()` nur dann, wenn der Wert tatsächlich ein lesbarer Name ist. Technische Werte im Format `geyser_custom_*` werden niemals als HUD-Name angezeigt.

Es werden keine neuen Preise geraten. Benutzerdefinierte Items greifen weiterhin nicht auf Vanilla-Preise zurück.
