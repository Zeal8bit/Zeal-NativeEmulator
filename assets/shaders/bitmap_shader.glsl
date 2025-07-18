#define MODE_BITMAP_256   2
#define MODE_BITMAP_320   3

#define PALETTE_SIZE    256
#define PALETTE_LAST    (PALETTE_SIZE - 1)

#define SCREEN_WIDTH    (320.0)
#define SCREEN_HEIGHT   (240.0)

#define BITMAP_256_BORDER   32
#define BITMAP_320_BORDER   20

#define SIZEOF_COLOR        4
#define TILESET_TEX_WIDTH   16384.0

#ifdef OPENGL_ES
precision highp float;
precision highp int;
#endif

in vec3 vertexPos;
in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D   texture0;
uniform sampler2D   tileset;
uniform vec3        palette[PALETTE_SIZE];
uniform int         video_mode;


out vec4 finalColor;

int color_from_idx(int idx)
{
    vec2 addr = vec2(float(idx / SIZEOF_COLOR) / (TILESET_TEX_WIDTH + 0.0001), 0.5);
    /* Get one set of color per layer, each containing 4 pixels */
    vec4 set = texture(tileset, addr);
    int channel = idx % SIZEOF_COLOR;
    float byte_value = (channel == 0) ? set.r :
                        (channel == 1) ? set.g :
                        (channel == 2) ? set.b :
                                         set.a;
    return int(byte_value * 255.0);
}


void main() {
    // Create absolute coordinates, with (0,0) at the top-left
    vec2 flipped   = vec2(fragTexCoord.x, 1.0 - fragTexCoord.y)
                   * vec2(SCREEN_WIDTH, SCREEN_HEIGHT);
    ivec2 orig = ivec2(int(flipped.x), int(flipped.y));
    ivec2 coord = orig;
    int index = 0;

    if (video_mode == MODE_BITMAP_256) {
        /* In 256 mode, there are a left and right border of 32 pixels each */
        index = coord.y * 256 + (coord.x - 32);
    } else {
        /* In 320 mode, there are a top and bottom borders of 20 pixels each */
        index = (coord.y - 20) * 320 + coord.x;
    }

    if (
        (video_mode == MODE_BITMAP_256 && (orig.x < BITMAP_256_BORDER || orig.x >= BITMAP_256_BORDER + 256)) ||
        (video_mode == MODE_BITMAP_320 && (orig.y < BITMAP_320_BORDER || orig.y >= BITMAP_320_BORDER + 200))
       )
    {
        /* Last value is used as the border color */
        index = 0xFFFF;
    }

    /* Get the color out of the `tileset` texture */
    int color_idx = color_from_idx(index);

    /* Convert the color index into an RGB color */
    finalColor = vec4(palette[color_idx], 1.0f);
}
