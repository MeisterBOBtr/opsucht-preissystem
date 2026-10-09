# Performance + Custom-Item Fix

## What changed

The previous build rescanned 36 slots every few ticks and repeatedly ran the full API matching logic. On Android this can cause visible frame drops.

This build scans the inventory about once per second and caches resolved item prices. The cache is invalidated when the API price table changes.

The second issue was more dangerous: a Geyser Custom item could fail its custom mapping and then fall through to the Java/vanilla material. A custom Netherite Pickaxe could therefore display the normal Netherite Pickaxe price.

The new resolver explicitly blocks that fallback for `Geyser Custom:` stacks. An unresolved custom item is shown without a price instead of receiving a wrong vanilla price.

## Remaining technical point

The correct OPSUCHT price still requires a reliable Bedrock/Geyser custom identifier. Geyser's custom-item system uses a unique Bedrock identifier, but BedrockTools does not expose a universal public wrapper for that server-side mapping. This build therefore fixes the unsafe fallback and performance first rather than pretending an unverified mapping is correct.
