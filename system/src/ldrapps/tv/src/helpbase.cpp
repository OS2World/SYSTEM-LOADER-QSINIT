/*
  NO_TV_STREAMS WORKS WITH THE HELP FIXED BY URI BECHAR
*/

/*------------------------------------------------------------*/
/* filename -       HelpBase.cpp                              */
/*                                                            */
/* function(s)                                                */
/*                  Member function(s) of following classes   */
/*                      THelpTopic                            */
/*                      THelpIndex                            */
/*                      THelpFile                             */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define Uses_TStreamableClass
#define Uses_TPoint
#define Uses_TStreamable
#define Uses_ipstream
#define Uses_opstream
#define Uses_fpstream
#define Uses_TRect
#include <tv.h>

#include "helpbase.h"
#include "util.h"


#include <string.h>
#include <limits.h>
//#include <sys\stat.h>
#ifndef __QSINIT__
#include <ctype.h>
#include <io.h>
#endif

TCrossRefHandler crossRefHandler = notAssigned;

#ifndef NO_TV_STREAMS

// THelpTopic

const char *const THelpTopic::name = "THelpTopic";

TStreamable *THelpTopic::build() {
   return new THelpTopic(streamableInit);
}


TStreamableClass RHelpTopic(THelpTopic::name,
                            THelpTopic::build,
                            __DELTA(THelpTopic)
                           );
#endif  // ifndef NO_TV_STREAMS

THelpTopic::THelpTopic() : TObject() {
   paragraphs = 0;
   numRefs = 0;
   crossRefs = 0;
   width = 0;
   lastOffset = 0;
   lastLine = INT_MAX;
   lastParagraph = 0;
};

void THelpTopic::disposeParagraphs() {
   TParagraph *p, *t;

   p = paragraphs;
   while (p != 0) {
      t = p;
      p = p->next;
      delete t->text;
      delete t;
   }
}


THelpTopic::~THelpTopic() {
   TCrossRef *crossRefPtr;

   disposeParagraphs();
   if (crossRefs != 0) {
      crossRefPtr = (TCrossRef *)crossRefs;
      delete [] crossRefPtr;
   }
}

void THelpTopic::addCrossRef(TCrossRef ref) {
   TCrossRef *p;
   TCrossRef *crossRefPtr;

   p =  new TCrossRef[numRefs+1];
   if (numRefs > 0) {
      crossRefPtr = crossRefs;
      memmove(p, crossRefPtr, numRefs * sizeof(TCrossRef));
      delete [] crossRefPtr;
   }
   crossRefs = p;
   crossRefPtr = crossRefs + numRefs;
   *crossRefPtr = ref;
   ++numRefs;
}


void THelpTopic::addParagraph(TParagraph *p) {
   TParagraph  *pp, *back;

   if (paragraphs == 0)
      paragraphs = p;
   else {
      pp = paragraphs;
      back = pp;
      while (pp != 0) {
         back = pp;
         pp = pp->next;
      }
      back->next = p;
   }
   p->next = 0;
}

void THelpTopic::getCrossRef(int i, TPoint &loc, uchar &length,
                             int &ref) {
   int oldOffset, curOffset, offset, paraOffset;
   TParagraph *p;
   int line;
   TCrossRef *crossRefPtr;

   paraOffset = 0;
   curOffset = 0;
   oldOffset = 0;
   line = 0;
   crossRefPtr = crossRefs + i;
   offset = crossRefPtr->offset;
   p = paragraphs;
   while (paraOffset + curOffset < offset) {
      oldOffset = paraOffset + curOffset;
      wrapText(p->text, p->size, curOffset, p->wrap);
      ++line;
      if (curOffset >= p->size) {
         paraOffset += p->size;
         p = p->next;
         curOffset = 0;
      }
   }
   loc.x = offset - oldOffset - 1;
   loc.y = line;
   length = crossRefPtr->length;
   ref = crossRefPtr->ref;
}

char *THelpTopic::getLine(int line) {
   int offset, i;
   TParagraph *p;
   static char buffer[maxViewWidth];    // bug fix (added static) ig 14.04.95

   if (lastLine < line) {
      i = line;
      line -= lastLine;
      lastLine = i;
      offset = lastOffset;
      p = lastParagraph;
   } else {
      p = paragraphs;
      offset = 0;
      lastLine = line;
   }
   buffer[0] = 0;
   while (p != 0) {
      while (offset < p->size) {
         --line;
         strcpy(buffer, wrapText(p->text, p->size, offset, p->wrap));
         if (line == 0) {
            lastOffset = offset;
            lastParagraph = p;
            return buffer;
         }
      }
      p = p->next;
      offset = 0;
   }
   buffer[0] = 0;
   return buffer;
}

int THelpTopic::getNumCrossRefs() {
   return numRefs;
}

int THelpTopic::numLines() {
   int offset, lines;
   TParagraph *p;

   offset = 0;
   lines = 0;
   p = paragraphs;
   while (p != 0) {
      offset = 0;
      while (offset < p->size) {
         ++lines;
         wrapText(p->text, p->size, offset, p->wrap);
      }
      p = p->next;
   }
   return lines;
}

void THelpTopic::setCrossRef(int i, TCrossRef &ref) {
   TCrossRef *crossRefPtr;

   if (i < numRefs) {
      crossRefPtr = crossRefs + i;
      *crossRefPtr = ref;
   }
}

void THelpTopic::setNumCrossRefs(int i) {
   TCrossRef  *p, *crossRefPtr;

   if (numRefs == i)
      return;
   p = new TCrossRef[i];
   if (numRefs > 0) {
      crossRefPtr = crossRefs;
      if (i > numRefs)
         memmove(p, crossRefPtr, numRefs * sizeof(TCrossRef));
      else
         memmove(p, crossRefPtr, i * sizeof(TCrossRef));

      delete [] crossRefPtr;
   }
   crossRefs = p;
   numRefs = i;
}


void THelpTopic::setWidth(int aWidth) {
   width = aWidth;
}

static Boolean isBlank(char ch) {
   if (isspace(uchar(ch)))
      return True;
   else
      return False;
}

int scan(char *p, int offset, char c) {
   char *temp1, *temp2;

   temp1 = p + offset;
   temp2 = strchr(temp1, c);
   if (temp2 == 0)
      return maxViewWidth;
   else {
      if ((int)(temp2 - temp1) <= maxViewWidth)
         return (int)(temp2 - temp1) + 1;
      else
         return maxViewWidth;
   }
}

void textToLine(void *text, int offset, int length, char *line) {
   strncpy(line, (char *)text+offset, length);
   line[length] = 0;
}

char *THelpTopic::wrapText(char *text, int size,
                           int &offset, Boolean wrap) {
   static char line[maxViewWidth];      // bug fix (added static) ig 14.04.95
   int i;

   i = scan(text, offset, '\n');
   if (i + offset > size)
      i = size - offset;
   if ((i >= width) && (wrap == True)) {
      i = offset + width;
      if (i > size)
         i = size;
      else {
         while ((i > offset) && !(isBlank(text[i])))
            --i;
         if (i == offset) {
            i = offset + width;
            while ((i < size) && !isBlank(text[i]))
               ++i;
            if (i < size) ++i;          // skip Blank
         } else
            ++i;
      }
      if (i == offset)
         i = offset + width;
      i -= offset;
   }
   textToLine(text, offset, i, line);
   if (line[strlen(line) - 1] == '\n')
      line[strlen(line) - 1] = 0;
   offset += i;
   return line;
}

// THelpIndex

#ifndef NO_TV_STREAMS
const char *const THelpIndex::name = "THelpIndex";

void THelpIndex::write(opstream &os) {
   long *indexArrayPtr;

   os << size;
   for (int i = 0; i < size; ++i) {
      indexArrayPtr = index + i;
      os << *indexArrayPtr;
   }
}

void *THelpIndex::read(ipstream &is) {
   long *indexArrayPtr;

   is >> size;
   if (size == 0)
      index = 0;
   else {
      index =  new long[size];
      for (int i = 0; i < size; ++i) {
         indexArrayPtr = index + i;
         is >> *indexArrayPtr;
      }
   }
   return this;
}

TStreamable *THelpIndex::build() {
   return new THelpIndex(streamableInit);
}

TStreamableClass RHelpIndex(THelpIndex::name,
                            THelpIndex::build,
                            __DELTA(THelpIndex)
                           );

#else
//Added to file by Uri bechar
void THelpIndex::write(FILE *fp) {

   long *indexArrayPtr;

   fwrite(&size,sizeof(size),1,fp);


   for (int i = 0; i < size; ++i) {
      indexArrayPtr = index + i;
      fwrite(indexArrayPtr, sizeof(long),1,fp);
   }


}

void THelpIndex::read(FILE *fp) {

   long *indexArrayPtr;

   fread(&size,sizeof(size),1,fp);

   if (size == 0)
      index = 0;
   else {
      index =  new long[size];

      for (int i = 0; i < size; ++i) {
         indexArrayPtr = index + i;
         fread(indexArrayPtr, sizeof(long),1,fp);
      }
   }

}


#endif  // ifndef NO_TV_STREAMS

THelpIndex::~THelpIndex() {
   delete [] index;
}

THelpIndex::THelpIndex(void): TObject() {
   size = 0;
   index = 0;
}

long THelpIndex::position(int i) {
   long *indexArrayPtr;

   if (i < size) {
      indexArrayPtr = index + i;
      return (*indexArrayPtr);
   } else
      return -1;
}

void THelpIndex::add(int i, long val) {
   int delta = 10;
   long *p;
   int newSize;
   long *indexArrayPtr;

   if (i >= size) {
      newSize = (i + delta) / delta * delta;
      p = new long[newSize];
      if (p != 0) {
         memmove(p, index, size * sizeof(long));
         memset(p+size, 0xFF, (newSize - size) * sizeof(long));
      }
      if (size > 0) {
         delete [] index;
      }
      index = p;
      size = newSize;
   }
   indexArrayPtr = index + i;
   *indexArrayPtr = val;
}

// THelpFile

//--------------------------------------------------------------------------
#ifndef NO_TV_STREAMS
void THelpTopic::readParagraphs(ipstream &s) {
   ushort i, size;
   TParagraph **pp;
   ushort temp;

   s >> i;
   pp = &paragraphs;
   while (i > 0) {
      s >> size;
      *pp = new TParagraph;
      (*pp)->text = new char[size];
      (*pp)->size = (ushort) size;
      s >> temp;
      (*pp)->wrap = Boolean(temp);
      s.readBytes((*pp)->text, (*pp)->size);
      pp = &((*pp)->next);
      --i;
   }
   *pp = 0;
}

void THelpTopic::readCrossRefs(ipstream &s) {
   int i;
   TCrossRef *crossRefPtr;

   s >> numRefs;
   crossRefs = new TCrossRef[numRefs];
   for (i = 0; i < numRefs; ++i) {
      crossRefPtr = (TCrossRef *)crossRefs + i;
      s.readBytes(crossRefPtr, sizeof(TCrossRef));
   }
}

void THelpTopic::writeParagraphs(opstream &s) {
   ushort i;
   TParagraph  *p;
   ushort temp;

   p = paragraphs;
   for (i = 0; p != 0; ++i)
      p = p->next;
   s << i;
   for (p = paragraphs; p != 0; p = p->next) {
      s << p->size;
      temp = int(p->wrap);
      s << temp;
      s.writeBytes(p->text, p->size);
   }
}


void THelpTopic::writeCrossRefs(opstream &s) {
   int i;
   TCrossRef *crossRefPtr;

   s << numRefs;
   if (crossRefHandler == notAssigned) {
      for (i = 0; i < numRefs; ++i) {
         crossRefPtr = crossRefs + i;
         s << crossRefPtr->ref << crossRefPtr->offset << crossRefPtr->length;
      }
   } else
      for (i = 0; i < numRefs; ++i) {
         crossRefPtr = crossRefs + i;
         crossRefHandler(s, crossRefPtr->ref);
         s << crossRefPtr->offset << crossRefPtr->length;
      }
}

THelpFile::THelpFile(fpstream  &s) {
   long magic;
   int handle;
   long size;

   magic = 0;
   s.seekg(0);
   handle = s.rdbuf()->fd();
   size = filelength(handle);
   if (size > sizeof(magic))
      s >> magic;
   if (magic != magicHeader) {
      indexPos = 12;
      s.seekg(indexPos);
      index =  new THelpIndex;
      modified = True;
   } else {
      s.seekg(8);
      s >> indexPos;
      s.seekg(indexPos);
      s >> index;
      modified = False;
   }
   stream = &s;
}

THelpFile::~THelpFile(void) {
   long magic, size;
   int handle;

   if (modified == True) {
      stream->seekp(indexPos);
      *stream << index;
      stream->seekp(0);
      magic = magicHeader;
      handle = stream->rdbuf()->fd();
      size = filelength(handle) - 8;
      *stream << magic;
      *stream << size;
      *stream << indexPos;
   }
   delete stream;
   delete index;
}

THelpTopic *THelpFile::getTopic(int i) {
   long pos;
   THelpTopic *topic;

   pos = index->position(i);
   if (pos > 0) {
      stream->seekg(pos);
      *stream >> topic;
      return topic;
   } else return(invalidTopic());
}

void THelpFile::putTopic(THelpTopic *topic) {
   stream->seekp(indexPos);
   *stream << topic;
   indexPos = stream->tellp();
   modified = True;
}

void THelpTopic::write(opstream &os) {
   writeParagraphs(os);
   writeCrossRefs(os);

}

void *THelpTopic::read(ipstream &is) {
   readParagraphs(is);
   readCrossRefs(is);
   width = 0;
   lastLine = INT_MAX;
   return this;
}

void notAssigned(opstream & , int) {
}

//--------------------------------------------------------------------------
#else   // ifndef NO_TV_STREAMS
void THelpTopic::readParagraphs(FILE *fp) {
   ushort i, size;
   TParagraph **pp;
   ushort temp;

   fread(&i,sizeof(i),1,fp);
   pp = &paragraphs;
   while (i > 0) {
      fread(&size,sizeof(size),1,fp);
      *pp = new TParagraph;
      (*pp)->text = new char[size];
      (*pp)->size = (ushort) size;
      fread(&temp,sizeof(temp),1,fp);
      (*pp)->wrap = Boolean(temp);
      fread((*pp)->text,(*pp)->size,1,fp);
      pp = &((*pp)->next);
      --i;
   }
   *pp = 0;
}

void THelpTopic::readCrossRefs(FILE *fp) {
   int i;
   TCrossRef *crossRefPtr;

   fread(&numRefs,sizeof(numRefs),1,fp);
   crossRefs = new TCrossRef[numRefs];
   for (i = 0; i < numRefs; ++i) {
      crossRefPtr = (TCrossRef *)crossRefs + i;
      fread(crossRefPtr,sizeof(TCrossRef),1,fp);
   }
}

void THelpTopic::writeParagraphs(FILE *fp) {
   ushort i;
   TParagraph  *p;
   ushort temp;

   p = paragraphs;
   for (i = 0; p != 0; ++i)
      p = p->next;
   fwrite(&i,sizeof(i),1,fp);
   for (p = paragraphs; p != 0; p = p->next) {
      fwrite(&p->size,sizeof(p->size),1,fp);
      temp = int(p->wrap);
      fwrite(&temp,sizeof(temp),1,fp);
      fwrite(p->text,p->size,1,fp);
   }
}


void THelpTopic::writeCrossRefs(FILE *fp) {
   int i;
   TCrossRef *crossRefPtr;

   fwrite(&numRefs,sizeof(numRefs),1,fp);
   if (crossRefHandler == notAssigned) {
      for (i = 0; i < numRefs; ++i) {
         crossRefPtr = crossRefs + i;
         fwrite(&crossRefPtr->ref,   sizeof(crossRefPtr->ref),   1,fp);
         fwrite(&crossRefPtr->offset,sizeof(crossRefPtr->offset),1,fp);
         fwrite(&crossRefPtr->length,sizeof(crossRefPtr->length),1,fp);
      }
   } else
      for (i = 0; i < numRefs; ++i) {
         crossRefPtr = crossRefs + i;
         crossRefHandler(fp, crossRefPtr->ref);
         fwrite(&crossRefPtr->offset,sizeof(crossRefPtr->offset),1,fp);
         fwrite(&crossRefPtr->length,sizeof(crossRefPtr->length),1,fp);
      }
}

THelpFile::THelpFile(FILE *fp) {
   long magic;
   int handle;
   long size;

   magic = 0;
   fseek(fp,0,SEEK_SET);
   handle = fileno(fp);
   size = filelength(handle);
   if (size > sizeof(magic))
      fread(&magic,sizeof(magic),1,fp);
   if (magic != magicHeader) {
      indexPos = 12;
      fseek(fp,indexPos,SEEK_SET);
      index =  new THelpIndex;
      modified = True;
   } else {
      fseek(fp,8,SEEK_SET);
      fread(&indexPos,sizeof(indexPos),1,fp);
      fseek(fp,indexPos+13,SEEK_SET); //must add 13 for the rigth position

      index =  new THelpIndex;  //patched by Uri Bechar
      index->read(fp);
      modified = False;
   }
   hfp = fp;
}

THelpFile::~THelpFile(void) {
   long magic, size;
   int handle;

   if (modified == True) {
      fseek(hfp,indexPos,SEEK_SET);
      fwrite(&index,sizeof(index),1,hfp);
      fseek(hfp,0,SEEK_SET);
      magic = magicHeader;
      handle = fileno(hfp);
      size = filelength(handle) - 8;
      fwrite(&magic,sizeof(magic),1,hfp);
      fwrite(&size,sizeof(size),1,hfp);
      fwrite(&indexPos,sizeof(indexPos),1,hfp);
   }
   fclose(hfp);
   delete index;
}

void THelpTopic::write(FILE *fp) {
   writeParagraphs(fp);
   writeCrossRefs(fp);

}

void *THelpTopic::read(FILE *fp) {
   readParagraphs(fp);
   readCrossRefs(fp);
   width = 0;
   lastLine = INT_MAX;
   return this;
}

THelpTopic *THelpFile::getTopic(int i) {
   long pos;
   THelpTopic *topic;

   pos = index->position(i);
   if (pos > 0) {
      fseek(hfp,pos + 13,SEEK_SET); //must add 13 for the rigth position
      topic = new THelpTopic;
      topic->read(hfp);
      return topic;
   } else return(invalidTopic());
}

void THelpFile::putTopic(THelpTopic *topic) {
   fseek(hfp,indexPos,SEEK_SET);
   topic->write(hfp);
   indexPos = ftell(hfp);
   modified = True;
}

void notAssigned(FILE * , int) {
}

//--------------------------------------------------------------------------
#endif  // ifndef NO_TV_STREAMS


THelpTopic *THelpFile::invalidTopic() {
   THelpTopic *topic;
   TParagraph *para;

   topic =  new THelpTopic;
   para =  new TParagraph;
   para->text = newStr(invalidContext);
   para->size = strlen(invalidContext);
   para->wrap = False;
   para->next = 0;
   topic->addParagraph(para);
   return topic;
}

void THelpFile::recordPositionInIndex(int i) {
   index->add(i, indexPos);
   modified = True;
}


