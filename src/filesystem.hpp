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
	static constexpr uint64_t DIRECT_ADDRESS_COUNT = 8;
	static constexpr uint64_t INDIRECT_ADDRESS_COUNT = 1;
	static constexpr uint64_t DOUBLE_INDIRECT_ADDRESS_COUNT = 1;
	static constexpr uint64_t ADDRESS_COUNT = DIRECT_ADDRESS_COUNT + INDIRECT_ADDRESS_COUNT + DOUBLE_INDIRECT_ADDRESS_COUNT;
	static uint64_t INDIRECT_TABLE_SIZES[] = {DIRECT_ADDRESS_COUNT, INDIRECT_ADDRESS_COUNT, DOUBLE_INDIRECT_ADDRESS_COUNT};
	
	uint64_t UID = 0; //user id
	uint64_t last_modified = 0; //last modified timestamp
	uint64_t file_size = 0; //size of file
	uint64_t reference_count = 0; //reference count to the inode
	uint64_t addresses[ADDRESS_COUNT] = {0}; //8 direct
	uint64_t direct_addresses[DIRECT_ADDRESS_COUNT] = {0}; //8 direct
	uint64_t indirect_addresses[INDIRECT_ADDRESS_COUNT] = {0}; //1 indirect
	uint64_t double_indirect_addresses[DOUBLE_INDIRECT_ADDRESS_COUNT] = {0}; //1 indirect
	std::bitset<11> inode_bits(0LL); //rwxrwxrwx (ow, g, oth) dir special 

	SuperBlock *superblock;	

	std::shared_ptr<Chunk> resolve_indirection(uint64_t chunk_number){
		const uint64_t num_chunk_address_per_chunk = superblock->disk_chunk_size / sizeof(uint64_t);
		uint64_t indirect_address_count = 1;
	
		uint64_t *indirect_table = addresses;
		for(uint64_t indirection = 0; indirection < sizeof(INDIRECT_TABLE_SIZES) / sizeof(uint64_t); indirection++){
			if(chunk_number < (indirect_address_count * INDIRECT_TABLE_SIZES[indirection])){
				uint64_t next_chunk_loc = indirect_table[chunk_number / indirect_address_count];
				if(next_chunk_loc == 0){
					return nullptr;
				}
				auto chunk = superblock->disk->get_chunk(next_chunk_loc);
				while(indirection != 0){
					uint64_t *lookup_table = chunk->data->get();
					next_chunk_loc = lookup_table[chunk_number % indirect_address_count];
					if(next_chunk_loc == 0){
						return nullptr;
					}
					indirect_address_count /= num_chunk_address_per_chunk;
					chunk = superblock->disk->get_chunk(next_chunk_loc);
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

	/**
	std::shared_ptr<Chunk> get_chunk(uint64_t chunk_number){
		const uint64_t num_chunk_address_per_chunk = superblock->disk_chunk_size / sizeof(uint64_t);
		if(chunk_number < DIRECT_ADDRESS_COUNT){ //access direct blocks
			if(direct_addresses[chunk_number] == 0){
				throw FileSystemException("INode attempt to access chunk that does not exist");
			}
			return superblock->disk->get_chunk(direct_address[chunk_number]);
		}else if(chunk_number < (DIRECT_ADDRESS_COUNT + (num_chunk_address_per_chunk * INDIRECT_ADDRESS_COUNT))){ //access indirect block
			chunk_number -= DIRECT_ADDRESS_COUNT;
			uint64_t indirect_block_number = chunk_number / num_chunk_address_per_chunk;
			if(indirect_addresses[indirect_block_number] == 0){
				throw FileSystemException("INode attempt to access chunk that does not exist");
			}
			auto indirect_block = superblock->disk->get_chunk(indirect_address[indirect_block_number]);
			uint64_t *indirect_addresses = (uint64_t*)indirect_block->data->get();
			if(indirect_addresses[chunk_number % num_chunk_address_per_chunk] == 0){
				throw FileSystemException("INode attempt to access chunk that does not exist");
			}
		}else{ //access double indirect access
			if(direct_addresses[chunk_number] == 0){
				throw FileSystemException("INode attempt to access chunk that does not exist");
			}
			superblock->disk->get_chunk(direct_address[chunk_number]);
		}
	}
	**/

	uint64_t read(uint64_t starting_offset, char *buf, uint64_t n){
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

