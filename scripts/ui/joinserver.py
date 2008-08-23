#!/usr/bin/env python
import ui, python
for x in xrange(100):
  menu = ui.Menu(x)
  if menu.window.name == "joinserver":
    join_menu = menu
  elif menu.window.name == "main":
    main_menu = menu
  elif not menu.window.name:
    break
def animate_joinserver(data):
  import ui, common
  main_menu = data["main_menu"]
  join_menu = data["join_menu"]
  main_rect = main_menu.window.rect
  join_rect = join_menu.window.rect
  if not data["setup"]:
    join_rect.x = 640.0
    data["setup"] = 1
  main_rect.x -= 10
  join_rect.x -= 10
  main_menu.UpdatePosition()
  join_menu.UpdatePosition()
  if join_rect.x == 0 and data["setup"]:
    ui.RemoveMoveFunc(data["func_id"])

animate_joinserver_data = python.ScHash({"main_menu": main_menu, "join_menu": join_menu, "setup": 0})
func_id = ui.AddMoveFunc(animate_joinserver, animate_joinserver_data)
animate_joinserver_data["func_id"] = func_id
