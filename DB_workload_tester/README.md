Using **tracePrint** from **libCacheSim**, you can obtain the converted data trace.
The workload can be downloaded from:
[https://ftp.pdl.cmu.edu/pub/datasets/twemcacheWorkload/cacheDatasets/twitter/](https://ftp.pdl.cmu.edu/pub/datasets/twemcacheWorkload/cacheDatasets/twitter/)

```bash
/libCacheSim/_build/bin/tracePrint <workload>.zst oracleGeneral > db_data.txt
```

Build the database from the trace:

```bash
./build_db <db_name> db_data.txt
```

Then replay the database:

```bash
./replay_trace <db_name> db_data.txt <time_limit>
```
