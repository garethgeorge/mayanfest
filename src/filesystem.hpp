#ifndef FILESYSTEM_HPP
#define FILESYSTEM_HPP

#include <bitset>
#include <array>
#include <vector>
#include <memory>

#include "diskinterface.hpp"

using Size = uint64_t;

struct SuperBlock;

struct FileSystemException : public std::exception {
	std::string message;
	FileSystemException(const std::string &message) : message(message) { };
};

struct INode {
	uint64_t UID = 0; //user id
	uint64_t last_modified = 0; //last modified timestamp
	uint64_t file_size = 0; //size of file
	uint64_t reference_count = 0; //reference count to the inode
	uint64_t direct_addresses[8] = {0}; //8 direct
	uint64_t indirect_addresses[1] = {0}; //1 indirect
	uint64_t double_indirect_addresses[1] = {0}; //1 indirect
	//TODO: might want to make sure this gets zeroed
	std::bitset<11> inode_bits(); //rwxrwxrwx (ow, g, oth) dir special 

	SuperBlock *superblock;	

	uint64_t read(uint64_t starting_offset, char *buf, uint64_t n);

};

struct INodeTable {
	SuperBlock *superblock;
	uint64_t inode_table_size_chunks = 0; // size of the inode table including used_inodes bitmap + ilist 
	uint64_t inode_table_offset = 0; // this actually winds up being the offset of the used_inodes bitmap
	uint64_t inode_ilist_offset = 0; // this ends up storing the calculated real offset of the inodes
	uint64_t inode_count = 0;
	uint64_t inodes_per_chunk = 0;

	SharedObjectCache<uint64_t, INode> inodecache;
	std::unique_ptr<DiskBitMap> used_inodes;
	// struct INode ilist[10]; //TODO: change the size

	INodeTable(SuperBlock *superblock);

	void format_inode_table();

	// returns the size of the entire table in chunks
	uint64_t size_chunks();
	
	INode get_inode(uint64_t idx);
	
	void set_inode(uint64_t idx, INode &node);

	void free_inode(uint64_t idx);
};

struct SuperBlock {
	Disk *disk;
    const uint64_t superblock_size_chunks = 1;
	const uint64_t disk_size_bytes;
    const uint64_t disk_size_chunks;
	const uint64_t disk_chunk_size;

	uint64_t disk_block_map_offset; // chunk in which the disk block map starts
    uint64_t disk_block_map_size_chunks; // number of chunks in disk block map
	std::unique_ptr<DiskBitMap> disk_block_map;

	uint64_t inode_table_offset; // chunk in which the inode table starts
	uint64_t inode_table_size_chunks; // number of chunks in the inode table
	std::unique_ptr<INodeTable> inode_table;

    uint64_t data_offset; //where free chunks begin

	SuperBlock(Disk *disk);

    void init(double inode_table_size_rel_to_disk);
    void load_from_disk(Disk * disk);
};

struct FileSystem {
	Disk *disk;			
	std::unique_ptr<SuperBlock> superblock;

	// the file system, once constructed, takes ownership of the disk
	FileSystem(Disk *disk) {
		// DOES THE STUFF TO CONSTRUCT THE THINGS
		this->disk = disk;		
		superblock = std::unique_ptr<SuperBlock>(new SuperBlock(disk));
	}

	void init(double inode_table_size_rel_to_disk = 0.1) {
		superblock->init(inode_table_size_rel_to_disk);
	}
};

#endif