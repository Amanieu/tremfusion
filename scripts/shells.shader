models/weapons/shells/rifle-shell
{
	{
		map models/weapons/shells/rifle-shell.jpg
		blendFunc GL_ONE GL_ZERO
		rgbGen identity
	}
	{
		map $lightmap
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
	{
		map models/weapons/shells/fx.tga
		blendFunc GL_DST_COLOR GL_SRC_COLOR
		detail
		rgbGen identity
		tcGen environment
		tcMod scale 3 3
	}
}
