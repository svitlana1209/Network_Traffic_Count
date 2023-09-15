# Network_Traffic_Count
Accounting of network traffic with saving to own database.</br>
The program displays the total incoming / outgoing traffic for the selected network interface to the console.</br>
The most active network connection is also displayed.</br>
Works on Linux.</br>

## Use
Superuser permissions required to run.</br>
The program can be used:</br>

1. At startup, the name of the network interface for monitoring is specified as a parameter:</br>
```
./ntc enp3s0
```
![](/screenshot/ntc_without_db.png)

   After completion of the work, a report is generated in a text file.</br>


2. At startup, in addition to the name of the network interface, specify the 'db' parameter.</br>
In this case, the traffic will be saved and accumulated in the database. This information is then used for advanced analysis.</br>
```
./ntc enp3s0 db
```
![](/screenshot/ntc_with_db.png)

   A report is also generated in a text file.</br>

## DB and IDX file structure
The file consists of blocks. Each block is a page of 8192 bytes.</br>
When a page in a DB file is full, a new block is added to the end of the file.</br>
IDX is implemented as b-tree.</br>

    DB page structure:</br>
    +----------------+-------------+-------------+     +-------------+</br>
    | service_record | data_record | data_record | ... | data_record |</br>
    |     20 bytes   |   24 bytes  |   24 bytes  | ... |   24 bytes  |</br>
    +----------------+-------------+-------------+     +-------------+</br></br>

    ------+--------------------+---------------+--------------------------------------------------</br>
    Field | Field name         |  Field size,  | Description</br>
    number|                    |  bites        |</br>
    ------+--------------------+---------------+--------------------------------------------------</br>
      SERVICE FIELDS AT THE TOP OF THE PAGE (DB_SERVICE_RECORD):</br>
      1     count                    4           Number of records (db_data_record) per page.</br>
      2     page_number              4           This page number. Numbering starts from one.</br>
      3     page_for_write           4           Work page number to write (FOR CORE PAGE ONLY!).</br>
      4     page_max                 4           Number of pages in db file (FOR CORE PAGE ONLY!).</br>
      5     db_records_number        4           Number of records in DB (FOR CORE PAGE ONLY!).</br>
      -------- TOTAL -------------- 20 bytes</br>
      DATA FIELDS (DB_DATA_RECORD):</br>
      1     YEAR                     2           (u_int16_t)</br>
            MONTH                    1           (u_int8_t)</br>
            DAY                      1           (u_int8_t)</br>
      2     srcIP                    4           (u_int32_t)</br>
      3     dstIP                    4           (u_int32_t)</br>
      4     vol                      8           (long long int)</br>
      5     packs                    4           (u_int32_t)</br>
      -------- TOTAL -------------- 24 bytes</br>


    Structure of records on index file page:</br>
    +----------------+-------------+-------------+     +-------------+</br>
    | service_record | data_record | data_record | ... | data_record |</br>
    |     32 bytes   |   24 bytes  |  24 bytes   | ... |   24 bytes  |</br>
    +----------------+-------------+-------------+     +-------------+</br></br>

    ------+--------------------+-----------+--------------------------------------------------------------------------------------------------</br>
    Field | Field name         |  Field    | Description</br>
    number|                    |  size,    |</br>
          |                    |  bites    |</br>
    ------+--------------------+-----------+--------------------------------------------------------------------------------------------------</br>
       IDX_SERVICE_RECORD:</br>
       1     count                  4        Number of keys per page.</br>
       2     level_number           4        Level number in the tree (0/1/2)</br>
       3     page_number            4        Page number of idx file. Numbering starts from one.</br>
       4     page_count             4        Number of pages in idx file (FOR CORE PAGE ONLY! If page number != 1 and page_count==0 then this is not the root page).</br>
       5     offset_0               4        Page number "offset_0"</br>
       6     rezerv                 4</br>
       7     rezerv                 4</br>
       8     rezerv                 4</br>
      -------- TOTAL ------------- 32 bytes</br>
       IDX_DATA_RECORD:</br>
       1     YEAR                   2  ---+</br>
             MONTH                  1     |</br>
             DAY                    1     |--- This is the key</br>
       2     srcIP                  4     |</br>
       3     dstIP                  4  ---+</br>
       4     page_lower_level       4        Page number of the lower level (offset_N).</br>
       5     db_page_number         4        The page number of the db file for this key's data.</br>
       6     db_offset_on_page      4        Offset inside the DB page relative to its beginning for the data of this key.</br>
      -------- TOTAL ------------- 24 bytes</br></br>

    The schema of the IDX tree:</br>

![](/schema/schema_idx.png)

    The tree expansion starts from the bottom level of the tree - from the leaves.</br>
    A page of this level will be divided if a new key (k) cannot be added to it, because the number of keys on it = 2n.</br>
    At the current level, a blank page is created, but in fact it is added to the end of the file.</br>
    The content of the page and the new key k (i.e. 2n+1) are sorted, halved,</br>
    the first half is written back onto the page, and the second half onto a new blank page of the same level.</br>
    The key that is in the middle (median) goes up one page level.</br>
    If the "count" of this page does not exceed 2n, then add the median key to the page.</br>
    If the "count" exceeds 2n and does not allow insertion of a median key, then this page will also be divided.</br>
    Only the root is not divisible. If the root reaches 2n keys, then the tree is full, because its height does not exceed 3.</br>
    If it is necessary to split the root, i.e. increase the height of the tree, then you need to drop IDX_LEVEL_LIMIT</br>
    and set a variable.</br>


## Prerequisites

Install ncurses library. For example:

```
apt-get install libncurses5-dev libncursesw5-dev
```

### License

This project is licensed under the MIT License.

