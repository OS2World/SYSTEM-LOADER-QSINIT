//
// QSINIT "sysview" module
// huge cluster-based data binary editor
//

#if !defined( __hexlong_h )
#define __hexlong_h

#define Uses_TEvent
#define Uses_TRect
#define Uses_TView
#define Uses_TGroup
#define Uses_TKeys
#define Uses_TScreen
#define Uses_MsgBox

#include <tv.h>

typedef unsigned long long le_cluster_t;

/** editor settings.
    Expansion & truncation is possible only when setsize function available.
    In this case one more byte is always accessible for editing - next after
    the "end of data" position. */
typedef struct {
   le_cluster_t      clusters;   ///< number of "clusters" in data
   unsigned long  clustersize;   ///< size of "cluster" (must be a multiple of 16)
   unsigned long      lcbytes;   ///< bytes in the last cluster
   unsigned long     poswidth;   ///< # of position digits (only when showtitles==0, 4..16, 0 for default)
   int             showtitles;   ///< titles presence flag
   int               userdata;   ///< user data for calls
   int              noreaderr;   ///< do not show error message box on read error
   /// read data
   int  (*read)     (int userdata, le_cluster_t cluster, void *data);
   /** write data.
       Pointer is optional, absence mean r/o data.
       Note, that write function should handle the last cluster correctly.
       @param userdata    user data field from this struct
       @param cluster     new number of clusters
       @param data        data to write 
       @return success flag (1/0) */
   int  (*write)    (int userdata, le_cluster_t cluster, void *data);
   /// get cluster title (128 chars)
   void (*gettitle) (int userdata, le_cluster_t cluster, char *title);
   /** ask about new data size.
       Pointer is optional, absence mean constant size.

       Function should accept increasing size by 1 and truncation. In case of
       trunction it can show confirmation dialog.

       @param userdata    user data field from this struct
       @param cluster     new number of clusters
       @param last        size of the last cluster
       @return success flag (1/0) */
   int  (*setsize)  (int userdata, le_cluster_t clusters, unsigned long last);
   /** ask about writing.
       @param userdata    user data field from this struct
       @param cluster     cluster to write
       @param data        data to be written
       @return 1 for yes, 0 for no (drop changes) and -1 to cancel and stay in cluster. */
   int  (*askupdate)(int userdata, le_cluster_t cluster, void *data);
} TLongEditorData;

class TLongEditor :public TView {
protected:
   TLongEditorData   uinfo;
   uchar          *changed;   ///< changed data for current clusters
   uchar              *vcl;   ///< visible clusters data
   uchar          *vcflags;   ///< visible clusters flags
   le_cluster_t    clstart;   ///< first cluster of visible data
   unsigned long   clcount;   ///< count of clusters in vcl array
   unsigned long   clshift;   ///< bit shift of cluster size
   int              clline;   ///< first visible line in clstart cluster
   int         pos10,pos60;
   unsigned long  ilcbytes;   ///< initial value of lcbytes in last cluster

   void updateVcl(uchar *chsave = 0, unsigned long chofs = 0);
   void freeVcl();
   /// read/write op for cluster (with error dialogs)
   int  rwAction(le_cluster_t cluster, void *data, int write);
   /** window can be positioned to the 1st pos of a new cluster.
       return 1 only when setsize is available & last cluster is full. */
   unsigned long newCluster();
   /** total number of lines beween this position & the end of viewable data.
       Usable for the last clusters only (to avoid overflow in result) */
   unsigned long untilEOV(le_cluster_t cluster, int line);
   /// find position of changed data in vcl
   unsigned long changedPos();
public:
   TLongEditor(const TRect& bounds);

   virtual void       changeBounds(const TRect &bounds);
   virtual TPalette&  getPalette() const;

   virtual void       handleEvent(TEvent& event);
   virtual void       draw();

   virtual size_t     dataSize();
   virtual void       getData(void *rec);
   virtual void       setData(void *rec);
   virtual Boolean    valid(ushort command);

   int                isChanged();
   /// ask and commit changed data
   void               commitChanges();
   /** notify about data was changed by external activity.
       This call DROP all unsaved changes was made */
   void               dataChanged();
   /// query line number under cursor
   int                cursorLine(le_cluster_t *cluster = 0);
   /// query offset in cluster under cursor
   unsigned long      cursorByte(le_cluster_t *cluster = 0);
   // return 0 if cluster is not in visible area
   uchar*             changedArray(le_cluster_t cluster);
   // get cluster size
   unsigned long      clusterSize();
   // full data size in bytes
   unsigned long long fullSize();

   enum posmoveType { posStart,       ///< pos to the 1st cluster
                      posPageUp,      ///< press page up
                      posPageDown,    ///< press page down
                      posEnd,         ///< pos to the last cluster
                      posUp,          ///< press arrow up
                      posDown,        ///< press arrow down
                      posSet,         ///< pos to "cluster" parameter
                      posUndo,        ///< press backSpace (undo prev. byte)
                      posNullOp,      ///< null op (just check for update)
                      posEOV          ///< pos to the end of data (last cluster/line)
                    };
   /** change the pos of visible area.
       @param mv       Type of action
       @param cluster  Pos to this cluster (only for mv==posSet) */
   void processKey(posmoveType mv, le_cluster_t cluster = 0);
   /// change pos of visible area and set cursor to line/col of posin
   void posToCluster(le_cluster_t cluster, unsigned long posin = 0);
   /// try to truncate data (in editable size mode)
   int  truncateData();

   virtual void shutDown();
};

#endif // __hexlong_h
