@ctype float f32
@ctype vec2 V2
@ctype vec3 V3
@ctype vec4 V4
@ctype mat3 M3
@ctype mat4 M4

// NOTE(felix): for WebGL, a uniform block cannot have more than 16 members
@block common_uniform_block
{
    mat4 model; // NOTE(felix): this should be mat3 but I was running into an alignment bug (?) with shdc
    mat4 inverse_model; // NOTE(felix): likewise
    vec2 framebuffer_size;
    vec4 color; // also acts as top_left_color for below

    // 0 = rectangle
    // 1 = quad
    int kind;

    // rectangle, quad
    vec4 bottom_left_color;
    vec4 bottom_right_color;
    vec4 top_right_color;

    // rectangle
    vec2 top_left_pixels;
    vec2 bottom_right_pixels;
    vec4 border_color;
    vec2 border_data; // .x = width, .y = radius
    // float border_width;
    // float border_radius;

    // arbitrary quadrilateral
    vec2 quad_top_left;
    vec2 quad_bottom_left;
    vec2 quad_bottom_right;
    vec2 quad_top_right;
};
@end


@block common_functions

vec2 ndc_from_pixels(vec2 pixels) {
    vec2 ndc = (pixels / framebuffer_size) * 2.0 - 1.0;
    ndc.y = -ndc.y;
    return ndc;
}

vec2 pixels_from_ndc(vec2 ndc) {
    ndc.y = -ndc.y;
    vec2 pixels = (ndc + 1.0) * 0.5;
    pixels *= framebuffer_size;
    return pixels;
}

@end


@vs vertex_shader


layout(set = 0, binding = 0) uniform vertex_parameters
@include_block common_uniform_block

@include_block common_functions

vec2 corner_from_vertex_index(int vertex_index) {
    vec2 corners[4] = vec2[4](
        vec2(0.0, 0.0), // bottom left
        vec2(1.0, 0.0), // bottom right
        vec2(1.0, 1.0), // top right
        vec2(0.0, 1.0) // top left
    );
    int indices[6] = int[6](0, 1, 2, 0, 2, 3);
    vec2 corner = corners[indices[vertex_index]];
    return corner;
}

out vec2 quad_corner_uv;
void main() {
    if (kind == 0) { // rectangle
        vec2 half_size_pixels = (bottom_right_pixels - top_left_pixels) * 0.5;
        vec2 centre_pixels = top_left_pixels + half_size_pixels;
        float border_width = border_data.x;
        float border_radius = border_data.y;
        vec2 half_size_with_border_pixels = half_size_pixels + vec2(border_width);

        vec2 corners_pixels[4] = vec2[4](
            centre_pixels + vec2(-half_size_with_border_pixels.x, -half_size_with_border_pixels.y),
            centre_pixels + vec2( +half_size_with_border_pixels.x, -half_size_with_border_pixels.y),
            centre_pixels + vec2( +half_size_with_border_pixels.x, +half_size_with_border_pixels.y),
            centre_pixels + vec2(-half_size_with_border_pixels.x, +half_size_with_border_pixels.y)
        );

        vec2 transformed_corners_pixels[4];
        for (int i = 0; i < 4; i += 1) {
            vec3 transformed_corner = mat3(model) * vec3(corners_pixels[i], 1.0);
            transformed_corners_pixels[i] = transformed_corner.xy;
        }

        vec2 min_point = transformed_corners_pixels[0], max_point = transformed_corners_pixels[0];
        for (int i = 1; i < 4; i += 1) {
            min_point = min(min_point, transformed_corners_pixels[i]);
            max_point = max(max_point, transformed_corners_pixels[i]);
        }

        vec2 corner = corner_from_vertex_index(gl_VertexIndex);
        vec2 position_ndc = mix(ndc_from_pixels(min_point), ndc_from_pixels(max_point), corner);
        gl_Position = vec4(position_ndc, 0.0, 1.0);
    }

    if (kind == 1) { // quad
        vec2 quad_corners[4] = vec2[4](
            quad_top_left,
            quad_bottom_left,
            quad_bottom_right,
            quad_top_right
        );

        int corner_index = 0;
        vec2 corner = corner_from_vertex_index(gl_VertexIndex);
        quad_corner_uv = corner;
        if (corner.x < 0.5 && corner.y < 0.5) { corner_index = 0; }
        else if (corner.x > 0.5 && corner.y < 0.5) { corner_index = 1; }
        else if (corner.x > 0.5 && corner.y > 0.5) { corner_index = 2; }
        else { corner_index = 3; }

        vec2 coordinate = quad_corners[corner_index];
        vec3 transformed = mat3(model) * vec3(coordinate, 1.0);
        vec2 position_pixels = transformed.xy;

        vec2 position_ndc = ndc_from_pixels(position_pixels);
        gl_Position = vec4(position_ndc, 0.0, 1.0);
    }
}
@end


@fs fragment_shader

layout(set = 0, binding = 1) uniform fragment_parameters
@include_block common_uniform_block

@include_block common_functions

in vec2 quad_corner_uv;
out vec4 out_color;
void main() {
    if (kind == 0) { // rectangle
        vec2 size_pixels = bottom_right_pixels - top_left_pixels;
        vec2 half_size_pixels = size_pixels * 0.5;
        vec2 centre_pixels = top_left_pixels + half_size_pixels;

        vec2 fragment_coordinate = gl_FragCoord.xy;
        vec2 point_pixels = (mat3(inverse_model) * vec3(fragment_coordinate, 1.0)).xy;

        vec2 point_relative_to_centre_pixels = point_pixels - centre_pixels;

        float border_width = border_data.x;
        float border_radius = border_data.y;

        vec2 offset = abs(point_relative_to_centre_pixels) - (half_size_pixels - vec2(border_radius));
        vec2 offset_above_0  = max(offset, vec2(0.0));
        float outside_distance = length(offset_above_0);
        float inside_distance  = min(max(offset.x, offset.y), 0.0);
        float distance = outside_distance + inside_distance - border_radius;

        float inner_distance = distance + border_width;
        float anti_aliasing = max(1.0, fwidth(distance));
        float outer_mask = smoothstep(anti_aliasing, -anti_aliasing, distance);
        float inner_mask = smoothstep(anti_aliasing, -anti_aliasing, inner_distance);

        vec2 uv = (point_pixels - top_left_pixels) / size_pixels;
        uv = clamp(uv, 0.0, 1.0);
        vec4 top_left_color = color;
        vec4 top = mix(top_left_color, top_right_color, uv.x);
        vec4 bottom = mix(bottom_left_color, bottom_right_color, uv.x);
        vec4 fill_color = mix(bottom, top, uv.y);

        vec4 border_mix = mix(vec4(0.0), border_color, outer_mask - inner_mask);
        vec4 fill_mix = mix(vec4(0.0), fill_color,  inner_mask);

        vec4 color = vec4(border_mix.rgb + fill_mix.rgb, max(outer_mask * border_color.a, inner_mask * fill_color.a));
        out_color = color;
    }

    if (kind == 1) { // quad
        float u = clamp(quad_corner_uv.x, 0.0, 1.0);
        float v = clamp(quad_corner_uv.y, 0.0, 1.0);

        vec4 top_left_color = color;
        vec4 c0 = top_left_color;
        vec4 c1 = bottom_left_color;
        vec4 c2 = bottom_right_color;
        vec4 c3 = top_right_color;

        vec4 bottom = mix(c0, c1, u);
        vec4 top    = mix(c3, c2, u);
        vec4 color  = mix(bottom, top, v);

        out_color = color;
    }
}
@end


@program uber vertex_shader fragment_shader
