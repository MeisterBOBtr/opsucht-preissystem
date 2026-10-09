
## Identifier Bridge 1.1.0

This build adds a second API-side identifier bridge: useful string values inside the OPSUCHT `item` object are indexed as lookup aliases, while the client-side resolver also tests separator-normalized and namespace-tail variants. This is intentionally an additional bridge, not a claim that the Bedrock runtime identifier is already exposed by a public BedrockTools wrapper. The project still needs a device test before any runtime mapping can be declared proven.
# Architektur V2

## 1. Client
Minecraft Bedrock Android + BedrockTools + Preloader.

## 2. API-Worker
Ein unabhängiger Hintergrundthread lädt die OPSUCHT-Daten. Das HUD und die Inventarlogik warten nicht auf HTTP.

## 3. API-Katalog
Die aktiven Auktionen werden in mehreren Maps abgelegt:
- normalisierter DisplayName
- API-Identifier / Item-ID
- Material
- weitere technische Aliase

## 4. Inventar
Der vorhandene BedrockTools/Native-Inventarpfad liest den Player-Container. Mengen werden unabhängig von der Darstellung gesammelt.

## 5. Resolver
Resolver-Reihenfolge:
1. exakter normalisierter API-Name
2. exakter API-Identifier/Alias
3. Material
4. konservativer Text-Fallback

Wichtig: `Geyser Custom:` ist nur noch ein Fallback-Signal, keine Primär-ID.

## 6. Anzeige
Die Anzeige ist absichtlich von der Inventarberechnung getrennt. 36 Slots können intern verarbeitet werden; die HUD-Liste zeigt nur einen begrenzten sichtbaren Ausschnitt.
