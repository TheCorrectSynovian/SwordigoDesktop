# TVPG vs ext4 Source File Comparison

## File Size Comparison (main.cpp)

| Location | Size | Notes |
|----------|------|-------|
| ext4 `~/SwordigoDesktop/src/main.cpp` | **252,888 bytes** | ✅ CURRENT (largest, most features) |
| TVPG `src/main.cpp` | 224,843 bytes | Intermediate snapshot |
| TVPG `.src/main.cpp` | 177,356 bytes | Oldest snapshot |

## Conclusion
**ext4 is the MOST up-to-date version** — no features lost.

The TVPG copies are OLDER snapshots. Features like coins, death loop fix, SRE hooks, etc. are all in the ext4 version.

## Coin Show/Hide Issue
The `g_sre_player_coins` symbol is referenced in main.cpp but **never defined in libsre.so** (the SRE source has no `g_sre_player_coins` variable). The `sre_player_coins_addr` resolves to 0, so the debug HUD reads from address 0 = always shows 0.

**Fix needed**: Define `g_sre_player_coins` in SRE and populate it from the game state.
