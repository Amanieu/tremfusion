models/weapons/lgun/lgun
{
	cull disable
	{
		map models/weapons/lgun/lgun.jpg
		rgbGen lightingDiffuse
	}
	{
		map models/weapons/lgun/lgun_glow.jpg
		blendFunc add
		rgbGen wave sawtooth .6 .1 0 7
	}
}
