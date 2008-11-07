models/weapons/prifle/prifle
{
	cull disable
	{
		map models/weapons/prifle/prifle.jpg
		rgbGen lightingDiffuse
	}
	{
		map models/weapons/prifle/prifle_glow.jpg
		blendFunc add
		rgbGen wave sawtooth .6 .1 0 7
	}
}
