models/weapons/mdriver/mdriver
{
	cull disable
	{
		map models/weapons/mdriver/mdriver.jpg
		rgbGen lightingDiffuse
	}
	{
		map models/weapons/mdriver/mdriver.jpg
		blendFunc GL_SRC_ALPHA GL_ONE
		detail
		alphaGen lightingSpecular
	}
	{
		map models/buildables/mgturret/ref_map.jpg
		blendFunc GL_DST_COLOR GL_ONE
		detail
		tcGen environment
	}
	{
		map models/weapons/mdriver/mdriver_glow.jpg
		blendFunc add
		rgbGen wave sawtooth .6 .1 0 7
		detail
	}
}

models/weapons/mdriver/glow
{

	cull disable
	{
		map models/weapons/mdriver/glow.jpg
		blendfunc GL_ONE GL_ONE
		tcMod scroll -9.0 9.0
	}
}
