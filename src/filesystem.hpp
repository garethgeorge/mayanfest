#ifndef FILESYSTEM_HPP
#define FILESYSTEM_HPP

#include <bitset>
#include <array>
#include <vector>
#include <memory>
#include <cstdint>
#include <string>
#include <cassert>

#include "diskinterface.hpp"

using Size = uint64_t;

struct FileSystem;
struct INode;
struct INodeTable;
struct SuperBlock;

struct FileSystemException : public StorageException {
	FileSystemException(const std::string &message) : StorageException(message) { };
};

struct SuperBlock {
  Disk *disk = nullptr;
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
  
  std::shared_ptr<Chunk> allocate_chunk() {
    DiskBitMap::BitRange range = this->disk_block_map->find_unset_bits(1);
    if (range.bit_count != 1) {
      throw FileSystemException("FileSystem out of space -- unable to allocate a new chunk");
    }
    
    std::shared_ptr<Chunk> chunk = this->disk->get_chunk(range.start_idx);
    this->disk_block_map->set(range.start_idx);
    
    return std::move(chunk);
  }
};

struct FileSystem {
  Disk *disk;			
  std::unique_ptr<SuperBlock> superblock;
  
  // the file system, once constructed, takes ownership of the disk
  FileSystem(Disk *disk) : disk(disk), superblock(new SuperBlock(disk)) {
  }

  void printForDebug();
};

struct INode;

struct INodeTable {
	std::recursive_mutex lock;

	SuperBlock *superblock = nullptr;
	uint64_t inode_table_size_chunks = 0; // size of the inode table including used_inodes bitmap + ilist 
	uint64_t inode_table_offset = 0; // this actually winds up being the offset of the used_inodes bitmap
	uint64_t inode_ilist_offset = 0; // this ends up storing the calculated real offset of the inodes
	uint64_t inode_count = 0;
	uint64_t inodes_per_chunk = 0;

	SharedObjectCache<uint64_t, INode> inodecache;
	std::unique_ptr<DiskBitMap> used_inodes;
	// struct INode ilist[10]; //TODO: change the size

	// size and offset are in chunks
	INodeTable(SuperBlock *superblock, uint64_t offset_chunks, uint64_t size_chunks);

	void format_inode_table();

	// returns the size of the entire table in chunks
	uint64_t size_chunks();
	uint64_t size_inodes() {
		return inode_count;
	}

	// TODO: have these calls block when an inode is in use
	INode alloc_inode();
	
	INode get_inode(uint64_t idx);
	
	void set_inode(uint64_t idx, INode &node);

	void free_inode(uint64_t idx);
};

struct INode {
	static constexpr uint64_t DIRECT_ADDRESS_COUNT = 8;
	static constexpr uint64_t INDIRECT_ADDRESS_COUNT = 1;
	static constexpr uint64_t DOUBLE_INDIRECT_ADDRESS_COUNT = 1;
	static constexpr uint64_t TRIPPLE_INDIRECT_ADDRESS_COUNT = 1;
	static constexpr uint64_t ADDRESS_COUNT = DIRECT_ADDRESS_COUNT + INDIRECT_ADDRESS_COUNT + DOUBLE_INDIRECT_ADDRESS_COUNT + TRIPPLE_INDIRECT_ADDRESS_COUNT;
	static const uint64_t INDIRECT_TABLE_SIZES[4];

	struct INodeData {
		// we store the data in a subclass so that it can be serialized independently 
		// from data structures that INode needs to keep when loaded in memory
		uint64_t UID = 0; // user id
		uint64_t last_modified = 0; //last modified timestamp
		uint64_t file_size = 0; //size of file
		uint64_t reference_count = 0; //reference count to the inode
		uint64_t addresses[ADDRESS_COUNT] = {0}; //8 direct
		std::bitset<11> inode_bits; //rwxrwxrwx (ow, g, oth) dir special 
	};
	
	uint64_t inode_table_idx = 0;
	INodeData data;
	SuperBlock *superblock = nullptr;	

	std::shared_ptr<Chunk> resolve_indirection(uint64_t chunk_number, bool createIfNotExists);

	static uint64_t get_file_size();

	// NOTE: read is NOT const, it will allocate chunks when reading inodes 
	// that have not been written but that ARE within the size of the file,
	// TODO: possibly be smart about this
	uint64_t read(uint64_t starting_offset, char *buf, uint64_t n);
	uint64_t write(uint64_t starting_offset, const char *buf, uint64_t n);

	std::string to_string();
};


struct IDirectory {
private:
	struct DirHeader {
		uint64_t record_count = 0;
		uint64_t deleted_record_count = 0;

		uint64_t dir_entries_tail = 0;
		uint64_t dir_entries_head = 0;
	};

	DirHeader header;
	INode* inode;

public:

	struct DirEntry {
		struct DirEntryData {
			uint64_t next_entry_ptr = 0;
			uint64_t filename_length = 0;
			uint64_t inode_idx = 0;
		};

		DirEntry(INode *inode) : inode(inode) { };

		INode* inode;
		DirEntryData data;
		char *filename = nullptr;

		~DirEntry() {
			if (filename != nullptr) {
				free(filename);
			}
		}

		// returns the size of the thing it read
		uint64_t read_from_disk(size_t offset) {
			this->inode->read(offset, (char *)(&(this->data)), sizeof(DirEntryData));
			offset += sizeof(DirEntryData);
			
			if (this->filename != nullptr) {
				free(this->filename);
			}

			this->filename = (char *)malloc(this->data.filename_length + 1);
			std::memset(this->filename, 0, this->data.filename_length + 1);
			this->inode->read(offset, this->filename, data.filename_length);
			offset += data.filename_length;

			return offset;
		}

		// only pass filename if you want to update it
		uint64_t write_to_disk(size_t offset, const char *filename) {
			this->inode->write(offset, (char *)(&(this->data)), sizeof(DirEntryData));
			offset += sizeof(DirEntryData);
			
			if (filename != nullptr) {
				assert(data.filename_length == strlen(filename));
				this->inode->write(offset, filename, data.filename_length);
			}

			offset += data.filename_length;

			return offset;
		}
	};

	IDirectory(INode &inode) : inode(&inode) {
		this->inode->read(0, (char *)&header, sizeof(DirHeader));
	}

	void flush() { // flush your changes 
		inode->write(0, (char *)&header, sizeof(DirHeader));
	}

	void initializeEmpty() {
		header = DirHeader();
		inode->write(0, (char *)&header, sizeof(DirHeader));
	}

	void add_file(const char *filename, const INode &child) {
		
		if (header.dir_entries_head == 0) {
			// then it is the first and only element in the linked list!
			DirEntry entry(this->inode);
			entry.data.filename_length = strlen(filename);
			entry.data.inode_idx = child.inode_table_idx;
			entry.filename = strdup(filename);
			// returns the offset after the write of the entry
			size_t next_offset = entry.write_to_disk(sizeof(DirHeader), entry.filename);
			header.dir_entries_head = sizeof(DirHeader);
			header.dir_entries_tail = sizeof(DirHeader);
			header.record_count++;
			// finally, flush ourselves to the disk
			this->flush();
		} else {

			DirEntry last_entry(this->inode);
			size_t next_offset = last_entry.read_from_disk(header.dir_entries_tail);
			last_entry.data.next_entry_ptr = next_offset;
			last_entry.write_to_disk(header.dir_entries_tail, nullptr);

			DirEntry new_entry(this->inode);
			new_entry.data.filename_length = strlen(filename);
			new_entry.data.inode_idx = child.inode_table_idx;
			new_entry.filename = strdup(filename);
			next_offset = new_entry.write_to_disk(next_offset, new_entry.filename);

			header.dir_entries_tail = next_offset;
			header.record_count++;

			this->flush();
		}
	}

	std::unique_ptr<DirEntry> get_file(const char *filename) {
		std::unique_ptr<DirEntry> entry = nullptr;

		while (entry = this->next_entry(entry)) {
			if (strcmp(entry->filename, filename) == 0) {
				return entry;
			}
		}

		return nullptr;
	}

	std::unique_ptr<DirEntry> next_entry(const std::unique_ptr<DirEntry>& entry) {
		std::unique_ptr<DirEntry> next(new DirEntry(this->inode));
		if (entry == nullptr) {
			if (header.record_count == 0)
				return nullptr;

			next->read_from_disk(this->header.dir_entries_head);
		} else {
			if (entry->data.next_entry_ptr == 0) 
				return nullptr; // reached the end of the linked list

			next->read_from_disk(entry->data.next_entry_ptr);
		}

		return next;
	}
};


// struct IDirectory {
// 	static constexpr uint64_t MAX_SEGMENT_LENGTH = 256;

// 	struct FileNotFoundException : public std::exception { };

// 	INode inode;

// 	// NOTE: starts uninitialized
// 	struct IDirEntry {
// 		IDirectory *directory;

// 		IDirEntry(IDirectory *directory) : directory(directory) {
// 		}
		
// 		size_t next_offset = 0; // holds the offset for the next idirentry
// 		size_t offset = 0; // holds the offset for the current value of the IDirEntry
// 		char filename[MAX_SEGMENT_LENGTH + 1]; 
// 		int64_t inode_idx = -1;

// 		bool is_valid() {
// 			return inode_idx != -1;
// 		}

// 		bool have_next() {
// 			return next_offset < this->directory->inode->data.file_size;
// 		}

// 		void get_next() {
// 			auto &inode = this->directory->inode;

// 			// std::memset(entry->filename, 0, sizeof(IDirEntry().filename));
// 			this->filename[MAX_SEGMENT_LENGTH] = 0;
			
// 			size_t read_chars = inode.read(next_offset, this->filename, MAX_SEGMENT_LENGTH);
// 			if (read_chars != MAX_SEGMENT_LENGTH) {
// 				throw FileSystemException("failed to read directory entry, this should NEVER happen");
// 			}

// 			if (inode.read(next_offset + MAX_SEGMENT_LENGTH, (char *)(&(this->inode_idx)), sizeof(uint64_t)) != sizeof(uint64_t)) {
// 				throw FileSystemException("failed to read directory entry, this should NEVER happen");
// 			}

// 			offset = next_offset; 
// 			next_offset += MAX_SEGMENT_LENGTH + sizeof(uint64_t);
// 		}
// 	};

// 	IDirectory(const INode &inode) : inode(inode) {
// 	};

// 	void add_file(const char *filename, const INode& child) {
// 		// TODO: do a sequential scan of the directory looking for a slot to place the new entry in
// 		size_t write_position = inode.data.file_size;

// 		IDirEntry entry = IDirEntry(this);
// 		while (entry.have_next()) {
// 			entry.get_next();
// 			if (entry.filename[0] == 0) {
// 				write_position = entry.offset; 
// 				break ;
// 			}
// 		}

// 		char filename_buf[MAX_SEGMENT_LENGTH];
// 		strncpy(filename_buf, filename, MAX_SEGMENT_LENGTH);
// 		// write out the path name for the child
// 		inode.write(write_position, filename_buf, MAX_SEGMENT_LENGTH);
// 		// write out the inode_idx for the child
// 		int64_t inode_idx = child.inode_table_idx;
// 		inode.write(write_position + MAX_SEGMENT_LENGTH, (char *)(&inode_idx), sizeof(int64_t));
// 	}

// 	IDirEntry find_file(const char *filename) {
// 		IDirEntry entry = IDirEntry(this);
// 		while (entry.have_next()) {
// 			entry.get_next();

// 			if (strcmp(entry.filename, filename) == 0) {
// 				return entry;
// 			}
// 		}

// 		// return the 'invalid directory entry'
// 		return IDirEntry(this);
// 	}

// 	void remove_file(const char *filename) {
// 		IDirEntry entry = IDirEntry(this);

// 		char zerobuf[MAX_SEGMENT_LENGTH + sizeof(uint64_t)];
// 		std::memset(zerobuf, 0, sizeof(zerobuf));

// 		while (entry.have_next()) {
// 			entry.get_next();

// 			if (strcmp(entry.filename, filename) == 0) {
// 				// entirely overwrite the current entry
// 				inode.write(entry.offset, zerobuf, sizeof(zerobuf));
				
// 				return ;
// 			}
// 		}

// 		// return the 'invalid directory entry'
// 		throw FileNotFoundException();
// 	}
// };

#endif
