#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILE_NUM	100		// MAX_file_number  : 100
#define NUM_BLOCKS	1024		// MAX_NUM_BLOCK    : 1024
#define BLOCK_SIZE	32		// block_size(byte) : 32
#define MAX_FILE_NAME	100		// max_file_name    : 100
#define FS_STAT	"fs_state.dat"		// DISK FILE 

// File Entry
typedef struct{
	char filename[MAX_FILE_NAME];
	int  start_block;
	int  size;
}FileEntry;

// File System structure
typedef struct{
	int fat_table[NUM_BLOCKS];	//  FAT Table
	FileEntry directory[MAX_FILE_NUM];
	char data_area[NUM_BLOCKS * BLOCK_SIZE];  // 1024 blocks * 32 byte
}FileSystem;

FileSystem myfat;	// File System Instance

/* ==================================================================
 			CONTROL API
===================================================================== */
int create_file(const char *filename);		// Create & allocate block
int write_file(const char *filename, const char* data);  // Write data to file & link blocks in FAT table
int read_file(const char *filename); 			// Read data from File , follow linked blocks
int delete_file(const char *name);			// Delete the file, release blocks in the FAT table
void list_files(void);					// Display a list of all files in the system
int find_free_block(void);				// helper func


/* END OF API*/
void save_file_system(void);		// Save the File System to Disk
void load_file_system(void);		// Restore the File System from Disk

// free block찾아서 인덱스 반환 , 없으면 -1 반
int find_free_block(void){
	for(int i=0; i<NUM_BLOCKS; i++){
		if(myfat.fat_table[i] == 0) // 0 == free
			return i;
	}
	return -1; // no free block
}

// Save file system to disk
void save_file_system(void)
{
	FILE*f = fopen(FS_STAT,"wb");
	if(f== NULL){
		printf("Error : can't save file system state.\n");
		return;
	}
	fwrite(&myfat,sizeof(FileSystem),1,f);
	fclose(f);
}


// Restore the File System from Disk
void load_file_system(void){
	FILE*f = fopen(FS_STAT,"rb");
	if(f== NULL){
		// 첫 실행이거나 저장된 상태 없을 때
		printf("Warning : No saved state found. Starting fresh.\n");
		memset(&myfat,0,sizeof(FileSystem));
		return;
	}
	fread(&myfat,sizeof(FileSystem),1,f);
	fclose(f);
}

/*
 - 파일 생성
 - 1) 같은 이름이 이미 있으면 실패
 - 2) 디렉토리에서 빈 entry를 찾고
 - 3) FAT 테이블에서 free block을 찾아서 start block으로 사용함
 */
int create_file(const char* filename){

	//1. 파일 이름 중복체크
	for(int i = 0; i< MAX_FILE_NUM; i++){
		if(strcmp(myfat.directory[i].filename,filename)==0){
			printf("File '%s' already exists.\n",filename);
			return -1;
		}
	}

	// 2. 디렉토리에서 빈 엔트리 찾기
	for(int i = 0; i< MAX_FILE_NUM; i++){
		if(myfat.directory[i].filename[0]=='\0'){
			// 3. FAT에서  free 블록 찾기
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
	printf("File System full. cannot create more files.\n");
	return -1;
}


/*
 파일 쓰기
 - 기존 파일의 끝 위치를 찾아서 그 뒤부터 이어 쓰기
 - 블록이 꽉 차면 위에 정의된  find_free_block()으로 새 블록을 할당하고 FAT 체인에 연결한다.
 */
int write_file(const char *filename,const char *data){


	// 1) 디렉토리에서 파일 찾기
	for(int i = 0; i< MAX_FILE_NUM; i++){
	    if(strcmp(myfat.directory[i].filename,filename) == 0){

	    int start_block = myfat.directory[i].start_block;
            int file_size   = myfat.directory[i].size;  // 기존 파일의 크기
            int data_len    = strlen(data);  // 새로 쓸 데이터 크

            if (start_block < 0 || start_block >= NUM_BLOCKS) {
                printf("File %s has invalid start block.\n", filename);
                return -1;
            }

            // 2) 현재 파일의 마지막 블록과 그 안에서의 offset 찾기
            int block = start_block;
            int remain = file_size;

            while (remain >= BLOCK_SIZE && myfat.fat_table[block] != 0xFFFF) {
                remain -= BLOCK_SIZE;
                block = myfat.fat_table[block]; // 다음 블록으로 이동
            }

            int offset_in_block = file_size % BLOCK_SIZE;  // 마지막 블록 안에서 이미 사용 중인 바이트 수

            //  3) block, offset_in_block 위치부터 data를 이어서 쓰기 시작
            int bytes_written = 0;
            int data_offset   = 0;
		
	   
            while (bytes_written < data_len) {
                if (block < 0 || block >= NUM_BLOCKS) {
                    printf("FAT chain corrupt file %s.\n", filename);
                    return -1;
                }

		// 현재 블록이 꽉 찬 경우 새 블록으로 연결
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

	    // 4) 파일 크기 갱신 ( 기존 크기 + 새로 쓴 바이트 수 )
            myfat.directory[i].size = file_size + bytes_written;
            printf("Data written to '%s'.\n", filename);
            return 0;
		}	
	}
	printf("FILE %s not found.\n",filename);
	return -1;
}


/*
 - 디렉토리에서 파일을 찾고
 - start_block 부터 fat체인을 따라가며 전체 데이터 출력
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

                        if(start_block == -1){
                          // FILE not Initalized
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

// File delete
int delete_file(const char * filename)
{
	for(int i = 0; i< MAX_FILE_NUM;i++){
		if(strcmp(myfat.directory[i].filename,filename) == 0){
			int start_block = myfat.directory[i].start_block;
			int block = start_block;

			// Release Cluster at FAT Table
			while(block != 0xFFFF){
				int next_block  = myfat.fat_table[block];
				myfat.fat_table[block] = 0 ; // Release Cluster
				block = next_block;
			}
			
			// Remove file at Directory
			myfat.directory[i].filename[0] = '\0';
			printf("File '%s' deleted.\n",filename);

			return 0;

		}
	
	}
	printf("File %s not found.\n",filename);
	return -1;
}


// Display a list of all files in the system.
void list_files(){
	printf("Files in the file system.\n");
	for(int i = 0; i < MAX_FILE_NUM; i++){
		if(myfat.directory[i].filename[0] != '\0'){
			printf("File : %s, SIze: %d bytes\n",
				myfat.directory[i].filename,myfat.directory[i].size);
		}
	}
}

/*
 * - cmd : argv[1]
 * - filename : arvg[2]
 * - data : argv[3]
 * - num : argc
 * */
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

	load_file_system();				// Restore File Systme from disk
	execute_cmd(argv[1],argv[2],argv[3],argc); 	// Execute command
	save_file_system();	// Save File System to Disk
	exit(0);


}












