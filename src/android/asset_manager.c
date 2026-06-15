#include "asset_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct AAssetManager {
    char base_path[256];
};

struct AAsset {
    FILE* fp;
    char name[256];
};

static struct AAssetManager g_mgr;

void asset_manager_init(const char* base_path) {
    strncpy(g_mgr.base_path, base_path, sizeof(g_mgr.base_path));
}

AAssetManager* AAssetManager_fromJava(void* env, void* assetManager) {
    return &g_mgr;
}

AAsset* AAssetManager_open(AAssetManager* mgr, const char* filename, int mode) {
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/%s", mgr->base_path, filename);
    
    printf("OPEN: %s\n", filename);
    
    if (strstr(filename, "swordigo_title_2x.pvr") != NULL) {
        printf("\n***************************\n");
        printf("* RENDERING PHASE REACHED *\n");
        printf("***************************\n\n");
    }

    FILE* fp = fopen(full_path, "rb");
    if (!fp) {
        // printf("AssetManager: Failed to open %s\n", full_path);
        return NULL;
    }
    
    AAsset* asset = (AAsset*)malloc(sizeof(AAsset));
    asset->fp = fp;
    strncpy(asset->name, filename, sizeof(asset->name));
    return asset;
}

int AAsset_read(AAsset* asset, void* buf, size_t count) {
    if (!asset || !asset->fp) return -1;
    int read = fread(buf, 1, count, asset->fp);
    printf("READ: %s (%d bytes)\n", asset->name, read);
    return read;
}

void AAsset_close(AAsset* asset) {
    if (asset) {
        printf("CLOSE: %s\n", asset->name);
        if (asset->fp) fclose(asset->fp);
        free(asset);
    }
}

off_t AAsset_getLength(AAsset* asset) {
    if (!asset || !asset->fp) return 0;
    long current = ftell(asset->fp);
    fseek(asset->fp, 0, SEEK_END);
    off_t len = ftell(asset->fp);
    fseek(asset->fp, current, SEEK_SET);
    printf("GETLEN: %s -> %ld\n", asset->name, (long)len);
    return len;
}


int AAsset_openFileDescriptor(AAsset* asset, off_t* outStart, off_t* outLength) {
    if (!asset || !asset->fp) return -1;
    *outStart = 0;
    *outLength = AAsset_getLength(asset);
    return fileno(asset->fp);
}
