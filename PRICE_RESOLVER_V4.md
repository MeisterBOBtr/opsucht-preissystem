# Preiszuordnung V4

Dieser Build verwendet die ermittelte Geyser-Custom-Kennung als primäre Identität für benutzerdefinierte OPSUCHT-Items.

- Benutzerdefinierte Kennungen werden in einen stabilen technischen Schlüssel umgewandelt.
- API-seitige Item-Kennungen und alle nützlichen Item-Zeichenketten werden unter derselben technischen Schlüsselform indexiert.
- Geyser-Custom-Items greifen niemals auf den Preis ihres Vanilla-Materials zurück.
- Wenn keine exakte Übereinstimmung mit der benutzerdefinierten Kennung gefunden wird, zeigt das HUD `-` statt eines falschen Preises.
- Bei einer exakten Übereinstimmung zeigt das HUD den Anzeigenamen aus der OPSUCHT-API.
- Ist kein API-Anzeigename verfügbar, wird aus der technischen Kennung ein kurzer, lesbarer Ersatzname erzeugt.
- Der bisherige Inspektorblock wird aus dem sichtbaren HUD entfernt.

Vanilla-Items behalten die bestehende exakte Zuordnung über Namen und Aliase. Die Zuordnung behauptet nicht, verzauberte Varianten korrekt bepreisen zu können, solange die API nicht genügend Variantenmerkmale für einen exakten Abgleich bereitstellt.
