# Swordigo Version Comparison

| Feature | Swordigo 1.1 | Swordigo 1.4.6 |
|---|---|---|
| **Binary Size (libswordigo.so)** | ~4.7 MB | ~4.8 MB |
| **Total Dynamic Symbols** | 18,656 | 17,219 |
| **Caver:: Symbols** | 16,435 | 15,380 |
| **JNI Native Exports** | 18 | 31 |
| **Vita Port Alignment** | Low (Missing key exports) | High (Matches call sequence) |
| **Android API Level** | Older (Basic) | Newer (Includes Ads, Play Services) |

## Summary
Swordigo 1.4.6 is a significantly updated version that introduced Google Play Games integration, Cloud Snapshots, and Ads. Crucially, it added the `handleApplicationLaunch` and `googleSignInCompleted` JNI exports that the Vita port relies upon.
