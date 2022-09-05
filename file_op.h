/*内存映射实战之文件操作父类头文件操作*/
#ifndef QINIU_LARGE_FILE_OP_H_//防止重复包含
#define QINIU_LARGE_FILE_OP_H_

#include "common.h"

namespace qiniu{
    namespace largefile{//做重复的命名的目的：为了对模块进行更加方便的管理
        class FileOperation{
        public:
            FileOperation(const std::string &file_name, const int open_flags = O_RDWR | O_LARGEFILE);//构造函数
            ~FileOperation();//析构函数

            int open_file();//打开文件需要文件句柄
            void close_file();//关闭文件

            int flush_file();//把文件立即写到磁盘中去，write写到内存中，但是没有写到磁盘中去
            int unlink_file();//删除文件

            virtual int pread_file(char *buf, const int32_t nbytes, const int64_t offset);//精细化从文件里读数据，需要buffer;nbytes是要读的字节数；
                                                                                        //offset:偏移量(用64位的，避免超出32位的表达范围)
            virtual int pwrite_file(const char *buf, const int32_t nbytes, const int64_t offset);//精细化写

            int write_file(const char *buf, const int32_t nbytes);//直接从当前位置写

            int64_t get_file_size();//读取文件大小

            int ftruncate_file(const int64_t length);//改变文件大小

            int seek_file(const int64_t offset);//移动文件中的读取指针

            int get_fd() const{
                return fd_;
            }

        protected:
            int fd_;
            int open_flags_;
            char *file_name_;
        protected:
            int check_file();
        protected:
            static const mode_t OPEN_MODE = 0644;
            static const int MAX_DISK_TIMES = 5;//最大磁盘读取的次数
        };

    }
}

#endif