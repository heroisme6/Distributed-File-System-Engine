#include "/home/share/tfs_largefile/mmap/mmap_file/mmap_file_op.h"
#include <iostream>

using namespace std;
using namespace qiniu;

const static largefile::MMapOption mmap_option = {10240000, 4096, 4096};//内存映射的参数：10M(最大)，4096kb(页大小)，4096(每次增长的大小)

int main(void){
    int ret = 0;
    const char * filename = "mmap_file_op.txt";
    largefile::MMapFileOperation * mmfo = new largefile::MMapFileOperation(filename);//open_flags被缺省掉

    int fd = mmfo->open_file();//创建mmap_file_op.txt文件
    if (fd < 0){
        fprintf(stderr, "open file %s failed.reason:%s\n",filename, strerror(-fd));
        exit(-1);
    }
    ret = mmfo->mmap_file(mmap_option);//对mmap_file_op.txt文件进行映射，文件应该有4096个字节
    if (ret == largefile::TFS_ERROR){
        fprintf(stderr,"mmap_file failed. reason:%s\n",strerror(errno));
        mmfo->close_file();
        exit(-2);
    }
    char buffer[129];
    memset(buffer, '6', 128);

    ret = mmfo->pwrite_file(buffer, 128, 8);//把buffer的数据写到文件中
    if (ret < 0){
        if (ret == largefile::EXIT_DIST_OPER_INCOMPLETE){
            fprintf(stderr, "pwrite_file:read length is less than required!\n");
        }else{
            fprintf(stderr,"pwrite fail %s falid.reason:%s\n",filename, strerror(-ret));
        }
    }

    memset(buffer, 0, 128);//初始化buffer数组
    ret = mmfo->pread_file(buffer, 128, 8);//从文件映射中读出来
    if (ret < 0){
        if (ret == largefile::EXIT_DIST_OPER_INCOMPLETE){
            fprintf(stderr, "pread_file:read length is less than required!\n");
        }else{ 
            fprintf(stderr,"pread fail %s falid.reason:%s\n",filename, strerror(-ret));
        }
    }else{
        buffer[128] = '\0';//虽然参数memset第二个参数要求是一个整数，但是整型和字符型是互通的。
        //但是赋值为 '\0' 和 0 是等价的，因为字符 '\0' 在内存中就是 0。所以在 memset 中初始化为 0 也具有结束标志符 '\0' 的作用，
        //所以通常我们就写“0”
        printf("read:%s\n",buffer);
    }

    ret = mmfo->flush_file();//将内存同步到磁盘中，成功的话，文件会有128个6
    if (ret == largefile::TFS_ERROR){
        fprintf(stderr, "flush file failed. reason:%s\n", strerror(errno));
    }

    ret = mmfo->pwrite_file(buffer, 128, 4000);
    mmfo->munmap_file();//解除映射
    mmfo->close_file();//关闭文件
    return 0;
}