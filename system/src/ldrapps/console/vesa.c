//
// QSINIT
// screen support dll: vesa interfaces
//
#include "stdlib.h"
#include "qsutil.h"
#include "conint.h"
#include "qsint.h"
#include "console.h"
#include "qspage.h"
#include "vio.h"

// disk r/w buffer, used for vesa calls
extern boot_data boot_info;
extern int    global_nolfb;
extern u32t  current_flags; // current mode flags
extern u32t    modelimit_x,
               modelimit_y;

u32t vesadetect(VBEModeInfo *vinfo, modeinfo *minfo, u32t mmask, u32t *memsize) {
   u16t       rmbuff = boot_info.diskbuf_seg, *modes;
/* note: using real mode disk buffer to get vesa info.
   Vesa info and vesa mode info must use separate buffes, because at least VBox,
   puts mode list into VBEInfo->Reserved area */
   VBEInfo     *vib = (VBEInfo*)hlp_segtoflat(rmbuff);
   VBEModeInfo  *cm = (VBEModeInfo*)(vib+1);
   struct rmcallregs_s rr;
   u32t  ii;
   if (memsize) *memsize = 0;

   memset(&rr, 0, sizeof(rr));
   rr.r_ds  = rmbuff;
   rr.r_es  = rmbuff;
   rr.r_eax = 0x4F00;
   // make sure vesa 1.x call
   vib->VBESignature = 0;
   // query list of modes
   if (!int10h(&rr)) return 0;
   if ((rr.r_eax&0xFFFF)!=0x004F) return 0;
   ii = vib->ModeTablePtr;
   // log_printf("mode list ptr: %04X:%04X\n", ii>>16, ii&0xFFFF);
   modes = (u16t*)hlp_rmtopm(ii);
   // total video memory size
   if (memsize) *memsize = (u32t)vib->TotalMemory<<16;
   log_printf("%ukb of vesa memory\n",vib->TotalMemory<<6);

   ii = 0;
   while (*modes!=0xFFFF && ii<MAXVESA_MODES) {
      memset(&rr, 0, sizeof(rr));
      memset(cm, 0, sizeof(VBEModeInfo));
      rr.r_ds  = rmbuff;
      rr.r_es  = rmbuff;
      rr.r_edi = sizeof(VBEInfo);
      rr.r_eax = 0x4F01;
      rr.r_ecx = *modes;
      // query mode
      if (int10h(&rr)) {
         if ((rr.r_eax&0xFFFF)==0x004F) {
            do {
               u32t bpp = cm->NumberOfPlanes * cm->BitsPerPixel,
                   mmdl = cm->MemoryModel;
               if ((cm->ModeAttributes&VSMI_SUPPORTED)==0) break;
               if ((cm->ModeAttributes&VSMI_OPTINFO)==0) break;
               if (global_nolfb && (cm->ModeAttributes&VSMI_NOBANK)) break;
               if (cm->NumberOfPlanes>1 && cm->BitsPerPixel!=1) break;
               // check graphic modes only
               if (cm->ModeAttributes&VSMI_GRAPHMODE) {
                  static u32t bbits[] = {8, 15, 16, 24, 32, 1, 4, 0};
                  u32t  mvalue, *bp;
                  // check memory model
                  if (bpp==4 && mmdl!=3 || bpp==8 && mmdl!=4 ||
                      bpp>8 && !(mmdl==6||mmdl==4)) break;

                  mvalue = 1; bp = bbits;
                  while (*bp)
                     if (*bp==cm->BitsPerPixel) break; else { bp++; mvalue<<=1; }
                  // uncknown number of pixels
                  if (!*bp) break;
                  // mode deprecated by user
                  if ((mmask&mvalue)==0) break;
                  if (modelimit_x && modelimit_x<cm->XResolution) break;
                  if (modelimit_y && modelimit_y<cm->YResolution) break;
               } else {
                  // drop incorrect modes
                  if (cm->XResolution>132 || cm->YResolution>60) break;
               }
               vinfo[ii] = *cm;
               minfo[ii].vesaref = ii;
               minfo[ii].modenum = *modes;

               ii++;
            } while (false);
         }
      }
      modes++;
   }

   return ii;
}

/// call mode info for single mode
u32t vesamodeinfo(VBEModeInfo *vinfo, u16t mode) {
   u16t     rmbuff = boot_info.diskbuf_seg, *modes;
   VBEModeInfo *cm = (VBEModeInfo*)hlp_segtoflat(rmbuff);
   struct rmcallregs_s rr;

   memset(&rr, 0, sizeof(rr));
   memset(cm, 0, sizeof(VBEModeInfo));
   rr.r_ds  = rmbuff;
   rr.r_es  = rmbuff;
   rr.r_eax = 0x4F01;
   rr.r_ecx = mode;
   // query mode
   if (int10h(&rr)) {
      *vinfo = *cm;
      return 1;
   }
   return 0;
}

/// call mode info for single mode
u32t vesasetmode(VBEModeInfo *vinfo, u16t mode, u32t flags) {
   struct rmcallregs_s rr;
   if (vinfo->ModeAttributes&VSMI_NOBANK)
      if (global_nolfb||(flags&CON_NOLFB)) return 0;

   memset(&rr, 0, sizeof(rr));
   rr.r_eax = 0x4F02;
   rr.r_ebx = mode;

   if (flags&CON_NOSCREENCLEAR) rr.r_ebx|=0x8000;
   if ((flags&CON_GRAPHMODE)!=0 && (flags&CON_NOLFB)==0) rr.r_ebx|=0x4000;
   // set mode
   if (!int10h(&rr)) return 0;
   if ((rr.r_eax&0xFFFF)!=0x004F) return 0;
   if ((flags&CON_GRAPHMODE)==0) {
      vio_intensity(1);
      vio_getmode(0,0);
      if ((flags&CON_NOSCREENCLEAR)==0) vio_clearscr(); else
         vio_setpos(0,0);
   }
   return 1;
}


u32t vesamemcpy(con_modeinfo *mi, VBEModeInfo *vi, u32t x, u32t y, 
                u32t dx, u32t dy, void *buf, u32t pitch, int write)
{
   u32t   bpp = vi->NumberOfPlanes * vi->BitsPerPixel,
        linep = vi->BytesPerScanline * y,
           rc = 0;

   if ((current_flags&CON_NOLFB)!=0) {
   } else {
      if (mi->physmap) {
         u8t* membase = mi->physmap;

         if (vi->NumberOfPlanes==1) {
            if (bpp>=8) { // plain 1..4 byte width modes
               u32t width = dx*bpp>>3,
                     xpos = x*bpp>>3;
               membase   += linep + xpos;
               // write to video memory
               if (write) {
                  if (width==pitch && width==mi->mempitch)
                     memcpy(membase, buf, width*dy);
                  else
                     MoveBlock(buf, membase, width, dy, pitch, mi->mempitch);
                  // update shadow buffer
                  if (mi->shadow) {
                     membase = mi->shadow + mi->shadowpitch * y + xpos;
                     if (width==pitch && width==mi->shadowpitch)
                        memcpy(membase, buf, width*dy);
                     else
                        MoveBlock(buf, membase, width, dy, pitch, mi->mempitch);
                  }
               } else { // read from video memory
                  u32t vmem_p = mi->shadow? mi->shadowpitch : mi->mempitch;
                  if (mi->shadow) 
                     membase = mi->shadow + vmem_p * y + xpos;

                  if (width==pitch && width==vmem_p)
                     memcpy(buf, membase, width*dy);
                  else
                     MoveBlock(membase, buf, width, dy, vmem_p, pitch);
               }
               rc = 1;
            } else { // plain 1,4 bits width modes

            }
         } else { // 4 planes
         }
      }
   }
   return rc;
}

u32t vesascroll(con_modeinfo *mi, u32t ys, u32t yd, u32t lines) {
   u32t  rc = 0;
   if ((current_flags&CON_NOLFB)==0 && ys<mi->height && yd<mi->height) {
      if (ys==yd) return 1;
      if (ys+lines > mi->height) lines = mi->height-ys;
      if (yd+lines > mi->height) lines = mi->height-yd;

      if (mi->physmap) {
         u8t *membase = mi->physmap;
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
               if (mi->mempitch!=mi->shadowpitch) {
                  u32t cpy = mi->mempitch;
                  if (mi->shadowpitch<cpy) cpy = mi->shadowpitch;

                  MoveBlock(mi->shadow+dofs, membase+yd*mi->mempitch,
                     cpy, lines, mi->shadowpitch, mi->mempitch);
               } else
                  memcpy(membase+dofs, mi->shadow+dofs, size);
            } else {
               dofs = yd*mi->mempitch;
               sofs = ys*mi->mempitch;
               size = lines*mi->mempitch;

               if (ys<yd) memcpy(membase+sofs, membase+dofs, size); else
                  memcpy(membase+dofs, membase+sofs, size);
            }

            rc = 1;
         } else { // 4 planes
         }
      }
   }
   return rc;
}

/// fill number of screen lines by color value
u32t vesaclear(con_modeinfo *mi, u32t ypos, u32t lines, u32t color) {
   u32t  rc = 0;
   if ((current_flags&CON_NOLFB)==0 && ypos<mi->height) {
      if (ypos+lines > mi->height) lines = mi->height-ypos;

      if (mi->physmap) {
         u32t   vbpps = BytesBP(mi->bits);

         if (vbpps==1) {
            if (mi->shadow) memset(mi->shadow+ypos*mi->shadowpitch, color,
               lines * mi->shadowpitch);
            if (mi->physmap) memset(mi->physmap+ypos*mi->mempitch, color,
               lines * mi->mempitch);
         } else
         if (vbpps>1) {
            if (mi->shadow) {
               con_fillblock(mi->shadow + ypos*mi->shadowpitch, 
                  mi->width, lines, mi->shadowpitch, color, vbpps);
            }
            if (mi->physmap) {
               con_fillblock(mi->physmap + ypos*mi->mempitch, 
                  mi->width, lines, mi->mempitch, color, vbpps);
            }
            rc = 1;
         } else { // 4 planes
         }
      }
   }
   return rc;
}
