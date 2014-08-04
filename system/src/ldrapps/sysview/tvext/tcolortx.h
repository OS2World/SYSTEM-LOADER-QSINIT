#if defined( Uses_TColoredText ) && !defined( __TColoredText )
#define __TColoredText

class far TRect;

class TColoredText : public TStaticText
{

public:

    TColoredText( const TRect& bounds, const char *aText, ushort attribute );

    virtual void draw();
    virtual ushort getTheColor();

protected:

    ushort attr;

private:

    virtual const char *streamableName() const
      { return name; }

protected:

#ifndef NO_TV_STREAMS
    TColoredText( StreamableInit );
    virtual void write( opstream& );
    virtual void *read( ipstream& );
#endif

public:

    static const char * const near name;
#ifndef NO_TV_STREAMS
    static TStreamable *build();
#endif

};

#ifndef NO_TV_STREAMS
inline ipstream& operator >> ( ipstream& is, TColoredText& cl )
    { return is >> (TStreamable&)cl; }
inline ipstream& operator >> ( ipstream& is, TColoredText*& cl )
    { return is >> (void *&)cl; }

inline opstream& operator << ( opstream& os, TColoredText& cl )
    { return os << (TStreamable&)cl; }
inline opstream& operator << ( opstream& os, TColoredText* cl )
    { return os << (TStreamable *)cl; }
#endif

#endif  // Uses_TColoredText

