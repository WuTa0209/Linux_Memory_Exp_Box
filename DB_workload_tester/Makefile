all:
	g++ -O2 -std=c++11 -lleveldb build_level_db.cpp -o build_level_db
	g++ -O2 -std=c++11 -lleveldb read_level_db.cpp -o read_level_db
	g++ -O2 -std=c++11 -lleveldb replay_trace.cpp -o replay_trace
clean:
	rm -f build_level_db read_level_db replay_trace