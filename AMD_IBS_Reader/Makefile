all:
	gcc -O2 -Wall data_src_decoder.c ibs_reader.c -o ibs_reader -lpthread
	gcc -O2 data_src_decoder.c data_src_decoder_test.c -o data_src_decoder_test
clean:
	rm -f ibs_reader
	rm -f ibs_samples.csv
	rm -f data_src_decoder_test
show_test:
	python mem_block_hotness.py ibs_samples.csv --delimiter ',' --header --bar --top 10
test_data_src_decoder: all
	./data_src_decoder_test

.PHONY: all clean show_test test_data_src_decoder