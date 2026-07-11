// Mono WASM runtime JS library - provides functions that Mono expects from JS environment
mergeInto(LibraryManager.library, {
    mono_wasm_debugger_init: function() {
        // No-op: debugger not supported in this build
    },

    mono_wasm_set_timeout: function(timeout, id) {
        // No-op: setTimeout not needed in single-threaded WASM
    },

    mono_wasm_asm_loaded: function(name, assembly_ptr, pdb_ptr, pdb_size) {
        // No-op: assembly loading callback
    },

    mono_wasm_debugger_log: function(level, msg_ptr) {
        // No-op: debugger logging
    },

    mono_wasm_visit_stack_frames: function(cb) {
        // No-op: stack frame visiting
    },

    mono_wasm_get_caller_no_inlines: function() {
        return 0;
    },

    mono_wasm_write_managed_stream: function(stream_handle, buffer_ptr, length) {
        // No-op: managed stream writing
    },

    mono_wasm_enqueue_web_worker_job: function(job) {
        // No-op: web worker jobs not supported
        return 0;
    },

    mono_wasm_load_assembly_with_pdb_bytes: function(assembly_name, assembly_bytes, assembly_size, pdb_bytes, pdb_size) {
        // No-op: assembly loading handled by Mono runtime directly
        return 0;
    },

    mono_wasm_get_loaded_assembly_count: function() {
        return 0;
    },

    mono_wasm_string_from_utf16: function() {
        return 0;
    },

    mono_wasm_compile_method: function(method_ptr) {
        return 0;
    },
});
