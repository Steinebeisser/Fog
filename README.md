# FOG -> File OrGanizer

High-performance filesystem cli tool

WIP

Currently Linux only



## Scanner

Cold run
```
./fog scan / -rH
File Count : 2640986
Dir Count  : 173424
Total Size : 1.25 TiB
Disk Size  : 1.78 TiB
Time taken : 18926 ms
```

Warm run
```
./fog scan / -rH
File Count : 2640873
Dir Count  : 173424
Total Size : 1.25 TiB
Disk Size  : 1.78 TiB
Time taken : 3868 ms
```


## Dupe Finder

```
./fog dupe /home/stein/programming -rHn10000 -S
    scan done: 253478 filesfound
    238074 candidate files to hash
    head hashing... 230000/238074
    head hash done                    
    124814 files are still alive and need to be fully hashed
    full hashing... 120000/124814
   full hash done

| Duplicate scan summary
-----------------------
| Groups: 35798
| Duplicate files: 124814
| Wasted: 670.28 MiB
| Largest Group: 2 files (31.50 MiB per member)



Time taken : 29174 ms
```

## File Hasher

```
./fog hash fog --check 78cd8f8c21839b893627013af1e6fedda25c3dfcbd7ce32120fd6523717deba21066772aa3ac599b1043314b7817ce75893cbc3e055d0dde651d5a1804831be4 -a b512
[OK]
78cd8f8c21839b893627013af1e6fedda25c3dfcbd7ce32120fd6523717deba21066772aa3ac599b1043314b7817ce75893cbc3e055d0dde651d5a1804831be4
  fog
```

## Git Like CLI

```
./fog -h
Available arguments:

  scan
      Scan a folder/file

  dupe
      Scans a folder and returns duplicte files

  hash
      Hash fieles/input

  -h, --help
      Show help message
```

---

```
./fog scan -h
Available arguments:

  -h, --help
      Show help message

  -v, --verbose
      Enable verbose output

  -r, --recurseive
      Scan recusively

  -e, --no-exluce
      By default some directories are excluded, this scans them anyways, can lead to weird behavior

  -H, --include-hidden
      Include Hidden Files in the scan

  -m, --include-mount
      Include Mount Files in the scan

  -d, --decimal
      Use decimal units (TB, GB, MB) instead of binary (TiB, GiB, MiB)

  -a, --apparent
      Show the logiacl size
```

---

```
./fog dupe -h
Available arguments:

  -h, --help
      Show help message

  -v, --verbose
      Enable verbose output

  -r, --recurseive
      Scan recusively

  -e, --no-exluce
      By default some directories are excluded, this scans them anyways, can lead to weird behavior

  -H, --include-hidden
      Include Hidden Files in the scan

  -m, --include-mount
      Include Mount Files in the scan

  -n, --notify
      After how many files it should send a life signal

  -s, --min-size
      Will ignore every file under this limit

  -t, --top
      largest duplicate groups

  -E, --exclude
      Exclude files, comma separated list

  -S, --summary
      Create Summary
```
