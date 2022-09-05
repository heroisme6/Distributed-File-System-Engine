
#include</home/share/tfs_largefile/mmap/mmap_file/mmap_file.h>
#include "/home/share/tfs_largefile/mmap/mmap_file/common.h"

using namespace std;
using namespace qiniu;
static const mode_t OPEN_MODE = 0644;//6->110 4->100 4->100
const static largefile::MMapOption mmap_option = {10240000, 4096, 4096};//内存映射的参数：10M(最大)，4096kb(页大小)，4096(每次增长的大小)
//文件映射：前提是一个打开的文件
int open_file(string file_name, int open_flags){
    int fd = open(file_name.c_str(), open_flags, OPEN_MODE);//第一个参数需要c类型，所以需c_str()函数将string类型转变成char类型
    if (fd < 0){
        return -errno;//errno,linux系统如果出错了，会把错误的编号设置在errno中
                //加-号比较巧妙，负数表示出错，出错的原因在加一个符号就可以显示出来了，用sterror(errno)

    } 
    return fd;


}

int main(void){
    const char *filename = "./mapfile_test.txt";//将该文件保存到当前文件夹下

    //1.打开/创建一个文件，取得文件的句柄，用open函数
    int fd = open_file(filename, O_RDWR | O_CREAT | O_LARGEFILE);//可读可写|打开文件不存在，创建新文件|大文件模式
    if (fd < 0){
        fprintf(stderr, "open file failed.filename:%s, error desc:%s\n",filename, strerror(-fd));
        return -1;//往往负数表示出错
    }

    //创建文件映射对象
    largefile::MMapFile *map_file = new largefile::MMapFile(mmap_option, fd);//构造函数

    bool ismapped = map_file->map_file(true);
    if (ismapped) {
        map_file->remap_file();
        memset(map_file->get_data(), '8', map_file->get_size());//拿到内存地址，进行写：从map_file->get_data()这个地址的内存开始，将长度为map_file->get_size()的区域的字节全部设置为8
        map_file->sync_file();//写回到文件中去，内存与文件的同步
        map_file->munmap_file();//解除映射
    }
    else {
        fprintf(stderr, "map file filed\n");
    }

    close(fd);
    return 0;
}