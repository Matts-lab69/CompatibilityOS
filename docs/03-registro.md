# Registry and Immutable Base Strategy

## Status and labels

Both alternatives below are **PENDING USER DECISION**. Neither is selected. No implementation or migration test is claimed.

- **VERIFIED FACT** identifies behavior directly shown by pinned upstream source.
- **HYPOTHESIS** identifies a proposed CompatibilityOS design or expected consequence.
- **NOT ANALYZED** marks unanswered compatibility or implementation questions.

References are pinned to Wine commit [`7298a818b79446bae153b8d5aa27235ab3703bf9`](https://github.com/wine-mirror/wine/tree/7298a818b79446bae153b8d5aa27235ab3703bf9).

## Alternative A — effective per-app copy

**HYPOTHESIS — PENDING USER DECISION.** Keep a versioned, immutable base and seed an effective per-app copy under `~/.config/<app>/`. Apply versioned `.reg` migrations to that effective state, rather than replaying migrations on every launch. This is the **recommended** alternative, with **medium** estimated complexity.

The copy boundary, migration ordering, rollback policy, and exact registry files need implementation design. “Recommended” expresses a proposal only; it is not approval or a selected architecture.

**HYPOTHESIS — trade-off.** This preserves Wine's single-tree behavior without patching server registry semantics. It duplicates base data per app and needs upgrade and rollback rules that preserve deletions and MSI state.

## Alternative B — deltas and tombstones

**HYPOTHESIS — PENDING USER DECISION.** Keep the immutable base and store only per-app registry deltas, including tombstones for deleted values or keys. This is estimated **high** complexity because reads, writes, deletes, enumeration, and upgrades must consistently compose the base and overlay.

No delta overlay is implemented or tested. Tombstone semantics and conflict handling remain undefined.

**HYPOTHESIS — trade-off.** Deltas reduce duplication but add precedence, enumeration, and upgrade rules. Persisting only deltas is not an initial requirement.

## Verified upstream registry behavior

**VERIFIED FACT.** Wine's server registry initialization creates one tree per `wineserver` and loads `system.reg`, `userdef.reg`, and `user.reg`. See pinned [`server/registry.c`, lines 1829–1921](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/server/registry.c#L1829-L1921).

**VERIFIED FACT.** The inspected save path writes complete branches, not deltas. `save_branch` can truncate the target of a symlink or the inode shared by hard links. Do not link mutable registry files back to the base. See pinned [`server/registry.c`, lines 2016–2095](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/server/registry.c#L2016-L2095).

These source observations do not settle the CompatibilityOS decision or establish behavior across upgrades or multiple concurrent app instances.

## Decisions and open questions

- **PENDING USER DECISION:** Select Alternative A or Alternative B.
- **NOT ANALYZED:** Registry sharing/isolation requirements for apps that communicate through common Windows registry state.
- **NOT ANALYZED:** Migration rollback, interrupted updates, concurrent writers, and compatibility with arbitrary imported `.reg` files.
- **NOT ANALYZED:** Whether the Wine path table and the selected registry strategy can be combined without application-specific exceptions.
