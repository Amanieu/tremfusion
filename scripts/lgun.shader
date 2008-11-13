models/weapons/lgun/lgun
{
	cull disable
	{
		map models/weapons/lgun/lgun.jpg
		rgbGen lightingDiffuse
	}
	{
		map models/weapons/lgun/lgun.jpg
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
		map models/weapons/lgun/lgun_glow.jpg
		blendFunc add
		rgbGen wave sawtooth .6 .1 0 7
		detail
	}
}
