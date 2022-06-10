#include <stdlib.h>
#include<string.h>
#include <stdio.h>
#include<stdarg.h>
#include<emscripten.h>

#include"junzip.h"


// the javascript callback
EM_IMPORT(onUnzippedFile) extern void onUnzippedFile(char*data,int length,char* filename,int fileNameLength);
EM_IMPORT(console_log) extern void console_log(char* str);


int unzipBufferSize=1048576;
char *unzipBuffer=NULL;

char *logBuf=NULL;

void logFormat(char*fmt,...)
{
    if(logBuf==NULL)
    {
        logBuf=malloc(256);
    }
    va_list myargs;

    /* Initialise the va_list variable with the ... after fmt */

    va_start(myargs, fmt);

    /* Forward the '...' to vprintf */
    vsnprintf(logBuf,256,fmt, myargs);
    console_log(logBuf);

    /* Clean up the va_list */
    va_end(myargs);
}
// buffer for any bytes left over from the previous call
char *spareBuffer=NULL;
int spareBufferSize=0;
char *currentBuffer=NULL;
size_t currentBufferLeft=0;
int currentBufferPos=0;
int weOwnCurrentBuffer=0;

static char* getReadyBytes(size_t length)
{
    char* retVal;
    if(currentBufferLeft>=length)
    {
        // straight from the current buffer
        retVal=&currentBuffer[currentBufferPos];
        currentBufferLeft-=length;
        currentBufferPos+=length;
        return retVal;
    }
    return NULL;
}

void loadBuffer(char*buffer,int length)
{
    if(spareBufferSize!=0)
    {
        currentBuffer=realloc(spareBuffer,spareBufferSize+length);
        spareBuffer=NULL;
        weOwnCurrentBuffer=1;
    }else
    {
        currentBuffer=buffer;
        currentBufferLeft=length;
        currentBufferPos=0;
        weOwnCurrentBuffer=0;
    }
}

void unloadBuffer()
{
    if(currentBufferLeft>0)
    {
        // copy it to a spare buffer
        spareBuffer=malloc(currentBufferLeft);
        spareBufferSize=currentBufferLeft;
    }
    if(weOwnCurrentBuffer)
    {
        free(currentBuffer);
        currentBuffer=NULL;
    }
}





static JZLocalFileHeader currentHeader;

typedef enum 
{
    INIT,
    WAIT_HEADER,
    WAIT_FILENAME,
    WAIT_FILE
} ReadState;

ReadState state=INIT;
int bytesNeeded=0;

JZLocalFileHeader curHeader;
char currentFname[1024];
int currentFnameLen=0;

int addUnzipData(char *data,int size)
{
    if(!unzipBuffer)
    {
        unzipBuffer=malloc(unzipBufferSize);
    }
    if(data==NULL)
    {
        return 104;
    }

    int fnameOffset=0;
    int result=0;

    loadBuffer(data,size);
    while(1)
    {
        
        char*theBytes=NULL;
        if(bytesNeeded!=0)
        {
            theBytes=getReadyBytes(bytesNeeded);
            if(!theBytes)
            {
                break;
            }
        }
        switch(state)
        {
            case INIT:
                state=WAIT_HEADER;
                bytesNeeded=sizeof(JZLocalFileHeader);
                break;
            case WAIT_HEADER:
                memcpy(&currentHeader,theBytes,sizeof(JZLocalFileHeader));
                if(jzCheckFileHeader(&currentHeader)==Z_ERRNO)
                {
                    return data[0];
                }
                bytesNeeded=currentHeader.fileNameLength+currentHeader.extraFieldLength;
                state=WAIT_FILENAME;
                break;
            case WAIT_FILENAME:
                fnameOffset=currentHeader.extraFieldLength;
                if(currentHeader.fileNameLength>1023)
                {
                    memcpy(currentFname,&theBytes[fnameOffset],1023);
                    currentFname[1023]=0;
                    currentFnameLen=1024;
                }else
                {
                    memcpy(currentFname,&theBytes[fnameOffset],currentHeader.fileNameLength);
                    currentFname[currentHeader.fileNameLength]=0;
                    currentFnameLen=currentHeader.fileNameLength;
                }
                state=WAIT_FILE;
                bytesNeeded=currentHeader.compressedSize;
                break;
            case WAIT_FILE:
                // got a filename and some bytes, chuck them into the js export
                // first make sure the buffer is big enough
                if(unzipBufferSize<currentHeader.uncompressedSize)
                {
                    unzipBufferSize=currentHeader.uncompressedSize;
                    unzipBuffer=realloc(unzipBuffer,unzipBufferSize);
                }
                // read zip data                
                result=jzReadDataBuffer(theBytes,&currentHeader,unzipBuffer);
                
                onUnzippedFile(unzipBuffer,currentHeader.uncompressedSize,currentFname,currentFnameLen);
                state=WAIT_HEADER;
                bytesNeeded=sizeof(JZLocalFileHeader);
                break;
        }
    }
    unloadBuffer();
    return 0;
}