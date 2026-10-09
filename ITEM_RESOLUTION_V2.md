# Artikelzuordnung V2

## Kennungsbrücke 1.1.0

Diese Version ergänzt eine zweite Kennungsbrücke auf API-Seite: Verwendbare Zeichenketten im OPSUCHT-Objekt `item` werden als Such-Aliase indexiert. Gleichzeitig prüft die clientseitige Zuordnung Varianten mit vereinheitlichten Trennzeichen und Varianten ohne den Namespace-Anteil. Dies ist eine zusätzliche Zuordnungshilfe und kein Beweis dafür, dass die Bedrock-Laufzeitkennung bereits über eine öffentliche BedrockTools-Schnittstelle verfügbar ist. Eine Prüfung auf dem Gerät ist weiterhin erforderlich, bevor die Laufzeitzuordnung als bestätigt gelten kann.

Das bisherige Problem war die Vermischung von Bedrock-Anzeige-/Geyser-Namen mit der OPSUCHT-Identität.

Geyser dokumentiert für benutzerdefinierte Items eine eindeutige `bedrock_identifier`. Die V2-Architektur behandelt `Geyser Custom:...` deshalb nicht als sicheren Namensnachweis.

Zielkette:

Bedrock-ItemStack
→ echte Item-/Registry-Information
→ OPSUCHT-Item-ID / Kennung
→ OPSUCHT-Anzeigename
→ Durchschnittspreis

Falls die Bedrock-Seite diese Information über den aktuell verfügbaren SDK-/Binärpfad nicht bereitstellt, bleibt dies der technische Engpass. Es sollen dann keine zufälligen, unscharfen Namensvergleiche verwendet werden.
