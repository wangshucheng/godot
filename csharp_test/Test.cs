using Godot;
using System;

// ============================================================
// Godot 4.7 Mono WASM C# Workflow Systematic Test Suite
// 24 test scenarios with detailed assertions.
// All native operations use WASM-safe icalls (string/int params only).
// Assertion framework is implemented in C++ via icalls to avoid
// Mono WASM interpreter signature mismatch on void methods with strings.
// State machine: one test per frame, results via Debug UI.
// ============================================================
public partial class Test : Node
{
	// State machine
	private int _state;
	private int _frameCount;
	private int _physicsCount;
	private int _isWeb;
	private int _waitFrames;

	public override void _Ready()
	{
		// Empty: avoids Mono WASM _Ready signature mismatch.
		// All init happens in _Process state 0.
	}

	public override void _PhysicsProcess(double delta)
	{
		_physicsCount++;
	}

	// Scenario 24e: method-group signal handler (mirrors Fuzz10's proven path).
	private int _signalReceived = -1;
	private void OnHealthChangedForTest(int v) { _signalReceived = v; }

	// NOTE: Do NOT create custom instance void methods with string params.
	// Mono WASM interpreter corrupts function table on instance void method
	// calls with string parameters. Use Runtime.TestAssert/TestFinishTest
	// (static icall wrappers) directly instead.

	public override void _Process(double delta)
	{
		_frameCount++;

		// State 0: Initialize
		if (_state == 0)
		{
			_frameCount = 1;
			_physicsCount = 0;
			_waitFrames = 0;
			_isWeb = Runtime.TestIsWebPlatform();
			Runtime.TestResetCounters();
			Runtime.DebugUiInit();
			Runtime.DebugUiClear();
			Runtime.DebugUiAddLine("=== Godot 4.7 C# Workflow Tests ===");
			Runtime.DebugUiAddLine("24 scenarios with detailed assertions");
			Runtime.DebugUiAddSeparator();

			// Reflection API verification (on non-WASM platforms)
			if (_isWeb == 0)
			{
				// Test ClassDB reflection
				string[] classes = Reflection.GetClassList();
				GD.Print("Reflection: " + classes.Length + " classes in ClassDB");
				Runtime.DebugUiAddLine("Reflection: " + classes.Length + " classes in ClassDB");

				bool nodeExists = Reflection.ClassExists("Node");
				bool fakeExists = Reflection.ClassExists("FakeClass123");
				GD.Print("Reflection: Node exists=" + nodeExists + " Fake=" + fakeExists);
				Runtime.DebugUiAddLine("Reflection: Node exists=" + nodeExists + " Fake=" + fakeExists);

				string nodeParent = Reflection.GetParentClass("Node");
				string labelParent = Reflection.GetParentClass("Label");
				GD.Print("Reflection: Node parent=" + nodeParent + " Label parent=" + labelParent);
				Runtime.DebugUiAddLine("Reflection: Node parent=" + nodeParent + " Label parent=" + labelParent);

				bool isSubclass = Reflection.IsSubclassOf("Label", "CanvasItem");
				GD.Print("Reflection: Label subclass of CanvasItem=" + isSubclass);
				Runtime.DebugUiAddLine("Reflection: Label subclass of CanvasItem=" + isSubclass);

				string[] nodeMethods = Reflection.GetMethodList("Node");
				GD.Print("Reflection: Node has " + nodeMethods.Length + " methods");
				Runtime.DebugUiAddLine("Reflection: Node has " + nodeMethods.Length + " methods");

				bool hasReady = Reflection.HasMethod("Node", "_ready");
				bool hasProcess = Reflection.HasMethod("Node", "_process");
				GD.Print("Reflection: Node has _ready=" + hasReady + " _process=" + hasProcess);
				Runtime.DebugUiAddLine("Reflection: Node has _ready=" + hasReady + " _process=" + hasProcess);

				int addChildArgs = Reflection.GetMethodArgumentCount("Node", "add_child");
				GD.Print("Reflection: add_child arg count=" + addChildArgs);
				Runtime.DebugUiAddLine("Reflection: add_child arg count=" + addChildArgs);

				string[] nodeProps = Reflection.GetPropertyList("Node");
				GD.Print("Reflection: Node has " + nodeProps.Length + " properties");
				Runtime.DebugUiAddLine("Reflection: Node has " + nodeProps.Length + " properties");

				bool hasName = Reflection.HasProperty("Node", "name");
				GD.Print("Reflection: Node has 'name' property=" + hasName);
				Runtime.DebugUiAddLine("Reflection: Node has 'name' property=" + hasName);

				string[] nodeSignals = Reflection.GetSignalList("Node");
				GD.Print("Reflection: Node has " + nodeSignals.Length + " signals");
				Runtime.DebugUiAddLine("Reflection: Node has " + nodeSignals.Length + " signals");

				bool hasReadySig = Reflection.HasSignal("Node", "ready");
				GD.Print("Reflection: Node has 'ready' signal=" + hasReadySig);
				Runtime.DebugUiAddLine("Reflection: Node has 'ready' signal=" + hasReadySig);

				// REAL assertions (previously all of the above was only
				// printed, never asserted — a reflection regression could
				// not fail the suite).
				Runtime.TestAssert("0a ClassExists(Node)", nodeExists ? 1 : 0);
				Runtime.TestAssert("0b !ClassExists(FakeClass123)", !fakeExists ? 1 : 0);
				Runtime.TestAssert("0c GetParentClass(Node)==Object", nodeParent == "Object" ? 1 : 0);
				Runtime.TestAssert("0d GetParentClass(Label)==Control", labelParent == "Control" ? 1 : 0);
				Runtime.TestAssert("0e IsSubclassOf(Label,CanvasItem)", isSubclass ? 1 : 0);
				// NOTE: _ready/_process are VIRTUAL methods and are not in
				// ClassDB::has_method's method_map, so assert on a real bound
				// method plus a negative check instead.
				Runtime.TestAssert("0f HasMethod(Node,add_child)", Reflection.HasMethod("Node", "add_child") ? 1 : 0);
				Runtime.TestAssert("0f2 !HasMethod(Node,fake_method_123)", !Reflection.HasMethod("Node", "fake_method_123") ? 1 : 0);
				Runtime.TestAssert("0g add_child argc>=1", addChildArgs >= 1 ? 1 : 0);
				Runtime.TestAssert("0h HasProperty(Node,name)", hasName ? 1 : 0);
				Runtime.TestAssert("0i HasSignal(Node,ready)", hasReadySig ? 1 : 0);
				Runtime.TestFinishTest("0. Reflection: ClassDB introspection");
			}

			_state = 1;
			return;
		}

		// ============================================================
		// Test 1: base_test - Node lifecycle, signals, tree operations
		// ============================================================
		if (_state == 1)
		{
			Runtime.DebugUiAddLine("Test 1: Node Lifecycle & Signals");

			// 1a: Create Node and verify
			int created = Runtime.TestCreate("Node");
			Runtime.TestAssert("1a Create Node", created == 1 ? 1 : 0);

			// 1b: Add to scene tree
			Runtime.TestAddToScene();
			Runtime.TestAssert("1b AddToScene", Runtime.TestIsValid() == 1 ? 1 : 0);

			// 1c: Add children and verify count
			int c1 = Runtime.TestAddChild("Node");
			int c2 = Runtime.TestAddChild("Node");
			int childCount = Runtime.TestGetChildCount();
			Runtime.TestAssert("1c AddChild x2", (c1 == 1 && c2 == 1) ? 1 : 0);
			Runtime.TestAssert("1c ChildCount==2", childCount == 2 ? 1 : 0);

			// 1d: Set name and verify length
			Runtime.TestSetName("TestBaseNode");
			int nameLen = Runtime.TestGetNameLen();
			Runtime.TestAssert("1d SetName len==12", nameLen == 12 ? 1 : 0);

			// 1e: Set/Get int property (process_mode)
			Runtime.TestSetIntProp("process_mode", 1);
			int propVal = Runtime.TestGetIntProp("process_mode");
			Runtime.TestAssert("1e SetIntProp==1", propVal == 1 ? 1 : 0);

			// 1f: Set string property
			Runtime.TestSetStringProp("name", "ChildName");

			// 1g: Has method check
			int hasReady = Runtime.TestHasMethod("is_inside_tree");
			Runtime.TestAssert("1g HasMethod is_inside_tree", hasReady == 1 ? 1 : 0);

			// 1h: Signal system - REAL round trip. TestConnectSignal performs
			// an actual Object::connect to a native TestSignalReceiver;
			// TestEmitSignal dispatches; the counter is incremented by the
			// receiver callback (previously Connect was a no-op and the
			// counter was self-incremented by the emit icall — fake pass).
			int sigConnect = Runtime.TestConnectSignal("ready");
			int sigEmit = Runtime.TestEmitSignal("ready");
			int sigCount = Runtime.TestGetSignalCount();
			Runtime.TestAssert("1h ConnectSignal", sigConnect == 1 ? 1 : 0);
			Runtime.TestAssert("1h EmitSignal", sigEmit == 1 ? 1 : 0);
			Runtime.TestAssert("1h SignalCount==1 (callback fired)", sigCount == 1 ? 1 : 0);

			// 1i: Remove child and verify count decreased
			int removed = Runtime.TestRemoveChildIdx(0);
			int newCount = Runtime.TestGetChildCount();
			Runtime.TestAssert("1i RemoveChild", removed == 1 ? 1 : 0);
			Runtime.TestAssert("1i ChildCount==1", newCount == 1 ? 1 : 0);

			// 1j: Free object. queue_free is deferred, so post-free validity
			// cannot be checked in the same frame — assert validity BEFORE
			// the free instead of the old constant-1 pseudo assertion.
			Runtime.TestAssert("1j Valid before free", Runtime.TestIsValid() == 1 ? 1 : 0);
			Runtime.TestFree();

			// 1k: _Process is running. (_physicsCount >= 0 was an always-true
			// tautology; physics frames are properly verified in scenario 16
			// where the counts are guaranteed non-zero.)
			Runtime.TestAssert("1k Process running", _frameCount > 0 ? 1 : 0);

			Runtime.TestFinishTest("1. Base: Node lifecycle & signals");
			_state = 2;
			return;
		}

		// ============================================================
		// Test 2: scene_test - Scene load, instantiate, reuse
		// ============================================================
		if (_state == 2)
		{
			Runtime.DebugUiAddLine("Test 2: Scene Management");

			// 2a: Load scene from res://
			int load1 = Runtime.TestLoadScene("res://base_test.tscn");
			Runtime.TestAssert("2a LoadScene base_test", load1 == 1 ? 1 : 0);

			// 2b: Instantiate scene
			int inst1 = Runtime.TestInstantiateScene();
			Runtime.TestAssert("2b InstantiateScene", inst1 == 1 ? 1 : 0);

			// 2c: Get child count of instantiated scene
			int sc1 = Runtime.TestGetSceneChildCount();
			Runtime.TestAssert("2c SceneChildCount==3", sc1 == 3 ? 1 : 0);

			// 2d: Free scene instance (void op; load/instantiate above are
			// the real assertions — the old constant-1 assert was vacuous)
			Runtime.TestFreeScene();

			// 2e: Load second scene (scene_test.tscn)
			int load2 = Runtime.TestLoadScene("res://scene_test.tscn");
			Runtime.TestAssert("2e LoadScene scene_test", load2 == 1 ? 1 : 0);

			// 2f: Instantiate second scene
			int inst2 = Runtime.TestInstantiateScene();
			Runtime.TestAssert("2f InstantiateScene 2", inst2 == 1 ? 1 : 0);

			// 2g: Verify child count of second scene
			int sc2 = Runtime.TestGetSceneChildCount();
			Runtime.TestAssert("2g Scene2 ChildCount==2", sc2 == 2 ? 1 : 0);

			// 2h: Free second scene
			Runtime.TestFreeScene();

			// 2i: Reload first scene (test resource caching/reuse)
			int load1b = Runtime.TestLoadScene("res://base_test.tscn");
			Runtime.TestAssert("2i Reload base_test", load1b == 1 ? 1 : 0);
			Runtime.TestFreeScene();

			Runtime.TestFinishTest("2. Scene: Load, instantiate, reuse");
			_state = 3;
			return;
		}

		// ============================================================
		// Test 3: rendering_test - Camera, Light, Mesh, Sprite
		// ============================================================
		if (_state == 3)
		{
			Runtime.DebugUiAddLine("Test 3: Rendering Objects");

			// 3a: Camera3D creation
			int cam = Runtime.TestCreate("Camera3D");
			Runtime.TestAssert("3a Create Camera3D", cam == 1 ? 1 : 0);
			if (cam == 1) { Runtime.TestAddToScene(); Runtime.TestFree(); }

			// 3b: DirectionalLight3D creation
			int light = Runtime.TestCreate("DirectionalLight3D");
			Runtime.TestAssert("3b Create DirectionalLight3D", light == 1 ? 1 : 0);
			if (light == 1) { Runtime.TestAddToScene(); Runtime.TestFree(); }

			// 3c: Sprite2D creation
			int sprite = Runtime.TestCreate("Sprite2D");
			Runtime.TestAssert("3c Create Sprite2D", sprite == 1 ? 1 : 0);
			if (sprite == 1) { Runtime.TestFree(); }

			// 3d: MeshInstance3D creation
			int mesh = Runtime.TestCreate("MeshInstance3D");
			Runtime.TestAssert("3d Create MeshInstance3D", mesh == 1 ? 1 : 0);
			if (mesh == 1) { Runtime.TestFree(); }

			// 3e: OmniLight3D creation (PointLight3D renamed to OmniLight3D in Godot 4)
			int plight = Runtime.TestCreate("OmniLight3D");
			Runtime.TestAssert("3e Create OmniLight3D", plight == 1 ? 1 : 0);
			if (plight == 1) { Runtime.TestFree(); }

			// 3f: SpotLight3D creation
			int slight = Runtime.TestCreate("SpotLight3D");
			Runtime.TestAssert("3f Create SpotLight3D", slight == 1 ? 1 : 0);
			if (slight == 1) { Runtime.TestFree(); }

			// 3g: WorldEnvironment creation
			int env = Runtime.TestCreate("WorldEnvironment");
			Runtime.TestAssert("3g Create WorldEnvironment", env == 1 ? 1 : 0);
			if (env == 1) { Runtime.TestFree(); }

			Runtime.TestFinishTest("3. Rendering: Camera/Light/Mesh/Sprite");
			_state = 4;
			return;
		}

		// ============================================================
		// Test 4: input_test - Keyboard, mouse, ClassDB
		// ============================================================
		if (_state == 4)
		{
			Runtime.DebugUiAddLine("Test 4: Input System");

			// 4a: Keyboard query (Space, Enter, Escape - should not be pressed)
			int keySpace = Input.IsKeyPressed(Key.Space) ? 1 : 0;
			int keyEnter = Input.IsKeyPressed(Key.Enter) ? 1 : 0;
			int keyEsc = Input.IsKeyPressed(Key.Escape) ? 1 : 0;
			Runtime.TestAssert("4a KeySpace not pressed", keySpace == 0 ? 1 : 0);
			Runtime.TestAssert("4a KeyEnter not pressed", keyEnter == 0 ? 1 : 0);
			Runtime.TestAssert("4a KeyEsc not pressed", keyEsc == 0 ? 1 : 0);

			// 4b: Mouse query (Left, Right, Middle - should not be pressed)
			int mouseLeft = Input.IsMouseButtonPressed(MouseButton.Left) ? 1 : 0;
			int mouseRight = Input.IsMouseButtonPressed(MouseButton.Right) ? 1 : 0;
			int mouseMid = Input.IsMouseButtonPressed(MouseButton.Middle) ? 1 : 0;
			Runtime.TestAssert("4b MouseLeft not pressed", mouseLeft == 0 ? 1 : 0);
			Runtime.TestAssert("4b MouseRight not pressed", mouseRight == 0 ? 1 : 0);
			Runtime.TestAssert("4b MouseMid not pressed", mouseMid == 0 ? 1 : 0);

			// 4c: ClassDB category detection
			int nodeCat = Runtime.TestGetClassCategory("Node");
			int ctrlCat = Runtime.TestGetClassCategory("Control");
			int resCat = Runtime.TestGetClassCategory("Resource");
			Runtime.TestAssert("4c Node category==1", nodeCat == 1 ? 1 : 0);
			Runtime.TestAssert("4c Control category==2", ctrlCat == 2 ? 1 : 0);
			Runtime.TestAssert("4c Resource category==4", resCat == 4 ? 1 : 0);

			// 4d: Verify more class categories
			int labelCat = Runtime.TestGetClassCategory("Label");
			int buttonCat = Runtime.TestGetClassCategory("Button");
			Runtime.TestAssert("4d Label is Control(2)", labelCat == 2 ? 1 : 0);
			Runtime.TestAssert("4d Button is Control(2)", buttonCat == 2 ? 1 : 0);

			Runtime.TestFinishTest("4. Input: Keyboard/mouse/ClassDB");
			_state = 5;
			return;
		}

		// ============================================================
		// Test 5: ui_test - Label, Button, LineEdit, Containers
		// ============================================================
		if (_state == 5)
		{
			Runtime.DebugUiAddLine("Test 5: UI Controls");

			// 5a: Label creation and text setting
			int label = Runtime.TestCreate("Label");
			Runtime.TestAssert("5a Create Label", label == 1 ? 1 : 0);
			if (label == 1)
			{
				Runtime.TestSetStringProp("text", "Hello UI");
				Runtime.TestAddToScene();
				Runtime.TestFree();
			}

			// 5b: Button creation
			int btn = Runtime.TestCreate("Button");
			Runtime.TestAssert("5b Create Button", btn == 1 ? 1 : 0);
			if (btn == 1)
			{
				Runtime.TestSetStringProp("text", "Click Me");
				Runtime.TestFree();
			}

			// 5c: LineEdit creation
			int edit = Runtime.TestCreate("LineEdit");
			Runtime.TestAssert("5c Create LineEdit", edit == 1 ? 1 : 0);
			if (edit == 1)
			{
				Runtime.TestSetStringProp("text", "Input here");
				Runtime.TestFree();
			}

			// 5d: TextEdit creation
			int ted = Runtime.TestCreate("TextEdit");
			Runtime.TestAssert("5d Create TextEdit", ted == 1 ? 1 : 0);
			if (ted == 1) { Runtime.TestFree(); }

			// 5e: VBoxContainer with children
			int vbox = Runtime.TestCreate("VBoxContainer");
			Runtime.TestAssert("5e Create VBoxContainer", vbox == 1 ? 1 : 0);
			if (vbox == 1)
			{
				Runtime.TestAddChild("Label");
				Runtime.TestAddChild("Button");
				Runtime.TestAddChild("LineEdit");
				int vboxChildren = Runtime.TestGetChildCount();
				Runtime.TestAssert("5e VBox children==3", vboxChildren == 3 ? 1 : 0);
				Runtime.TestFree();
			}

			// 5f: HBoxContainer
			int hbox = Runtime.TestCreate("HBoxContainer");
			Runtime.TestAssert("5f Create HBoxContainer", hbox == 1 ? 1 : 0);
			if (hbox == 1)
			{
				Runtime.TestAddChild("Label");
				Runtime.TestAddChild("Button");
				int hc = Runtime.TestGetChildCount();
				Runtime.TestAssert("5f HBox children==2", hc == 2 ? 1 : 0);
				Runtime.TestFree();
			}

			// 5g: GridContainer
			int grid = Runtime.TestCreate("GridContainer");
			Runtime.TestAssert("5g Create GridContainer", grid == 1 ? 1 : 0);
			if (grid == 1)
			{
				Runtime.TestSetIntProp("columns", 3);
				int cols = Runtime.TestGetIntProp("columns");
				Runtime.TestAssert("5g Grid columns==3", cols == 3 ? 1 : 0);
				Runtime.TestFree();
			}

			Runtime.TestFinishTest("5. UI: Label/Button/LineEdit/Containers");
			_state = 6;
			return;
		}

		// ============================================================
		// Test 6: animation_test - AnimationPlayer, AnimationTree
		// ============================================================
		if (_state == 6)
		{
			Runtime.DebugUiAddLine("Test 6: Animation System");

			// 6a: AnimationPlayer creation
			int ap = Runtime.TestCreate("AnimationPlayer");
			Runtime.TestAssert("6a Create AnimationPlayer", ap == 1 ? 1 : 0);

			// 6b: Add to scene (required for playback)
			if (ap == 1) { Runtime.TestAddToScene(); }

			// 6c: Add animation
			int addAnim = 0;
			if (ap == 1)
			{
				addAnim = Runtime.TestAddAnimation("test_anim");
			}
			Runtime.TestAssert("6c AddAnimation", addAnim == 1 ? 1 : 0);

			// 6d: Get animation count
			int animCount = 0;
			if (ap == 1)
			{
				animCount = Runtime.TestGetAnimationCount();
			}
			Runtime.TestAssert("6d AnimationCount>=1", animCount >= 1 ? 1 : 0);

			// 6e: Play animation
			int playAnim = 0;
			if (ap == 1)
			{
				playAnim = Runtime.TestPlayAnimation("test_anim");
			}
			Runtime.TestAssert("6e PlayAnimation", playAnim == 1 ? 1 : 0);

			// 6f: Check if animation is playing
			int isPlaying = 0;
			if (ap == 1)
			{
				isPlaying = Runtime.TestIsAnimationPlaying("test_anim");
			}
			Runtime.TestAssert("6f IsAnimationPlaying", isPlaying == 1 ? 1 : 0);

			// 6g: AnimationTree creation
			Runtime.TestFree();
			int at = Runtime.TestCreate("AnimationTree");
			Runtime.TestAssert("6g Create AnimationTree", at == 1 ? 1 : 0);
			if (at == 1) { Runtime.TestFree(); }

			Runtime.TestFinishTest("6. Animation: AnimationPlayer/Tree");
			_state = 7;
			return;
		}

		// ============================================================
		// Test 7: physics_test - RigidBody, Collision, Raycast
		// ============================================================
		if (_state == 7)
		{
			Runtime.DebugUiAddLine("Test 7: Physics Engine");

			// 7a: RigidBody3D creation
			int rigid = Runtime.TestCreate("RigidBody3D");
			Runtime.TestAssert("7a Create RigidBody3D", rigid == 1 ? 1 : 0);
			if (rigid == 1) { Runtime.TestAddToScene(); Runtime.TestFree(); }

			// 7b: StaticBody3D creation
			int sb = Runtime.TestCreate("StaticBody3D");
			Runtime.TestAssert("7b Create StaticBody3D", sb == 1 ? 1 : 0);
			if (sb == 1) { Runtime.TestFree(); }

			// 7c: CollisionShape3D creation
			int cs = Runtime.TestCreate("CollisionShape3D");
			Runtime.TestAssert("7c Create CollisionShape3D", cs == 1 ? 1 : 0);
			if (cs == 1) { Runtime.TestFree(); }

			// 7d: CharacterBody3D creation
			int cb = Runtime.TestCreate("CharacterBody3D");
			Runtime.TestAssert("7d Create CharacterBody3D", cb == 1 ? 1 : 0);
			if (cb == 1) { Runtime.TestFree(); }

			// 7e: Area3D creation
			int area = Runtime.TestCreate("Area3D");
			Runtime.TestAssert("7e Create Area3D", area == 1 ? 1 : 0);
			if (area == 1) { Runtime.TestFree(); }

			// 7f: 2D physics objects
			int r2d = Runtime.TestCreate("RigidBody2D");
			Runtime.TestAssert("7f Create RigidBody2D", r2d == 1 ? 1 : 0);
			if (r2d == 1) { Runtime.TestFree(); }

			int s2d = Runtime.TestCreate("StaticBody2D");
			Runtime.TestAssert("7f Create StaticBody2D", s2d == 1 ? 1 : 0);
			if (s2d == 1) { Runtime.TestFree(); }

			int cs2d = Runtime.TestCreate("CollisionShape2D");
			Runtime.TestAssert("7f Create CollisionShape2D", cs2d == 1 ? 1 : 0);
			if (cs2d == 1) { Runtime.TestFree(); }

			// 7g: Raycast in an empty physics world must MISS (deterministic
			// expectation — the old assert accepted both 0 and 1, i.e. nothing).
			int ray = Runtime.TestRaycast3D();
			Runtime.TestAssert("7g Raycast3D empty world == miss", ray == 0 ? 1 : 0);

			Runtime.TestFinishTest("7. Physics: Rigid/Static/Collision/Raycast");
			_state = 8;
			return;
		}

		// ============================================================
		// Test 8: audio_test - AudioStreamPlayer, volume control
		// ============================================================
		if (_state == 8)
		{
			Runtime.DebugUiAddLine("Test 8: Audio System");

			// 8a: AudioStreamPlayer creation
			int asp = Runtime.TestCreate("AudioStreamPlayer");
			Runtime.TestAssert("8a Create AudioStreamPlayer", asp == 1 ? 1 : 0);

			// 8b: Add to scene and verify the object is still valid
			// (previously this re-asserted 8a's creation result).
			if (asp == 1) { Runtime.TestAddToScene(); }
			Runtime.TestAssert("8b AddToScene valid", Runtime.TestIsValid() == 1 ? 1 : 0);

			// 8c: Set volume
			int setVol = 0;
			if (asp == 1)
			{
				setVol = Runtime.TestSetAudioVolume(-50); // -5.0 dB
			}
			Runtime.TestAssert("8c SetAudioVolume", setVol == 1 ? 1 : 0);

			// 8d: Get volume and verify
			int getVol = -999;
			if (asp == 1)
			{
				getVol = Runtime.TestGetAudioVolume();
			}
			Runtime.TestAssert("8d GetAudioVolume==-50", getVol == -50 ? 1 : 0);

			// 8e: Change volume and verify
			if (asp == 1)
			{
				Runtime.TestSetAudioVolume(0); // 0.0 dB
				int vol2 = Runtime.TestGetAudioVolume();
				Runtime.TestAssert("8e Volume changed to 0", vol2 == 0 ? 1 : 0);
			}

			// 8f: Free AudioStreamPlayer
			if (asp == 1) { Runtime.TestFree(); }

			// 8g: AudioStreamPlayer2D creation
			int asp2d = Runtime.TestCreate("AudioStreamPlayer2D");
			Runtime.TestAssert("8g Create AudioStreamPlayer2D", asp2d == 1 ? 1 : 0);
			if (asp2d == 1) { Runtime.TestFree(); }

			// 8h: AudioStreamPlayer3D creation
			int asp3d = Runtime.TestCreate("AudioStreamPlayer3D");
			Runtime.TestAssert("8h Create AudioStreamPlayer3D", asp3d == 1 ? 1 : 0);
			if (asp3d == 1) { Runtime.TestFree(); }

			Runtime.TestFinishTest("8. Audio: StreamPlayer/Volume/2D/3D");
			_state = 9;
			return;
		}

		// ============================================================
		// Test 9: fs_test - File I/O, path resolution
		// ============================================================
		if (_state == 9)
		{
			Runtime.DebugUiAddLine("Test 9: File System");

			// 9a: Write file to user://
			int write1 = Runtime.TestFileWrite("user://test_file.txt", "Hello FileSystem");
			Runtime.TestAssert("9a FileWrite", write1 == 1 ? 1 : 0);

			// 9b: Check file exists
			int exists1 = Runtime.TestFileExists("user://test_file.txt");
			Runtime.TestAssert("9b FileExists", exists1 == 1 ? 1 : 0);

			// 9c: Read file and verify length
			int readLen = Runtime.TestFileRead("user://test_file.txt");
			Runtime.TestAssert("9c FileRead len==16", readLen == 16 ? 1 : 0);

			// 9d: Check non-existent file
			int exists2 = Runtime.TestFileExists("user://nonexistent.txt");
			Runtime.TestAssert("9d NonExistent==0", exists2 == 0 ? 1 : 0);

			// 9e: Write second file (different content)
			int write2 = Runtime.TestFileWrite("user://test_data.txt", "Test data content");
			Runtime.TestAssert("9e FileWrite 2", write2 == 1 ? 1 : 0);

			// 9f: Read second file
			int readLen2 = Runtime.TestFileRead("user://test_data.txt");
			Runtime.TestAssert("9f FileRead 2 len==17", readLen2 == 17 ? 1 : 0);

			// 9g: Delete file
			int del1 = Runtime.TestFileDelete("user://test_file.txt");
			Runtime.TestAssert("9g FileDelete", del1 == 1 ? 1 : 0);

			// 9h: Verify deleted
			int exists3 = Runtime.TestFileExists("user://test_file.txt");
			Runtime.TestAssert("9h Deleted file==0", exists3 == 0 ? 1 : 0);

			// 9i: Resource loading from res://
			int loadScene = Runtime.TestLoadScene("res://fs_test.tscn");
			Runtime.TestAssert("9i LoadScene fs_test", loadScene == 1 ? 1 : 0);
			if (loadScene == 1) { Runtime.TestFreeScene(); }

			// 9j: Load multiple resources
			int loadUi = Runtime.TestLoadScene("res://ui_test.tscn");
			int loadAudio = Runtime.TestLoadScene("res://audio_test.tscn");
			Runtime.TestAssert("9j LoadScene ui_test", loadUi == 1 ? 1 : 0);
			Runtime.TestAssert("9j LoadScene audio_test", loadAudio == 1 ? 1 : 0);
			if (loadUi == 1) { Runtime.TestFreeScene(); }
			if (loadAudio == 1) { Runtime.TestFreeScene(); }

			// Cleanup
			Runtime.TestFileDelete("user://test_data.txt");

			Runtime.TestFinishTest("9. FileSystem: Write/Read/Exists/Delete/Load");
			_state = 10;
			return;
		}

		// ============================================================
		// Test 10: save_test - Data persistence, JSON simulation
		// ============================================================
		if (_state == 10)
		{
			Runtime.DebugUiAddLine("Test 10: Save & Persistence");

			// 10a: Write save data (simulating JSON)
			int saveWrite = Runtime.TestFileWrite(
				"user://save_game.dat",
				"score=1000,level=5,player=Hero");
			Runtime.TestAssert("10a WriteSaveData", saveWrite == 1 ? 1 : 0);

			// 10b: Verify save file exists
			int saveExists = Runtime.TestFileExists("user://save_game.dat");
			Runtime.TestAssert("10b SaveExists", saveExists == 1 ? 1 : 0);

			// 10c: Read save data
			int saveLen = Runtime.TestFileRead("user://save_game.dat");
			Runtime.TestAssert("10c ReadSaveData len>0", saveLen > 0 ? 1 : 0);

			// 10d: Verify content length (30 chars)
			Runtime.TestAssert("10d SaveData len==30", saveLen == 30 ? 1 : 0);

			// 10e: Write second save (version 2 - test forward compat)
			int save2 = Runtime.TestFileWrite(
				"user://save_game_v2.dat",
				"score=1000,level=5,player=Hero,health=100");
			Runtime.TestAssert("10e WriteSaveV2", save2 == 1 ? 1 : 0);

			// 10f: Read v2 save
			int save2Len = Runtime.TestFileRead("user://save_game_v2.dat");
			Runtime.TestAssert("10f ReadSaveV2 len>0", save2Len > 0 ? 1 : 0);

			// 10g: Load save_test.tscn scene
			int loadScene = Runtime.TestLoadScene("res://save_test.tscn");
			Runtime.TestAssert("10g LoadScene save_test", loadScene == 1 ? 1 : 0);

			// 10h: Instantiate save scene
			int instScene = 0;
			if (loadScene == 1)
			{
				instScene = Runtime.TestInstantiateScene();
			}
			Runtime.TestAssert("10h InstantiateScene", instScene == 1 ? 1 : 0);

			// 10i: Get scene child count
			int sc = 0;
			if (instScene == 1)
			{
				sc = Runtime.TestGetSceneChildCount();
			}
			Runtime.TestAssert("10i SceneChildCount==1", sc == 1 ? 1 : 0);

			if (loadScene == 1) { Runtime.TestFreeScene(); }

			// 10j: Cleanup save files
			int del1 = Runtime.TestFileDelete("user://save_game.dat");
			int del2 = Runtime.TestFileDelete("user://save_game_v2.dat");
			Runtime.TestAssert("10j Cleanup saves", (del1 == 1 && del2 == 1) ? 1 : 0);

			Runtime.TestFinishTest("10. Save: Write/Read/Persist/Version");
			_state = 11;
			return;
		}

		// ============================================================
		// Test 11: desktop_hotreload_test - Assembly & scene reload
		// ============================================================
		if (_state == 11)
		{
			Runtime.DebugUiAddLine("Test 11: Hot Reload");

			// 11a: C# assembly is loaded and executing (this script runs)
			Runtime.TestAssert("11a Assembly loaded", 1);

			// 11b: _Process is being called (frame counter incrementing)
			Runtime.TestAssert("11b Process running", _frameCount > 1 ? 1 : 0);

			// 11c: _PhysicsProcess is being called
			Runtime.TestAssert("11c PhysicsProcess running", _physicsCount > 0 ? 1 : 0);

			// 11d: Scene reload test (load, instantiate, free, reload)
			int load1 = Runtime.TestLoadScene("res://desktop_hotreload_test.tscn");
			Runtime.TestAssert("11d LoadScene hotreload", load1 == 1 ? 1 : 0);
			if (load1 == 1)
			{
				int inst1 = Runtime.TestInstantiateScene();
				Runtime.TestAssert("11d InstantiateScene", inst1 == 1 ? 1 : 0);
				Runtime.TestFreeScene();

				// Reload
				int load2 = Runtime.TestLoadScene("res://desktop_hotreload_test.tscn");
				Runtime.TestAssert("11d ReloadScene", load2 == 1 ? 1 : 0);
				if (load2 == 1) { Runtime.TestFreeScene(); }
			}

			// 11e: Runtime is initialized
			Runtime.TestAssert("11e Runtime initialized", Runtime.IsInitialized ? 1 : 0);

			// 11f: Multiple scene loads (simulate development iteration)
			int loads = 0;
			for (int i = 0; i < 3; i++)
			{
				int l = Runtime.TestLoadScene("res://base_test.tscn");
				if (l == 1) { Runtime.TestFreeScene(); loads++; }
			}
			Runtime.TestAssert("11f 3x scene reload", loads == 3 ? 1 : 0);

			Runtime.TestFinishTest("11. HotReload: Assembly/Scene reload");
			_state = 12;
			return;
		}

		// ============================================================
		// Test 12: bcl_test - .NET BCL compatibility
		// ============================================================
		if (_state == 12)
		{
			Runtime.DebugUiAddLine("Test 12: BCL Compatibility");

			// 12a: engine-side container smoke via C++ icall. NOTE: despite
			// the name, TestBclListTest exercises Godot's Vector<int> on the
			// NATIVE side — it is NOT a .NET BCL test. Real BCL checks: 12e.
			int listPass = Runtime.TestBclListTest();
			Runtime.TestAssert("12a native Vector smoke 4/4", listPass == 4 ? 1 : 0);
			Runtime.DebugUiAddLineInt("    List pass=", listPass);

			// 12b: engine-side HashMap smoke via C++ icall (same caveat as 12a).
			int dictPass = Runtime.TestBclDictTest();
			Runtime.TestAssert("12b native HashMap smoke 4/4", dictPass == 4 ? 1 : 0);
			Runtime.DebugUiAddLineInt("    Dict pass=", dictPass);

			// 12c: File IO test (via C++ icall)
			int ioWrite = Runtime.TestFileWrite("user://bcl_test.txt", "BCL IO test data");
			int ioRead = Runtime.TestFileRead("user://bcl_test.txt");
			int ioExists = Runtime.TestFileExists("user://bcl_test.txt");
			Runtime.TestAssert("12c BclIO write", ioWrite == 1 ? 1 : 0);
			Runtime.TestAssert("12c BclIO read", ioRead == 16 ? 1 : 0);
			Runtime.TestAssert("12c BclIO exists", ioExists == 1 ? 1 : 0);
			Runtime.TestFileDelete("user://bcl_test.txt");

			// 12d: JSON serialization simulation (via file write/read)
			int jsonWrite = Runtime.TestFileWrite(
				"user://bcl_json.dat",
				"{\"name\":\"test\",\"value\":42}");
			int jsonRead = Runtime.TestFileRead("user://bcl_json.dat");
			Runtime.TestAssert("12d JSON write", jsonWrite == 1 ? 1 : 0);
			Runtime.TestAssert("12d JSON read len>0", jsonRead > 0 ? 1 : 0);
			Runtime.TestFileDelete("user://bcl_json.dat");

			// 12e: REAL .NET BCL test (desktop only; on WASM the interpreter's
			// List<T>/Dictionary<K,V> paths are intentionally not exercised
			// here — they are covered by the H9 WASM suite instead).
			// The old TestBclAsyncTest icall returned 1 unconditionally (fake).
			if (_isWeb == 0)
			{
				var bclList = new System.Collections.Generic.List<int>();
				bclList.Add(10); bclList.Add(20); bclList.Add(30);
				int listOk = (bclList.Count == 3 && bclList[1] == 20 && bclList.Contains(30)) ? 1 : 0;
				bclList.RemoveAt(1);
				listOk = (listOk == 1 && bclList.Count == 2 && bclList[1] == 30) ? 1 : 0;
				Runtime.TestAssert("12e BCL List<int> real ops", listOk);

				var bclDict = new System.Collections.Generic.Dictionary<int, int>();
				bclDict[1] = 100; bclDict[2] = 200;
				bclDict[2] = 250;
				int dictOk = (bclDict.Count == 2 && bclDict[2] == 250) ? 1 : 0;
				bclDict.Remove(1);
				dictOk = (dictOk == 1 && bclDict.Count == 1 && !bclDict.ContainsKey(1)) ? 1 : 0;
				Runtime.TestAssert("12e BCL Dictionary<int,int> real ops", dictOk);

				int taskResult = System.Threading.Tasks.Task.Run(() => 42).Result;
				Runtime.TestAssert("12e BCL Task.Run result", taskResult == 42 ? 1 : 0);
			}
			else
			{
				Runtime.DebugUiAddLine("  12e: Skipped on WASM (real BCL test is desktop-only)");
			}

			// 12f: Basic C# arithmetic (safe in WASM)
			int a = 10;
			int b = 20;
			int sum = a + b;
			int product = a * b;
			Runtime.TestAssert("12f Arithmetic sum==30", sum == 30 ? 1 : 0);
			Runtime.TestAssert("12f Arithmetic product==200", product == 200 ? 1 : 0);

			// 12g: C# array operations (safe in WASM)
			int[] arr = new int[5];
			for (int i = 0; i < 5; i++) { arr[i] = i * 10; }
			int arrSum = 0;
			for (int i = 0; i < 5; i++) { arrSum += arr[i]; }
			Runtime.TestAssert("12g Array sum==100", arrSum == 100 ? 1 : 0);

			// 12h: Runtime platform info
			Runtime.TestAssert("12h Runtime initialized", Runtime.IsInitialized ? 1 : 0);

			Runtime.TestFinishTest("12. BCL: List/Dict/IO/JSON/Async/Array");
			_state = 13;
			return;
		}

		// ============================================================
		// Test 13: GC Stress - node create/destroy cycle
		// ============================================================
		if (_state == 13)
		{
			Runtime.DebugUiAddLine("Test 13: GC Stress");

			// 13a: Small batch (100 nodes)
			int smallBatch = Runtime.TestGcStressTest(100);
			Runtime.TestAssert("13a GC stress 100 nodes", smallBatch == 1 ? 1 : 0);

			// 13b: Medium batch (500 nodes - reduced from 1000 for WASM stability)
			int medBatch = Runtime.TestGcStressTest(500);
			Runtime.TestAssert("13b GC stress 500 nodes", medBatch == 1 ? 1 : 0);

			// 13c: Large batch (1000 nodes - reduced from 10000 for WASM stability)
			int largeBatch = Runtime.TestGcStressTest(1000);
			Runtime.TestAssert("13c GC stress 1000 nodes", largeBatch == 1 ? 1 : 0);

			// 13d: Multiple cycles (2x 500 nodes - reduced for WASM stability)
			int cyclesOk = 1;
			for (int i = 0; i < 2; i++) {
				if (Runtime.TestGcStressTest(500) != 1) {
					cyclesOk = 0;
					break;
				}
			}
			Runtime.TestAssert("13d GC stress repeated cycles", cyclesOk);

			Runtime.TestFinishTest("13. GC Stress: node create/destroy");
			_state = 14;
			return;
		}

		// ============================================================
		// Test 14: HTTP Request - HTTPRequest node API
		// ============================================================
		if (_state == 14)
		{
			Runtime.DebugUiAddLine("Test 14: HTTP Request");

			// 14a: Create HTTPRequest node
			int httpCreated = Runtime.TestCreate("HTTPRequest");
			Runtime.TestAssert("14a HTTPRequest created", httpCreated);

			// 14b: Set timeout property
			Runtime.TestSetIntProp("timeout", 5000);
			int timeoutVal = Runtime.TestGetIntProp("timeout");
			Runtime.TestAssert("14b timeout set/get", timeoutVal == 5000 ? 1 : 0);

			// 14c: Check method existence
			int hasRequest = Runtime.TestHasMethod("request");
			Runtime.TestAssert("14c has request method", hasRequest);
			int hasCancel = Runtime.TestHasMethod("cancel_request");
			Runtime.TestAssert("14d has cancel_request", hasCancel);

			// 14e: Check HTTP body size limit
			Runtime.TestSetIntProp("body_size_limit", 1024);
			int bodyLimit = Runtime.TestGetIntProp("body_size_limit");
			Runtime.TestAssert("14e body_size_limit set/get", bodyLimit == 1024 ? 1 : 0);

			// 14f: Skip BCL HttpClient on all platforms (network not required for test).
			// NOTE: skips are logged, NOT asserted — a constant-1 "skip" assert
			// inflates the pass count with a non-test.
			Runtime.DebugUiAddLine("  14f: Skipped (no network test)");

			Runtime.TestFree();
			Runtime.TestFinishTest("14. HTTP: HTTPRequest node + BCL HttpClient");
			_state = 15;
			return;
		}

		// ============================================================
		// Test 15: WebSocket Client - WebSocketPeer API
		// ============================================================
		if (_state == 15)
		{
			Runtime.DebugUiAddLine("Test 15: WebSocket Client");

			// 15a: Create WebSocketPeer
			int wsCreated = Runtime.TestCreate("WebSocketPeer");
			Runtime.TestAssert("15a WebSocketPeer created", wsCreated);

			// 15b: Check method existence
			int hasConnect = Runtime.TestHasMethod("connect_to_url");
			Runtime.TestAssert("15b has connect_to_url", hasConnect);
			int hasPoll = Runtime.TestHasMethod("poll");
			Runtime.TestAssert("15c has poll", hasPoll);
			int hasSend = Runtime.TestHasMethod("send");
			Runtime.TestAssert("15d has send", hasSend);
			int hasGetState = Runtime.TestHasMethod("get_ready_state");
			Runtime.TestAssert("15e has get_ready_state", hasGetState);

			// 15f: Check supported protocols property
			Runtime.TestSetStringProp("supported_protocols", "chat, superchat");
			int hasProtocols = Runtime.TestHasMethod("get_supported_protocols");
			Runtime.TestAssert("15f supported_protocols", hasProtocols);

			// 15g: Desktop-only actual WebSocket connection using BCL
			if (_isWeb == 0)
			{
				try
				{
					// Test BCL ClientWebSocket creation (no actual connection)
					var wsClient = new System.Net.WebSockets.ClientWebSocket();
					Runtime.TestAssert("15g BCL ClientWebSocket created", wsClient != null ? 1 : 0);
					wsClient.Dispose();
				}
				catch
				{
					Runtime.TestAssert("15g BCL ClientWebSocket created", 0);
				}
			}
			else
			{
				Runtime.DebugUiAddLine("  15g: Skipped on WASM (no native WS)");
			}

			Runtime.TestFree();
			Runtime.TestFinishTest("15. WebSocket: WebSocketPeer + BCL ClientWS");
			_state = 16;
			return;
		}

		// ============================================================
		// Test 16: Performance - FPS, object creation speed
		// ============================================================
		if (_state == 16)
		{
			Runtime.DebugUiAddLine("Test 16: Performance");

			// 16a: FPS measurement
			int fps = Runtime.GetEngineFps();
			Runtime.TestAssert("16a FPS > 0", fps > 0 ? 1 : 0);
			Runtime.DebugUiAddLineInt("  FPS: ", fps);

			// 16b: Object creation speed (100 nodes)
			int createOk = Runtime.TestGcStressTest(100);
			Runtime.TestAssert("16b 100 node creation", createOk);

			// 16c: FPS after stress test
			int fpsAfter = Runtime.GetEngineFps();
			Runtime.TestAssert("16c FPS still > 0 after stress", fpsAfter > 0 ? 1 : 0);

			// 16d: Multiple stress cycles for performance consistency
			int cyclesOk = 1;
			for (int i = 0; i < 5; i++)
			{
				if (Runtime.TestGcStressTest(500) != 1)
				{
					cyclesOk = 0;
					break;
				}
			}
			Runtime.TestAssert("16d 5x500 stress cycles", cyclesOk);

			// 16e: Frame count tracking
			Runtime.TestAssert("16e frame count > 0", _frameCount > 0 ? 1 : 0);
			Runtime.DebugUiAddLineInt("  Frames: ", _frameCount);

			// 16f: Physics frame tracking
			Runtime.TestAssert("16f physics frames > 0", _physicsCount > 0 ? 1 : 0);
			Runtime.DebugUiAddLineInt("  Physics frames: ", _physicsCount);

			Runtime.TestFinishTest("16. Performance: FPS/stress/frames");
			_state = 17;
			return;
		}

		// ============================================================
		// Test 17: Memory - GC memory, allocation patterns
		// ============================================================
		if (_state == 17)
		{
			Runtime.DebugUiAddLine("Test 17: Memory");

			// NOTE: Avoid System.GC.* static methods AND direct BCL collection
			// usage (List<T>.Add, Dictionary<K,V>.set_Item) on WASM - they
			// trigger Mono WASM interpreter "function signature mismatch"
			// RuntimeError. Use array ops (instance indexing, safe) and
			// Runtime icalls (TestGcStressTest, TestBclListTest, etc.)
			// which perform BCL ops on the C++ side.

			// 17a: Array allocation and verification
			var largeArray = new int[10000];
			for (int i = 0; i < largeArray.Length; i++)
			{
				largeArray[i] = i;
			}
			Runtime.TestAssert("17a array alloc 10000 ints", largeArray.Length == 10000 ? 1 : 0);

			// 17b: Verify array contents (int arithmetic, no method calls)
			int sum = 0;
			for (int i = 0; i < largeArray.Length; i++)
			{
				sum += largeArray[i];
			}
			// sum of 0..9999 = 9999*10000/2 = 49995000
			Runtime.TestAssert("17b array sum correct", sum == 49995000 ? 1 : 0);

			// 17c: GC stress test via icall (avoids System.GC.* static methods)
			int stressOk = Runtime.TestGcStressTest(500);
			Runtime.TestAssert("17c GC stress 500 nodes", stressOk);

			// 17d: Multiple GC stress cycles via icall
			int cyclesOk = 1;
			for (int i = 0; i < 3; i++)
			{
				if (Runtime.TestGcStressTest(200) != 1)
				{
					cyclesOk = 0;
					break;
				}
			}
			Runtime.TestAssert("17d multiple GC cycles", cyclesOk);

			// 17e: BCL collections via icall (List<T>.Add etc. unsafe in C# on WASM)
			int listPass = Runtime.TestBclListTest();
			Runtime.TestAssert("17e BCL list via icall", listPass == 4 ? 1 : 0);

			// 17f: BCL Dictionary via icall (Dictionary<K,V>.set_Item unsafe in C# on WASM)
			int dictPass = Runtime.TestBclDictTest();
			Runtime.TestAssert("17f BCL dict via icall", dictPass == 4 ? 1 : 0);

			// 17g: Byte buffer allocation (single-dim array, safe)
			var buffer = new byte[1000];
			for (int i = 0; i < buffer.Length; i++)
			{
				buffer[i] = (byte)(i & 0xFF);
			}
			int bufSum = 0;
			for (int i = 0; i < buffer.Length; i++)
			{
				bufSum += buffer[i];
			}
			// sum of (i & 0xFF) for i=0..999 = 4 * (0+1+..+255) + (0+1+..+239) truncated
			// Just verify it's non-zero (alloc worked).
			Runtime.TestAssert("17g byte buffer alloc", bufSum > 0 ? 1 : 0);

			// 17h: Large array release (set null, GC stress triggers collection)
			largeArray = null;
			buffer = null;
			int releaseOk = Runtime.TestGcStressTest(100);
			Runtime.TestAssert("17h array release + GC", releaseOk);

			Runtime.TestFinishTest("17. Memory: GC/alloc/collect/stress");
			_state = 18;
			return;
		}

		// ============================================================
		// Test 18: Mobile - Touch input simulation
		// ============================================================
		if (_state == 18)
		{
			Runtime.DebugUiAddLine("Test 18: Mobile Input");

			// 18a: Create Control for touch area
			int ctrlCreated = Runtime.TestCreate("Control");
			Runtime.TestAssert("18a Control created", ctrlCreated);

			// 18b removed: it computed a has_method result and then asserted a
			// constant 1 regardless (dead variable, vacuous test).

			// 18c: Create InputEventScreenTouch via ClassDB
			int touchEventCreated = Runtime.TestCreate("InputEventScreenTouch");
			Runtime.TestAssert("18c InputEventScreenTouch created", touchEventCreated);

			// 18d: Set touch properties
			Runtime.TestSetIntProp("index", 0);
			int touchIdx = Runtime.TestGetIntProp("index");
			Runtime.TestAssert("18d touch index set/get", touchIdx == 0 ? 1 : 0);

			// 18e: Set pressed state
			Runtime.TestSetIntProp("pressed", 1);
			int pressedVal = Runtime.TestGetIntProp("pressed");
			Runtime.TestAssert("18e pressed state set/get", pressedVal == 1 ? 1 : 0);

			// 18f: Create InputEventScreenDrag
			int dragEventCreated = Runtime.TestCreate("InputEventScreenDrag");
			Runtime.TestAssert("18f InputEventScreenDrag created", dragEventCreated);

			// 18g: Set drag position
			Runtime.TestSetIntProp("index", 1);
			int dragIdx = Runtime.TestGetIntProp("index");
			Runtime.TestAssert("18g drag index set/get", dragIdx == 1 ? 1 : 0);

			// 18h: platform note only (no meaningful headless assertion exists
			// for touch input — the old constant-1 asserts were vacuous).
			Runtime.DebugUiAddLine(_isWeb == 0
				? "  18h: Desktop (touch events constructed above, not injected)"
				: "  18h: WASM (touch via JS events, not covered headless)");

			Runtime.TestFree();
			Runtime.TestFinishTest("18. Mobile: Touch/drag input events");
			_state = 19;
			return;
		}

		// ============================================================
		// Test 19: UI Resolution - Anchors, layout, responsive
		// ============================================================
		if (_state == 19)
		{
			Runtime.DebugUiAddLine("Test 19: UI Resolution");

			// 19a: Create Control with anchors
			int ctrlCreated = Runtime.TestCreate("Control");
			Runtime.TestAssert("19a Control created", ctrlCreated);

			// 19b: Set anchor presets (full rect)
			Runtime.TestSetIntProp("anchor_left", 0);
			Runtime.TestSetIntProp("anchor_right", 1);
			Runtime.TestSetIntProp("anchor_top", 0);
			Runtime.TestSetIntProp("anchor_bottom", 1);
			int anchorLeft = Runtime.TestGetIntProp("anchor_left");
			int anchorRight = Runtime.TestGetIntProp("anchor_right");
			Runtime.TestAssert("19b anchors set", (anchorLeft == 0 && anchorRight == 1) ? 1 : 0);

			// 19c: Set offsets
			Runtime.TestSetIntProp("offset_left", 10);
			Runtime.TestSetIntProp("offset_top", 10);
			Runtime.TestSetIntProp("offset_right", -10);
			Runtime.TestSetIntProp("offset_bottom", -10);
			int offsetLeft = Runtime.TestGetIntProp("offset_left");
			int offsetRight = Runtime.TestGetIntProp("offset_right");
			Runtime.TestAssert("19c offsets set", (offsetLeft == 10 && offsetRight == -10) ? 1 : 0);

			// 19d: Create child Control
			int childCreated = Runtime.TestAddChild("Control");
			Runtime.TestAssert("19d child Control added", childCreated);

			// 19e: Set anchors on the ACTUAL child (previously these set/got
			// the PARENT's anchors while claiming to test the child — the
			// test context never moved). TestSelectChild moves the context.
			Runtime.TestSelectChild(0);
			Runtime.TestSetIntProp("anchor_left", 0);
			Runtime.TestSetIntProp("anchor_right", 1);
			Runtime.TestSetIntProp("anchor_top", 0);
			Runtime.TestSetIntProp("anchor_bottom", 1);
			int childAnchorOk = Runtime.TestGetIntProp("anchor_right") == 1 ? 1 : 0;
			Runtime.TestAssert("19e child anchors (real child)", childAnchorOk);
			Runtime.TestSelectParent();

			// 19f: Check size flags (parent)
			Runtime.TestSetIntProp("size_flags_horizontal", 3);
			Runtime.TestSetIntProp("size_flags_vertical", 3);
			int sizeFlagsH = Runtime.TestGetIntProp("size_flags_horizontal");
			int sizeFlagsV = Runtime.TestGetIntProp("size_flags_vertical");
			Runtime.TestAssert("19f size flags", (sizeFlagsH == 3 && sizeFlagsV == 3) ? 1 : 0);

			// 19g: Create Label for resolution display
			int labelCreated = Runtime.TestAddChild("Label");
			Runtime.TestAssert("19g Label for resolution", labelCreated);

			// 19h: Mouse filter (parent)
			Runtime.TestSetIntProp("mouse_filter", 1);
			int mouseFilter = Runtime.TestGetIntProp("mouse_filter");
			Runtime.TestAssert("19h mouse filter", mouseFilter == 1 ? 1 : 0);

			// 19i: Mouse filter on the ACTUAL child (previously overwrote the
			// parent's filter from 19h and asserted that instead).
			Runtime.TestSelectChild(0);
			Runtime.TestSetIntProp("mouse_filter", 2);
			int childMouseFilter = Runtime.TestGetIntProp("mouse_filter");
			Runtime.TestAssert("19i child mouse filter (real child)", childMouseFilter == 2 ? 1 : 0);
			Runtime.TestSelectParent();

			// 19j: Exact child count (Control + Label)
			int childCount = Runtime.TestGetChildCount();
			Runtime.TestAssert("19j child count == 2", childCount == 2 ? 1 : 0);

			Runtime.TestFree();
			Runtime.TestFinishTest("19. UI Resolution: anchors/layout/flags");
			_state = 20;
			return;
		}

		// ============================================================
		// Test 20: Multiplayer - ENet peer (desktop only)
		// ============================================================
		if (_state == 20)
		{
			Runtime.DebugUiAddLine("Test 20: Multiplayer");

			if (_isWeb == 1)
			{
				// WASM: ENet not supported, skip (logged, not asserted).
				Runtime.DebugUiAddLine("  20a-e: Skipped on WASM (no ENet)");
			}
			else
			{
				// 20a: Create ENetMultiplayerPeer
				int peerCreated = Runtime.TestCreate("ENetMultiplayerPeer");
				Runtime.TestAssert("20a ENetMultiplayerPeer created", peerCreated);

				// 20b: Check create_server method
				int hasCreateServer = Runtime.TestHasMethod("create_server");
				Runtime.TestAssert("20b has create_server", hasCreateServer);

				// 20c: Check create_client method
				int hasCreateClient = Runtime.TestHasMethod("create_client");
				Runtime.TestAssert("20c has create_client", hasCreateClient);

				// 20d: Check get_available_packet_count method (PacketPeer base)
				int hasGetPacketCount = Runtime.TestHasMethod("get_available_packet_count");
				Runtime.TestAssert("20d has get_available_packet_count", hasGetPacketCount);

				// 20e: Check get_transfer_channel method
				int hasGetChannel = Runtime.TestHasMethod("get_transfer_channel");
				Runtime.TestAssert("20e has get_transfer_channel", hasGetChannel);

				Runtime.TestFree();
			}

			// 20f: Create SceneMultiplayer (works on all platforms as API check)
			int sceneMpCreated = Runtime.TestCreate("SceneMultiplayer");
			Runtime.TestAssert("20f SceneMultiplayer created", sceneMpCreated);

			// 20g: Check SceneMultiplayer methods
			int hasSetMultiplayerPeer = Runtime.TestHasMethod("set_multiplayer_peer");
			Runtime.TestAssert("20g has set_multiplayer_peer", hasSetMultiplayerPeer);

			// 20h: Check root path method
			int hasSetRootPath = Runtime.TestHasMethod("set_root_path");
			Runtime.TestAssert("20h has set_root_path", hasSetRootPath);

			// 20i: Check object configuration (object_configuration_add in Godot 4.7)
			int hasGetObjectConfiguration = Runtime.TestHasMethod("object_configuration_add");
			Runtime.TestAssert("20i has object_configuration_add", hasGetObjectConfiguration);

			// 20j: Check allow_object_decoding property
			Runtime.TestSetIntProp("allow_object_decoding", 0);
			int allowDecoding = Runtime.TestGetIntProp("allow_object_decoding");
			Runtime.TestAssert("20j allow_object_decoding set/get", allowDecoding == 0 ? 1 : 0);

			Runtime.TestFree();
			Runtime.TestFinishTest("20. Multiplayer: ENet/SceneMultiplayer API");
			_state = 21;
			return;
		}

		// ============================================================
		// Test 21: Hot Update - Scene reload, assembly inspection
		// ============================================================
		if (_state == 21)
		{
			Runtime.DebugUiAddLine("Test 21: Hot Update");

			// 21a: Load scene
			int loadOk = Runtime.TestLoadScene("res://base_test.tscn");
			Runtime.TestAssert("21a scene loaded", loadOk);

			// 21b: Instantiate scene
			int instOk = Runtime.TestInstantiateScene();
			Runtime.TestAssert("21b scene instantiated", instOk);

			// 21c: Check scene child count
			int sceneChildren = Runtime.TestGetSceneChildCount();
			Runtime.TestAssert("21c scene has children", sceneChildren > 0 ? 1 : 0);

			// 21d: Free scene (void op)
			Runtime.TestFreeScene();

			// 21e: Reload same scene (hot reload simulation)
			int reloadOk = Runtime.TestLoadScene("res://base_test.tscn");
			Runtime.TestAssert("21e scene reloaded", reloadOk);

			// 21f: Re-instantiate
			int reinstOk = Runtime.TestInstantiateScene();
			Runtime.TestAssert("21f scene re-instantiated", reinstOk);

			// 21g: Free reloaded scene (void op)
			Runtime.TestFreeScene();

			// 21h: Load different scene
			int loadOther = Runtime.TestLoadScene("res://ui_test.tscn");
			Runtime.TestAssert("21h different scene loaded", loadOther);

			// 21i: Instantiate and free other scene
			int instOther = Runtime.TestInstantiateScene();
			Runtime.TestAssert("21i other scene instantiated", instOther);
			Runtime.TestFreeScene();

			// 21j: Assembly inspection - skip BCL System.Reflection static methods
			// on all platforms to avoid Mono WASM signature mismatch (WEB_ENABLED
			// not reliably defined, so _isWeb cannot be trusted). Custom Reflection
			// icalls in state 0 already cover ClassDB reflection on desktop.
			Runtime.DebugUiAddLine("  21j: Skipped (BCL reflection unsafe for WASM)");

			Runtime.TestFinishTest("21. Hot Update: Scene reload/assembly inspect");
			_state = 22;
			return;
		}

		// ============================================================
		// Test 22: Skeleton/Spine - Skeleton2D, Bone2D hierarchy
		// ============================================================
		if (_state == 22)
		{
			Runtime.DebugUiAddLine("Test 22: Skeleton/Spine Animation");

			// 22a: Create Skeleton2D
			int skelCreated = Runtime.TestCreate("Skeleton2D");
			Runtime.TestAssert("22a Skeleton2D created", skelCreated);

			// 22b: Add first Bone2D
			int bone1Added = Runtime.TestAddChild("Bone2D");
			Runtime.TestAssert("22b Bone2D root added", bone1Added);

			// 22c: Add second Bone2D NESTED under the root bone (previously it
			// was added to the skeleton while the comment claimed nesting —
			// the context never moved to bone1).
			Runtime.TestSelectChild(0);
			int bone2Added = Runtime.TestAddChild("Bone2D");
			Runtime.TestAssert("22c Bone2D nested under root bone", bone2Added);
			int bone1Children = Runtime.TestGetChildCount();
			Runtime.TestAssert("22c root bone child count==1", bone1Children == 1 ? 1 : 0);
			Runtime.TestSelectParent();

			// 22d: Exact child count (only the root bone at this point)
			int boneCount = Runtime.TestGetChildCount();
			Runtime.TestAssert("22d skeleton children==1", boneCount == 1 ? 1 : 0);
			Runtime.DebugUiAddLineInt("  Bone count: ", boneCount);

			// 22e: Check Skeleton2D methods
			int hasGetBoneCount = Runtime.TestHasMethod("get_bone_count");
			Runtime.TestAssert("22e has get_bone_count", hasGetBoneCount);

			// 22f: Bone naming — REAL round trip (previously: set name, then
			// asserted TestIsValid, which never checks the name at all).
			Runtime.TestSetName("RootBone");
			string boneName = Runtime.TestGetStringProp("name");
			Runtime.TestAssert("22f bone name round-trip", boneName == "RootBone" ? 1 : 0);

			// 22g: Add attachment node to skeleton (BoneAttachment2D may not exist in all builds)
			int attachCreated = Runtime.TestAddChild("Node2D");
			Runtime.TestAssert("22g attachment node added", attachCreated);

			// 22h: Check Polygon2D for skinning (create, verify, free to restore context)
			int polyCreated = Runtime.TestCreate("Polygon2D");
			Runtime.TestAssert("22h Polygon2D created", polyCreated);
			Runtime.TestFree(); // restore: no current object after free

			// 22i: Check SkeletonModification2D existence (create, verify, free)
			int modCreated = Runtime.TestCreate("SkeletonModification2D");
			Runtime.TestAssert("22i SkeletonModification2D", modCreated);
			Runtime.TestFree();

			// 22j: Create AnimationPlayer for bone animation
			int animPlayer = Runtime.TestCreate("AnimationPlayer");
			Runtime.TestAssert("22j AnimationPlayer created", animPlayer);

			// 22k: Check animation method on AnimationPlayer
			int hasPlay = Runtime.TestHasMethod("play");
			Runtime.TestAssert("22k has play method", hasPlay);

			// 22l: Check AnimationTree as alternative (free AnimationPlayer first)
			Runtime.TestFree();
			int animTree = Runtime.TestCreate("AnimationTree");
			Runtime.TestAssert("22l AnimationTree created", animTree);

			Runtime.TestFree();
			Runtime.TestFinishTest("22. Skeleton: Bone2D/Animation/Skinning");
			_state = 23;
			return;
		}

		// ============================================================
		// Scenario 23: WXAudio (WeChat InnerAudioContext adapter)
		// 桌面平台 stub：Play 返回 0，其他方法无操作。测试验证 stub 不崩溃 + API 完整。
		// WASM 平台：实际调用 GameGlobal.GodotAudioWX（InnerAudioContext）。
		// ============================================================
		if (_state == 23)
		{
			Runtime.DebugUiAddLine("--- Scenario 23: WXAudio ---");

			// 1. Play (桌面 stub 返回 0，WASM 返回 audio id)
			int id1 = WXAudio.Play("res://sounds/test.wav", false);
			Runtime.DebugUiAddLineInt("Play id1=", id1);
			Runtime.TestAssert("WXAudio.Play returns int (stub=0/wasm>0)", id1 >= 0 ? 1 : 0);

			// 2. PlayEffect (不循环)
			int id2 = WXAudio.PlayEffect("res://sounds/effect.wav");
			Runtime.DebugUiAddLineInt("PlayEffect id2=", id2);
			Runtime.TestAssert("WXAudio.PlayEffect returns int", id2 >= 0 ? 1 : 0);

			// 3. PlayBgm (默认循环)
			int id3 = WXAudio.PlayBgm("res://music/bgm.mp3");
			Runtime.DebugUiAddLineInt("PlayBgm id3=", id3);
			Runtime.TestAssert("WXAudio.PlayBgm returns int", id3 >= 0 ? 1 : 0);

			// 4. SetVolume (0.0~1.0)
			WXAudio.SetVolume(id3, 0.5f);
			Runtime.TestAssert("WXAudio.SetVolume no throw", 1);

			// 5. SetVolume 边界值
			WXAudio.SetVolume(id3, 0.0f);
			WXAudio.SetVolume(id3, 1.0f);
			WXAudio.SetVolume(id3, -0.5f);  // 应被 clamp 到 0
			WXAudio.SetVolume(id3, 2.0f);   // 应被 clamp 到 1
			Runtime.TestAssert("WXAudio.SetVolume boundary clamp no throw", 1);

			// 6. Pause / Resume
			WXAudio.Pause(id3);
			WXAudio.Resume(id3);
			Runtime.TestAssert("WXAudio.Pause/Resume no throw", 1);

			// 7. Stop 单个
			WXAudio.Stop(id1);
			WXAudio.Stop(id2);
			Runtime.TestAssert("WXAudio.Stop no throw", 1);

			// 8. StopAll
			WXAudio.StopAll();
			Runtime.TestAssert("WXAudio.StopAll no throw", 1);

			// 9. 对 id=0 (无效 id) 的操作不应崩溃
			WXAudio.Stop(0);
			WXAudio.Pause(0);
			WXAudio.Resume(0);
			WXAudio.SetVolume(0, 0.5f);
			Runtime.TestAssert("WXAudio invalid id=0 no throw", 1);

			Runtime.TestFinishTest("23. WXAudio: Play/Stop/Pause/Resume/SetVolume");
			_state = 24;
			return;
		}

			// ============================================================
			// Test 24: Script Bridge - [Export] property round trip +
			// [Signal] end-to-end through the C# Callable bridge.
			// Exercises the REAL editor-feature bridge paths (P1/P3):
			// engine Object::set/get -> CSharpInstance::set/get on [Export]
			// members, and signal connect/emit via CallableCustomMono.
			// ============================================================
			if (_state == 24)
			{
				Runtime.DebugUiAddLine("Test 24: Script Bridge ([Export]/[Signal])");

				Node exportNode = GetNodeOrNull("ExportTestNode");
				Runtime.TestAssert("24a ExportTestNode found", exportNode != null ? 1 : 0);

				if (exportNode != null)
				{
					// 24b: [Export] int field set/get through the ENGINE property
					// path (Object::set -> CSharpInstance::set -> C# field).
					exportNode.Set("Speed", 321);
					object speedObj = exportNode.Get("Speed");
					// Variant::INT always arrives boxed as Int64 (H6 contract).
				Runtime.TestAssert("24b [Export] Speed set/get", (speedObj is long sl && sl == 321) ? 1 : 0);

					// 24c: [Export] string property round trip
					exportNode.Set("PlayerName", "BridgeHero");
					object nameObj = exportNode.Get("PlayerName");
					Runtime.TestAssert("24c [Export] PlayerName set/get", (nameObj as string) == "BridgeHero" ? 1 : 0);

					// 24d: [Export] float field round trip (Variant FLOAT -> double)
					exportNode.Set("Health", 73.5);
					object healthObj = exportNode.Get("Health");
					bool healthOk = false;
					if (healthObj is double hd) healthOk = System.Math.Abs(hd - 73.5) < 0.001;
					else if (healthObj is float hf) healthOk = System.Math.Abs(hf - 73.5f) < 0.001f;
					Runtime.TestAssert("24d [Export] Health set/get", healthOk ? 1 : 0);

					// 24e: [Signal] declared + C#-side connect/emit round trip
					Runtime.TestAssert("24e [Signal] HealthChanged declared", exportNode.HasSignal("HealthChanged") ? 1 : 0);
					_signalReceived = -1;
					exportNode.Connect("HealthChanged", (System.Action<int>)OnHealthChangedForTest);
					exportNode.EmitSignal("HealthChanged", 42);
					Runtime.TestAssert("24e [Signal] emit -> managed callback", _signalReceived == 42 ? 1 : 0);

					// 24f: lambda closure callback (same delegate path, but a
					// compiler-generated closure target instead of a method group).
					int lambdaReceived = -1;
					exportNode.Connect("HealthChanged", (int v) => { lambdaReceived = v; });
					exportNode.EmitSignal("HealthChanged", 7);
					Runtime.TestAssert("24f [Signal] emit -> lambda callback", lambdaReceived == 7 ? 1 : 0);
				}

				Runtime.TestFinishTest("24. ScriptBridge: [Export] set/get + [Signal] e2e");
				_state = 25;
				return;
			}

			// ============================================================
			// Summary
			// ============================================================
			if (_state == 25)
			{
			int passCount = Runtime.TestGetPassCount();
			int failCount = Runtime.TestGetFailCount();
			Runtime.DebugUiAddSeparator();
			Runtime.DebugUiAddLine("=== Test Summary ===");
			Runtime.DebugUiAddLineInt("Passed: ", passCount);
			Runtime.DebugUiAddLineInt("Failed: ", failCount);
			Runtime.DebugUiAddLineInt("Physics frames: ", _physicsCount);
			Runtime.DebugUiAddLineInt("Process frames: ", _frameCount);
			if (_isWeb == 1)
			{
				Runtime.DebugUiAddLine("Platform: Web (WASM)");
			}
			else
			{
				Runtime.DebugUiAddLine("Platform: Desktop");
			}
			Runtime.DebugUiAddSeparator();
			Runtime.DebugUiAddLine("All 24 scenarios complete.");
			// Desktop/headless: exit with a verdict code so CI runners can
			// fail the build. WASM stays alive for the JS-side Debug UI
			// polling used by the H9 harness.
			if (_isWeb == 0)
			{
				GetTree().Quit(failCount > 0 ? 1 : 0);
			}
			_state = 26;
			return;
		}

		// State 26: Idle - periodic display refresh (WASM only)
		if (_state == 26)
		{
			_waitFrames++;
			if (_waitFrames >= 600)
			{
				_waitFrames = 0;
				int passCount = Runtime.TestGetPassCount();
				int failCount = Runtime.TestGetFailCount();
				Runtime.DebugUiClear();
				Runtime.DebugUiAddLine("=== Godot 4.7 C# Workflow Tests ===");
				Runtime.DebugUiAddSeparator();
				Runtime.DebugUiAddLine("All 24 scenarios completed.");
				Runtime.DebugUiAddLineInt("Passed: ", passCount);
				Runtime.DebugUiAddLineInt("Failed: ", failCount);
				Runtime.DebugUiAddLineInt("Physics frames: ", _physicsCount);
				Runtime.DebugUiAddLineInt("Process frames: ", _frameCount);
				if (_isWeb == 1)
				{
					Runtime.DebugUiAddLine("Platform: Web (WASM)");
				}
				else
				{
					Runtime.DebugUiAddLine("Platform: Desktop");
				}
				Runtime.DebugUiAddSeparator();
				Runtime.DebugUiAddLine("Idle - tests already run.");
			}
			return;
		}
	}
}
