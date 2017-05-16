/* ------------------------------------------------------------------------*/
/*                                                                         */
/*   VALIDATE.H                                                            */
/*                                                                         */
/*   defines the classes TValidator, TPXPictureValidator, TRangeValidator, */
/*   TFilterValidator, TLookupValidator, and TStringLookupValidator.       */
/*                                                                         */
/* ------------------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#if defined(Uses_TValidator) && !defined(__TValidator)
#define __TValidator

// TValidator Status constants

static const int
   vsOk     =  0,
   vsSyntax =  1,     // Error in the syntax of either a TPXPictureValidator
                      // or a TDBPictureValidator
// Validator option flags
   voFill     =  0x0001,
   voTransfer =  0x0002,
   voReserved =  0x00FC;

// TVTransfer constants

enum TVTransfer {vtDataSize, vtSetData, vtGetData};

// Abstract TValidator object

class TValidator : public TObject
#ifndef NO_TV_STREAMS
   , public TStreamable
#endif  // ifndef NO_TV_STREAMS
{
public:
   TValidator();
   virtual void error();
   virtual Boolean isValidInput(char* s, Boolean suppressFill);
   virtual Boolean isValid(const char* s);
   virtual ushort transfer(char *s, void* buffer, TVTransfer flag);
   Boolean validate(const char* s);

   ushort status;
   ushort options;

#ifndef NO_TV_STREAMS
protected:
   TValidator( StreamableInit );
   virtual void write( opstream& os );
   virtual void* read( ipstream& is );

private:
   virtual const char *streamableName() const  {return name;};

public:
   static TStreamable *build();
   static const char * const name;
#endif  // ifndef NO_TV_STREAMS

};

#endif  // Uses_TValidator


#if defined(Uses_TPXPictureValidator) && !defined(__TPXPictureValidator)
#define __TPXPictureValidator

// TPXPictureValidator result type

enum TPicResult {prComplete, prIncomplete, prEmpty, prError, prSyntax,
                 prAmbiguous, prIncompNoFill};

// TPXPictureValidator

class TPXPictureValidator : public TValidator {

   static const char * errorMsg;

public:
   TPXPictureValidator(const char* aPic, Boolean autoFill);
   ~TPXPictureValidator();
   virtual void error();
   virtual Boolean isValidInput(char* s, Boolean suppressFill);
   virtual Boolean isValid(const char* s);
   virtual TPicResult picture(char* input, Boolean autoFill);

private:
   void consume(char ch, char* input);
   void toGroupEnd(int& i, int termCh);
   Boolean skipToComma(int termCh);
   int calcTerm(int);
   TPicResult iteration(char* input, int termCh);
   TPicResult group(char* input, int termCh);
   TPicResult checkComplete(TPicResult rslt, int termCh);
   TPicResult scan(char* input, int termCh);
   TPicResult process(char* input, int termCh);
   Boolean syntaxCheck();

   int index, jndex;

protected:

   char* pic;

#ifndef NO_TV_STREAMS
   TPXPictureValidator( StreamableInit );
   virtual void write( opstream& os );
   virtual void* read( ipstream& is );

private:

   virtual const char *streamableName() const  {return name;};

public:
   static TStreamable *build();
   static const char * const name;
#endif  // ifndef NO_TV_STREAMS

};

#ifndef NO_TV_STREAMS
inline ipstream& operator >> ( ipstream& is, TValidator& v )
   { return is >> (TStreamable&)v; }
inline ipstream& operator >> ( ipstream& is, TValidator*& v )
   { return is >> (void *&)v; }

inline opstream& operator << ( opstream& os, TValidator& v )
   { return os << (TStreamable&)v; }
inline opstream& operator << ( opstream& os, TValidator* v )
   { return os << (TStreamable *)v; }
#endif  // ifndef NO_TV_STREAMS

#endif  // Uses_TPXPictureValidator


#if defined(Uses_TFilterValidator) && !defined(__TFilterValidator)
#define __TFilterValidator

// TFilterValidator

class TFilterValidator : public TValidator {

   static const char * errorMsg;

public:
   TFilterValidator(const char* aValidChars);
   ~TFilterValidator();
   virtual void error();
   virtual Boolean isValidInput(char* s, Boolean suppressFill);
   virtual Boolean isValid(const char* s);

protected:

   char* validChars;

#ifndef NO_TV_STREAMS
   TFilterValidator( StreamableInit );
   virtual void write( opstream& os);
   virtual void* read( ipstream& is );

private:
   virtual const char *streamableName() const  {return name;};

public:
   static TStreamable *build();
   static const char * const name;
#endif  // ifndef NO_TV_STREAMS

};

#ifndef NO_TV_STREAMS
inline ipstream& operator >> ( ipstream& is, TFilterValidator& v )
   { return is >> (TStreamable&)v; }
inline ipstream& operator >> ( ipstream& is, TFilterValidator*& v )
   { return is >> (void *&)v; }

inline opstream& operator << ( opstream& os, TFilterValidator& v )
   { return os << (TStreamable&)v; }
inline opstream& operator << ( opstream& os, TFilterValidator* v )
   { return os << (TStreamable *)v; }
#endif  // ifndef NO_TV_STREAMS

#endif  // Uses_TFilterValidator


#if defined(Uses_TRangeValidator) && !defined(__TRangeValidator)
#define __TRangeValidator

// TRangeValidator


class TRangeValidator : public TFilterValidator {

   static const char * validUnsignedChars;
   static const char * validSignedChars;
   static const char * errorMsg;

public:
   TRangeValidator(long aMin, long aMax);
   virtual void error();
   virtual Boolean isValid(const char* s);
   virtual ushort transfer(char* s, void* buffer, TVTransfer flag);

protected:
   long min;
   long max;

#ifndef NO_TV_STREAMS
   TRangeValidator( StreamableInit );
   virtual void write( opstream& os );
   virtual void* read( ipstream& is );

private:
   virtual const char *streamableName() const  {return name;};

public:
   static TStreamable *build();
   static const char * const name;
#endif  // ifndef NO_TV_STREAMS

};

#ifndef NO_TV_STREAMS
inline ipstream& operator >> ( ipstream& is, TRangeValidator& v )
   { return is >> (TStreamable&)v; }
inline ipstream& operator >> ( ipstream& is, TRangeValidator*& v )
   { return is >> (void *&)v; }

inline opstream& operator << ( opstream& os, TRangeValidator& v )
   { return os << (TStreamable&)v; }
inline opstream& operator << ( opstream& os, TRangeValidator* v )
   { return os << (TStreamable *)v; }
#endif  // ifndef NO_TV_STREAMS

#endif  // Uses_TRangeValidator

#if defined(Uses_TLookupValidator) && !defined(__TLookupValidator)
#define __TLookupValidator

// TLookupValidator

class TLookupValidator : public TValidator {
public:
   TLookupValidator() {};
   virtual Boolean isValid(const char* s);
   virtual Boolean lookup(const char* s);

#ifndef NO_TV_STREAMS
   static TStreamable *build();
   static const char * const name;
protected:
   TLookupValidator(StreamableInit);
private:
   virtual const char *streamableName() const { return name; }
#endif  // ifndef NO_TV_STREAMS

};

#ifndef NO_TV_STREAMS
inline ipstream& operator >> ( ipstream& is, TLookupValidator& v )
   { return is >> (TStreamable&)v; }
inline ipstream& operator >> ( ipstream& is, TLookupValidator*& v )
   { return is >> (void *&)v; }

inline opstream& operator << ( opstream& os, TLookupValidator& v )
   { return os << (TStreamable&)v; }
inline opstream& operator << ( opstream& os, TLookupValidator* v )
   { return os << (TStreamable *)v; }
#endif  // ifndef NO_TV_STREAMS

#endif


#if defined(Uses_TStringLookupValidator) && !defined(__TStringLookupValidator)
#define __TStringLookupValidator

// TStringLookupValidator

class TStringLookupValidator : public TLookupValidator {

   static const char * errorMsg;

public:
   TStringLookupValidator(TStringCollection* aStrings);
   ~TStringLookupValidator();
   virtual void error();
   virtual Boolean lookup(const char* s);

   void newStringList(TStringCollection* aStrings);
protected:

   TStringCollection* strings;

#ifndef NO_TV_STREAMS
   TStringLookupValidator(StreamableInit);
   virtual void write(opstream& os);
   virtual void* read(ipstream& is);

private:
   virtual const char *streamableName() const { return name; }

public:
   static TStreamable *build();
   static const char * const name;
#endif  // ifndef NO_TV_STREAMS

};

#ifndef NO_TV_STREAMS
inline ipstream& operator >> ( ipstream& is, TStringLookupValidator& v )
   { return is >> (TStreamable&)v; }
inline ipstream& operator >> ( ipstream& is, TStringLookupValidator*& v )
   { return is >> (void *&)v; }

inline opstream& operator << ( opstream& os, TStringLookupValidator& v )
   { return os << (TStreamable&)v; }
inline opstream& operator << ( opstream& os, TStringLookupValidator* v )
   { return os << (TStreamable *)v; }
#endif  // ifndef NO_TV_STREAMS

#endif  // Uses_TStringLookupValidator
