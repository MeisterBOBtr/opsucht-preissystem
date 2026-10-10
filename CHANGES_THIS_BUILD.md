# OPVANTIS UI-Test 1

Diese Testversion fokussiert den Inventar-Overlay-Zustand. Sie entschärft die bisherige automatische Rücksetzung des Inventarstatus, wenn einige Frames lang keine Inventar-Textlabels erkannt wurden.

Das originale Rahmenbild bleibt als Projektressource erhalten. Die aktuelle DrawCommand-Schnittstelle zeichnet weiterhin Rechtecke und Text, daher ist das Einbinden des PNG-Rahmens als echte Bildfläche noch offen.

Direkte Opsucht-API-Abfragen und die bisherige Preislogik bleiben in diesem Zwischenbuild noch unverändert, damit dieser Test möglichst nur den UI-Zustand betrifft. Die Trennung auf Supabase-Read-only erfolgt nach dem UI-Test.
