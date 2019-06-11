#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>

#define PAGE_SIZE 4096
#define BUF_SIZE 512
typedef long long ll;
int main (int argc, char* argv[])
{
	char buf[BUF_SIZE];
	int i, dev_fd, file_fd;// the fd for the device and the fd for the input file
	size_t ret, file_size = 0, data_size = -1;
	char file_name[50];
	char method[20];
	char ip[20];
	struct timeval start;
	struct timeval end;
	double trans_time; //calulate the time between the device is opened and it is closed
	char *kernel_address, *file_address;
    size_t pagenum=0;
    char *mmapaddr=NULL;


	strcpy(file_name, argv[1]);
	strcpy(method, argv[2]);
	strcpy(ip, argv[3]);

	if( (dev_fd = open("/dev/slave_device", O_RDWR)) < 0)//should be O_RDWR for PROT_WRITE when mmap()
	{
		perror("failed to open /dev/slave_device\n");
		return 1;
	}
	gettimeofday(&start ,NULL);
	if( (file_fd = open (file_name, O_RDWR | O_CREAT | O_TRUNC)) < 0)
	{
		perror("failed to open input file\n");
		return 1;
	}

	if(ioctl(dev_fd, 0x12345677, ip) == -1)	//0x12345677 : connect to master in the device
	{
		perror("ioclt create slave socket error\n");
		return 1;
	}
// we can comment this line
    write(1, "ioctl success\n", 14);

	switch(method[0])
	{
		case 'f'://fcntl : read()/write()
			do
			{
				ret = read(dev_fd, buf, sizeof(buf)); // read from the the device
				write(file_fd, buf, ret); //write to the input file
				file_size += ret;
			}while(ret > 0);
			break;
        case 'm': //mmap
            while((ret=read(dev_fd,buf,sizeof(buf)))>0){
                size_t remain = 0;
                if((ll)PAGE_SIZE-(ll)ret-(ll)data_size <0 ||data_size<0){
                    if(data_size<0){
                        remain = PAGE_SIZE - data_size;
                        pagenum++;
                        for(size_t i=0; i<remain; i++){
                            mmapaddr[data_size+i] = buf[i];
                        }
                        if(msync(mmapaddr,PAGE_SIZE,MS_ASYNC)==-1)
                            perror("Can't sync to disk :(");
                        if(munmap(mmapaddr,PAGE_SIZE)==-1)
                            perror("Failed when unmmap!!");
                    }
                    if(lseek(file_fd,PAGE_SIZE-1,SEEK_END)==-1)
                        perror("lseek to end failed!!");
                    if(write(file_fd,"",1)==-1)
                        perror("failed when write to end!");
                    mmapaddr = (char*)mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED,file_fd,pagenum*PAGE_SIZE);
                    if(mmapaddr==MAP_FAILED)
                        perror("mmap failed!");
                    data_size = 0;
                }
                for(size_t i =0; i<ret-remain; i++)
                    mmapaddr[data_size+i] = buf[remain+i];
                data_size += ret;
                file_size += ret;
            }
            if(msync(mmapaddr,PAGE_SIZE,MS_ASYNC)==-1)
                perror("Can't sync to disk :( ");
            if(munmap(mmapaddr,PAGE_SIZE)==-1)
                perror("Failed when unmmap!!");
            if(ftruncate(file_fd,file_size)==-1)
                perror("Fail to truncate!");



	}
//	ioctl(dev_fd, 123456);



	if(ioctl(dev_fd, 0x12345679) == -1)// end receiving data, close the connection
	{
		perror("ioclt client exits error\n");
		return 1;
	}
	gettimeofday(&end, NULL);
	trans_time = (end.tv_sec - start.tv_sec)*1000 + (end.tv_usec - start.tv_usec)*0.0001;
	printf("Transmission time: %lf ms, File size: %ld bytes\n", trans_time, file_size / 8);


	close(file_fd);
	close(dev_fd);
	return 0;
}


