#pragma once

#ifdef TOOLS_ENABLED

#include "editor/export/editor_export_plugin.h"

class MonoExportPlugin : public EditorExportPlugin {
	GDCLASS(MonoExportPlugin, EditorExportPlugin);

protected:
	static void _bind_methods() {}

public:
	virtual void _export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) override;
	virtual String get_name() const override { return "Mono"; }
};

#endif // TOOLS_ENABLED
