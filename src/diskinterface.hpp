#ifndef DISKINTERFACE_HPP
#define DISKINTERFACE_HPP

#include <stdint.h>
#include <mutex>
#include <unordered_map>

using Byte = uint8_t;

class Disk;

struct DiskException : public std::exception {
	std::string message;
	DiskException(const std::string &message) : message(message) { };
};

struct Chunk {
	Disk *parent = nullptr;

	std::mutex lock;
	size_t size_bytes = 0;
	size_t chunk_idx = 0;
	std::unique_ptr<Byte[]> data = nullptr;

	~Chunk();
};


template<typename K, typename V>
class SharedObjectCache {
private:
	size_t size_next_sweep = 16;
	std::unordered_map<K, std::weak_ptr<V>> map;
public:
	void sweep(bool force) {
		if (!force && map.size() < size_next_sweep)
			return ;

		for (auto it = this->map.cbegin(); it != this->map.cend();){
			if ((*it).second.expired()) {
				this->map.erase(it++);    // or "it = m.erase(it)" since C++11
			} else {
				++it;
			}
		}

		size_next_sweep = this->map.size() < 16 ? 16 : this->map.size();
	}

	void put(const K& k, std::weak_ptr<V> v) {
		map[k] = std::move(v);
		this->sweep(false);
	}

	std::shared_ptr<V> get(const K& k) {
		auto ref = this->map.find(k);
		if (ref != this->map.end()) {
			if (std::shared_ptr<V> v = (*ref).second.lock()) {
				return std::move(v);
			}
		}

		return nullptr;
	}

	inline size_t size() {
		return this->map.size();
	}
};

/*
	acts as an interface onto the disk as well as a cache for chunks on disk
	in this way the same chunk can be accessed and modified in multiple places
	at the same time if this is desirable
*/
class Disk {
private:
	// properties of the class
	const size_t _size_chunks;
	const size_t _chunk_size;

	std::unique_ptr<Byte[]> data;

	// a mutex which protects access to the disk
	std::mutex lock;

	// a cache of chunks that are loaded in
	SharedObjectCache<size_t, Chunk> chunk_cache;

	// loops over weak pointers, if any of them are expired, it deletes 
	// the entries from the unordered map 
	void sweep_chunk_cache(); 
public:

	Disk(size_t size_chunk_ctr, size_t chunk_size_ctr) 
		: _chunk_size(chunk_size_ctr), _size_chunks(size_chunk_ctr) {
		// initialize the data for the disk
		this->data = std::unique_ptr<Byte[]>(new Byte[this->size_bytes()]);
		std::memset(this->data.get(), 0, this->size_bytes());
	}

	inline size_t size_bytes() const {
		return _size_chunks * _chunk_size;
	}

	inline size_t size_chunks() const {
		return _size_chunks;
	}

	inline size_t chunk_size() const {
		return _chunk_size;
	}

	std::shared_ptr<Chunk> get_chunk(size_t chunk_idx);

	void flush_chunk(const Chunk& chunk);

	void try_close();

	~Disk();
};


#endif