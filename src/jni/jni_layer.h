#ifndef JNI_LAYER_H
#define JNI_LAYER_H

#include <stdint.h>

// JNI Types for 32-bit ARM
typedef uint32_t jint;
typedef uint32_t jsize;
typedef uint32_t jboolean;
typedef uint32_t jlong; // 64-bit in real JNI, but we'll handle it
typedef uint32_t jfloat;
typedef uint32_t jdouble;
typedef uint32_t jobject;
typedef uint32_t jclass;
typedef uint32_t jthrowable;
typedef uint32_t jstring;
typedef uint32_t jarray;
typedef uint32_t jbooleanArray;
typedef uint32_t jbyteArray;
typedef uint32_t jcharArray;
typedef uint32_t jshortArray;
typedef uint32_t jintArray;
typedef uint32_t jlongArray;
typedef uint32_t jfloatArray;
typedef uint32_t jdoubleArray;
typedef uint32_t jobjectArray;
typedef uint32_t jweak;
typedef uint32_t jmethodID;
typedef uint32_t jfieldID;

// JNI Function Table for JNIEnv (v1.6)
// Total of 229 functions. We only need the ones Swordigo calls.
typedef struct {
    uint32_t reserved0;
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t reserved3;
    uint32_t GetVersion;
    uint32_t DefineClass;
    uint32_t FindClass;           // [6] offset 0x18
    uint32_t FromReflectedMethod;
    uint32_t FromReflectedField;
    uint32_t ToReflectedMethod;
    uint32_t GetSuperclass;
    uint32_t IsAssignableFrom;
    uint32_t ToReflectedField;
    uint32_t Throw;
    uint32_t ThrowNew;            // [14] offset 0x38
    uint32_t ExceptionOccurred;
    uint32_t ExceptionDescribe;
    uint32_t ExceptionClear;
    uint32_t FatalError;
    uint32_t PushLocalFrame;      // [19] offset 0x4C
    uint32_t PopLocalFrame;       // [20] offset 0x50
    uint32_t NewGlobalRef;        // [21] offset 0x54
    uint32_t DeleteGlobalRef;     // [22] offset 0x58
    uint32_t DeleteLocalRef;      // [23] offset 0x5C
    uint32_t IsSameObject;
    uint32_t NewLocalRef;
    uint32_t EnsureLocalCapacity;
    uint32_t AllocObject;
    uint32_t NewObject;
    uint32_t NewObjectV;          // [29] offset 0x74
    uint32_t NewObjectA;
    uint32_t GetObjectClass;      // [31] offset 0x7C
    uint32_t IsInstanceOf;
    uint32_t GetMethodID;         // [33] offset 0x84
    uint32_t CallObjectMethod;
    uint32_t CallObjectMethodV;   // [35] offset 0x8C
    uint32_t CallObjectMethodA;
    uint32_t CallBooleanMethod;
    uint32_t CallBooleanMethodV;  // [38] offset 0x98
    uint32_t CallBooleanMethodA;
    uint32_t CallByteMethod;
    uint32_t CallByteMethodV;
    uint32_t CallByteMethodA;
    uint32_t CallCharMethod;
    uint32_t CallCharMethodV;
    uint32_t CallCharMethodA;
    uint32_t CallShortMethod;
    uint32_t CallShortMethodV;
    uint32_t CallShortMethodA;
    uint32_t CallIntMethod;
    uint32_t CallIntMethodV;      // [50] offset 0xC8
    uint32_t CallIntMethodA;
    uint32_t CallLongMethod;
    uint32_t CallLongMethodV;     // [53] offset 0xD4
    uint32_t CallLongMethodA;
    uint32_t CallFloatMethod;
    uint32_t CallFloatMethodV;
    uint32_t CallFloatMethodA;
    uint32_t CallDoubleMethod;
    uint32_t CallDoubleMethodV;
    uint32_t CallDoubleMethodA;
    uint32_t CallVoidMethod;
    uint32_t CallVoidMethodV;     // [62] offset 0xF8
    uint32_t CallVoidMethodA;
    // ... many more ...
    uint32_t padding[30]; // Placeholder for offsets up to 0x178
    uint32_t GetFieldID;          // [94] offset 0x178
    uint32_t GetBooleanField;      // [95] offset 0x17C
    uint32_t GetByteField;
    uint32_t GetCharField;
    uint32_t GetShortField;
    uint32_t GetIntField;         // [100] offset 0x190
    uint32_t GetLongField;
    uint32_t GetFloatField;       // [102] offset 0x198
    // ... more ...
    uint32_t padding2[10];
    uint32_t GetStaticMethodID;   // [113] offset 0x1C4
    uint32_t CallStaticObjectMethod;
    uint32_t CallStaticObjectMethodV; // [115] offset 0x1CC
    uint32_t CallStaticObjectMethodA;
    uint32_t CallStaticBooleanMethod;
    uint32_t CallStaticBooleanMethodV; // [118] offset 0x1D8
    // ... more ...
    uint32_t padding3[11];
    uint32_t CallStaticIntMethodV; // [130] offset 0x208
    uint32_t padding4[4];
    uint32_t CallStaticLongMethodV; // [135] offset 0x21C
    uint32_t CallStaticFloatMethodV; // [136] offset 0x220
    uint32_t padding5[5];
    uint32_t CallStaticVoidMethodV; // [142] offset 0x238
    uint32_t padding6[1];
    uint32_t GetStaticFieldID;    // [144] offset 0x240
    uint32_t GetStaticObjectField; // [145] offset 0x244
    // ... more ...
    uint32_t padding7[21];
    uint32_t NewStringUTF;        // [167] offset 0x29C
    uint32_t GetStringUTFLength;  // [168] offset 0x2A0
    uint32_t GetStringUTFChars;   // [169] offset 0x2A4
    uint32_t ReleaseStringUTFChars; // [170] offset 0x2A8
    uint32_t GetArrayLength;      // [171] offset 0x2AC
    uint32_t NewObjectArray;
    uint32_t GetObjectArrayElement; // [173] offset 0x2B4
    uint32_t padding8[5];
    uint32_t NewIntArray;         // [179] offset 0x2CC
    uint32_t padding9[7];
    uint32_t GetIntArrayElements; // [187] offset 0x2EC
    uint32_t padding10[7];
    uint32_t ReleaseIntArrayElements; // [195] offset 0x30C
    uint32_t padding11[15];
    uint32_t SetIntArrayRegion;   // [211] offset 0x34C
    uint32_t padding12[3];
    uint32_t RegisterNatives;     // [215] offset 0x35C
    uint32_t padding13[3];
    uint32_t GetJavaVM;           // [219] offset 0x36C
    uint32_t padding14[1];
    uint32_t GetStringUTFRegion;  // [221] offset 0x374
} JNIEnv_vtable;

typedef struct {
    uint32_t functions; // Pointer to JNIEnv_vtable
} JNIEnv_fake;

typedef struct {
    uint32_t reserved0;
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t DestroyJavaVM;
    uint32_t AttachCurrentThread;
    uint32_t DetachCurrentThread;
    uint32_t GetEnv;              // [6] offset 0x18
    uint32_t AttachCurrentThreadAsDaemon;
} JavaVM_vtable;

typedef struct {
    uint32_t functions; // Pointer to JavaVM_vtable
} JavaVM_fake;

#endif
