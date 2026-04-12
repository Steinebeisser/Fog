# FOG -> File OrGanizer

## Scan

### Flags
- -r/--recursive
- -v/--verbose | only enabled when logging enbaled which is not default as costs performance
    - Compile with `-DPGS_LOG_ENABLED=true` to enable logs again

if you get a segfault when scanning try to lower `GETDENTS64_CACHE_SIZE` default is 128KiB

### Benachmarks

```
perf stat -e cycles,instructions,cache-references,cache-misses,\
page-faults,context-switches,migrations \
    ./fog scan / -r
File Count: 943850
Dir Count: 72116
Total Size: 922 GiB
Took 2574 ms
 Performance counter stats for './fog scan / -r':
       272,554,690      cycles:u                                                              
       218,865,148      instructions:u                                                        
        31,970,651      cache-references:u                                                    
           136,134      cache-misses:u                                                        
                90      page-faults:u                                                         
                 0      context-switches:u                                                    
                 0      migrations:u                                                          
       2.575597194 seconds time elapsed
       0.245282000 seconds user
       1.167380000 seconds sys
```
