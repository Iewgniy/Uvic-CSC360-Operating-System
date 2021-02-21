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
int name_length = 0;
int ex_length = 0;
char * memo_pointer;
int found_filesize = 0;

//FFAT_Packing
int FAT_packing(int entry, char* disk_map){
  //FAT start from 0x200 = 512 after first sector
  disk_map += SECTOR_SIZE;
  int next_sector;
  int low_high; //lower four for even; higher four for odd
  int eight;    //all eight bits
  int fun = (int)(3 * entry/2);

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

//search file if exist then return, if not return error.
int search_for_file(char *file, char *copyfile_name, char *copyfile_ex){
  //file not found
  if(file[0] == 0x00){
    return 0;
  }

  //record current entry file name
  int curname_length = 0;
  char curr_fname[10];
  int i;
  for(i=0; i<8; i++){
    if(file[i] == ' '){
      curr_fname[i] = '\0';
      break;
    }
    curr_fname[i] = file[i];
    curname_length++;
  }
  char curr_ex[3];
  int curex_length = 0;
  for(i=0; i<3; i++){
    if(file[i+8] == ' '){
      curr_ex[i] = '\0';
      break;
    }
    curr_ex[i] = file[i+8];
    curex_length++;
  }

  //first logical sector
  int cluster_number = (file[26] & 0xFF) + (file[27] << 8);

  //if file found, return
  if(strncmp(curr_fname, copyfile_name, strlen(copyfile_name)) == 0 && strncmp(curr_ex, copyfile_ex, strlen(copyfile_ex)) == 0
          && curname_length == strlen(copyfile_name) && curex_length == strlen(copyfile_ex)){
    found_filesize = (file[28] & 0xFF) + ((file[29] & 0xFF) << 8) + ((file[30] & 0xFF) << 16) + ((file[31] & 0xFF) << 24);
    return cluster_number;
  }else{
    file = file + 32;
    return search_for_file(file, copyfile_name, copyfile_ex);
  }
}

//Copy file to new file.
void copy_to_newfile(char * disk_map, char * new_map, int first_sector){
  int n_sector = first_sector;
  int rem_size = found_filesize;
  int p_addr;
  int i;

  do{
    //first cluster
    if(rem_size == found_filesize){
      p_addr = (31 + n_sector) * SECTOR_SIZE;
    }else{
      n_sector = FAT_packing(n_sector, disk_map);
      p_addr = (31 + n_sector) * SECTOR_SIZE;
    }
    //copy file (sector by sector)
    for(i=0; i<SECTOR_SIZE; i++){
      if(rem_size == 0) return;
      new_map[found_filesize - rem_size] = disk_map[i + p_addr];
      rem_size--;
    }
  }while(FAT_packing(n_sector, disk_map) != 0xFFF); //last cluster
}

int main(int argc, char *argv[]){
  //catch wrong input format
  if(argc < 3){
    printf("ERROR: Invalid arugument. Enter: diskget <file system image> <file to be copyed>.\n");
    exit(1);
  }

  //open read file system
  int file_system = open(argv[1], O_RDWR);
  if(file_system < 0){
    printf("ERROR: Failed to open file system image.\n");
    exit(1);
  }

  //map to memory
  struct stat sb;
  fstat(file_system, &sb);
  memo_pointer = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, file_system, 0);



  //convert file name to upper case
  char * copy_file = argv[2];
  char copy_filename [10];
  char copy_fileex[4];
  int name_or_ex = 0; 
  int i;
  for(i=0; copy_file[i] != '\0'; i++){
    if(copy_file[i] == '.'){
      copy_filename[name_length] = '\0';
      name_or_ex = 1;
      i++;
    }
    if(name_or_ex == 0){
      copy_filename[name_length] = toupper(copy_file[i]); //convert to upper
      name_length++;
    }else{
      copy_fileex[ex_length] = toupper(copy_file[i]);
      ex_length++;
    }
  }
  copy_fileex[ex_length] = '\0';

  //search filename: if match get first cluster number and size else file not found
  memo_pointer = memo_pointer + SECTOR_SIZE * 19; //move to root directory
  int cluster_number = search_for_file(memo_pointer, copy_filename, copy_fileex);
  memo_pointer = memo_pointer - SECTOR_SIZE * 19; //restore mapping pointer

  //if failed to find file in disk
  if(cluster_number == 0){
     printf("File not found.\n");
     close(file_system);
     munmap(memo_pointer, sb.st_size);
     exit(0);
  }

  //create new file in current directory to be wriiten
  int fd = open(copy_file, O_RDWR | O_CREAT, 0644);

  //if create fail, error
  if(fd < 0){
    printf("ERROR: failed to open new file\n");
    close(fd);
    close(file_system);
    munmap(memo_pointer, sb.st_size);
    exit(1);
  }
  //seek file (according found file size) move to the end of file
  int seek_return = lseek(fd,found_filesize-1, SEEK_SET);
  //if seek failed
  if(seek_return == -1){
    printf("ERROR: failed to seek new file.\n");
    close(fd);
    close(file_system);
    munmap(memo_pointer, sb.st_size);
    exit(1);
  }
  seek_return = write(fd, "", 1); //write to mark the end of file
  //if write filed
  if(seek_return != 1){
    printf("ERROR: failed to write to new file.\n");
    close(fd);
    close(file_system);
    munmap(memo_pointer, sb.st_size);
    exit(1);
  }

  char * new_map = mmap(NULL, found_filesize, PROT_WRITE, MAP_SHARED, fd, 0);

  //start copying file contain to new file
  copy_to_newfile(memo_pointer, new_map, cluster_number);

  //free memory/ finish
  close(fd);
  munmap(new_map, found_filesize);
  close(file_system);
  munmap(memo_pointer, sb.st_size);
  return 0;
}