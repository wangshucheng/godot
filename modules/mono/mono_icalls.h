#pragma once

#include <mono/metadata/object.h>
#include <mono/metadata/assembly.h>

#include "core/object/object.h"

void godot_register_icalls();

// TestSignalReceiver: native target for REAL signal connect/emit round-trips
// in the C# workflow test suite (csharp_test/Test.cs scenario 1h).
// godot_icall_Test_ConnectSignal performs an actual Object::connect() to an
// instance of this class; godot_icall_Test_EmitSignal dispatches the signal;
// the shared counter is incremented HERE (by the callback), so the test only
// passes when the full signal path works end to end.
class TestSignalReceiver : public Object {
	GDCLASS(TestSignalReceiver, Object);

protected:
	static void _bind_methods() {}

public:
	void on_test_signal();
};
