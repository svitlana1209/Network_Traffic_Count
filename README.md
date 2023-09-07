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

## Prerequisites

Install ncurses library. For example:

```
apt-get install libncurses5-dev libncursesw5-dev
```

### License

This project is licensed under the MIT License.

