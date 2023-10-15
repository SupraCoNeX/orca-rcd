# ORCA-RCD

The Minstrel-API exports information and control only locally via debugfs/relayfs. However, to perform realistic, valid network experiments, remote access to the API is usually desired or necessary. ORCA-RCD - Minstrel Remote-Control-Daemon - closes this gap by providing the interface on a network port, allowing multiple clients to connect and use the API without the need for local access.

*Note that, e.g. the RateMan package relies on ORCA-RCD to perform it's functions and thus has little or no capabilities without the API + ORCA-RCD.*

## `orca-rcd` features and behaviour

> TO BE EXTENDED

### Compression

`orca-rcd` by default serves plain API access via a TCP socket at port `21059` (P1). Due to the fact that the API may produce a high amount of traces depending on the network traffic that is monitored, this may lead to a high amount of monitoring traffic caused by the API and `orca-rcd`. Thus, `orca-rcd` also provides its output in zstd-compressed format at an additional TCP socket with port `P1 + 1` which is by default port `21060`. 

### Security

`orca-rcd` currently does not implement any kind of secured access control or encryption. Thus, the opened TCP ports can just be captured without further authentication, and the traffic is plain, not encrypted. However, this can be easily circumvented by using a VPN like Wireguard, or some firewall rules. Encryption may also be implemented in `orca-rcd` in the future.

## Differences between raw API output and output coming through `orca-rcd`

**`orca-rcd` runs locally on a target device and multiplexes the API in- and output for all existing PHYs. Thus, output captured through `orca-rcd` is always slightly different than the output captured directly from `api_info`, `api_phy` and `api_event`. The same applies to commands that are issued via `orca-rcd` versus commands that are directly written into a PHY's `api_control`.**   
To be able to differ between different PHYs, `orca-rcd` prepends additional information to each line that is coming from the API. In the other direction, analogous information must be added to each command sent through `orca-rcd`.

For example, while the following line coming from the API looks like:
```
16c4added930f1b4;txs;d4:a3:3d:5f:76:4a;1;1;1;266,2,1f;272,1,21;,,;,,
```
the same line passing through orca-rcd would look like (in case it is associated to PHY phy0):
```
phy0;16c4added930f1b4;txs;d4:a3:3d:5f:76:4a;1;1;1;266,2,1f;272,1,21;,,;,,
```
Thus, `orca-rcd` always prepends the name/ID of the corresponding PHY before forwarding the output to its clients. This also applies to the static information that `orca-rcd` reads from `api_info` and forwards to its clients. This keeps the output format of all lines consistent to be easily parsed and processed. Taking a line of the raw `api_info` output which looks like:
```
#start;txs;rxs;stats;sta;tprc_echo
```
`orca-rcd` will prepend `*;0;` to this line so it looks like:
```
*;0;#start;txs;rxs;stats;sta;tprc_echo
```
In detail, `*;0` is analogous to `phy0;16c4added930f1b4` and means, that the line belongs to all PHYs (`*` is wildcard) and the timestamp is set to 0 as is has no relevant meaning for such lines.

As mentioned before, the other direction (commands) also requires this information to ensure, that `orca-rcd` properly delegates the commands to the API endpoint of the correct PHY.
The command for setting an MRR chain when writing directly to `api_control` looks like:
```
set_rates_power;aa:bb:cc:dd:ee:ff;d7,4,a;d2,4,c;c1,4,1f
```
but in case this command should be executed for PHY phy0, this information must be prepended like:
```
phy0;set_rates_power;aa:bb:cc:dd:ee:ff;d7,4,a;d2,4,c;c1,4,1f
```

## PHY-specific capabilities/information produced by `orca-rcd`

Upon establishing a connection to ORCA-RCD, the `api_info` is read and printed. However, this static output only contains global information and thus, `orca-rcd` reads this for only one WiFi device. After this, `orca-rcd` reads `api_phy` for each PHY and passes the contained information in a condensed format to its clients. The format syntax is as follows:
```
<phy>;<timestamp>;<type>;<driver>;<vifs>;<active_mon>;<num_ftrs>;<ftrs>;<tpc_caps>;<max_tpc>
```
Example: `phy1;0;add;ath9k;phy1-ap0,phy1-sta0;mrr;1;0,40,0,2`

|Field|Explanation|
|:----|:----------|
|`<phy>`|ID/name of the WiFi device|
|`<timestamp>`|Timestamp, for initial ORCA-RCD generated lines always `0`.|
|`<type>`|Denotes that a WiFi device action occured. When connecting to ORCA-RCD, WiFi devices are always added, thus this will be `add`|
|`<driver>`|Name of the driver that is assigned to the WiFi device.|
|`<vifs>`|List of virtual interfaces assigned to the WiFi device, separated by `;`.|
|`<active_mon>`| Comma-separated list of active monitor modes. |
|`<num_ftrs>`| Number of following feature blocks. |
|`<ftrs>`| `<num_ftrs>` feature blocks showing the supported features and their current states. Each feature block has the format `<ftr>,<state>` where `ftr` is the feature identifier and `state` the numeric state of the feature.|
|`<tpc_caps>`| TPC capabilities as described in [ORCA `api_phy` output](https://github.com/SupraCoNeX/orca#api_phy---phy-specific-api-info) |
|`<max_tpc>`| The maximum power index (refering to `tpc_caps`) that can be set via the TPC feature. |

## How to setup a connection to `orca-rcd`?

In this example, the router IP address is 10.10.200.2

  1. In a terminal (T1), connect to your device via an SSH connection
  ```
  ssh root@10.10.200.2
  ```
  
  2. In T1, enable `orca-rcd`. This opens a connection for other programmes to use the rate control API. This can be done once with
  ```
  orca-rcd -h 0.0.0.0 &
  ``` 
  or startup at system boot can be enabled. In this case, `orca-rcd` always starts as a daemon at system startup. For OpenWrt systems, this can be set in `/etc/config/orca-rcd` config file.
  
  We can use this connection to access directories containing specific information relevant to rate control.
  
  3. In another terminal (T2), start a TCP/IP connection via a tool like `netcat` to communicate with the API via `orca-rcd`. It operates over a designated port, in our case it is 21059.
  ```
  ncat 10.10.200.2 21059
  ```
  Upon connection, `orca-rcd` will proceed
