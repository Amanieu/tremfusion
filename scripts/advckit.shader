models/weapons/advckit/advckit
{
	cull disable
	{
		map models/weapons/advckit/advckit.jpg
		rgbGen lightingDiffuse
	}
	{
		map models/weapons/advckit/advckit.jpg
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
}

models/weapons/ackit/rep_cyl
{
	cull disable
	{
		map models/weapons/ackit/rep_cyl.jpg
		blendfunc add
		rgbGen lightingDiffuse
		tcMod scroll 0.2 0
	}
	{
		map models/weapons/ackit/lines2.jpg
		blendfunc add
		rgbGen identity
		tcMod scroll 0 -0.2
	}
}

models/weapons/ackit/particle
{
	cull disable
	{
		map models/weapons/ackit/particle.jpg
		blendfunc add
		rgbGen identity
		tcMod scroll 0.02 -0.4
	}
}

models/weapons/ackit/screen
{
	noPicMip
	{
		map models/weapons/ackit/screen.jpg
	}

	{
		map models/weapons/ackit/scroll.jpg
		blendfunc add
		tcMod scroll 10.0 -0.2
	}
	{
		map models/buildables/mgturret/ref_map.jpg
		blendFunc GL_DST_COLOR GL_ONE
		detail
		tcGen environment
	}
}


models/weapons/ackit/screen2
{
	noPicMip
	{
		map models/weapons/ackit/screen2.jpg
	}

	{
		map models/weapons/ackit/scroll2.jpg
		blendfunc add
		tcMod scroll 0.2 -10.0
	}
	{
		map models/buildables/mgturret/ref_map.jpg
		blendFunc GL_DST_COLOR GL_ONE
		detail
		tcGen environment
	}
}
