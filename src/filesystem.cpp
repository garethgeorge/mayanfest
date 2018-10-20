#include <bitset>
#include <array>
#include <vector>
#include <memory>

#include "diskinterface.hpp"
#include "filesystem.hpp"


using Size = uint64_t;

const uint64_t INode::INDIRECT_TABLE_SIZES[4] = {DIRECT_ADDRESS_COUNT, INDIRECT_ADDRESS_COUNT, DOUBLE_INDIRECT_ADDRESS_COUNT, TRIPPLE_INDIRECT_ADDRESS_COUNT};

uint64_t INode::read(uint64_t starting_offset, char *buf, uint64_t n) const {
    /*
		uint64_t chunk_number; //index of chunk
		uint64_t byte_offset; //index of byte within that chunk
		uint64_t starting_offset_chunk; //starting byte offset of chunk
		uint64_t ending_offset_chunk; //ending byte offset of chunk
		uint64_t num_chunk_address_per_chunk = superblock->chunk_size / sizeof(uint64_t);
		uint64_t num_bytes_till_end_of_chunk; //including byte_offset
		uint64_t num_of_chunks_to_access; //number of chunks to access
		uint64_t i; //loop counter
		uint64_t bytes_to_read;
		std::shared_ptr<Chunk> chunk;

		//setup info for first chunk
		chunk_number = starting_offset / superblock->chunk_size;
		byte_offset = starting_offset % superblock->chunk_size;
		starting_offset_chunk = chunk_number * superblock->chunk_size;
		ending_offset_chunk = starting_offset_chunk + superblock->chunk_size - 1;
		num_bytes_till_end_of_chunk = superblock->chunk_size - byte_offset;

		//find number of chunks to access
		num_of_chunks_to_access = 1;
		if(n > num_bytes_till_end_of_chunk){
			n_temp = n - num_bytes_till_end_of_chunk;
			num_of_chunks_to_access += (n_temp / superblock->chunk_size);
		}

		for(i = 0; i < num_of_chunks_to_access; i++){
			if(n <= num_bytes_till_end_of_chunk){
				bytes_to_read = n;
			}else{
				bytes_to_read = num_bytes_till_end_of_chunk;
			}
			n -= bytes_to_read;

			//split into function
			chunk = disk->get_chunk(chunk_number);
			std::memcpy((void*)chunk->get(), buf, bytes_to_read);
			chunk_number += 1;
			byte_offset = 0;
			starting_offset_chunk = chunk_number * superblock->chunk_size;
			ending_offset_chunk = starting_offset_chunk + superblock->chunk_size - 1;
			num_bytes_till_end_of_chunk = superblock->chunk_size - byte_offset;
		}
		*/
		return 0;
}

std::shared_ptr<Chunk> INode::resolve_indirection(uint64_t chunk_number) {
    const uint64_t num_chunk_address_per_chunk = superblock->disk_chunk_size / sizeof(uint64_t);
    uint64_t indirect_address_count = 1;

    uint64_t *indirect_table = data.addresses;
    for(uint64_t indirection = 0; indirection < sizeof(INDIRECT_TABLE_SIZES) / sizeof(uint64_t); indirection++){
        if(chunk_number < (indirect_address_count * INDIRECT_TABLE_SIZES[indirection])){
            uint64_t next_chunk_loc = indirect_table[chunk_number / indirect_address_count];
            if(next_chunk_loc == 0){
                std::shared_ptr<Chunk> chunk = this->superblock->allocate_chunk();
                std::memset((void *)chunk->data.get(), 0, chunk->size_bytes);
                indirect_table[chunk_number / indirect_address_count] = chunk->chunk_idx;
            }
            auto chunk = superblock->disk->get_chunk(next_chunk_loc);
            while(indirection != 0){
                uint64_t *lookup_table = (uint64_t *)chunk->data.get();
                next_chunk_loc = lookup_table[chunk_number / indirect_address_count];
                if(next_chunk_loc == 0) {
                    std::shared_ptr<Chunk> chunk = this->superblock->allocate_chunk();
                    std::memset((void *)chunk->data.get(), 0, chunk->size_bytes);
                    lookup_table[chunk_number / indirect_address_count] = chunk->chunk_idx;
                } else {
                    chunk = superblock->disk->get_chunk(next_chunk_loc);
                }
                indirect_address_count /= num_chunk_address_per_chunk;
                indirection--;
            }
            return chunk;
        }
        chunk_number -= (indirect_address_count * INDIRECT_TABLE_SIZES[indirection]);
        indirect_table += INDIRECT_TABLE_SIZES[indirection];
        indirect_address_count *= num_chunk_address_per_chunk;
    }
    return nullptr;
}

INodeTable::INodeTable(SuperBlock *superblock, uint64_t offset, uint64_t size) : superblock(superblock) {
    inode_table_size_chunks = size;
    inode_table_offset = offset;
    inodes_per_chunk = superblock->disk_chunk_size / sizeof(INode::INodeData);

    used_inodes = std::unique_ptr<DiskBitMap>(
        new DiskBitMap(superblock->disk, inode_table_offset, inode_count)
    );

    inode_count = inodes_per_chunk * inode_table_size_chunks - used_inodes->size_chunks();
}

void INodeTable::format_inode_table() {
    // no inodes are used initially
    used_inodes->clear_all();
}

// returns the size of the entire table in chunks
uint64_t INodeTable::size_chunks() {
    return used_inodes->size_chunks() + inode_count;
}

INode INodeTable::get_inode(uint64_t idx) {
    if (idx >= inode_count) 
        throw FileSystemException("INode index out of bounds");
    if (!used_inodes->get(idx)) 
        throw FileSystemException("INode at index is not currently in use. You can't have it.");

    INode node;
    uint64_t chunk_idx = idx / inodes_per_chunk;
    uint64_t chunk_offset = idx % inodes_per_chunk;
    std::shared_ptr<Chunk> chunk = superblock->disk->get_chunk(chunk_idx);
    std::memcpy((void *)(&(node.data)), chunk->data.get() + sizeof(INode::INodeData) * chunk_offset, sizeof(INode::INodeData));
    node.superblock = this->superblock;
    return node;
}

void INodeTable::set_inode(uint64_t idx, INode &node) {
    if (idx >= inode_count) 
        throw FileSystemException("INode index out of bounds");
    used_inodes->set(idx);

    uint64_t chunk_idx = idx / inodes_per_chunk;
    uint64_t chunk_offset = idx % inodes_per_chunk;
    std::shared_ptr<Chunk> chunk = superblock->disk->get_chunk(chunk_idx);
    std::memcpy((void *)(chunk->data.get() + sizeof(INode::INodeData) * chunk_offset), (void *)(&(node.data)), sizeof(INode::INodeData));
}

void INodeTable::free_inode(uint64_t idx) {
    if (idx >= inode_count) 
        throw FileSystemException("INode index out of bounds");
    used_inodes->clr(idx);
}

SuperBlock::SuperBlock(Disk *disk) 
    : disk(disk), disk_size_bytes(disk->size_bytes()), 
    disk_size_chunks(disk->size_chunks()),
    disk_chunk_size(disk->chunk_size()) {
}

void SuperBlock::init(double inode_table_size_rel_to_disk) {
    //requested size of things in chunks
    inode_table_size_chunks = inode_table_size_rel_to_disk * disk_size_chunks;
    
    std::cout << "Offset " << inode_table_offset << std::endl;

    std::cout << "disk block map size in chunks is " << disk_block_map_size_chunks << std::endl;
    std::cout << "inode table size in chunks is " << inode_table_size_chunks << std::endl;

    //block map init
    std::cout << "BEGIN INITIALIZE BLOCK MAP" << std::endl;
    disk_block_map_offset = superblock_size_chunks; 
    disk_block_map = std::unique_ptr<DiskBitMap>(new DiskBitMap(disk, disk_block_map_offset, disk->size_chunks()));
    disk_block_map->clear_all();
    disk_block_map_size_chunks = disk_block_map->size_chunks();
    std::cout << "FINISHED INITIALIZE BLOCK MAP" << std::endl;
    //get actual size of the block map made

    //check that metadata isn't too big
    if(disk_block_map_size_chunks + inode_table_size_chunks + disk_block_map_size_chunks >= disk_size_chunks) {
        throw new FileSystemException("Requested size of superblock, inode table, and bit map exceeds size of disk");
    }

    //create inode table
    inode_table_offset = superblock_size_chunks + disk_block_map_size_chunks;
    inode_table = std::unique_ptr<INodeTable>(new INodeTable(this, inode_table_offset, inode_table_size_chunks));
    inode_table->format_inode_table();
    inode_table_size_chunks = inode_table->size_chunks();

    //free space
    data_offset = superblock_size_chunks + disk_block_map_size_chunks + inode_table_size_chunks;

    //set all metadata chunk bits to `used'
    for(uint64_t bit_i = 0; bit_i < disk_block_map_size_chunks + disk_block_map_size_chunks + inode_table_size_chunks; ++bit_i) {
        disk_block_map->set(bit_i);
    }

    std::cout << "What's new guys?" << std::endl;

    

    //serialize to disk
    if(superblock_size_chunks != 1) {
        throw new FileSystemException("superblock size > 1 chunk not supported!");
    }
    auto sb_chunk = disk->get_chunk(0);
    auto sb_data = sb_chunk->data.get();
    int offset = 0;
    *(uint64_t *)(sb_data+offset) = superblock_size_chunks;
    offset += sizeof(uint64_t);
    *(uint64_t *)(sb_data+offset) = disk_size_bytes;
    offset += sizeof(uint64_t);
    *(uint64_t *)(sb_data+offset) = disk_size_chunks;
    offset += sizeof(uint64_t);
    *(uint64_t *)(sb_data+offset) = disk_chunk_size;
    offset += sizeof(uint64_t);
    *(uint64_t *)(sb_data+offset) = disk_block_map_offset;
    offset += sizeof(uint64_t);
    *(uint64_t *)(sb_data+offset) = disk_block_map_size_chunks;
    offset += sizeof(uint64_t);
    *(uint64_t *)(sb_data+offset) = inode_table_offset;
    offset += sizeof(uint64_t);
    *(uint64_t *)(sb_data+offset) = inode_table_size_chunks;
    offset += sizeof(uint64_t);
    *(uint64_t *)(sb_data+offset) = data_offset;
}

void SuperBlock::load_from_disk(Disk * disk) {
    auto sb_chunk = disk->get_chunk(0);
    auto sb_data = sb_chunk->data.get();
    int offset = 0;
    //superblock_size_chunks = *(uint64_t *)(sb_data + offset);
    offset += sizeof(uint64_t);
    //disk_size_bytes = *(uint64_t *)(sb_data+offset);
    offset += sizeof(uint64_t);
    //disk_size_chunks = *(uint64_t *)(sb_data+offset);
    offset += sizeof(uint64_t);
    //disk_chunk_size = *(uint64_t *)(sb_data+offset);
    offset += sizeof(uint64_t);
    disk_block_map_offset = *(uint64_t *)(sb_data+offset);
    offset += sizeof(uint64_t);
    disk_block_map_size_chunks = *(uint64_t *)(sb_data+offset);
    offset += sizeof(uint64_t);
    inode_table_offset = *(uint64_t *)(sb_data+offset);
    offset += sizeof(uint64_t);
    inode_table_size_chunks = *(uint64_t *)(sb_data+offset);
    offset += sizeof(uint64_t);
    data_offset = *(uint64_t *)(sb_data+offset);
    
    //TODO: Deserialize these properly
    //disk_block_map = std::unique_ptr<DiskBitMap>(new DiskBitMap(disk, disk_block_map_offset, disk_block_map_size_chunks));
    //inode_table = std::unique_ptr<INodeTable>(new INodeTable(this, ));
}

