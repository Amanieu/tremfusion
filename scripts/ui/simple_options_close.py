#!/usr/bin/env python
import ui, python
for x in xrange(100):
	menu = ui.Menu(x)
	if menu.window.name == "simple_options":
		menu_id = x
		break
def close_simple_options(data):
	import ui, common
	menu = data["menu"]
	window = menu.window
	rect = window.rect
	rect.y += 15
	menu.UpdatePosition()
	if rect.y >= 640.0:
		ui.RemoveMoveFunc(data["func_id"])
		menu.Close()

data = python.ScHash({"menu": menu})
func_id = ui.AddMoveFunc(close_simple_options, data)
data["func_id"] = func_id
