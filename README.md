# Minstrel API + Minstrel-RCD

**Provide a user-space API to perform fine-grained rate and tx power control for IEEE 802.11 networks**

- [Minstrel API](#rate-control-api-aka-minstrel-api)
- [Minstrel-RCD](#minstrel-rcd)


## Rate Control API (aka Minstrel API)

The rate control API enables to access information and control parameters of kernel space rate-control from the user space. In addition to enabling monitoring of status information, the API allows to execute routines to perform rate setting based on an algorithm implemented in user space. The API is designed for WiFi systems running a Linux kernel in general. However, our testing only includes OpenWrt-based WiFi Systems.

Felix Fietkau is the maintainer of the API.

Latest version of OpenWrt with the patches related to the Rate Control API is available at

~~https://git.openwrt.org/?p=openwrt/staging/nbd.git;a=commit;h=f565e743c2ce88065b3da245a734fc50dd21d9a8~~

The API's core components are:

Based on OpenWrt Linux kernel patches and patchset to enable in OpenWrt that exports Python-based package for
 - Monitoring of status through TX status (`txs`), Received Signal Strength Indicator (`rxs`), and Rate control statistics (`stats`) information of one or more access points in a network.
 - Rate and TX power control through setting of appropriate MRR chain per client/station per access point.
 - Data collection and management from network measurements.

### Interface

The API provides a debugfs + relayfs based interface to pass information to and accept commands from user space. This interface consists of three files which are created for each
wifi device in `/sys/kernel/debug/ieee80211/<phy>/rc/`:
|File|Technique|Purpose|
|:---|:--------|:------|
|`api_info`|debugfs| |(Read-only) Upon read, prints static information about the API, like rate definitions, and per-phy information, including specific hardware capabilities, virtual interfaces, etc. Output is in CSV format.
|`api_event`|relayfs|(Read-only) Continuously exposes monitoring information like txs and rxs traces, depending on what has been enabled before.|
|`api_control`|debugfs|(Write-only) Upon write, a supported command is parsed and executed. The available commands are listed below.|

### API information

Example output of `api_info`:
```
*;0;#group;index;offset;type;nss;bw;gi;airtime0;airtime1;airtime2;airtime3;airtime4;airtime5;airtime6;airtime7;airtime8;airtime9
*;0;#sta;action;macaddr;overhead_mcs;overhead_legacy;mcs0;mcs1;mcs2;mcs3;mcs4;mcs5;mcs6;mcs7;mcs8;mcs9;mcs10;mcs11;mcs12;mcs13;mcs14;mcs15;mcs16;mcs17;mcs18;mcs19;mcs20;mcs21;mcs22;mcs23;mcs24;mcs25;mcs26;mcs27;mcs28;mcs29;mcs30;mcs31;mcs32;mcs33;mcs34;mcs35;mcs36;mcs37;mcs38;mcs39;mcs40;mcs41
*;0;#txs;macaddr;num_frames;num_acked;probe;rate0;count0;rate1;count1;rate2;count2;rate3;count3
*;0;#rxs;macaddr;last_signal;signal0;signal1;signal2;signal3
*;0;#stats;macaddr;rate;avg_prob;avg_tp;cur_success;cur_attempts;hist_success;hist_attempts
*;0;#best_rates;macaddr;maxtp0;maxtp1;maxtp2;maxtp3;maxprob
*;0;#sample_rates;macaddr;inc0;inc1;inc2;inc3;inc4;jump0;jump1;jump2;jump3;jump4;slow0;slow1;slow2;slow3;slow4
*;0;#reset_stats;macaddr
*;0;#rates;macaddr;rates;counts
*;0;#probe;macaddr;rate
*;0;#sample_table;cols;rows;column0;column1;column2;column3;column4;column5;column6;column7;column8;column9
*;0;group;0;0;ht;1;0;0;168980;b44c0;783c0;5a260;3c1e0;2d1a0;28180;24120;;
*;0;group;1;10;ht;2;0;0;b44c0;5a260;3c1e0;2d1a0;1e170;16950;14140;12110;;
*;0;group;2;20;ht;3;0;0;783d0;3c1e8;28198;1e170;14148;f130;d5d8;c060;;
*;0;group;3;30;ht;4;0;0;5a260;2d1a8;1e170;16950;f130;b4a8;a120;9088;;
*;0;group;4;40;ht;1;0;1;1448c0;a2460;6c3a0;51240;361e0;289a0;241a0;207a0;;
*;0;group;5;50;ht;2;0;1;a2470;51250;361e0;289b0;1b170;14560;12150;10450;;
*;0;group;6;60;ht;3;0;1;6c3a0;361e8;241a0;1b178;12158;d948;c0a8;ad50;;
*;0;group;7;70;ht;4;0;1;51250;289b0;1b178;14560;d948;a2c8;9130;8240;;
*;0;group;8;80;ht;1;1;0;ada50;56da0;39ec0;2b750;1cfd0;15ba0;13590;11650;;
*;0;group;9;90;ht;2;1;0;56da0;2b750;1cfd8;15ba8;e868;add0;9b40;8ba0;;
*;0;group;a;a0;ht;3;1;0;39ec0;1cfdc;13590;e86c;9b44;7434;6784;5cc4;;
*;0;group;b;b0;ht;4;1;0;2b750;15ba8;e86c;add4;7434;56e8;4e20;4650;;
*;0;group;c;c0;ht;1;1;1;9c4a0;4e2e0;34240;271f0;1a1a0;13910;116c0;faa0;;
*;0;group;d;d0;ht;2;1;1;4e2e0;271f8;1a1a8;13910;d160;9ca0;8bf0;7de0;;
*;0;group;e;e0;ht;3;1;1;34244;1a1ac;116cc;d160;8bf0;68c8;5d5c;53b0;;
*;0;group;f;f0;ht;4;1;1;271f8;13914;d160;9ca4;68c8;4e68;4680;3f78;;
*;0;group;10;100;cck;1;0;0;960e00;4c9100;1dcc00;106f00;949700;4b1a00;1c5500;ef800;;
*;0;group;11;110;ofdm;1;0;0;190640;10d880;cc1a0;8aac0;6a720;493e0;399e0;33c20;;
*;0;group;12;120;vht;1;0;0;168980;b44c0;783c0;5a260;3c1e0;2d1a0;28180;24120;1e160;1b180
*;0;group;13;130;vht;2;0;0;b44c0;5a260;3c1e0;2d1a0;1e170;16950;14140;12110;f130;d8c0
*;0;group;14;140;vht;3;0;0;783d0;3c1e8;28198;1e170;14148;f130;d5d8;c060;a120;9088
*;0;group;15;150;vht;4;0;0;5a260;2d1a8;1e170;16950;f130;b4a8;a120;9088;7918;6c60
*;0;group;16;160;vht;1;0;1;1448c0;a2460;6c3a0;51240;361e0;289a0;241a0;207a0;1b160;18660
*;0;group;17;170;vht;2;0;1;a2470;51250;361e0;289b0;1b170;14560;12150;10450;d940;c350
*;0;group;18;180;vht;3;0;1;6c3a0;361e8;241a0;1b178;12158;d948;c0a8;ad50;9130;8240
*;0;group;19;190;vht;4;0;1;51250;289b0;1b178;14560;d948;a2c8;9130;8240;6d28;61c0
*;0;group;1a;1a0;vht;1;1;0;ada50;56da0;39ec0;2b750;1cfd0;15ba0;13590;11650;e860;d0f0
*;0;group;1b;1b0;vht;2;1;0;56da0;2b750;1cfd8;15ba8;e868;add0;9b40;8ba0;7430;6878
*;0;group;1c;1c0;vht;3;1;0;39ec0;1cfdc;13590;e86c;9b44;7434;6784;5cc4;4e20;4650
*;0;group;1d;1d0;vht;4;1;0;2b750;15ba8;e86c;add4;7434;56e8;4e20;4650;3a98;34bc
*;0;group;1e;1e0;vht;1;1;1;9c4a0;4e2e0;34240;271f0;1a1a0;13910;116c0;faa0;d160;bc40
*;0;group;1f;1f0;vht;2;1;1;4e2e0;271f8;1a1a8;13910;d160;9ca0;8bf0;7de0;68c8;5e38
*;0;group;20;200;vht;3;1;1;34244;1a1ac;116cc;d160;8bf0;68c8;5d5c;53b0;4680;3f78
*;0;group;21;210;vht;4;1;1;271f8;13914;d160;9ca4;68c8;4e68;4680;3f78;34ec;2fa8
*;0;group;22;220;vht;1;2;0;50238;28198;1abb8;14148;d5d8;a120;8e90;80e8;6b68;60a8
*;0;group;23;230;vht;2;2;0;28198;14148;d5dc;a120;6b6c;510c;4748;4074;35b4;30d4
*;0;group;24;240;vht;3;2;0;1abbc;d5de;8e94;6b6c;474a;35b6;2fda;2af8;2422;203a
*;0;group;25;250;vht;4;2;0;1414a;a122;6b6c;510e;35b6;2904;2422;203a;1b58;186a
*;0;group;26;260;vht;1;2;1;48230;241a0;18128;12158;c0a8;9130;8080;7430;60e0;5730
*;0;group;27;270;vht;2;2;1;241a0;12158;c0ac;9134;60e0;4924;4058;3a34;3088;2c24
*;0;group;28;280;vht;3;2;1;18128;c0ac;8084;60e0;405a;3088;2b42;26de;20b6;1d32
*;0;group;29;290;vht;4;2;1;1215a;9136;60e0;4924;3088;251c;20b6;1d32;18ce;162a
*;0;sample_table;a;a;6,8,9,2,1,5,3,4,0,7;8,9,0,2,4,6,1,7,3,5;1,4,5,3,7,9,2,0,6,8;7,8,9,6,3,1,0,4,2,5;8,6,9,0,3,2,4,1,5,7;6,8,9,0,1,4,7,3,5,2;0,1,6,7,3,4,8,9,5,2;4,5,6,2,1,7,8,9,3,0;6,8,0,1,7,9,4,2,3,5;5,0,1,6,8,7,9,4,3,2
```

All lines starting with `*` describe global, mostly static information about Minstrel-HT and the API. In the beginning, lines are printed starting with `*;0;#` which should be considered as meta-information and denote the specific format for each output or command line that is used. After this, the global, static rate definitions of Minstrel-HT are printed.
Minstrel-HT internally uses a rate representation that differs from the default MCS index + further parameters by separating all rates into groups with continuous indices.

### Commands

Here, commands which are supported and can be run through `api_control`. All commands use the CSV format.   
*The examples in the table use phy0 for the WiFi device.*

|Function|Description|Kernel function|Command example|Additional information|
|:------:|:----------|:--------------|---------------|----------------------|
|dump    | Print out the supported data rate set for each client already connected - useful to separate tx_status packets that are supported by minstrel.|`minstrel_ht_dump_stations(mp)`|`phy0;dump`||
|start   | Enable live print outs of tx statuses of connected STAs.|`minstrel_ht_api_set_active(mp, true)`|`phy0;start`||
|start;txs   | Enable live print outs of tx statuses of connected STAs.|> ?|`phy0;start;txs`||
|start;rxs  | Enable live print outs of RSSI of connected STAs.|> ?|`phy0;start;rxs`||
|start;stats   |Enable live print outs of tx statuses of connected STAs.|> ?|`phy0;start;stats`||
|stop    | Disable live print outs of tx statuses of connected STAs.|`minstrel_ht_api_set_active(mp, false)`|`phy0;stop`||
|manual  | Disable minstrel-ht of kernel space and enable manual rate settings.|`minstrel_ht_api_set_manual(mp, true)`|`phy0;manual`||
|auto    | Enable minstrel-ht of kernel space.|`minstrel_ht_api_set_manual(mp, false)`|`phy0;auto`||
|rates   | Set rate table with given rates.|`minstrel_ht_set_rates(mp, mi, args[1], args[2], args[3])`|`phy0;{MAC address};{list of rate idxs};{num of counts for each rate};{list of tx-power idxs}`|`args[1]` = list of rates separated by `,`, `args[2]` = list of number of tries for a rate until choosing next rate, separated by `,` and `args[3]` = list of transmit power indices to be used for the MRR stages, separated by `,`.|
|probe   | Set rate to be probed for specific STA|`minstrel_ht_set_probe_rate(mp, mi, args[1])`|`phy0;probe;{MAC address};{rate_idx}`|`args[1]` = list of rates supported by STA and AP|

*Note: `mp` stands for an instance of the internal kernel structure minstrel_priv which is created per phy. `mi` stands for the internal kernel structure minstrel_ht_sta which is created per station (MAC address). Thus, functions are executed per phy/per station.*

### Monitoring tasks
For monitoring the status of a given access point, make sure the TCP/IP connection is established. The `txs` and/or `rcs` can be received by triggering the following example commands. These commands are handled over the radio interfaces of the access point which are typically denoted as `phy0`, `phy1` and so on. 


#### To trigger receiving the `txs`, run:
  ```
  phy1;start;txs
  ```
#### To trigger receiving the `txs`, run:
  ```
  phy1;start;rxs
  ```
#### To trigger receiving the `rcs`, run:
  ```
  phy1;start;stats
  ```
#### To trigger receiving multiple functions together run:
  ```
  phy1;start;stats;txs
  ```
_Note: Upon triggering this command, trace lines for `txs` and `rcs` will be printed separately. You can include combinations of the three available functions -  `txs`, `rxs`, and `stats`.


### Monitoring information format
Once the monitoring tasks have been triggered as above, the trace lines are received/printed in the Terminal T2. Format of these lines is as follows,

#### Format of trace for `txs` information
```
phyID;hex_timestamp_nanosec;txs;macaddr;num_frames;num_acked;probe;rate0;count0;rate1;count1;rate2;count2;rate3;count3
```

|Field|Description|
|:------|:----------|
|`phyID`| Radio ID, e.g. `phy0`.|
|`hex_timestamp_nanosec`| Timestamp for system time (Unix time) in nanoseconds in hex format.|
|`txs`| Denotes that the traces is for TX status.|
|`macaddr`| MAC address of the station/client for which trace is received.|
|`num_frames`| Number of data packets in a given TX frame.|
|`num_acked`| Number of data packets of a frame which were successfully transmitted for which an `ACK` has been received.|
|`probe`| Binary index for type of frame. If `probe` = 1 for probing frame, 0 otherwise. |
|`rate0;count0;txpwr0`| 1st MCS rate (`rate0`) chosen for probing or data frame with `count0` attempts/tries and `txpwr0` transmit power index.|
|`rate1;count1;txpwr1`| 2nd MCS rate (`rate1`) chosen for probing or data frame with `count1` attempts/tries and `txpwr1` transmit power index.|
|`rate2;count2;txpwr2`| 3rd MCS rate (`rate2`) chosen for probing or data frame with `count2` attempts/tries and `txpwr2` transmit power index.|
|`rate3;count3;txpwr3`| 4th MCS rate (`rate3`) chosen for probing or data frame with `count3` attempts/tries and `txpwr3` transmit power index.|

_Note: In the rate table containing upto four rates and corresponding counts, if a sequential rate-count-txpwr is not used, the rate and tx-power fields are denoted by `ffff`.

E.g. 1. Successful transmission on 1st MCS rate
```
phy0;16c4added930f1b4;txs;cc:32:e5:9d:ab:58;3;3;0;d7;1;28;ffff;0;ffff;ffff;0;ffff;ffff;0;ffff
```
Here we have a trace from `phy0` at timestamp, `1626196159.112026795`, for client with the MAC address of `cc:32:e5:9d:ab:58`, with `num_frames = 3`, `num_acked = 3`, `probe = 0` denotes that it was not a probing frame, index of 1st MCS rate tried `rate0` is `d7`, number of transmission tries for `rate0` was `count0 = 1` and a tx-power idx of `28` was used. In this case only one MCS rate was tried and successfully used. 

E.g. 2. Successful transmission on 2nd MCS rate 
```
phy1;16c4added930f1b4;txs;d4:a3:3d:5f:76:4a;1;1;1;266;2;1f;272;1;21;ffff;0;ffff;ffff;0;ffff
```
Here we have a trace from `phy1` at timestamp `1626189830.926593008` for a client with the MAC address `d4:a3:3d:5f:76:4a`, with `num_frames = 1`, `num_acked = 1`, `probe = 1` denotes that it was a probing frame, index of 1st MCS rate tried `rate0` is `266`, number of transmission tries for `rate0` was `count0 = 2` and a tx-power index of `1f` was used. In this case the `rate0` was not successful and hence a 2nd MCS rate with index `rate1` of `272` was tried `count1 = 1` times with a tx-pwoer index of `21` and this transmission was successful.

E.g. 3. Erroneous `txs` trace
```
phy1;16c4added930f1b4;txs;86:f9:1e:47:68:da;2;0;0;ffff;0;ffff;ffff;0;ffff;ffff;0;ffff;ffff;0;ffff
```
In this case, the trace implies that no MCS rate has been tried.

#### How to Read the `rateX` fields
Consider again the example from the previous section:
```
phy1;16c4added930f1b4;txs;d4:a3:3d:5f:76:4a;1;1;1;266,2;272;1;ffff;0;ffff;0
```
The first digits of `rateX` tell us in which rate group to look. The rightmost digit from the rate entries gives us the group offset. *Note, that these are hex digits!*
In our example, rate `266` refers to the `6`th rate from group `26` and `272` refers to the `2`nd rate from group `27`. Looking at the `group` output mentioned above, we can find the exact rates. What `minstrel-rcd` is telling us is that we first tried to send a frame at rate **TODO RATE** twice before falling back to rate **TODO RATE** where transmission succeeded after one attempt.

#### How to Read the `txpwrX` fields

Keep in mind that the values are not absolute values in dBm, and the values are in HEX. The range and meaning of the values depends on what the driver defines for a WiFi device. Thus, they should just be considered as abstract values. For example, ath9k defines power levels from 0 to 63, idx 0 corresponds to 0 dBm and the power levels have a value-distance of 0.5 dBm. Information about the power levels / ranges is produced by Minstrel-RCD upon connecting, see [below](#minstrel-rcd-information) for a detailed explanation.

#### Format of trace for `stats` information
```
phyID;<timestamp>;stats;<macaddr>;<rate>;<avg_prob>;<avg_tp>;<cur_success>;<cur_attempts>;<hist_success>;<hist_attempts>
```

|Field|Description|
|:------|:----------|
|`stats`| Denotes that the trace signals a Rate Control Statistics update, in particular an update of the statistics for one rate.|
|`<macaddr>`| MAC address of the station/client for which trace is received.|
|`<rate>`| The idx of the rate whose statistics were updated.|
|`<avg_prob>`| Average success probability of the rate. |
|`<avg_tp>`| Average estimated throughput of the rate. |
|`<cur_success>`| Number of successes in the current interval. |
|`<cur_attempts>`| Number of attempts in the current interval. |
|`<hist_success>`| Number of successes in the last interval. |
|`<hist_attempts>`| Number of attempts in the last interval. |

E.g. 1. 
```
phy1;17503da1e84dea50;stats;04:f0:21:26:d9:25;c4;3e8;1a2;1;1;3f9;400
```

> TODO: Explain example

#### Format of trace for `best_rates` information

```
phy1;<timestamp>;best_rates;<macaddr>;<maxtp0>;<maxtp1>;<maxtp2>;<maxtp3>;<maxprob>
```

|Field|Description|
|:------|:----------|
|`best_rates`| Denotes that the trace contains current best rate selection.|
|`<macaddr>`| MAC address of the station/client for which trace is received.|
|`<maxtp0>`| Rate with highest estimated throughput. |
|`<maxtp1>`| Rate with second highest estimated throughput. |
|`<maxtp2>`| Rate with third highest estimated throughput. |
|`<maxtp3>`| Rate with fourth highest estimated throughput. |
|`<maxprob>`| Rate with highest success probability. |

phy1;17503da1e84ec73d;best_rates;04:f0:21:26:d9:25;94;93;c4;92;c4

> TODO: Explain example
  
### How to set MRR chain?

Upon establishing a TCP/IP connection with the rate control API in T2, you can use the following steps to perform rate setting
  
  1. Enable rate control API for a given radio using the command:
  ```
  phy1;start
  ```
  This command enables a continuous string of TX_Status.
       
  2. Enable manual rate setting using the command:
  ```
  phy1;manual
  ```
  This command also disables the default rate control algorithm, in our case Minstrel-HT.
    
  3. Set desired MCS rate using the command format:
  ```
  phy1;rates;<macaddr>;<rates>;<counts>;<txpower>
  ```    
  Actual rate setting is done using the `rates` argument in the second position. Note that the `rates` to be set must be the HEX version of the rate `idx` found in the `rc_stats` table.
     

### Rate Control Statistics

![alt tag](https://user-images.githubusercontent.com/79704080/112141900-385fd980-8bd6-11eb-99a2-5c18ff8e37e5.PNG)

> TODO: Add description of variables in table
     
    
## Minstrel-RCD

The Minstrel-API exports information and control only locally via debugfs. However, to perform realistic, valid network experiments, remote access to the API is usually desired or necessary. Minstrel-RCD - Minstrel Remote-Control-Daemon - closes this gap by providing the interface on a network port, allowing multiple clients to connect and use the API without the need for local access.

Note that, e.g. the RateMan package utilizes the Minstrel-RCD to perform it's functions and thus has little or no capabilities without the API + Minstrel-RCD.

In this section, we will cover the basic details of the `minstrel-rcd`.

### How to setup a connection to `minstrel-rcd`?

In this subsection, we provide a list of steps to remotely communicate with the rate control API, monitor default rate control algorithm and perform MCS rate setting on your router. In this example, the router IP address is 10.10.200.2

  1. In terminal T1, connect to router via a SSH connection
  ```
  ssh root@10.10.200.2
  ```
  
  2. In terminal T1, enable `minstrel-rcd`. This opens a connection for other programmes to use the rate control API.
  ```
  minstrel-rcd -h 0.0.0.0 &
  ``` 
  We can use this connection to access directories containing specific information relevant to rate control.
  
  3. In another terminal (T2), start a TCP/IP connection via tool like netcat to communicate with the rate control API via minstrel-rcd. Minstrel-RCD operates over a designated port, in our case it is 21059.
  ```
  nc 10.10.200.2 21059
  ```
  On connection, the API will print all possible command options followed by a list of MCS gups available for the given router. 

### Minstrel-RCD information

Upon establishing a connection to Minstrel-RCD, the `api_info` is read and printed. Minstrel-RCD reads this for one WiFi device and adds further lines for each WiFi device, containing additional, radio-specific information like interfaces, driver name and TPC capabilities. The information is currently collected from various debugfs files. These lines look like the following:
```
<phy>;<ts>;<type>;<driver>;<vifs>;<tpc_type>;<tpc_ranges>;<tpc_range_block>
```
Example: `phy1;0;add;ath9k;phy1-ap0;mrr;1;0,40,0,2`

|Field|Explanation|
|:----|:----------|
|`<phy>`|ID/name of the WiFi device|
|`<ts>`|Timestamp, for initial Minstrel-RCD generated lines always `0`.|
|`<type>`|Denotes that a WiFi device action occured. When connecting to Minstrel-RCD, WiFi devices are always added, thus this will be `add`|
|`<driver>`|Name of the driver that is assigned to the WiFi device.|
|`<vifs>`|List of virtual interfaces assigned to the WiFi device, separated by `;`.|
|`<tpc_type>`, `pkt` or `mrr`|`not`, `pkt` or `mrr`. Denotes the type of TPC support, respectively 'no support', 'tpc per packet' or 'tpc per mrr stage'.|
|`<tpc_ranges>`|Number of following TPC range blocks.|
|`<tpc_range_block>`|One or multiple range blocks describing the different TPC power ranges that are supported. A range consists of the four values `start_idx`, `n_levels`, `start_pwr` and `pwr_step` (all HEX), separated by `,`. Example: `0,40,0,2` describes a power range starting at idx 0 with 64 levels, the power level at idx 0 corresponds to (0 * 0.25) dBm and the range has a step width of (2 * 0.25 = 0.5) dBm.|

### TPC in the Linux kernel

Our TPC patches against the Linux kernel for fine-grained TPC use indexed TX-power levels instead of absolute dBm values to annotate transmit power. This has several advantages, most notably better support for different capabilities. When a driver registers a new WiFi device at the mac80211 layer, it must specify the type of TPC support and the power levels it supports. The specific power levels are specified by so-called tx-power ranges. A range describes a discrete, sequentially indexed set of power levels with a fixed value-distance. To support special capabilities such as (0..10 dBm in 0.5 dBm steps) + (10..20 dBm in 1 dBm steps), drivers can specify multiple power ranges. Every tx-power value that is used for fine-grained TPC is always such a tx-power index pointing into the value set defined by those ranges.
Although a valid tx-power index is always `>= 0`, a value of `-1` can be specified to give the decision about the tx-power to the driver. For example, ath9k uses the maximum allowed tx-power when `-1` is specified.
