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
  
  uint64_t inode_table_inode_count; // number of inodes in the inode_table
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
	INodeTable(SuperBlock *superblock, uint64_t offset_chunks, uint64_t inode_count);

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


/*
	TODO: implement cleaning of a directory
*/
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

		uint64_t offset = 0;
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
			this->offset = offset;

			std::cout << "READ INODE: offset = " << offset << std::endl;

			this->inode->read(offset, (char *)(&(this->data)), sizeof(DirEntryData));
			offset += sizeof(DirEntryData);
			
			if (this->filename != nullptr) {
				free(this->filename);
			}
			fprintf(stdout, "\treading filename from disk of length %d at position %d\n", this->data.filename_length, offset);
			this->filename = (char *)malloc(this->data.filename_length + 1);
			std::memset(this->filename, 0, this->data.filename_length + 1);
			this->inode->read(offset, this->filename, data.filename_length);
			offset += this->data.filename_length;

			fprintf(stdout, "\t\tread filename: %s\n", this->filename);

			return offset;
		}

		// only pass filename if you want to update it
		uint64_t write_to_disk(size_t offset, const char *filename) {
			this->offset = offset;
			std::cout << "WROTE INODE: offset = " << offset << std::endl;
			if (this->filename)
				std::cout << "\tFILE NAME: " << this->filename << std::endl;
			std::cout << "\tFILE NAME LENGTH: " << this->data.filename_length << std::endl;
			std::cout << "\tNEXT ENTRY PTR: " << this->data.next_entry_ptr << std::endl;
			std::cout << "\tINODE IDX: " << this->data.inode_idx << std::endl;
			this->inode->write(offset, (char *)(&(this->data)), sizeof(DirEntryData));
			offset += sizeof(DirEntryData);
			
			if (filename != nullptr) {
				std::cout << "\tWROTE OUT FILENAME AT OFFSET: " << offset << " LENGTH: " << data.filename_length << std::endl;
				assert(data.filename_length != 0);
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

	std::unique_ptr<DirEntry> add_file(const char *filename, const INode &child) {
		if (this->get_file(filename) != nullptr) {
			return nullptr;
		}

		if (header.dir_entries_head == 0) {
			// then it is the first and only element in the linked list!
			std::unique_ptr<DirEntry> entry(new DirEntry(this->inode));
			entry->data.filename_length = strlen(filename);
			entry->data.inode_idx = child.inode_table_idx;
			entry->filename = strdup(filename);
			
			// returns the offset after the write of the entry
			size_t next_offset = entry->write_to_disk(sizeof(DirHeader), entry->filename);
			header.dir_entries_head = sizeof(DirHeader);
			header.dir_entries_tail = sizeof(DirHeader);
			header.record_count++;
			// finally, flush ourselves to the disk
			
			this->flush();
			return std::move(entry);
		} else {

			DirEntry last_entry(this->inode);
			size_t next_offset = last_entry.read_from_disk(header.dir_entries_tail);
			last_entry.data.next_entry_ptr = next_offset;
			last_entry.write_to_disk(header.dir_entries_tail, nullptr);

			std::unique_ptr<DirEntry> new_entry(new DirEntry(this->inode));
			new_entry->data.filename_length = strlen(filename);
			new_entry->data.inode_idx = child.inode_table_idx;
			new_entry->filename = strdup(filename);
			new_entry->write_to_disk(next_offset, new_entry->filename);

			header.dir_entries_tail = next_offset;
			header.record_count++;

			this->flush();
			return std::move(new_entry);
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

	std::unique_ptr<DirEntry> remove_file(const char *filename) {
		// TODO: update reference count when removing a file

		std::unique_ptr<DirEntry> last_entry = nullptr;
		std::unique_ptr<DirEntry> entry = nullptr;

		while (entry = this->next_entry(entry)) {
			if (strcmp(entry->filename, filename) == 0) {
				std::cout << "REMOVING A FILE WITH THE NAME: " << filename << std::endl;
				if (last_entry == nullptr) {
					std::cout << "\tLAST ENTRY IS NULL PTR, MOVING HEAD FORWARD TO: " << header.dir_entries_head << std::endl;
					header.dir_entries_head = entry->data.next_entry_ptr;
					if (entry->data.next_entry_ptr == 0) {
						std::cout << "\tNEXT_ENTRY_PTR IS NULL, SETTING TAIL TO 0" << std::endl;
						header.dir_entries_tail = 0;

						std::cout << "\t\tVALUES OF HEAD AND TAIL ARE " << header.dir_entries_head << ", " << header.dir_entries_tail << std::endl;
					}
				} else {
					std::cout << "LAST ENTRY NOT NULL, UPDATING LAST_ENTRY's NEXT_ENTRY_PTR TO " << entry->data.next_entry_ptr << std::endl;
					last_entry->data.next_entry_ptr = entry->data.next_entry_ptr;
					last_entry->write_to_disk(last_entry->offset, nullptr);

					if (last_entry->data.next_entry_ptr == 0) {
						header.dir_entries_tail = last_entry->offset;
					}
				}
				
				header.deleted_record_count++;
				header.record_count--;
				return std::move(entry);
			}
			last_entry = std::move(entry);
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

		fprintf(stdout, "\tfound entry at offset: %d -> %s -> next_entry_ptr %d\n", next->offset, next->filename, next->data.next_entry_ptr);
		
		return next;
	}
};


#endif
