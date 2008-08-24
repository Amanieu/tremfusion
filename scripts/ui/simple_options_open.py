#!/usr/bin/env python
import ui, python
for x in xrange(100):
	menu = ui.Menu(x)
	if menu.window.name == "simple_options":
		menu_id = x
		break
print menu.window.rect.x, menu.window.rect.y, menu.window.rect.h, menu.window.rect.w
print "Menu id: %d" % menu_id
def animate_simple_options(data):
	import ui, common
	menu = data["menu"]
	window = menu.window
	rect = window.rect
	if not data["setup"]:
		rect.y = 0- rect.h
		data["setup"] = 1
	rect.y += 15
	menu.UpdatePosition()
	if rect.y>=50 and data["setup"]:
		ui.RemoveMoveFunc(data["func_id"])

animate_simple_options_data = python.ScHash({"menu": menu, "setup": 0})
func_id = ui.AddMoveFunc(animate_simple_options, animate_simple_options_data)
animate_simple_options_data["func_id"] = func_id
