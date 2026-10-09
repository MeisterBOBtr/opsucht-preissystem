# V5 display rollback fix

The previous V5 display-name change was too aggressive: on this client, `getCustomName()` can return the technical Geyser identifier. That caused the HUD to lose the readable/API names.

This corrective build restores the proven V4 price resolver and only uses `getCustomName()` when it is actually a human-readable name. Technical `geyser_custom_*` values are never rendered as the HUD name.

No new price guessing is introduced here. Custom items still do not fall back to vanilla prices.
