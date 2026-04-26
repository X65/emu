@vs offscreen_vs
layout(location=0) in vec2 in_pos;
layout(location=1) in vec2 in_uv;

layout(binding=0) uniform offscreen_vs_params {
    vec2 uv_offset;
    vec2 uv_scale;
};

out vec2 uv;
void main() {
    gl_Position = vec4(in_pos*2.0-1.0, 0.5, 1.0);
    uv = (in_uv * uv_scale) + uv_offset;
}
@end

@fs offscreen_fs
layout(binding=0) uniform texture2D fb_tex;
layout(binding=0) uniform sampler smp;
in vec2 uv;
out vec4 frag_color;
void main() {
    frag_color = texture(sampler2D(fb_tex, smp), uv);
}
@end

// offscreen shader with color palette decoding
@fs offscreen_pal_fs
layout(binding=0) uniform texture2D fb_tex;
layout(binding=1) uniform texture2D pal_tex;
layout(binding=0) uniform sampler smp;
in vec2 uv;
out vec4 frag_color;
void main() {
    float pix = texture(sampler2D(fb_tex, smp), uv).x;
    frag_color = vec4(texture(sampler2D(pal_tex, smp), vec2(pix,0)).xyz, 1.0);
}
@end

@vs display_vs
layout(location=0) in vec2 in_pos;
layout(location=1) in vec2 in_uv;
out vec2 uv;
void main() {
    gl_Position = vec4(in_pos*2.0-1.0, 0.5, 1.0);
    uv = in_uv;
}
@end

@fs display_fs
layout(binding=0) uniform texture2D tex;
layout(binding=0) uniform sampler smp;
in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = vec4(texture(sampler2D(tex, smp), uv).xyz, 1.0);
}
@end

// CRT post-process display shader (parallel to display_fs)
@fs display_crt_fs
layout(binding=0) uniform texture2D crt_tex;
layout(binding=0) uniform sampler crt_smp;
layout(binding=0) uniform display_crt_fs_params {
    vec2  output_size;
    float scanline_intensity;
    float mask_intensity;
    float curvature;
    float gamma;
    float vignette;
    float _pad0;
    float _pad1;
};
in vec2 uv;
out vec4 frag_color;

vec2 crt_warp(vec2 p) {
    vec2 c = p * 2.0 - 1.0;
    c *= vec2(1.0 + (c.y * c.y) * curvature * 0.10,
              1.0 + (c.x * c.x) * curvature * 0.12);
    return c * 0.5 + 0.5;
}

vec3 crt_mask(vec2 frag_px) {
    float m = mod(floor(frag_px.x), 3.0);
    vec3 c = vec3(1.0 - mask_intensity);
    if (m < 1.0)      c.r = 1.0;
    else if (m < 2.0) c.g = 1.0;
    else              c.b = 1.0;
    return c;
}

void main() {
    vec2 wuv = crt_warp(uv);
    if (wuv.x < 0.0 || wuv.x > 1.0 || wuv.y < 0.0 || wuv.y > 1.0) {
        frag_color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    vec3 col = texture(sampler2D(crt_tex, crt_smp), wuv).rgb;
    float scan_phase = wuv.y * output_size.y;
    float scan = 1.0 - scanline_intensity * (1.0 - cos(scan_phase * 3.14159265)) * 0.5;
    col *= scan;
    col *= crt_mask(wuv * output_size);
    float d = distance(wuv, vec2(0.5));
    col *= mix(1.0, max(0.0, 1.0 - vignette), smoothstep(0.4, 0.85, d));
    col = pow(max(col, vec3(0.0)), vec3(1.0 / max(gamma, 0.01)));
    frag_color = vec4(col, 1.0);
}
@end

@program offscreen offscreen_vs offscreen_fs
@program offscreen_pal offscreen_vs offscreen_pal_fs
@program display display_vs display_fs
@program display_crt display_vs display_crt_fs
