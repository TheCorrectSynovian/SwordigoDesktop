# JNI Export Diff: 1.1 vs 1.4.6

This document lists the JNI exports present in 1.4.6 but missing in 1.1.

## Added in 1.4.6
- `Java_com_touchfoo_swordigo_Native_debugFunction`
- `Java_com_touchfoo_swordigo_Native_finishedRestoringPurchases`
- `Java_com_touchfoo_swordigo_Native_googleSignInCompleted`
- `Java_com_touchfoo_swordigo_Native_handleApplicationLaunch`
- `Java_com_touchfoo_swordigo_Native_interstitialAdVisibilityChanged`
- `Java_com_touchfoo_swordigo_Native_productPurchased`
- `Java_com_touchfoo_swordigo_Native_productPurchaseFailed`
- `Java_com_touchfoo_swordigo_Native_reloadContext`
- `Java_com_touchfoo_swordigo_Native_reviewFlowCompleted`
- `Java_com_touchfoo_swordigo_Native_snapshotLoaded`
- `Java_com_touchfoo_swordigo_Native_startedRestoringPurchases`
- `Java_com_touchfoo_swordigo_Native_storeProductFetched`
- `Java_com_touchfoo_swordigo_Native_storeProductFetchFailed`

## Core Startup Comparison
The following sequence is used in 1.4.6 and Vita, but fails in 1.1 due to missing symbols:
1. `setFilesDir`
2. `setCacheDir`
3. `setAssetManager`
4. **`googleSignInCompleted`** (Missing in 1.1)
5. **`handleApplicationLaunch`** (Missing in 1.1)
6. `setupNativeInterface`
7. `setupApplication`
