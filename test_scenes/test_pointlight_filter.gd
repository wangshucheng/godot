extends Node2D

var test_label: Label

func _ready():
	# 创建测试说明
	test_label = Label.new()
	test_label.text = "PointLight2D NormalMap Filter Test\n\nFilter Mode: Nearest (should show pixelated lighting)\n\nPress 1: Set to Nearest\nPress 2: Set to Linear\nPress 3: Set to Default"
	test_label.position = Vector2(20, 20)
	test_label.add_theme_color_override("font_color", Color.WHITE)
	add_child(test_label)
	
	# 获取 PointLight2D
	var light = $PointLight2D
	print("Initial texture_filter: ", light.get_texture_filter())
	
	# 设置初始为 Nearest (1)
	light.set_texture_filter(CanvasItem.TEXTURE_FILTER_NEAREST)
	update_label(light)

func _input(event):
	if event is InputEventKey and event.pressed:
		var light = $PointLight2D
		
		if event.keycode == KEY_1:
			light.set_texture_filter(CanvasItem.TEXTURE_FILTER_NEAREST)
			print("Set to NEAREST")
		elif event.keycode == KEY_2:
			light.set_texture_filter(CanvasItem.TEXTURE_FILTER_LINEAR)
			print("Set to LINEAR")
		elif event.keycode == KEY_3:
			light.set_texture_filter(CanvasItem.TEXTURE_FILTER_PARENT_NODE)
			print("Set to DEFAULT")
		
		update_label(light)

func update_label(light: PointLight2D):
	var filter_names = ["Default", "Nearest", "Linear"]
	var filter = light.get_texture_filter()
	var filter_name = filter_names[filter] if filter < filter_names.size() else str(filter)
	test_label.text = "PointLight2D NormalMap Filter Test\n\nCurrent Filter: %s\n\nPress 1: Nearest\nPress 2: Linear\nPress 3: Default" % filter_name
