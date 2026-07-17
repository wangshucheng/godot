extends Label

var _frame_count: int = 0

func _process(delta: float) -> void:
	_frame_count += 1
	if _frame_count % 60 == 0:
		text = "Hello from Godot 4.7 Mono! Frame: " + str(_frame_count)
