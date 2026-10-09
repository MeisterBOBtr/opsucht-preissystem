# Architektur V2

## Kennungsbrücke 1.1.0

Diese Version ergänzt eine zweite Kennungsbrücke auf API-Seite: Verwendbare Zeichenketten im OPSUCHT-Objekt `item` werden als Such-Aliase indexiert. Gleichzeitig prüft die clientseitige Zuordnung Varianten mit vereinheitlichten Trennzeichen und Varianten ohne den Namespace-Anteil. Dies ist eine zusätzliche Zuordnungshilfe und kein Beweis dafür, dass die Bedrock-Laufzeitkennung bereits über eine öffentliche BedrockTools-Schnittstelle verfügbar ist. Eine Prüfung auf dem Gerät ist weiterhin erforderlich, bevor die Laufzeitzuordnung als bestätigt gelten kann.

## 1. Client
Minecraft Bedrock auf Android + BedrockTools + Preloader.

## 2. API-Arbeiter
Ein unabhängiger Hintergrundthread lädt die OPSUCHT-Daten. HUD und Inventarlogik müssen nicht auf HTTP-Anfragen warten.

## 3. API-Katalog
Die aktiven Auktionen werden in mehreren Zuordnungstabellen abgelegt:
- vereinheitlichter Anzeigename
- API-Kennung / Item-ID
- Material
- weitere technische Aliase

## 4. Inventar
Der vorhandene BedrockTools-/Native-Inventarpfad liest den Spielercontainer. Mengen werden unabhängig von der Darstellung erfasst.

## 5. Zuordnung
Reihenfolge der Zuordnung:
1. exakt übereinstimmender, vereinheitlichter API-Name
2. exakt übereinstimmende API-Kennung / Alias
3. Material
4. vorsichtiger Text-Fallback

Wichtig: `Geyser Custom:` ist nur noch ein Fallback-Hinweis und keine primäre ID.

## 6. Anzeige
Die Anzeige ist absichtlich von der Inventarberechnung getrennt. Intern können 36 Slots verarbeitet werden; die HUD-Liste zeigt nur einen begrenzten sichtbaren Ausschnitt.
