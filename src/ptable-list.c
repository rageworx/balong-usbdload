//   Программа для замены таблицы разделов в загрузчике usbloader
// 
// 
#include <stdio.h>
#include <stdint.h>

#ifndef WIN32
    #include <stdlib.h>
    #include <string.h>
    #include <unistd.h>
#else
    #include <windows.h>
    #include <getopt.h>
#endif

#include "parts.h"

  
//############################################################################################################

int main(int argc, char* argv[]) 
{
    struct ptable_t ptable;
    FILE* in = NULL;

    if (argc != 2) 
    {
        printf( "\n ERROR - File name not specified with partition table\n" );
        return -1;
    }  

    in=fopen(argv[optind],"r+b");
    if (in == 0) 
    {
        printf( "\n ERROR - File %s not open.\n", argv[optind] );
        return -1;
    }

     
    // читаем текущую таблицу
    fread(&ptable,sizeof(ptable),1,in);

    if (strncmp(ptable.head, "pTableHead", 16) != 0) 
    {
        printf( "\n ERROR - The file is not a partition table\n" );
        return -1;
    }
      
    show_map(ptable);

    return 0;
}
