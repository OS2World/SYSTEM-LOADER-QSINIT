#if defined(__QSINIT__)
#  include <stdlib.h>
#  include <direct.h>
#  define MAXPATH               _MAX_PATH
#  define MAXDRIVE              _MAX_DRIVE
#  define MAXDIR                _MAX_DIR
#  define MAXFILE               _MAX_FNAME
#  define MAXEXT                _MAX_EXT
#  define FA_DIREC              _A_SUBDIR
#  define FA_RDONLY             _A_RDONLY
#  define FA_ARCH               _A_ARCH
#  define findnext              _dos_findnext
#  define findfirst(p,blk,attr) (_dos_findfirst(p,attr,blk)?-1:0)
#  define findclose(p)          _dos_findclose(p)
#  define ffblk                 dir_t
#  define ff_attrib             d_attr
#  define ff_ftime              d_wtime
#  define ff_fdate              d_wtime
#  define ff_fsize              d_size
#  define ff_name               d_name
#elif defined(__WATCOMC__)
#  include <stdlib.h>
#  include <direct.h>
#  include <dos.h>
#  define MAXPATH               _MAX_PATH
#  define MAXDRIVE              _MAX_DRIVE
#  define MAXDIR                _MAX_DIR
#  define MAXFILE               _MAX_FNAME
#  define MAXEXT                _MAX_EXT
#  define FA_DIREC              _A_SUBDIR
#  define FA_RDONLY             _A_RDONLY
#  define FA_ARCH               _A_ARCH
#  define findnext              _dos_findnext
#  define findfirst(p,blk,attr) (_dos_findfirst(p,attr,blk)?-1:0)
#  define findclose(p)          _dos_findclose(p)
#  define ffblk                 find_t
#  define ff_attrib             attrib
#  define ff_ftime              wr_time
#  define ff_fdate              wr_date
#  define ff_fsize              size
#  define ff_name               name
#elif defined(__MSVC__)
#  include <stdlib.h>
#  include <direct.h>
#  include <io.h>
#  include <dos.h>
#  include <windows.h>
#  define MAXPATH               _MAX_PATH
#  define MAXDRIVE              _MAX_DRIVE
#  define MAXDIR                _MAX_DIR
#  define MAXFILE               _MAX_FNAME
#  define MAXEXT                _MAX_EXT
#  define FA_DIREC              _A_SUBDIR
#  define FA_RDONLY             _A_RDONLY
#  define FA_ARCH               _A_ARCH
#  define findnext(h,p)         (FindNextFile((HANDLE)(h),p)?0:1)
#  define findfirst(p,blk,attr) (long)(FindFirstFile(p,blk))
#  define findclose(p)          FindClose((HANDLE)(p))
#  define ffblk                 WIN32_FIND_DATA
#  define ff_attrib             dwFileAttributes
#  define ff_ftime              ftLastWriteTime
#  define ff_fdate              ftLastWriteTime
#  define ff_fsize              nFileSizeLow
#  define ff_name               cFileName
#elif defined(__IBMCPP__)
#  include <stdlib.h>
#  include <direct.h>
#  include <io.h>
#ifdef __OS2__
#ifndef ffblk
#  define INCL_DOSFILEMGR
#  include <os2.h>
#  define MAXPATH               CCHMAXPATH
#  define MAXDRIVE              3
#  define MAXDIR                CCHMAXPATH
#  define MAXFILE               CCHMAXPATH
#  define MAXEXT                CCHMAXPATH
#  define FA_DIREC              FILE_DIRECTORY
#  define FA_RDONLY             FILE_READONLY
#  define FA_ARCH               FILE_ARCHIVED
#  define ffblk                 FILEFINDBUF3
#  define ff_attrib             attrFile
#  define ff_ftime              ftimeLastWrite
#  define ff_fdate              fdateLastWrite
#  define ff_fsize              cbFile
#  define ff_name               achName
inline long findfirst(const char *mask,ffblk *blk,unsigned attr) {
   blk->oNextEntryOffset=0;
   HDIR handle=HDIR_CREATE;
   ULONG cnt=1;
   if (DosFindFirst(mask,&handle,attr,blk,sizeof(ffblk),&cnt,FIL_STANDARD))
      return -1;
   return handle;
}
inline long findnext(long handle,ffblk *blk) {
   blk->oNextEntryOffset=0;
   ULONG cnt=1;
   return DosFindNext((HDIR)handle,blk,sizeof(ffblk),&cnt);
}
#  define findclose(p)          DosFindClose((HDIR)(p))
#endif // ffblk - no duplicates...
#else
#  include <windows.h>
#  define MAXPATH               _MAX_PATH
#  define MAXDRIVE              _MAX_DRIVE
#  define MAXDIR                _MAX_DIR
#  define MAXFILE               _MAX_FNAME
#  define MAXEXT                _MAX_EXT
#  define FA_DIREC              FILE_ATTRIBUTE_DIRECTORY
#  define FA_RDONLY             FILE_ATTRIBUTE_READONLY
#  define FA_ARCH               FILE_ATTRIBUTE_ARCHIVE
#  define findnext(h,p)         (FindNextFile((HANDLE)(h),p)?0:1)
#  define findfirst(p,blk,attr) (long)(FindFirstFile(p,blk))
#  define findclose(p)          FindClose((HANDLE)(p))
#  define ffblk                 WIN32_FIND_DATA
#  define ff_attrib             dwFileAttributes
#  define ff_ftime              ftLastWriteTime
#  define ff_fdate              ftLastWriteTime
#  define ff_fsize              nFileSizeLow
#  define ff_name               cFileName
#endif
#else
#  define findclose(p)
#  if !defined( __DIR_H )
#    include <Dir.h>
#  endif  // __DIR_H
#endif
