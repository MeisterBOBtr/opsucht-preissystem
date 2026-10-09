# Changes – V4 Price Resolver

- Uses the discovered Geyser custom technical identifier as the primary custom-item key.
- Adds stable technical-key normalization to both API aliases and client ItemStack candidates.
- Removes fuzzy custom-item matching.
- Prevents custom items from inheriting vanilla material prices.
- Shows OPSUCHT API display names when an exact identifier/alias match is found.
- Uses a short fallback display name for unresolved custom identifiers.
- Removes the diagnostic inspector from the normal HUD.
- Keeps the one-second inventory refresh/performance improvements.


## Price/list update
- Geyser custom price resolution now also derives the readable technical suffix (e.g. `..._golden_excalibur`) and performs exact matching against OPSUCHT display-name/alias data.
- Custom items never fall back to their vanilla material price.
- HUD list expanded to 18 visible rows with a compact 23px row height so the footer remains clear.
