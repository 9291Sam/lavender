#version 460

#include "global_descriptor_set.glsl"

const float fTurbulence = 0.35;

const vec3 vNightColor   = vec3(.015, 0.03, 0.06) / 2.5;
const vec3 vHorizonColor = vec3(0.6, 0.3, 0.0);
const vec3 vDayColor     = vec3(0.7, 0.8, 1);

// const vec3 vNightColor   = vec3(0.0, 0.0, 0.0);
// const vec3 vHorizonColor = vec3(0.6, 0.3, 0.4);
// const vec3 vDayColor     = vec3(1.0);

const vec3 vSunColor    = vec3(1.0, 0.8, 0.6);
const vec3 vSunRimColor = vec3(1.0, 0.66, 0.33);

// float noise( in vec3 x )
// {
//     vec3 p = floor(x);
//     vec3 f = fract(x);
// 	f = f*f*f*(3.0-2.0*f);
// 	vec2 uv = (p.xy+vec2(37.0,17.0)*p.z) + f.xy;
// 	vec4 rg = texture( iChannel0, (uv+ 0.5)/256.0, -100.0 );
// 	return (-1.0+2.0*mix( rg.g, rg.r, f.z ));
// }

// float mod289(float x){return x - floor(x * (1.0 / 289.0)) * 289.0;}
// vec4 mod289(vec4 x){return x - floor(x * (1.0 / 289.0)) * 289.0;}
// vec4 perm(vec4 x){return mod289(((x * 34.0) + 1.0) * x);}

// float noise(vec3 p){
//     vec3 a = floor(p);
//     vec3 d = p - a;
//     d = d * d * (3.0 - 2.0 * d);

//     vec4 b = a.xxyy + vec4(0.0, 1.0, 0.0, 1.0);
//     vec4 k1 = perm(b.xyxy);
//     vec4 k2 = perm(k1.xyxy + b.zzww);

//     vec4 c = k2 + a.zzzz;
//     vec4 k3 = perm(c);
//     vec4 k4 = perm(c + 1.0);

//     vec4 o1 = fract(k3 * (1.0 / 41.0));
//     vec4 o2 = fract(k4 * (1.0 / 41.0));

//     vec4 o3 = o2 * d.z + o1 * (1.0 - d.z);
//     vec2 o4 = o3.yw * d.x + o3.xz * (1.0 - d.x);

//     return o4.y * d.y + o4.x * (1.0 - d.y);
// }

// float hash(float n) { return fract(sin(n) * 1e4); }
// float hash(vec2 p) { return fract(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 +
// p.x)))); }

// float noise(float x) {
// 	float i = floor(x);
// 	float f = fract(x);
// 	float u = f * f * (3.0 - 2.0 * f);
// 	return mix(hash(i), hash(i + 1.0), u);
// }

// float noise(vec2 x) {
// 	vec2 i = floor(x);
// 	vec2 f = fract(x);

// 	// Four corners in 2D of a tile
// 	float a = hash(i);
// 	float b = hash(i + vec2(1.0, 0.0));
// 	float c = hash(i + vec2(0.0, 1.0));
// 	float d = hash(i + vec2(1.0, 1.0));

// 	// Simple 2D lerp using smoothstep envelope between the values.
// 	// return vec3(mix(mix(a, b, smoothstep(0.0, 1.0, f.x)),
// 	//			mix(c, d, smoothstep(0.0, 1.0, f.x)),
// 	//			smoothstep(0.0, 1.0, f.y)));

// 	// Same code, with the clamps in smoothstep and common subexpressions
// 	// optimized away.
// 	vec2 u = f * f * (3.0 - 2.0 * f);
// 	return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
// }

// // This one has non-ideal tiling properties that I'm still tuning
// float noise(vec3 x) {
// 	const vec3 step = vec3(110, 241, 171);

// 	vec3 i = floor(x);
// 	vec3 f = fract(x);

// 	// For performance, compute the base input to a 1D hash from the integer part of the
// argument and the
// 	// incremental change to the 1D based on the 3D -> 1D wrapping
//     float n = dot(i, step);

// 	vec3 u = f * f * (3.0 - 2.0 * f);
// 	return mix(mix(mix( hash(n + dot(step, vec3(0, 0, 0))), hash(n + dot(step, vec3(1, 0, 0))),
// u.x),
//                    mix( hash(n + dot(step, vec3(0, 1, 0))), hash(n + dot(step, vec3(1, 1, 0))),
//                    u.x), u.y),
//                mix(mix( hash(n + dot(step, vec3(0, 0, 1))), hash(n + dot(step, vec3(1, 0, 1))),
//                u.x),
//                    mix( hash(n + dot(step, vec3(0, 1, 1))), hash(n + dot(step, vec3(1, 1, 1))),
//                    u.x), u.y), u.z);
// }

vec4 permute(vec4 x)
{
    return mod(((x * 34.0) + 1.0) * x, 289.0);
}
vec4 taylorInvSqrt(vec4 r)
{
    return 1.79284291400159 - 0.85373472095314 * r;
}

float noise(vec3 v)
{
    const vec2 C = vec2(1.0 / 6.0, 1.0 / 3.0);
    const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);

    // First corner
    vec3 i  = floor(v + dot(v, C.yyy));
    vec3 x0 = v - i + dot(i, C.xxx);

    // Other corners
    vec3 g  = step(x0.yzx, x0.xyz);
    vec3 l  = 1.0 - g;
    vec3 i1 = min(g.xyz, l.zxy);
    vec3 i2 = max(g.xyz, l.zxy);

    //  x0 = x0 - 0. + 0.0 * C
    vec3 x1 = x0 - i1 + 1.0 * C.xxx;
    vec3 x2 = x0 - i2 + 2.0 * C.xxx;
    vec3 x3 = x0 - 1. + 3.0 * C.xxx;

    // Permutations
    i      = mod(i, 289.0);
    vec4 p = permute(
        permute(permute(i.z + vec4(0.0, i1.z, i2.z, 1.0)) + i.y + vec4(0.0, i1.y, i2.y, 1.0)) + i.x
        + vec4(0.0, i1.x, i2.x, 1.0));

    // Gradients
    // ( N*N points uniformly over a square, mapped onto an octahedron.)
    float n_ = 1.0 / 7.0; // N=7
    vec3  ns = n_ * D.wyz - D.xzx;

    vec4 j = p - 49.0 * floor(p * ns.z * ns.z); //  mod(p,N*N)

    vec4 x_ = floor(j * ns.z);
    vec4 y_ = floor(j - 7.0 * x_); // mod(j,N)

    vec4 x = x_ * ns.x + ns.yyyy;
    vec4 y = y_ * ns.x + ns.yyyy;
    vec4 h = 1.0 - abs(x) - abs(y);

    vec4 b0 = vec4(x.xy, y.xy);
    vec4 b1 = vec4(x.zw, y.zw);

    vec4 s0 = floor(b0) * 2.0 + 1.0;
    vec4 s1 = floor(b1) * 2.0 + 1.0;
    vec4 sh = -step(h, vec4(0.0));

    vec4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
    vec4 a1 = b1.xzyw + s1.xzyw * sh.zzww;

    vec3 p0 = vec3(a0.xy, h.x);
    vec3 p1 = vec3(a0.zw, h.y);
    vec3 p2 = vec3(a1.xy, h.z);
    vec3 p3 = vec3(a1.zw, h.w);

    // Normalise gradients
    vec4 norm = taylorInvSqrt(vec4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3)));
    p0 *= norm.x;
    p1 *= norm.y;
    p2 *= norm.z;
    p3 *= norm.w;

    // Mix final noise value
    vec4 m = max(0.6 - vec4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
    m      = m * m;
    return 42.0 * dot(m * m, vec4(dot(p0, x0), dot(p1, x1), dot(p2, x2), dot(p3, x3)));
}

vec4 render(in vec3 rd, in float iTime)
{
    float fSunSpeed = 0.35 * iTime + 2.32;
    vec3  sundir    = normalize(vec3(cos(fSunSpeed), sin(fSunSpeed), 0.0));
    float sun       = clamp(dot(sundir, rd), 0.0, 1.0);

    float fSunHeight = sundir.y;

    // below this height will be full night color
    float fNightHeight = -0.36;
    // above this height will be full day color
    float fDayHeight   = 0.36;

    float fHorizonLength     = fDayHeight - fNightHeight;
    float fInverseHL         = 1.0 / fHorizonLength;
    float fHalfHorizonLength = fHorizonLength / 2.0;
    float fInverseHHL        = 1.0 / fHalfHorizonLength;
    float fMidPoint          = fNightHeight + fHalfHorizonLength;

    float fNightContrib   = clamp((fSunHeight - fMidPoint) * (-fInverseHHL), 0.0, 1.0);
    float fHorizonContrib = -clamp(abs((fSunHeight - fMidPoint) * (-fInverseHHL)), 0.0, 1.0) + 1.0;
    float fDayContrib     = clamp((fSunHeight - fMidPoint) * (fInverseHHL), 0.0, 1.0);

    // sky color
    vec3 vSkyColor = vec3(0.0);
    vSkyColor += mix(vec3(0.0), vNightColor, fNightContrib);     // Night
    vSkyColor += mix(vec3(0.0), vHorizonColor, fHorizonContrib); // Horizon
    vSkyColor += mix(vec3(0.0), vDayColor, fDayContrib);         // Day

    vec3 col = vSkyColor;

    // atmosphere brighter near horizon
    // col -= 1.0; // rd.y; // clamp(rd.y, 0.0, 0.3);

    // draw sun
    col += 0.04 * vSunRimColor * pow(sun, 4.0);
    col += 1.0 * vSunColor * pow(sun, 2000.0);

    // stars
    float fStarSpeed = -fSunSpeed * 0.5;

    float fStarContrib = clamp((fSunHeight - fDayHeight) * (-fInverseHL), 0.0, 1.0);

    vec3 vStarDir = rd
                  * mat3(
                        vec3(cos(fStarSpeed), -sin(fStarSpeed), 0.0),
                        vec3(sin(fStarSpeed), cos(fStarSpeed), 0.0),
                        vec3(0.0, 0.0, 1.0));

    float p = noise(vec3(vStarDir.xyz * 128 + 30.4));

    if ((p * (1 + exp(100 * p + -95))) > 0.9)
    {
        col += 1.0;
    }

    // col += pow(noise(vec3(vStarDir.xy * 700.0, 0.384)) + noise(vec3(vStarDir.yz * 380.0, -3.44))
    // * 0.5, 88.0) * fStarContrib * 4.0;
    return vec4(col, 1.0);
}

layout(location = 0) in vec3 in_frag_position_world;

layout(location = 0) out vec4 out_color;

void main()
{
    const vec3 dir = normalize(in_frag_position_world - in_global_info.camera_position.xyz);

    // out_color = vec4((dir /2 + 0.5), 1.0);
    out_color    = render(dir, in_global_info.time_alive);
    gl_FragDepth = 0.9999999;
}
