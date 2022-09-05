#include "/home/share/tfs_largefile/mmap/mmap_file/file_op.h"
#include "/home/share/tfs_largefile/mmap/mmap_file/common.h"

namespace qiniu{
    namespace largefile{
        FileOperation::FileOperation(const std::string &file_name, const int open_flags):
        fd_(-1),open_flags_(open_flags)
        {
            file_name_ = strdup(file_name.c_str());//strdup字符串复制
        }

        FileOperation::~FileOperation(){
            if (fd_ > 0){
                ::close(fd_);//关闭文件,两个::的意思是全局的意思，全局调用close()函数
            }

            //释放strdup分配的内存
            if (NULL != file_name_){
                free(file_name_);
                file_name_ = NULL;
            }
        }
        //打开文件
        int FileOperation::open_file(){
            //如果文件已经打开
            if (fd_ > 0){
                close(fd_);
                fd_ = -1;
            }

            fd_ = ::open(file_name_, open_flags_, OPEN_MODE);
            if (fd_ < 0){
                return -errno;
            }

            return fd_;
        }
        //关闭文件
        void FileOperation::close_file(){
            if (fd_ < 0){
                return;
            }

            ::close(fd_);
            fd_ = -1;
        }

        //精细化从文件里读数据，需要buffer;nbytes是要读的字节数；offset:偏移量(用64位的，避免超出32位的表达范围)
        int FileOperation::pread_file(char *buf, const int32_t nbytes, const int64_t offset){
            //要做好分多次读的准备
            int32_t left = nbytes;//left表示剩余的要读字节数
            int64_t read_offset = offset;//起始位置的偏移量
            int32_t read_len = 0;//已读的长度
            char *p_tmp = buf;//文件在buffer中的地址,保存最新的位置

            int i = 0;

            while (left > 0){
                i++;

                //读取次数达到限制
                if (i >= MAX_DISK_TIMES){
                    break;
                }

                //保证文件打开
                if (check_file() < 0){
                    return -errno;
                }

                read_len = ::pread64(fd_, p_tmp, left, read_offset);
                //读到的长度可能会失败
                if (read_len < 0){
                    read_len = -errno;
                    if (-read_len == EINTR || -read_len == EAGAIN){//EINTR:临时中断的标志；EAGAIN:继续尝试的标志
                        continue;
                    }else if(EBADF == -read_len){//EBADF:文件描述符变坏的标志
                        fd_ = -1;
                        return read_len;//负的错误代号
                    }else{
                        return read_len;
                    }
                }else if (read_len == 0){//文件读到尾部
                    break;
                }

                left = left - read_len;
                p_tmp = p_tmp + read_len;//指针移动
                read_offset = read_offset + read_len;//偏移量
            }

            if (0 != left){
                return EXIT_DIST_OPER_INCOMPLETE;
            }

            return TFS_SUCCESS;
        }

        //
        int FileOperation::pwrite_file(const char *buf, const int32_t nbytes, const int64_t offset){
            //要做好分多次写的准备
            int32_t left = nbytes;//left表示剩余的要读字节数
            int64_t write_offset = offset;//起始位置的偏移量
            int32_t written_len = 0;//已写的长度
            const char *p_tmp = buf;//文件在buffer中的地址,保存最新的位置

            int i = 0;

            while (left > 0){
                i++;

                //读取次数达到限制
                if (i >= MAX_DISK_TIMES){
                    break;
                }

                //保证文件打开
                if (check_file() < 0){
                    return -errno;
                }

                written_len = ::pwrite64(fd_, p_tmp, left, write_offset);
                //读到的长度可能会失败
                if (written_len < 0){
                    written_len = -errno;
                    if (-written_len == EINTR || -written_len == EAGAIN){//EINTR:临时中断的标志；EAGAIN:继续尝试的标志
                        continue;
                    }else if(EBADF == -written_len){//EBADF:文件描述符变坏的标志
                        fd_ = -1;
                        return written_len;//负的错误代号
                    }else{
                        return written_len;
                    }
                }

                left = left - written_len;
                p_tmp = p_tmp + written_len;//指针移动
                write_offset = write_offset + written_len;//偏移量
            }

            if (0 != left){
                return EXIT_DIST_OPER_INCOMPLETE;
            }

            return TFS_SUCCESS;
        }

        //直接从当前位置写
        int FileOperation::write_file(const char *buf, const int32_t nbytes){
            //要做好分多次写的准备
            int32_t left = nbytes;//left表示剩余的要读字节数
            //int64_t write_offset = offset;//起始位置的偏移量
            int32_t written_len = 0;//已写的长度
            const char *p_tmp = buf;//文件在buffer中的地址,保存最新的位置

            int i = 0;
            while (left > 0){
                i++;

                //读取次数达到限制
                if (i >= MAX_DISK_TIMES){
                    break;
                }

                //保证文件打开
                if (check_file() < 0){
                    return -errno;
                }
                written_len = ::write(fd_, p_tmp, left);
                //读到的长度可能会失败
                if (written_len < 0){
                    written_len = -errno;
                    if (-written_len == EINTR || -written_len == EAGAIN){//EINTR:临时中断的标志；EAGAIN:继续尝试的标志
                        continue;
                    }else if(EBADF == -written_len){//EBADF:文件描述符变坏的标志
                        fd_ = -1;
                        continue;//再次进行尝试
                    }else{
                        return written_len;
                    }
                }

                left -= written_len;
                p_tmp += written_len;
            }

            if (0 != left){
                return EXIT_DIST_OPER_INCOMPLETE;
            }

            return TFS_SUCCESS;
        }

        //得到文件大小
        int64_t FileOperation::get_file_size(){
            int fd = check_file();//如果文件没打开，这里可以打开
            if (fd < 0){
                return -1;
            }

            struct stat statbuf;
            if (fstat(fd, &statbuf) != 0){
                return -1;
            }
            return statbuf.st_size;
        }

        //如果文件没有打开，用这个函数打开
        int FileOperation::check_file(){
            if (fd_ < 0){
                fd_ = open_file();

                if (fd_ < 0){
                    return -1;
                }
            }

            return fd_;
        }

        //截断文件，把大文件变成小文件或者小文件变成大文件
        int FileOperation::ftruncate_file(const int64_t length){
            int fd = check_file();

            if (fd < 0){
                return fd;
            }

            return ftruncate(fd, length);
        }

        //移动文件中的读取指针
        int FileOperation::seek_file(const int64_t offset){
            int fd = check_file();

            if (fd < 0){
                return fd;
            }

            return lseek(fd, offset, SEEK_SET);
        }

        //把文件立即写到磁盘中去，调用write函数只是写到内存中，但是没有写到磁盘中去
        int FileOperation::flush_file(){
            if (open_flags_ & O_SYNC){//写到磁盘上面，才返还成功；如果写入磁盘之前失败了，就不会返回成功。写成功了，才返回就是以同步的方式打开文件
                return 0;
            }

            int fd = check_file();
            if (fd < 0){
                return fd;
            }

            return fsync(fd);//将缓冲区数据写回磁盘
        }
        //删除文件
        int FileOperation::unlink_file(){
            close_file();
            return ::unlink(file_name_);
        }
    }
}
