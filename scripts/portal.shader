models/weapons/portalgun/flash
{
  sort additive
  cull disable
  {
    map	models/weapons/portalgun/flash.jpg
    blendfunc GL_ONE GL_ONE
  }
}

gfx/portalgun/blue
{  
  cull disable
  {
    animmap 24 gfx/portalgun/blue_1.jpg gfx/portalgun/blue_2.jpg gfx/portalgun/blue_3.jpg gfx/portalgun/blue_4.jpg
    blendFunc GL_ONE GL_ONE
  }
}

gfx/portalgun/red
{  
  cull disable
  {
    animmap 24 gfx/portalgun/red_1.jpg gfx/portalgun/red_2.jpg gfx/portalgun/red_3.jpg gfx/portalgun/red_4.jpg
    blendFunc GL_ONE GL_ONE
  }
}

models/weapons/portalgun/trail_s
{
	cull disable
	{
		map models/weapons/portalgun/bolt.jpg
		blendfunc add
		rgbGen vertex
		tcMod scroll 0.2 0
	}
	{
		map models/weapons/portalgun/bolt.jpg
		blendfunc add
		rgbGen wave sin 0 1 0 5 
		tcMod scroll 0.5 0
		tcMod scale -1 1
	}
}
