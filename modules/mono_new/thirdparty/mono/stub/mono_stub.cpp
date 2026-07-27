#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" {

void* mono_jit_init(const char* file) { return (void*)0x1; }
void* mono_jit_init_version(const char* root_domain_name, const char* runtime_version) { return (void*)0x1; }
void mono_jit_cleanup(void* domain) {}
const char* mono_runtime_get_version(void) { return "0.0.0 (stub)"; }
const char* mono_get_runtime_build_info(void) { return "0.0.0 (stub)"; }

void mono_config_parse(const char* filename) {}
void mono_debug_init(int format) {}
void mono_set_dirs(const char* assembly_dir, const char* config_dir) {}
void mono_assembly_setrootdir(const char* root_dir) {}
void mono_config_set_dirs(const char* a, const char* b) {}
const char* mono_get_config_dir(void) { return ""; }
const char* mono_get_assemblies_path(void) { return ""; }

void* mono_domain_get(void) { return (void*)0x1; }
void* mono_domain_create_appdomain(char* friendly_name, char* config) { return (void*)0x2; }
void mono_domain_set(void* domain, int force) {}
int mono_domain_set_internal(void* domain) { return 1; }
void* mono_domain_get_image(void* domain) { return (void*)0x3; }
void mono_domain_unload(void* domain) {}
int mono_domain_is_unloading(void* domain) { return 0; }
void* mono_domain_assembly_open(void* domain, const char* name) { return (void*)0x4; }
void* mono_domain_try_type_resolve(void* domain, char* name, void* image) { return nullptr; }
void* mono_domain_get_corlib(void* domain) { return (void*)0x5; }

void* mono_assembly_open(const char* filename, int* status) { return nullptr; }
void* mono_assembly_get_image(void* assembly) { return (void*)0x3; }
const char* mono_assembly_name_get_name(void* aname) { return ""; }
void mono_assembly_close(void* assembly) {}

const char* mono_image_get_name(void* image) { return "stub"; }
const char* mono_image_get_filename(void* image) { return ""; }
int mono_image_get_table_rows(void* image, int table) { return 0; }
int mono_image_get_entry_point(void* image) { return 0; }
int mono_image_close(void* image) { return 0; }
int mono_image_has_entry_point(void* image) { return 0; }
const char* mono_image_get_version(void* image) { return "0.0"; }
void* mono_get_corlib(void) { return (void*)0x3; }

void* mono_class_from_name(void* image, const char* name_space, const char* name) { return nullptr; }
const char* mono_class_get_name(void* klass) { return ""; }
const char* mono_class_get_namespace(void* klass) { return ""; }
void* mono_class_get_parent(void* klass) { return nullptr; }
int mono_class_is_subclass_of(void* klass, void* parent_class, int check_interfaces) { return 0; }
void* mono_class_get_field_from_name(void* klass, const char* name) { return nullptr; }
void* mono_class_get_method_from_name(void* klass, const char* name, int param_count) { return nullptr; }
void* mono_class_get_property_from_name(void* klass, const char* name) { return nullptr; }
int mono_class_get_field_count(void* klass) { return 0; }
void* mono_class_get_fields(void* klass, void* iter) { return nullptr; }
int mono_class_init(void* klass) { return 1; }
void* mono_class_vtable(void* domain, void* klass) { return nullptr; }
void mono_class_set_vtable(void* domain, void* klass, void* vtable) {}
int mono_class_is_valuetype(void* klass) { return 0; }
int mono_class_is_enum(void* klass) { return 0; }
uint32_t mono_type_get_type(void* type) { return 0; }
void* mono_class_get_type(void* klass) { return nullptr; }
void* mono_type_get_class(void* type) { return nullptr; }

void* mono_method_signature(void* method) { return nullptr; }
const char* mono_method_get_name(void* method) { return ""; }
void* mono_method_get_class(void* method) { return nullptr; }
void* mono_method_get_return_type(void* method) { return nullptr; }
int mono_method_get_param_count(void* method) { return 0; }
void* mono_compile_method(void* method) { return nullptr; }

const char* mono_field_get_name(void* field) { return ""; }
void* mono_field_get_type(void* field) { return nullptr; }
void mono_field_get_value(void* obj, void* field, void* value) {}
void mono_field_set_value(void* obj, void* field, void* value) {}
void mono_field_static_get_value(void* domain, void* field, void* value) {}
void mono_field_static_set_value(void* domain, void* field, void* value) {}

const char* mono_property_get_name(void* prop) { return ""; }
void* mono_property_get_get_method(void* prop) { return nullptr; }
void* mono_property_get_set_method(void* prop) { return nullptr; }

void* mono_object_new(void* domain, void* klass) { return nullptr; }
void* mono_object_new_alloc_specific(void* vtable) { return nullptr; }
int mono_object_isinst(void* obj, void* klass) { return 0; }
void* mono_object_get_class(void* obj) { return nullptr; }
void mono_runtime_object_init(void* obj) {}
void mono_gc_wbarrier_set_field(void* obj, void* field_ptr, void* value) {}

void* mono_array_new(void* domain, void* klass, uintptr_t n) { return nullptr; }
void* mono_array_get(void* array, void* klass, int index) { return nullptr; }
void* mono_array_addr_with_size(void* array, int size, int index) { return nullptr; }
int mono_array_length(void* array) { return 0; }
void* mono_array_element_size(void* array) { return nullptr; }
void* mono_array_class_get(void* array, int rank) { return nullptr; }

void* mono_string_new(void* domain, const char* text) { return nullptr; }
void* mono_string_new_len(void* domain, const char* text, int len) { return nullptr; }
void* mono_string_new_utf16(void* domain, const uint16_t* text, int len) { return nullptr; }
const char* mono_string_to_utf8(void* string) { return ""; }
const uint16_t* mono_string_to_utf16(void* string) { return nullptr; }
int mono_string_length(void* string) { return 0; }
wchar_t* mono_string_to_utf16_internal(void* string) { return nullptr; }

uint32_t mono_gchandle_new(void* obj, int pinned) { return 0; }
uint32_t mono_gchandle_new_weakref(void* obj, int track_resurrection) { return 0; }
void mono_gchandle_free(uint32_t gchandle) {}
void* mono_gchandle_get_target(uint32_t gchandle) { return nullptr; }

void mono_add_internal_call(const char* name, const void* method) {}

void* mono_thread_current(void) { return nullptr; }
void mono_thread_attach(void* domain) {}
void mono_thread_detach(void* thread) {}

void* mono_delegate_to_ftnptr(void* delegate) { return nullptr; }
void* mono_delegate_get_method(void* delegate) { return nullptr; }
void* mono_delegate_get_target(void* delegate) { return nullptr; }

void mono_raise_exception(void* ex) {}

int32_t mono_runtime_invoke(void* method, void* obj, void** params, void** exc) { return 0; }
int32_t mono_runtime_invoke_array(void* method, void* obj, void* args, void** exc) { return 0; }

void mono_trace_set_log_handler(void* callback, void* user_data) {}
void mono_trace_set_print_handler(void(*func)(const char*)) {}
void mono_trace_set_printerr_handler(void(*func)(const char*)) {}
void mono_print_unhandled_exception(void* ex) {}
void mono_log_write_logfile(const char* domain, int level, const char* msg, ...) {}

const char* mono_pmip(void* ip) { return nullptr; }
const char* mono_stack_walk_no_il(void* method) { return nullptr; }
void mono_stack_walk(void* start_ctx, ...) {}

void mono_dllmap_insert(void* assembly, const char* dll, const char* func, const char* tdll, const char* tfunc) {}

void mono_free(void* ptr) {}

void mono_jit_parse_options(int argc, char** argv) {}
int mono_jit_set_trace_options(const char* options) { return 0; }
void mono_security_set_core_clr_platform_callback(void* cb) {}

const char* mono_environment_exitcode_get(void) { return nullptr; }
void mono_environment_exitcode_set(int code) {}

int mono_class_is_assignable_from(void* klass, void* ok) { return 0; }
void* mono_method_get_object(void* image, void* klass, void* method) { return nullptr; }
int mono_runtime_object_init_exception(void* obj, void** exc) { return 0; }
uint32_t mono_object_get_size(void* obj) { return 0; }
int mono_string_is_white_space(int c) { return 0; }
int mono_class_num_fields(void* klass) { return 0; }
void* mono_field_full_name(void* field) { return nullptr; }
const char* mono_type_get_name(void* type) { return ""; }

void* mono_class_get_image(void* klass) { return (void*)0x3; }
void* mono_object_unbox(void* obj) { return nullptr; }
void* mono_value_box(void* domain, void* klass, void* val) { return nullptr; }
void* mono_get_boolean_class(void) { return nullptr; }
void* mono_get_int32_class(void) { return nullptr; }
void* mono_get_int64_class(void) { return nullptr; }
void* mono_get_single_class(void) { return nullptr; }
void* mono_get_double_class(void) { return nullptr; }
void* mono_get_string_class(void) { return nullptr; }
void* mono_get_object_class(void) { return nullptr; }
void* mono_get_byte_class(void) { return nullptr; }
void mono_array_setref(void* array, int index, void* value) {}

// P2 修复: 补全 stub 缺失的 Mono API，使 mono_new_stub=yes 能完整链接验证。
void mono_gc_wbarrier_set_arrayref(void* arr, void* slot_ptr, void* value) {}
void* mono_field_get_value_object(void* domain, void* field, void* obj) { return nullptr; }
void mono_install_unhandled_exception_hook(void* hook, void* user_data) {}
int mono_is_debugger_attached(void) { return 0; }
const void* mono_image_get_table_info(void* image, int table_id) { return nullptr; }
int mono_table_info_get_rows(const void* table) { return 0; }
void* mono_class_get(void* image, uint32_t type_token) { return nullptr; }
void* mono_property_get_value(void* prop, void* obj, void** params, void** exc) { return nullptr; }
char* mono_object_to_string(void* obj, void** exc) { return (char*)""; }

}
