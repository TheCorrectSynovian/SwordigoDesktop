#ifndef ASSET_MANAGER_H
#define ASSET_MANAGER_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct AAssetManager AAssetManager;
typedef struct AAsset AAsset;

// Simplified AAssetManager APIs
AAssetManager* AAssetManager_fromJava(void* env, void* assetManager);
AAsset* AAssetManager_open(AAssetManager* mgr, const char* filename, int mode);
int AAsset_read(AAsset* asset, void* buf, size_t count);
void AAsset_close(AAsset* asset);
off_t AAsset_getLength(AAsset* asset);
int AAsset_openFileDescriptor(AAsset* asset, off_t* outStart, off_t* outLength);

// Internal setup
void asset_manager_init(const char* base_path);

#endif
