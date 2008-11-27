models/buildables/overmind/pod_strands
{
	cull disable
	{
		map models/buildables/eggpod/pod_strands.tga
		rgbGen lightingDiffuse
		alphaFunc GE128
		depthWrite
	}
	{
		map models/buildables/eggpod/pod_strands.tga
		blendFunc GL_SRC_ALPHA GL_ONE
		detail
		alphaGen lightingSpecular
		depthFunc equal
	}
	{
		map models/buildables/mgturret/ref_map.jpg
		blendFunc GL_DST_COLOR GL_ONE
		detail
		tcGen environment
		depthFunc equal
	}
}

models/buildables/overmind/over_spike
{
	{
		map models/buildables/overmind/over_spike.tga
		rgbGen lightingDiffuse
	}
	{
		map models/buildables/overmind/over_spike.tga
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

models/buildables/overmind/overhead
{
	{
		map models/buildables/overmind/overhead.jpg
		rgbGen lightingDiffuse
	}
	{
		map models/buildables/overmind/overhead.jpg
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

models/buildables/overmind/overmind
{
	{
		map models/buildables/overmind/overmind.jpg
		rgbGen lightingDiffuse
	}
	{
		map models/buildables/overmind/overmind.jpg
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

models/buildables/overmind/over_body
{
	{
		map models/buildables/overmind/over_body.jpg
		rgbGen lightingDiffuse
	}
	{
		map models/buildables/overmind/over_body.jpg
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
