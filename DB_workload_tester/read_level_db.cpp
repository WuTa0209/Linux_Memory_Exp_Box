#include <iostream>
#include <leveldb/db.h>
#include <memory>
#include <string>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <LevelDB Path>\n";
    return 1;
  }
  std::string dbpath = argv[1];

  leveldb::DB *db = nullptr;
  leveldb::Options options;
  options.create_if_missing = false;

  leveldb::Status st = leveldb::DB::Open(options, dbpath, &db);
  if (!st.ok()) {
    std::cerr << "Failed to open LevelDB: " << st.ToString() << "\n";
    return 2;
  }

  {
    leveldb::ReadOptions ro;
    std::unique_ptr<leveldb::Iterator> it(db->NewIterator(ro));

    int count = 0;
    for (it->SeekToFirst(); it->Valid() && count < 10; it->Next(), ++count) {
      std::cout << "Key: " << it->key().ToString()
                << ", Value: " << it->value().ToString()
                << ", Value.size=" << it->value().size() << "\n";
    }
    if (!it->status().ok()) {
      std::cerr << "Iterator error: " << it->status().ToString() << "\n";
    }
  }

  delete db;
  return 0;
}
