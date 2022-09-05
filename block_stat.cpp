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


//查看块信息
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
    

    //1.加载索引文件
    largefile::IndexHandle * index_handle = new largefile::IndexHandle(".", block_id);//索引文件句柄
    if (debug == 1){
            printf("load index...\n");
    }

    ret = index_handle->load(block_id,bucket_size,mmap_option);
    if (ret != largefile::TFS_SUCCESS){
        fprintf(stderr,"create index %d failed\n",block_id);
        delete index_handle;;
        exit(-2);
    }

    
    delete index_handle;//释放资源
    return 0;
}
