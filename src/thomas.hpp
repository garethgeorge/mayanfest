#include <memory>
#include <math>

#include "filesystem.hpp"
#include "diskinterface.hpp"

struct SuperBlock {
	const Disk *disk;
    const uint_64_t superblock_size_chunks = 1;
	const uint64_t disk_size_bytes;
    const uint64_t disk_size_chunks;
	const uint64_t disk_chunk_size;

	uint64_t disk_block_map_offset; // chunk in which the disk block map starts
    uint64_t disk_block_map_size_chunks; // number of chunks in disk block map
	std::unique_ptr<DiskBitMap> disk_block_map;

	uint64_t inode_table_offset; // chunk in which the inode table starts
	uint64_t inode_table_size_chunks; // number of chunks in the inode table
	std::unique_ptr<INodeTable> inodetable;

    uint64_t data_offset; //where free chunks begin

	SuperBlock(Disk *disk) 
		: disk(disk), disk_size_bytes(disk->size_bytes()), 
        disk_size_chunks(disk->size_chunks()),
		disk_chunk_size(disk->chunk_size()) {
	}

    void init(double inode_table_size_rel_to_disk) {
		//size of things in chunks
		disk_block_map_size_chunks = Math.Ceil(((double)disk_size_chunks)/(8*disk_chunk_size);
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
        for(uint_64_t bit_i = 0; bit_i < disk_block_map_size_chunks + dbm_size + inot_size; ++bit_i) {
            disk_block_map->set(bit_i);
        }

        //create inode table
        inode_table = std::unique_ptr<INodeTable>(new INodeTable(this));

        //serialize to disk
        if(superblock_size_chunks != 1) {
            throw new FileSystemException("superblock size > 1 chunk not supported!");
        }
        auto sb_chunk = disk->get_chunk(0);
		auto sb_data = sb_chunk->
	}

    void load() {
        
    }
};

struct FileSystem {
	Disk *disk;			
	std::unique_ptr<SuperBlock> superblock;

	// the file system, once constructed, takes ownership of the disk
	FileSystem(Disk *disk) {
		// DOES THE STUFF TO CONSTRUCT THE THINGS
		this->disk = disk;		
		superblock = std::unique_ptr<SuperBlock>(new SuperBlock);
	}

	void init(double inode_table_size_rel_to_disk = 0.1) {
		superblock->init(inode_table_size_rel_to_disk)
	}
}