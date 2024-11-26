#version 460



const float fTurbulence = 0.35;


const vec3 vNightColor   = vec3(.015, 0.03, 0.06) / 2.5;
const vec3 vHorizonColor = vec3(0.6, 0.3, 0.0);
const vec3 vDayColor     = vec3(0.7,0.8,1);


// const vec3 vNightColor   = vec3(0.0, 0.0, 0.0);
// const vec3 vHorizonColor = vec3(0.6, 0.3, 0.4);
// const vec3 vDayColor     = vec3(1.0);

const vec3 vSunColor     = vec3(1.0,0.8,0.6);
const vec3 vSunRimColor  = vec3(1.0,0.66,0.33);

// float noise( in vec3 x )
// {
//     vec3 p = floor(x);
//     vec3 f = fract(x);
// 	f = f*f*f*(3.0-2.0*f);
// 	vec2 uv = (p.xy+vec2(37.0,17.0)*p.z) + f.xy;
// 	vec4 rg = texture( iChannel0, (uv+ 0.5)/256.0, -100.0 );
// 	return (-1.0+2.0*mix( rg.g, rg.r, f.z ));
// }

float mod289(float x){return x - floor(x * (1.0 / 289.0)) * 289.0;}
vec4 mod289(vec4 x){return x - floor(x * (1.0 / 289.0)) * 289.0;}
vec4 perm(vec4 x){return mod289(((x * 34.0) + 1.0) * x);}

float noise(vec3 p){
    vec3 a = floor(p);
    vec3 d = p - a;
    d = d * d * (3.0 - 2.0 * d);

    vec4 b = a.xxyy + vec4(0.0, 1.0, 0.0, 1.0);
    vec4 k1 = perm(b.xyxy);
    vec4 k2 = perm(k1.xyxy + b.zzww);

    vec4 c = k2 + a.zzzz;
    vec4 k3 = perm(c);
    vec4 k4 = perm(c + 1.0);

    vec4 o1 = fract(k3 * (1.0 / 41.0));
    vec4 o2 = fract(k4 * (1.0 / 41.0));

    vec4 o3 = o2 * d.z + o1 * (1.0 - d.z);
    vec2 o4 = o3.yw * d.x + o3.xz * (1.0 - d.x);

    return o4.y * d.y + o4.x * (1.0 - d.y);
}



vec4 render(in vec3 rd, in float iTime)
{
    float fSunSpeed = 0.35 * iTime;
    vec3 sundir = normalize( vec3(cos(fSunSpeed),sin(fSunSpeed),0.0) );
	float sun = clamp( dot(sundir,rd), 0.0, 1.0 );
    
    float fSunHeight = sundir.y;
    
    // below this height will be full night color
    float fNightHeight = -0.36;
    // above this height will be full day color
    float fDayHeight   = 0.36;
    
    float fHorizonLength = fDayHeight - fNightHeight;
    float fInverseHL = 1.0 / fHorizonLength;
    float fHalfHorizonLength = fHorizonLength / 2.0;
    float fInverseHHL = 1.0 / fHalfHorizonLength;
    float fMidPoint = fNightHeight + fHalfHorizonLength;
    
    float fNightContrib = clamp((fSunHeight - fMidPoint) * (-fInverseHHL), 0.0, 1.0);
    float fHorizonContrib = -clamp(abs((fSunHeight - fMidPoint) * (-fInverseHHL)), 0.0, 1.0) + 1.0;
    float fDayContrib = clamp((fSunHeight - fMidPoint) * ( fInverseHHL), 0.0, 1.0);
    
    // sky color
    vec3 vSkyColor = vec3(0.0);
    vSkyColor += mix(vec3(0.0),   vNightColor, fNightContrib);   // Night
    vSkyColor += mix(vec3(0.0), vHorizonColor, fHorizonContrib); // Horizon
    vSkyColor += mix(vec3(0.0),     vDayColor, fDayContrib);     // Day
    
	vec3 col = vSkyColor;
    
    // atmosphere brighter near horizon
    // col -= 1.0; // rd.y; // clamp(rd.y, 0.0, 0.3);
    
    // draw sun
	col += 0.04 * vSunRimColor * pow( sun,    4.0 );
	col += 1.0 * vSunColor    * pow( sun, 2000.0 );
    
    // stars
    // float fStarSpeed = -fSunSpeed * 0.5;
    
    // float fStarContrib = clamp((fSunHeight - fDayHeight) * (-fInverseHL), 0.0, 1.0);
    
    // vec3 vStarDir = rd * mat3( vec3(cos(fStarSpeed), -sin(fStarSpeed), 0.0),
    //                            vec3(sin(fStarSpeed),  cos(fStarSpeed), 0.0),
    //                            vec3(0.0,             0.0,            1.0));
                              
    // col += pow(noise(vec3(vStarDir.xy * 700.0, 0.384)) + noise(vec3(vStarDir.yz * 380.0, -3.44)) * 0.5, 88.0) * fStarContrib * 4.0;
    return vec4( col, 1.0 );
}


layout(push_constant) uniform PushConstants
{
    mat4 model_matrix;
    vec4 camera_position;
    vec4 camera_normal;
    uint mvp_matricies_id;
    uint draw_id;
    float time_alive;
} in_push_constants;

layout(location = 0) in vec3 in_frag_position_world;

layout(location = 0) out vec4 out_color;

void main()
{
    const vec3 dir = normalize(in_frag_position_world - in_push_constants.camera_position.xyz);

    // out_color = vec4((dir /2 + 0.5), 1.0);
    out_color = render(dir, in_push_constants.time_alive);
    gl_FragDepth = 0.9999999;
}
