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
		for (size_t i = 0; i < disk->chunk_size(); ++i) {
			if (chunk0->data[i] != 0) {
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

}