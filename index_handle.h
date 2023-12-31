#ifndef LARGE_FILE_INDEX_HANDLE_H
#define LARGE_FILE_INDEX_HANDLE_H

#include "common.h"
#include "mmap_file_op.h"

static int debug = 1;
/*
 * 		IndexHandler由「IndexHeader + MetaInfo哈希链表」两个部分组成。IndexHandler通过参数block_id和"."组成path，
 * 然后配合mmap_file_op，可以对索引文件（IndexHeader+MetaInfo）进行创建、加载、移除、保存至磁盘的操作。
 * 		为了实现上述操作：
 * 			1. 该类对象会提供操作MetaInfo的接口：write_segment_meta、read_segment_meta、delete_segment_meta。
 * 			2. 同时，为了使用哈希链表数据结构保存MetaInfo，提供接口：hash_find，hash_insert。
 * 			3. 提供修改block_info的接口update_block_info。提供修改IndexHeader中data offset信息的接口commit_block_data_offset
*/
namespace qiniu {
	namespace largefile {
		struct IndexHeader
		{
			IndexHeader() {
				memset(this, 0, sizeof(IndexHeader));
			}

			BlockInfo block_info_;		//meta的块信息, 28B
			int32_t bucket_size_;		//哈希桶大小, 4B
			int32_t data_file_offset_;	//主块文件中的未使用数据的起始偏移, 4B
			int32_t index_file_size_;	//索引文件的大小，同时也是偏移量(offset after index_header+all buckets),指向下一个待分配的索引文件位置, 4B
			int32_t free_head_offset_;	//可重用的链表节点（即之前删掉的节点，他们并未真的被删除，只是被标记了）,4B
		};

		class IndexHandler {
		public:
			IndexHandler(const std::string& base_path, const uint32_t main_block_id);
			~IndexHandler();
			int create(const uint32_t logic_block_id, const int32_t bucket_size, const MMapOption map_option);
			int load(const uint32_t logic_block_id, const int32_t bucket_size, const MMapOption map_option);
			int remove(const uint32_t logic_block_id);
			int flush();
			int32_t write_segment_meta(const uint64_t key, MetaInfo& meta_info);
			int32_t read_segment_meta(const uint64_t key, MetaInfo& meta_info);
			int32_t delete_segment_meta(const uint64_t key);
			int hash_find(const uint64_t key, int32_t& current_offset, int32_t& previous_offset);
			int hash_insert(const uint64_t key, int32_t previous_offset, MetaInfo& meta);
			void commit_block_data_offset(const int file_size) {
				reinterpret_cast<IndexHeader*>(mmap_file_op_->get_map_data())->data_file_offset_ += file_size;
			}
			int update_block_info(const OperationType oper_type, const int32_t file_size);

			//mmap_file_op_对象map了之后，mmap_file_op_里的data就是索引文件，而索引文件以IndexHeader开头，所以可以强行转换
			IndexHeader* index_header() {
				return reinterpret_cast<IndexHeader*>(mmap_file_op_->get_map_data());
			}
			//mmap_file_op_对象map了之后，mmap_file_op_里的data就是索引文件，而索引文件以IndexHeader开头，IndexHeader又以BlockInfo开头，所以可以强行转换
			BlockInfo* block_info() {
				return reinterpret_cast<BlockInfo*>(mmap_file_op_->get_map_data());
			}
			//返回哈希桶的大小
			int32_t get_bucket_size() const {
				return reinterpret_cast<IndexHeader*>(mmap_file_op_->get_map_data())->bucket_size_;
			}
			//返回主块文件中的未使用数据的起始偏移
			int32_t get_data_file_offset() const {
				return reinterpret_cast<IndexHeader*>(mmap_file_op_->get_map_data())->data_file_offset_;
			}

			//找到第一个meta_info节点的地址，之后通过[]操作符来找到具体的哈希桶
			int32_t* get_first_meta_info() const {
				//先转换成char类型指针，方便通过字节来移动（1个char占1个字节Byte），最后转换成int32_t类型指针
				return reinterpret_cast<int32_t*>(reinterpret_cast<char*>(mmap_file_op_->get_map_data()) + sizeof(IndexHeader));
			}

			bool hash_compare(const uint64_t left_key, const uint64_t right_key) {
				return left_key == right_key;
			}
		private:
			MMapFileOperation* mmap_file_op_;
			bool is_loaded;
		};
	}
}

#endif // !LARGE_FILE_INDEX_HANDLE_H
