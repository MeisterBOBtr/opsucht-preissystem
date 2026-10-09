# Leistungsverbesserung und Korrektur für benutzerdefinierte Items

## Was geändert wurde

Der vorherige Build durchsuchte alle 36 Slots alle paar Ticks erneut und führte wiederholt die vollständige API-Zuordnungslogik aus. Auf Android kann dies zu sichtbaren Rucklern führen.

Dieser Build durchsucht das Inventar ungefähr einmal pro Sekunde und speichert aufgelöste Itempreise im Zwischenspeicher. Der Zwischenspeicher wird ungültig, sobald sich die API-Preistabelle ändert.

Das zweite Problem war schwerwiegender: Ein Geyser-Custom-Item konnte bei der benutzerdefinierten Zuordnung scheitern und anschließend auf das Java-/Vanilla-Material zurückfallen. Eine benutzerdefinierte Netherite-Spitzhacke konnte dadurch den Preis einer normalen Netherite-Spitzhacke anzeigen.

Die neue Zuordnung verhindert diesen Rückfall ausdrücklich für `Geyser Custom:`-Stacks. Ein nicht zugeordnetes benutzerdefiniertes Item wird ohne Preis angezeigt, statt einen falschen Vanilla-Preis zu erhalten.

## Verbleibender technischer Punkt

Der korrekte OPSUCHT-Preis erfordert weiterhin eine zuverlässige benutzerdefinierte Bedrock-/Geyser-Kennung. Geysers System für benutzerdefinierte Items verwendet eine eindeutige Bedrock-Kennung, aber BedrockTools bietet keinen universellen öffentlichen Wrapper für diese serverseitige Zuordnung. Dieser Build behebt deshalb zuerst den unsicheren Rückfall und die Leistungsprobleme, statt eine ungeprüfte Zuordnung als korrekt auszugeben.
