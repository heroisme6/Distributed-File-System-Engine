/*数据放在文件里面读取出来太慢了，这个是为了把它映射到内存里面，实现更快的访问速度*/
#include"/home/share/tfs_largefile/mmap/mmap_file/mmap_file.h"
#include "/home/share/tfs_largefile/mmap/mmap_file/common.h"

static int debug = 1;
namespace qiniu{
    namespace largefile{
        //无参构造函数
        MMapFile::MMapFile():
        size_(0), fd_(-1), data_(NULL)
        {
        }
        //一个参数构造函数
        MMapFile::MMapFile(const int fd):
        size_(0), fd_(fd), data_(NULL)
        {
        }
        //构造函数
        MMapFile::MMapFile(const MMapOption& mmap_option, const int fd):
        size_(0), fd_(fd), data_(NULL)
        {
            mmap_file_option_.max_mmap_size_ = mmap_option.max_mmap_size_;
            mmap_file_option_.first_mmap_size_ = mmap_option.first_mmap_size_;
            mmap_file_option_.per_mmap_size_ = mmap_option.per_mmap_size_;
        }
        //析构函数
        MMapFile::~MMapFile(){
            //如果data不为空，说明之前的内存映射已经成功
            if (data_){
                if (debug) printf("mmap file destruct, fd: %d, maped size: %d, data:%p\n",fd_, size_, data_);
                //进行同步操作(同步)
                msync (data_, size_, MS_SYNC);//同步操作：将内存的数据同步到磁盘中
                munmap (data_, size_);//解除映射

                //参数初始化
                size_ = 0;
                data_ = NULL;
                fd_ = -1;

                mmap_file_option_.max_mmap_size_ = 0;
                mmap_file_option_.first_mmap_size_ = 0;
                mmap_file_option_.per_mmap_size_ = 0;
            }
        }
        //同步文件
        bool MMapFile::sync_file(){
            if (data_ != NULL && size_ > 0){
                return msync(data_, size_, MS_ASYNC) == 0;
            }

            return true;
        }

        //将文件映射到内存，同时设置访问权限
        bool MMapFile::map_file(const bool write){
            int flags = PROT_READ;
            if (write){
                flags |= PROT_WRITE;
            }

            if (fd_ < 0){
                return false;
            }

            if (mmap_file_option_.max_mmap_size_ == 0){
                return false;
            }

            if (size_ < mmap_file_option_.max_mmap_size_){
                size_ = mmap_file_option_.first_mmap_size_;
            }else{
                size_ = mmap_file_option_.max_mmap_size_;
            }

            if(!ensure_file_size(size_)){
                fprintf(stderr, "ensure file size failed in map_file, size:%d\n",size_);
                return false;
            } 
            data_ = mmap(0, size_, flags, MAP_SHARED, fd_, 0);

            //如果映射失败
            if (MAP_FAILED == data_){
                fprintf(stderr, "map file failed %s\n", strerror(errno));

                size_ = 0;
                fd_ = -1;
                data_ = NULL;
                return false;
            }

            if (debug) printf("mmap file successed, fd:%d maped size:%d, data:%p\n", fd_, size_, data_);
            
            return true;
        }
        void * MMapFile::get_data() const{//获取映射到内存的数据的首地址，不允许在内部实现时修改
            return data_;
        }
        int32_t MMapFile::get_size() const{//映射的数据大小,出错
            return size_;
        }
        bool MMapFile::munmap_file(){//解除映射
            if (munmap(data_, size_) == 0){
                return true;
            }else{
                return false;
            }
        }

        bool MMapFile::ensure_file_size(const int32_t size){
            struct stat s;//存放文件的状态,fstat()函数作用是获取文件相关信息
            if (fstat(fd_, &s) < 0){
                fprintf(stderr,"fstar error, error desc:%s\n", strerror(errno));
                return false;
            }

            //如果文件的大小 小于文件已经映射的大小 需要对文件进行扩容
            if (s.st_size < size){
                if (ftruncate(fd_, size) < 0){//ftruncate()函数作用是改变文件的大小
                    fprintf(stderr,"ftruncate error, size:%d, error desc:%s\n", size, strerror(errno));
                    return false;
                }
            }

            return true;
        }

        bool MMapFile::remap_file(){
            //1.防御性编程
            if (fd_ < 0 || data_ == NULL){
                fprintf(stderr,"MMapFile not mapped yet\n");
                return false;
            }

            //2.如果已经映射的内存空间大于等于规定的最大内存大小
            if (size_ == mmap_file_option_.max_mmap_size_){
                fprintf(stderr,"already mapped max size, now size:%d,max size:%d\n",size_,mmap_file_option_.max_mmap_size_);
                return false;
            }

            int32_t new_size = size_ + mmap_file_option_.per_mmap_size_;//原来的尺寸+每次增长的尺寸
            if (new_size > mmap_file_option_.max_mmap_size_){
                new_size = mmap_file_option_.max_mmap_size_;
            }

            //内存加大了，相应的文件也要加大
            if (!ensure_file_size(new_size)){
                fprintf(stderr, "ensure file size failed in map_file, size:%d, error desc:%s\n",size_, strerror(errno));
                return false;
            } 

            if (debug) printf("mremap start, fd:%d, now size:%d, new size:%d, old data:%p\n",fd_, size_, new_size, data_);
            
            void *new_map_data = mremap(data_, size_, new_size, MREMAP_MAYMOVE);
            //分配也可能失败,MAP_FAILED == NULL
            if (MAP_FAILED == new_map_data){
                fprintf(stderr,"mremap failed, fd:%d, new size:%d, error desc:%s\n",fd_, new_size, strerror(errno));
            }else{
                if (debug) 
                    printf("mremap success. fd:%d, now size:%d, new size:%d, old data:%p, new data:%p\n",fd_, size_, new_size, data_, new_map_data);
            }

            data_ = new_map_data;
            size_ = new_size;
            return true;
        }
    }
}