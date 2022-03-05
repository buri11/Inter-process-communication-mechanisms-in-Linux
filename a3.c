#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>


#define MY_NB 87771

int main(){
	char write_pipe_name[] = "RESP_PIPE_87771";
	char read_pipe_name[] = "REQ_PIPE_87771";
	int fd_write = -1;
	int fd_read = -1;
	int fd_shm = -1;
	unsigned char *file_data = NULL;
	char *shm_data = NULL;
	unsigned int shm_size = 0;
	unsigned int file_size = 0;

	if(mkfifo(write_pipe_name, 0600) != 0){
		printf("ERROR\ncannot create the response pipe\n");
		return 1;
	}

	fd_read = open(read_pipe_name, O_RDONLY);
	if(fd_read == -1){
		printf("ERROR\ncannot open the request pipe\n");
		return 1;
	}

	fd_write = open(write_pipe_name, O_WRONLY);
	if(fd_write == -1){
		perror("Error opening write pipe!\n");
		return 1;
	}

	char connect_msg[] = "CONNECT";
	int length = strlen(connect_msg);
	if((write(fd_write, &length, 1) != -1) && (write(fd_write, connect_msg, strlen(connect_msg)) != -1)){
		printf("SUCCESS\n");
	}
	
	
	while(true){
		unsigned int comm_size = 0;
		if(read(fd_read, &comm_size, 1) == 0){
			break;
		}
		char command[comm_size+1];
		read(fd_read, command, comm_size);
		command[comm_size] = '\0';

		if(strcmp(command, "PING") == 0){
			char ping[] = "PING";
			char pong[] = "PONG";
			unsigned int my_nb = MY_NB;
			unsigned int ping_len = 4;

			write(fd_write, &ping_len, 1);
			write(fd_write, ping, strlen(ping));
			write(fd_write, &ping_len, 1);
			write(fd_write, pong, strlen(pong));
			write(fd_write, &my_nb, sizeof(my_nb));
		}
		
		if(strcmp(command, "CREATE_SHM") == 0){
			read(fd_read, &shm_size, sizeof(shm_size));
			bool ok = true;
			fd_shm = shm_open("/qAzAGwAu", O_CREAT | O_RDWR, 0664);
			if(fd_shm < 0){
				ok = false;
			}
			ftruncate(fd_shm, shm_size);
			shm_data = (char*)mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
			if(shm_data == (void*)-1){
				ok = false;
			}

			char create_shm[] = "CREATE_SHM";
			unsigned int size_create = strlen(create_shm);
			write(fd_write, &size_create, 1);
			write(fd_write, create_shm, size_create);
			if(ok){
				char success[] = "SUCCESS";
				unsigned succ_len = strlen(success);
				write(fd_write, &succ_len, 1);
				write(fd_write, success, succ_len);
			}else{
				char err[] = "ERROR";
				unsigned err_len = strlen(err);
				write(fd_write, &err_len, 1);
				write(fd_write, err, err_len);
			}
		}

		if(strcmp(command, "WRITE_TO_SHM") == 0){
			unsigned int offset = 0;
			unsigned int value = 0;
			read(fd_read, &offset, sizeof(offset));
			read(fd_read, &value, sizeof(value));

			char write_shm[] = "WRITE_TO_SHM";
			unsigned int size_write = strlen(write_shm);
			write(fd_write, &size_write, 1);
			write(fd_write, write_shm, size_write);
			if(offset < 0 || offset > shm_size - 4){
				char err[] = "ERROR";
				unsigned int err_size = strlen(err);
				write(fd_write, &err_size, 1);
				write(fd_write, err, err_size);
			}else{
				lseek(fd_shm, offset, SEEK_SET);
				write(fd_shm, &value, sizeof(value));
				char succ[] = "SUCCESS";
				unsigned int succ_size = strlen(succ);
				
				write(fd_write, &succ_size, 1);
				write(fd_write, succ, succ_size);
			}
		}

		if(strcmp(command, "MAP_FILE") == 0){
			unsigned int path_len = 0;
			read(fd_read, &path_len, 1);
			char path[path_len+1];
			read(fd_read, path, path_len);
			path[path_len] = '\0';

			write(fd_write, &comm_size, 1);
			write(fd_write, command, comm_size);

			bool ok = true;
			int fd_arg = -1;
			fd_arg = open(path, O_RDONLY);
			if(fd_arg == -1){
				ok = false;
			}
			
			file_size = lseek(fd_arg, 0, SEEK_END);
			lseek(fd_arg, 0, SEEK_SET);

			
			file_data = (unsigned char*)mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd_arg, 0);

			if(file_data == (void*)-1){
				ok = false;
			}

			if(ok){
				char succ[] = "SUCCESS";
				unsigned int succ_size = strlen(succ);
				write(fd_write, &succ_size, 1);
				write(fd_write, succ, succ_size);
			}else{
				char err[] = "ERROR";
				unsigned int err_size = strlen(err);
				write(fd_write, &err_size, 1);
				write(fd_write, err, err_size);
			}
		}

		if(strcmp(command, "READ_FROM_FILE_OFFSET") == 0){
			unsigned int nb_bytes = 0;
			unsigned int offset = 0;
			read(fd_read, &offset, sizeof(offset));
			read(fd_read, &nb_bytes, sizeof(nb_bytes));

			write(fd_write, &comm_size, 1);
			write(fd_write, command, comm_size);
			if( (offset+nb_bytes) > file_size || shm_size == 0){
				char err[] = "ERROR";
				unsigned int err_size = strlen(err);
				write(fd_write, &err_size, 1);
				write(fd_write, err, err_size);
			}else{
				char contents[nb_bytes];
				int iterator = 0;
				for(int i = offset; i < offset+nb_bytes; i++){
					contents[iterator++] = file_data[i];
				}
				lseek(fd_shm, 0, SEEK_SET);
				write(fd_shm, contents, nb_bytes);

				char succ[] = "SUCCESS";
				unsigned int succ_size = strlen(succ);
				write(fd_write, &succ_size, 1);
				write(fd_write, succ, succ_size);
			}
		}

		if(strcmp(command, "READ_FROM_FILE_SECTION") == 0){
			unsigned int section_nb = 0;
			unsigned int offset = 0;
			unsigned int nb_bytes = 0;
			read(fd_read, &section_nb, 4);
			read(fd_read, &offset, 4);
			read(fd_read, &nb_bytes, 4);

			write(fd_write, &comm_size, 1);
			write(fd_write, command, comm_size);
			
			char magic[3];
			for(int i = 0; i < 2; i++){
				magic[i] = file_data[i];
			}
			magic[2] = '\0';
			//unsigned int header_size = (unsigned int)(256 * file_data[3] + file_data[2]);
			unsigned int version = (unsigned int)(256 * file_data[5] + file_data[4]);
			unsigned int nb_sections = (unsigned int)(file_data[6]);
			
			if(strcmp(magic, "Ih") != 0 || version < 46 || version > 147 || nb_sections < 9 || 
			nb_sections > 12 || section_nb > nb_sections || section_nb > 12){
				char err[] = "ERROR";
				unsigned int err_size = strlen(err);
				
				write(fd_write, &err_size, 1);
				write(fd_write, err, err_size);
				continue;
			}
			unsigned int sect_type = 0;
			unsigned int sect_offset = 0;
			unsigned int sect_size = 0;
			int sect_start_addr = 7 + ((section_nb-1) * 17);
			sect_type = (unsigned int)(256 * file_data[sect_start_addr+7+1] + file_data[sect_start_addr+7]);
			if(sect_type != 66 && sect_type != 97 && sect_type != 23 && sect_type != 28 && sect_type != 55 &&
			sect_type != 57 && sect_type != 54){
				char err[] = "ERROR";
				unsigned int err_size = strlen(err);
				write(fd_write, &err_size, 1);
				write(fd_write, err, err_size);
				continue;
			}
			sect_offset = (unsigned int)(256*256*256 * file_data[sect_start_addr+12] + 256*256 * file_data[sect_start_addr+11] + 
			256 * file_data[sect_start_addr+10] + file_data[sect_start_addr+9]);
			sect_size = (unsigned int)(256*256*256 * file_data[sect_start_addr+16] + 256*256 * file_data[sect_start_addr+15] + 
			256 * file_data[sect_start_addr+14] + file_data[sect_start_addr+13]);
			
			if(offset+nb_bytes > sect_size){
				char err[] = "ERROR";
				unsigned int err_size = strlen(err);
				write(fd_write, &err_size, 1);
				write(fd_write, err, err_size);
				continue;
			}
			
			char result[nb_bytes];
			int iterator = 0;
			for(int i = sect_offset+offset; i < sect_offset+offset+nb_bytes; i++){
				result[iterator++] = file_data[i];
			}
			
			lseek(fd_shm, 0, SEEK_SET);
			write(fd_shm, result, nb_bytes);

			char succ[] = "SUCCESS";
			unsigned int succ_size = strlen(succ);
			write(fd_write, &succ_size, 1);
			write(fd_write, succ, succ_size);
		}

		if(strcmp(command, "READ_FROM_LOGICAL_SPACE_OFFSET") == 0){
			break;
		}

		if(strcmp(command, "EXIT") == 0){
			break;
		}

	}
	
	close(fd_read);
	close(fd_write);
	unlink(write_pipe_name);
	
	return 0;
}

