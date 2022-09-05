/*内存映射：把文件映射到内存中去*/
#ifndef QINIU_LARGEFILE_MMAPFILE_H_//防止重复包含
#define QINIU_LARGEFILE_MMAPFILE_H_
#include<unistd.h>
#include "/home/share/tfs_largefile/mmap/mmap_file/common.h"

//不先写类的定义，先定义模块，便于管理
namespace qiniu {
    namespace largefile {
        /*struct MMapOption{
            //int32_t不管在32还是64位操作系统，都是32位的
            int32_t max_mmap_size_;
            int32_t first_mmap_size_;
            int32_t per_mmap_size_;
        };*/

        class MMapFile{
            //定义接口
        public:
            //构造函数
            MMapFile();

            //映射也可以直接通过某一个已经打开的文件描述符进行映射：fd
            //explicit 避免不清晰的构造
            explicit MMapFile(const int fd);
            MMapFile(const MMapOption& mmap_option, const int fd);

            //析构函数
            ~MMapFile();

            //把内存的内容共享到磁盘中
            bool sync_file();//同步文件

            //映射文件时，设置一个属性
            bool map_file(const bool write = false);//将文件映射到内存，默认情况下，不可以写.
            
            //映射完了，要让别人读数据，需要返回文件起始地址指针
            void *get_data() const;//获取映射到内存的数据的首地址，不允许在内部实现时修改
            int32_t get_size() const;//映射的数据大小

            //第一次映射完，解除映射，再进行映射
            bool munmap_file();//解除映射
            bool remap_file();//重新映射,追加什么存储空间
        private:
            bool ensure_file_size(const int32_t size);//进行扩容
            int32_t size_;//已经映射的大小
            int fd_;//文件地址
            void *data_;//映射后的数据所在内存首地址
            MMapOption mmap_file_option_;
        };
    }
}

#endif