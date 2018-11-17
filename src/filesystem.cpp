#include <bitset>
#include <array>
#include <vector>
#include <memory>
#include <cassert>
#include <sstream>

#include "diskinterface.hpp"
#include "filesystem.hpp"

// #define DEBUG

using Size = uint64_t;

const uint64_t INode::INDIRECT_TABLE_SIZES[4] = {DIRECT_ADDRESS_COUNT, INDIRECT_ADDRESS_COUNT, DOUBLE_INDIRECT_ADDRESS_COUNT, TRIPPLE_INDIRECT_ADDRESS_COUNT};

uint64_t INode::read(uint64_t starting_offset, char *buf, uint64_t bytes_to_write) {
	const uint64_t chunk_size = this->superblock->disk_chunk_size;
    int64_t n = bytes_to_write;
    uint64_t bytes_written = bytes_to_write;

    if (starting_offset + bytes_to_write > this->data.file_size) {
        // TODO: test this error case
        if (starting_offset > this->data.file_size) 
            return 0;

        bytes_to_write = this->data.file_size - starting_offset;
    }
    
    // room to write for the first chunk
    const uint64_t room_first_chunk = chunk_size - starting_offset % chunk_size;
    uint64_t bytes_write_first_chunk = room_first_chunk;
    if (n < room_first_chunk) {
        bytes_write_first_chunk = n;
    }

    {
        std::shared_ptr<Chunk> chunk = this->resolve_indirection(starting_offset / chunk_size, false);
        if (chunk == nullptr) {
            std::memset(buf, 0, bytes_write_first_chunk);
        } else {
            std::lock_guard<std::mutex> g(chunk->lock);
            std::memcpy(buf, chunk->data.get() + starting_offset % chunk_size, bytes_write_first_chunk);
        }
        
        buf += bytes_write_first_chunk;
        n -= bytes_write_first_chunk;
    }
    

    if (n == 0) { // early return if we wrote less than a chunk
        return bytes_to_write;
    }

    // fix the starting offset
    starting_offset += bytes_write_first_chunk;
    assert(starting_offset % chunk_size == 0);

    while (n > chunk_size) {
        std::shared_ptr<Chunk> chunk = this->resolve_indirection(starting_offset / chunk_size, false);
        if (chunk == nullptr) {
            std::memset(buf, 0, chunk_size);
        } else {
            std::lock_guard<std::mutex> g(chunk->lock);
            std::memcpy(buf, chunk->data.get(), chunk_size);
        }

        buf += chunk_size;
        n -= chunk_size;
        starting_offset += chunk_size;
    }
    
    {
        std::shared_ptr<Chunk> chunk = this->resolve_indirection(starting_offset / chunk_size, false);
        if (chunk == nullptr) {
            std::memset(buf, 0, n);
        } else {
            std::lock_guard<std::mutex> g(chunk->lock);
            std::memcpy(buf, chunk->data.get(), n);
        }
    }

    return bytes_written;
}

uint64_t INode::write(uint64_t starting_offset, const char *buf, uint64_t bytes_to_write) {
    const uint64_t chunk_size = this->superblock->disk_chunk_size;
    int64_t n = bytes_to_write;
    uint64_t bytes_written = bytes_to_write;
    
    // std::cout << "WRITING BYTES: starting offset " << starting_offset << " count " << bytes_to_write << std::endl;

    if (starting_offset + bytes_to_write > this->data.file_size) {
        this->data.file_size = starting_offset + bytes_to_write;
    }

    // room to write for the first chunk
    const uint64_t room_first_chunk = chunk_size - starting_offset % chunk_size;
    uint64_t bytes_write_first_chunk = room_first_chunk;
    if (n < room_first_chunk) {
        bytes_write_first_chunk = n;
    }

    {
        std::shared_ptr<Chunk> chunk = this->resolve_indirection(starting_offset / chunk_size, true);
        std::lock_guard<std::mutex> g(chunk->lock);
        std::memcpy(chunk->data.get() + (starting_offset % chunk_size), buf, bytes_write_first_chunk);
        buf += bytes_write_first_chunk;
        n -= bytes_write_first_chunk;
    }
    
    if (n == 0) { // early return if we wrote less than a chunk
        return bytes_to_write;
    }

    // fix the starting offset
    starting_offset += bytes_write_first_chunk;
    assert(starting_offset % chunk_size == 0);

    while (n > chunk_size) {
        std::shared_ptr<Chunk> chunk = this->resolve_indirection(starting_offset / chunk_size, true);
        std::lock_guard<std::mutex> g(chunk->lock);
        std::memcpy(chunk->data.get(), buf, chunk_size);
        buf += chunk_size;
        n -= chunk_size;
        starting_offset += chunk_size;
    }
    
    {
        std::shared_ptr<Chunk> chunk = this->resolve_indirection(starting_offset / chunk_size, true);
        std::lock_guard<std::mutex> g(chunk->lock);
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
            // TODO: implement locking on this chunk, this will be HARD HARD HARD because of all the places
            // the reference to the chunk is changed

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

std::string INode::to_string() {
    std::stringstream out;
    out << "INODE... " << std::endl;
    for(int i = 0; i < ADDRESS_COUNT; i++) {
        out << i << ": " << data.addresses[i] << std::endl;
    }
    out << "END INODE" << std::endl;
    return out.str();
}

INodeTable::INodeTable(SuperBlock *superblock, uint64_t offset, uint64_t inode_count) : superblock(superblock) {
    this->inode_count = inode_count;
    this->inode_table_offset = offset;
    this->inodes_per_chunk = superblock->disk_chunk_size / sizeof(INode::INodeData);

    // allocate this at the beginning of the table 
    this->used_inodes = std::unique_ptr<DiskBitMap>(
        new DiskBitMap(superblock->disk, this->inode_table_offset, inode_count)
    );
    
    this->inode_ilist_offset = this->inode_table_offset + this->used_inodes->size_chunks();
    
    
    this->inode_table_size_chunks = this->used_inodes->size_chunks() + inode_count / inodes_per_chunk + 1;
}

void INodeTable::format_inode_table() {
    // no inodes are used initially
    this->used_inodes->clear_all();
}

// returns the size of the entire table in chunks
uint64_t INodeTable::size_chunks() {
    return inode_table_size_chunks;
}

INode INodeTable::alloc_inode() {
    std::lock_guard<std::recursive_mutex> g(this->lock);

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
    std::lock_guard<std::recursive_mutex> g(this->lock);

    if (idx >= inode_count) 
        throw FileSystemException("INode index out of bounds");
    if (!used_inodes->get(idx)) 
        throw FileSystemException("INode at index is not currently in use. You can't have it.");

    INode node;
    uint64_t chunk_idx = inode_ilist_offset + idx / inodes_per_chunk;
    uint64_t chunk_offset = idx % inodes_per_chunk;
    std::shared_ptr<Chunk> chunk = superblock->disk->get_chunk(chunk_idx);
    std::memcpy((void *)(&(node.data)), chunk->data.get() + sizeof(INode::INodeData) * chunk_offset, sizeof(INode::INodeData));
    node.superblock = this->superblock;
    node.inode_table_idx = idx;
    return node;
}

void INodeTable::set_inode(uint64_t idx, INode &node) {
    std::lock_guard<std::recursive_mutex> g(this->lock);
    
    if (idx >= inode_count) 
        throw FileSystemException("INode index out of bounds");
    used_inodes->set(idx);

    uint64_t chunk_idx = inode_ilist_offset + idx / inodes_per_chunk;
    uint64_t chunk_offset = idx % inodes_per_chunk;
    std::shared_ptr<Chunk> chunk = superblock->disk->get_chunk(chunk_idx);
    std::memcpy((void *)(chunk->data.get() + sizeof(INode::INodeData) * chunk_offset), (void *)(&(node.data)), sizeof(INode::INodeData));
}

void INodeTable::free_inode(uint64_t idx) {
    std::lock_guard<std::recursive_mutex> g(this->lock);

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
    uint64_t offset = this->superblock_size_chunks; // sspace reserved for the superblock's header

    if (this->disk->size_chunks() < 16 || disk->size_chunks() * (1.0 - inode_table_size_rel_to_disk) < 16) {
        throw new FileSystemException("Requested size of superblock, inode table, and bitmap will potentially exceed disk size");
    }

    // initialize the disk block map
    {
        this->disk_block_map = std::unique_ptr<DiskBitMap>(
            new DiskBitMap(this->disk, offset, disk->size_chunks()));
        this->disk_block_map->clear_all();
        // set the properties on the superblock for the blockmap
        this->disk_block_map_offset = offset;
        this->disk_block_map_size_chunks = this->disk_block_map->size_chunks();
        offset += this->disk_block_map->size_chunks();
    }
    
    // initialize the inode table
    {   
        uint64_t inodes_per_chunk = disk->chunk_size() / sizeof(INode);
        uint64_t inode_count_to_request = (unsigned long)(inode_table_size_rel_to_disk * disk->size_chunks()) * inodes_per_chunk;
        
        this->inode_table_inode_count = inode_count_to_request;
        this->inode_table = std::unique_ptr<INodeTable>(
            new INodeTable(this, offset, inode_count_to_request));
        this->inode_table->format_inode_table();
        this->inode_table_offset = offset;
        this->inode_table_size_chunks = this->inode_table->size_chunks();
        offset += this->inode_table->size_chunks();
    }

    // give ourselves an extra margin of 1 chunk
    offset++;

    //set all metadata chunk bits to `used' a la Thomas
    for(uint64_t bit_i = 0; bit_i < offset; ++bit_i) {
        disk_block_map->set(bit_i);
    }

    this->data_offset = offset;

    //serialize to disk
    {
        auto sb_chunk = disk->get_chunk(0);
        Byte* sb_data = sb_chunk->data.get();
        uint64_t offset = 0;
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
        *(uint64_t *)(sb_data+offset) = inode_table_inode_count;
        offset += sizeof(uint64_t);
        *(uint64_t *)(sb_data+offset) = data_offset;
        disk->flush_chunk(*sb_chunk);
        {
            auto sb_chunk = disk->get_chunk(0);
            Byte* sb_data = sb_chunk->data.get();
            int offset = 0;
            //std::cout << "END OF INIT: " << *(uint64_t *)(sb_data+offset) << std::endl;
        }
    }
}

void SuperBlock::load_from_disk(Disk * disk) {
    // //std::cout << "ENTERING LOAD_FROM_DISK" << std::endl;
    auto sb_chunk = disk->get_chunk(0);
    auto sb_data = sb_chunk->data.get();
    uint64_t offset = 0;
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
    uint64_t disk_block_map_offset = *(uint64_t *)(sb_data+offset);
    offset += sizeof(uint64_t);
    uint64_t disk_block_map_size_chunks = *(uint64_t *)(sb_data+offset);
    offset += sizeof(uint64_t);
    uint64_t inode_table_offset = *(uint64_t *)(sb_data+offset);
    offset += sizeof(uint64_t);
    uint64_t inode_table_size_chunks = *(uint64_t *)(sb_data+offset);
    offset += sizeof(uint64_t);
    uint64_t inode_table_inode_count = *(uint64_t *)(sb_data+offset);
    offset += sizeof(uint64_t);
    data_offset = *(uint64_t *)(sb_data+offset);

    offset = this->superblock_size_chunks;

    // initialize the disk block map
    {
        this->disk_block_map = std::unique_ptr<DiskBitMap>(
            new DiskBitMap(this->disk, offset, disk->size_chunks()));
        // this->disk_block_map->clear_all();
        // set the properties on the superblock for the blockmap
        this->disk_block_map_offset = offset;
        this->disk_block_map_size_chunks = this->disk_block_map->size_chunks();
        offset += this->disk_block_map->size_chunks();

        if (this->disk_block_map_offset != disk_block_map_offset || 
            this->disk_block_map_size_chunks != disk_block_map_size_chunks) {
            throw FileSystemException("The disk blockmap became corrupted when attempting to load it");
        }
    }
    
    // initialize the inode table
    {   
        uint64_t inode_count_to_request = inode_table_inode_count;

        this->inode_table = std::unique_ptr<INodeTable>(
            new INodeTable(this, offset, inode_count_to_request));
        // this->inode_table->format_inode_table();
        this->inode_table_offset = offset;
        this->inode_table_size_chunks = this->inode_table->size_chunks();
        offset += this->inode_table->size_chunks();

        if (this->inode_table_offset != inode_table_offset || 
            this->inode_table_size_chunks != inode_table_size_chunks) {
            throw FileSystemException("The inode table became corrupted when attempting to load it");
        }
    }

    this->data_offset = offset;

    // finally, these two values should add up
    if (this->data_offset != data_offset) {
        throw FileSystemException("found the wrong final data offset after loading the inode table. Something went wrong.");
    }

    // also check that the disk bit map marks every chunk up to the data offset as in use
    for(uint64_t bit_i = 0; bit_i < offset; ++bit_i) {
        if (!disk_block_map->get(bit_i)) {
            throw FileSystemException("disk bit map should hold every bit in superblock marked as 'in use' why is this not the case?");
        }
    }
}

void FileSystem::printForDebug() {
  //TODO: write this function
  throw FileSystemException("thomas you idiot...");
}
