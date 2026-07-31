extends SceneTree
func _init():
    var ed = EditorInterface.new() if Engine.has_singleton(\"EditorInterface\") else null
    print(\"Setting Android SDK path...\")
    quit()
