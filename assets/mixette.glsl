uniform vec3      	iResolution;
uniform sampler2D 	iChannel0;
uniform sampler2D 	iChannel1;
uniform sampler2D   iChannel2;
uniform sampler2D   iChannel3;
uniform sampler2D   iChannel4;
uniform sampler2D   iChannel5;
uniform sampler2D   iChannel6;
uniform sampler2D   iChannel7;
uniform sampler2D   iChannel8;
uniform float       iWeight0;
uniform float       iWeight1;
uniform float       iWeight2;
uniform float       iWeight3;
uniform float       iWeight4;
uniform float       iWeight5;
uniform float       iWeight6;
uniform float       iWeight7;
uniform float       iWeight8;
uniform bool		iDebug;
uniform int			iBlendmode;

// left main lines begin
vec3 shaderLeft(vec2 uv)
{
	vec4 left = texture2D(iChannel0, uv);
	return vec3( left.r, left.g, left.b );
}
// left main lines end

// right main lines begin
vec3 shaderRight(vec2 uv)
{
	vec4 right = texture2D(iChannel1, uv);
	return vec3( right.r, right.g, right.b );
}

// right main lines end

// Blend functions begin
vec3 multiply( vec3 s, vec3 d )
{
   return s*d;
}
vec3 colorBurn( vec3 s, vec3 d )
{
   return 1.0 - (1.0 - d) / s;
}
vec3 linearBurn( vec3 s, vec3 d )
{
   return s + d - 1.0;
}
vec3 darkerColor( vec3 s, vec3 d )
{
   return (s.x + s.y + s.z < d.x + d.y + d.z) ? s : d;
}
vec3 lighten( vec3 s, vec3 d )
{
   return max(s,d);
}
vec3 darken( vec3 s, vec3 d )
{
   return min(s,d);
}
vec3 screen( vec3 s, vec3 d )
{
   return s + d - s * d;
}

vec3 colorDodge( vec3 s, vec3 d )
{
   return d / (1.0 - s);
}

vec3 linearDodge( vec3 s, vec3 d )
{
   return s + d;
}

vec3 lighterColor( vec3 s, vec3 d )
{
   return (s.x + s.y + s.z > d.x + d.y + d.z) ? s : d;
}

float overlay( float s, float d )
{
   return (d < 0.5) ? 2.0 * s * d : 1.0 - 2.0 * (1.0 - s) * (1.0 - d);
}

vec3 overlay( vec3 s, vec3 d )
{
   vec3 c;
   c.x = overlay(s.x,d.x);
   c.y = overlay(s.y,d.y);
   c.z = overlay(s.z,d.z);
   return c;
}

float softLight( float s, float d )
{
   return (s < 0.5) ? d - (1.0 - 2.0 * s) * d * (1.0 - d) 
      : (d < 0.25) ? d + (2.0 * s - 1.0) * d * ((16.0 * d - 12.0) * d + 3.0) 
                : d + (2.0 * s - 1.0) * (sqrt(d) - d);
}

vec3 softLight( vec3 s, vec3 d )
{
   vec3 c;
   c.x = softLight(s.x,d.x);
   c.y = softLight(s.y,d.y);
   c.z = softLight(s.z,d.z);
   return c;
}

float hardLight( float s, float d )
{
   return (s < 0.5) ? 2.0 * s * d : 1.0 - 2.0 * (1.0 - s) * (1.0 - d);
}

vec3 hardLight( vec3 s, vec3 d )
{
   vec3 c;
   c.x = hardLight(s.x,d.x);
   c.y = hardLight(s.y,d.y);
   c.z = hardLight(s.z,d.z);
   return c;
}

float vividLight( float s, float d )
{
   return (s < 0.5) ? 1.0 - (1.0 - d) / (2.0 * s) : d / (2.0 * (1.0 - s));
}

vec3 vividLight( vec3 s, vec3 d )
{
   vec3 c;
   c.x = vividLight(s.x,d.x);
   c.y = vividLight(s.y,d.y);
   c.z = vividLight(s.z,d.z);
   return c;
}

vec3 linearLight( vec3 s, vec3 d )
{
   return 2.0 * s + d - 1.0;
}

float pinLight( float s, float d )
{
   return (2.0 * s - 1.0 > d) ? 2.0 * s - 1.0 : (s < 0.5 * d) ? 2.0 * s : d;
}

vec3 pinLight( vec3 s, vec3 d )
{
   vec3 c;
   c.x = pinLight(s.x,d.x);
   c.y = pinLight(s.y,d.y);
   c.z = pinLight(s.z,d.z);
   return c;
}

vec3 hardMix( vec3 s, vec3 d )
{
   return floor(s + d);
}

vec3 difference( vec3 s, vec3 d )
{
   return abs(d - s);
}

vec3 exclusion( vec3 s, vec3 d )
{
   return s + d - 2.0 * s * d;
}

vec3 subtract( vec3 s, vec3 d )
{
   return s - d;
}

vec3 divide( vec3 s, vec3 d )
{
   return s / d;
}

// rgb<-->hsv functions by Sam Hocevar
// http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl
vec3 rgb2hsv(vec3 c)
{
   vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
   vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
   vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
   
   float d = q.x - min(q.w, q.y);
   float e = 1.0e-10;
   return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c)
{
   vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
   vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
   return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 hue( vec3 s, vec3 d )
{
   d = rgb2hsv(d);
   d.x = rgb2hsv(s).x;
   return hsv2rgb(d);
}

vec3 color( vec3 s, vec3 d )
{
   s = rgb2hsv(s);
   s.z = rgb2hsv(d).z;
   return hsv2rgb(s);
}

vec3 saturation( vec3 s, vec3 d )
{
   d = rgb2hsv(d);
   d.y = rgb2hsv(s).y;
   return hsv2rgb(d);
}

vec3 luminosity( vec3 s, vec3 d )
{
   float dLum = dot(d, vec3(0.3, 0.59, 0.11));
   float sLum = dot(s, vec3(0.3, 0.59, 0.11));
   float lum = sLum - dLum;
   vec3 c = d + lum;
   float minC = min(min(c.x, c.y), c.z);
   float maxC = max(max(c.x, c.y), c.z);
   if(minC < 0.0) return sLum + ((c - sLum) * sLum) / (sLum - minC);
   else if(maxC > 1.0) return sLum + ((c - sLum) * (1.0 - sLum)) / (maxC - sLum);
   else return c;
} 
// Blend functions end


void main() {
	vec2 uv = gl_FragCoord.xy / iResolution.xy;
	vec3 c = vec3(0.0);
	
	switch ( iBlendmode )
   {
   case 0: 
      c = texture(iChannel0, uv).xyz * iWeight0
		+ texture(iChannel1, uv).xyz * iWeight1 
		+ texture(iChannel2, uv).xyz * iWeight2
		+ texture(iChannel3, uv).xyz * iWeight3 
		+ texture(iChannel4, uv).xyz * iWeight4 
		+ texture(iChannel5, uv).xyz * iWeight5 
		+ texture(iChannel6, uv).xyz * iWeight6 
		+ texture(iChannel7, uv).xyz * iWeight7
		+ texture(iChannel8, uv).xyz * iWeight8;
      break;
   case 1: 
      c = multiply( shaderLeft(uv), shaderRight(uv) );
      break;
   case 2: 
      c = colorBurn( shaderLeft(uv), shaderRight(uv) );
      break;
   case 3: 
      c = linearBurn( shaderLeft(uv), shaderRight(uv) );
      break;
   case 4: 
      c = darkerColor( shaderLeft(uv), shaderRight(uv) );
      break;
   case 5: 
      c = lighten( shaderLeft(uv), shaderRight(uv) );
      break;
   case 6: 
      c = screen( shaderLeft(uv), shaderRight(uv) );
      break;
   case 7: 
      c = colorDodge( shaderLeft(uv), shaderRight(uv) );
      break;
   case 8: 
      c = linearDodge( shaderLeft(uv), shaderRight(uv) );
      break;
   case 9: 
      c = lighterColor( shaderLeft(uv), shaderRight(uv) );
      break;
   case 10: 
      c = overlay( shaderLeft(uv), shaderRight(uv) );
      break;
   case 11: 
      c = softLight( shaderLeft(uv), shaderRight(uv) );
      break;
   case 12: 
      c = hardLight( shaderLeft(uv), shaderRight(uv) );
      break;
   case 13: 
      c = vividLight( shaderLeft(uv), shaderRight(uv) );
      break;
   case 14: 
      c = linearLight( shaderLeft(uv), shaderRight(uv) );
      break;
   case 15: 
      c = pinLight( shaderLeft(uv), shaderRight(uv) );
      break;
   case 16: 
      c = hardMix( shaderLeft(uv), shaderRight(uv) );
      break;
   case 17: 
      c = difference( shaderLeft(uv), shaderRight(uv) );
      break;
   case 18: 
      c = exclusion( shaderLeft(uv), shaderRight(uv) );
      break;
   case 19: 
      c = subtract( shaderLeft(uv), shaderRight(uv) );
      break;
   case 20: 
      c = divide( shaderLeft(uv), shaderRight(uv) );
      break;
   case 21: 
      c = hue( shaderLeft(uv), shaderRight(uv) );
      break;
   case 22: 
      c = color( shaderLeft(uv), shaderRight(uv) );
      break;
   case 23: 
      c = saturation( shaderLeft(uv), shaderRight(uv) );
      break;
   case 24: 
      c = luminosity( shaderLeft(uv), shaderRight(uv) );
      break;
   case 25: 
      c = darken( shaderLeft(uv), shaderRight(uv) );
      break;
   case 26: 
      c = shaderLeft(uv);
      break;
   default: // in any other case.
      c = texture(iChannel0, uv).xyz;
      break;
   }
   

	gl_FragColor = vec4(c.r, c.g, c.b, 1.0);	
}
