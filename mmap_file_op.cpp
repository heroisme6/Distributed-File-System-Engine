#include "/home/share/tfs_largefile/mmap/mmap_file/common.h"
#include "/home/share/tfs_largefile/mmap/mmap_file/mmap_file_op.h"

static int debug = 1;

namespace qiniu{
    namespace largefile{
        //映射文件
        int MMapFileOperation::mmap_file(const MMapOption& mmap_option){
            if (mmap_option.max_mmap_size_ < mmap_option.first_mmap_size_){//最大值不能小于初始值
                return TFS_ERROR;
            }

            if (mmap_option.max_mmap_size_ <= 0){
                return TFS_ERROR;
            }

            //要执行mmap_file()函数，首先要获得打开文件的句柄，但可能这个mmap_file()函数被重复调用
            int fd = check_file();
            if (fd < 0){
                fprintf(stderr, "MMapFileOperation::mmap_file-checking file failed!");
                return TFS_ERROR;
            }

            if (!is_mapped_){
                if (map_file_) delete(map_file_);//如果map_file_对象之前已经被创建过，需要先删除掉
                map_file_ = new MMapFile(mmap_option, fd);
                is_mapped_ = map_file_->map_file(true);//设置可写
            }

            //成功了
            if (is_mapped_){
                return TFS_SUCCESS;
            }else{
                return TFS_ERROR;
            }
        }

        //解除映射
        int MMapFileOperation:: munmap_file(){
            //如果已经映射了
            if (is_mapped_ && map_file_ != NULL){
                delete(map_file_);//删除map_file_对象时，会调用这个对象的析构函数
                is_mapped_ = false;
            }
            //没有映射，直接返回成功
            return TFS_SUCCESS;
        }

        //获取映射的数据
        void *MMapFileOperation::get_map_data() const{
            if (is_mapped_){
                return map_file_->get_data();
            }

            return NULL;
        }

        //
        int MMapFileOperation::pread_file(char *buf, const int32_t size, const int64_t offset){

            //情况1：内存已经映射,可以直接从内存里面读
            //如果要读的最终长度大于内存已经映射的大小，需要扩容
            if (is_mapped_ && (offset + size) > map_file_->get_size()){
                if (debug) fprintf(stderr, "MMapFileOperation:pread, size:%d, offst:%ld, map file size:%d.need remap\n", 
                                    size, offset, map_file_->get_size());//平时用%lld来输出64位有符号整型数  
                map_file_->remap_file();//追加内存
            }
            //如果要读的最终长度小于内存已经映射的大小，可以直接在内存里面读
            if (is_mapped_ && (offset + size) <= map_file_->get_size()){
                memcpy(buf, (char *)map_file_->get_data() + offset, size);//从内存读到buffer里面去
                return TFS_SUCCESS;
            }

            //情况2：内存没有映射或是映射不全，只能从文件里面去读了 
            return FileOperation::pread_file(buf, size, offset);//(之前FileOperation写成了MMapFileOperation，找了一天，才发现错误)

        }

        //写
        int MMapFileOperation::pwrite_file(const char * buf, const int32_t size, const int64_t offset){
            //情况1：内存已经映射,可以直接从内存里面写
            //如果要写的最终长度大于内存已经映射的大小，需要扩容
            if (is_mapped_ && (offset + size) > map_file_->get_size()){
                if (debug) fprintf(stderr, "MMapFileOperation:pwrite, size:%d, offst:%ld, map file size:%d.need remap\n", 
                                    size, offset, map_file_->get_size());//平时用%lld来输出64位有符号整型数  
                map_file_->remap_file();//追加内存
            }
            //如果要写的最终长度小于内存已经映射的大小，可以直接在内存里面写
            if (is_mapped_ && (offset + size) <= map_file_->get_size()){
                memcpy((char *)map_file_->get_data() + offset, buf, size);//把buffer写到内存去
                return TFS_SUCCESS;
            }

            //情况2：内存没有映射或是需要写入的数据映射不全，只能从文件里面去写了
            return FileOperation::pwrite_file(buf, size, offset);//(之前FileOperation写成了MMapFileOperation，找了一天，才发现错误)
        }

        //把内存的数据同步到磁盘
        int MMapFileOperation::flush_file(){
            if (is_mapped_){//只有当文件映射了才需要同步
                if (map_file_->sync_file()){
                    return TFS_SUCCESS;
                }else{
                    return TFS_ERROR;
                }
            }

            return FileOperation::flush_file();//如果没有映射，但是文件被打开了
        }
    };
}