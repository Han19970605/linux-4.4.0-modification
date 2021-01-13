/* Compute the current effective MSS, taking SACKs and IP options,
 * and even PMTU discovery events into account.
 */
unsigned int tcp_current_mss(struct sock *sk)
{
    const struct tcp_sock *tp = tcp_sk(sk);
    const struct dst_entry *dst = __sk_dst_get(sk);
    u32 mss_now;
    unsigned int header_len;
    struct tcp_out_options opts;
    struct tcp_md5sig_key *md5;

    mss_now = tp->mss_cache;

    if (dst)
    {
        u32 mtu = dst_mtu(dst);
        if (mtu != inet_csk(sk)->icsk_pmtu_cookie)
            mss_now = tcp_sync_mss(sk, mtu);
    }

    header_len = tcp_established_options(sk, NULL, &opts, &md5) +
                 sizeof(struct tcphdr);
    /* The mss_cache is sized based on tp->tcp_header_len, which assumes
	 * some common options. If this is an odd packet (because we have SACK
	 * blocks etc) then our calculated header_len will be different, and
	 * we have to adjust mss_now correspondingly */
    if (header_len != tp->tcp_header_len)
    {
        int delta = (int)header_len - tp->tcp_header_len;
        mss_now -= delta;
    }

    /*
    * 在此之前记录了mss_now 是最大的mss值，之后的调整在此基础上进行
    *  1.需要记录每次调整之后的mss_now值
    *  2.TCP算法中的拥塞窗口是根据段进行调整的，如果mss_now发生了变化，cwnd也要发生变化
    */
    u32 mss_max = mss_now;
    

    return mss_now;
}