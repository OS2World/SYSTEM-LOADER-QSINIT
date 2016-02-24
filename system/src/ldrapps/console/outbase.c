//
// QSINIT
// screen support dll: common output functions
//
#include "conint.h"
#include "stdlib.h"

u32t _std common_copy(u32t mode, u32t x, u32t y, u32t dx, u32t dy, void *buf,
   u32t pitch, int write) 
{
   con_modeinfo *mi = modes+mode;
   u32t          rc = 0;
   if (mode>=mode_cnt) return 0;
   if (!mi->physmap && !mi->shadow) return 0;

   if (mi->bits>=8) {
      u8t* membase = 0;
      u32t   vbpps = BytesBP(mi->bits),
             width = dx*vbpps,
              xpos = x*vbpps;
      // write to video memory
      if (write) {
         if (mi->physmap) {
            membase = mi->physmap + mi->mempitch*y + xpos;
            if (width==pitch && width==mi->mempitch) memcpy(membase, buf, width*dy);
               else MoveBlock(buf, membase, width, dy, pitch, mi->mempitch);
         }
         // update shadow buffer
         if (mi->shadow) {
            membase = mi->shadow + mi->shadowpitch * y + xpos;
            if (width==pitch && width==mi->shadowpitch) memcpy(membase, buf, width*dy);
               else MoveBlock(buf, membase, width, dy, pitch, mi->shadowpitch);
         }
      } else { // read from video memory
         u32t vmem_p = mi->shadow? mi->shadowpitch : mi->mempitch;
         membase = (mi->shadow?mi->shadow:mi->physmap) + vmem_p * y + xpos;

         if (width==pitch && width==vmem_p) memcpy(buf, membase, width*dy);
            else MoveBlock(membase, buf, width, dy, vmem_p, pitch);
      }
      rc = 1;
   }
   return rc;
}

/// fill number of screen lines by color value
u32t common_clear(u32t mode, u32t x, u32t y, u32t dx, u32t dy, u32t color) {
   con_modeinfo *mi = modes+mode;
   u32t          rc = 0;
   if (mode>=mode_cnt) return 0;

   if (x<mi->width && y<mi->height) {
      u32t   vbpps = BytesBP(mi->bits);
      // special feature to make evio_* life more easy
      if (!x && !dx) dx=mi->width; else
      if (x+dx>mi->width ) dx=mi->width -x;
      if (y+dy>mi->height) dy=mi->height-y;

      if (vbpps==1 && dx==mi->width) {
         if (mi->shadow) memset(mi->shadow+y*mi->shadowpitch, color, dy*mi->shadowpitch);
         if (mi->physmap) memset(mi->physmap+y*mi->mempitch, color, dy*mi->mempitch);
         rc = 1;
      } else {
         if (mi->shadow) con_fillblock(mi->shadow + y*mi->shadowpitch + x*vbpps,
            dx, dy, mi->shadowpitch, color, vbpps);
         if (mi->physmap) con_fillblock(mi->physmap + y*mi->mempitch + x*vbpps,
            dx, dy, mi->mempitch, color, vbpps);
         rc = 1;
      }
   }
   return rc;
}

/// fill number of screen lines by color value
u32t common_clear_nofb(u32t mode, u32t x, u32t y, u32t dx, u32t dy, u32t color) {
   u32t  rc = 0;
   if (common_clear(mode, x, y, dx, dy, color)) {
      con_modeinfo *mi = modes+mode;

      if (mi->physmap) rc = 1; else
      if (pl_dirclear) {
         if (!x && !dx)
            rc = pl_dirclear(0, y, mi->width, dy, color);
         else
            rc = pl_dirclear(x, y, dx, dy, color);
      }
   }
   return rc;
}

u32t common_scroll(u32t mode, u32t ys, u32t yd, u32t lines) {
   con_modeinfo *mi = modes+mode;
   u32t          rc = 0;
   if (mode>=mode_cnt) return 0;

   if (ys<mi->height && yd<mi->height) {
      if (ys==yd) return 1;
      if (ys+lines > mi->height) lines = mi->height-ys;
      if (yd+lines > mi->height) lines = mi->height-yd;

      if (mi->bits>=8) {
         u32t  dofs, sofs, size;

         if (mi->shadow) {
            dofs = yd*mi->shadowpitch;
            sofs = ys*mi->shadowpitch;
            size = lines*mi->shadowpitch;
            // update shadow buffer
            if (ys<yd) memcpy(mi->shadow+sofs, mi->shadow+dofs, size); else
               memcpy(mi->shadow+dofs, mi->shadow+sofs, size);
            // copy to screen
            if (mi->physmap) {
               if (mi->mempitch!=mi->shadowpitch) {
                  u32t cpy = mi->mempitch;
                  if (mi->shadowpitch<cpy) cpy = mi->shadowpitch;
               
                  MoveBlock(mi->shadow+dofs, mi->physmap+yd*mi->mempitch,
                     cpy, lines, mi->shadowpitch, mi->mempitch);
               } else
                  memcpy(mi->physmap+dofs, mi->shadow+dofs, size);
            }
            rc = 1;
         } else 
         if (mi->physmap) {
            dofs = yd*mi->mempitch;
            sofs = ys*mi->mempitch;
            size = lines*mi->mempitch;

            if (ys<yd) memcpy(mi->physmap+sofs, mi->physmap+dofs, size); else
               memcpy(mi->physmap+dofs, mi->physmap+sofs, size);
            rc = 1;
         }
      }
   }
   return rc;
}

u32t common_scroll_nofb(u32t mode, u32t ys, u32t yd, u32t lines) {
   u32t  rc = 0;
   if (common_scroll(mode, ys, yd, lines)) {
      con_modeinfo *mi = modes+mode;

      if (mi->physmap) rc = 1; else 
         pl_flush(mode, 0, yd, mi->width, lines);
   }
   return rc;
}

u32t common_flush(u32t mode, u32t x, u32t y, u32t dx, u32t dy) {
   con_modeinfo  *mi = modes+mode;
   u32t        vbpps;
   if (mode>=mode_cnt) return 0;
   if (!mi->shadow || !mi->physmap) return 0;
   if (x>=mi->width || y>=mi->height) return 0;
   if (x+dx>mi->width)  dx=mi->width -x;
   if (y+dy>mi->height) dy=mi->height-y;
   if (!dx||!dy) return 0;

   vbpps = BytesBP(mi->bits);
   MoveBlock(mi->shadow + y*mi->shadowpitch + x*vbpps, mi->physmap +
      y*mi->mempitch + x*vbpps, dx*vbpps, dy, mi->shadowpitch, mi->mempitch);
   return 1;
}

u32t common_flush_nofb(u32t mode, u32t x, u32t y, u32t dx, u32t dy) {
   con_modeinfo  *mi = modes+mode;
   if (mode>=mode_cnt) return 0;
   if (!mi->shadow) return 0;
   if (x>=mi->width || y>=mi->height) return 0;
   if (x+dx>mi->width)  dx=mi->width -x;
   if (y+dy>mi->height) dy=mi->height-y;
   if (!dx||!dy) return 0;
   /* common_flush() returns 1 only if both shadow & frame exists, so we
      finished here, else - trying to blit shadow buffer by EFI call */
   if (common_flush(mode, x, y, dx, dy)) return 1; else {
      u32t  vbpps = BytesBP(mi->bits);
      // copy to screen
      if (pl_dirblit)
         if (pl_dirblit(x, y, dx, dy, mi->shadow + y*mi->shadowpitch +
            x*vbpps, mi->shadowpitch)) return 1;
   }
   return 0;
}
