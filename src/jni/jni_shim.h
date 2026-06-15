#ifndef JNI_SHIM_H
#define JNI_SHIM_H

#include <stdint.h>
#include <stddef.h>

typedef void* jobject;
typedef void* jclass;
typedef void* jmethodID;
typedef void* jfieldID;
typedef void* jstring;
typedef void* jarray;
typedef int32_t jint;
typedef uint8_t jboolean;
typedef float jfloat;
typedef double jdouble;
typedef long jlong;

// Large struct to hold JNI functions at correct offsets
typedef struct {
    void* functions[300];
} JNINativeInterface;

typedef const JNINativeInterface* JNIEnv;

typedef struct {
    void* functions[300];
} JNIInvokeInterface;

typedef const JNIInvokeInterface* JavaVM;

void jni_init_env(JNIEnv* env);
void jni_init_vm(JavaVM* vm);

#endif
