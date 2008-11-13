models/buildables/acid_tube/acid_tube
{
	{
		map models/buildables/acid_tube/acid_tube.jpg
		rgbGen lightingDiffuse
	}
	{
		map models/buildables/acid_tube/acid_tube.jpg
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

models/buildables/acid_tube/acid_tube_inside
{
	{
		map models/buildables/acid_tube/acid_tube_inside.jpg
		rgbGen lightingDiffuse
	}
	{
		map models/buildables/acid_tube/acid_tube_inside.jpg
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
