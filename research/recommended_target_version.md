# Recommended Target Version

## Recommendation: Swordigo 1.4.6

### Rationale

1. **Vita Port Parity**: The most significant asset we have for this port is the existing PS Vita loader. Our analysis proves that the Vita loader was built against version 1.4.x. Targeting 1.4.6 allows us to use the Vita source code as a direct template for JNI shimming and the initialization sequence.
2. **Feature Completeness**: 1.4.6 includes all modern gameplay features, including cloud saves and achievement systems, which are already partially mapped in the native symbols.
3. **Symbol Clarity**: While 1.1 has slightly more symbols, the delta is negligible compared to the architectural advantage of matching the Vita loader. Both versions are "Research Friendly."
4. **Boot Milestone**: To reach `setupApplication()` successfully on Linux, we must provide the state expected by the engine. 1.4.6 expects `handleApplicationLaunch`, which we can now implement based on the Vita reference.

### Immediate Action
Direct all future Boot Prototype work (Mission 3 onwards) toward the `reference/lib/swordigo 1.4.6/` binary and the `reference/decompiled/swordigo 1.4.6/` Java source.
