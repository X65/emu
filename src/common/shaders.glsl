// CRT post-process display shader, injected into sokol_framebuffer.h via
// sfb_render_ex(). The vertex stage mirrors sfb's own render shader: a
// buffer-less fullscreen triangle, with the 'rotate' uniform sfb applies to
// slot 0 itself; texture and sampler slot 0 are sfb's offscreen image.
@vs display_crt_vs
layout(binding=0) uniform render_vs_params {
    int rotate;
};
out vec2 uv;
void main() {
    float x = ((gl_VertexIndex & 1) != 0) ? 2.0 : 0.0;
    float y = ((gl_VertexIndex & 2) != 0) ? 2.0 : 0.0;
    gl_Position = vec4(vec2(x, y) * 2.0 - 1.0, 0.5, 1.0);
    if (rotate == 0) {
        uv = vec2(x, 1.0 - y);
    } else {
        uv = vec2(1.0 - y, 1.0 - x);
    }
}
@end

@fs display_crt_fs
layout(binding=0) uniform texture2D crt_tex;
layout(binding=0) uniform sampler crt_smp;
layout(binding=1) uniform display_crt_fs_params {
    vec2  output_size;
    float scanline_intensity;
    float mask_intensity;
    float curvature;
    float gamma;
    float vignette;
    float blur;
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
    // 3-tap horizontal blur — simulates CRT analog bandwidth limit.
    // Offset is in UV scaled by output_size so the blur is ~1 output pixel
    // wide at blur=1.0; weights collapse to identity when blur=0.
    float ox = blur / max(output_size.x, 1.0);
    vec3 col = texture(sampler2D(crt_tex, crt_smp), wuv).rgb * 0.5
             + texture(sampler2D(crt_tex, crt_smp), vec2(wuv.x - ox, wuv.y)).rgb * 0.25
             + texture(sampler2D(crt_tex, crt_smp), vec2(wuv.x + ox, wuv.y)).rgb * 0.25;
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

@program display_crt display_crt_vs display_crt_fs
