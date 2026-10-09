# V3 – Item-Inspektor

Dieser Build ist ein Diagnoseschritt und keine endgültige Preisreparatur.

Der funktionierende Inventar-/API-/HUD-Pfad bleibt erhalten. Für belegte Slots zeichnet die Version Hinweise zum clientseitigen ItemStack auf, die über die native Brücke derzeit zugänglich sind:
- mögliche Zeichenketten, die von den verfügbaren ItemStack-Funktionen zurückgegeben werden
- Zeiger auf das Item und dessen VTable
- die ersten 32 Byte des aktuellen ItemStack als diagnostischer Fingerabdruck

Damit soll Schluss mit Vermutungen sein. OPSUCHT-/Geyser-Items können ein Vanilla-Basisitem verwenden und gleichzeitig eine benutzerdefinierte Identität und/oder Komponenten besitzen. Die nächste Zuordnungslogik sollte erst geschrieben werden, wenn die tatsächlichen Identitätsdaten auf der Clientseite bekannt sind.

Die API wird weiterhin im Hintergrund geladen. Dieser Inspektor darf jedoch nicht als Beweis dafür gelten, dass ein angezeigter Name der tatsächlichen Marktidentität entspricht.
