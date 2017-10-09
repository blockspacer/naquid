// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is autogenerated by
//     base/android/jni_generator/jni_generator.py
// For
//     org/chromium/base/JNIUtils

#ifndef org_chromium_base_JNIUtils_JNI
#define org_chromium_base_JNIUtils_JNI

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_generator/jni_generator_helper.h"

#include "base/android/jni_int_wrapper.h"

// Step 1: forward declarations.
JNI_REGISTRATION_EXPORT extern const char
    kClassPath_org_chromium_base_JNIUtils[];
const char kClassPath_org_chromium_base_JNIUtils[] =
    "org/chromium/base/JNIUtils";

// Leaking this jclass as we cannot use LazyInstance from some threads.
JNI_REGISTRATION_EXPORT base::subtle::AtomicWord
    g_org_chromium_base_JNIUtils_clazz = 0;
#ifndef org_chromium_base_JNIUtils_clazz_defined
#define org_chromium_base_JNIUtils_clazz_defined
inline jclass org_chromium_base_JNIUtils_clazz(JNIEnv* env) {
  return base::android::LazyGetClass(env, kClassPath_org_chromium_base_JNIUtils,
      &g_org_chromium_base_JNIUtils_clazz);
}
#endif

// Step 2: method stubs.

static base::subtle::AtomicWord g_org_chromium_base_JNIUtils_getClassLoader = 0;
static base::android::ScopedJavaLocalRef<jobject>
    Java_JNIUtils_getClassLoader(JNIEnv* env) {
  CHECK_CLAZZ(env, org_chromium_base_JNIUtils_clazz(env),
      org_chromium_base_JNIUtils_clazz(env), NULL);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_STATIC>(
      env, org_chromium_base_JNIUtils_clazz(env),
      "getClassLoader",
"("
")"
"Ljava/lang/Object;",
      &g_org_chromium_base_JNIUtils_getClassLoader);

  jobject ret =
      env->CallStaticObjectMethod(org_chromium_base_JNIUtils_clazz(env),
          method_id);
  jni_generator::CheckException(env);
  return base::android::ScopedJavaLocalRef<jobject>(env, ret);
}

static base::subtle::AtomicWord
    g_org_chromium_base_JNIUtils_isSelectiveJniRegistrationEnabled = 0;
static jboolean Java_JNIUtils_isSelectiveJniRegistrationEnabled(JNIEnv* env) {
  CHECK_CLAZZ(env, org_chromium_base_JNIUtils_clazz(env),
      org_chromium_base_JNIUtils_clazz(env), false);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_STATIC>(
      env, org_chromium_base_JNIUtils_clazz(env),
      "isSelectiveJniRegistrationEnabled",
"("
")"
"Z",
      &g_org_chromium_base_JNIUtils_isSelectiveJniRegistrationEnabled);

  jboolean ret =
      env->CallStaticBooleanMethod(org_chromium_base_JNIUtils_clazz(env),
          method_id);
  jni_generator::CheckException(env);
  return ret;
}

#endif  // org_chromium_base_JNIUtils_JNI
