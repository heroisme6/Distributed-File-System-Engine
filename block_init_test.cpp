/*1、这几个字符串的地址&c1,&c2,&c3都不相同，但是char *c1 = "abc"和 c3 = "abc"中c1,c3的地址是相同的。

     这是因为char *c1 = "abc"中，定义了一个字符串常量，存放在global区域。当编译c3 = "abc"时，并没有再生成一个 "abc"字符串，
     而是在常量区中找到这个字符串，让c3指向它。

     即 char c[] 把内容存在stack ， char *c 则把指针存在stack，把内容存在constants.

2、char c2[] = "abc";c2、&c2的值是一样的。

3、 printf("%d %d %s %c\n",&c1,c1,c1,*c1);   结果： 2272348 4202624 abc a

        &a 表示a的地址 . 

        a 表示a存储的内容, 因为a是指针, 所以它的内容读取出来就是一个地址 . 

        *a 表示a指针存储的这个地址存储的内容 . */
//块初始化步骤：1.创建一个主块文件；2.创建一个索引文件；3.调用IndexHandle的create()函数
#include "/home/share/tfs_largefile/mmap/mmap_file/common.h"
#include "/home/share/tfs_largefile/mmap/mmap_file/file_op.h"//生成主块文件，需要用到file_op.h
#include "/home/share/tfs_largefile/mmap/mmap_file/index_handle.h"
#include <sstream>

using namespace qiniu;
using namespace std;

const static largefile::MMapOption mmap_option = {1024000,4096,4096};//内存映射的参数
const static uint32_t main_blocksize = 1024*1024*64;//主块文件的大小：64M
const static uint32_t bucket_size = 1000;//哈希桶的大小
static int32_t block_id = 1;
static int debug = 1;

int main(int argc, char** argv){//以rm -f a.out命令为例，argv[0] = 'rm' argv[1] = '-f' argv[2] = 'a.out',char **c 也是指针，指向二维数组. 其它表达char *c[], char c[ ][ ]
                                //char *b=“12345” or char b[]="12345" 是指针指向一个字符串
    std::string mainblock_path;
    std::string index_path;
    int32_t ret = largefile::TFS_SUCCESS;

    cout<<"Type your blockid:"<<endl;
    cin>>block_id;  
    if (block_id < 0){
        cerr<<"Invalid blockid,exit."<<endl;
        exit(-1);
    }
    

    //1.创建索引文件
    largefile::IndexHandle * index_handle = new largefile::IndexHandle(".", block_id);//索引文件句柄
    if (debug == 1){
            printf("init index...\n");
    }

    ret = index_handle->create(block_id,bucket_size,mmap_option);
    if (ret != largefile::TFS_SUCCESS){
        fprintf(stderr,"create index %d failed\n",block_id);
        delete index_handle;;
        exit(-3);
    }

    //2.生成主块文件
    std::stringstream tmp_stream;
    tmp_stream<<"."<<largefile::MAINBLOCK_DIR_PREFIX<<block_id;
    tmp_stream>>mainblock_path;

    largefile::FileOperation *mainblock = new largefile::FileOperation(mainblock_path, O_RDWR | O_LARGEFILE | O_CREAT);
    //生成文件，并修改成指定的大小
    ret = mainblock->ftruncate_file(main_blocksize);
    if (ret != 0){//成功会返回0.不成功返回的不是0  
        fprintf(stderr,"create main block %s falied.reason:%s\n",mainblock_path.c_str(), strerror(errno));
        delete mainblock;//删除主块文件之后，还需要删除索引文件，索引文件已经加载到内存了
        //创建索引文件时的步骤是：1.把头部初始化；2.把头部写到文件中去；3.把文件映射到内存。
        //删除索引文件的步骤和创建索引文件时的步骤相反：1.解除映射；2.删除文件；
        index_handle->remove(block_id);
        exit(-2);
    }

    //其它操作
    mainblock->close_file();//关闭文件
    index_handle->flush();//将内存的数据同步到文件中去
    delete mainblock;//释放资源
    delete index_handle;//释放资源
    return 0;
}
