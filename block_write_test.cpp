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

    //2.文件写入至主块文件
    std::stringstream tmp_stream;
    tmp_stream<<"."<<largefile::MAINBLOCK_DIR_PREFIX<<block_id;
    tmp_stream>>mainblock_path;

    largefile::FileOperation *mainblock = new largefile::FileOperation(mainblock_path, O_RDWR | O_LARGEFILE | O_CREAT);//生成主块
    
    char buffer[4096];//保存写入的内容
    memset(buffer, '6', sizeof(buffer));

    int32_t data_offset = index_handle->get_block_data_offset();//未使用数据起始的偏移
    uint32_t file_no = index_handle->block_info()->seq_no_;//下一个可分配的文件编号1.......(2^64-1)
    if ( (ret = mainblock->pwrite_file(buffer, sizeof(buffer), data_offset)) != largefile::TFS_SUCCESS ){//向文件中写内容
        fprintf(stderr,"write to main block failed.ret:%d,reason:%s\n",ret, strerror(errno));
        delete mainblock;
        delete index_handle;
        exit(-3);
    }

    //向文件中写成功后，未使用数据起始的偏移data_offset进行移动
    //3.在索引文件中写入MetaInfo
    largefile::MetaInfo meta;
    meta.set_file_id(file_no);
    meta.set_offset(data_offset);
    meta.set_size(sizeof(buffer));

    
    ret = index_handle->write_segment_meta(meta.get_key(), meta);
    if (ret == largefile::TFS_SUCCESS){
        //1.更新索引头部信息
        index_handle->commit_block_data_offset( sizeof(buffer) );
        //2.更新块信息,更新块信息需要两个前提，1.索引信息写成功了；2.主块文件写成功了
        index_handle->update_block_info(largefile::C_OPER_INSERT, sizeof(buffer));
        
        index_handle->flush();//把内存的数据同步到磁盘
        if (ret != largefile::TFS_SUCCESS){
            fprintf(stderr, "flush mainblock %d failed. file no:%u\n",block_id, file_no);

        }
    }else{
        fprintf(stderr, "write_segment_meta-mainblock:%d failed.file no:%u\n", block_id, file_no);//测试时，这里出现了错误，原因是在update_block_info()函数中，file_count_
        //和seq_no_未++.导致write_segment_meta()函数返回了EXIT_META_UNEXPECT_FOUND_ERROR;
    }
    
    if (ret != largefile::TFS_SUCCESS){
        fprintf(stderr, "write to mainblock %d failed. file no:%u\n", block_id, file_no);
    }else{
        if (debug) printf("write successfully.file no:%u ,block_id:%d\n",file_no, block_id);
    }
    
    
    mainblock->close_file();
    delete mainblock;//释放资源
    delete index_handle;//释放资源
    return 0;
}
