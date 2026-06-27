#include "jni_shim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static jclass FindClass(void* env, const char* name) {
    printf("JNI: FindClass(%s)\n", name);
    return (jclass)0x41414141;
}

static jmethodID GetMethodID(void* env, jclass clazz, const char* name, const char* sig) {
    printf("JNI: GetMethodID(%s, %s)\n", name, sig);
    return (jmethodID)0x42424242;
}

static jmethodID GetStaticMethodID(void* env, jclass clazz, const char* name, const char* sig) {
    printf("JNI: GetStaticMethodID(%s, %s)\n", name, sig);
    return (jmethodID)0x43434343;
}

static jstring NewStringUTF(void* env, const char* bytes) {
    printf("JNI: NewStringUTF(%s)\n", bytes);
    return (jstring)strdup(bytes);
}

static const char* GetStringUTFChars(void* env, jstring string, jboolean* isCopy) {
    return (const char*)string;
}

static void ReleaseStringUTFChars(void* env, jstring string, const char* utf) {
}

static int GetEnv(void* vm, void** env, int version) {
    printf("JNI: GetEnv\n");
    return 0;
}

static JNINativeInterface g_jni_env;
static JNIInvokeInterface g_jni_vm;

void jni_init_env(JNIEnv* env_ptr) {
    memset(&g_jni_env, 0, sizeof(g_jni_env));
    
    // Assign functions based on Vita/Standard offsets (index = offset / 4)
    g_jni_env.functions[6] = FindClass;
    g_jni_env.functions[33] = GetMethodID;
    g_jni_env.functions[113] = GetStaticMethodID;
    g_jni_env.functions[167] = NewStringUTF;
    g_jni_env.functions[169] = GetStringUTFChars;
    g_jni_env.functions[170] = ReleaseStringUTFChars;
    
    *env_ptr = &g_jni_env;
}

void jni_init_vm(JavaVM* vm_ptr) {
    memset(&g_jni_vm, 0, sizeof(g_jni_vm));
    g_jni_vm.functions[6] = GetEnv; // Index 6 matches 0x18 offset in standard JNI? No, standard is different.
    // Vita main.c says: *(uintptr_t *)(fake_vm + 0x18) = (uintptr_t)GetEnv;
    // So index is 0x18 / 4 = 6.
    
    *vm_ptr = &g_jni_vm;
}
