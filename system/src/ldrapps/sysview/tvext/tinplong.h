#if defined( Uses_TInputLong ) && !defined( __TInputLong )
#define __TInputLong

//flags for TInputLong constructor

const int
  ilHex = 1,          //will enable hex input with leading '0x'
  ilBlankEqZero = 2,  //No input (blank) will be interpreted as '0'
  ilDisplayHex = 4,   //Number displayed as hex when possible
  ilUlong = 8,        //Limits and data are actually ulong!
  ilPureHex = 16;     //Only hex, without 0x prefix

class TRect;
class TEvent;

class TInputLong : public TInputLine
{
    long  getCurrentValue(char **eptr = 0);
    char *formHexStr(char *s, long L);
public:

    TInputLong( const TRect& bounds, int aMaxLen, long lowerLim,
		    long upperLim, ushort flags, const char* labelName = 0 );
    ~TInputLong();

    virtual size_t dataSize();
    virtual void getData( void *rec );
    virtual void handleEvent( TEvent& event );
    virtual void setData( void *rec );
    virtual Boolean rangeCheck();
    virtual void error();
    virtual Boolean valid(ushort command);

    ushort ilOptions;
    long lLim, uLim;
    char* label;

private:

    virtual const char *streamableName() const
	{ return name; }

protected:

#ifndef NO_TV_STREAMS
    TInputLong( StreamableInit );
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
inline ipstream& operator >> ( ipstream& is, TInputLong& cl )
    { return is >> (TStreamable&)cl; }
inline ipstream& operator >> ( ipstream& is, TInputLong*& cl )
    { return is >> (void *&)cl; }

inline opstream& operator << ( opstream& os, TInputLong& cl )
    { return os << (TStreamable&)cl; }
inline opstream& operator << ( opstream& os, TInputLong* cl )
    { return os << (TStreamable *)cl; }
#endif

#endif  // Uses_TInputLong

