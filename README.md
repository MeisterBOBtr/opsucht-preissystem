# OPSUCHT Inventarwert V4 – Price Resolver

Android/Bedrock client mod for the OPSUCHT inventory value HUD.

## This build

The previous inspector proved that the client exposes a useful Geyser custom identifier. This version uses that identifier for exact price lookup instead of guessing from the vanilla base item.

### Important behavior

- API remains a fixed background source.
- Inventory is refreshed about once per second.
- Geyser custom items use their technical identifier as primary identity.
- Custom items never fall back to a vanilla material price.
- Exact match -> OPSUCHT display name + average unit price.
- No exact match -> `-` (safe, rather than a wrong price).
- The technical identifier stays internal; the HUD shows a short/official name.

## Install/build

Replace the complete project with this ZIP, keep `.github/workflows/build.yml` as included, and run the GitHub Actions build.

Do not edit individual source files unless a build error requires it.


## Price/list update
- Geyser custom price resolution now also derives the readable technical suffix (e.g. `..._golden_excalibur`) and performs exact matching against OPSUCHT display-name/alias data.
- Custom items never fall back to their vanilla material price.
- HUD list expanded to 18 visible rows with a compact 23px row height so the footer remains clear.
