#version 330 core

uniform vec4 u_color;
uniform float u_stipple;
in float v_alpha;
out vec4 frag_color;

void main()
{
    if (u_stipple > 0.5) {
        float d = gl_FragCoord.x + gl_FragCoord.y;
        if (mod(d, 10.0) > 4.0)
            discard;
    }
    frag_color = vec4(u_color.rgb, u_color.a * v_alpha);
}
