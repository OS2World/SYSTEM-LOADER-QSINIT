#include "qshm.h"
#include "pci.h"
#include "qslog.h"
#include <stdlib.h>
#include "internal.h"
#include "qsstor.h"
#include "qsint.h"
#include "qssys.h"
#include "qsutil.h"

static u32t num_buses = 0;
static u8t *funcmask  = 0;     ///<256 * 32 array of bits for function presence

static void pci_devdump(u8t bus, u8t slot, u8t func) {
   u32t  buf[4];
   int    ii;
   log_it(3, "pci %02X:%02X.%d config space:\n", bus, slot, func);
   for (ii=0; ii<64; ii++) {
      buf[ii&3] = hlp_pciread(bus, slot, func, ii<<2, 4);
      if ((ii&3)==3) log_it(3,"%03X: %16b\n", (ii&~3)<<2, &buf);
   }
}

// do not use max values here for debug reason
void _std log_pcidump(void) {
   u32t bus, slot, func;
   log_it(3, "all 256 buses scan can take a minutes on old PCs!\n");
   for (bus=0; bus<256; bus++)
      for (slot=0; slot<32; slot++)
         for (func=0; func<8; func++) {
            if (hlp_pciread(bus,slot,func,PCI_CLASS_REV,4) == 0xFFFFFFFF)
               continue;
            pci_devdump(bus, slot, func);

            if (func == 0)
               if ((hlp_pciread(bus,slot,func,PCI_HEADER_TYPE,1)&0x80) == 0)
                  break;
         }
}

static void init_enum(void) {
   static int processing = 0;
   mt_swlock();
   // simultaneous init call from different threads?
   if (processing) { 
      mt_swunlock();
      while (processing) usleep(32000);
      return; 
   } else {
      u32t bus, slot, func;
      u32t scan_all = sto_dword(STOKEY_PCISCAN),
            max_bus = 256, rc = 0;
      processing = 1;
      mt_swunlock();
      // init counters
      if (!funcmask) funcmask = (u8t*)malloc(256*32);
      mem_zero(funcmask);
      // ask PCI BIOS
      if (!scan_all) max_bus = (hlp_querybios(QBIO_PCI)>>8&0xFF) + 1;
      
      for (bus=0; bus<max_bus; bus++)
         for (slot=0; slot<32; slot++)
            for (func=0; func<8; func++) {
               if (hlp_pciread(bus,slot,func,PCI_CLASS_REV,4) == 0xFFFFFFFF)
                  continue;
               // update max values
               funcmask[bus<<5|slot]|=1<<func;
               if (rc<=bus) rc = bus+1;
      
               if (func == 0)
                  if ((hlp_pciread(bus,slot,func,PCI_HEADER_TYPE,1)&0x80) == 0)
                     break;
            }
      log_it(2, "number of buses: %d\n", rc);
      num_buses = rc;
   }
}

/** enum PCI devices.
    @param  offsets  array of offsets in configuration data
    @param  sizes    array of sizes in configuration data
    @param  values   array of values to compare with configuration data
    @param  count,   number of entries in offsets/sizes/values arrays, can be 0
    @param  index    zero-based index of device with required parameters
    @param  dev      device data (output, optional)
    @param  usenext  flag 1 to start enumeration from dev->bus,slot,func
    @param  nodupes  detect devices, mapped to all 8 functions of selected slot
    @return success flag (1/0) */
static int def_enum(u8t *offsets, u8t *sizes, u32t *values, int count,
                    u16t index, pci_location *dev, int usenext, int nodupes)
{
   u32t bus, slot, func,
        prevbus, prevslot;
   if (usenext && !dev || count<0) return 0;
   if (!num_buses) init_enum();
   // increment location
   if (usenext) {
      prevbus  = dev->bus;
      prevslot = dev->slot;
      if (++dev->func>=8) {
         dev->func = 0;
         if (++dev->slot>=32) {
            dev->slot = 0;
            if (++dev->bus>=num_buses) return 0;
         }
      }
      usenext = 1;
   }
   // searching
   for (bus=usenext==1?dev->bus:0; bus<num_buses; bus++) {
      for (slot=usenext==1?dev->slot:0; slot<32; slot++)
         // at least one bit present
         if (funcmask[bus<<5|slot])
         for (func=usenext==1?dev->func:0; func<8; func++) {
            // used on duplicate check below
            if (usenext==1) usenext++;
            // is entry present?
            if ((funcmask[bus<<5|slot]&1<<func)!=0) {
               u32t clrev = hlp_pciread(bus,slot,func,PCI_CLASS_REV,4);
               if (clrev == 0xFFFFFFFF) continue; else {
                  int ii, success = !count;
                  for (ii=0; ii<count; ii++)
                     if (hlp_pciread(bus,slot,func,offsets[ii],sizes[ii])!=values[ii])
                        break; else
                     if (ii==count-1) success = 1;

                  if (success) {
                     u32t vendor = hlp_pciread(bus,slot,func,PCI_VENDOR_ID,4);
                     // additional check
                     if ((vendor&0xFFFF)==0 || (vendor&0xFFFF)==0xFFFF) continue;
                     // check for eight equal functions in one slot
                     if (usenext && nodupes && dev && func>0 && (funcmask[bus<<5|slot]&1)!=0)
                        if (prevbus==bus && prevslot==slot && dev->vendorid==(vendor&0xFFFF)
                           && dev->deviceid == vendor>>16)
                        {
                           u32t func0[64], funcx[64];
                           int kk;
                           for (kk=0; kk<64; kk++) {
                              funcx[kk] = hlp_pciread(bus, slot, func, kk<<2, 4);
                              func0[kk] = hlp_pciread(bus, slot, 0   , kk<<2, 4);
                           }
                           if (!memcmp(funcx, func0, 256)) continue;
                        }

                     if (index) index--; else {
                        if (dev) {
                           dev->bus       = bus;
                           dev->slot      = slot;
                           dev->func      = func;
                           dev->classcode = clrev>>16;
                           dev->progface  = clrev>>8;
                           dev->vendorid  = vendor;
                           dev->deviceid  = vendor>>16;
                           dev->header    = hlp_pciread(bus,slot,func,PCI_HEADER_TYPE,1);
                        }
                        return 1;
                     }
                  }
                  if (func == 0)
                     if ((hlp_pciread(bus,slot,func,PCI_HEADER_TYPE,1)&0x80) == 0)
                        break;
               }
            }
         }
   }
   return 0;
}

int _std START_EXPORT(hlp_pcifind)(u16t vendor_id, u16t device_id, u16t index,
                                   pci_location *dev, int nodupes)
{
   static u8t offsets[] = {PCI_VENDOR_ID, PCI_DEVICE_ID},
                sizes[] = {2, 2};
   u32t       values[2];
   values[0] = vendor_id;
   values[1] = device_id;
   return def_enum(offsets, sizes, values, 2, index, dev, 0, nodupes);
}

int _std hlp_pciclass(u32t classcode, u16t index, pci_location *dev, int nodupes) {
   static u8t offset = PCI_DEV_CLASS, size = 2;
   return def_enum(&offset, &size, &classcode, 1, index, dev, 0, nodupes);
}

static u32t getsize(u32t base, u32t maxbase, u32t mask) {
   u32t size = mask & maxbase;
   if (!size) return 0;

   size = (size&~(size-1))-1;
   if (base==maxbase && ((base|size)&mask)!=mask) return 0;
   return size;
}

u32t _std hlp_pcigetbase(pci_location *dev, u64t *bases, u64t *sizes) {
   if (!dev||!bases||!sizes) return 0;
   if (!num_buses) init_enum();
   if (!hlp_pciexist(dev)) return 0;

   if (dev->bus>=num_buses) return 0; else {
      u32t vendor = hlp_pciread(dev->bus,dev->slot,dev->func,PCI_VENDOR_ID,4),
            clrev = hlp_pciread(dev->bus,dev->slot,dev->func,PCI_CLASS_REV,4),
           maxcnt = 0,
          rccount = 0;
      int   svint;
      u8t     pos;

      if (clrev == 0xFFFFFFFF) return 0;
      dev->vendorid  = vendor;
      dev->deviceid  = vendor>>16;
      if (dev->vendorid==0 || dev->vendorid==0xFFFF) return 0;
      dev->classcode = clrev>>16;
      dev->progface  = clrev>>8;
      dev->header    = hlp_pciread(dev->bus,dev->slot,dev->func,PCI_HEADER_TYPE,1);

      switch (dev->header&0x7F) {
         case PCI_HEADER_NORMAL : maxcnt = 6; break;
         case PCI_HEADER_BRIDGE : maxcnt = 2; break;
         case PCI_HEADER_CARDBUS: maxcnt = 1; break;
         default: return 0;
      }
      /* address read is not an atomic op, but just disable ints here, instead
         of MT lock */
      svint = sys_intstate(0);

      for (pos=PCI_BASE_ADDR0; maxcnt>0; maxcnt--,pos+=4) {
         u32t addr = hlp_pciread(dev->bus, dev->slot, dev->func, pos, 4),
              size = 0;
         hlp_pciwrite(dev->bus, dev->slot, dev->func, pos, 4, FFFF);
         size = hlp_pciread(dev->bus, dev->slot, dev->func, pos, 4);
         hlp_pciwrite(dev->bus, dev->slot, dev->func, pos, 4, addr);

         if (!size || size==FFFF) continue;
         if (addr==FFFF) addr = 0;

         if ((addr&PCI_ADDR_SPACE)==0) {
            u32t addrh = 0, sizeh = 0;
            size = getsize(addr, size, PCI_ADDR_MEM_MASK);
            if (!size) continue;

            if ((addr&PCI_ADDR_MEMTYPE_MASK)==PCI_ADDR_MEMTYPE_64) {
               pos+=4; maxcnt--;
               addrh = hlp_pciread(dev->bus, dev->slot, dev->func, pos, 4);
               hlp_pciwrite(dev->bus, dev->slot, dev->func, pos, 4, FFFF);
               sizeh = hlp_pciread(dev->bus, dev->slot, dev->func, pos, 4);
               hlp_pciwrite(dev->bus, dev->slot, dev->func, pos, 4, addrh);
               sizeh = getsize(addrh, sizeh, FFFF);
            }
            bases[rccount] = (u64t)addrh<<32 | addr;
            sizes[rccount] = (u64t)sizeh<<32 | size;
         } else {
            size = getsize(addr, size, PCI_ADDR_IO_MASK & 0xFFFF);
            if (!size) continue;
            bases[rccount] = addr;
            sizes[rccount] = size;
         }
         sizes[rccount++]++;
      }
      sys_intstate(svint);

      return rccount;
   }
}

int _std START_EXPORT(hlp_pcigetnext)(pci_location *dev, int init, int nodupes) {
   if (!dev) return 0;
   return def_enum(0, 0, 0, 0, 0, dev, !init, nodupes);
}

int  _std hlp_pciexist(pci_location *dev) {
   if (!dev) return 0;
   if (!num_buses) init_enum();
   if (dev->bus>=num_buses || dev->slot>=32 || dev->func>=8) return 0;

   if ((funcmask[dev->bus<<5|dev->slot]&1<<dev->func)==0) return 0; else {
       u32t clrev = hlp_pciread(dev->bus,dev->slot,dev->func,PCI_CLASS_REV,4),
           vendor;
       if (clrev == 0xFFFFFFFF) return 0;
       vendor = hlp_pciread(dev->bus,dev->slot,dev->func,PCI_VENDOR_ID,4);
       // additional check
       if ((vendor&0xFFFF)==0 || (vendor&0xFFFF)==0xFFFF) return 0;
       dev->classcode = clrev>>16;
       dev->progface  = clrev>>8;
       dev->vendorid  = vendor;
       dev->deviceid  = vendor>>16;
       dev->header    = hlp_pciread(dev->bus,dev->slot,dev->func,PCI_HEADER_TYPE,1);
       return 1;
   }
}
