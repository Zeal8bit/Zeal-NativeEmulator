/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define MODE_GFX_320_8BIT 5
#define MODE_GFX_320_4BIT 7

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in mat4 instanceTransform;

uniform mat4 mvp;
uniform int video_mode;
uniform int debug_sprite_instances;

out vec2 fragTexCoord;
flat out int sprite_tile_number;
flat out int sprite_palette_mask;
flat out int sprite_flags;
flat out ivec2 sprite_pos;
flat out ivec2 sprite_size;

void main()
{
    bool mode_320 = video_mode == MODE_GFX_320_8BIT || video_mode == MODE_GFX_320_4BIT;
    float scale = mode_320 ? 2.0 : 1.0;
    int screen_height = mode_320 ? 240 : 480;

    sprite_size = ivec2(
        instanceTransform[0].x / scale,
        instanceTransform[1].y / scale);
    sprite_pos = ivec2(
        instanceTransform[3].x / scale,
        float(screen_height - sprite_size.y) - instanceTransform[3].y / scale);
    sprite_tile_number = int(instanceTransform[2].x);
    sprite_palette_mask = int(instanceTransform[2].y);
    sprite_flags = int(instanceTransform[2].z);

    fragTexCoord = vertexTexCoord;
    gl_Position = mvp * instanceTransform * vec4(vertexPosition, 1.0);
}
