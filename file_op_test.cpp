#include "/home/share/tfs_largefile/mmap/mmap_file/file_op.h"
#include "/home/share/tfs_largefile/mmap/mmap_file/common.h"

using namespace std;
using namespace qiniu;

int main(void){
    const char * filename = "file_op.txt";
    largefile::FileOperation *fileOP = new largefile::FileOperation(filename, O_CREAT | O_RDWR | O_LARGEFILE);

    int fd = fileOP->open_file();
    if (fd < 0){//打开文件失败
        fprintf(stderr, "open file %s failed. reason:%s\n", filename,strerror(-fd));
        exit(-1);
    }

    char buffer[65]; 

    memset(buffer, '8', 64);
    int ret = fileOP->pwrite_file(buffer, 64, 128);//偏移量为128的位置写64个8
    if (ret < 0){
        if (ret == largefile::EXIT_DIST_OPER_INCOMPLETE){
            fprintf(stderr,"pwrite_file:read length is less than required!");
        }else{
            fprintf(stderr, "pwrite file %s failed. reason:%s\n",filename, strerror(-ret)); 
        }
    }

    memset(buffer, 0, 64);
    ret = fileOP->pread_file(buffer, 64, 128);//把128位置的64个8读出来
    fileOP->close_file();
    if (ret < 0){
        if (ret == largefile::EXIT_DIST_OPER_INCOMPLETE){
            fprintf(stderr,"pread_file:read length is less than required!");
        }else{
            fprintf(stderr, "pread file %s failed. reason:%s\n",filename, strerror(-ret));
        }   
    }else{
        buffer[64] = '\0';
        printf("read:%s\n",buffer);
    }

    memset(buffer, '9', 64);
    ret = fileOP->write_file(buffer, 64);//pwrite和wread并不移动指针，指针还是在头部，所以64个9在头部
    if (ret < 0){
        if (ret == largefile::EXIT_DIST_OPER_INCOMPLETE){
            fprintf(stderr,"pwrite_file:read length is less than required!");
        }else{
            fprintf(stderr, "write _file %s failed. reason:%s\n",filename, strerror(-ret));
        }
    }

    fileOP->close_file();
    return 0;
}