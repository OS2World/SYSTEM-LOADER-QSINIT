/* 
  another 1001st \r\n -> \n converter
*/

#include "classes.hpp"

int main(int argc, char *argv[]) {
   if (argc<2 || argc>3) {
      printf("dos2unix <source_txt> [<destin_txt>]\n");
      return 1;
   }
   TStrings src;
   if (!src.LoadFromFile(argv[1])) {
      printf("unable to load \"%s\"\n", argv[1]);
      return 2;
   }
   if (!src.SaveToFile(argv[argc-1],true)) {
      printf("unable to save \"%s\"\n", argv[argc-1]);
      return 3;
   }
   return 0;
}
