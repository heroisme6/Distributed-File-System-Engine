 #ifndef _COMMON_H_INCLUDED_//防止重复包含
 #define _COMMON_H_INCLUDED_

 #include <iostream>
 #include <fcntl.h>
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <string>
 #include <string.h>
 #include <stdint.h>
 #include <errno.h>
 #include <stdio.h>
 #include <sys/mman.h>
 #include <unistd.h>
 #include <stdlib.h>
 #include <assert.h>


namespace qiniu{
    namespace largefile{
        const int32_t EXIT_DIST_OPER_INCOMPLETE = -8012;//读或者写的长度小于我们要求的长度
        const int32_t TFS_SUCCESS = 0;
        const int32_t TFS_ERROR = -1;
        const int32_t EXIT_INDEX_ALREADY_LOADER_ERROR = -8013;//index is loader when create or load
        const int32_t EXIT_META_UNEXPECT_FOUND_ERROR = -8014;//meta found in index when insert
        const int32_t EXIT_INDEX_CORRUPT_ERROR = -8015;//index is corrupt
        const int32_t EXIT_BLOCKID_CONFLICT_ERROR = -8016;//
        const int32_t EXIT_BUCKET_CONFIGURE_ERROR = -8017;
        const int32_t EXIT_META_NOT_FOUND_ERROR = -8018;
        const int32_t EXIT_BLOCKID_ZERO_ERROR = -8019;

        static const std::string MAINBLOCK_DIR_PREFIX = "/mainblock/";//存储主块文件的
        static const std::string INDEX_DIR_PREFIX = "/index/";//"./"表示当前目录下面，存储索引文件的
        static const mode_t DIR_MODE = 0755;

        //内存映射进行调整：后面追加内存，主要是在后面加内存的时候会用到下面三个值
        struct MMapOption{
            //int32_t不管在32还是64位操作系统，都是32位的
            int32_t max_mmap_size_;
            int32_t first_mmap_size_;
            int32_t per_mmap_size_;
        };

        enum OperType{
            C_OPER_INSERT = 1,
            C_OPER_DELETE
        };

        struct BlockInfo{
        //public:不需要写public，struct中默认是public:，class中默认是private:
            uint32_t block_id_;//块id
            int32_t version_;//版本号
            int32_t file_count_;//文件数量
            int32_t size_;//当前已保存文件数据总大小
            int32_t del_file_count_;//已删除文件数量,是块文件整理的一个指标
            int32_t del_size_;//已删除文件数据总大小
            uint32_t seq_no_;//下一个可分配的文件编号1.......(2^64-1)

            //构造函数，使用之前先清0
            BlockInfo(){
                memset(this, 0, sizeof(BlockInfo));//把所有的成员变量都清0
            }

            inline bool operator==(const BlockInfo& rhs) const{//重载==号
                return block_id_ == rhs.block_id_ && version_ == rhs.version_ && file_count_ == rhs.file_count_ && 
                size_ == rhs.size_ && del_file_count_ == rhs.del_file_count_ && del_size_ == rhs.del_size_ 
                && seq_no_ == rhs.seq_no_;
            }
        };

        struct MetaInfo{
            //无参构造函数
            MetaInfo(){
                init();
            }
            //有参构造函数
            MetaInfo(const uint64_t file_id, const int32_t in_offset, const int32_t file_size, const int32_t next_meta_offset){
                fileid_ = file_id;
                location_.inner_offset_ = in_offset;
                location_.size_ = file_size;
                next_meta_offset_ = next_meta_offset;
            }
            //拷贝构造函数
            MetaInfo(const MetaInfo& meta_info){
                memcpy(this, &meta_info, sizeof(MetaInfo));
            }
            //重载==
            bool operator==(const MetaInfo& rhs) const{
                return fileid_ == rhs.fileid_ && location_.inner_offset_ == rhs.location_.inner_offset_ && location_.size_ == rhs.location_.size_ &&
                next_meta_offset_ == rhs.next_meta_offset_;
            }

            //获得key值
            uint64_t get_key() const{
                return fileid_;
            }
            //修改key值
            void set_key(const uint64_t key){
                fileid_ = key;
            }
            //获得文件id
            uint64_t get_file_id(){
                return fileid_;
            }
            //修改文件id
            void set_file_id(const uint64_t file_id){
                fileid_ = file_id;
            }

            //获得文件在块内部的偏移量
            int32_t get_offset() const{
                return location_.inner_offset_;
            }
            //修改文件在块内部的偏移量
            void set_offset(const int32_t offset){
                location_.inner_offset_ = offset;
            }

            //获得文件大小
            int32_t get_size() const{
                return location_.size_;
            }
            //修改文件大小
            void set_size(const int32_t file_size){
                location_.size_ = file_size;
            }

            //获得当前哈希链下一个节点在索引文件中的偏移量
            int32_t get_next_meta_offset() const{
                return next_meta_offset_;
            }
            //修改当前哈希链下一个节点在索引文件中的偏移量
            void set_next_meta_offset(const int32_t offset){
                next_meta_offset_ = offset;
            }

            //重载复制运算符，防止出现浅拷贝的问题
            MetaInfo& operator=(const MetaInfo& meta_info){
                if (this == &meta_info){
                    return *this;
                }

                fileid_ = meta_info.fileid_;
                location_.inner_offset_ = meta_info.location_.inner_offset_;
                location_.size_ = meta_info.location_.size_;
                next_meta_offset_ = meta_info.next_meta_offset_;
            }

            //克隆函数
            MetaInfo& clone(const MetaInfo& meta_info){
                assert(this != &meta_info);//断言

                fileid_ = meta_info.fileid_;
                location_.inner_offset_ = meta_info.location_.inner_offset_;
                location_.size_ = meta_info.location_.size_;
                next_meta_offset_ = meta_info.next_meta_offset_;
                return *this;
            }
            
            uint64_t fileid_;//文件编号
            struct{
                int32_t inner_offset_;//文件在块内部的偏移量
                int32_t size_;//文件大小
            }location_;//文件元数据（使用匿名结构体来定义结构体变量，这样的方式虽然简单，但不能再次定义新的结构体变量了）
            int32_t next_meta_offset_;//当前哈希链下一个节点在索引文件中的偏移量
        private:
            void init(){
                fileid_ = 0;
                location_.inner_offset_ = 0;
                location_.size_ = 0;
                next_meta_offset_ = 0;

            }
        };
     }
 }

 #endif /*_COMMON_H_INCLUDED_*/