#ifndef JNI_LAYER_ARM64_H
#define JNI_LAYER_ARM64_H

#include <stdint.h>

// ============================================================================
// JNI Types for ARM64
//
// Key difference from ARM32:
//   All pointers are 64-bit → function pointers in vtable are uint64_t
//   jlong is actually 64-bit (not truncated like ARM32)
//   All vtable offsets DOUBLE (8 bytes per entry instead of 4)
//
//   ARM32 offset 0x84 (GetMethodID, entry [33]) → ARM64 offset 0x108
//   Formula: arm64_offset = arm32_entry_index * 8
// ============================================================================

// JNI scalar types — same semantics, but pointers are 64-bit in guest memory
typedef uint32_t jint_64;
typedef uint32_t jsize_64;
typedef uint8_t  jboolean_64;
typedef int64_t  jlong_64;       // Actually 64-bit on ARM64!
typedef float    jfloat_64;      // Hardware float on ARM64
typedef double   jdouble_64;     // Hardware double on ARM64

// JNI reference types — 64-bit pointers in ARM64 guest
typedef uint64_t jobject_64;
typedef uint64_t jclass_64;
typedef uint64_t jthrowable_64;
typedef uint64_t jstring_64;
typedef uint64_t jarray_64;
typedef uint64_t jobjectArray_64;
typedef uint64_t jintArray_64;
typedef uint64_t jbyteArray_64;
typedef uint64_t jmethodID_64;
typedef uint64_t jfieldID_64;

// ============================================================================
// JNI Function Table for JNIEnv — ARM64 version
//
// Each entry is a 64-bit function pointer (guest address).
// Total of 229 functions. We only populate the ones Swordigo calls.
//
// Entry index stays the same as ARM32, but byte offsets double:
//   Entry [N] is at byte offset N * 8
// ============================================================================

typedef struct {
    uint64_t reserved0;
    uint64_t reserved1;
    uint64_t reserved2;
    uint64_t reserved3;
    uint64_t GetVersion;                  // [4]
    uint64_t DefineClass;                 // [5]
    uint64_t FindClass;                   // [6]   ARM32: 0x18, ARM64: 0x30
    uint64_t FromReflectedMethod;         // [7]
    uint64_t FromReflectedField;          // [8]
    uint64_t ToReflectedMethod;           // [9]
    uint64_t GetSuperclass;               // [10]
    uint64_t IsAssignableFrom;            // [11]
    uint64_t ToReflectedField;            // [12]
    uint64_t Throw;                       // [13]
    uint64_t ThrowNew;                    // [14]
    uint64_t ExceptionOccurred;           // [15]
    uint64_t ExceptionDescribe;           // [16]
    uint64_t ExceptionClear;              // [17]
    uint64_t FatalError;                  // [18]
    uint64_t PushLocalFrame;              // [19]
    uint64_t PopLocalFrame;               // [20]
    uint64_t NewGlobalRef;                // [21]
    uint64_t DeleteGlobalRef;             // [22]
    uint64_t DeleteLocalRef;              // [23]
    uint64_t IsSameObject;                // [24]
    uint64_t NewLocalRef;                 // [25]
    uint64_t EnsureLocalCapacity;         // [26]
    uint64_t AllocObject;                 // [27]
    uint64_t NewObject;                   // [28]
    uint64_t NewObjectV;                  // [29]
    uint64_t NewObjectA;                  // [30]
    uint64_t GetObjectClass;              // [31]
    uint64_t IsInstanceOf;                // [32]
    uint64_t GetMethodID;                 // [33]
    uint64_t CallObjectMethod;            // [34]
    uint64_t CallObjectMethodV;           // [35]
    uint64_t CallObjectMethodA;           // [36]
    uint64_t CallBooleanMethod;           // [37]
    uint64_t CallBooleanMethodV;          // [38]
    uint64_t CallBooleanMethodA;          // [39]
    uint64_t CallByteMethod;              // [40]
    uint64_t CallByteMethodV;             // [41]
    uint64_t CallByteMethodA;             // [42]
    uint64_t CallCharMethod;              // [43]
    uint64_t CallCharMethodV;             // [44]
    uint64_t CallCharMethodA;             // [45]
    uint64_t CallShortMethod;             // [46]
    uint64_t CallShortMethodV;            // [47]
    uint64_t CallShortMethodA;            // [48]
    uint64_t CallIntMethod;               // [49]
    uint64_t CallIntMethodV;              // [50]
    uint64_t CallIntMethodA;              // [51]
    uint64_t CallLongMethod;              // [52]
    uint64_t CallLongMethodV;             // [53]
    uint64_t CallLongMethodA;             // [54]
    uint64_t CallFloatMethod;             // [55]
    uint64_t CallFloatMethodV;            // [56]
    uint64_t CallFloatMethodA;            // [57]
    uint64_t CallDoubleMethod;            // [58]
    uint64_t CallDoubleMethodV;           // [59]
    uint64_t CallDoubleMethodA;           // [60]
    uint64_t CallVoidMethod;              // [61]
    uint64_t CallVoidMethodV;             // [62]
    uint64_t CallVoidMethodA;             // [63]
    uint64_t CallNonvirtualObjectMethod;  // [64]
    uint64_t CallNonvirtualObjectMethodV; // [65]
    uint64_t CallNonvirtualObjectMethodA; // [66]
    uint64_t CallNonvirtualBooleanMethod; // [67]
    uint64_t CallNonvirtualBooleanMethodV;// [68]
    uint64_t CallNonvirtualBooleanMethodA;// [69]
    uint64_t CallNonvirtualByteMethod;    // [70]
    uint64_t CallNonvirtualByteMethodV;   // [71]
    uint64_t CallNonvirtualByteMethodA;   // [72]
    uint64_t CallNonvirtualCharMethod;    // [73]
    uint64_t CallNonvirtualCharMethodV;   // [74]
    uint64_t CallNonvirtualCharMethodA;   // [75]
    uint64_t CallNonvirtualShortMethod;   // [76]
    uint64_t CallNonvirtualShortMethodV;  // [77]
    uint64_t CallNonvirtualShortMethodA;  // [78]
    uint64_t CallNonvirtualIntMethod;     // [79]
    uint64_t CallNonvirtualIntMethodV;    // [80]
    uint64_t CallNonvirtualIntMethodA;    // [81]
    uint64_t CallNonvirtualLongMethod;    // [82]
    uint64_t CallNonvirtualLongMethodV;   // [83]
    uint64_t CallNonvirtualLongMethodA;   // [84]
    uint64_t CallNonvirtualFloatMethod;   // [85]
    uint64_t CallNonvirtualFloatMethodV;  // [86]
    uint64_t CallNonvirtualFloatMethodA;  // [87]
    uint64_t CallNonvirtualDoubleMethod;  // [88]
    uint64_t CallNonvirtualDoubleMethodV; // [89]
    uint64_t CallNonvirtualDoubleMethodA; // [90]
    uint64_t CallNonvirtualVoidMethod;    // [91]
    uint64_t CallNonvirtualVoidMethodV;   // [92]
    uint64_t CallNonvirtualVoidMethodA;   // [93]
    uint64_t GetFieldID;                  // [94]
    uint64_t GetObjectField;              // [95]
    uint64_t GetBooleanField;             // [96]
    uint64_t GetByteField;                // [97]
    uint64_t GetCharField;                // [98]
    uint64_t GetShortField;               // [99]
    uint64_t GetIntField;                 // [100]
    uint64_t GetLongField;                // [101]
    uint64_t GetFloatField;               // [102]
    uint64_t GetDoubleField;              // [103]
    uint64_t SetObjectField;              // [104]
    uint64_t SetBooleanField;             // [105]
    uint64_t SetByteField;                // [106]
    uint64_t SetCharField;                // [107]
    uint64_t SetShortField;               // [108]
    uint64_t SetIntField;                 // [109]
    uint64_t SetLongField;                // [110]
    uint64_t SetFloatField;               // [111]
    uint64_t SetDoubleField;              // [112]
    uint64_t GetStaticMethodID;           // [113]
    uint64_t CallStaticObjectMethod;      // [114]
    uint64_t CallStaticObjectMethodV;     // [115]
    uint64_t CallStaticObjectMethodA;     // [116]
    uint64_t CallStaticBooleanMethod;     // [117]
    uint64_t CallStaticBooleanMethodV;    // [118]
    uint64_t CallStaticBooleanMethodA;    // [119]
    uint64_t CallStaticByteMethod;        // [120]
    uint64_t CallStaticByteMethodV;       // [121]
    uint64_t CallStaticByteMethodA;       // [122]
    uint64_t CallStaticCharMethod;        // [123]
    uint64_t CallStaticCharMethodV;       // [124]
    uint64_t CallStaticCharMethodA;       // [125]
    uint64_t CallStaticShortMethod;       // [126]
    uint64_t CallStaticShortMethodV;      // [127]
    uint64_t CallStaticShortMethodA;      // [128]
    uint64_t CallStaticIntMethod;         // [129]
    uint64_t CallStaticIntMethodV;        // [130]
    uint64_t CallStaticIntMethodA;        // [131]
    uint64_t CallStaticLongMethod;        // [132]
    uint64_t CallStaticLongMethodV;       // [133]
    uint64_t CallStaticLongMethodA;       // [134]
    uint64_t CallStaticFloatMethod;       // [135]
    uint64_t CallStaticFloatMethodV;      // [136]
    uint64_t CallStaticFloatMethodA;      // [137]
    uint64_t CallStaticDoubleMethod;      // [138]
    uint64_t CallStaticDoubleMethodV;     // [139]
    uint64_t CallStaticDoubleMethodA;     // [140]
    uint64_t CallStaticVoidMethod;        // [141]
    uint64_t CallStaticVoidMethodV;       // [142]
    uint64_t CallStaticVoidMethodA;       // [143]
    uint64_t GetStaticFieldID;            // [144]
    uint64_t GetStaticObjectField;        // [145]
    uint64_t GetStaticBooleanField;       // [146]
    uint64_t GetStaticByteField;          // [147]
    uint64_t GetStaticCharField;          // [148]
    uint64_t GetStaticShortField;         // [149]
    uint64_t GetStaticIntField;           // [150]
    uint64_t GetStaticLongField;          // [151]
    uint64_t GetStaticFloatField;         // [152]
    uint64_t GetStaticDoubleField;        // [153]
    uint64_t SetStaticObjectField;        // [154]
    uint64_t SetStaticBooleanField;       // [155]
    uint64_t SetStaticByteField;          // [156]
    uint64_t SetStaticCharField;          // [157]
    uint64_t SetStaticShortField;         // [158]
    uint64_t SetStaticIntField;           // [159]
    uint64_t SetStaticLongField;          // [160]
    uint64_t SetStaticFloatField;         // [161]
    uint64_t SetStaticDoubleField;        // [162]
    uint64_t NewString;                   // [163]
    uint64_t GetStringLength;             // [164]
    uint64_t GetStringChars;              // [165]
    uint64_t ReleaseStringChars;          // [166]
    uint64_t NewStringUTF;                // [167]
    uint64_t GetStringUTFLength;          // [168]
    uint64_t GetStringUTFChars;           // [169]
    uint64_t ReleaseStringUTFChars;       // [170]
    uint64_t GetArrayLength;              // [171]
    uint64_t NewObjectArray;              // [172]
    uint64_t GetObjectArrayElement;       // [173]
    uint64_t SetObjectArrayElement;       // [174]
    uint64_t NewBooleanArray;             // [175]
    uint64_t NewByteArray;                // [176]
    uint64_t NewCharArray;                // [177]
    uint64_t NewShortArray;               // [178]
    uint64_t NewIntArray;                 // [179]
    uint64_t NewLongArray;                // [180]
    uint64_t NewFloatArray;               // [181]
    uint64_t NewDoubleArray;              // [182]
    uint64_t GetBooleanArrayElements;     // [183]
    uint64_t GetByteArrayElements;        // [184]
    uint64_t GetCharArrayElements;        // [185]
    uint64_t GetShortArrayElements;       // [186]
    uint64_t GetIntArrayElements;         // [187]
    uint64_t GetLongArrayElements;        // [188]
    uint64_t GetFloatArrayElements;       // [189]
    uint64_t GetDoubleArrayElements;      // [190]
    uint64_t ReleaseBooleanArrayElements; // [191]
    uint64_t ReleaseByteArrayElements;    // [192]
    uint64_t ReleaseCharArrayElements;    // [193]
    uint64_t ReleaseShortArrayElements;   // [194]
    uint64_t ReleaseIntArrayElements;     // [195]
    uint64_t ReleaseLongArrayElements;    // [196]
    uint64_t ReleaseFloatArrayElements;   // [197]
    uint64_t ReleaseDoubleArrayElements;  // [198]
    uint64_t GetBooleanArrayRegion;       // [199]
    uint64_t GetByteArrayRegion;          // [200]
    uint64_t GetCharArrayRegion;          // [201]
    uint64_t GetShortArrayRegion;         // [202]
    uint64_t GetIntArrayRegion;           // [203]
    uint64_t GetLongArrayRegion;          // [204]
    uint64_t GetFloatArrayRegion;         // [205]
    uint64_t GetDoubleArrayRegion;        // [206]
    uint64_t SetBooleanArrayRegion;       // [207]
    uint64_t SetByteArrayRegion;          // [208]
    uint64_t SetCharArrayRegion;          // [209]
    uint64_t SetShortArrayRegion;         // [210]
    uint64_t SetIntArrayRegion;           // [211]
    uint64_t SetLongArrayRegion;          // [212]
    uint64_t SetFloatArrayRegion;         // [213]
    uint64_t SetDoubleArrayRegion;        // [214]
    uint64_t RegisterNatives;             // [215]
    uint64_t UnregisterNatives;           // [216]
    uint64_t MonitorEnter;                // [217]
    uint64_t MonitorExit;                 // [218]
    uint64_t GetJavaVM;                   // [219]
    uint64_t GetStringRegion;             // [220]
    uint64_t GetStringUTFRegion;          // [221]
    uint64_t GetPrimitiveArrayCritical;   // [222]
    uint64_t ReleasePrimitiveArrayCritical;// [223]
    uint64_t GetStringCritical;           // [224]
    uint64_t ReleaseStringCritical;       // [225]
    uint64_t NewWeakGlobalRef;            // [226]
    uint64_t DeleteWeakGlobalRef;         // [227]
    uint64_t ExceptionCheck;              // [228]
} JNIEnv_vtable_64;

typedef struct {
    uint64_t functions; // Pointer to JNIEnv_vtable_64
} JNIEnv_fake_64;

typedef struct {
    uint64_t reserved0;
    uint64_t reserved1;
    uint64_t reserved2;
    uint64_t DestroyJavaVM;
    uint64_t AttachCurrentThread;
    uint64_t DetachCurrentThread;
    uint64_t GetEnv;                      // [6]
    uint64_t AttachCurrentThreadAsDaemon;
} JavaVM_vtable_64;

typedef struct {
    uint64_t functions; // Pointer to JavaVM_vtable_64
} JavaVM_fake_64;

#endif
