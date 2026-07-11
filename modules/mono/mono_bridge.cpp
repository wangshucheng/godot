#include "mono_bridge.h"
#include "mono_variant.h"
#include "mono_gc_bridge.h"
#include "core/object/ref_counted.h"
#include "core/error/error_macros.h"
#include "scene/main/node.h"
#include "scene/resources/packed_scene.h"
#include "scene/main/scene_tree.h"
#include "core/input/input_event.h"
#include <mono/metadata/image.h>
#include <mono/metadata/blob.h>
#include <cstdio>
#include <cstdint>
#include <cstring>

namespace mono_bridge {

static MonoDomain *domain = nullptr;
static MonoClass *godot_object_class = nullptr;
static MonoClass *godot_node_class = nullptr;
static MonoClass *godot_resource_class = nullptr;
static MonoClass *godot_packed_scene_class = nullptr;
static MonoClass *godot_input_event_class = nullptr;
static MonoClass *godot_scene_tree_class = nullptr;
static MonoClass *system_intptr_class = nullptr;

void init(MonoDomain *p_domain) {
	domain = p_domain;

	MonoImage *corlib = mono_get_corlib();
	system_intptr_class = mono_class_from_name(corlib, "System", "IntPtr");

	printf("[Mono] Bridge initialized.\n");
	fflush(stdout);
}

void shutdown() {
	domain = nullptr;
	godot_object_class = nullptr;
	godot_node_class = nullptr;
	godot_resource_class = nullptr;
	godot_packed_scene_class = nullptr;
	godot_input_event_class = nullptr;
	godot_scene_tree_class = nullptr;
	system_intptr_class = nullptr;
	printf("[Mono] Bridge shut down.\n");
	fflush(stdout);
}

Object *unmanaged_get_from_ptr(intptr_t p_native_ptr) {
	if (p_native_ptr == 0) return nullptr;
	return (Object *)p_native_ptr;
}

Object *unmanaged_get(MonoObject *p_cs_obj) {
	if (!p_cs_obj) return nullptr;
	return mono_gc_bridge::get_native(p_cs_obj);
}

void unregister_object(Object *p_obj) {
	if (!p_obj) return;
	mono_gc_bridge::notify_native_destroyed(p_obj);
}

static void set_native_ptr_field(MonoObject *p_cs_obj, intptr_t p_ptr_val) {
	if (!p_cs_obj || !domain) return;

	MonoClass *klass = mono_object_get_class(p_cs_obj);
	MonoClassField *field = nullptr;
	for (MonoClass *k = klass; k; k = mono_class_get_parent(k)) {
		field = mono_class_get_field_from_name(k, "NativePtr");
		if (field) break;
	}
	if (!field) return;

	mono_field_set_value(p_cs_obj, field, &p_ptr_val);
}

void tie_native_ptr(MonoObject *p_cs_obj, Object *p_obj) {
	if (!p_cs_obj || !p_obj) return;

	mono_gc_bridge::tie_managed_to_native(p_cs_obj, p_obj, true);
	set_native_ptr_field(p_cs_obj, (intptr_t)p_obj);
}

MonoObject *managed_get_or_create(Object *p_obj, MonoClass *p_class) {
	if (!p_obj) return nullptr;

	MonoObject *existing = mono_gc_bridge::get_managed(p_obj);
	if (existing) return existing;

	if (!p_class) p_class = godot_object_class;
	if (!p_class) {
		intptr_t ptr_val = (intptr_t)p_obj;
		return mono_value_box(domain, system_intptr_class, &ptr_val);
	}

	MonoObject *cs_obj = mono_object_new(domain, p_class);
	if (!cs_obj) return nullptr;

	void *args[1];
	intptr_t ptr_val = (intptr_t)p_obj;
	MonoObject *box = mono_value_box(domain, system_intptr_class, &ptr_val);
	args[0] = box;

	MonoMethod *ctor = nullptr;
	void *iter = nullptr;
	while (MonoMethod *m = mono_class_get_methods(p_class, &iter)) {
		const char *name = mono_method_get_name(m);
		if (strcmp(name, ".ctor") == 0) {
			MonoMethodSignature *sig = mono_method_signature(m);
			int param_count = mono_signature_get_param_count(sig);
			if (param_count >= 1) {
				void *arg_iter = nullptr;
				MonoType *param_type = mono_signature_get_params(sig, &arg_iter);
				if (param_type && mono_type_get_class(param_type) == system_intptr_class) {
					ctor = m;
					break;
				}
			}
		}
	}

	MonoObject *exc = nullptr;
	if (ctor) {
		mono_runtime_invoke(ctor, cs_obj, args, &exc);
	} else {
		mono_runtime_object_init(cs_obj);
	}
	if (exc) {
		char *msg = mono_string_to_utf8(mono_object_to_string(exc, nullptr));
		printf("[Mono] Exception in Godot.Object ctor: %s\n", msg ? msg : "unknown");
		mono_free(msg);
	}

	return cs_obj;
}

MonoDomain *get_domain() { return domain; }
MonoClass *get_godot_object_class() { return godot_object_class; }
MonoClass *get_godot_node_class() { return godot_node_class; }
MonoClass *get_godot_resource_class() { return godot_resource_class; }
MonoClass *get_godot_packed_scene_class() { return godot_packed_scene_class; }
MonoClass *get_godot_input_event_class() { return godot_input_event_class; }
MonoClass *get_godot_scene_tree_class() { return godot_scene_tree_class; }

MonoClass *get_mono_class_for_object(Object *p_obj) {
	if (!p_obj) return nullptr;
	if (godot_scene_tree_class && p_obj->is_class("SceneTree")) return godot_scene_tree_class;
	if (godot_packed_scene_class && p_obj->is_class("PackedScene")) return godot_packed_scene_class;
	if (godot_input_event_class && p_obj->is_class("InputEvent")) return godot_input_event_class;
	if (godot_resource_class && p_obj->is_class("Resource")) return godot_resource_class;
	if (godot_node_class && p_obj->is_class("Node")) return godot_node_class;
	return godot_object_class;
}

void cache_godot_classes(MonoImage *p_godot_image) {
	printf("[Mono] cache_godot_classes: image=%p\n", p_godot_image);
	fflush(stdout);
	if (!p_godot_image) {
		printf("[Mono] WARNING: GodotSharp image is NULL!\n");
		fflush(stdout);
		return;
	}

	// Try loading classes
	godot_object_class = mono_class_from_name(p_godot_image, "Godot", "Object");
	godot_node_class = mono_class_from_name(p_godot_image, "Godot", "Node");
	godot_resource_class = mono_class_from_name(p_godot_image, "Godot", "Resource");
	godot_packed_scene_class = mono_class_from_name(p_godot_image, "Godot", "PackedScene");
	godot_input_event_class = mono_class_from_name(p_godot_image, "Godot", "InputEvent");
	godot_scene_tree_class = mono_class_from_name(p_godot_image, "Godot", "SceneTree");

	printf("[Mono] Class lookup: Object=%p Node=%p Resource=%p PackedScene=%p InputEvent=%p SceneTree=%p\n",
		godot_object_class, godot_node_class, godot_resource_class,
		godot_packed_scene_class, godot_input_event_class, godot_scene_tree_class);
	fflush(stdout);

	// If mono_class_from_name fails for Object, try alternative methods
	if (!godot_object_class) {
		printf("[Mono] Object class is NULL. Trying alternative methods...\n");
		fflush(stdout);

		// Method 1: Iterate all TypeDefs in the image to find "Object" in namespace "Godot"
		int num_typedefs = mono_image_get_table_rows(p_godot_image, MONO_TABLE_TYPEDEF);
		printf("[Mono] Image has %d TypeDef rows. Scanning for 'Object'...\n", num_typedefs);
		fflush(stdout);

		for (int i = 1; i <= num_typedefs; i++) {
			uint32_t token = (MONO_TABLE_TYPEDEF << 24) | i;
			MonoClass *klass = mono_class_get(p_godot_image, token);
			if (klass) {
				const char *kname = mono_class_get_name(klass);
				const char *kns = mono_class_get_namespace(klass);
				if (kname && strcmp(kname, "Object") == 0 && kns && strcmp(kns, "Godot") == 0) {
					godot_object_class = klass;
					printf("[Mono] Found Godot.Object via TypeDef scan: row=%d token=0x%08x class=%p\n",
						i, token, klass);
					fflush(stdout);
					break;
				}
			}
		}

		// Method 2: Try via Node's parent
		if (!godot_object_class && godot_node_class) {
			printf("[Mono] Trying via Node's parent...\n");
			fflush(stdout);

			MonoClass *parent = mono_class_get_parent(godot_node_class);
			if (parent) {
				const char *pname = mono_class_get_name(parent);
				const char *pns = mono_class_get_namespace(parent);
				printf("[Mono] Node's parent: name='%s' ns='%s' class=%p\n",
					pname ? pname : "(null)", pns ? pns : "(null)", parent);
				fflush(stdout);

				if (pname && strcmp(pname, "Object") == 0) {
					godot_object_class = parent;
					printf("[Mono] Got Object via Node parent: %p\n", godot_object_class);
					fflush(stdout);
				}
			}
		}
	}

	// Inspect and initialize the Object class
	if (godot_object_class) {
		int num_fields = mono_class_num_fields(godot_object_class);
		int num_methods = mono_class_num_methods(godot_object_class);
		printf("[Mono] Object class metadata: fields=%d methods=%d\n", num_fields, num_methods);
		fflush(stdout);

		// Check the class's parent
		MonoClass *obj_parent = mono_class_get_parent(godot_object_class);
		if (obj_parent) {
			const char *pn = mono_class_get_name(obj_parent);
			const char *pns = mono_class_get_namespace(obj_parent);
			printf("[Mono] Object's parent: '%s' (ns='%s')\n", pn ? pn : "?", pns ? pns : "?");
			fflush(stdout);
		} else {
			printf("[Mono] Object has NO parent (root class).\n");
			fflush(stdout);
		}

		printf("[Mono] Calling mono_class_init on Object class...\n");
		fflush(stdout);
		mono_bool init_ok = mono_class_init(godot_object_class);
		printf("[Mono] mono_class_init(Object) returned: %d\n", (int)init_ok);
		fflush(stdout);

		if (!init_ok) {
			printf("[Mono] Object init FAILED.\n");
			fflush(stdout);

			// Try initializing the parent class first
			if (obj_parent) {
				printf("[Mono] Trying to init Object's parent first...\n");
				fflush(stdout);
				mono_bool pok = mono_class_init(obj_parent);
				printf("[Mono] Parent init result: %d\n", (int)pok);
				fflush(stdout);

				if (pok) {
					printf("[Mono] Retrying Object init after parent init...\n");
					fflush(stdout);
					init_ok = mono_class_init(godot_object_class);
					printf("[Mono] Object init retry result: %d\n", (int)init_ok);
					fflush(stdout);
				}
			}
		}

		// If still failing, try initializing Node first (which might force Object resolution)
		if (!init_ok && godot_node_class) {
			printf("[Mono] Trying to init Node class (might force Object resolution)...\n");
			fflush(stdout);
			mono_bool ninit = mono_class_init(godot_node_class);
			printf("[Mono] Node init result: %d\n", (int)ninit);
			fflush(stdout);

			if (ninit) {
				printf("[Mono] Retrying Object init after Node init...\n");
				fflush(stdout);
				init_ok = mono_class_init(godot_object_class);
				printf("[Mono] Object init retry result: %d\n", (int)init_ok);
				fflush(stdout);
			}
		}
	}

	if (!godot_object_class) {
		printf("[Mono] WARNING: Godot.Object class not found in GodotSharp.\n");
	} else {
		printf("[Mono] Godot classes cached.\n");
	}
	fflush(stdout);
}

}
