uniform vec3 iResolution;uniform sampler2D iChannel0;uniform float iZoom;
uniform float iTime;uniform float iTempoTime;uniform float iRatio;uniform float iVignette;uniform float iToggle;
uniform float iExposure;uniform float iSobel;uniform float iChromatic;uniform float iGreyScale;
uniform float iFlipV;uniform float iFlipH;uniform float iInvert;uniform float iTrixels;
uniform float iPixelate;uniform float iGlitch;
uniform float       iRedMultiplier;			// red multiplier 
uniform float       iGreenMultiplier;		// green multiplier 
uniform float       iBlueMultiplier;		// blue multiplier 
vec2  fragCoord = gl_FragCoord.xy;
float intensity(in vec4 c){return sqrt((c.x*c.x)+(c.y*c.y)+(c.z*c.z));}
vec4 sobel(float stepx, float stepy, vec2 center) {
	float tleft = intensity(texture(iChannel0,center + vec2(-stepx,stepy))); float left = intensity(texture(iChannel0,center + vec2(-stepx,0)));
	float bleft = intensity(texture(iChannel0,center + vec2(-stepx,-stepy))); float top = intensity(texture(iChannel0,center + vec2(0,stepy)));
	float bottom = intensity(texture(iChannel0,center + vec2(0,-stepy)));float tright = intensity(texture(iChannel0,center + vec2(stepx,stepy)));
	float right = intensity(texture(iChannel0,center + vec2(stepx,0)));float bright = intensity(texture(iChannel0,center + vec2(stepx,-stepy)));
	float x = tleft + 2.0*left + bleft - tright - 2.0*right - bright;float y = -tleft - 2.0*top - tright + bleft + 2.0 * bottom + bright;
	return vec4(sqrt((x*x) + (y*y)));
}
vec4 chromatic( vec2 uv ) {	
	vec2 offset = vec2(iChromatic / 36., .0);
	return vec4(texture(iChannel0, uv + offset.xy).r,  texture(iChannel0, uv).g, texture(iChannel0, uv + offset.yx).b, 1.0);
}
vec4 trixels( vec2 inUV, sampler2D tex )
{
	// trixels https://www.shadertoy.com/view/4lj3Dm
	vec4 rtn;
	
    float height = iResolution.x/(1.01 - iTrixels)/90.0;
    float halfHeight = height*0.5;
    float halfBase = height/sqrt(3.0);
    float base = halfBase*2.0;

    float screenX = gl_FragCoord.x;
    float screenY = gl_FragCoord.y;    

    float upSlope = height/halfBase;
    float downSlope = -height/halfBase;

    float oddRow = mod(floor(screenY/height),2.0);
    screenX -= halfBase*oddRow;
    
    float oddColumn = mod(floor(screenX/halfBase), 2.0);

    float localX = mod(screenX, halfBase);
    float localY = mod(screenY, height);

    if(oddColumn == 0.0 )
    {
        if(localY >= localX*upSlope)
        {
            screenX -= halfBase;
        }
    }
    else
    {
        if(localY <= height+localX*downSlope)
        {
            screenX -= halfBase;
        }
    }
    
    float startX = floor(screenX/halfBase)*halfBase;
    float startY = floor(screenY/height)*height;
    vec4 blend = vec4(0.0);
    for(float x = 0.0; x < 3.0; x += 1.0)
    {
        for(float y = 0.0; y < 3.0; y += 1.0)
        {
            vec2 screenPos = vec2(startX+x*halfBase,startY+y*halfHeight);
            vec2 uv1 = screenPos / iResolution.xy;
			uv1.y = 1.0 - uv1.y;
			blend += texture2D(tex, uv1);         
        }
    }
    rtn = (blend / 9.0);
   	return rtn;
}
// trixels end
// glitch begin
float glitchHash(float x)
{
	return fract(sin(x * 11.1753) * 192652.37862);
}
float glitchNse(float x)
{
	float fl = floor(x);
	return mix(glitchHash(fl), glitchHash(fl + 1.0), smoothstep(0.0, 1.0, fract(x)));
}
// glitch end
vec4 greyScale( vec4 colored )
{
   return vec4( (colored.r+colored.g+colored.b)/3.0 );
}
void main() {
	vec2 uv = gl_FragCoord.xy / iResolution.xy;
	// zoom centered
	if ( iZoom != 1.0 )
	{
	  float xZ = (uv.x - 0.5)*(1.0-iZoom)*2.0;
	  float yZ = (uv.y - 0.5)*(1.0-iZoom)*2.0;
	  vec2 cZ = vec2(xZ, yZ);
	  uv = uv+cZ;
	}
	// flip horizontally
	/*if (iFlipH > 0.0)
	{
		uv.x = 1.0 - uv.x;
	}*/
	// flip vertically
	if (iFlipV == 0.0)
	{
		uv.y = 1.0 - uv.y;
	}
	if ( iPixelate < 1.0 )
	{
		vec2 divs = vec2(iResolution.x * iPixelate / iResolution.y*60.0, iPixelate*60.0);
		uv = floor(uv * divs)/ divs;
	}
	// glitch
	if (iGlitch > 0.0) 
	{
		float s = iTempoTime * iRatio;
		float te = iTempoTime * 9.0 / 16.0;
		vec2 shk = (vec2(glitchNse(s), glitchNse(s + 11.0)) * 2.0 - 1.0) * exp(-5.0 * fract(te * 4.0)) * 0.1;
		uv += shk;		
	}
	vec4 t0 = texture(iChannel0, uv);
	vec4 c = vec4(0.0);
	if (iSobel > 0.03) { t0 = sobel(iSobel * 3.0 /iResolution.x, iSobel * 3.0 /iResolution.y, uv); }
	if (iChromatic > 0.0) { t0 = chromatic(uv) * t0; }
	if (iTrixels > 0.0) { t0 = trixels( uv, iChannel0 ); }

	c = t0;c *= iExposure;
	if (iInvert > 0.0) { c.r = 1.0 - c.r; c.g = 1.0 - c.g; c.b = 1.0 - c.b; }
	//if (iToggle > 0.0) { c.rgb = c.brg; }
	if (iGreyScale > 0.0) { c = greyScale( c ); }
	c.r *= iRedMultiplier;
	c.g *= iGreenMultiplier;
	c.b *= iBlueMultiplier;
	/* old vignette
	if (iVignette > 0.0) { 
		vec2 p = 1.0 + -2.0 * uv;
		p.y *= 0.4;
		c = mix( c, vec4( 0.1 ), dot( p, p )*iVignette*3.0 ); 
	}*/
	// new vignette
	if (iVignette > 0.0) { 
		vec2 p = 1.0 + -2.0 * uv;
		p.y *= 1.1;
		p.x *= 2.0;
		c -= pow(length(p), 500.0);
	}
   	gl_FragColor = c;
}
