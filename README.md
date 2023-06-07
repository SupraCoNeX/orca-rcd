# ORCA + Minstrel-RCD

**Provide a user-space API to perform fine-grained rate and TX power control for IEEE 802.11 networks**

- [Open-Source Resource Control API - ORCA](#open-source-resource-control-api-orca)
- [Minstrel-RCD](#minstrel-rcd)


# Open-Source Resource Control API (ORCA)

The rate control API enables to access information and control parameters of kernel space rate-control from the user space. In addition to enabling monitoring of status information, the API allows to execute routines to perform rate setting based on an algorithm implemented in user space. The API is designed for WiFi systems running a Linux kernel in general. However, our testing only includes OpenWrt-based WiFi Systems.

Felix Fietkau is the maintainer of the API.

Latest version of OpenWrt with the patches related to the Rate Control API is available at

~~https://git.openwrt.org/?p=openwrt/staging/nbd.git;a=commit;h=f565e743c2ce88065b3da245a734fc50dd21d9a8~~

The API's core components are:

Based on OpenWrt Linux kernel patches and patchset to enable in OpenWrt that exports Python-based package for
 - Monitoring of status through TX status (`txs`), Received Signal Strength Indicator (`rxs`), and Rate control statistics (`stats`) information of one or more access points in a network.
 - Rate and TX power control through setting of appropriate MRR chain per client/station per access point.
 - Data collection and management of network measurements.

## Interface

The API provides a debugfs + relayfs based interface to pass information to and accept commands from user space. This interface consists of four files which are created for each
wifi device in `/sys/kernel/debug/ieee80211/<phy>/rc/`:
| File         | Technique | Purpose      |
|:-------------|:----------|:-------------|
| [`api_info`](#api_info---static-api-information) | debugfs | *\[read-only\]* Upon read, prints static information about the API, like rate definitions, and per-phy information, including specific hardware capabilities, virtual interfaces, etc. Output is in CSV format. |
| [`api_control`](#api_control---commands)         | debugfs | *\[write-only\]* Upon write, a supported command is parsed and executed. The available commands are listed below.|
| [`api_event`](api_event---monitoring-tasks)      | relayfs | *\[read-only\]* Continuously exposes monitoring information like txs and rxs traces, depending on what has been enabled before.|
| [`api_phy`](#api_phy---phy-specific-api-info)    | debugfs | *\[read-only\]* Exposes phy-specific information including driver name, ifaces, and TPC capabilities.|

**All API outputs and commands use a CSV-like format**

## `api_info` - static API information

Example output of `api_info` for a PHY (`/sys/kernel/debug/ieee80211/<phy>/rc/api_info`):
```
orca_version;1
#group;index;offset;type;nss;bw;gi;airtime0;airtime1;airtime2;airtime3;airtime4;airtime5;airtime6;airtime7;airtime8;airtime9
#sta;action;macaddr;iface;rc_mode;tpc_mode;overhead_mcs;overhead_legacy;mcs0;mcs1;mcs2;mcs3;mcs4;mcs5;mcs6;mcs7;mcs8;mcs9;mcs10;mcs11;mcs12;mcs13;mcs14;mcs15;mcs16;mcs17;mcs18;mcs19;mcs20;mcs21;mcs22;mcs23;mcs24;mcs25;mcs26;mcs27;mcs28;mcs29;mcs30;mcs31;mcs32;mcs33;mcs34;mcs35;mcs36;mcs37;mcs38;mcs39;mcs40;mcs41
#txs;macaddr;num_frames;num_acked;probe;rate0,count0,txpwr0;rate1,count1,txpwr1;rate2,count2,txpwr2;rate3,count3,txpwr3
#rxs;macaddr;last_signal;signal0;signal1;signal2;signal3
#stats;macaddr;rate;avg_prob;avg_tp;cur_success;cur_attempts;hist_success;hist_attempts
#best_rates;macaddr;maxtp0;maxtp1;maxtp2;maxtp3;maxprob
#sample_rates;macaddr;inc0;inc1;inc2;inc3;inc4;jump0;jump1;jump2;jump3;jump4;slow0;slow1;slow2;slow3;slow4
#sample_table;cols;rows;column0;column1;column2;column3;column4;column5;column6;column7;column8;column9
#start;txs;rxs;stats;sta;tprc_echo
#stop;txs;rxs;stats;sta;tprc_echo
#set_rates;macaddr;rate0,count0;rate1,count1;rate2,count2;rate3,count3
#set_power;macaddr;txpwr0;txpwr1;txpwr2;txpwr3
#set_rates_power;macaddr;rate0,count0,txpwr0;rate1,count1,txpwr1;rate2,count2,txpwr2;rate3,count3,txpwr3
#set_probe;macaddr;rate,count,txpwr
#rc_mode;macaddr;mode
#rc_mode;all;mode
#tpc_mode;macaddr;mode
#tpc_mode;all;mode
#reset_stats;macaddr
#reset_stats;all
group;0;0;ht;1;0;0;168980;b44c0;783c0;5a260;3c1e0;2d1a0;28180;24120;;
group;1;10;ht;2;0;0;b44c0;5a260;3c1e0;2d1a0;1e170;16950;14140;12110;;
group;2;20;ht;3;0;0;783d0;3c1e8;28198;1e170;14148;f130;d5d8;c060;;
group;3;30;ht;4;0;0;5a260;2d1a8;1e170;16950;f130;b4a8;a120;9088;;
group;4;40;ht;1;0;1;1448c0;a2460;6c3a0;51240;361e0;289a0;241a0;207a0;;
group;5;50;ht;2;0;1;a2470;51250;361e0;289b0;1b170;14560;12150;10450;;
group;6;60;ht;3;0;1;6c3a0;361e8;241a0;1b178;12158;d948;c0a8;ad50;;
group;7;70;ht;4;0;1;51250;289b0;1b178;14560;d948;a2c8;9130;8240;;
group;8;80;ht;1;1;0;ada50;56da0;39ec0;2b750;1cfd0;15ba0;13590;11650;;
group;9;90;ht;2;1;0;56da0;2b750;1cfd8;15ba8;e868;add0;9b40;8ba0;;
group;a;a0;ht;3;1;0;39ec0;1cfdc;13590;e86c;9b44;7434;6784;5cc4;;
group;b;b0;ht;4;1;0;2b750;15ba8;e86c;add4;7434;56e8;4e20;4650;;
group;c;c0;ht;1;1;1;9c4a0;4e2e0;34240;271f0;1a1a0;13910;116c0;faa0;;
group;d;d0;ht;2;1;1;4e2e0;271f8;1a1a8;13910;d160;9ca0;8bf0;7de0;;
group;e;e0;ht;3;1;1;34244;1a1ac;116cc;d160;8bf0;68c8;5d5c;53b0;;
group;f;f0;ht;4;1;1;271f8;13914;d160;9ca4;68c8;4e68;4680;3f78;;
group;10;100;cck;1;0;0;960e00;4c9100;1dcc00;106f00;949700;4b1a00;1c5500;ef800;;
group;11;110;ofdm;1;0;0;190640;10d880;cc1a0;8aac0;6a720;493e0;399e0;33c20;;
group;12;120;vht;1;0;0;168980;b44c0;783c0;5a260;3c1e0;2d1a0;28180;24120;1e160;1b180
group;13;130;vht;2;0;0;b44c0;5a260;3c1e0;2d1a0;1e170;16950;14140;12110;f130;d8c0
group;14;140;vht;3;0;0;783d0;3c1e8;28198;1e170;14148;f130;d5d8;c060;a120;9088
group;15;150;vht;4;0;0;5a260;2d1a8;1e170;16950;f130;b4a8;a120;9088;7918;6c60
group;16;160;vht;1;0;1;1448c0;a2460;6c3a0;51240;361e0;289a0;241a0;207a0;1b160;18660
group;17;170;vht;2;0;1;a2470;51250;361e0;289b0;1b170;14560;12150;10450;d940;c350
group;18;180;vht;3;0;1;6c3a0;361e8;241a0;1b178;12158;d948;c0a8;ad50;9130;8240
group;19;190;vht;4;0;1;51250;289b0;1b178;14560;d948;a2c8;9130;8240;6d28;61c0
group;1a;1a0;vht;1;1;0;ada50;56da0;39ec0;2b750;1cfd0;15ba0;13590;11650;e860;d0f0
group;1b;1b0;vht;2;1;0;56da0;2b750;1cfd8;15ba8;e868;add0;9b40;8ba0;7430;6878
group;1c;1c0;vht;3;1;0;39ec0;1cfdc;13590;e86c;9b44;7434;6784;5cc4;4e20;4650
group;1d;1d0;vht;4;1;0;2b750;15ba8;e86c;add4;7434;56e8;4e20;4650;3a98;34bc
group;1e;1e0;vht;1;1;1;9c4a0;4e2e0;34240;271f0;1a1a0;13910;116c0;faa0;d160;bc40
group;1f;1f0;vht;2;1;1;4e2e0;271f8;1a1a8;13910;d160;9ca0;8bf0;7de0;68c8;5e38
group;20;200;vht;3;1;1;34244;1a1ac;116cc;d160;8bf0;68c8;5d5c;53b0;4680;3f78
group;21;210;vht;4;1;1;271f8;13914;d160;9ca4;68c8;4e68;4680;3f78;34ec;2fa8
group;22;220;vht;1;2;0;50238;28198;1abb8;14148;d5d8;a120;8e90;80e8;6b68;60a8
group;23;230;vht;2;2;0;28198;14148;d5dc;a120;6b6c;510c;4748;4074;35b4;30d4
group;24;240;vht;3;2;0;1abbc;d5de;8e94;6b6c;474a;35b6;2fda;2af8;2422;203a
group;25;250;vht;4;2;0;1414a;a122;6b6c;510e;35b6;2904;2422;203a;1b58;186a
group;26;260;vht;1;2;1;48230;241a0;18128;12158;c0a8;9130;8080;7430;60e0;5730
group;27;270;vht;2;2;1;241a0;12158;c0ac;9134;60e0;4924;4058;3a34;3088;2c24
group;28;280;vht;3;2;1;18128;c0ac;8084;60e0;405a;3088;2b42;26de;20b6;1d32
group;29;290;vht;4;2;1;1215a;9136;60e0;4924;3088;251c;20b6;1d32;18ce;162a
sample_table;a;a;6,8,9,2,1,5,3,4,0,7;8,9,0,2,4,6,1,7,3,5;1,4,5,3,7,9,2,0,6,8;7,8,9,6,3,1,0,4,2,5;8,6,9,0,3,2,4,1,5,7;6,8,9,0,1,4,7,3,5,2;0,1,6,7,3,4,8,9,5,2;4,5,6,2,1,7,8,9,3,0;6,8,0,1,7,9,4,2,3,5;5,0,1,6,8,7,9,4,3,2
```

**Lines starting with `#` are format lines. They describe the exact format of all output lines and commands.**
Minstrel-HT internally uses a rate representation that differs from the default MCS index + further parameters by separating all rates into groups with continuous indices.
The output of `api_info` is mostly the same for all PHYs, especially the format lines and all lines starting with `group`. These `group` lines expose the rate representation that Minstrel-HT uses internally and is fixed at compile-time.

## `api_control` - Commands

The API currently provides the following commands:
* [`dump`](#dump)
* [`start`](#start)
* [`stop`](#stop)
* [`rc_mode`](#rc_mode)
* [`tpc_mode`](#tpc_mode)
* [`set_rates`](#set_rates)
* [`set_power`](#set_power)
* [`set_rates_power`](#set_rates_power)
* [`set_probe`](#set_probe)
* [`reset_stats`](#reset_stats)

The commands are described in detail in the following subsections.

### `dump`
	
Print out the supported data rate set for each client already connected - useful to separate tx_status packets that are supported by minstrel.   
	   
**Full syntax:** `dump`   
*No parameters.*
	
---
### `start`
Enable one or multiple monitoring tasks. Currently this command accepts tasks `txs`, `rxs`, `stats`, `sta` and `tprc_echo`. See below for explanation of each.   
	   
**Full syntax:** `start;<task0>;<task1>;...;<taskX>` (multiple tasks can be chained)   
**Parameters:**   
| | | |
|:--|:--|:--|
|`taskX`| `string` | A supported monitoring task, i.e. `txs`, `rxs`, `stats` or `tprc_echo`. See section *Monitoring tasks* below for explanation of each. |   
	   
**Example:** `start;txs;rxs;tprc_echo`
	
---
### `stop`  
Disable one or multiple monitoring tasks.   
	   
**Full syntax**: `start;<task0>;<task1>;...;<taskX>` (multiple tasks can be chained)   
**Parameters:**   
| | | |
|:--|:--|:--|
|`taskX`| `string` | A supported monitoring task, i.e. `txs`, `rxs`, `stats` or `tprc_echo`. See section *Monitoring tasks* below for explanation of each. |
	   
**Example:** `stop;txs;rxs;tprc_echo`
	
---
### `rc_mode`   
Sets the Rate control operation mode for one or all connected STAs.  
	   
**Full syntax:** `rc_mode;<macaddr>;<mode>`   
**Parameters:**
| | | |
|:--|:--|:--|
|`macaddr`| `string` | The MAC address of the STA for which the rc_mode should be set, e.g. `aa:bb:cc:dd:ee:ff`, or `all` to perform the action for all currently connected STAs. |
|`mode`   | `string` | Either `auto` or `manual`. |
|         | | In `auto` mode, kernel-space Minstrel-HT rate control is running and performing rate selection. |
|         | | In `manual` mode, kernel-space Minstrel-HT is turned off and statistics and rate selection can be performed via the API. |
	   
**Example:** `rc_mode;aa:bb:cc:dd:ee:ff;manual` or `rc_mode;all;auto`   

---
### `tpc_mode`   
Set the Transmit power control operation mode for one or all connected STAs.  
	   
**Full syntax:** `tpc_mode;<macaddr>;<mode>`   
**Parameters:**
| | |
|:--|:--|
|`macaddr`| The MAC address of the STA for which the tpc_mode should be set, e.g. `aa:bb:cc:dd:ee:ff`, or `all` to perform the action for all currently connected STAs. |
|`mode`   | Either `auto` or `manual`. |
|         | In `auto` mode, kernel-space Minstrel-HT rate control is running and passing `-1` to the driver indicating that the default power should be used |
|         | In `manual` mode, kernel-space Minstrel-HT is turned off and statistics and tx power selection can be performed via the API. |
	   
**Example:** `rc_mode;aa:bb:cc:dd:ee:ff;manual` or `rc_mode;all;auto`   

---
### `set_rates`   
Set the rate table of the given STA with the given rates and counts while leaving TX power untouched.   
	   
**Full syntax:** `set_rates;<macaddr>;<stage0>;<stage1>;<stage2>;<stage3>`   
**Parameters:**   
| | |
|:--|:--|
| `macaddr` | The MAC address of the STA whose rate table should be modified, e.g. `aa:bb:cc:dd:ee:ff`. |
| `stageX`  | Comma-separated 2-tuple with transmission rate and try count, both values in HEX format, e.g. `d7,4`. The TX rate is an index corresponding to the rate representation that Minstrel-HT uses internally. It is not an MCS index! |
|           | Maximum number of stages is currently 4, minimum is 1. Unused stages can be omitted and will be set to `rate = -1` and `count = 0` in the rate table. |
	   
**Example:** `set_rates;aa:bb:cc:dd:ee:ff;d7,4;d2,4;c1,4` 

---
### `set_power`   
Set the rate table of the given STA with the given TX power while leaving rates and counts untouched.   
	   
**Full syntax:** `set_power;<macaddr>;<txpwr0>;<txpwr1>;<txpwr2>;<txpwr3>`   
**Parameters:**   
| | |
|:--|:--|
| `macaddr` | The MAC address of the STA whose rate table should be modified, e.g. `aa:bb:cc:dd:ee:ff`. |
| `txpwrX`  | The TX power index in HEX format, e.g. `a`. The TX power is an index corresponding to the TX power ranges which the driver defines in its TPC capabilities. It is not an absolute power value in dBm! |
|           | Maximum number of TX power entries that can be specified is currently 4, minimum is 1. However, the maximum depends on the capabilities of the WiFi hardware. Unused entries can be omitted and will be set to `-1` in the rate table. |
	   
**Example:** `set_power;aa:bb:cc:dd:ee:ff;a;c;1f` 

---
### `set_rates_power`   
Set the rate table of the given STA with the given rates, counts and tx power.   
	   
**Full syntax:** `set_rates_power;<macaddr>;<stage0>;<stage1>;<stage2>;<stage3>`   
**Parameters:**   
| | |
|:--|:--|
| `macaddr` | The MAC address of the STA whose rate table should be modified, e.g. `aa:bb:cc:dd:ee:ff`. |
| `stageX`  | Comma-separated 3-tuple with transmission rate, try count and tx power index. All three values must be given in HEX format, e.g. `d7,4,a`. The TX rate is an index corresponding to the rate representation that Minstrel-HT uses internally. It is not an MCS index! |
|           | The TX power is an index corresponding to the TX power ranges which the driver defines in its TPC capabilities. It is not an absolute power value in dBm! |
|           | Maximum number of stages is currently 4, minimum is 1. Unused stages can be omitted and will be set to `rate = -1` and `count = 0` in the rate table. |
   
**Example:** `set_rates_power;aa:bb:cc:dd:ee:ff;d7,4,a;d2,4,c;c1,4,1f` 

---
### `set_probe`   
Set the given transmission parameters (rate, count, tx power) to be probed for the given STA.   
	   
**Full syntax:** `set_probe;<macaddr>;<rate>,<count>,<txpwr>`   
**Parameters:**   
| | |
|:--|:--|
| `macaddr` | The MAC address of the STA whose rate table should be modified, e.g. `aa:bb:cc:dd:ee:ff`. |
| `rate`    | The TX rate to be probed. The TX rate is an index corresponding to the rate representation that Minstrel-HT uses internally. It is not an MCS index! |
| `count`   | How often the probing rate should be tried at most. |
| `txpwr`   | The TX power index in HEX format, e.g. `a`. The TX power is an index corresponding to the TX power ranges which the driver defines in its TPC capabilities. It is not an absolute power value in dBm! |
	   
**Example:** `set_probe;aa:bb:cc:dd:ee:ff;d7,4,a`   
	   
---
### `reset_stats`
Reset the Minstrel-HT statistics for one or all connected STAs.   
	   
**Full syntax:** `reset_stats;<macaddr>`   
**Parameters:**   
| | |
|:--|:--|
| `macaddr` | The MAC address of the STA for which the tpc_mode should be set, e.g. `aa:bb:cc:dd:ee:ff`, or `all` to perform the action for all currently connected STAs. |

---
### Command echoing

The commands `start`, `stop`, `rc_mode`, `tpc_mode` and `reset_stats` are always echoed after execution. E.g., after having successfully executed the command
```
rc_mode;aa:bb:cc:dd:ee:ff;manual
```
a corresponding echo line will appear in `api_event` output:
```
16c4added930f1b4;rc_mode;aa:bb:cc:dd:ee:ff;manual
```

This feature is provided to ensure:
* the mentioned commands do not have to be monitored separately and appear in the `api_event` log for full consistency and traceability
* in case multiple clients capture the output of `api_event`, all clients know that one of the clients issued a command

## `api_event` - Monitoring tasks

For monitoring the status and events of a PHY, one or multiple monitoring tasks must be activated using the aforementioned `start` command. The available monitoring tasks are as followed with a short description. The format of each trace line that is emitted is explained below the table.

| Monitoring task | Description |
|:----------------|:------------|
| `txs` | Monitor TX status events. Prints a `txs` trace line for each acknowledged transmitted frame. |
| `rxs` | Monitor RX status events. Prints a `rxs` trace line for each received frame. |
| `stats` | Monitor Minstrel-HT statistics. Prints `stats` and `best_rates` lines regularly after a fixed interval. |
| `tprc_echo` | Also echo the commands `set_rates`, `set_power`, `set_rates_power` and `set_probe`, analogous to the other commands. This is disabled by default due to possible performance issues caused by echoing these typically frequently used commands. |

### Monitoring information format
Once the monitoring tasks have been triggered as mentioned above, the trace lines are printed to the corresponding `api_event` endpoint. Format of these lines is as follows:

#### Format of trace for `txs` information
```
<timestamp>;txs;<macaddr>;<num_frames>;<num_acked>;<probe>;<rate0>,<count0>,<txpwr0>;<rate1>,<count1>,<txpwr1>;<rate2>,<count2>,<txpwr2>;<rate3>,<count3>,<txpwr3>
```

| Field                  | Description |
|:-----------------------|:------------|
| `timestamp`            | Timestamp for system time (Unix epoch time) in nanoseconds in hex format. |
| `txs`                  | Denotes that the traces is for TX status. |
| `macaddr`              | MAC address of the station/client for which trace is received. |
| `num_frames`           | Number of data packets in a given TX frame. |
| `num_acked`            | Number of data packets of a frame which were successfully transmitted for which an `ACK` has been received. |
| `probe`                | Binary index for type of frame. If `probe` = 1 for probing frame, 0 otherwise. |
| `rate0,count0,txpwr0`  | 1st MCS rate (`rate0`) chosen for probing or data frame with `count0` attempts/tries and `txpwr0` transmit power index. |
| `rate1,count1,txpwr1`  | 2nd MCS rate (`rate1`) chosen for data frame with `count1` attempts/tries and `txpwr1` transmit power index. |
| `rate2,count2,txpwr2`  | 3rd MCS rate (`rate2`) chosen for data frame with `count2` attempts/tries and `txpwr2` transmit power index. |
| `rate3,count3,txpwr3`  | 4th MCS rate (`rate3`) chosen for data frame with `count3` attempts/tries and `txpwr3` transmit power index. |

_Note: In the rate table containing upto four rates, corresponding counts and tx powers, if a sequential rate-count-txpwr is not used, the rate, count and tx-power fields are empty._

E.g. 1. Successful transmission on 1st MCS rate
```
16c4added930f1b4;txs;cc:32:e5:9d:ab:58;3;3;0;d7,1,28;,,;,,;,,
```
Here we have a trace at timestamp, `1626196159.112026795`, for client with the MAC address of `cc:32:e5:9d:ab:58`, with `num_frames = 3`, `num_acked = 3`, `probe = 0` denotes that it was not a probing frame, index of 1st MCS rate tried `rate0` is `d7`, number of transmission tries for `rate0` was `count0 = 1` and a tx-power idx of `28` was used. In this case only one MCS rate was tried and successfully used. 

E.g. 2. Successful transmission on 2nd MCS rate 
```
16c4added930f1b4;txs;d4:a3:3d:5f:76:4a;1;1;1;266,2,1f;272,1,21;,,;,,
```
Here we have a trace at timestamp `1626189830.926593008` for a client with the MAC address `d4:a3:3d:5f:76:4a`, with `num_frames = 1`, `num_acked = 1`, `probe = 1` denotes that it was a probing frame, index of 1st MCS rate tried `rate0` is `266`, number of transmission tries for `rate0` was `count0 = 2` and a tx-power index of `1f` was used. In this case the `rate0` was not successful and hence a 2nd MCS rate with index `rate1` of `272` was tried `count1 = 1` times with a tx-pwoer index of `21` and this transmission was successful.

E.g. 3. Erroneous `txs` trace
```
16c4added930f1b4;txs;86:f9:1e:47:68:da;2;0;0;,,;,,;,,;,,
```
In this case, the trace implies that no MCS rate has been tried.

#### How to read the `rateX` fields
Consider again the example from the previous section:
```
16c4added930f1b4;txs;d4:a3:3d:5f:76:4a;1;1;1;266,2,1f;272,1,21;,,;,,
```
The first digits of `rateX` tell us in which rate group to look. The rightmost digit from the rate entries gives us the group offset. *Note, that these are hex digits!*
In our example, rate `266` refers to the `6`th rate from group `26` and `272` refers to the `2`nd rate from group `27`. Looking at the `group` output mentioned above, we can find the exact rates. What the API is telling us is that we first tried to send a frame at rate `266` twice before falling back to rate `272` where transmission succeeded after one attempt.

#### How to read the `txpwrX` fields

Keep in mind that the values are not absolute values in dBm, and the values are in HEX. The range and meaning of the values depends on what the driver defines for a WiFi device. Thus, they should be considered as abstract values. For example, ath9k defines power levels from 0 to 63, idx 0 corresponds to 0 dBm and the power levels have a value-distance of 0.5 dBm. Information about the power levels / ranges is exposed by `api_phy` and also given by Minstrel-RCD upon connecting, see [below](#api_phy---phy-specific-api-info) for a detailed explanation.

---
#### Format of trace for `stats` information
```
<timestamp>;stats;<macaddr>;<rate>;<avg_prob>;<avg_tp>;<cur_success>;<cur_attempts>;<hist_success>;<hist_attempts>
```

| Field             | Description |
|:------------------|:------------|
| `<timestamp>`     | Timestamp for system time (Unix epoch time) in nanoseconds in hex format. |
| `stats`           | Denotes that the trace signals a Rate Control Statistics update, in particular an update of the statistics for one rate.|
| `<macaddr>`       | MAC address of the station/client for which trace is received.|
| `<rate>`          | The idx of the rate whose statistics were updated.|
| `<avg_prob>`      | Average success probability of the rate. |
| `<avg_tp>`        | Average estimated throughput of the rate. |
| `<cur_success>`   | Number of successes in the current interval. |
| `<cur_attempts>`  | Number of attempts in the current interval. |
| `<hist_success>`  | Number of successes in the last interval. |
| `<hist_attempts>` | Number of attempts in the last interval. |

E.g. 1. 
```
phy1;17503da1e84dea50;stats;04:f0:21:26:d9:25;c4;3e8;1a2;1;1;3f9;400
```

> TODO: Explain example

---
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

## `api_phy` - PHY-specific API info

PHY-specific information is exposed separately so clients only need to read `api_info` once and can retrieve the much smaller PHY-specific info from `api_phy` for each PHY. This file uses the format as follows:
```
<type>;<info>
```
`<type>` denotes the type of information. Currently, the following types are used:
| Type  | Information content |
|:------|:--------------------|
| `drv` | The driver name that is loaded for the current PHY. |
| `if`  | Virtual interfaces associated with the current PHY. |
| `tpc` | Transmit power control capabilities announced by the driver. |

An example output of `api_phy` may be:
```
drv;ath9k
if;wlan2
tpc;mrr;1;0,40,0,2
```

### Content format of `tpc`
The content of the `tpc` has the following syntax:
```
tpc;<tpc_type>;<tpc_ranges>;<tpc_range_block0>;<tpc_range_block_1>
```

| Parameter            | Description |
|:---------------------|:------------|
| `<tpc_type>`         | `not`, `pkt` or `mrr`. Denotes the type of TPC support, respectively 'no support', 'tpc per packet' or 'tpc per mrr stage'.|
| `<tpc_ranges>`       | Number of following TPC range blocks.|
| `<tpc_range_blockX>` | One or multiple range blocks describing the different TPC power ranges that are supported. A range consists of a comma-separated 4-tuple with the values in order `start_idx`, `n_levels`, `start_pwr` and `pwr_step` (all HEX). Example: `0,40,0,2` describes a power range starting at idx 0 with 64 levels, the power level at idx 0 corresponds to (0 * 0.25) dBm and the range has a step width of (2 * 0.25 = 0.5) dBm. |

## Typical workflow: Set MRR chain with rates, counts and tx power + validation

Perform the following steps to set an MRR chain:
  
  1. Enable txs monitoring for a given STA using the command:
  ```
  start;txs
  ```
  This command enables a continuous stream of TX status traces to be able to immediately validate the MRR setting.
       
  2. Enable manual MRR setting using the command:
  ```
  rc_mode;<macaddr>;manual
  tpc_mode;<macaddr>;manual
  ```
  This command disables the default kernel-space Minstrel-HT rate control algorithm and the current TX power control behaviour. `tpc_mode` can be omitted in case `set_rates` is used to only set rates and counts.
    
  3. Set desired MRR chain using the command:
  ```
  set_rates_power;<macaddr>;<stage0>;<stage1>;<stage2>;<stage3>
  ```    
  See the command description [above](#api_control---commands) for the detailed format of the command.

## Minstrel-HT Rate Control Statistics

![alt tag](https://user-images.githubusercontent.com/79704080/112141900-385fd980-8bd6-11eb-99a2-5c18ff8e37e5.PNG)

> TODO: Add description of variables in table
     
## Transmit Power Control in the Linux kernel

Fine-grained Transmit Power Control (TPC) is introduced alongside our API, to be able to take the transmission power as another degree of freedom into account, e.g. for a joint RC-TPC algorithm.
Our TPC patches against the Linux kernel for fine-grained TPC use indices into a driver-specified list as representation of individual TX-power levels instead of absolute dBm values to annotate transmit power. This has several advantages, most notably better support for different hardware capabilities. When a driver registers a new WiFi device at the mac80211 layer, it must specify the type of TPC support and the power levels it supports. The power levels are specified by so-called tx-power ranges. A range describes a discrete, sequentially indexed set of power levels with a fixed value-distance. To support special capabilities such as (0..10 dBm in 0.5 dBm steps) + (10..20 dBm in 1 dBm steps), drivers can specify multiple power ranges. When a TX-power value is specified for a packet or an MRR-stage, it should be considered as a list index into the TX-power level list that is defined by the previously mentioned TX-power ranges. The same applies to the TX-power values that are reported by the driver in the TX-status. To avoid confusion, struct members and variables for fine-grained TPC in the mac80211 subsystem itself are usually called 'txpower_idx'. Drivers may and should only use identifiers like 'txpower' after they converted the TX-power index into their internal representation. Valid tx-power indices are always `>= 0` per definition. However, a value of `-1` can be specified to pass the decision about the tx-power to use to the driver. For example, ath9k uses the maximum allowed tx-power when `-1` is specified.
> TO BE REVISED

---
# Minstrel-RCD

The Minstrel-API exports information and control only locally via debugfs/relayfs. However, to perform realistic, valid network experiments, remote access to the API is usually desired or necessary. Minstrel-RCD - Minstrel Remote-Control-Daemon - closes this gap by providing the interface on a network port, allowing multiple clients to connect and use the API without the need for local access.

*Note that, e.g. the RateMan package relies on Minstrel-RCD to perform it's functions and thus has little or no capabilities without the API + Minstrel-RCD.*

## `minstrel-rcd` features and behaviour

> TO BE EXTENDED

### Compression

`minstrel-rcd` by default serves plain API access via a TCP socket at port `21059` (P1). Due to the fact that the API may produce a high amount of traces depending on the network traffic that is monitored, this may lead to a high amount of monitoring traffic caused by the API and `minstrel-rcd`. Thus, `minstrel-rcd` also provides its output in zstd-compressed format at an additional TCP socket with port `P1 + 1` which is by default port `21060`. 

### Security

`minstrel-rcd` currently does not implement any kind of secured access control or encryption. Thus, the opened TCP ports can just be captured without further authentication, and the traffic is plain, not encrypted. However, this can be easily circumvented by using a VPN like Wireguard, or some firewall rules. Encryption may also be implemented in `minstrel-rcd` in the future.

## Differences between raw API output and output coming through `minstrel-rcd`

**`minstrel-rcd` runs locally on a target device and multiplexes the API in- and output for all existing PHYs. Thus, output captured through `minstrel-rcd` is always slightly different than the output captured directly from `api_info`, `api_phy` and `api_event`. The same applies to commands that are issued via `minstrel-rcd` versus commands that are directly written into a PHY's `api_control`.**   
To be able to differ between different PHYs, `minstrel-rcd` prepends additional information to each line that is coming from the API. In the other direction, analogous information must be added to each command sent through `minstrel_rcd`.

For example, while the following line coming from the API looks like:
```
16c4added930f1b4;txs;d4:a3:3d:5f:76:4a;1;1;1;266,2,1f;272,1,21;,,;,,
```
the same line passing through minstrel-rcd would look like (in case it is associated to PHY phy0):
```
phy0;16c4added930f1b4;txs;d4:a3:3d:5f:76:4a;1;1;1;266,2,1f;272,1,21;,,;,,
```
Thus, `minstrel-rcd` always prepends the name/ID of the corresponding PHY before forwarding the output to its clients. This also applies to the static information that `minstrel-rcd` reads from `api_info` and forwards to its clients. This keeps the output format of all lines consistent to be easily parsed and processed. Taking a line of the raw `api_info` output which looks like:
```
#start;txs;rxs;stats;sta;tprc_echo
```
`minstrel-rcd` will prepend `*;0;` to this line so it looks like:
```
*;0;#start;txs;rxs;stats;sta;tprc_echo
```
In detail, `*;0` is analogous to `phy0;16c4added930f1b4` and means, that the line belongs to all PHYs (`*` is wildcard) and the timestamp is set to 0 as is has no relevant meaning for such lines.

As mentioned before, the other direction (commands) also requires this information to ensure, that `minstrel-rcd` properly delegates the commands to the API endpoint of the correct PHY.
The command for setting an MRR chain when writing directly to `api_control` looks like:
```
set_rates_power;aa:bb:cc:dd:ee:ff;d7,4,a;d2,4,c;c1,4,1f
```
but in case this command should be executed for PHY phy0, this information must be prepended like:
```
phy0;set_rates_power;aa:bb:cc:dd:ee:ff;d7,4,a;d2,4,c;c1,4,1f
```

## PHY-specific capabilities/information produced by `minstrel-rcd`

Upon establishing a connection to Minstrel-RCD, the `api_info` is read and printed. However, this static output only contains global information and thus, `minstrel-rcd` reads this for only one WiFi device. After this, `minstrel-rcd` reads `api_phy` for each PHY and passes the contained information in a condensed format to its clients. The format syntax is as follows:
```
<phy>;<timestamp>;<type>;<driver>;<vifs>;<tpc_caps>
```
Example: `phy1;0;add;ath9k;phy1-ap0,phy1-sta0;mrr;1;0,40,0,2`

|Field|Explanation|
|:----|:----------|
|`<phy>`|ID/name of the WiFi device|
|`<timestamp>`|Timestamp, for initial Minstrel-RCD generated lines always `0`.|
|`<type>`|Denotes that a WiFi device action occured. When connecting to Minstrel-RCD, WiFi devices are always added, thus this will be `add`|
|`<driver>`|Name of the driver that is assigned to the WiFi device.|
|`<vifs>`|List of virtual interfaces assigned to the WiFi device, separated by `;`.|
|`<tpc_caps>`| TPC capabilities as described in [`api_phy` output](#api_phy---phy-specific-api-info) |

## How to setup a connection to `minstrel-rcd`?

In this example, the router IP address is 10.10.200.2

  1. In a terminal (T1), connect to your device via an SSH connection
  ```
  ssh root@10.10.200.2
  ```
  
  2. In T1, enable `minstrel-rcd`. This opens a connection for other programmes to use the rate control API. This can be done once with
  ```
  minstrel-rcd -h 0.0.0.0 &
  ``` 
  or startup at system boot can be enabled. In this case, `minstrel-rcd` always starts as a daemon at system startup. For OpenWrt systems, this can be set in `/etc/config/minstrel-rcd` config file.
  
  We can use this connection to access directories containing specific information relevant to rate control.
  
  3. In another terminal (T2), start a TCP/IP connection via a tool like `netcat` to communicate with the API via `minstrel-rcd`. It operates over a designated port, in our case it is 21059.
  ```
  ncat 10.10.200.2 21059
  ```
  Upon connection, `minstrel-rcd` will proceed
