#include <fstream>
#include <iostream>
#include <leveldb/cache.h>
#include <leveldb/db.h>
#include <random>
#include <sstream>
#include <string>
#include <unordered_set>

/* generate random value of given size */
std::string random_value(size_t size, int seed = 0) {
  static std::mt19937 gen(seed ? seed : std::random_device{}());
  static const char charset[] =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);
  std::string str(size, 0);
  for (size_t i = 0; i < size; ++i)
    str[i] = charset[dist(gen)];
  return str;
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0]
              << " <LevelDB database path> <trace file>\n";
    return 1;
  }
  std::string dbpath = argv[1], tracefile = argv[2];
  leveldb::DB *db;
  leveldb::Options options;
  options.create_if_missing = true;
  options.block_cache = leveldb::NewLRUCache(1024); // 1KB cache
  leveldb::Status status = leveldb::DB::Open(options, dbpath, &db);
  if (!status.ok()) {
    std::cerr << "LevelDB open failed: " << status.ToString() << "\n";
    return 2;
  }
  std::ifstream fin(tracefile);
  if (!fin) {
    std::cerr << "Cannot open: " << tracefile << "\n";
    return 3;
  }
  std::string line;
  size_t n = 0;
  std::unordered_set<std::string> inserted;
  /* Skip header row */
  std::getline(fin, line);

  while (std::getline(fin, line)) {
    if (line.empty())
      continue;
    std::istringstream iss(line);
    std::string time, object, size_s, next;
    std::getline(iss, time, ',');
    std::getline(iss, object, ',');
    std::getline(iss, size_s, ',');
    std::getline(iss, next, ',');

    /* only insert the first occurrence of each object */
    if (inserted.count(object))
      continue;
    inserted.insert(object);

    size_t size = std::stoul(size_s);
    std::string value = random_value(size, n);

    leveldb::Status s = db->Put(leveldb::WriteOptions(), object, value);
    if (!s.ok())
      std::cerr << "Put failed: " << object << " " << s.ToString() << "\n";
    ++n;
    if (n % 10000 == 0)
      std::cout << "Inserted: " << n << " records\n";
  }
  std::cout << "Total inserted " << n << " records\n";
  delete db;
  return 0;
}
