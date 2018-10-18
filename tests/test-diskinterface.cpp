#include <iostream>

#include "catch.hpp"

#include "diskinterface.hpp"

TEST_CASE( "Disk interface should work", "[diskinterface]" ) {
	std::unique_ptr<Disk> disk(new Disk(256, 16));

	std::shared_ptr<Chunk> chunk0;
	SECTION("can get a chunk") {
		chunk0 = disk->get_chunk(0);
		REQUIRE(chunk0 != nullptr);
		REQUIRE(chunk0->size_bytes == disk->chunk_size());
		REQUIRE(chunk0->chunk_idx == 0);
		REQUIRE(chunk0->data != nullptr);
	}

	SECTION("that chunk should be filled with 0's by default") {
		chunk0 = disk->get_chunk(0);
		for (size_t i = 0; i < disk->chunk_size(); ++i) {
			if (chunk0->data.get()[i] != 0) {
				REQUIRE(false);
			}
		}
	}

	SECTION("can get many chunks and trigger a sweep of the chunk cache without segfaulting") {
		std::shared_ptr<Chunk> chunk;
		for (size_t i = 0; i < 128; ++i) {
			chunk = disk->get_chunk(i);
			REQUIRE(chunk != nullptr);
		}
	}

	SECTION("can get many chunks again, hold on to them, and then free them all at once") {
		std::vector<std::shared_ptr<Chunk>> chunkvec;
		for (size_t i = 0; i < 128; ++i) {
			chunkvec.push_back(disk->get_chunk(i));
		}
	}

	SECTION("can get two references to the same chunk, change a value in one, and see it in the other") {
		std::shared_ptr<Chunk> refA = disk->get_chunk(2);
		std::shared_ptr<Chunk> refB = disk->get_chunk(2);
		refA->data.get()[0] = 1;
		REQUIRE(refB->data.get()[0] == 1);
	}

	SECTION("can get a reference, release it thus flushing chunk to disk, and then get a new reference and find the same data") {
		{
			std::shared_ptr<Chunk> refA = disk->get_chunk(4);
			refA->data.get()[0] = 1;
		}
		
		{
			std::shared_ptr<Chunk> refB = disk->get_chunk(4);
			REQUIRE(refB->data.get()[0] == 1);
		}
	}

}

TEST_CASE( "Disk bitmap should work", "[bitmap]" ) {
	constexpr size_t bitmap_size = 32;
	std::unique_ptr<Disk> disk(new Disk(256, 16));
	std::unique_ptr<DiskBitMap> bitmap(new DiskBitMap(disk.get(), 0, bitmap_size));

	SECTION("clearing the chunk should leave it initialized as all 0's") {
		bitmap->clear_all();

		for (size_t idx = 0; idx < bitmap_size; ++idx) {
			REQUIRE(bitmap->get(idx) == 0);
		}
	}

	SECTION("can set and read back every other bit") {
		bitmap->clear_all();

		for (size_t idx = 0; idx < bitmap_size; idx += 2) {
			bitmap->set(idx + 1);
		}

		for (size_t idx = 0; idx < bitmap_size; idx += 2) {
			REQUIRE(bitmap->get(idx) == 0);
			REQUIRE(bitmap->get(idx + 1) == 1);
		}
	}

	SECTION("can set every other bit and then request a bunch of free bits") {
		bitmap->clear_all();

		for (size_t idx = 0; idx < bitmap_size; idx += 2) {
			bitmap->set(idx + 1);
		}

		for (size_t idx = 0; idx < bitmap_size; idx += 2) {
			auto range = bitmap->find_unset_bits(1);
			REQUIRE(range.bit_count == 1);
			REQUIRE(range.start_idx == idx);
			range.set_range(*bitmap);
		}
	}

	SECTION("can set every other 4th bit and then request allocation of free bits") {
		bitmap->clear_all();

		for (size_t idx = 0; idx < bitmap_size; idx += 4) {
			bitmap->set(idx);
		}

		for (size_t idx = 0; idx < bitmap_size; idx += 4) {
			auto range = bitmap->find_unset_bits(3);
			REQUIRE(range.bit_count == 3);
			REQUIRE(range.start_idx == idx + 1);
			range.set_range(*bitmap);
		}
	}

	SECTION("a test of edge conditions with small bitvectors and large bit requests") {
		std::unique_ptr<DiskBitMap> bitmap2(new DiskBitMap(disk.get(), bitmap->size_chunks(), 4));
		auto range = bitmap2->find_unset_bits(8);
		REQUIRE(range.bit_count == 4);
		REQUIRE(range.start_idx == 0);
	}
}