#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <ctype.h>


//Define global variable.
#define SECTOR_SIZE 512
int SEC_COUNT;
char * memo_pointer;


//read OS name frin boot sector,OS name: starting byte 3, length 8 bytes in boot sector.
void read_OSname(char *OSname, char *file){
    int i;
    for(i=0; i<8; i++){
        OSname[i] = file[3+i];
    }
    printf("OS Name: %s\n", OSname);
}

//read disk label from root directory.
void read_label(char *label, char *file){
    int i;
    file = file + SECTOR_SIZE *19;
    while(file[0] != 0x00){
        //offset 11: attributes -> mask: 0x08 Volume label
        if(file[11] == 8){
        //read label
        for(i=0; i<8; i++){
            label[i] = file[i];
        }
    }
    //read next entry
    file = file + 32;
    }
    printf("Label of the disk: %s\n", label);
}

//read disk size
int read_disksize(char *file){
  //byte 19: total sector count (2 bytes)
  SEC_COUNT = file[19] + (file[20] << 8); //little endian
  int total_size = SEC_COUNT * SECTOR_SIZE;
  printf("Total size of the disk: %d bytes\n", total_size);
  return total_size;
}

//FAT_packing
int FAT_packing(int entry, char* disk_map){
  disk_map += SECTOR_SIZE;
  int next_sector;
  int low_high; //lower four for even; higher four for odd
  int eight;    //all eight bits
  int fun = (int)(3 * entry/2);

  //FAT start from 0x200 = 512 after first sector
  //if entry is even
  if(entry % 2 == 0){
    low_high = disk_map[1 + fun] & 0x0F; //low bit
    eight = disk_map[fun] & 0xFF;
    next_sector = (low_high << 8) + eight;
  //if entry is odd
  }else{
    low_high = disk_map[fun] & 0xF0; //high bit
    eight = disk_map[1 + fun] & 0xFF;
    next_sector = (low_high >> 4) + (eight << 4);
  }
  return next_sector;
}

//FUNCTION: read free size of the disk
int read_freesize(char *file){
  int empty_sector = 0;
  int i;
  //first 2 entry are reserved, ignore useless entries after 2848
  for(i=2; i<2849; i++){
    //if unused
    if(FAT_packing(i, file) == 0x00){
      empty_sector ++;
    }
  }
  int free = empty_sector * SECTOR_SIZE;
  printf("Free size of the disk: %d bytes\n\n", free);
  return free;
}

//FUNCTION: count the number of file including root and sub
int count_file(char *file){
  int count = 0;

  if(file[0] == 0x00){
    return count;
  }
  /*
    NOT ELIGIBAL FILE:
    skip entry if
    attr = 0x0f, 4th-bit(subdirectory) attr = 1, volumn label bit = 1,
    free = first byte of filename field is 0xE5
    first logical cluster is 1 or 0
  */
  int first_logical = file[26] + (file[27] << 8); //first logical cluster
  //if not eligibal
  if(first_logical == 1 || first_logical == 0 || file[11] == 0x0f || (file[11] & 0x08) == 0x08 || (file[0] & 0xFF) == 0xE5){
    file = file + 32; //skip
    return count + count_file(file);
  //is a subdirectory
  }else if((file[11] & 0x10) == 0x10){
    //find sub location
    int sub_location = SECTOR_SIZE * (first_logical + 31) + 64; //move to sub sector and skip entry . and ..
    char *sub_dire = memo_pointer - SECTOR_SIZE * 19 + sub_location; //record sub location
    file = file + 32;
    //recursivly travser sublayer
    return count + count_file(sub_dire) + count_file(file);
  //is a file
  }else{
    count++;
    file = file + 32;
    return count + count_file(file);
  }
}

//read the number of fat copies from boot sector
int read_numfat(char *file){
  printf("Number of FAT copies: %d\n", file[16]);
  return file[16];
}

//read sector per FAT from boot sector
int read_secfat(char *file){
 int sec_per_fat = file[22] + (file[23] << 8); //little endian
 printf("Sectors per FAT: %d\n", sec_per_fat);
 return sec_per_fat;
}

int main(int argc, char *argv[])
{
    //catch wrong input format
    if(argc < 2){
        printf("ERROR: Invalid arugument. Enter: diskinfo <file system image>.\n");
        exit(1);
    }
	int fd;
	struct stat sb;

	fd = open(argv[1], O_RDWR);
	fstat(fd, &sb);
	// printf("Size: %lu\n\n", (uint64_t)sb.st_size);

	memo_pointer = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); // memo_pointer points to the starting pos of your mapped memory
	if (memo_pointer == MAP_FAILED) {
		printf("Error: failed to map memory\n");
		exit(1);
	}
	
    //read OS name
    char* OSname = malloc(sizeof(char));
    read_OSname(OSname, memo_pointer);

    //read label
    char* label = malloc(sizeof(char));
    read_label(label, memo_pointer);

    //read total disk size
    int disk_size = read_disksize(memo_pointer);
    //read free size of the disk
    int free_size = read_freesize(memo_pointer);

    //format
    printf("==============\n");
    //The number of file in the disk(including all files in the root direc and file in sub)
    //count file in root directory
    //move file pointer to root
    memo_pointer = memo_pointer + SECTOR_SIZE * 19;
    int num_file = count_file(memo_pointer);
    printf("The number of files in the disk (including all files in the root directory and files in all subdirectories): %d\n\n", num_file);
    memo_pointer = memo_pointer - SECTOR_SIZE * 19;
    printf("=============\n");

    //number of FAT copies
    int num_fat = read_numfat(memo_pointer);
    //sector per FAT
    int sec_fat = read_secfat(memo_pointer);

    //free memory/ finish
    free(OSname);
    free(label);
	munmap(memo_pointer, sb.st_size); // the modifed the memory data would be mapped to the disk image
	close(fd);
	return 0;
}