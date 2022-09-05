#ifndef QINIU_LARGEFILE_INDEX_HANDLE_H_//防止重复包含
#define QINIU_LARGEFILE_INDEX_HANDLE_H_

#include "/home/share/tfs_largefile/mmap/mmap_file/common.h"
#include "/home/share/tfs_largefile/mmap/mmap_file/mmap_file_op.h"



namespace qiniu{
    namespace largefile{
        struct IndexHeader
        {
            //无参构造函数
            IndexHeader(){//IndexHandle
                memset(this, 0, sizeof(IndexHeader));//初始化
            }
            BlockInfo block_info_;//meta block info
            //脏数据状态标记被省略掉了，因为这个是用在整个大的分布式系统的其他模块的，这里用不到
            int32_t bucket_size_;//哈希桶大小(hash bucket size)
            int32_t data_file_offset_;//未使用数据起始的偏移
            int32_t index_file_size_;//索引文件的偏移(offset after:index_header + all buckets),在块索引结构中：块信息、脏数据状态标记、哈希桶大小
            //未使用数据起始的偏移、索引文件当前偏移、可重用的链表节点、存放n个哈希桶首节点偏移量的n个(int32_t)区域；后面就是哈希索引块(MetaInfo)
            int32_t free_head_offset_;//可重用的链表节点

        };

        class IndexHandle{
        public:
            //构造函数
            IndexHandle(const std::string & base_path, const uint32_t main_block_id);
            ~IndexHandle();
            int create (const uint32_t logic_block_id, const int32_t bucket_size, const MMapOption map_option);//逻辑块ID包括主块、扩展块
            int load (const uint32_t logic_block_id, const int32_t bucket_size, const MMapOption map_option);
            int remove(const uint32_t logic_block_id);//remove index file:unmmap and unlink file
            int flush();
            IndexHeader * index_header() {
                return reinterpret_cast<IndexHeader *>(file_op_->get_map_data());
            }

            int update_block_info(const OperType oper_type, const uint32_t modify_size);
            
            BlockInfo * block_info(){
                return reinterpret_cast<BlockInfo *>(file_op_->get_map_data());//因为IndexHeader中第一个写的是BlockInfo，所以IndexHeader的首地址就是BlockInfo的首地址
            }

            int32_t* bucket_slot(){//返回链表的头指针
                return reinterpret_cast<int32_t*>(reinterpret_cast<char*>(file_op_->get_map_data()) + sizeof(IndexHeader));
            }
            int32_t bucket_size() const{
                return reinterpret_cast<IndexHeader *>(file_op_->get_map_data())->bucket_size_;
            }
            int32_t get_block_data_offset() const{
                return reinterpret_cast<IndexHeader *>(file_op_->get_map_data())->data_file_offset_;//之前写的是return data_file_offset_,IndexHandle里没有data_file_offset_
                //这个变量 ,data_file_offset_:未使用数据起始的偏移
            }

            int32_t free_head_offset() const{
                return reinterpret_cast<IndexHeader *>(file_op_->get_map_data())->free_head_offset_;
            }
            void commit_block_data_offset(const int file_size){
                reinterpret_cast<IndexHeader *>(file_op_->get_map_data())->data_file_offset_ += file_size;//追加长度
            }
            int32_t write_segment_meta(const uint64_t key, MetaInfo& meta);
            int32_t read_segment_meta(const uint64_t key, MetaInfo& meta);//如果key存在，就把它的metainfo读到meta中
            int32_t delete_segment_meta(const uint64_t key);
            
            int32_t hash_find(const uint64_t key, int32_t& current_offset, int32_t& previous_offset);//如果key存在，就要读取到它的当前偏移量、上一个key的偏移量

            int32_t hash_insert(const uint64_t key, int32_t previous_offset, MetaInfo& meta);
        private:
            bool hash_compare(const uint64_t left_key, const uint64_t right_key) {
                return (left_key == right_key);
            }

            MMapFileOperation* file_op_;
            bool is_load_;
        };  
        
    }
}
 #endif