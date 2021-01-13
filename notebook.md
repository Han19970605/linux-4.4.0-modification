# LINUX NOTEBOOK

## 数据结构
### SOCK数据结构
    1.最原始的结构是struct sock(/include/net/sock.h)
    2.struct inet_sock 是对sock的扩充
    3.struct inet_connection_sock 是对inet_sock的扩充：是面向连接的协议的socket的相关信息，它的第一个域是inet_sock，可以很方便的进行转换
    4.struct tcp_sock 是对inet_connection sock的扩充

### SK_BUFF数据结构
    在/include/linux/skbuff.h中，它被不同的Layer所使用，是层层传递的

### MSS相关字段含义
    tcp_options_received
        user_mss 用户配置的MSS，优先级最高
            # 如果配置了该值，MSS均不能超过改值，在setsockopt函数中设置，操作字段为TCP_MAXSEG，范围为（最小MSS,最大窗口值）
        mss_clamp 对端能接受的最大mss，是user_mss和对端通告的mss中的较小值
    tcp_sock
        advmss 一般用于通告对端本端能接受的最大mss值
        mss_cache 缓存当前发送方有效的mss值，会根据pmtu进行变化，不会超过mss_clamp
    inet_connection_sock    
        rcv_mss 由最近收到的段估计对方的mss
    
    总结：
        mss_clamp = min(user_mss, dst_mss) //即不考虑路径,只考虑本端和对端
        mss_cache = min(mss_clamp, pmtu-hdr) //本端，对端和路径
        adv_mss = min(pmtu-hdr, user_mss) //通告窗口，仅考虑路径和本端
        rcv_mss = min(adv_mss, mss_cache) //由最近接收的段估算的mss，主要用于确定是否执行延迟确认

## MSS的修改过程
第一次握手:

    客户端发送syn：
    1.在tcp_connect_init函数中，如果有用户的配置，则将clamp设为用户值
        /* If user gave his TCP_MAXSEG, record it to clamp */
        if (tp->rx_opt.user_mss)
            tp->rx_opt.mss_clamp = tp->rx_opt.user_mss;
    2.调用tcp_sync_mss来同步mss，这一步是根据设备的mtu同步mss
        tcp_sync_mss(sk, dst_mtu(dst));
        在这段代码中首先依据对端mtu限制mss，计算出mss_now,如果开启了mtu路径探测，则要选择所有路径中最小的mtu对mss加以限制
    3.tcp_sync_mss中调用的函数，存在mss_cache中
        tcp_mtu_to_mss 在上个函数的基础上减去TCP头
            __tcp_mtu_to_mss 在此函数中减去了IP头部，TCP头部和IP选项的长度
        tcp_bound_to_half_wnd 根据对端通知的最大窗口和当前的mss调整mss
    注: 在发送syn的过程中，会将advmss添加到tcp的首部，调用关系为tcp_transmit_skb->tcp_syn_options->tcp_advertise_mss，但是在这里添加的并不是之前计算出的adv_mss,而是重新获取的
    
    服务端接收syn：
    /net/ipv4/tcp_ipv4.c
    1.tcp_v4_do_rcv：
        在TCP的处理过程中TCP_RCV_STATE_PROCESS负责根据接收到的包维护TCP的状态机

    /net/ipv4/tcp_input.c
    2.调用tcp_rcv_state_process: 
        除了ESTABLISHED和TIME_WAIT都需要用到，独立于地址(因为同时适用于Ipv4和Ipv6)，检查syn是否合法,最终会调用到tcp_conn_request进行处理
    3.tcp_conn_request-->tcp_parse_options
        case TCPOPT_MSS:
				if (opsize == TCPOLEN_MSS && th->syn && !estab)
				{
                    /* 解析mss选项 */
					u16 in_mss = get_unaligned_be16(ptr);
					if (in_mss)
					{
						if (opt_rx->user_mss &&
							opt_rx->user_mss < in_mss)
							in_mss = opt_rx->user_mss;
                        // 协商好之后的本端mss_clamp
						opt_rx->mss_clamp = in_mss;
					}
				}
				break;
    4.tcp_conn_request-->tcp_openreq_init,
        req->mss = rx_opt->mss_clamp;//将mss初始化为clamp
    
    /net/ipv4/output.c
    5.tcp_conn_request检查无误之后，准备发送SynAck包-->tcp_make_synack
        与客户端选mss的方式类似，将mss_clamp添加到synack包中

第二次握手：
    
    /net/ipv4/tcp_input.c文件中
    1.tcp_rcv_synsent_state_process函数
        再次进入tcp_parse_options,之前在收syn的时候也用了，之后发送ACK,客户端进入ESTABLISHED阶段。
    
    /net/ipv4/tcp.c
    2.tcp_sendmsg-->tcp_current_mss

    /net/ipv4/tcp_output.c
    3.tcp_current_mss
        将IP头，路径MTU全部考虑进去，如果缓存中存在mtu，以当前的mtu为准，如果头部长度不相等也需要更新

## TCP建立连接
/net/ipv4/tcp_ipv4中

    1.调用tcp_v4_connect函数，初始化传输控制块，调用函数发送SYN包(tcp_output/tcp_connect函数中)
    总结起来tcp_v4_connect是根据用户提供的目的地址设置好了传输控制模块，在这里将MSS设置为默认值

/net/ipv4/tcp_output

    2.tcp_connect调用了tcp_connect_init
    3.tcp_connect_init创建SYN包并且发送




## 定义INET_SOCKET
int socket(int domain, int type, int protocol)
DOMIAN:套接字要用的协议簇

    1.INET指一切支持IP协议的网络
    2.AF_INET(PF_INET)是IPv4网络协议的套接字类型，AF_INET6则是IPv6，AF_UNIX则是Unix系统本地通信
    (PF指代ProtocolFamily,AF指AddressFamily)
TYPE指的是套接字类型：

    1.SOCKET_STREAM(TCP流)
    2.SOCKET_DGRAM(UDP数据报)
    3.SOCKET_RAW(原始套接字)
当domain参数已经确定的时候，protocol的值往往为0，当domain参数未知的情况下，protocol可以确定协议的种类

## 代码方案初步设计
    考虑到user_mss是最高优先级是不变的，改变的是mss_clamp和mss_cache
    在最初TCP握手的时候将TCP的mss都设置为最大值
    在数据开始传送的时候切包的规则改变