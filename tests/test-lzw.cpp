/**
 * @file   test-lzw.cpp
 * @brief  Test code for LZW compression/decompression filters.
 *
 * Copyright (C) 2010-2011 Adam Nielsen <malvineous@shikadi.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <boost/test/unit_test.hpp>

#include <camoto/lzw.hpp>
#include <camoto/bitstream.hpp>
#include <camoto/stream_string.hpp>
#include <camoto/stream_filtered.hpp>
#include <camoto/util.hpp>

#include "tests.hpp"

using namespace camoto;

struct lzw_decomp_sample: public default_sample {

	stream::string_sptr in;
	stream::string_sptr out;

	lzw_decomp_sample() :
		in(new stream::string()),
		out(new stream::string())
	{
	}

	boost::test_tools::predicate_result is_equal(const std::string& strExpected)
	{
		// See if the stringstream now matches what we expected
		return this->default_sample::is_equal(strExpected, out->str());
	}

};

BOOST_FIXTURE_TEST_SUITE(lzw_decomp_suite, lzw_decomp_sample)

BOOST_AUTO_TEST_CASE(lzw_decomp_read)
{
	BOOST_TEST_MESSAGE("Decompress some LZW data");

	bitstream_sptr bit_in(new bitstream(this->in, bitstream::bigEndian));
	bit_in->write(9, 'H');
	bit_in->write(9, 'e');    // 0x101 -> He
	bit_in->write(9, 'l');    // 0x102 -> el
	bit_in->write(9, 'l');    // 0x103 -> ll
	bit_in->write(9, 'o');    // 0x104 -> lo
	bit_in->write(9, ' ');    // 0x105 -> o 
	bit_in->write(9, 'h');    // 0x106 ->  h
	bit_in->write(9, 0x102);  // 0x107 -> he
	bit_in->write(9, 0x104);  // 0x108 -> ell
	bit_in->write(9, 0x106);  // 0x109 -> lo
	bit_in->write(9, 0x108);  // 0x10a ->  he
	bit_in->write(9, 'o');
	bit_in->write(9, '.');
	bit_in->write(9, 0x100);

	filter_sptr filt(new filter_lzw_decompress(9, 9, 0x101, 0x100, 0,
		LZW_BIG_ENDIAN | LZW_EOF_PARAM_VALID));
	stream::input_filtered_sptr processed(new stream::input_filtered());
	processed->open(this->in, filt);

	stream::copy(this->out, processed);

	BOOST_CHECK_MESSAGE(is_equal("Hello hello hello."),
		"Decompressing LZW data failed");
}

BOOST_AUTO_TEST_CASE(lzw_decomp_bitlength_expand)
{
	BOOST_TEST_MESSAGE("Codeword bit length expansion when LZW decompressing");

	bitstream_sptr bit_in(new bitstream(this->in, bitstream::bigEndian));
	for (int i = 0; i < 256; i++) {
		bit_in->write(9, 'A');
	}
	// Codeword will have just expanded to 10 bits
	bit_in->write(10, 'B');
	bit_in->write(10, 0x100);

	filter_sptr filt(new filter_lzw_decompress(9, 10, 0x101, 0x100, 0,
		LZW_BIG_ENDIAN | LZW_EOF_PARAM_VALID));
	stream::input_filtered_sptr processed(new stream::input_filtered());
	processed->open(this->in, filt);

	stream::copy(this->out, processed);

	BOOST_CHECK_MESSAGE(is_equal(std::string(256, 'A') + "B"),
		"Codeword bit length expansion when LZW decompressing failed");
}

BOOST_AUTO_TEST_CASE(lzw_decomp_reset)
{
	BOOST_TEST_MESSAGE("Dictionary reset shared with EOF codeword when LZW decompressing");

	bitstream_sptr bit_in(new bitstream(this->in, bitstream::bigEndian));
	for (int i = 0; i < 256; i++) {
		bit_in->write(9, 'A');
	}
	// Codeword will have just expanded to 10 bits
	bit_in->write(10, 'B');
	bit_in->write(10, 0x100);
	bit_in->write(9, 'C');
	bit_in->write(9, 'C');
	bit_in->write(9, 0x100);

	filter_sptr filt(new filter_lzw_decompress(9, 10, 0x101, 0x100, 0x100,
		LZW_BIG_ENDIAN | LZW_RESET_PARAM_VALID));
	stream::input_filtered_sptr processed(new stream::input_filtered());
	processed->open(this->in, filt);

	stream::copy(this->out, processed);

	BOOST_CHECK_MESSAGE(is_equal(std::string(256, 'A') + "BCC"),
		"Dictionary reset shared with EOF codeword when LZW decompressing failed");
}

BOOST_AUTO_TEST_CASE(lzw_decomp_dict_overflow)
{
	BOOST_TEST_MESSAGE("Decompress some LZW data with a dictionary overflow");

	bitstream_sptr bit_in(new bitstream(this->in, bitstream::bigEndian));
	for (int i = 0; i < (1<<8); i++) bit_in->write(9, 'a');
	for (int i = 0; i < (1<<9); i++) bit_in->write(10, 'b');
	for (int i = 0; i < (1<<10); i++) bit_in->write(11, 'c');
	for (int i = 0; i < (1<<11); i++) bit_in->write(12, 'd');
	bit_in->write(12, 'e');
	bit_in->write(12, 'e');
	bit_in->write(12, 0x100);

	filter_sptr filt(new filter_lzw_decompress(9, 12, 0x101, 0x100, 0,
		LZW_BIG_ENDIAN | LZW_EOF_PARAM_VALID));
	stream::input_filtered_sptr processed(new stream::input_filtered());
	processed->open(this->in, filt);

	stream::copy(this->out, processed);

	BOOST_CHECK_MESSAGE(is_equal(createString(
		std::string((1 << 8), 'a') <<
		std::string((1 << 9), 'b') <<
		std::string((1 << 10), 'c') <<
		std::string((1 << 11), 'd') <<
		"ee"
		)),
		"Decompressing LZW data with a dictionary overflow failed");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(lzw_comp_suite, lzw_decomp_sample)

BOOST_AUTO_TEST_CASE(lzw_comp_write)
{
	BOOST_TEST_MESSAGE("Compress some LZW data");

	stream::string_sptr exp(new stream::string());
	bitstream_sptr bit_exp(new bitstream(exp, bitstream::bigEndian));
	bit_exp->write(9, 'H');
	bit_exp->write(9, 'e');
	bit_exp->write(9, 'l');
	bit_exp->write(9, 'l');
	bit_exp->write(9, 'o');
	bit_exp->write(9, ' ');
	bit_exp->write(9, 'h');
	bit_exp->write(9, 'e');
	bit_exp->write(9, 'l');
	bit_exp->write(9, 'l');
	bit_exp->write(9, 'o');
	bit_exp->write(9, ' ');
	bit_exp->write(9, 'h');
	bit_exp->write(9, 'e');
	bit_exp->write(9, 'l');
	bit_exp->write(9, 'l');
	bit_exp->write(9, 'o');
	bit_exp->write(9, '.');
	bit_exp->write(9, 0x100);

	this->in->write("Hello hello hello.");

	filter_sptr filt(new filter_lzw_compress(9, 9, 0x101, 0x100, 0, LZW_BIG_ENDIAN | LZW_EOF_PARAM_VALID));
	stream::input_filtered_sptr processed(new stream::input_filtered());
	processed->open(this->in, filt);

	stream::copy(this->out, processed);

	BOOST_CHECK_MESSAGE(is_equal(exp->str()),
		"Compressing LZW data failed");
}

BOOST_AUTO_TEST_CASE(lzw_comp_write_dict_grow)
{
	BOOST_TEST_MESSAGE("Compress some LZW data with a growing dictionary");

	stream::string_sptr exp(new stream::string());
	bitstream_sptr bit_exp(new bitstream(exp, bitstream::bigEndian));
	for (int i = 0; i < 256; i++) {
		bit_exp->write(9, 'a');
	}
	bit_exp->write(10, 'b');
	bit_exp->write(10, 0x100);

	for (int i = 0; i < 256; i++) {
		this->in->write("a");
	}
	this->in->write("b");

	filter_sptr filt(new filter_lzw_compress(9, 12, 0x101, 0x100, 0, LZW_BIG_ENDIAN | LZW_EOF_PARAM_VALID));
	stream::input_filtered_sptr processed(new stream::input_filtered());
	processed->open(this->in, filt);

	stream::copy(this->out, processed);

	BOOST_CHECK_MESSAGE(is_equal(exp->str()),
		"Compressing LZW data with growing dictionary failed");
}

BOOST_AUTO_TEST_CASE(lzw_comp_write_dict_overflow)
{
	BOOST_TEST_MESSAGE("Compress some LZW data with an overflowing dictionary");

	stream::string_sptr exp(new stream::string());
	bitstream_sptr bit_exp(new bitstream(exp, bitstream::bigEndian));
	for (int i = 0; i < (1<<8); i++) bit_exp->write(9, 'a');
	for (int i = 0; i < (1<<9); i++) bit_exp->write(10, 'b');
	for (int i = 0; i < (1<<10); i++) bit_exp->write(11, 'c');
	for (int i = 0; i < (1<<11); i++) bit_exp->write(12, 'd');
	bit_exp->write(12, 'e');
	bit_exp->write(12, 'e');
	bit_exp->write(12, 0x100);

	for (int i = 0; i < (1<<8); i++) this->in->write("a");
	for (int i = 0; i < (1<<9); i++) this->in->write("b");
	for (int i = 0; i < (1<<10); i++) this->in->write("c");
	for (int i = 0; i < (1<<11); i++) this->in->write("d");
	this->in->write("ee");

	filter_sptr filt(new filter_lzw_compress(9, 12, 0x101, 0x100, 0, LZW_BIG_ENDIAN | LZW_EOF_PARAM_VALID));
	stream::input_filtered_sptr processed(new stream::input_filtered());
	processed->open(this->in, filt);

	stream::copy(this->out, processed);

	BOOST_CHECK_MESSAGE(is_equal(exp->str()),
		"Compressing LZW data with an overflowing dictionary failed");
}

BOOST_AUTO_TEST_CASE(lzw_comp_write_dict_overflow_reset)
{
	BOOST_TEST_MESSAGE("Compress some LZW data with an autoreset dictionary");

	stream::string_sptr exp(new stream::string());
	bitstream_sptr bit_exp(new bitstream(exp, bitstream::bigEndian));
	for (int i = 0; i < (1<<8); i++) bit_exp->write(9, 'a');
	for (int i = 0; i < (1<<9); i++) bit_exp->write(10, 'b');
	for (int i = 0; i < (1<<10); i++) bit_exp->write(11, 'c');
	for (int i = 0; i < (1<<11); i++) bit_exp->write(12, 'd');
	bit_exp->write(9, 'e');
	bit_exp->write(9, 'e');
	bit_exp->write(9, 0x100);

	for (int i = 0; i < (1<<8); i++) this->in->write("a");
	for (int i = 0; i < (1<<9); i++) this->in->write("b");
	for (int i = 0; i < (1<<10); i++) this->in->write("c");
	for (int i = 0; i < (1<<11); i++) this->in->write("d");
	this->in->write("ee");

	filter_sptr filt(new filter_lzw_compress(9, 12, 0x101, 0x100, 0,
		LZW_BIG_ENDIAN | LZW_EOF_PARAM_VALID | LZW_RESET_FULL_DICT));
	stream::input_filtered_sptr processed(new stream::input_filtered());
	processed->open(this->in, filt);

	stream::copy(this->out, processed);

	BOOST_CHECK_MESSAGE(is_equal(exp->str()),
		"Compressing LZW data with an autoreset dictionary failed");
}

BOOST_AUTO_TEST_SUITE_END()
