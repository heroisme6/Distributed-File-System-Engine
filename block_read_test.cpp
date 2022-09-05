/*找到索引文件，打开主块文件*/
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

    //2.读取文件的meta info,这里自己输入
    uint64_t file_id = 0;
    cout<<"Type your file_id:"<<endl;
    cin>>file_id;  
    if (file_id < 1){
        cerr<<"Invalid filrid,exit."<<endl;
        exit(-1);
    }

    largefile::MetaInfo meta;
    ret = index_handle->read_segment_meta(file_id, meta);
    if (ret != largefile::TFS_SUCCESS){
        fprintf(stderr,"read_segment_meta error.file_id:%lu, ret:%d\n",file_id, ret);//uint64_t类型用lu输出
        exit(-3);
    }

    //3.根据meta info读取文件
    std::stringstream tmp_stream;
    tmp_stream<<"."<<largefile::MAINBLOCK_DIR_PREFIX<<block_id;
    tmp_stream>>mainblock_path;
    largefile::FileOperation * mainblock = new largefile::FileOperation(mainblock_path, O_RDWR);//被操作的主块文件的对象
    char* buffer = new char[meta.get_size() + 1];
    ret = mainblock->pread_file( buffer, meta.get_size(), meta.get_offset() );
    if (ret != largefile::TFS_SUCCESS){
        fprintf(stderr, "read from main block failed.ret:%d, reason:%s\n", ret, strerror(errno));

        mainblock->close_file();
        delete mainblock;
        delete index_handle;
        exit(-4);
    }

    buffer[meta.get_size()] = '\0';//结束符
    printf("read size:%d, content:%s\n", meta.get_size(), buffer);

    mainblock->close_file();
    delete mainblock;
    delete index_handle;//释放资源。为什么index_handle不先关闭呢？因为析构函数中已经帮助我们关闭了
    return 0;
}
