#include <bitset>
#include <array>
#include <vector>
#include <memory>

#include "diskinterface.hpp"
#include "filesystem.hpp"


using Size = uint64_t;

uint64_t INode::read(uint64_t starting_offset, char *buf, uint64_t n) {
    uint64_t chunk_number = starting_offset / superblock->disk_chunk_size;
    uint64_t byte_offset = starting_offset % superblock->disk_chunk_size;
    //split into function
    if(chunk_number < 8){ //access direct blocks
        
    }else if(chunk_number < (8 + (superblock->disk_chunk_size / sizeof(uint64_t)))){//change to var
        
    }
    throw FileSystemException("Not Implemented");														
}

INodeTable::INodeTable(SuperBlock *superblock) : superblock(superblock) {
    inode_table_size_chunks = superblock->inode_table_size_chunks;
    inode_table_offset = superblock->inode_table_offset;
    inodes_per_chunk = superblock->disk_chunk_size / sizeof(INode);

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
    std::memcpy((void *)(&node), chunk->data.get() + sizeof(INode) * chunk_offset, sizeof(INode));
    return node;
}

void INodeTable::set_inode(uint64_t idx, INode &node) {
    if (idx >= inode_count) 
        throw FileSystemException("INode index out of bounds");
    used_inodes->set(idx);

    INode node;
    uint64_t chunk_idx = idx / inodes_per_chunk;
    uint64_t chunk_offset = idx % inodes_per_chunk;
    std::shared_ptr<Chunk> chunk = superblock->disk->get_chunk(chunk_idx);
    std::memcpy((void *)(chunk->data.get() + sizeof(INode) * chunk_offset), (void *)(&node), sizeof(INode));
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
    //size of things in chunks
    disk_block_map_size_chunks = std::ceil(((double)disk_size_chunks)/(8*disk_chunk_size);
    inode_table_size_chunks = inode_table_size_rel_to_disk * disk_size_chunks;
    
    //check that metadata isn't too big
    if(disk_block_map_size_chunks + inode_table_size_chunks + disk_block_map_size_chunks >= disk_size_chunks) {
        throw new FileSystemException("Requested size of superblock, inode table, and bit map exceeds size of disk");
    }

    //write offsets of structures to superblock
    disk_block_map_offset = superblock_size_chunks;
    inode_table_offset = superblock_size_chunks + disk_block_map_size_chunks;
    data_offset = superblock_size_chunks + disk_block_map_size_chunks + inode_table_size_chunks;

    //create the disk block map
    disk_block_map = std::unique_ptr<DiskBitMap>(new DiskBitMap(disk, disk_block_map_offset, disk_block_map_size_chunks));
    disk_block_map->clear_all();

    //set all metadata chunk bits to `used'
    for(uint64_t bit_i = 0; bit_i < disk_block_map_size_chunks + disk_block_map_size_chunks + inode_table_size_chunks; ++bit_i) {
        disk_block_map->set(bit_i);
    }

    //create inode table
    inode_table = std::unique_ptr<INodeTable>(new INodeTable(this));
    inode_table->format_inode_table();

    //serialize to disk
    if(superblock_size_chunks != 1) {
        throw new FileSystemException("superblock size > 1 chunk not supported!");
    }
    auto sb_chunk = disk->get_chunk(0);
    auto sb_data = sb_chunk->data->get();
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
    auto sb_data = sb_chunk->data->get();
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
    
    disk_block_map = std::unique_ptr<DiskBitMap>(new DiskBitMap(disk, disk_block_map_offset, disk_block_map_size_chunks));
    inode_table = std::unique_ptr<INodeTable>(new INodeTable(this));
}

