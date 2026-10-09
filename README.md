# OPSUCHT Inventarwert – Supabase-Verbindungstest V1.4.0

Diese Testversion basiert auf `OPSUCHT_Inventarwert_V7_CUSTOM_ID_PRICE_FIX`. Sie behält die bisherige OPSUCHT-API-Abfrage bei und ergänzt einen isolierten Verbindungstest zu Supabase.

**Wichtig:** Der Test speichert keine Artikel oder Preise. Er aktualisiert alle 60 Sekunden nur eine feste Testzeile mit Zeitstempel und erkannter Auktionsanzahl. So lässt sich prüfen, ob die Mod während des Spielens schreiben kann, ohne vorhandene Daten zu verändern.

Siehe [SUPABASE_TEST_DE.md](SUPABASE_TEST_DE.md) für den Ablauf.
