// fbdump.c
#include <stdint.h>
#include <png.h>

#include <stdlib.h>

#include "fb_data.h"

#define RED_5 10
#define BLUE_5 0
#define RED_6 11
#define BLUE_6 0
#define RED_8 16
#define BLUE_8 0

int main() {

 char sname[256];
	sprintf(sname, "dc.png");
	FILE *fp = fopen(sname, "wb");
    if (fp == NULL)
    	return 0;

    int h = HEIGHT;
    int w = WIDTH;

    uint16_t *CurrentRow = fb_data;

	png_bytepp rows = (png_bytepp)malloc(h * sizeof(png_bytep));
	for (int y = 0; y < h; y++) {
		png_bytep dst = rows[y] = (png_bytep)malloc(w * 4);	// 32-bit per pixel
        for (int x = 0; x < w; x++) {
            uint16_t src = fb_data[x + y * w];
            *dst++ = (((src >> RED_6) & 0x1F) << 3);
            *dst++ = (((src >> 5) & 0x3F) << 2);
            *dst++ = (((src >> BLUE_6) & 0x1F) << 3);
            *dst++ = 0xFF;
        }
	}

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info_ptr = png_create_info_struct(png_ptr);

    png_init_io(png_ptr, fp);


    /* write header */
    png_set_IHDR(png_ptr, info_ptr, w, h,
                         8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                         PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_write_info(png_ptr, info_ptr);


            /* write bytes */
    png_write_image(png_ptr, rows);

    /* end write */
    png_write_end(png_ptr, NULL);
    fclose(fp);

	for (int y = 0; y < h; y++)
		free(rows[y]);  
	free(rows);

    return 0;
}