set(common_src
	./src/chromium/base/at_exit.cc
	./src/chromium/base/base64.cc
	./src/chromium/base/callback_internal.cc
	./src/chromium/base/debug/alias.cc
	./src/chromium/base/debug/debugger.cc
	./src/chromium/base/debug/stack_trace.cc
	./src/chromium/base/lazy_instance.cc
	./src/chromium/base/location.cc
	./src/chromium/base/logging.cc
	./src/chromium/base/memory/singleton.cc
	./src/chromium/base/pickle.cc
	./src/chromium/base/rand_util.cc
	./src/chromium/base/strings/string_number_conversions.cc
	./src/chromium/base/strings/string_piece.cc
	./src/chromium/base/strings/string_split.cc
	./src/chromium/base/strings/string_util.cc
	./src/chromium/base/strings/string_util_constants.cc
	./src/chromium/base/strings/string16.cc
	./src/chromium/base/strings/stringprintf.cc
	./src/chromium/base/strings/utf_offset_string_conversions.cc
	./src/chromium/base/strings/utf_string_conversions.cc
	./src/chromium/base/strings/utf_string_conversion_utils.cc
	./src/chromium/base/synchronization/lock.cc
	./src/chromium/base/time/time.cc
	./src/chromium/base/third_party/nspr/prtime.cc
	./src/chromium/base/third_party/dmg_fp/dtoa_wrapper.cc
	./src/chromium/base/third_party/dmg_fp/g_fmt.cc
	./src/chromium/base/third_party/icu/icu_utf.cc
	./src/chromium/base/threading/thread_id_name_manager.cc
	./src/chromium/base/threading/thread_local_storage.cc
	./src/chromium/base/threading/thread_restrictions.cc
	./src/chromium/base/trace_event/memory_usage_estimator.cc

	./src/chromium/crypto/hkdf.cc
	./src/chromium/crypto/hmac.cc
	./src/chromium/crypto/openssl_util.cc
	./src/chromium/crypto/random.cc
	./src/chromium/crypto/secure_util.cc
	./src/chromium/crypto/symmetric_key.cc

	./src/chromium/third_party/modp_b64/modp_b64.cc

	./src/chromium/url/url_constants.cc
	./src/chromium/url/url_canon_etc.cc
	./src/chromium/url/url_canon_filesystemurl.cc
	./src/chromium/url/url_canon_fileurl.cc
	./src/chromium/url/url_canon_host.cc
	./src/chromium/url/url_canon_internal.cc
	./src/chromium/url/url_canon_ip.cc
	#./src/chromium/url/url_canon_icu.cc
	./src/chromium/url/url_canon_mailtourl.cc
	./src/chromium/url/url_canon_path.cc
	./src/chromium/url/url_canon_pathurl.cc
	./src/chromium/url/url_canon_query.cc
	./src/chromium/url/url_canon_relative.cc
	./src/chromium/url/url_canon_stdstring.cc
	./src/chromium/url/url_canon_stdurl.cc
	./src/chromium/url/url_util.cc
	./src/chromium/url/url_parse_file.cc
	./src/chromium/url/gurl.cc
	./src/chromium/url/third_party/mozilla/url_parse.cc
)
