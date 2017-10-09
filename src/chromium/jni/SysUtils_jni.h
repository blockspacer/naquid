// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is autogenerated by
//     base/android/jni_generator/jni_generator.py
// For
//     org/chromium/base/SysUtils

#ifndef org_chromium_base_SysUtils_JNI
#define org_chromium_base_SysUtils_JNI

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_generator/jni_generator_helper.h"

#include "base/android/jni_int_wrapper.h"

// Step 1: forward declarations.
JNI_REGISTRATION_EXPORT extern const char
    kClassPath_org_chromium_base_SysUtils[];
const char kClassPath_org_chromium_base_SysUtils[] =
    "org/chromium/base/SysUtils";

// Leaking this jclass as we cannot use LazyInstance from some threads.
JNI_REGISTRATION_EXPORT base::subtle::AtomicWord
    g_org_chromium_base_SysUtils_clazz = 0;
#ifndef org_chromium_base_SysUtils_clazz_defined
#define org_chromium_base_SysUtils_clazz_defined
inline jclass org_chromium_base_SysUtils_clazz(JNIEnv* env) {
  return base::android::LazyGetClass(env, kClassPath_org_chromium_base_SysUtils,
      &g_org_chromium_base_SysUtils_clazz);
}
#endif

namespace base {
namespace android {

// Step 2: method stubs.

static void LogPageFaultCountToTracing(JNIEnv* env, const
    base::android::JavaParamRef<jclass>& jcaller);

JNI_GENERATOR_EXPORT void
    Java_org_chromium_base_SysUtils_nativeLogPageFaultCountToTracing(JNIEnv*
    env, jclass jcaller) {
  return LogPageFaultCountToTracing(env,
      base::android::JavaParamRef<jclass>(env, jcaller));
}

static base::subtle::AtomicWord g_org_chromium_base_SysUtils_isLowEndDevice = 0;
static jboolean Java_SysUtils_isLowEndDevice(JNIEnv* env) {
  CHECK_CLAZZ(env, org_chromium_base_SysUtils_clazz(env),
      org_chromium_base_SysUtils_clazz(env), false);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_STATIC>(
      env, org_chromium_base_SysUtils_clazz(env),
      "isLowEndDevice",
"("
")"
"Z",
      &g_org_chromium_base_SysUtils_isLowEndDevice);

  jboolean ret =
      env->CallStaticBooleanMethod(org_chromium_base_SysUtils_clazz(env),
          method_id);
  jni_generator::CheckException(env);
  return ret;
}

static base::subtle::AtomicWord
    g_org_chromium_base_SysUtils_isCurrentlyLowMemory = 0;
static jboolean Java_SysUtils_isCurrentlyLowMemory(JNIEnv* env) {
  CHECK_CLAZZ(env, org_chromium_base_SysUtils_clazz(env),
      org_chromium_base_SysUtils_clazz(env), false);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_STATIC>(
      env, org_chromium_base_SysUtils_clazz(env),
      "isCurrentlyLowMemory",
"("
")"
"Z",
      &g_org_chromium_base_SysUtils_isCurrentlyLowMemory);

  jboolean ret =
      env->CallStaticBooleanMethod(org_chromium_base_SysUtils_clazz(env),
          method_id);
  jni_generator::CheckException(env);
  return ret;
}

}  // namespace android
}  // namespace base

#endif  // org_chromium_base_SysUtils_JNI
