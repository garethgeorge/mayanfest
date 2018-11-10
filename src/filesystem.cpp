#include <bitset>
#include <array>
#include <vector>
#include <memory>
#include <cassert>

#include "diskinterface.hpp"
#include "filesystem.hpp"

// #define DEBUG


using Size = uint64_t;

const uint64_t INode::INDIRECT_TABLE_SIZES[4] = {DIRECT_ADDRESS_COUNT, INDIRECT_ADDRESS_COUNT, DOUBLE_INDIRECT_ADDRESS_COUNT, TRIPPLE_INDIRECT_ADDRESS_COUNT};

uint64_t INode::read(uint64_t starting_offset, char *buf, uint64_t bytes_to_write) {
	const uint64_t chunk_size = this->superblock->disk_chunk_size;
    int64_t n = bytes_to_write;
    uint64_t bytes_written = bytes_to_write;
    
    // room to write for the first chunk
    const uint64_t room_first_chunk = chunk_size - starting_offset % chunk_size;
    uint64_t bytes_write_first_chunk = room_first_chunk;
    if (n < room_first_chunk) {
        bytes_write_first_chunk = n;
    }

    std::shared_ptr<Chunk> chunk = this->resolve_indirection(starting_offset / chunk_size, false);
    std::memcpy(buf, chunk->data.get() + starting_offset % chunk_size, bytes_write_first_chunk);
    buf += bytes_write_first_chunk;
    n -= bytes_write_first_chunk;

    if (n == 0) { // early return if we wrote less than a chunk
        return bytes_to_write;
    }

    // fix the starting offset
    starting_offset += bytes_write_first_chunk;
    assert(starting_offset % chunk_size == 0);

    while (n > chunk_size) {
        std::shared_ptr<Chunk> chunk = this->resolve_indirection(starting_offset / chunk_size, false);
        std::memcpy(buf, chunk->data.get(), chunk_size);

        buf += chunk_size;
        n -= chunk_size;
        starting_offset += chunk_size;
    }
    
    {
        std::shared_ptr<Chunk> chunk = this->resolve_indirection(starting_offset / chunk_size, false);
        std::memcpy(buf, chunk->data.get(), n);
    }

    return bytes_written;
}

uint64_t INode::write(uint64_t starting_offset, const char *buf, uint64_t bytes_to_write) {
    const uint64_t chunk_size = this->superblock->disk_chunk_size;
    int64_t n = bytes_to_write;
    uint64_t bytes_written = bytes_to_write;
    
    // room to write for the first chunk
    const uint64_t room_first_chunk = chunk_size - starting_offset % chunk_size;
    uint64_t bytes_write_first_chunk = room_first_chunk;
    if (n < room_first_chunk) {
        bytes_write_first_chunk = n;
    }

    std::shared_ptr<Chunk> chunk = this->resolve_indirection(starting_offset / chunk_size, true);
    std::memcpy(chunk->data.get() + starting_offset % chunk_size, buf, bytes_write_first_chunk);
    buf += bytes_write_first_chunk;
    n -= bytes_write_first_chunk;

    if (n == 0) { // early return if we wrote less than a chunk
        return bytes_to_write;
    }

    // fix the starting offset
    starting_offset += bytes_write_first_chunk;
    assert(starting_offset % chunk_size == 0);

    while (n > chunk_size) {
        std::shared_ptr<Chunk> chunk = this->resolve_indirection(starting_offset / chunk_size, true);
        std::memcpy(chunk->data.get(), buf, chunk_size);

        buf += chunk_size;
        n -= chunk_size;
        starting_offset += chunk_size;
    }
    
    {
        std::shared_ptr<Chunk> chunk = this->resolve_indirection(starting_offset / chunk_size, true);
        std::memcpy(chunk->data.get(), buf, n);
    }

    return bytes_written;
}

std::shared_ptr<Chunk> INode::resolve_indirection(uint64_t chunk_number, bool createIfNotExists) {
    const uint64_t num_chunk_address_per_chunk = superblock->disk_chunk_size / sizeof(uint64_t);
    uint64_t indirect_address_count = 1;

#ifdef DEBUG
    fprintf(stdout, "INode::resolve_indirection for chunk_number %llu\n", chunk_number);
#endif 

    uint64_t *indirect_table = data.addresses;
    for(uint64_t indirection = 0; indirection < sizeof(INDIRECT_TABLE_SIZES) / sizeof(uint64_t); indirection++){
#ifdef DEBUG 
        fprintf(stdout, 
            "INode::resolve_indirection looking for chunk_number %llu"
            " at indirect table level %llu\n", chunk_number, indirection);
#endif 
        
        // TODO: there is something WRONG here sadface cries.

        if(chunk_number < (indirect_address_count * INDIRECT_TABLE_SIZES[indirection])){
            size_t indirect_table_idx = chunk_number / indirect_address_count;
            // chunk_number / indirect_address_count + INDIRECT_TABLE_SIZES[indirection];
            uint64_t next_chunk_loc = indirect_table[indirect_table_idx];
#ifdef DEBUG 
            fprintf(stdout, "Determined that the chunk is in fact located in the table at level %llu\n", indirection);
            fprintf(stdout, "Looked up the indirection table at index %llu and found chunk id %llu\n"
                            "\tside note: indirect address count at this level is %llu\n", 
                    indirect_table_idx,
                    next_chunk_loc,
                    indirect_address_count
                );
#endif
            if (next_chunk_loc == 0){
                if (!createIfNotExists) {
                    return nullptr;
                }

                std::shared_ptr<Chunk> newChunk = this->superblock->allocate_chunk();
#ifdef DEBUG 
                fprintf(stdout, "next_chunk_loc was 0, so we created new "
                    "chunk id %zu/%llu and placed it in the table\n", 
                    newChunk->chunk_idx, 
                    this->superblock->disk->size_chunks());
#endif
                std::memset((void *)newChunk->data.get(), 0, newChunk->size_bytes);
                indirect_table[indirect_table_idx] = newChunk->chunk_idx;
                next_chunk_loc = newChunk->chunk_idx;
                
#ifdef DEBUG
                fprintf(stdout, "the real next_chunk_loc is %llu\n", next_chunk_loc);
#endif
            }

#ifdef DEBUG
            fprintf(stdout, "chasing chunk through the indirection table:\n");
#endif 
            std::shared_ptr<Chunk> chunk = superblock->disk->get_chunk(next_chunk_loc);
            while (indirection != 0){
                indirect_address_count /= num_chunk_address_per_chunk;
#ifdef DEBUG 
                fprintf(stdout, "\tcurrent indirect level is: %llu, indirect block id is: %llu\n", indirection, chunk->chunk_idx);
#endif 
                uint64_t *lookup_table = (uint64_t *)chunk->data.get();
                next_chunk_loc = lookup_table[chunk_number / indirect_address_count];

#ifdef DEBUG 
                fprintf(stdout, "\tfound next_chunk_loc %llu in table at index %llu\n"
                    "\t\tside note: indirect address count is %llu\n", 
                    next_chunk_loc, 
                    chunk_number / indirect_address_count,
                    indirect_address_count);
#endif

                if (next_chunk_loc == 0) {
                    if (!createIfNotExists) {
                        return nullptr;
                    }

                    std::shared_ptr<Chunk> newChunk = this->superblock->allocate_chunk();
                    std::memset((void *)newChunk->data.get(), 0, newChunk->size_bytes);
                    next_chunk_loc = newChunk->chunk_idx;
                    lookup_table[chunk_number / indirect_address_count] = newChunk->chunk_idx;
#ifdef DEBUG 
                    fprintf(stdout, "\tnext_chunk_loc was 0, so we created new "
                        "chunk id %zu/%llu and placed it in the table\n", 
                        newChunk->chunk_idx, this->superblock->disk->size_chunks());
#endif 
                }

                chunk = superblock->disk->get_chunk(next_chunk_loc);
                chunk_number %= indirect_address_count;

                indirection--;
            }

#ifdef DEBUG 
            fprintf(stdout, "found chunk with id %zu, parent disk %llx\n", chunk->chunk_idx, (unsigned long long)chunk->parent);
#endif 

            return chunk;
        }
        chunk_number -= (indirect_address_count * INDIRECT_TABLE_SIZES[indirection]);
        indirect_table += INDIRECT_TABLE_SIZES[indirection];
        indirect_address_count *= num_chunk_address_per_chunk;
    }
    return nullptr;
}

INodeTable::INodeTable(SuperBlock *superblock, uint64_t offset, uint64_t size_chunks) : superblock(superblock) {
    inode_table_offset = offset;
    inodes_per_chunk = superblock->disk_chunk_size / sizeof(INode::INodeData);

    inode_table_size_chunks = size_chunks;
    inode_count = inodes_per_chunk * size_chunks;
    used_inodes = std::unique_ptr<DiskBitMap>(
        new DiskBitMap(superblock->disk, inode_table_offset, inode_count)
    );
}

void INodeTable::format_inode_table() {
    // no inodes are used initially
    this->used_inodes->clear_all();
}

// returns the size of the entire table in chunks
uint64_t INodeTable::size_chunks() {
    return used_inodes->size_chunks() + inode_table_size_chunks;
}

INode INodeTable::alloc_inode() {
    std::lock_guard<std::mutex> g(this->lock);

    DiskBitMap::BitRange range = this->used_inodes->find_unset_bits(1);
    if (range.bit_count != 1) {
      throw FileSystemException("INodeTable out of inodes -- no free inode available for allocation");
    }

    INode inode;
    inode.superblock = this->superblock;
    inode.inode_table_idx = range.start_idx;
    this->set_inode(range.start_idx, inode);
    
    return inode;
}

INode INodeTable::get_inode(uint64_t idx) {
    std::lock_guard<std::mutex> g(this->lock);

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
    node.inode_table_idx = idx;
    return node;
}

void INodeTable::set_inode(uint64_t idx, INode &node) {
    std::lock_guard<std::mutex> g(this->lock);
    
    if (idx >= inode_count) 
        throw FileSystemException("INode index out of bounds");
    used_inodes->set(idx);

    uint64_t chunk_idx = idx / inodes_per_chunk;
    uint64_t chunk_offset = idx % inodes_per_chunk;
    std::shared_ptr<Chunk> chunk = superblock->disk->get_chunk(chunk_idx);
    std::memcpy((void *)(chunk->data.get() + sizeof(INode::INodeData) * chunk_offset), (void *)(&(node.data)), sizeof(INode::INodeData));
}

void INodeTable::free_inode(uint64_t idx) {
    std::lock_guard<std::mutex> g(this->lock);

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

    std::cout << "requested disk block map size in chunks is " << disk->size_chunks() << std::endl;

    //block map init
    std::cout << "BEGIN INITIALIZE BLOCK MAP" << std::endl;
    disk_block_map_offset = superblock_size_chunks; 
    disk_block_map = std::unique_ptr<DiskBitMap>(new DiskBitMap(disk, disk_block_map_offset, disk->size_chunks()));
    disk_block_map->clear_all();
    std::cout << "FINISHED INITIALIZE BLOCK MAP" << std::endl;
    
    //block map size in chunks
    disk_block_map_size_chunks = disk_block_map->size_chunks();

    //check that metadata isn't too big
    if(superblock_size_chunks + inode_table_size_chunks + disk_block_map_size_chunks >= disk_size_chunks) {
        std::cout << "meta data exceeds disk size..." << std::endl;
	std::cout << "block map size in chunks = " << disk_block_map_size_chunks << std::endl;
	std::cout << "inode table size in chunks = " << inode_table_size_chunks << std::endl;
	std::cout << "superblock size chunks = " << superblock_size_chunks << std::endl;
	std::cout << "disk size chunks = " << disk_size_chunks << std::endl;
        throw new FileSystemException("Requested size of superblock, inode table, and bit map exceeds size of disk");
    }

    //create inode table
    //use the actual size of the block map made for the next offset
    inode_table_offset = superblock_size_chunks + disk_block_map->size_chunks();
    std::cout << "Inode table offset is " << inode_table_offset << std::endl;
    inode_table = std::unique_ptr<INodeTable>(new INodeTable(this, inode_table_offset, inode_table_size_chunks));
    inode_table->format_inode_table();

    std::cout << "inode table size in chunks is " << inode_table->size_chunks() << std::endl;

    //free space
    data_offset = superblock_size_chunks + disk_block_map->size_chunks() + inode_table->size_chunks();

    //set all metadata chunk bits to `used'
    for(uint64_t bit_i = 0; bit_i < superblock_size_chunks + disk_block_map_size_chunks + inode_table_size_chunks; ++bit_i) {
        disk_block_map->set(bit_i);
    }

    //serialize to disk
    //TODO: support this if needed
    if(superblock_size_chunks != 1) {
        throw new FileSystemException("superblock size > 1 chunk not supported!");
    }
    auto sb_chunk = disk->get_chunk(0);
    Byte* sb_data = sb_chunk->data.get();
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
    std::cout << "ENTERING LOAD_FROM_DISK" << std::endl;
    auto sb_chunk = disk->get_chunk(0);
    auto sb_data = sb_chunk->data.get();
    int offset = 0;
    //superblock_size_chunks = *(uint64_t *)(sb_data + offset);
    //TODO: throw an error code that filesystem was corrupted instead /////////////////////////////////////////////
    if(superblock_size_chunks != *(uint64_t *)(sb_data + offset)) {
      throw new FileSystemException("Stored superblock size corrupted!");
    }
    offset += sizeof(uint64_t);
    //disk_size_bytes = *(uint64_t *)(sb_data+offset);
    if(disk_size_bytes != *(uint64_t *)(sb_data + offset)) {
      throw new FileSystemException("Stored disk size in byte corrupted!");
    }
    offset += sizeof(uint64_t);
    //disk_size_chunks = *(uint64_t *)(sb_data+offset);
    if(disk_size_chunks != *(uint64_t *)(sb_data + offset)) {
      throw new FileSystemException("Stored disk size in chunks corrupted!");
    }
    offset += sizeof(uint64_t);
    //disk_chunk_size = *(uint64_t *)(sb_data+offset);
    if(disk_chunk_size != *(uint64_t *)(sb_data + offset)) {
      throw new FileSystemException("Stored disk chunk size corrupted!");
    }    
    offset += sizeof(uint64_t);
    disk_block_map_offset = *(uint64_t *)(sb_data+offset);
    std::cout << "\tdisk block map offset = " << disk_block_map_offset << std::endl;
    offset += sizeof(uint64_t);
    disk_block_map_size_chunks = *(uint64_t *)(sb_data+offset);
    std::cout << "\tdisk block map size in chunks = " << disk_block_map_size_chunks << std::endl;
    offset += sizeof(uint64_t);
    inode_table_offset = *(uint64_t *)(sb_data+offset);
    std::cout << "\tinode table offset = " << inode_table_offset << std::endl;
    offset += sizeof(uint64_t);
    inode_table_size_chunks = *(uint64_t *)(sb_data+offset);
    std::cout << "\tinode table size in chunks = " << inode_table_size_chunks << std::endl;
    offset += sizeof(uint64_t);
    data_offset = *(uint64_t *)(sb_data+offset);
    std::cout << "\tdata offset = " << data_offset << std::endl;
    
    std::cout << "disk size chunks = " << disk_size_chunks << std::endl;

    disk_block_map = std::unique_ptr<DiskBitMap>(new DiskBitMap(disk, disk_block_map_offset, disk_size_chunks));
    inode_table = std::unique_ptr<INodeTable>(new INodeTable(this, inode_table_offset, inode_table_size_chunks));

    std::cout << "EXITING LOAD_FROM_DISK" << std::endl;
}

void FileSystem::printForDebug() {
  //TODO: write this function
}
