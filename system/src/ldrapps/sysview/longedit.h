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

typedef struct {
   le_cluster_t      clusters;   ///< number of "clusters" in data
   unsigned long  clustersize;   ///< size of "cluster" (must be a multiple of 16)
   int             showtitles;   ///< titles presence flag
   int               userdata;   ///< user data for calls
   int              noreaderr;   ///< do not show error message box on read error
   int  (*read)     (int userdata, le_cluster_t cluster, void *data);  ///< read data
   int  (*write)    (int userdata, le_cluster_t cluster, void *data);  ///< write data
   void (*gettitle) (int userdata, le_cluster_t cluster, char *title); ///< get cluster title (128 chars)
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

   void updateVcl();
   void freeVcl();
   /// read/write op for cluster (with error dialogs)
   int  rwAction(le_cluster_t cluster, void *data, int write);
public:
   TLongEditor(const TRect& bounds);

   virtual void changeBounds(const TRect &bounds);
   virtual TPalette &getPalette() const;

   virtual void handleEvent(TEvent& event);
   virtual void draw();

   virtual size_t dataSize();
   virtual void getData(void *rec);
   virtual void setData(void *rec);
   virtual Boolean valid(ushort command);

   int  isChanged();
   /// ask and commit changed data
   void commitChanges();
   /** notify about data was changed by external activity.
       This call DROP all unsaved changes was made */
   void dataChanged();
   /// query line number under cursor
   int  cursorLine(le_cluster_t *cluster = 0);
   // return 0 if cluster is not in visible area
   uchar *changedArray(le_cluster_t cluster);
   // get cluster size
   unsigned long clusterSize();

   enum posmoveType { posStart,       ///< pos to 1st cluster
                      posPageUp,      ///< press page up
                      posPageDown,    ///< press page down
                      posEnd,         ///< pos to last cluster
                      posUp,          ///< press arrow up
                      posDown,        ///< press arrow down
                      posSet,         ///< pos to "cluster" parameter
                      posUndo,        ///< press backSpace (undo prev. byte)
                      posNullOp       ///< null op (just check for update)
                    };
   /** change the pos of visible area.
       @param mv       Type of action
       @param cluster  Pos to this cluster (only for mv==posSet) */
   void processKey(posmoveType mv, le_cluster_t cluster = 0);
   /// change pos of visible area and set cursor to line/col of posin
   void posToCluster(le_cluster_t cluster, unsigned long posin = 0);

   virtual void shutDown();
};

#endif // __hexlong_h
