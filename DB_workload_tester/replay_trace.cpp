#include <algorithm>
#include <cassert>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <leveldb/db.h>
#include <leveldb/options.h>
#include <sstream>
#include <string>
#include <vector>

/* parse oracleGeneral format trace line */
struct TraceEntry {
  std::string time;       /* timestamp */
  std::string object;     /* object key */
  size_t size;            /* object size */
  std::string next_vtime; /* next timestamp */
};

TraceEntry parse_trace_line(const std::string &line) {
  std::istringstream iss(line);
  std::string token;
  TraceEntry entry;
  std::getline(iss, entry.time, ',');
  std::getline(iss, entry.object, ',');
  std::getline(iss, token, ',');
  entry.size = std::stoull(token);
  std::getline(iss, entry.next_vtime, ',');
  return entry;
}

double percentile(std::vector<double> &data, double p) {
  assert(!data.empty());
  std::sort(data.begin(), data.end());
  size_t idx = static_cast<size_t>(p * data.size());
  if (idx >= data.size())
    idx = data.size() - 1;
  return data[idx];
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    std::cerr
        << "Usage: " << argv[0]
        << " <LevelDB path> <trace file> [max execution time sec, optional]\n";
    return 1;
  }
  std::string db_path = argv[1];
  std::string trace_file = argv[2];
  int max_duration_sec = 0;
  if (argc >= 4) {
    max_duration_sec = std::stoi(argv[3]);
    if (max_duration_sec < 0)
      max_duration_sec = 0;
  }

  leveldb::DB *db;
  leveldb::Options options;
  options.create_if_missing = false; /* must already exist */
  leveldb::Status status = leveldb::DB::Open(options, db_path, &db);
  if (!status.ok()) {
    std::cerr << "LevelDB open failed: " << status.ToString() << std::endl;
    return 2;
  }

  std::string size;
  if (db->GetProperty("leveldb.estimate-live-data-size", &size)) {
    std::cout << "Live data size: " << size << " bytes\n";
  }
  if (db->GetProperty("leveldb.total-sst-files-size", &size)) {
    std::cout << "Total SST files size: " << size << " bytes\n";
  }

  std::ifstream fin(trace_file);
  if (!fin.is_open()) {
    std::cerr << "Cannot open: " << trace_file << std::endl;
    return 3;
  }

  std::vector<double> latencies;
  size_t total_ops = 0, success_ops = 0, notfound_ops = 0;
  auto time_begin = std::chrono::steady_clock::now();

  std::string line;
  while (std::getline(fin, line)) {
    if (line.empty() || line[0] == '#')
      continue;

    auto now = std::chrono::steady_clock::now();
    if (max_duration_sec > 0) {
      auto elapsed_sec =
          std::chrono::duration_cast<std::chrono::seconds>(now - time_begin)
              .count();
      if (elapsed_sec >= max_duration_sec) {
        std::cout << "Reached max execution time limit " << max_duration_sec
                  << " seconds, stopping replay.\n";
        break;
      }
    }

    TraceEntry entry = parse_trace_line(line);

    auto t0 = std::chrono::steady_clock::now();
    std::string value;
    leveldb::ReadOptions ro;
    ro.fill_cache = false; // not pollute cache
    leveldb::Status s = db->Get(ro, entry.object, &value);
    auto t1 = std::chrono::steady_clock::now();
    double latency = std::chrono::duration<double, std::milli>(t1 - t0).count();

    latencies.push_back(latency);
    ++total_ops;
    if (s.ok())
      ++success_ops;
    else if (s.IsNotFound())
      ++notfound_ops;
    else
      std::cerr << "Read failed: " << s.ToString() << std::endl;
  }

  auto time_end = std::chrono::steady_clock::now();
  double elapsed = std::chrono::duration<double>(time_end - time_begin).count();

  double throughput = (elapsed > 0) ? (total_ops / elapsed) : 0.0;
  double p99_latency = latencies.empty() ? 0.0 : percentile(latencies, 0.99);

  std::cout << std::fixed << std::setprecision(2);
  std::cout << "Total ops:      " << total_ops << std::endl;
  std::cout << "Found:          " << success_ops << std::endl;
  std::cout << "Not found:      " << notfound_ops << std::endl;
  std::cout << "Elapsed time:   " << elapsed << " seconds" << std::endl;
  std::cout << "Throughput:     " << throughput << " ops/sec" << std::endl;
  std::cout << "p99 latency:    " << p99_latency << " ms" << std::endl;

  delete db;
  return 0;
}
