/*想删除一个块，必须先把索引打开(先指定是哪个块、哪个文件)*/
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

    //2.删除文件的meta info,这里自己输入。实现一个函数：用来实现删除文件对应的segment_meta
    uint64_t file_id = 0;
    cout<<"Type your file_id:"<<endl;
    cin>>file_id;  
    if (file_id < 1){
        cerr<<"Invalid filrid,exit."<<endl;
        exit(-2);
    }

    ret = index_handle->delete_segment_meta(file_id);
    if ( ret != largefile::TFS_SUCCESS ){
        fprintf(stderr, "delete indexx failed.file_id:%lu, ret:%d\n", file_id, ret);
    }
    ret = index_handle->flush();//把内存的数据同步到磁盘
    if (ret != largefile::TFS_SUCCESS){
        fprintf(stderr, "flush mainblock %d failed. file no:%lu\n",block_id, file_id);

    }

    printf("delete successfully!\n");
    delete index_handle;

    return 0;
}
