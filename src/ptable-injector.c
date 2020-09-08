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
  
//############################################################################################################3

int main(int argc, char* argv[]) 
{
    int     opt;  
    int     mflag = 0;
    char    ptfile[100] = {0};
    int     rflag = 0;
    int     xflag = 0;

    uint32_t ptaddr = 0;
    struct ptable_t ptable;

    FILE* ldr = NULL;
    FILE* out = NULL;
    FILE* in  = NULL;

    while ((opt = getopt(argc, argv, "mr:hx")) != -1) 
    {
        switch (opt) 
        {
            case 'h': 
         
                printf( "\n Utility for replacing the partition table in bootloaders usbloader\n\n" );
                printf( "    %s [option] <usbloader file name>\n\n", 
                        argv[0] );
                printf( "options :\n\n" );
                printf( "  -m  : show current partition map in usbloader\n" );
                printf( "  -x  : extract current map to ptable.bin file\n" );
                printf( "  -r <file> : replace the partition map with the map from the given file\n" );
                return 0;
            
           case 'm':
                mflag=1;
                break;
            
           case 'x':
                xflag=1;
                break;
            
           case 'r':
                rflag=1;
                strcpy (ptfile,optarg);
                break;
             
           case '?':
           case ':':  
                return 0;
        }  
    }  /// of while( opt ... )

    if (optind>=argc) 
    {
        printf("\n ERROR - Loader file name not specified.\n");
        return -1;
    }  

    ldr=fopen(argv[optind],"r+b");

    if (ldr == 0) 
    {
        printf("\n ERROR - File %s not open.\n", argv[optind]);
        return -1;
    }
     
    // Looking for the partition table in the bootloader file

    ptaddr=find_ptable(ldr);

    if (ptaddr == 0) 
    {
        printf("\n ERROR - Partition table in bootloader not found.\n");
        fclose( ldr );
        return -1;
    }

    // read the current table
    fread(&ptable,sizeof(ptable),1,ldr);

    if (xflag) 
    {
        out=fopen("ptable.bin","wb");
        fwrite(&ptable,sizeof(ptable),1,out);
        fclose(out);
    }   

    if (mflag) 
    {
        show_map(ptable);
    }

    if (mflag | xflag) 
    {
        fclose( ldr );
        return -1;
    }
      
    if (rflag) 
    { 
        in=fopen(ptfile,"rb");

        if (in == 0) 
        {
            printf("\n ERROR - File %s cannot writen.\n",ptfile);
            fclose( ldr );
            return -1;
        }

        fread(&ptable,sizeof(ptable),1,in);
        fclose(in);
          
        if (memcmp(ptable.head,headmagic,16) != 0) 
        {
            printf("\n ERROR - Input file is not a partition table.\n");
            fclose( ldr );
            return -1;
        }

        fseek(ldr,ptaddr,SEEK_SET);
        fwrite(&ptable,sizeof(ptable),1,ldr);
        fclose(ldr);
    }  

    return 0;
}
