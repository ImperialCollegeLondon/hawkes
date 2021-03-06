/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class dr_inference_hawkes_NativeHPHSingleton */

#ifndef _Included_dr_inference_hawkes_NativeHPHSingleton
#define _Included_dr_inference_hawkes_NativeHPHSingleton
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     dr_inference_hawkes_NativeHPHSingleton
 * Method:    initialize
 * Signature: (IIJ)I
 */
JNIEXPORT jint JNICALL Java_dr_inference_hawkes_NativeHPHSingleton_initialize
  (JNIEnv *, jobject, jint, jint, jlong, jint, jint);

/*
 * Class:     dr_inference_hawkes_NativeHPHSingleton
 * Method:    updateLocations
 * Signature: (II[D)V
 */
JNIEXPORT void JNICALL Java_dr_inference_hawkes_NativeHPHSingleton_updateLocations
  (JNIEnv *, jobject, jint, jint, jdoubleArray);

/*
 * Class:     dr_inference_hawkes_NativeHPHSingleton
 * Method:    getSumOfIncrements
 * Signature: (I)D
 */
JNIEXPORT jdouble JNICALL Java_dr_inference_hawkes_NativeHPHSingleton_getSumOfIncrements
  (JNIEnv *, jobject, jint);

/*
 * Class:     dr_inference_hawkes_NativeHPHSingleton
 * Method:    storeState
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_dr_inference_hawkes_NativeHPHSingleton_storeState
  (JNIEnv *, jobject, jint);

/*
 * Class:     dr_inference_hawkes_NativeHPHSingleton
 * Method:    restoreState
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_dr_inference_hawkes_NativeHPHSingleton_restoreState
  (JNIEnv *, jobject, jint);

/*
 * Class:     dr_inference_hawkes_NativeHPHSingleton
 * Method:    acceptState
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_dr_inference_hawkes_NativeHPHSingleton_acceptState
  (JNIEnv *, jobject, jint);

/*
 * Class:     dr_inference_hawkes_NativeHPHSingleton
 * Method:    makeDirty
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_dr_inference_hawkes_NativeHPHSingleton_makeDirty
  (JNIEnv *, jobject, jint);

/*
 * Class:     dr_inference_hawkes_NativeHPHSingleton
 * Method:    setPairwiseData
 * Signature: (I[D)V
 */
JNIEXPORT void JNICALL Java_dr_inference_hawkes_NativeHPHSingleton_setPairwiseData
  (JNIEnv *, jobject, jint, jdoubleArray);

/*
 * Class:     dr_inference_hawkes_NativeHPHSingleton
 * Method:    setParameters
 * Signature: (I[D)V
 */
JNIEXPORT void JNICALL Java_dr_inference_hawkes_NativeHPHSingleton_setParameters
  (JNIEnv *, jobject, jint, jdoubleArray);

/*
 * Class:     dr_inference_hawkes_NativeHPHSingleton
 * Method:    getPairwiseData
 * Signature: (I)[D
 */
JNIEXPORT jdoubleArray JNICALL Java_dr_inference_hawkes_NativeHPHSingleton_getPairwiseData
  (JNIEnv *, jobject, jint);

/*
 * Class:     dr_inference_hawkes_NativeHPHSingleton
 * Method:    getLocationGradient
 * Signature: (I[D)V
 */
JNIEXPORT void JNICALL Java_dr_inference_hawkes_NativeHPHSingleton_getLocationGradient
  (JNIEnv *, jobject, jint, jdoubleArray);

JNIEXPORT jint JNICALL Java_dr_inference_hawkes_NativeHPHSingleton_getInternalDimension
        (JNIEnv *, jobject, jint);
#ifdef __cplusplus
}
#endif
#endif
