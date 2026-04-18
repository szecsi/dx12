#version 300 es 
precision highp float;
precision highp int;

out vec4 fragmentColor;

in vec4 texCoord;
in vec3 sphericalTexCoord;
in vec4 worldNormal;
in vec4 worldTangent;
in vec4 worldPosition;

uniform struct{
  mat4 viewProjMatrix;
  vec3 position;
  vec3 ahead;
} camera;

uniform struct {
  sampler2D strokeTexture;
  vec2 lineSize;
  vec4 texScale;
  vec4 crossAngle;
} material;

const uvec4 bitPatterns[4] = uvec4[](
  uvec4(0x0625DF73, 0xD1B84B45, 0x152AD8D0, 0x8CFD9C7C),
  uvec4(0x179FB4DD, 0x612A0719, 0x0EA42B8D, 0x133CB7EC),   
  uvec4(0x1771BDB7, 0xEC44A092, 0x0F54D9E8, 0x83C88DDD),   
  uvec4(0x05E97738, 0x48B16C5F, 0x1CBD8666, 0x8A7EAA07)
);

uvec4 cycle(uvec4 i) {
  uvec4 o = i << 1u;
  o.y |= i.x >> 31u;
  o.x |= i.y >> 31u;
  o.w |= i.z >> 31u;
  o.z |= i.w >> 31u;
  return o;
}

void main(void) {
  vec3 normal = normalize(worldNormal.xyz);  
  vec3 bitangent = cross(normal, worldTangent.xyz);
  vec3 tangent = cross(bitangent, normal);

  vec3 x = worldPosition.xyz / worldPosition.w;
  vec3 viewDiff = camera.position - x;
  vec3 viewDir = normalize(viewDiff);
  vec3 lightDir = normalize(vec3(1, 1, 1));
  float tone = clamp(dot(normal, lightDir), 0.0, 1.0) * 4.2;

  fragmentColor = vec4(1, 1, 1, 1);

  for(uint j=4u; j>uint(tone) && j>0u; j--){
    vec2 tex = texCoord.xy / texCoord.w / material.texScale[j-1u] * 0.5;
    float rang = material.crossAngle[j-1u] * 6.28;

    vec3 h = tangent * cos(rang) + bitangent * sin(rang);
    float geom = length( cross(h, viewDir)) / dot(viewDiff, -camera.ahead) * 10.0;// / (40.0 + 8.0*float(j));
    float lod = -log2(geom) - 3.0;
    float ilod = floor(lod+1000.0) - 1000.0;
    float flod = fract(lod+1000.0);
    vec2 stex = tex / exp2(ilod);
    uvec4 bits = bitPatterns[j-1u];
    for(uint i=0u; i<256u; i++){
      vec2 seedUvPos = vec2(bits.xz >> 16u) / float(0xffff);
      
      vec2 fromSeed = stex - seedUvPos + vec2(100.5, 100.5) ;
      vec2 strokeTexPos = fract(fromSeed) - vec2(0.5, 0.5);
      uvec2 quadrant = uvec2(
        (fract(fromSeed.x * 0.5) > 0.5 )?1u:0u,
        (fract(fromSeed.y * 0.5) > 0.5 )?1u:0u
      );
      //fragmentColor = vec4(quadrant.x, quadrant.y, 0.3, 1);
      //return;
//      uvec2 quadId = uvec2(stex - seedUvPos + vec2(1024.5, 1024.5) );
//      quadId <<= (24u + ilod);
//      quadId |= bits >> (8u - ilod);
//      uvec2 bs = cyclen(bits, (ilod+24u)%16u);
//      uvec2 bikf = ~(bs ^ quadId);
//      quadId ^= (bikf << 8u) ^ (bikf << 16u) ^ (bikf << 24u);
//      uint sid = ((quadId.x & 0xff000000u) >> 16u) | (quadId.y >> 24u);
//      uvec2 quadrant = quadId & uvec2(1,1);

      float alpha = smoothstep(float(i)/257.0, float(i+1u)/257.0, float(j)-tone);

//      float alpha = smoothstep(0, 1, j-tone);
      if( ((bits.x & 1u) != quadrant.x) ||
          ((bits.y & 1u) != quadrant.y)
        ){
        alpha *= 1.0 - smoothstep(float(i)/257.0, float(i+1u)/257.0, flod);
      }
      strokeTexPos = vec2(strokeTexPos.x * cos(rang) + strokeTexPos.y * sin(rang), -strokeTexPos.x * sin(rang) + strokeTexPos.y * cos(rang));
      strokeTexPos *= vec2(1.5, 20.0) / material.lineSize.xy / exp2(flod);
      if( strokeTexPos.x > -0.5 &&
          strokeTexPos.y > -0.5 &&
          strokeTexPos.x <  0.5 &&
          strokeTexPos.y <  0.5
      ){

        vec4 c = texture(material.strokeTexture, strokeTexPos.yx + vec2(0.5,0.5));
        c.a = 1.0 - c.b;
        c.a *= alpha;
        fragmentColor = fragmentColor * (1.0 - c.a) + c * c.a;
        //vec3(float(sid % 7u) / 7.0 * 6.0, float(sid%3u) / 0.66, float(sid%5u) / 0.8) * 0.2;
      }      
      bits = cycle(bits);
    }
//    rang += 1.3;
  }
}
