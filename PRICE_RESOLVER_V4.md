# V4 Price Resolver

This build uses the discovered Geyser custom identifier as the primary identity for custom OPSUCHT items.

- Custom identifiers are normalized into a stable technical key.
- API-side item identifiers and all useful item strings are indexed under the same technical-key form.
- Geyser custom items are never allowed to fall back to the vanilla material price.
- If no exact custom identifier match exists, the HUD shows `-` instead of a wrong price.
- The HUD displays the OPSUCHT API display name when an exact match exists.
- If an API display name is unavailable, a short readable fallback is generated from the technical identifier.
- The previous inspector block is removed from the visible HUD.

Vanilla items keep the existing exact name/alias matching. The resolver does not claim to price enchanted variants correctly until the API exposes enough variant identity to match them exactly.
