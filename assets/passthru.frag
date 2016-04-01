#version 150
// shadertoy specific
uniform vec3      	iResolution; 			// viewport resolution (in pixels)
uniform sampler2D 	iChannel0; 				// input channel 0

out vec4 oColor;

void main( void )
{
	vec2 uv = gl_FragCoord.xy / iResolution.xy;
	oColor 		= texture2D(iChannel0, uv);
}
