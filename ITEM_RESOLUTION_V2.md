
## Identifier Bridge 1.1.0

This build adds a second API-side identifier bridge: useful string values inside the OPSUCHT `item` object are indexed as lookup aliases, while the client-side resolver also tests separator-normalized and namespace-tail variants. This is intentionally an additional bridge, not a claim that the Bedrock runtime identifier is already exposed by a public BedrockTools wrapper. The project still needs a device test before any runtime mapping can be declared proven.
# Item Resolution V2

Das bisherige Problem war die Vermischung von Bedrock-Anzeige-/Geyser-Namen mit der OPSUCHT-Identität.

Geyser dokumentiert für Custom Items eine eindeutige `bedrock_identifier`. Die V2-Architektur behandelt deshalb `Geyser Custom:...` nicht als Beweis für einen Namen.

Zielkette:

Bedrock ItemStack
→ echte Item-/Registry-Information
→ OPSUCHT Item-ID / Identifier
→ OPSUCHT DisplayName
→ Durchschnittspreis

Falls die Bedrock-Seite diese Information nicht über den aktuell verfügbaren SDK-/Binary-Pfad freigibt, ist das der verbleibende technische Engpass. Dann soll nicht weiter mit zufälligen Namens-Fuzzy-Matches gearbeitet werden.
