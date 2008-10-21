models/weapons/portal/flash
{
  sort additive
  cull disable
  {
    map	models/weapons/portal/flash.jpg
    blendfunc GL_ONE GL_ONE
  }
}

gfx/portal/blue
{  
  cull disable
  {
    animmap 24 gfx/portal/blue_1.jpg gfx/portal/blue_2.jpg gfx/portal/blue_3.jpg gfx/portal/blue_4.jpg
    blendFunc GL_ONE GL_ONE
  }
}

gfx/portal/red
{  
  cull disable
  {
    animmap 24 gfx/portal/red_1.jpg gfx/portal/red_2.jpg gfx/portal/red_3.jpg gfx/portal/red_4.jpg
    blendFunc GL_ONE GL_ONE
  }
}

models/weapons/portal/trail_s
{
	cull disable
	{
		map models/weapons/portal/bolt.jpg
		blendfunc add
		rgbGen vertex
		tcMod scroll 0.2 0
	}
	{
		map models/weapons/portal/bolt.jpg
		blendfunc add
		rgbGen wave sin 0 1 0 5 
		tcMod scroll 0.5 0
		tcMod scale -1 1
	}
}
