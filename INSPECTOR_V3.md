# V3 Item Inspector

This build is a diagnostic step, not a final pricing fix.

It keeps the working inventory/API/HUD path but records, for occupied slots, the client-side ItemStack evidence currently accessible through the native bridge:
- candidate strings returned by the available ItemStack functions
- Item pointer / vtable pointer
- first 32 bytes of the current ItemStack as a diagnostic fingerprint

The purpose is to stop guessing. OPSUCHT/Geyser items can use a vanilla base item while carrying custom identity and/or components. The next resolver should only be written after the actual client-side identity evidence is known.

The API is still loaded in the background, but this inspector must not be treated as proof that a displayed name is the market identity.
