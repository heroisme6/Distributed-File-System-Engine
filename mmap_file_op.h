/*文件映射操作肯定要基于文件映射来进行，不进行文件映射操作谁呢？*/
#ifndef QINIU_LARGEFILE_MMAPFILE_OP_H_
#define QINIU_LARGEFILE_MMAPFILE_OP_H_

#include "/home/share/tfs_largefile/mmap/mmap_file/file_op.h"
#include "/home/share/tfs_largefile/mmap/mmap_file/common.h"
#include "/home/share/tfs_largefile/mmap/mmap_file/mmap_file.h"

namespace qiniu{
    namespace largefile{
        class MMapFileOperation:public FileOperation{//继承
        public:
            MMapFileOperation(const std::string& file_name, const int open_flags = O_CREAT | O_RDWR | O_LARGEFILE):
            FileOperation(file_name, open_flags), map_file_(NULL), is_mapped_(false){//文件操作类没有默认的构造函数
    
            }

            //析构函数
            ~MMapFileOperation(){
                if (map_file_){
                    delete(map_file_);
                    map_file_ = NULL;
                }
            }
            int mmap_file(const MMapOption& mmap_option);//需要一个内存映射的参数
            int munmap_file();//解除映射

            int pread_file(char *buf, const int32_t size, const int64_t offset);
            int pwrite_file(const char * buf, const int32_t size, const int64_t offset);
        
            void *get_map_data() const;//获取数据，const表示只读，不能修改
            int flush_file();//将数据映射到磁盘
        private:
            MMapFile *map_file_;//文件映射的对象
            bool is_mapped_;//定义一个开关

        };
    }
}

#endif
