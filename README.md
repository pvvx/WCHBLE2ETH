# WCHBLE2ETH
BLE Advertisements Repeater into Ethernet TCP/IP

Проект в начальной разработке...

Чип CH32V208W.

Demo-board от WCH - CH32V208WBU6-EVT-R0:

![img](https://raw.githubusercontent.com/pvvx/WCHBLE2ETH/master/img/CH32V208WBU6-EVT-R0.jpg)

CH32V208W принимает BLE-рекламу и отправляет в TCP/IP сокет.

* Принимаются все типы BLE-реклам, включая "LE Long Range" (CODED PHY).
* Обеспечивает прием более 60 реклам в секунду.
* Используется DHCP.
* Порт сервера TCP/IP сокета устройства - 1000.

## Формат TCP потока

Передача принятых фреймов BLE-реклам стартует при соединении с сокетом устройства (порт с номером 1000).
Фреймы передаются друг за дургом.

|N байта | Информация|
|---|---|
| 0 | размер структуры данных BLE рекламы |
| 1 | [0:3] GAP Advertising Report Event Types, [4:7] GAP Address type  |
| 2 | [0:3] primary PHY, [4:7] secondary PHY |
| 3 | RSSI |
| 4..9 | MAC |
| 10.. | структура данных BLE рекламы |

Первый байт, с номером 0, равен размеру всего фрейма минус 10.

| ID | GAP Advertising Report Event Types |
|--- |--- |
| 0x00 | Connectable undirected advertisement |
| 0x01 | Connectable directed advertisement |
| 0x02 | Scannable undirected advertisement |
| 0x03 | Non-Connectable undirected advertisement |
| 0x04 | Scan Response |
| 0x05 | Extend Connectable directed report type |
| 0x06 | Extend Scannable undirected report type |
| 0x07 | Extend Non-Connectable and Non-Scannable undirected report type |
| 0x08 | Extend Connectable undirected report type |
| 0x09 | Extend Scannable directed report type |
| 0x0A | Extend Non-Connectable and Non-Scannable directed report type |
| 0x0B | Eextend Scan Response report type |

| ID | GAP Address type |
|--- |--- |
| 0x00 | Public address |
| 0x01 | Static address |
| 0x02 | Generate Non-Resolvable Private Address |
| 0x03 | Generate Resolvable Private Address |

| ID | Primary/Secondary PHY |
| 0x01 | 1M |
| 0x02 | 2M |
| 0x03 | Coded |

## Демонстрационный adv2eth.py

Производит соединение с устройством WCHBLE2ETH и распечатывает приемный поток.

Параметром задается IP адрес или URL устройства WCHBLE2ETH в сети.

Пример:

```
python3 adv2eth.py 192.168.2.134
```

Лог:
```
Press 'ESC' to exit
Connecting to 192.168.2.134 ...
00 11 bb a4c138565870 02010612161a1870585638c1a42d0663125c091cab0e
00 11 b6 a4c138565870 02010612161a1870585638c1a42d0663125c091cab0e
00 11 ac 1c90ffdc0cc6 020106030201a2141601a201d3496059d9146eded9e4d956127b7b6f
23 11 cc 1b18fbd1b5d6 1eff0600010920068c32d352e9e632c900aef72af1f0e7d8ebb185b080ed38
00 11 b7 381f8dd93b3a 020106030201a2141601a201ca55525ec10fca3811cab285e0b6b66c
32 11 cb 60419a85a1b6 0303f3fe
08 33 af a4c138ae3ebf 0201060e16d2fc40002c01470202f70393140b095448535f414533454246
00 11 d0 381f8dd93cb6 020106030201a2141601a2016db2d52cd37c86fa958e84fb795ead4e
...
```


## Сборка проекта

Для сборки проекта используйте импорт в [MounRiver Studio](http://mounriver.com).
