#include <cstddef>
#include <cstdint>
#include <cstring>

#ifdef _WIN32
#define MONO_STUB_EXPORT __declspec(dllexport)
#else
#define MONO_STUB_EXPORT __attribute__((visibility("default")))
#endif

extern "C" {

// JIT init
MONO_STUB_EXPORT void* mono_jit_init(const char* file) { return (void*)0x1; }
MONO_STUB_EXPORT void* mono_jit_init_version(const char* root_domain_name, const char* runtime_version) { return (void*)0x1; }
MONO_STUB_EXPORT void mono_jit_cleanup(void* domain) {}
MONO_STUB_EXPORT const char* mono_runtime_get_version(void) { return "0.0.0 (stub)"; }
MONO_STUB_EXPORT const char* mono_get_runtime_build_info(void) { return "0.0.0 (stub)"; }

// Config
MONO_STUB_EXPORT void mono_config_parse(const char* filename) {}
MONO_STUB_EXPORT void mono_debug_init(int format) {}
MONO_STUB_EXPORT void mono_set_dirs(const char* assembly_dir, const char* config_dir) {}
MONO_STUB_EXPORT void mono_assembly_setrootdir(const char* root_dir) {}
MONO_STUB_EXPORT void mono_config_set_dirs(const char* a, const char* b) {}
MONO_STUB_EXPORT const char* mono_get_config_dir(void) { return ""; }
MONO_STUB_EXPORT const char* mono_get_assemblies_path(void) { return ""; }

// Domain
MONO_STUB_EXPORT void* mono_domain_get(void) { return (void*)0x1; }
MONO_STUB_EXPORT void* mono_domain_create_appdomain(char* friendly_name, char* config) { return (void*)0x2; }
MONO_STUB_EXPORT int mono_domain_set(void* domain, int force) { return 1; }
MONO_STUB_EXPORT int mono_domain_set_internal(void* domain) { return 1; }
MONO_STUB_EXPORT void* mono_domain_get_image(void* domain) { return (void*)0x3; }
MONO_STUB_EXPORT void mono_domain_unload(void* domain) {}
MONO_STUB_EXPORT int mono_domain_is_unloading(void* domain) { return 0; }
MONO_STUB_EXPORT void* mono_domain_assembly_open(void* domain, const char* name) { return (void*)0x4; }
MONO_STUB_EXPORT void* mono_domain_try_type_resolve(void* domain, char* name, void* image) { return nullptr; }

// Assembly
MONO_STUB_EXPORT void* mono_assembly_open(const char* filename, int* status) { return nullptr; }
MONO_STUB_EXPORT void* mono_assembly_get_image(void* assembly) { return (void*)0x3; }
MONO_STUB_EXPORT const char* mono_assembly_name_get_name(void* aname) { return ""; }
MONO_STUB_EXPORT void mono_assembly_close(void* assembly) {}

// Image
MONO_STUB_EXPORT const char* mono_image_get_name(void* image) { return "stub"; }
MONO_STUB_EXPORT const char* mono_image_get_filename(void* image) { return ""; }
MONO_STUB_EXPORT int mono_image_get_table_rows(void* image, int table) { return 0; }
MONO_STUB_EXPORT int mono_image_get_entry_point(void* image) { return 0; }
MONO_STUB_EXPORT int mono_image_close(void* image) { return 0; }
MONO_STUB_EXPORT int mono_image_has_entry_point(void* image) { return 0; }
MONO_STUB_EXPORT const char* mono_image_get_version(void* image) { return "0.0"; }

// Class
MONO_STUB_EXPORT void* mono_class_from_name(void* image, const char* name_space, const char* name) { return nullptr; }
MONO_STUB_EXPORT void* mono_class_from_mono_type(void* type) { return nullptr; }
MONO_STUB_EXPORT void* mono_class_get(void* image, uint32_t token) { return nullptr; }
MONO_STUB_EXPORT void* mono_class_get_parent(void* klass) { return nullptr; }
MONO_STUB_EXPORT void* mono_class_get_element_class(void* klass) { return nullptr; }
MONO_STUB_EXPORT void* mono_class_get_image(void* klass) { return (void*)0x3; }
MONO_STUB_EXPORT const char* mono_class_get_name(void* klass) { return ""; }
MONO_STUB_EXPORT const char* mono_class_get_namespace(void* klass) { return ""; }
MONO_STUB_EXPORT void* mono_class_get_type(void* klass) { return nullptr; }
MONO_STUB_EXPORT uint32_t mono_class_get_type_token(void* klass) { return 0; }
MONO_STUB_EXPORT int mono_class_get_rank(void* klass) { return 0; }
MONO_STUB_EXPORT int mono_class_is_valuetype(void* klass) { return 0; }
MONO_STUB_EXPORT int mono_class_is_enum(void* klass) { return 0; }
MONO_STUB_EXPORT int mono_class_is_abstract(void* klass) { return 0; }
MONO_STUB_EXPORT int mono_class_is_interface(void* klass) { return 0; }
MONO_STUB_EXPORT int mono_class_is_sealed(void* klass) { return 0; }
MONO_STUB_EXPORT int mono_class_init(void* klass) { return 0; }
MONO_STUB_EXPORT int mono_class_is_subclass_of(void* klass, void* klassc, int check) { return 0; }
MONO_STUB_EXPORT int mono_class_num_fields(void* klass) { return 0; }
MONO_STUB_EXPORT int mono_class_get_method_count(void* klass) { return 0; }
MONO_STUB_EXPORT int mono_class_get_property_count(void* klass) { return 0; }
MONO_STUB_EXPORT int mono_class_instance_size(void* klass) { return 16; }
MONO_STUB_EXPORT void* mono_class_get_methods(void* klass, void** iter) { *iter = nullptr; return nullptr; }
MONO_STUB_EXPORT void* mono_class_get_method_from_name(void* klass, const char* name, int pc) { return nullptr; }
MONO_STUB_EXPORT void* mono_class_get_property_from_name(void* klass, const char* name) { return nullptr; }
MONO_STUB_EXPORT void* mono_class_get_fields(void* klass, void** iter) { *iter = nullptr; return nullptr; }
MONO_STUB_EXPORT void* mono_class_get_field_from_name(void* klass, const char* name) { return nullptr; }
MONO_STUB_EXPORT int mono_class_get_flags(void* klass) { return 0; }
MONO_STUB_EXPORT void* mono_class_vtable(void* domain, void* klass) { return (void*)0x5; }

// Field
MONO_STUB_EXPORT const char* mono_field_get_name(void* field) { return ""; }
MONO_STUB_EXPORT void* mono_field_get_type(void* field) { return nullptr; }
MONO_STUB_EXPORT void* mono_field_get_parent(void* field) { return nullptr; }
MONO_STUB_EXPORT void mono_field_get_value(void* obj, void* field, void* value) {}
MONO_STUB_EXPORT void mono_field_set_value(void* obj, void* field, void* value) {}

// Method
MONO_STUB_EXPORT const char* mono_method_get_name(void* method) { return ""; }
MONO_STUB_EXPORT uint32_t mono_method_get_token(void* method) { return 0; }
MONO_STUB_EXPORT void* mono_method_get_class(void* method) { return nullptr; }
MONO_STUB_EXPORT void* mono_method_get_return_type(void* method) { return nullptr; }
MONO_STUB_EXPORT int mono_method_get_param_count(void* method) { return 0; }
MONO_STUB_EXPORT void* mono_method_signature(void* method) { return nullptr; }
MONO_STUB_EXPORT int mono_method_is_static(void* method) { return 0; }
MONO_STUB_EXPORT uint32_t mono_method_get_flags(void* method) { return 0; }
MONO_STUB_EXPORT void* mono_method_get_header(void* method) { return nullptr; }

// Object
MONO_STUB_EXPORT void* mono_object_new(void* domain, void* klass) { return nullptr; }
MONO_STUB_EXPORT void* mono_object_new_specific(void* vtable) { return nullptr; }
MONO_STUB_EXPORT void mono_runtime_object_init(void* obj) {}
MONO_STUB_EXPORT void* mono_object_unbox(void* obj) { return nullptr; }
MONO_STUB_EXPORT void* mono_value_box(void* domain, void* klass, void* val) { return nullptr; }
MONO_STUB_EXPORT void* mono_object_get_class(void* obj) { return nullptr; }
MONO_STUB_EXPORT void* mono_object_get_domain(void* obj) { return (void*)0x1; }
MONO_STUB_EXPORT uint32_t mono_object_get_size(void* obj) { return 16; }
MONO_STUB_EXPORT void mono_gc_collect(int gen) {}
MONO_STUB_EXPORT void* mono_runtime_invoke(void* method, void* obj, void** params, void** exc) { return nullptr; }
MONO_STUB_EXPORT void* mono_runtime_invoke_array(void* method, void* obj, void* arr, void** exc) { return nullptr; }

// String
MONO_STUB_EXPORT void* mono_string_new(void* domain, const char* text) { return nullptr; }
MONO_STUB_EXPORT void* mono_string_new_len(void* domain, const char* text, int len) { return nullptr; }
MONO_STUB_EXPORT void* mono_string_empty(void* domain) { return nullptr; }
MONO_STUB_EXPORT char* mono_string_to_utf8(void* str) { return nullptr; }
MONO_STUB_EXPORT int mono_string_length(void* str) { return 0; }

// Array
MONO_STUB_EXPORT void* mono_array_new(void* domain, void* eclass, uintptr_t n) { return nullptr; }
MONO_STUB_EXPORT int mono_array_length(void* arr) { return 0; }
MONO_STUB_EXPORT char* mono_array_addr_with_size(void* arr, int size, uintptr_t idx) { return nullptr; }

// Internal calls
MONO_STUB_EXPORT void mono_add_internal_call(const char* name, const void* method) {}
MONO_STUB_EXPORT void mono_free(void* ptr) {}

// Get corlib classes
MONO_STUB_EXPORT void* mono_get_corlib(void) { return (void*)0x4; }
MONO_STUB_EXPORT void* mono_get_boolean_class(void) { return (void*)0x10; }
MONO_STUB_EXPORT void* mono_get_char_class(void) { return (void*)0x11; }
MONO_STUB_EXPORT void* mono_get_sbyte_class(void) { return (void*)0x12; }
MONO_STUB_EXPORT void* mono_get_int16_class(void) { return (void*)0x13; }
MONO_STUB_EXPORT void* mono_get_int32_class(void) { return (void*)0x14; }
MONO_STUB_EXPORT void* mono_get_int64_class(void) { return (void*)0x15; }
MONO_STUB_EXPORT void* mono_get_byte_class(void) { return (void*)0x16; }
MONO_STUB_EXPORT void* mono_get_uint16_class(void) { return (void*)0x17; }
MONO_STUB_EXPORT void* mono_get_uint32_class(void) { return (void*)0x18; }
MONO_STUB_EXPORT void* mono_get_uint64_class(void) { return (void*)0x19; }
MONO_STUB_EXPORT void* mono_get_single_class(void) { return (void*)0x1a; }
MONO_STUB_EXPORT void* mono_get_double_class(void) { return (void*)0x1b; }
MONO_STUB_EXPORT void* mono_get_string_class(void) { return (void*)0x1c; }
MONO_STUB_EXPORT void* mono_get_object_class(void) { return (void*)0x1d; }
MONO_STUB_EXPORT void* mono_get_array_class(void) { return (void*)0x1e; }
MONO_STUB_EXPORT void* mono_get_intptr_class(void) { return (void*)0x1f; }
MONO_STUB_EXPORT void* mono_get_exception_class(void) { return (void*)0x20; }
MONO_STUB_EXPORT void* mono_get_void_class(void) { return (void*)0x21; }
MONO_STUB_EXPORT void* mono_get_uintptr_class(void) { return (void*)0x22; }

// Threads
MONO_STUB_EXPORT void* mono_thread_current(void) { return (void*)0x30; }
MONO_STUB_EXPORT void mono_thread_set_main(void* thread) {}
MONO_STUB_EXPORT void* mono_thread_attach(void* domain) { return (void*)0x30; }
MONO_STUB_EXPORT void mono_thread_detach(void* thread) {}

// Exceptions
MONO_STUB_EXPORT void* mono_get_exception_argument_null(const char* arg) { return nullptr; }
MONO_STUB_EXPORT void* mono_get_exception_invalid_cast(void) { return nullptr; }

// Signature
MONO_STUB_EXPORT int mono_signature_get_param_count(void* sig) { return 0; }
MONO_STUB_EXPORT void* mono_signature_get_return_type(void* sig) { return nullptr; }

}
