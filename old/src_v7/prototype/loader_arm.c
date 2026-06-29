#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>

// Fake JNI structures
typedef struct {
    void* reserved[300];
} JNIEnv;

typedef struct {
    void* reserved[300];
} JavaVM;

// Shim functions we might need if it complains about unresolved symbols
int __android_log_print(int prio, const char *tag, const char *fmt, ...) {
    printf("[LOG] %s\n", tag);
    return 0;
}

void* AAssetManager_fromJava(void* env, void* assetManager) { return NULL; }
void* AAssetManager_open(void* mgr, const char* filename, int mode) { return NULL; }
int AAsset_read(void* asset, void* buf, size_t count) { return 0; }
void AAsset_close(void* asset) {}
long AAsset_getLength(void* asset) { return 0; }

int main() {
    printf("[*] ARM PoC Loader started.\n");

    const char* lib_path = "./reference/lib/swordigo 1.4.6/armeabi-v7a/libswordigo.so";
    printf("[*] Trying to dlopen %s\n", lib_path);

    void* handle = dlopen(lib_path, RTLD_LAZY | RTLD_LOCAL);
    if (!handle) {
        printf("[-] dlopen failed: %s\n", dlerror());
        return 1;
    }
    printf("[+] dlopen success!\n");

    void (*setupNativeInterface)(void*, void*) = dlsym(handle, "Java_com_touchfoo_swordigo_Native_setupNativeInterface");
    if (!setupNativeInterface) {
        printf("[-] Could not find setupNativeInterface: %s\n", dlerror());
        return 1;
    }
    printf("[+] Found setupNativeInterface at %p\n", setupNativeInterface);

    void (*setupApplication)(void*, void*) = dlsym(handle, "Java_com_touchfoo_swordigo_Native_setupApplication");
    if (!setupApplication) {
        printf("[-] Could not find setupApplication: %s\n", dlerror());
        return 1;
    }
    printf("[+] Found setupApplication at %p\n", setupApplication);

    // Call them with fake env
    JNIEnv env = {0};
    
    printf("[*] Calling setupNativeInterface...\n");
    setupNativeInterface(&env, NULL);
    printf("[+] setupNativeInterface returned!\n");

    printf("[*] Calling setupApplication...\n");
    setupApplication(&env, NULL);
    printf("[+] setupApplication returned!\n");

    dlclose(handle);
    return 0;
}
