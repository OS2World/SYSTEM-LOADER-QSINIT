#ifndef GIF_WRITE_H
#define GIF_WRITE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef char   GIFPalette[256][3];

void *WriteGIF(int *ressize,          // result size buffer
               void *srcdata,         // source data
               int x,                 // x size
               int y,                 // y size
               GIFPalette *Palette,   // palette
               int trcolor,           // transparent color (-1 to ignore)
               const char *Copyright  // copyright (can be 0)
              );

#ifdef __cplusplus
}
#endif

#endif //GIF_WRITE_H
