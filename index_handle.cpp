#include "/home/share/tfs_largefile/mmap/mmap_file/common.h"
#include "/home/share/tfs_largefile/mmap/mmap_file/index_handle.h"
#include <sstream>

static int debug = 1;
namespace qiniu{
    namespace largefile{
        //构造函数
        IndexHandle::IndexHandle(const std::string & base_path, const uint32_t main_block_id){
            //创建 文件处理 的对象
            //1.确定路径
            std::stringstream tmp_stream;//使用字符串的流,生成路径
            tmp_stream<<base_path<<INDEX_DIR_PREFIX<<main_block_id;// /root/martin/index/1  /root/martin是base_path, 
                                                                            //index是索引文件的前缀，1是索引文件名
            std::string index_path;
            tmp_stream>>index_path;//将字符串流输给index_path
            file_op_ = new MMapFileOperation(index_path, O_CREAT | O_RDWR | O_LARGEFILE);
            is_load_ = false;
        }

        //析构函数
        IndexHandle::~IndexHandle(){
            if (file_op_){
                delete file_op_;
                file_op_ = NULL;
            }
        }

        /*淘宝文件系统上线之初肯定是没有数据的，所以需要先进行块初始化，之后才能进行写文件，读文件*/
        //1.创建索引文件
        int IndexHandle::create (const uint32_t logic_block_id, const int32_t bucket_size, const MMapOption map_option){//逻辑块ID包括主块、扩展块

            int ret = TFS_SUCCESS;
            //刚开始，一个文件都没有,验证有没有文件
            if (debug){
                printf("create index,block id:%u  ,bucket size:%d,max_mmap_size:%d, first_mmap_size:%d,per_mmap_size:%d\n",
                        logic_block_id, bucket_size, map_option.max_mmap_size_, map_option.first_mmap_size_, map_option.per_mmap_size_);
            }

            //如果已经加载进去了，不需要创建
            if (is_load_){
                return EXIT_INDEX_ALREADY_LOADER_ERROR;
            }

            //判断文件存在不存在，调用file_op.h中的get_file_size_()函数，MMapFileOperation是FileOperation的子类
            int64_t file_size = file_op_->get_file_size();
            if ( file_size < 0 ){
                return TFS_ERROR;
            }else if (file_size == 0){//empty file，
                IndexHeader i_header;
                i_header.block_info_.block_id_ = logic_block_id;//块id
                i_header.block_info_.seq_no_ = 1;//下一个可分配的文件编号
                i_header.bucket_size_ = bucket_size;//哈希桶大小(hash bucket size)
                i_header.index_file_size_ = sizeof(IndexHeader) + bucket_size * sizeof(int32_t);//索引文件当前的偏移或者索引文件当前的大小
                //用整型数存储第一个节点的位置(4个字节)，比如说有10个桶，索引文件的大小需要再加上10 * 4个字节
                //index header + total buckets
                char * init_data = new char[i_header.index_file_size_];

                memcpy(init_data, &i_header, sizeof(IndexHeader));
                memset(init_data + sizeof(IndexHeader), 0, i_header.index_file_size_ - sizeof(IndexHeader));
                //write index header and buckets into index file
                ret = file_op_->pwrite_file(init_data, i_header.index_file_size_, 0);//把数据写到内存中去

                delete[] init_data;//释放内存
                init_data = NULL;
                if (ret != TFS_SUCCESS)
                    return ret;

                ret = file_op_->flush_file();//把内存的数据写到磁盘中去
                if ( ret != TFS_SUCCESS ) 
                    return ret;
            }else{//如果file_size > 0,说明索引文件已经存在了    
                return EXIT_META_UNEXPECT_FOUND_ERROR;
            }

            ret = file_op_->mmap_file(map_option);//将文件映射到内存中去
            if ( ret != TFS_SUCCESS )
                return ret;
            
            is_load_ = true;//映射成功了
            if (debug) printf("init blockid:%d index successful.data file size:%d, index file size:%d, bucket_size:%d,free head offset:%d,seqno:%d,size:%d,filecount:%d,del_size:%d,del_file_size:%d,version:%d\n",
                                logic_block_id, index_header()->data_file_offset_, index_header()->index_file_size_, 
                                index_header()->bucket_size_, index_header()->free_head_offset_, block_info()->seq_no_,
                                block_info()->size_, block_info()->file_count_, block_info()->del_size_, block_info()->del_file_count_,
                                block_info()->version_);//文件被映射到内存后，就可以不用i_header，而可以用index_header()
            return TFS_SUCCESS;
        }

        //2.创建了块之后，就不需要create()函数了，需要用到load()函数
        int IndexHandle::load (const uint32_t logic_block_id, const int32_t _bucket_size, const MMapOption map_option){
            int ret = TFS_SUCCESS;
            if (is_load_){//如果已经加载了
                return EXIT_INDEX_ALREADY_LOADER_ERROR;
            }

            int64_t file_size = file_op_->get_file_size();
            //处理异常情况
            if (file_size < 0){
                return file_size;
            }else if (file_size == 0){//空文件
                return EXIT_INDEX_CORRUPT_ERROR;
            }

            //处理file_size > 0的情况
            MMapOption tmp_map_option = map_option;
            if (file_size > tmp_map_option.first_mmap_size_ && file_size < tmp_map_option.max_mmap_size_)//如果文件大于第一次映射指定的大小
            {
                tmp_map_option.first_mmap_size_ == file_size;
            }
            //如果文件大小比tmp_map_option.max_mmap_size_还大，只能去文件里面去读了
            ret = file_op_->mmap_file(tmp_map_option);//将文件映射到内存
            if (ret != TFS_SUCCESS){
                return ret;
            }

            if (0 == bucket_size() || 0 == block_info()->block_id_){
                fprintf(stderr, "Index corrupt error.block_id:%u,block_size:%d\n", block_info()->block_id_, bucket_size());
                return EXIT_INDEX_CORRUPT_ERROR;
            }

            //检查文件大小check file size
            int32_t index_file_size = sizeof(IndexHeader) + bucket_size() * sizeof(int32_t);
            if (index_file_size > file_size){
                fprintf(stderr, "Index corrupt error.block_id:%u,block_size:%d,file_size:%ld,index file size:%d\n", block_info()->block_id_, bucket_size(), 
                        file_size, index_file_size);
                return EXIT_INDEX_CORRUPT_ERROR;
            }

            //check block id
            if (logic_block_id != block_info()->block_id_){
                fprintf(stderr, "block id conflict.blockid:%u,index blockid:%u\n", logic_block_id, block_info()->block_id_);
                return EXIT_BLOCKID_CONFLICT_ERROR;
            }

            //check bucket size
            if (_bucket_size != bucket_size()){
                fprintf(stderr, "index configure error.old bucket size:%d,new bucket size:%d\n", bucket_size(), _bucket_size);
                return EXIT_BLOCKID_CONFLICT_ERROR;
            }
            is_load_ = true;
            if ( debug ) printf("load blockid:%d index successful.data file size:%d, index file size:%d, bucket_size:%d,free head offset:%d,seqno:%d,size:%d,filecount:%d,del_size:%d,del_file_file_size:%d,version:%d\n",
                                logic_block_id, index_header()->data_file_offset_, index_header()->index_file_size_, 
                                index_header()->bucket_size_, index_header()->free_head_offset_, block_info()->seq_no_,
                                block_info()->size_, block_info()->file_count_, block_info()->del_size_, block_info()->del_file_count_,
                                block_info()->version_);//文件被映射到内存后，就可以不用i_header，而可以用index_header()
            return TFS_SUCCESS;
        }
        //remove index file:unmmap and unlink file
        int IndexHandle::remove (const uint32_t logic_block_id){
            if (is_load_){
                if (logic_block_id != block_info()->block_id_){
                    fprintf(stderr, "block id conflict.blockid:%d,index blockid:%d\n",logic_block_id, block_info()->block_id_);
                    return EXIT_BLOCKID_CONFLICT_ERROR;
                }
            }

            int ret = file_op_->munmap_file();//解除映射
            if (ret != TFS_SUCCESS){
                return ret;
            }

            //如果成功解除映射，接下来删除文件
            ret = file_op_->unlink_file();//从磁盘上删除文件
            return ret;
        }

        int IndexHandle::flush(){
            int ret = file_op_->flush_file();
            if (ret != TFS_SUCCESS){
                fprintf(stderr,"index flush fail,ret:%d error desc:%s\n",ret, strerror(errno));
            }
            return ret;
        } 

        //
        int IndexHandle::update_block_info(const OperType oper_type, const uint32_t modify_size){
            if (block_info()->block_id_ == 0){//如果块编号为0，出错
                return EXIT_BLOCKID_ZERO_ERROR;
            }

            if (oper_type == C_OPER_INSERT){
                block_info()->version_++;//版本号加1
                block_info()->file_count_++;//文件数量加1
                block_info()->seq_no_++;//下一个可分配的文件编号加1
                block_info()->size_ += modify_size;//增加当前已保存文件数据总大小
            }else if(oper_type == C_OPER_DELETE){
                block_info()->version_++;//版本号加1
                block_info()->file_count_--;//文件数量减1
                block_info()->size_ -= modify_size;//增加当前已保存文件数据总大小
                block_info()->del_file_count_++;//已删除的文件数量++
                block_info()->del_size_ += modify_size;//已删除的文件大小
            }

            if (debug){
                printf("update block info.blockid:%u, version:%u, file count:%u, size:%u, del file count:%u, del size:%u, seq no:%u, oper type:%d\n",
                block_info()->block_id_, block_info()->version_, block_info()->file_count_, block_info()->size_, block_info()->del_file_count_, block_info()->del_size_,
                block_info()->seq_no_, oper_type);
            }
            return TFS_SUCCESS;
        }

        int IndexHandle::write_segment_meta(const uint64_t key, MetaInfo& meta){
            int32_t current_offset = 0, previous_offset = 0;//当前偏移和上一个偏移
            //思考：查找key是否存在?只有当不存在时才能成功的写进去
            //1.从文件哈希表中查找key是否存在 hash_find(key, current_offset, previous_offset)
            int ret = hash_find(key, current_offset, previous_offset);
            if (TFS_SUCCESS == ret){//如果在哈希链表找到了key,说明已经存在了记录，如果再写，就把原来的覆盖掉了
                return EXIT_META_UNEXPECT_FOUND_ERROR;
            }else if(EXIT_META_NOT_FOUND_ERROR != ret){//如果出现了读文件异常的情况
                return ret;
            }
            //2.不存在就将meta写入到文件哈希表中 hash_insert(key, meta, previous_offset)
            ret = hash_insert(key, previous_offset, meta);
            return ret;
        }

        int32_t IndexHandle::read_segment_meta(const uint64_t key, MetaInfo& meta){
            int32_t current_offset = 0;
            int32_t previous_offset = 0;
            //1.确定key存放的桶(slot)的位置
            //int32_t slot = static_cast<uint32_t>(key) % bucket_size();//思考：上面的key是uint64_t,这里的slot是int32_t,会不会造成溢出？只要key的值不大于int32_t的最大值，就不会出现问题
            int32_t ret = hash_find(key, current_offset, previous_offset);//hash_find()中已经有了上一行的函数
            if (TFS_SUCCESS == ret){//如果查找到了key
                ret = file_op_->pread_file(reinterpret_cast<char*>(&meta), sizeof(MetaInfo), current_offset);
                return ret;
            }else{//如果没找到。就直接返回ret
                return ret;
            }

        }

        int32_t IndexHandle::delete_segment_meta(const uint64_t key){
            int32_t current_offset = 0;
            int32_t previous_offset = 0;
            //1.确定key存放的桶(slot)的位置
            int32_t ret = hash_find(key, current_offset, previous_offset);//hash_find()中已经有了上一行的函数
            if (ret != TFS_SUCCESS){
                return ret;
            }

            MetaInfo meta_info;
            ret = file_op_->pread_file(reinterpret_cast<char *>(&meta_info), sizeof(MetaInfo), current_offset);
            if (ret != TFS_SUCCESS){
                return ret;
            }

            int32_t next_pos = meta_info.get_next_meta_offset();
            //跳过删除节点，将删除节点的上一个节点连接到删除节点的下一个节点
            //删除节点的上一个节点有两种情况，情况1：上一个节点是头节点；情况2：上一个节点不是头节点
            if (previous_offset == 0){
                int32_t slot = static_cast<uint32_t>(key) % bucket_size();
                bucket_slot()[slot]  = next_pos;
            }else{
                MetaInfo pre_meta_info;
                ret = file_op_->pread_file(reinterpret_cast<char *>(&pre_meta_info), sizeof(MetaInfo), previous_offset);
                if (ret != TFS_SUCCESS){
                    return ret;
                }
                pre_meta_info.set_next_meta_offset(next_pos);

                ret = file_op_->pwrite_file(reinterpret_cast<const char *>(&pre_meta_info), sizeof(MetaInfo), previous_offset);//写回到内存中去
                if (ret != TFS_SUCCESS){
                    return ret;
                }
            }

            //把被删除的节点加入到可重用链表节点（下一个小节实现）
            meta_info.set_next_meta_offset(free_head_offset());//free_head_offset() = index_header()->free_head_offset_,将被删除节点的下一个节点指向可重用链表的第一个节点
            ret = file_op_->pwrite_file(reinterpret_cast<const char *>(&meta_info), sizeof(MetaInfo), current_offset);
            if (TFS_SUCCESS != ret){
                return ret;
            }
            index_header()->free_head_offset_ = current_offset;//index_header()中的free_head_offset_指向被删除的节点
            if (debug) printf("delete_srgment_meta-reuse metainfo, current_offset:%d\n",current_offset);

            update_block_info( C_OPER_DELETE, meta_info.get_size() );//删除索引就行了，主块中的文件不需要删除，因为从磁盘上删除文件需要将后面的文件向前移动，耗费时间太长
            //所以实际开发中，主块上的文件都是在晚上服务器空闲的时候删除.老师把这里的代码写到了previous_offset != 0范围中
            return TFS_SUCCESS;
        }

        int IndexHandle::hash_find(const uint64_t key, int32_t& current_offset, int32_t& previous_offset){
            int ret = TFS_SUCCESS;
            MetaInfo meta_info;
            current_offset = 0;
            previous_offset = 0;

            //1.确定key存放的桶(slot)的位置
            int32_t slot = static_cast<uint32_t>(key) % bucket_size();//思考：上面的key是uint64_t,这里的slot是int32_t,会不会造成溢出？只要key的值不大于int32_t的最大值，就不会出现问题
            //2.读取桶首节点的存储的第一个节点的偏移量，如果偏移量为0，直接返回(key不存在)EXIT_META_NOT_FOUND_ERROR
            int32_t pos = bucket_slot()[slot];//得到key在文件哈希索引块中的位置(是偏移量而不是地址)
            
            
            //从metainfo中取得下一个节点的、在文件中的偏移量，如果偏移量为0，直接返回EXIT_META_NOT_FOUND_ERROR，否则，跳转至3继续循环执行
            for (;pos != 0;){
                //3.根据偏移量读取存储的metainfo
                ret = file_op_->pread_file(reinterpret_cast<char*>(&meta_info), sizeof(MetaInfo), pos);
                if (TFS_SUCCESS != ret){
                    return ret;
                }

                //4.与key进行比较，相等就则设置current_offset和previous_offset，并返回TFS_SUCCESEE,否则继续执行5
                if ( hash_compare( key, meta_info.get_key() ) ) {
                    current_offset = pos;
                    return TFS_SUCCESS;
                }

                //5.从metainfo中取得下一个节点的、在文件中的偏移量，如果偏移量为0，直接返回EXIT_META_NOT_FOUND_ERROR，否则，跳转至3继续循环执行
                previous_offset = pos;
                pos = meta_info.get_next_meta_offset();
            }
            return EXIT_META_NOT_FOUND_ERROR;
        }

        int32_t IndexHandle::hash_insert(const uint64_t key, int32_t previous_offset, MetaInfo& meta) {
            int ret = TFS_SUCCESS;
            MetaInfo tmp_meta_info;
            //1.求出key要放到哪个桶
            int32_t slot = static_cast<uint32_t>(key) % bucket_size();

            //2.确定metaInfo节点存储在文件中的偏移量
            int current_offset = 0;
            if ( free_head_offset() != 0 ){//如果有可重用节点
                ret = file_op_->pread_file(reinterpret_cast<char *>(&tmp_meta_info), sizeof(MetaInfo), free_head_offset());
                if (TFS_SUCCESS != ret){
                    return ret;
                }

                current_offset = free_head_offset();//free_head_offset() = index_header()->free_head_offset_
                if (debug){
                    printf("reuse metainfo, current offset:%d\n", current_offset);
                }
                index_header()->free_head_offset_ = tmp_meta_info.get_next_meta_offset();
            }
            else{//如果没有可重用节点
                current_offset = index_header()->index_file_size_;
                index_header()->index_file_size_ += sizeof(MetaInfo);
            }

            //3.将metaInfo写到索引文件中
            meta.set_next_meta_offset(0);//为什么要进行这一步？因为这个MetaInfo是哈希链表的最后一个节点
            ret = file_op_->pwrite_file( reinterpret_cast<const char *>(&meta), sizeof(MetaInfo), current_offset);//写到了内存中
            if (TFS_SUCCESS != ret){
                index_header()->index_file_size_ -= sizeof(MetaInfo);
                return ret;
            }

            //4.将meta节点插入到哈希链表中

            //当前一个节点已经存在
            if (0 != previous_offset){
                ret = file_op_->pread_file(reinterpret_cast<char *>(&tmp_meta_info), sizeof(MetaInfo), previous_offset);//从内存中读取数据,偏移量为previous_offset
                //大小为sizeof(MetaInfo)
                if (TFS_SUCCESS != ret){
                    index_header()->index_file_size_ -= sizeof(MetaInfo);
                    return ret;
                }

                tmp_meta_info.set_next_meta_offset(current_offset);//将上一个节点的、下一个节点在索引文件中的偏移量设置为current_offset
                ret = file_op_->pwrite_file( reinterpret_cast<const char *>(&tmp_meta_info), sizeof(MetaInfo), previous_offset);//写回内存中去
                if (TFS_SUCCESS != ret){
                    index_header()->index_file_size_ -= sizeof(MetaInfo);
                    return ret;
                }
            }else{//如果没有前一个节点
                bucket_slot()[slot] = current_offset;
            }

            return TFS_SUCCESS;
        }
    }
}