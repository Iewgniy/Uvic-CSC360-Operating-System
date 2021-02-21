#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <ctype.h>


//Define global variables.
#define SECTOR_SIZE 512
char *memo_pointer;
int is_subdir_exist = 0;

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
    printf("%s\n", label);
}

#define timeOffset 14 //offset of creation time in directory entry
#define dateOffset 16 //offset of creation date in directory entry


void print_date_time(char * directory_entry_startPos, char file_type){
	
	int time, date;
	int hours, minutes, day, month, year;
    const char * months_str[12] = {"Jan", "Feb", "Mar", "Apr", "May", "June", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	
	time = *(unsigned short *)(directory_entry_startPos + timeOffset);
	date = *(unsigned short *)(directory_entry_startPos + dateOffset);
	
	//the year is stored as a value since 1980
	//the year is stored in the high seven bits
	year = ((date & 0xFE00) >> 9) + 1980;
	//the month is stored in the middle four bits
	month = (date & 0x1E0) >> 5;
	//the day is stored in the low five bits
	day = (date & 0x1F);
    if (file_type == 'D')
    {
        printf("%s %2d %4d ", months_str[month-1], day, year-1980);
    }else{
        printf("%s %2d %4d ", months_str[month-1], day, year);
    }
	//the hours are stored in the high five bits
	hours = (time & 0xF800) >> 11;
	//the minutes are stored in the middle 6 bits
	minutes = (time & 0x7E0) >> 5;
	
	printf("%02d:%02d\n", hours, minutes);
	
	return ;	
}

//FUNCTION: travser and list info regarding directories
int list_directory(char *file, char *dir, int layer){

  if(file[0] == 0x00){
    return 0;
  }

  //print sublayer title ONCE
  if(layer > is_subdir_exist){
    printf("/%s\n", dir);
    printf("==================\n");
    is_subdir_exist++;
  }

  //info
  char file_type; //type: F for file/D for directory
  int  file_size; //10 characters to show the file size in bytes
  char fname [15]; //file name

  //record info
  //skip if first logical cluster = 1/0
  int logical_cluster = (file[26] & 0xFF) + (file[27] << 8);
  if(logical_cluster == 1 || logical_cluster == 0){
    file = file + 32;
    return list_directory(file, dir, layer);//skip entry
  }else{
    //type
    if((file[11] & 0x10) == 0x10){ //attr subdirectory bit
      file_type = 'D';
    }else{
      file_type = 'F';
    }
    //size
    file_size = (file[28] & 0xFF) + ((file[29] & 0xFF) << 8) + ((file[30] & 0xFF) << 16) + ((file[31] & 0xFF) << 24); //little endian
    //file name (first field, 8 bytes)
    int i;
    int name_length = 0;
    int total_length = 0;
    for(i=0; i<8; i++){
      if(file[i] == ' '){
        break;
      }
      fname[i] = file[i];
      name_length++;
      total_length++;
    }
    //read extension part
    if(file_type == 'F'){
      fname[name_length] = '.';
      name_length++;
      total_length++;
      //name extension
      for(i=0; i<3; i++){
        if(file[i+8] == ' '){
          break;
        }
        fname[i+name_length] = file[i+8];
        total_length++;
      }
    }
    fname[total_length] = '\0';

    //if subdirectory
    if(file_type == 'D'){
      int sub_location = SECTOR_SIZE * (logical_cluster + 31) + 64; //move to sub sector and skip entry . and ..
      char *sub_dire = file - SECTOR_SIZE * 19 + sub_location; //record sub location
      int sub_layer = layer + 1;

      printf("%c %10d %20s ", file_type, file_size, fname);
    
      print_date_time(file,file_type);
        file = file + 32;
      //TRAVERSE CURRENTLEVEL THEN SUBLEVEL
    return list_directory(file, dir, layer), list_directory(sub_dire, fname, sub_layer);
    }else if(file_type == 'F') {  
      printf("%c %10d %20s ", file_type, file_size, fname);
    //   printf("%d",111);
      print_date_time(file,file_type);
      file = file + 32; //next entry
      return list_directory(file, dir, layer);
    }
  }
  return 0;
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

	memo_pointer = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); // p points to the starting pos of your mapped memory
	if (memo_pointer == MAP_FAILED) {
		printf("Error: failed to map memory\n");
		exit(1);
	}

    //print root level directory name (disk label)
    char* root = malloc(sizeof(char));
    read_label(root, memo_pointer);
    printf("==================\n");

    //start searching file starting from root directory, layer 0
    memo_pointer = memo_pointer + SECTOR_SIZE * 19; //move pointer to root directory
    list_directory(memo_pointer, root, 0);
    memo_pointer = memo_pointer - SECTOR_SIZE * 19; //restore memo_pointer pointer

    free(root);
    close(fd);
	munmap(memo_pointer, sb.st_size); // the modifed the memory data would be mapped to the disk image
	
	return 0;
}

