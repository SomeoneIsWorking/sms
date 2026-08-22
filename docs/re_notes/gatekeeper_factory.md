# GateKeeper factory evidence and native blocker

The GateKeeper scene-factory registration is proven from the GMSE01 retail DOL,
but it is deliberately **not active** in `MarNameRefGen_Enemy.cpp`. Activating it
pulls `Enemy/gatekeeper.cpp` out of the native static archive and exposes a real,
unimplemented Gorogoro behavior. The factory change was reverted; do not replace
that missing behavior with a stub.

## Retail factory evidence

Ghidra identifies `TMarNameRefGen::getNameRef_Enemy` at `0x802aa718`. Its two
adjacent GateKeeper branches are:

- `"GateKeeper"` at `0x803a5940`, compared at `0x802ac234`. Equality allocates
  `0x2a0` bytes, then calls
  `TBiancoGateKeeper::TBiancoGateKeeper(const char*)` at `0x800fc170`.
  Constructor argument `0x803a594c` decodes from Shift-JIS as
  `ビアンコゲートキーパー`.
- `"GateKeeperManager"` at `0x803a5964`, compared at `0x802ac26c`. Equality
  allocates `0x54` bytes, then calls
  `TBiancoGateKeeperManager::TBiancoGateKeeperManager(const char*)` at
  `0x800fca50`. Constructor argument `0x803a5978` decodes from Shift-JIS as
  `ゲートキーパーマネージャー`.

The actor constructor installs the `TBiancoGateKeeper` vtable and initializes
fields through offset `0x29c`. The manager constructor calls the enemy-manager
base constructor and installs the `TBiancoGateKeeperManager` vtable. The matched
actor `init(TLiveManager*)` stores its manager and calls `manageActor(this)`, so
ownership follows the ordinary `TEnemyManager` path; there is no separate
factory-side ownership operation to guess.

## Why activation does not link natively

A Clang native build with only those two factory cases activated compiled the
factory and `gatekeeper.cpp`, then failed the final `sms-boot` link on four
`R_X86_64_PLT32` relocations to the undefined
`TGorogoro::generateByGateKeeper(const TVec3<float>&, const TVec3<float>&)`:

| Native object owner | relocation offset |
|---|---:|
| `TBiancoGateKeeper::launchGorogoro()` | `0x15db` |
| `TNerveBGKSleepDamage::execute(...) const` | `0x3103` |
| `TNerveBGKDive::execute(...) const` | `0x3790` |
| `TNerveBGKLaunchGoro::execute(...) const` | `0x3889` |

The first is the emitted out-of-line helper. Clang also inlines its body into
the three nerve functions, producing the other three relocations. In the retail
DOL, `TGorogoro::generateByGateKeeper` is at `0x800b1208`; Ghidra finds the
corresponding three direct calls at `0x800f8d10`, `0x800f9070`, and
`0x800f9a04` inside those three nerve functions.

The proper next unit is therefore to decompile and port
`TGorogoro::generateByGateKeeper` (including any state it owns), verify it
against the DOL, and only then activate the two factory cases and remove
`GateKeeper` / `GateKeeperManager` from the native unimplemented allowlist.
Returning success, skipping launches, or adding a no-op body would hide a game
behavior dependency and is not an acceptable factory fix.

There is no useful pure unit seam for the factory itself: both constructors
enter the game heap and the JDrama/TEnemyManager hierarchy. Once the Gorogoro
behavior is present, verification is a Clang native build followed by a bounded
`run-safe.sh` stage containing both scene types; neither type may be classified
as unimplemented, and the manager must load before actor initialization.
