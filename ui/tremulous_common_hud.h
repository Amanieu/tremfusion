#define BORDER    10

#define STAT_W    45
#define STAT_H    22
#define STAT_X    (W-(BORDER+STAT_W))

#define CONSOLE_W (W-((3*BORDER)+STAT_W))
#define CONSOLE_H 180
#define MAIN_W    (W-(2*BORDER))

//CONSOLE
itemDef
{
  name "console"
  rect BORDER BORDER CONSOLE_W CONSOLE_H
  aspectBias ALIGN_LEFT
  style WINDOW_STYLE_EMPTY
  visible MENU_TRUE
  decoration
  forecolor 0.93 0.93 0.92 1
  textalign ALIGN_LEFT
  textvalign VALIGN_TOP
  textscale 0.35
  textstyle ITEM_TEXTSTYLE_SHADOWED
  ownerdraw CG_CONSOLE
}

//TUTORIAL
itemDef
{
  name "tutorial"
  rect BORDER 250 MAIN_W 180
  aspectBias ALIGN_LEFT
  style WINDOW_STYLE_EMPTY
  visible MENU_TRUE
  decoration
  forecolor 1 1 1 0.35
  textalign ALIGN_LEFT
  textvalign VALIGN_TOP
  textscale 0.3
  textstyle ITEM_TEXTSTYLE_NORMAL
  ownerdraw CG_TUTORIAL
}

//FPS
itemDef
{
  name "fps"
  rect STAT_X BORDER STAT_W STAT_H
  aspectBias ALIGN_RIGHT
  style WINDOW_STYLE_EMPTY
  visible MENU_TRUE
  decoration
  forecolor COMMON_HUD_R COMMON_HUD_G COMMON_HUD_B 1
  textalign ALIGN_RIGHT
  textvalign VALIGN_CENTER
  textscale 0.3
  textstyle ITEM_TEXTSTYLE_NORMAL
  ownerdraw CG_FPS
}
//TIMER
itemDef
{
  name "timer"
  rect STAT_X ((2*BORDER)+STAT_H) STAT_W STAT_H
  aspectBias ALIGN_RIGHT
  style WINDOW_STYLE_EMPTY
  visible MENU_TRUE
  decoration
  forecolor COMMON_HUD_R COMMON_HUD_G COMMON_HUD_B 1
  textalign ALIGN_RIGHT
  textvalign VALIGN_CENTER
  textscale 0.3
  textstyle ITEM_TEXTSTYLE_NORMAL
  ownerdraw CG_TIMER
}
//LAGOMETER
itemDef
{
  name "lagometer"
  rect STAT_X ((3*BORDER)+(2*STAT_H)) STAT_W STAT_H
  aspectBias ALIGN_RIGHT
  style WINDOW_STYLE_EMPTY
  visible MENU_TRUE
  decoration
  forecolor COMMON_HUD_R COMMON_HUD_G COMMON_HUD_B 1
  textscale 0.3
  ownerdraw CG_LAGOMETER
}
//CLOCK
itemDef
{
  name "clock"
  rect STAT_X ((4*BORDER)+(3*STAT_H)) STAT_W STAT_H
  aspectBias ALIGN_RIGHT
  style WINDOW_STYLE_EMPTY
  visible MENU_TRUE
  decoration
  forecolor COMMON_HUD_R COMMON_HUD_G COMMON_HUD_B 1
  textalign ALIGN_RIGHT
  textvalign VALIGN_CENTER
  textscale 0.3
  textstyle ITEM_TEXTSTYLE_NORMAL
  ownerdraw CG_CLOCK
}
//DEMO STATE
itemDef
{
  name "demoRecording"
  rect (STAT_X+(STAT_W-32)) ((5*BORDER)+(4*STAT_H)) 32 32
  aspectBias ALIGN_RIGHT
  style WINDOW_STYLE_EMPTY
  visible MENU_TRUE
  decoration
  forecolor 1 0 0 1
  textscale 0.3
  ownerdraw CG_DEMO_RECORDING
  background "ui/assets/neutral/circle.tga"
}
itemDef
{
  name "demoPlayback"
  rect (STAT_X+(STAT_W-32)) ((5*BORDER)+(4*STAT_H)) 32 32
  aspectBias ALIGN_RIGHT
  style WINDOW_STYLE_EMPTY
  visible MENU_TRUE
  decoration
  forecolor 1 1 1 1
  textscale 0.3
  ownerdraw CG_DEMO_PLAYBACK
  background "ui/assets/forwardarrow.tga"
}

//SNAPSHOT
itemDef
{
  name "snapshot"
  rect BORDER (H-(BORDER+STAT_H)) MAIN_W STAT_H
  aspectBias ALIGN_LEFT
  style WINDOW_STYLE_EMPTY
  visible MENU_TRUE
  decoration
  forecolor COMMON_HUD_R COMMON_HUD_G COMMON_HUD_B 1
  textalign ALIGN_LEFT
  textvalign VALIGN_CENTER
  textscale 0.4
  textstyle ITEM_TEXTSTYLE_NORMAL
  ownerdraw CG_SNAPSHOT
}

//PLAYER NAME
itemDef
{
  name "playername"
  rect 200 275 240 25
  aspectBias ALIGN_CENTER
  visible MENU_TRUE
  decoration
  textScale .5
  ownerdraw CG_PLAYER_CROSSHAIRNAMES
  textstyle ITEM_TEXTSTYLE_SHADOWED
}

//SQUAD MARKERS
itemDef
{
  name "squad-markers"
  visible MENU_TRUE
  forecolor COMMON_HUD_R COMMON_HUD_G COMMON_HUD_B 0.5
  ownerdraw CG_SQUAD_MARKERS
}

