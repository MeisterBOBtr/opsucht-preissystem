# OPSUCHT API Hintergrund

Die API ist im nativen Mod fest hinterlegt.

ACTIVE:
`https://api.opsucht.net/auctions/active`

CATEGORIES:
`https://api.opsucht.net/auctions/categories`

STREAM:
`https://api.opsucht.net/auctions/stream`

Der Stream wird in V2 noch nicht als dauerhafte JNI-SSE-Verbindung geöffnet. Stattdessen wird `active` alle 15 Sekunden abgefragt. Das verhindert eine blockierende Verbindung im Android-Network-Thread und ist für die Preisberechnung ausreichend stabil.

Wenn die OPSUCHT-API nicht erreichbar ist, löscht V2 die zuletzt erfolgreiche Preistabelle nicht.
