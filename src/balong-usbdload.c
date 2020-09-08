// The usbloader.bin loader via the emergency port for Balong V7R2 modems.
//
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifndef _WIN32
    #include <sys/types.h>
    #include <fcntl.h>
    #include <termios.h>
    #include <arpa/inet.h>
#else
    #include <windows.h>
    #include <setupapi.h>
    #include <getopt.h>
#endif /// of !_WIN32

#include "parts.h"
#include "patcher.h"

#ifndef _WIN32
    int siofd;
    struct termios sioparm;
#else
    static HANDLE hSerial;
#endif /// of !_WIN32

static FILE* ldr = NULL;

#define APP_VERSION_S    "2.20.3.12"

//*************************************************
//* HEX-dump memory area                          *
//*************************************************

void dump(unsigned char buffer[],int len) 
{
    int i,j;
    unsigned char ch;

    printf("\n");
    for (i=0;i<len;i+=16) 
    {
        printf("%04x: ",i);

        for (j=0;j<16;j++)
        {
            if ((i+j) < len) 
                printf("%02x ",buffer[i+j]&0xff);
            else 
                printf("   ");
        }

        printf(" *");

        for (j=0;j<16;j++) 
        {
            if ((i+j) < len) 
            {
                // byte conversion for character display
                ch=buffer[i+j];
                if ((ch < 0x20)||((ch > 0x7e)&&(ch<0xc0))) putchar('.');
                else putchar(ch);
            } 
            // padding for incomplete lines
            else 
                printf(" ");
        }

        printf("*\n");
    }
}


//*************************************************
//* Command batch checksum calculation
//*************************************************
void csum(unsigned char* buf, int len) 
{
    unsigned int i,c,csum=0;

    unsigned int cconst[] = { 0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 
                              0x50A5, 0x60C6, 0x70E7, 0x8108, 0x9129, 
                              0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 
                              0xF1EF };

    for (i=0;i<(len-2);i++) 
    {
        c=(buf[i]&0xff);
        csum=((csum<<4)&0x0000ffff)^cconst[(c>>4)^(csum>>12)];
        csum=((csum<<4)&0x0000ffff)^cconst[(c&0x0f)^(csum>>12)];
    }

    buf[len-2]=(csum>>8)&0xff;
    buf[len-1]=csum&0xff;  
}

//*************************************************
//* Sending a command package to the modem
//*************************************************
unsigned sendcmd( unsigned char* cmdbuf, size_t len ) 
{
    unsigned char replybuf[1024] = {0};
    unsigned int  replylen;

#ifndef _WIN32
    csum(cmdbuf,len);
    write(siofd,cmdbuf,len);  /// sending command
    tcdrain(siofd);
    replylen=read(siofd,replybuf,1024 );
#else
    DWORD bytes_written = 0;
    DWORD t = 0;

    csum(cmdbuf, len);
    WriteFile(hSerial, cmdbuf, len, &bytes_written, NULL);
    FlushFileBuffers(hSerial);

    t = GetTickCount();

    do 
    {
        ReadFile(hSerial, replybuf, 1024, (LPDWORD)&replylen, NULL);
    } 
    while (replylen == 0 && GetTickCount() - t < 1000);
#endif /// of _WIN32
    if (replylen == 0) 
        return 0; /// false
    
    if (replybuf[0] == 0xaa) 
        return 1; /// true

    return 0; /// false
}

//*************************************
// Opening and Configuring a Serial Port
//*************************************

unsigned open_port(char* devname) 
{
#ifndef _WIN32
    int i,dflag=1;
    char devstr[200]={0};

    // Instead of the full device name, 
    // only the t tyUSB port number is allowed

    // Checking the device name for non-numeric characters
    for(i=0;i<strlen(devname);i++) 
    {
      if ((devname[i]<'0') || (devname[i]>'9')) dflag=0;
    }

    // If the string contains only numbers, add a prefix '/dev/ttyUSB'
    if (dflag) 
        strcpy(devstr,"/dev/ttyUSB");

    // copy the device name
    strcat(devstr,devname);

    siofd = open(devstr, O_RDWR | O_NOCTTY |O_SYNC);
    if (siofd == -1) return 0;

    bzero(&sioparm, sizeof(sioparm));
    sioparm.c_cflag = B115200 | CS8 | CLOCAL | CREAD ;
    sioparm.c_iflag = 0;
    sioparm.c_oflag = 0;
    sioparm.c_lflag = 0;
    sioparm.c_cc[VTIME]=30; /// == time-out,
    sioparm.c_cc[VMIN]=0;  
    tcsetattr(siofd, TCSANOW, &sioparm);

    return 1;
#else
    char device[20] = "\\\\.\\COM";
    DCB dcbSerialParams = {0};
    COMMTIMEOUTS CommTimeouts;

    strcat(device, devname);
    
    hSerial = CreateFileA(device, 
                          GENERIC_READ | GENERIC_WRITE, 
                          0, 0, 
                          OPEN_EXISTING, 
                          FILE_ATTRIBUTE_NORMAL, 0);

    if (hSerial == INVALID_HANDLE_VALUE)
        return 0;

    ZeroMemory(&dcbSerialParams, sizeof(dcbSerialParams));
    dcbSerialParams.DCBlength=sizeof(dcbSerialParams);
    dcbSerialParams.BaudRate=CBR_115200;
    dcbSerialParams.ByteSize=8;
    dcbSerialParams.StopBits=ONESTOPBIT;
    dcbSerialParams.Parity=NOPARITY;
    dcbSerialParams.fBinary = TRUE;
    dcbSerialParams.fDtrControl = DTR_CONTROL_ENABLE;
    dcbSerialParams.fRtsControl = RTS_CONTROL_ENABLE;
    if(!SetCommState(hSerial, &dcbSerialParams))
    {
        CloseHandle(hSerial);
        return 0;
    }

    CommTimeouts.ReadIntervalTimeout = MAXDWORD;
    CommTimeouts.ReadTotalTimeoutConstant = 0;
    CommTimeouts.ReadTotalTimeoutMultiplier = 0;
    CommTimeouts.WriteTotalTimeoutConstant = 0;
    CommTimeouts.WriteTotalTimeoutMultiplier = 0;

    if (!SetCommTimeouts(hSerial, &CommTimeouts))
    {
        CloseHandle(hSerial);
        return 0;
    }

    return 1;
#endif /// of !_WIN32
}

//*************************************
//* Finding a linux kernel in a partition image
//*************************************
int locate_kernel(char* pbuf, uint32_t size) 
{
    int off;

    for(off=(size-8);off>0;off--) 
    {
      if (strncmp(pbuf+off,"ANDROID!",8) == 0) return off;
    }

    return 0;
}

#ifdef _WIN32

//DEFINE_GUID(GUID_DEVCLASS_PORTS, 0x4D36E978, 0xE325, 0x11CE, 0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18);
static GUID GUID_DEVCLASS_PORTS = { 0x4D36E978, 
                                    0xE325, 0x11CE, 
                                    0xBF, 0xC1, 0x08, 0x00, 
                                    0x2B, 0xE1, 0x03, 0x18 };

static unsigned find_port(int* port_no, char* port_name)
{
    HDEVINFO    device_info_set;
    DWORD       member_index = 0;
    SP_DEVINFO_DATA device_info_data;
    DWORD       reg_data_type;
    char        property_buffer[256] = {0};
    DWORD       required_size;
    char*       p = NULL;
    int         result = 1;
    BOOL        retB = FALSE;

    device_info_set = SetupDiGetClassDevsA( &GUID_DEVCLASS_PORTS, 
                                            NULL, 0, DIGCF_PRESENT);

    if (device_info_set == INVALID_HANDLE_VALUE)
        return result;

    while (TRUE)
    {
        ZeroMemory(&device_info_data, sizeof(SP_DEVINFO_DATA));
        device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

        retB = SetupDiEnumDeviceInfo( device_info_set, 
                                      member_index, 
                                      &device_info_data );
        if ( retB == FALSE )
            break;

        member_index++;

        retB = SetupDiGetDeviceRegistryPropertyA( device_info_set, 
                                                  &device_info_data, 
                                                  SPDRP_HARDWAREID,
                                                  &reg_data_type, 
                                                  (PBYTE)property_buffer, 
                                                  sizeof(property_buffer), 
                                                  &required_size);
        if ( retB == FALSE )
            continue;

        if ( strstr( _strupr(property_buffer), "VID_12D1&PID_1443") != NULL)
        {
            retB = SetupDiGetDeviceRegistryPropertyA( device_info_set, 
                                                      &device_info_data, 
                                                      SPDRP_FRIENDLYNAME,
                                                      &reg_data_type, 
                                                      (PBYTE)property_buffer, 
                                                      sizeof(property_buffer), 
                                                      &required_size );
            if ( retB == TRUE )
            {
                p = strstr( property_buffer, " (COM" );
                if (p != NULL)
                {
                    *port_no = atoi(p + 5);
                    strcpy(port_name, property_buffer);
                    result = 0;
                }
            }
          
            break;
        }
    }

    SetupDiDestroyDeviceInfoList(device_info_set);

    return result;
}

#endif /// of _WIN32

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

int main(int argc, char* argv[]) 
{
    unsigned int i;
    unsigned int res;
    unsigned int opt;
    unsigned int datasize;
    unsigned int pktcount;
    unsigned int adr;
    int bl;    /// current block
    unsigned char c = 0;
    int fbflag = 0;
    int tflag = 0;
    int mflag = 0;
    int bflag = 0;
    int cflag = 0;
    int koff;  /// offset to ANDROID header
    char ptfile[100] = {0};

    FILE* pt = NULL;
    char ptbuf[2048] = {0};
    uint32_t ptoff;

    struct ptable_t* ptable;

    unsigned char cmdhead[14]   = {0xfe,0, 0xff};
    unsigned char cmddata[1040] = {0xda,0,0};
    unsigned char cmdeod[5]     = {0xed,0,0,0,0};

    // list of partitions to set the file flaga
    uint8_t fileflag[41] = {0};

    struct {
      int lmode;  /// boot mode: 1 - direct start, 
                  ///            2 - through restarting A-core
      int size;   /// component size
      int adr;    /// component load address into memory
      int offset; /// offset to component from the beginning of the file
      char* pbuf; /// buffer for loading the component image
    } blk[10] = {0};

#ifndef _WIN32
    unsigned char devname[50] = "/dev/ttyUSB0";
#else
    char devname[50]="";
    DWORD bytes_written, bytes_read;
    int port_no;
    char port_name[256];
#endif /// of !_WIN32

    const char* argv0 = argv[0];
#ifndef _WIN32
    const char* divs = "/";
#else
    const char* divs = "\\";
#endif /// of !_WIN32

    // strip argv0 --
    while( 1 )
    {
        char* testargv0 = NULL;

        testargv0 = strstr( argv0, divs );
        if ( testargv0 != NULL )
        {
            argv0 = testargv0;
            argv0++;
        }
        else
            break;
    }

#ifndef _WIN32
    bzero(fileflag,sizeof(fileflag));
#else
    memset(fileflag, 0, sizeof(fileflag));
#endif /// of !_WIN32

    while ((opt = getopt(argc, argv, "hp:ft:ms:bc")) != -1) 
    {
        switch (opt) 
        {
            case 'h': 
            printf( "The utility is designed for emergency USB boot of devices on the Balong V7 chip.\n\n" );
            printf( "  usage : %s [options] <filename to upload>\n\n",
                    argv0 );
            printf( "options:\n\n" );
#ifndef _WIN32
            printf( "    -p <tty> : serial port for communication with the loader (default = /dev/ttyUSB0)\n" );
#else
            printf( "    -p (n)   : serial port number to communicate with the bootloader\n" );
            printf( "               for example, -p8 meaning COM8\n" );
            printf( "               if -p switch is not specified, port autodetection is performed\n" );
#endif /// of !_WIN32
            printf( "    -f       : load usbloader only to fastboot (without running Linux)\n" );
            printf( "    -b       : Similar to -f, but disable bad block check when erase.\n" );
            printf( "    -t <file>: take the partition table from the specified file\n" );
            printf( "    -m       : show bootloader partition table map and exit\n" );
            printf( "    -s (n)   : set a file flag for partition by n.\n" );
	    printf( "               it should be specified several times.\n" );
            printf( "    -c       : do not automatically patch erase sections\n" );
                return 0;

            case 'p':
                strcpy(devname,optarg);
                break;

            case 'f':
                fbflag=1;
                break;

            case 'c':
                cflag=1;
                break;

            case 'b':
                fbflag=1;
                bflag=1;
                break;

            case 'm':
                mflag=1;
                break;

            case 't':
                tflag=1;
                strcpy(ptfile,optarg);
                break;

            case 's':
                i=atoi(optarg);
                if (i>41) 
                {
                    printf("\n ERROR - Section #%i does not existed.\n",i);
                    return -1;
                }
                fileflag[i]=1;
                break;
             
            case '?':
            case ':':  
                return 0;
        }
    }  /// of while ((opt = .... ) 

    printf( "%s: Balong Chipset Emergency USB Writer ver %s ",
            argv0,
	    APP_VERSION_S );
#ifdef _WIN32
    printf( "[windows]\n" );
#else
    printf( "[linux]\n" );
#endif
    printf( "(c)2015 forth32, (c)2020 rageworx.info\n" );
#ifdef _WIN32
    printf( "(c)2016 rust3028, windows build\n" );
#endif

    if ( optind >= argc ) 
    {
        printf( "\n ERROR - File name not specified for upload\n" );
        return -1;
    }  

    ldr = fopen( argv[optind], "rb" );

    if (ldr == NULL ) 
    {
        printf( "\n ERROR - Cannot open: %s\n",
                argv[optind]);
        return -1;
    }

    // check signature of usbloader
    fread(&i,1,4,ldr);
    if ( i != 0x20000 ) 
    {
        printf( "\n ERROR - %s is not a file of bootloader or usbloader\n",
                argv[optind]);
        fclose( ldr );
        return -1;
    }  

    // start of block descriptors to load
    fseek( ldr, 36, SEEK_SET ); 

    // Disassemble the title
    fread(&blk[0],1,16,ldr);  /// == raminit
    fread(&blk[1],1,16,ldr);  /// == usbldr

    //---------------------------------------------------------------------
    // Reading components into memory
    for( bl=0; bl<2; bl++ )
    {
        // allocate memory for the full image of the partition
        blk[bl].pbuf=(char*)malloc(blk[bl].size);

        // read the partition image into memory
        fseek(ldr,blk[bl].offset,SEEK_SET);
        res = fread(blk[bl].pbuf,1,blk[bl].size,ldr);

        if (res != blk[bl].size) 
        {
            printf( "\n ERROR - Unexpected end of file:%i read expected %i\n",
                    res,
                    blk[bl].size );
            fclose( ldr );
            return -1;
        }

        if (bl == 0) 
            continue; // for raminit nothing else needs to be done

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%    
        // fastboot - patch
        if (fbflag) 
        {
            koff=locate_kernel(blk[bl].pbuf,blk[bl].size);
            if (koff != 0) 
            {
                // patching signature to 0x55.
                blk[bl].pbuf[koff]=0x55;
                // cut the partition to the beginning of the kernel
                blk[bl].size=koff+8;
            }
            else 
            {
                printf("\n ERROR - There is no ANDROID component in the bootloader - fastboot-overloading is not possible\n" );
                fclose( ldr );
                
                return -1;
            }
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%    
        // We are looking for the partition table in the bootloader
        ptoff=find_ptable_ram(blk[bl].pbuf,blk[bl].size);
          
        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%    
        // partition table patch
        if ( tflag ) 
        {
            pt=fopen(ptfile,"rb");

            if ( pt == NULL )
            { 
                printf( "\n ERROR - File %s not found\n", ptfile );
                printf( "         replacement of the partition table is not possible\n" );
                fclose( ldr );
                return -1;
            }

            fread(ptbuf,1,2048,pt);
            fclose(pt);

            if (memcmp(headmagic,ptbuf,sizeof(headmagic)) != 0) 
            {
                printf( "\n ERROR - File %s os mpt a partition table.\n",
                         ptfile);
                fclose( ldr );
                return -1;
            }  

            if (ptoff == 0) 
            {
                printf( "\n ERROR - Partition table not found in bootloader - no replacement possible\n" );
                fclose( ldr );
                return -1;
            }

            memcpy(blk[bl].pbuf+ptoff,ptbuf,2048);
        }

        fclose( ldr );
        ptable=(struct ptable_t*)(blk[bl].pbuf+ptoff);
          
        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%    
        // File Flags Patch
        for( i=0; i<41; i++ ) 
        {
            if (fileflag[i]) 
            {
                ptable->part[i].nproperty |= 1;
            }  
        }  

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%    
        // Outputting the partition table
        if (mflag) 
        {
            if (ptoff == 0) 
            {
                printf( "\n ERROR - Partition table not found - map output is not possible\n" );
                return -1;
            }

            show_map(*ptable);
            return -1;
        }

        // Patch erase procedures for ignoring bad blocks
        if (bflag) 
        {
            res=perasebad(blk[bl].pbuf, blk[bl].size);
            if (res == 0) 
            { 
                printf("\n ERROR - Isbad signature not found - download not possible\n" );
                return -1;
            }  
        }

        // Deleting the flash_eraseall procedure
        if (!cflag) 
        {
            res = pv7r1(blk[bl].pbuf, blk[bl].size);

            if (res == 0)
                res = pv7r2(blk[bl].pbuf, blk[bl].size);

            if (res == 0)
                res = pv7r11(blk[bl].pbuf, blk[bl].size);

            if (res == 0)
                res = pv7r22(blk[bl].pbuf, blk[bl].size);

            if (res == 0)
                res = pv7r22_2(blk[bl].pbuf, blk[bl].size);

            if (res == 0)
                res = pv7r22_3(blk[bl].pbuf, blk[bl].size);

            if (res != 0)
            {
                printf( "\n * Removed flash_eraseall procedure on offset 0x%08x", 
                        blk[bl].offset + res );
            }
            else 
            {
                printf("\n ERRPR - The eraseall routine was not found in the bootloader - use the -c switch to boot without a patch!\n");
                return -1;
            }
        }

    } /// of for( bl=0 ... )

    //---------------------------------------------------------------------

#ifdef _WIN32
    if (*devname == '\0')
    {
        printf("\n\nFinding the Emergency Boot Port ...\n");
      
        if (find_port(&port_no, port_name) == 0)
        {
            sprintf(devname, "%d", port_no);
            printf("--> Found Port: \"%s\"\n", port_name);
        }
        else
        {
            printf("--> Port not found!\n");
            return -1;
        }
    }
#endif

    if (!open_port(devname))
    {
        printf("\n ERROR - Serial port won't open\n");
        return -1;
    }  

    // Check port is accessable.
    c=0;
#ifndef _WIN32
    write(siofd,"A",1);
    res=read(siofd,&c,1);
#else
    WriteFile(hSerial, "A", 1, &bytes_written, NULL);
    FlushFileBuffers(hSerial);
    Sleep(100);
    ReadFile(hSerial, &c, 1, &bytes_read, NULL);
#endif /// of _WIN32
    if (c != 0x55) 
    {
        printf("\n ERROR - Port is not in USB Boot mode!\n");
        return -1;
    }  

    //----------------------------------
    // main load loop - load all blocks found in the header
    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%    

    fflush( stdout );
    printf( "\n\n"
            "Component    Address  Size     %%downloading\n"
            "-------------------------------------------\n" );

    for(bl=0;bl<2;bl++) 
    {
        datasize=1024;
        pktcount=1;

        // framing the package of the beginning of the block
        *((unsigned int*)&cmdhead[4])=htonl(blk[bl].size);
        *((unsigned int*)&cmdhead[8])=htonl(blk[bl].adr);
        cmdhead[3]=blk[bl].lmode;
          
        // we send the packet of the beginning of the block
        res=sendcmd(cmdhead,14);
        if (!res) 
        {
            printf("\n ERROR - Modem rejected header packet.\n");
            return -1;
        }  
          
        // ---------- Block data load cycle ---------------------
        for(adr=0;adr<blk[bl].size;adr+=1024) 
        {
            // we form the size of the last loaded package
            if ((adr+1024)>=blk[bl].size)
            {
                datasize=blk[bl].size-adr;
            }

            printf( "\r %s    %08x %8i   %i%% ",
                    bl?"usbboot":"raminit",
                    blk[bl].adr,
                    blk[bl].size,
                    (adr+datasize)*100/blk[bl].size ); 
            fflush( stdout );
            // preparing a data package
            cmddata[1]=pktcount;
            cmddata[2]=(~pktcount)&0xff;
            memcpy(cmddata+3,blk[bl].pbuf+adr,datasize);
            
            pktcount++;

            if (!sendcmd(cmddata,datasize+5)) 
            {
                printf( "\n ERROR - Modem rejected data packet.\n" );
                return -1;
            }

	    // little wait for safety transfer.
#ifndef _WIN32
	    usleep( 25000 );
#else
	    Sleep( 25 );
#endif /// of _WIN32
	}

        free(blk[bl].pbuf);

        // Framing the end of data packet
        cmdeod[1]=pktcount;
        cmdeod[2]=(~pktcount)&0xff;

        if (!sendcmd(cmdeod,5)) 
        {
            printf("\n WARNING - Modem rejected end of data packet\n" );
        }
    } 

    printf( "\n\nDownload finished.\n");  

    return 0;
}
