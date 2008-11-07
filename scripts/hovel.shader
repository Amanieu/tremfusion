models/buildables/hovel/hovel
{
	{
		map models/buildables/hovel/hovel.tga
		rgbGen lightingDiffuse
	}
	{
		map models/buildables/hovel/hovel.tga
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

models/buildables/hovel/hovel_front
{
	{
		map models/buildables/hovel/hovel_front.tga
		rgbGen lightingDiffuse
	}
	{
		map models/buildables/hovel/hovel_front.tga
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
