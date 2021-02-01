//Name: Laith Marzouq
//ID:   1001586886
//Compiled using command: gcc -Wall mfs.c -o mfs -std=c99


// The MIT License (MIT)
//
// Copyright (c) 2020 Trevor Bakker
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_NUM_ARGUMENTS 5

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

struct __attribute__((__packed__)) DirectoryEntry
{
  char      DIR_Name[11];
  uint8_t   DIR_Attr;
  uint8_t   Unused1[8];
  uint16_t  DIR_FirstClusterHigh;
  uint8_t   Unused2[4];
  uint16_t  DIR_FirstClusterLow;
  uint32_t  DIR_FileSize;
};
struct DirectoryEntry dir[16];

char      BS_OEMName[8];
int16_t   BPB_BytsPerSec;
int8_t    BPB_SecPerClus;
int16_t   BPB_RsvdSecCnt;
int8_t    BPB_NumFATs;
int32_t   BPB_FATSz32;
int16_t   BPB_RootEntCnt;
int32_t   BPB_RootClus;
char      BS_VolLab[11];

int32_t   RootDirAddress=0;

int32_t   CurrDir=0;
int32_t   RootDirSectors = 0;
int32_t   FirstDataSector = 0;
int32_t   FirstSectorofCluster = 0;

FILE * fp=NULL;
//funtion that checks if a string has '.' in it. Used to make sure cd isnt used on a .txt file
bool DotPresent(char input[], int size)
{
  for(int i=0; i<size; i++)
  {
    if(input[i]=='.')
    {
      return 1;
    }
  }
  return 0;
}
bool compare(char *IMG_Name, char *oldinput)
{
  char input[12];
  memset( input, 0, 12 );
  strncpy( input, oldinput, strlen( oldinput ) );

  char expanded_name[12];
  memset( expanded_name, ' ', 12 );

  char *token = strtok( input, "." );

  strncpy( expanded_name, token, strlen( token ) );

  token = strtok( NULL, "." );

  if( token )
  {
    strncpy( (char*)(expanded_name+8), token, strlen(token ) );
  }

  expanded_name[11] = '\0';

  int i;
  for( i = 0; i < 11; i++ )
  {
    expanded_name[i] = toupper( expanded_name[i] );
  }

  if( strncmp( expanded_name, IMG_Name, 11 ) == 0 )
  {
    return 1;
  }
  return 0;
}

int16_t NextLB(uint32_t sector)
{
  uint32_t FATAdress = (BPB_BytsPerSec * BPB_RsvdSecCnt) + (sector * 4);
  int16_t val;
  fseek(fp, FATAdress, SEEK_SET);
  fread( &val, 2, 1, fp);
  return val;
}
int LBAToOffset(int32_t sector)
{
  return ((sector - 2) * BPB_BytsPerSec) + (BPB_BytsPerSec * BPB_RsvdSecCnt) + (BPB_NumFATs * BPB_FATSz32 * BPB_BytsPerSec);
}

int main()
{

  char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );



  while( 1 )
  {
    // Print out the mfs prompt
    printf ("mfs> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );

    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    int   token_count = 0;

    // Pointer to point to the token
    // parsed by strsep
    char *arg_ptr;

    char *working_str  = strdup( cmd_str );

    // we are going to move the working_str pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *working_root = working_str;

    // Tokenize the input stringswith whitespace used as the delimiter
    while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) &&
              (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }
        token_count++;
    }

    // Now print the tokenized input as a debug check
    // \TODO Remove this code and replace with your FAT32 functionality
/*
    int token_index  = 0;
    for( token_index = 0; token_index < token_count; token_index ++ )
    {
      printf("token[%d] = %s\n", token_index, token[token_index] );
    }
*/
    if(strcmp(token[0], "open")==0)
    {
      if(fp!=NULL)
      {
        printf("Error: File system image already open.\n");
      }
      else
      {
        fp = fopen(token[1], "r");
        if(fp==NULL)
        {
          printf("Error: File system image not found.\n");
          continue;
        }
        fseek(fp, 11, SEEK_SET);
        fread(&BPB_BytsPerSec, 2, 1, fp);

        fseek(fp, 13, SEEK_SET);
        fread(&BPB_SecPerClus, 1, 1, fp);

        fseek(fp, 14, SEEK_SET);
        fread(&BPB_RsvdSecCnt, 2, 1, fp);

        fseek(fp, 16, SEEK_SET);
        fread(&BPB_NumFATs, 1, 1, fp);

        fseek(fp, 36, SEEK_SET);
        fread(&BPB_FATSz32, 4, 1, fp);

        fseek(fp, 17, SEEK_SET);
        fread(&BPB_RootEntCnt, 2, 1, fp);

        fseek(fp, 44, SEEK_SET);
        fread(&BPB_RootClus, 4, 1, fp);

        RootDirAddress= (BPB_NumFATs*BPB_FATSz32*BPB_BytsPerSec)+(BPB_RsvdSecCnt*BPB_BytsPerSec);
        CurrDir=RootDirAddress;
        fseek(fp, RootDirAddress, SEEK_SET);
        fread(dir, 16, sizeof(struct DirectoryEntry), fp);

      }
    }

    else if(strcmp(token[0], "close")==0)
    {
      if(fp==NULL)
      {
        printf("Error: File system not open.\n");
        continue;
      }
      printf("Closing File\n");
      fclose(fp);
      fp=NULL;
    }

    else if(strcmp(token[0], "bpb")==0)
    {
      if(fp==NULL)
      {
        printf("Error: File system image must be opened first.\n");
        continue;
      }
      printf("Printing Info   HEX          Decimal\n");
      printf("BPB_BytsPerSec: 0x%X        %d\n", BPB_BytsPerSec, BPB_BytsPerSec);
      printf("BPB_SecPerClus: 0x%X          %d\n", BPB_SecPerClus, BPB_SecPerClus);
      printf("BPB_RsvdSecCnt: 0x%X         %d\n", BPB_RsvdSecCnt, BPB_RsvdSecCnt);
      printf("BPB_NumFATs:    0x%X          %d\n", BPB_NumFATs, BPB_NumFATs);
      printf("BPB_FATSz32:    0x%X        %d\n", BPB_FATSz32, BPB_FATSz32);
    }
    //fix null
    else if(strcmp(token[0], "stat")==0)
    {
      if(fp==NULL)
      {
        printf("Error: File system image must be opened first.\n");
        continue;
      }
      if(token[1] == NULL)
      {
        printf("No file or directory name was given.\n");
        continue;
      }
      bool FileExists=false;
      for(int i=0; i<16; i++)
      {
         if( compare(dir[i].DIR_Name, token[1]))
         {
           char Null_name[12];
           strncpy(Null_name, dir[i].DIR_Name,12);
           Null_name[11]='\0';
           printf("\nFileName              Attributes       Cluster Number        Size\n");
           printf("%s           %d               %d                    %d\n",
                  Null_name, dir[i].DIR_Attr, dir[i].DIR_FirstClusterLow, dir[i].DIR_FileSize);
           FileExists=true;
           break;
         }
      }
      if(!FileExists)
      {
        printf("Error: File not found\n");
      }
    }
    else if(strcmp(token[0], "cd")==0)
    {
      if(fp==NULL)
      {
        printf("Error: File system image must be opened first.\n");
        continue;
      }
      if(token[1] == NULL)
      {
        printf("No directory name was given.\n");
        continue;
      }

      fseek(fp, CurrDir, SEEK_SET);
      fread(dir, 16, sizeof(struct DirectoryEntry), fp);
      int i;
      char dotdot[2] = "..";
      bool FileExists=false;
      if( strncmp( token[1], dotdot, 2 ) == 0 )
      {
        for(i=0; i<16; i++)
        {
         if(strncmp( token[1], dir[i].DIR_Name, 2 ) == 0)
         {
           FileExists=true;
           int cluster = dir[i].DIR_FirstClusterLow;
           if( cluster == 0 ) cluster = 2;
           CurrDir = LBAToOffset(cluster);
           fseek( fp, CurrDir, SEEK_SET );
           fread( dir, 16, sizeof( struct DirectoryEntry), fp);
           break;
          }
        }
      }
      else
      {
       for(i=0; i<16; i++)
        {
          //DotPresent
         if( compare(dir[i].DIR_Name, token[1]) && !(DotPresent(token[1], strlen(token[1]))))
         {
           FileExists=true;
           int cluster = dir[i].DIR_FirstClusterLow;
           if( cluster == 0 ) cluster = 2;
           CurrDir = LBAToOffset(cluster);
           fseek( fp, CurrDir, SEEK_SET );
           fread( dir, 16, sizeof( struct DirectoryEntry), fp);
           break;
          }
        }
        if(!FileExists)
        {
          printf("Error: Directory not found\n");
        }
      }

    }
    else if(strcmp(token[0], "ls")==0)
    {
      if(fp==NULL)
      {
        printf("Error: File system image must be opened first.\n");
        continue;
      }

      int i;
      char dotdot[2] = "..";
      if(token[1] != NULL && strncmp( token[1], dotdot, 2 ) == 0 )
      {
        for(i=0; i<16; i++)
        {
         if(strncmp( token[1], dir[i].DIR_Name, 2 ) == 0)
         {
           int cluster = dir[i].DIR_FirstClusterLow;
           if( cluster == 0 ) cluster = 2;
           fseek( fp, LBAToOffset(cluster), SEEK_SET );
           fread( dir, 16, sizeof( struct DirectoryEntry), fp);
           for(i=0; i<16; i++)
           {
              if( (dir[i].DIR_Attr==0x01 || dir[i].DIR_Attr==0x10 || dir[i].DIR_Attr==0x20)
                    && dir[i].DIR_Name[0] != '.' && dir[i].DIR_Name[0] != 0xFFFFFFE5)
              {
                char Null_name[12];
                strncpy(Null_name, dir[i].DIR_Name,12);
                Null_name[11]='\0';
                printf("FileName: %s\n", Null_name);
              }
           }
           fseek( fp, CurrDir, SEEK_SET );
           fread( dir, 16, sizeof( struct DirectoryEntry), fp);
           break;
          }
        }
      }
      else
      {
        for(i=0; i<16; i++)
        {
          if( (dir[i].DIR_Attr==0x01 || dir[i].DIR_Attr==0x10 || dir[i].DIR_Attr==0x20)
                && dir[i].DIR_Name[0] != '.' && dir[i].DIR_Name[0] != 0xFFFFFFE5)
          {
            char Null_name[12];
            strncpy(Null_name, dir[i].DIR_Name,12);
            Null_name[11]='\0';
            printf("FileName: %s\n", Null_name);
          }
        }
      }
    }

    else if(strcmp(token[0], "get")==0)
    {
      if(fp==NULL)
      {
        printf("Error: File system image must be opened first.\n");
        continue;
      }
      if(token[1] == NULL)
      {
        printf("No file or directory name was given.\n");
        continue;
      }
      int i;
      bool FileExists=false;
      for(i=0; i<16; i++)
      {
        if( compare(dir[i].DIR_Name, token[1]))
        {
          FileExists=true;
          int cluster = dir[i].DIR_FirstClusterLow;
          //if( cluster == 0 ) cluster = 2;
          int size = dir[i].DIR_FileSize;
          FILE *ofp;
          if(token[2]==NULL)
          {
            ofp = fopen( token[1], "w");
          }
          else
          {
            ofp = fopen( token[2], "w");
          }
          while( size>=512 )
          {
            uint8_t buffer[512];
            int offset = LBAToOffset( cluster );
            fseek(fp, offset, SEEK_SET);
            fread( buffer, 1, 512, fp );
            fwrite( buffer, 1, 512, ofp );
            size = size - 512;
            cluster = NextLB( cluster );
          }
          if( size )
          {
            if(cluster != -1)
            {
              uint8_t buffer[512];
              int offset = LBAToOffset( cluster);
              fseek(fp, offset, SEEK_SET);
              fread( buffer, 1, size, fp );
              fwrite( buffer, 1, size, ofp );
            }
          }
          fclose( ofp );

        }
      }
      if(!FileExists)
      {
        printf("Error: File not found\n");
      }

    }

    else if(strcmp(token[0], "read")==0)
    {
      if(fp==NULL)
      {
        printf("Error: File system image must be opened first.\n");
        continue;
      }
      if(token[1] == NULL || token[2] == NULL || token[3] == NULL)
      {
        printf("Invalid Parameters\n");
        continue;
      }
      char *tempFile= (char*) malloc(255);
      bool FileExists=false;
      int i;
      for(i=0; i<16; i++)
      {
        if(compare(dir[i].DIR_Name, token[1]))
        {
          int cluster = dir[i].DIR_FirstClusterLow;
          if( cluster == 0 ) cluster = 2;
          fseek( fp, LBAToOffset(cluster) + atoi(token[2]) , SEEK_SET );
          fread( tempFile, atoi(token[3]), 1, fp);
          FileExists=true;
          for(i=0; i< atoi(token[3]); i++)
          {
            printf("%X ", tempFile[i]);
          }
          printf("\n");
          break;
        }
      }
      if(!FileExists)
      {
        printf("Error: File not found\n");
      }


    }

    else if(strcmp(token[0], "exit")==0 || strcmp(token[0], "quit")==0)
    {
      if(fp!=NULL)
      {
        fclose(fp);
      }
      free( working_root );
      exit(0);
    }

    else
    {
      printf("Error: Invalid Command.\n");
    }
    free( working_root );

  }
  return 0;
}
