#ifndef MONO_MONO_PUBLIC_H
#define MONO_MONO_PUBLIC_H

#include <stdint.h>
#include <stddef.h>

#ifdef _MSC_VER
#  ifdef MONO_STATIC_BUILD
#    define MONO_API
#  else
#    ifdef _MONO_DLL
#      define MONO_API __declspec(dllexport)
#    else
#      define MONO_API __declspec(dllimport)
#    endif
#  endif
#else
#  if defined(__GNUC__) && __GNUC__ >= 4
#    define MONO_API __attribute__ ((visibility ("default")))
#  else
#    define MONO_API
#  endif
#endif

#ifdef __cplusplus
#define MONO_BEGIN_DECLS extern "C" {
#define MONO_END_DECLS }
#else
#define MONO_BEGIN_DECLS
#define MONO_END_DECLS
#endif

/* Basic types */
typedef int mono_bool;
typedef uint8_t MonoByte;
typedef uint16_t mono_unichar2;

#if defined(_WIN32) || defined(HOST_WIN32)
typedef wchar_t gunichar2;
#else
typedef uint16_t gunichar2;
#endif

MONO_BEGIN_DECLS

/* Metadata types (opaque pointers) */
typedef struct _MonoClass MonoClass;
typedef struct _MonoDomain MonoDomain;
typedef struct _MonoField MonoField;
typedef MonoField MonoClassField;
typedef struct _MonoImage MonoImage;
typedef struct _MonoMethod MonoMethod;
typedef struct _MonoProperty MonoProperty;
typedef struct _MonoVTable MonoVTable;
typedef struct _MonoType MonoType;
typedef struct _MonoAssembly MonoAssembly;
typedef struct _MonoAssemblyName MonoAssemblyName;
typedef struct _MonoMethodDesc MonoMethodDesc;
typedef struct _MonoMethodSignature MonoMethodSignature;
typedef struct _MonoMethodHeader MonoMethodHeader;
typedef struct _MonoMethodBody MonoMethodBody;
typedef struct _MonoParameter MonoParameter;
typedef struct _MonoILCode MonoILCode;
typedef struct _MonoTableInfo MonoTableInfo;
typedef struct _MonoGenericContainer MonoGenericContainer;
typedef struct _MonoGenericInst MonoGenericInst;
typedef struct _MonoCustomAttrInfo MonoCustomAttrInfo;
typedef struct _MonoEvent MonoEvent;
typedef struct _MonoJitInfo MonoJitInfo;
typedef struct _MonoProfiler MonoProfiler;
typedef struct _MonoDebugHandle MonoDebugHandle;
typedef struct _MonoLmf MonoLmf;
typedef struct _MonoAppDomain MonoAppDomain;
typedef struct _MonoStream MonoStream;
typedef struct _MonoThreadsSync MonoThreadsSync;
typedef struct _MonoInternalThread MonoInternalThread;
typedef struct _MonoThreadStartInfo MonoThreadStartInfo;
typedef struct _MonoThreadHandle MonoThreadHandle;

typedef uint32_t mono_gchandle;

/* Heap objects - all GC heap objects use MonoObject as the base type for
   C++ compatibility with the Mono embedding API pattern where MonoObject**
   is used for exception parameters. */
typedef struct _MonoObject MonoObject;
typedef MonoObject MonoString;
typedef MonoObject MonoArray;
typedef MonoObject MonoException;
typedef MonoObject MonoDelegate;
typedef MonoObject MonoReflectionType;
typedef MonoObject MonoReflectionAssembly;
typedef MonoObject MonoReflectionMethod;
typedef MonoObject MonoThread;
typedef MonoObject MonoGCHandle;

typedef enum {
	MONO_IMAGE_OK,
	MONO_IMAGE_ERROR_ERRNO,
	MONO_IMAGE_MISSING_ASSEMBLYREF,
	MONO_IMAGE_IMAGE_INVALID
} MonoImageOpenStatus;

/* JIT init functions */
MONO_API void mono_config_parse(const char *filename);
MONO_API void mono_debug_init(int format);
MONO_API MonoDomain* mono_jit_init(const char *file);
MONO_API MonoDomain* mono_jit_init_version(const char *root_domain_name, const char *runtime_version);
MONO_API void mono_jit_cleanup(MonoDomain *domain);
MONO_API char* mono_get_runtime_build_info(void);

/* Domain functions */
MONO_API MonoDomain* mono_domain_get(void);
MONO_API MonoDomain* mono_domain_create_appdomain(char *friendly_name, char *configuration_file);
MONO_API void mono_domain_set(MonoDomain *domain, int force);
MONO_API int mono_domain_set_internal(MonoDomain *domain);
MONO_API MonoImage* mono_get_corlib(void);
MONO_API void mono_domain_unload(MonoDomain *domain);

/* Thread functions */
MONO_API MonoThread* mono_thread_current(void);
MONO_API void mono_thread_set_main(MonoThread *thread);
MONO_API MonoThread* mono_thread_attach(MonoDomain *domain);
MONO_API void mono_thread_detach(MonoThread *thread);

/* Assembly/Image functions */
MONO_API MonoAssembly* mono_domain_assembly_open(MonoDomain *domain, const char *name);
MONO_API MonoImage* mono_assembly_get_image(MonoAssembly *assembly);
MONO_API const char* mono_image_get_name(MonoImage *image);

/* Class functions */
MONO_API MonoClass* mono_class_from_name(MonoImage *image, const char *name_space, const char *name);
MONO_API const char* mono_class_get_name(MonoClass *klass);
MONO_API const char* mono_class_get_namespace(MonoClass *klass);
MONO_API MonoImage* mono_class_get_image(MonoClass *klass);
MONO_API int mono_class_is_subclass_of(MonoClass *klass, MonoClass *klassc, int check_interfaces);
MONO_API MonoVTable* mono_class_vtable(MonoDomain *domain, MonoClass *klass);
MONO_API MonoMethod* mono_class_get_method_from_name(MonoClass *klass, const char *name, int param_count);
MONO_API MonoProperty* mono_class_get_property_from_name(MonoClass *klass, const char *name);
MONO_API MonoField* mono_class_get_field_from_name(MonoClass *klass, const char *name);
MONO_API int mono_class_get_field_count(MonoClass *klass);
MONO_API int mono_class_get_method_count(MonoClass *klass);
MONO_API int mono_class_get_property_count(MonoClass *klass);
MONO_API MonoClass* mono_class_get_parent(MonoClass *klass);
MONO_API int mono_class_init(MonoClass *klass);
MONO_API MonoType* mono_class_get_type(MonoClass *klass);
MONO_API int mono_class_is_valuetype(MonoClass *klass);
MONO_API int mono_class_is_enum(MonoClass *klass);

MONO_API int mono_class_num_fields(MonoClass *klass);
MONO_API MonoField* mono_class_get_fields(MonoClass *klass, void *iter);
MONO_API MonoMethod* mono_class_get_methods(MonoClass *klass, void *iter);
MONO_API MonoProperty* mono_class_get_properties(MonoClass *klass, void *iter);

/* Method functions */
MONO_API MonoMethod* mono_method_get_next(MonoMethod *m, void *iter);
MONO_API const char* mono_method_get_name(MonoMethod *method);
MONO_API int mono_method_get_param_count(MonoMethod *method);
MONO_API MonoClass* mono_method_get_class(MonoMethod *method);
MONO_API MonoType* mono_method_get_return_type(MonoMethod *method);
MONO_API MonoClass* mono_method_signature(MonoMethod *method);

/* Field functions */
MONO_API const char* mono_field_get_name(MonoField *field);
MONO_API MonoType* mono_field_get_type(MonoField *field);
MONO_API MonoClass* mono_field_get_parent(MonoField *field);
MONO_API void mono_field_get_value(MonoObject *obj, MonoField *field, void *value);
MONO_API void mono_field_set_value(MonoObject *obj, MonoField *field, void *value);

/* Property functions */
MONO_API const char* mono_property_get_name(MonoProperty *prop);

/* Object functions */
MONO_API MonoObject* mono_object_new(MonoDomain *domain, MonoClass *klass);
MONO_API void mono_runtime_object_init(MonoObject *obj);
MONO_API void* mono_object_unbox(MonoObject *obj);
MONO_API MonoObject* mono_value_box(MonoDomain *domain, MonoClass *klass, void *val);
MONO_API MonoClass* mono_object_get_class(MonoObject *obj);
MONO_API MonoDomain* mono_object_get_domain(MonoObject *obj);
MONO_API void mono_gc_collect(int generation);

/* Runtime invoke */
MONO_API MonoObject* mono_runtime_invoke(MonoMethod *method, void *obj, void **params, MonoObject **exc);
MONO_API MonoObject* mono_runtime_invoke_array(MonoMethod *method, void *obj, MonoArray *params, MonoObject **exc);

/* String functions */
MONO_API MonoString* mono_string_new(MonoDomain *domain, const char *text);
MONO_API MonoString* mono_string_new_len(MonoDomain *domain, const char *text, int len);
MONO_API MonoString* mono_string_new_utf16(MonoDomain *domain, const gunichar2 *text, int len);
MONO_API MonoString* mono_string_empty(MonoDomain *domain);
MONO_API char* mono_string_to_utf8(MonoString *string);
MONO_API gunichar2* mono_string_to_utf16(MonoString *string);
MONO_API int mono_string_length(MonoString *string);

/* Memory */
MONO_API void mono_free(void *ptr);

/* Internal calls */
MONO_API void mono_add_internal_call(const char *name, const void *method);

/* Runtime class getters */
MONO_API MonoClass* mono_get_boolean_class(void);
MONO_API MonoClass* mono_get_char_class(void);
MONO_API MonoClass* mono_get_sbyte_class(void);
MONO_API MonoClass* mono_get_int16_class(void);
MONO_API MonoClass* mono_get_int32_class(void);
MONO_API MonoClass* mono_get_int64_class(void);
MONO_API MonoClass* mono_get_byte_class(void);
MONO_API MonoClass* mono_get_uint16_class(void);
MONO_API MonoClass* mono_get_uint32_class(void);
MONO_API MonoClass* mono_get_uint64_class(void);
MONO_API MonoClass* mono_get_single_class(void);
MONO_API MonoClass* mono_get_double_class(void);
MONO_API MonoClass* mono_get_string_class(void);
MONO_API MonoClass* mono_get_object_class(void);
MONO_API MonoClass* mono_get_array_class(void);
MONO_API MonoClass* mono_get_intptr_class(void);
MONO_API MonoClass* mono_get_exception_class(void);
MONO_API MonoClass* mono_get_void_class(void);
MONO_API MonoClass* mono_get_uintptr_class(void);

/* Debug helpers */
MONO_API char* mono_debug_method_get_name(MonoMethod *method);
MONO_API MonoMethod* mono_lookup_internal_call(const char *name);

/* Assembly loading */
MONO_API void mono_assembly_setrootdir(const char *root_dir);
MONO_API void mono_set_dirs(const char *assembly_dir, const char *config_dir);

/* GC handle functions */
MONO_API mono_gchandle mono_gchandle_new(MonoObject *obj, mono_bool pinned);
MONO_API mono_gchandle mono_gchandle_new_weakref(MonoObject *obj, mono_bool track_resurrection);
MONO_API MonoObject* mono_gchandle_get_target(mono_gchandle gchandle);
MONO_API void mono_gchandle_free(mono_gchandle gchandle);

/* Array functions */
MONO_API MonoArray* mono_array_new(MonoDomain *domain, MonoClass *eclass, uintptr_t n);
MONO_API uintptr_t mono_array_length(MonoArray *array);
MONO_API void mono_array_setref(MonoArray *array, uintptr_t index, MonoObject *value);
MONO_API void* mono_array_addr_with_size(MonoArray *array, int size, uintptr_t idx);
#define mono_array_get(arr,type,index) (*(type*)mono_array_addr_with_size((arr),sizeof(type),(index)))

/* Domain functions */
MONO_API MonoAssembly* mono_domain_get_corlib(MonoDomain *domain);

/* Exception functions */
MONO_API void mono_print_unhandled_exception(MonoObject *exc);

MONO_END_DECLS

#endif
