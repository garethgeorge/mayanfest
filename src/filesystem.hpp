#ifndef FILESYSTEM_HPP
#define FILESYSTEM_HPP

#include <bitset>
#include <array>
#include <vector>

#include "diskinterface.hpp"

using Size = uint64_t;

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
	std::bitset<11> inode_bits(0LL); //rwxrwxrwx (ow, g, oth) dir special 

	SuperBlock *superblock;	

	uint64_t read(uint64_t starting_offset, char *buf, uint64_t n){
		uint64_t chunk_number = starting_offset / superblock->chunk_size;
		uint64_t byte_offset = starting_offset % superblock->chunk_size;
		//split into function
		if(chunk_number < 8){ //access direct blocks
			
		}else if(chunk_number < (8 + (superblock->chunk_size / sizeof(uint64_t)))){//change to var
			
		}																
	}

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

	INodeTable(SuperBlock *superblock) : superblock(superblock) {
		inode_table_size_chunks = superblock->inode_table_size_chunks;
		inode_table_offset = superblock->inode_table_offset;
		inodes_per_chunk = superblock->chunk_size / sizeof(INode);

		used_inodes = std::unique_ptr<DiskBitMap>(
			new DiskBitMap(superblock->disk, inode_table_offset, inode_count)
		);

		inode_count = inodes_per_chunk * inode_table_size_chunks - used_inodes->size_chunks();
	}

	void format_inode_table() {
		// no inodes are used initially
		used_inodes->clear_all();
	}

	// returns the size of the entire table in chunks
	uint64_t size_chunks() {
		return used_inodes->size_chunks() + inode_count;
	}
	
	INode get_inode(uint64_t idx) {
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
	
	void set_inode(uint64_t idx, INode &node) {
		if (idx >= inode_count) 
			throw FileSystemException("INode index out of bounds");
		used_inodes->set(idx);

		INode node;
		uint64_t chunk_idx = idx / inodes_per_chunk;
		uint64_t chunk_offset = idx % inodes_per_chunk;
		std::shared_ptr<Chunk> chunk = superblock->disk->get_chunk(chunk_idx);
		std::memcpy((void *)(chunk->data.get() + sizeof(INode) * chunk_offset), (void *)(&node), sizeof(INode));
	}

	void free_inode(uint64_t idx) {
		if (idx >= inode_count) 
			throw FileSystemException("INode index out of bounds");
		used_inodes->clr(idx);
	}
};

struct SuperBlock {
	const Disk *disk;

	const uint64_t size_bytes;
	const uint64_t chunk_size;

	uint64_t disk_block_map_offset; // chunk in which the disk block map starts
	std::unique_ptr<DiskBitMap> disk_block_map;

	uint64_t inode_table_offset; // chunk in which the inode table starts
	uint64_t inode_table_size_chunks;
	std::unique_ptr<INodeTable> inodetable;

	SuperBlock(Disk *disk) 
		: disk(disk), size_bytes(disk->size_bytes()), 
		chunk_size(disk->chunk_size()) {
	}

	void load_disk_block_map(uint64_t disk_block_map_offset) {
		// TODO: determine how big the disk_block_map should be, I would argue large enough to store
		// one bit for every chunk on the disk
		disk_block_map = std::unique_ptr<DiskBitMap>(new DiskBitMap(disk, disk_block_map_offset, ))
	}
};

