# V7 – Custom-ID Price Fix

This build keeps the V6 list length and display changes.

## Main fix
The Android/Geyser inventory can expose a custom item as:
`geyser_custom_main_misc_may26_golden_excalibur`

The previous resolver tried to remove the `geyser custom` prefix before converting `_` to separators. That meant the real `geyser_custom_...` form could keep the prefix and fail to match an OPSUCHT/API identifier such as:
`main_misc_may26_golden_excalibur`.

V7 normalizes separators first and then removes the Geyser presentation prefix. It therefore tries the prefixless technical identifier as an exact API alias, without falling back to the vanilla material price.

## Important
This does not change the End Shield display handling or the 18-row HUD. It only strengthens custom-item price matching.
