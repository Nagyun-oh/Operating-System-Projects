#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Configuration Constants */
#define MAX_FILE_NUM	100		// Maximum number of files in the directory
#define NUM_BLOCKS	1024		// Total number of blocks in the data area
#define BLOCK_SIZE	32			// Size of each block in bytes
#define MAX_FILE_NAME	100		// Maximum length of a filename
#define FS_STAT	"fs_state.dat"	// Persistent storage file for FS state

/**
 * FileEntry - Metadata for a single file
 */
typedef struct{
	char filename[MAX_FILE_NAME];
	int  start_block;				// First block index in the FAT chain
	int  size;						// Current file size in bytes
}FileEntry;

/**
 * FileSystem - Entire file system structure
 */
typedef struct{
	int fat_table[NUM_BLOCKS];	// FAT: 0 = Free, 0xFFFF = EOF, Else = Next Block
	FileEntry directory[MAX_FILE_NUM];
	char data_area[NUM_BLOCKS * BLOCK_SIZE];  // Physical storage area
}FileSystem;

FileSystem myfat;	// Global instance of the file system

/* ==================================================================
 			CONTROL API
===================================================================== */
int create_file(const char *filename);		
int write_file(const char *filename, const char* data);  
int read_file(const char *filename); 			
int delete_file(const char *name);			
void list_files(void);					
int find_free_block(void);				

/* END OF API*/
void save_file_system(void);		// Save the File System to Disk
void load_file_system(void);		// Restore the File System from Disk

/**
 * find_free_block - Scans the FAT table for an unallocated block
 * Returns block index if found, or -1 if the disk is full.
 */
int find_free_block(void){

	for(int i=0; i<NUM_BLOCKS; i++){
		if(myfat.fat_table[i] == 0) // 0 == free
			return i;
	}
	return -1; // no free block
}

/**
 * save_file_system - Flushes the in-memory FS structure to a binary file
 */
void save_file_system(void){

	FILE*f = fopen(FS_STAT,"wb");
	if(f== NULL){
		printf("Error : can't save file system state.\n");
		return;
	}
	fwrite(&myfat,sizeof(FileSystem),1,f);
	fclose(f);
}

/**
 * load_file_system - Loads the FS state from disk or initializes a new one
 */
void load_file_system(void){

	FILE*f = fopen(FS_STAT,"rb");
	if(f== NULL){
		printf("Warning : No saved state found. Starting fresh.\n");
		memset(&myfat,0,sizeof(FileSystem));
		return;
	}
	fread(&myfat,sizeof(FileSystem),1,f);
	fclose(f);
}

/**
 * create_file - Registers a new file in the directory
 * 1. Checks for duplicate filenames.
 * 2. Finds an empty directory entry and an initial free block.
 */
int create_file(const char* filename){

	// Check for duplicate filename
	for(int i = 0; i< MAX_FILE_NUM; i++){
		if(strcmp(myfat.directory[i].filename,filename)==0){
			printf("File '%s' already exists.\n",filename);
			return -1;
		}
	}

	// Find empty directory entry
	for(int i = 0; i< MAX_FILE_NUM; i++){
		if(myfat.directory[i].filename[0]=='\0'){
			
			for(int j = 0; j< NUM_BLOCKS; j++){
				if(myfat.fat_table[j]==0){
					myfat.fat_table[j] = 0XFFFF;;

					strcpy(myfat.directory[i].filename,filename);
					myfat.directory[i].start_block = j;
					myfat.directory[i].size = 0;
					printf("File '%s' created.\n",filename);
					return 0;
				}
			}
		}
	}
	printf("Error: Directory or Disk full.\n");
	return -1;
}


/**
 * write_file - Appends data to an existing file
 * Handles block allocation and FAT chain linking if additional space is needed.
 */
int write_file(const char *filename,const char *data){

	for(int i = 0; i< MAX_FILE_NUM; i++){
	    if(strcmp(myfat.directory[i].filename,filename) == 0){

	    int start_block = myfat.directory[i].start_block;
            int file_size   = myfat.directory[i].size;  
            int data_len    = strlen(data);  

            if (start_block < 0 || start_block >= NUM_BLOCKS) {
                printf("File %s has invalid start block.\n", filename);
                return -1;
            }

			// Traverse to the last block of the file
            int block = start_block;
            int remain = file_size;

            while (remain >= BLOCK_SIZE && myfat.fat_table[block] != 0xFFFF) {
                remain -= BLOCK_SIZE;
                block = myfat.fat_table[block];				// 다음 블록으로 이동
            }

            int offset_in_block = file_size % BLOCK_SIZE;	// 마지막 블록 안에서 이미 사용 중인 바이트 수

            // block, offset_in_block 위치부터 data를 이어서 쓰기 시작
            int bytes_written = 0;
            int data_offset   = 0;
		
            while (bytes_written < data_len) {
                if (block < 0 || block >= NUM_BLOCKS) {
                    printf("FAT chain corrupt file %s.\n", filename);
                    return -1;
                }
				// Allocate a new block if the current one is full
                int space_in_block = BLOCK_SIZE - offset_in_block;
                if (space_in_block <= 0) {
                    // 이 블록 꽉 찼으면 다음 free 블록을 FAT에서 가져와 체인에 연결
                    int new_block = find_free_block();
                    if (new_block < 0) {
                        printf("No more space in FAT table. Partial data append to %s.\n", filename);
                        break; 
                    }
                    myfat.fat_table[block] = new_block;
                    myfat.fat_table[new_block] = 0xFFFF;
                    block = new_block;
                    offset_in_block = 0;
                    space_in_block = BLOCK_SIZE;
                }

				// 데이터 중 아직 안쓴 부분의 길이 정의
				int remaining_data = data_len - bytes_written;
				// 이번 루프에서 현재 블록에 얼마나 쓸지 결정
                int to_write = (remaining_data < space_in_block) ? remaining_data : space_in_block;

				// 나머지 데이터 쓰기
                memcpy(&myfat.data_area[block * BLOCK_SIZE + offset_in_block],
                       &data[data_offset], to_write);

                bytes_written    += to_write; // 전체에서 지금까지 쓴 양 누적 
                data_offset      += to_write; // 원본 데이터에서 다음에 쓸 위치 갱신
                offset_in_block  += to_write; // 현재 블록에서 다음에 쓸 위치 갱신
            }

			// 파일 크기 갱신 ( 기존 크기 + 새로 쓴 바이트 수 )
            myfat.directory[i].size = file_size + bytes_written;
            printf("Data written to '%s'.\n", filename);
            return 0;
		}	
	}
	printf("FILE %s not found.\n",filename);
	return -1;
}


/**
 * read_file - Reads and prints file content by following the FAT chain
 */
int read_file(const char* filename){

	for(int i = 0; i< MAX_FILE_NUM; i++){
		if(strcmp(myfat.directory[i].filename,filename) == 0){
			int start_block = myfat.directory[i].start_block;
            int total_size = myfat.directory[i].size;

			// 파일이 비었는지 체크 
			if(total_size == 0){
				printf("file %s is empty\n",filename);
				return 0;
			}

			// FILE not Initalized  
			if(start_block == -1){            
				printf("File %s not initalized. Use create command first.\n",filename);           
				return -1;   
			}
			
			printf("Content of '%s' : ",filename);

			int block = start_block;
			int bytes_read = 0;
			while(block != 0xFFFF && bytes_read < total_size){
				if(block < 0 || block >= NUM_BLOCKS) {
					printf("FAT chain corrupt while read %s.\n",filename);
					return -1;
				}
		
				int remaining_bytes_in_block = total_size - bytes_read;              
				int read_size = (remaining_bytes_in_block < BLOCK_SIZE) ? remaining_bytes_in_block : BLOCK_SIZE;

				printf("%.*s",read_size,&myfat.data_area[block * BLOCK_SIZE]);
				bytes_read +=read_size;
				int next = myfat.fat_table[block];
				block = next;
			}
			printf("\n");
			return 0;
		}
	}

	printf("File %s not found.\n",filename);
	return -1;
}

/**
 * delete_file - Removes a file and releases its blocks back to the FAT
 */
int delete_file(const char * filename){

	for(int i = 0; i< MAX_FILE_NUM;i++){
		if(strcmp(myfat.directory[i].filename,filename) == 0){
			int start_block = myfat.directory[i].start_block;
			int block = start_block;

			// Release all linked blocks in FAT
			while(block != 0xFFFF){
				int next_block  = myfat.fat_table[block];
				myfat.fat_table[block] = 0 ; // Mark as free
				block = next_block;
			}
				
			myfat.directory[i].filename[0] = '\0';	// Invalidate directory entry
			printf("File '%s' deleted.\n",filename);
			return 0;
		}
	
	}
	printf("File %s not found.\n",filename);
	return -1;
}


/**
 * list_files - Lists all existing files and their sizes
 */
void list_files(){
	printf("Files in the file system.\n");
	for(int i = 0; i < MAX_FILE_NUM; i++){
		if(myfat.directory[i].filename[0] != '\0'){
			printf("File : %s, SIze: %d bytes\n",
				myfat.directory[i].filename,myfat.directory[i].size);
		}
	}
}

/* CLI Execution Logic */
void execute_cmd(char *cmd, char *filename, char* data, int num){

	if(strcmp(cmd,"create") == 0){
		if(num!= 3 || filename == NULL) printf("Usage: create <filename>\n");
		else create_file(filename);
	}
	else if(strcmp(cmd,"write") == 0){
		if(num !=4 || filename == NULL) printf("Usage : write <filename> <data>\n");
		else write_file(filename,data);
	}
	else if(strcmp(cmd,"read") == 0){
		if(num !=3|| filename == NULL) printf("Usage : read <filename>\n");
		else read_file(filename);
	}
	else if(strcmp(cmd,"delete") == 0){
		if(num != 3|| filename == NULL) printf("Usage : delete <filename>\n");
		else delete_file(filename);
	}
	else if(strcmp(cmd,"list") == 0){
		list_files();
	}
	else printf("Invalid command.\n");
}

int main(int argc, char* argv[])
{
	if(argc <=1){
		printf("USAGE : ./fat <COMMAND> [ARGS]...\n");
		exit(1);
	}

	load_file_system();				
	execute_cmd(argv[1],argv[2],argv[3],argc); 	
	save_file_system();
	exit(0);
}