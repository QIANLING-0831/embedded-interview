/** ===============================================================
 * EMBEDDED INTERVIEW PREP
 * 主题：物联网协议 — MQTT、CoAP、JSON、TLS
 * 文件：08_IoT_Protocols/01_mqtt_coap.c
 * ===============================================================
 *
 * MQTT 是物联网的主导协议。
 * CoAP 是受限设备的嵌入式 HTTP 替代方案。
 * =============================================================== */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ===============================================================
 * THEORY — MQTT（消息队列遥测传输）
 * ===============================================================
 *
 * MQTT v3.1.1 / v5.0 — TCP 上基于代理的发布/订阅
 *
 * 关键概念：
 * 代理：中央服务器（Mosquitto、HiveMQ、AWS IoT）
 * 发布者：向某个主题发送消息
 * 订阅者：接收来自它订阅的主题的消息
 * 主题：分层字符串，例如“工厂/生产线1/传感器/温度”
 * 通配符“+”=单级：“工厂/+/传感器/+”
 * 通配符“#”=多级：“factory/#”
 *
 * 服务质量级别：
 * 0 = 最多一次（即发即忘 — 可能损失）
 * 1 = 至少一次（保证交付，可能重复）
 * 2 = 恰好一次（两阶段握手，无重复）
 *
 * MQTT CONNECT 数据包字段：
 * ClientID、CleanSession、KeepAlive（秒）
 * 将主题/消息（如果客户端意外断开连接，则由代理发送）
 * 用户名/密码
 *
 * Keep-alive：客户端必须在KeepAlive间隔内发送PINGREQ。
 * 如果 1.5×KeepAlive 内没有消息/PINGREQ，Broker 会关闭连接。
 *
 * MQTT over TLS：端口 8883（与 1883 明文相比）
 * MQTT over WebSocket：端口 443（用于浏览器客户端）
 *
 * 固定头字节 1：
 * 位 [7:4] = 消息类型（CONNECT=1、PUBLISH=3、SUBSCRIBE=8、PINGREQ=12）
 * 位 [3:0] = 标志（DUP、QoS、RETAIN 对于 PUBLISH）
 *
 * PUBLISH数据包结构：
 * [固定标头][可变标头：主题 + 数据包 ID（如果 QoS>0）][有效负载]
 * ===============================================================*/

/*===============================================================
 * THEORY — CoAP（受限应用协议）
 * ===============================================================
 *
 * CoAP =“HTTP 对于受限设备” — 在 UDP（不是 TCP）上运行
 * RFC 7252
 *
 * 方法：GET、POST、PUT、DELETE（如 HTTP）
 * 具有可确认（可靠）或不可确认消息的请求/响应模型
 * 使用二进制编码（不是像 HTTP 这样的文本）
 * 支持观察（订阅资源变化——如MQTT）
 *
 * CoAP 与 MQTT：
 * MQTT：基于代理、发布/订阅、TCP、持久连接
 * CoAP：点对点（或代理）、请求/响应、UDP、无状态
 *
 * CoAP固定标头（4字节）：
 * [版本：2][T：2][TKL：4][代码：8][消息ID：16]
 * Ver = 01（始终），T = 可确认/不可确认/确认/重置
 * TKL = 令牌长度 (0-8)，代码 = 2.05 =“内容”
 *
 * 在以下情况下使用 CoAP：
 * - 设备由电池供电，使用蜂窝/LoRa（UDP 节省开销）
 * - 点对点控制（无需经纪人）
 * - 内存非常低（完整 CoAP 堆栈小于 10 KB RAM）
 * =============================================================== */

/* ===============================================================
 * TASK 1 — MQTT 主题字符串验证
 * =============================================================== */

int mqtt_topic_valid(const char *topic)
{
    /* TODO：主题不能为空
     * TODO：主题中间不能包含null
     * TODO：“+”只能显示为完整级别：“a/+/b” OK、“a/+x/b” NOT OK
     * TODO: '#' 只能出现在最后，作为一个完整的关卡： "a/b/#" OK
     * "a/#/b" NOT 可以, "a/b#" NOT 可以
     * 有效则返回1，无效则返回0 */
    if (!topic || !topic[0]) return 0;
    size_t len = strlen(topic);
    for (size_t i = 0; i < len; i++) {
        char c = topic[i];
        if (c == '+') {
            if ((i > 0 && topic[i-1] != '/') || (topic[i+1] != '/' && topic[i+1] != '\0'))
                return 0;
        }
        if (c == '#') {
            if ((i > 0 && topic[i-1] != '/') || topic[i+1] != '\0')
                return 0;
        }
    }
    return 1;
}

/* ===============================================================
 * TASK 2 — MQTT PUBLISH 数据包生成器 (QoS 0)
 *
 * PUBLISH 固定头字节 1：0x30（类型=PUBLISH，QoS=0，无保留，无重复）
 * 剩余长度：变长编码
 * 可变头：主题长度（2字节BE）+主题字符串
 * Payload：消息字节
 * =============================================================== */

int mqtt_encode_remaining_length(uint32_t value, uint8_t *out)
{
    /* MQTT 采用变长编码：每字节 7 位，bit7=连续标志 */
    int len = 0;
    do {
        uint8_t encoded = value & 0x7Fu;
        value >>= 7;
        if (value > 0) encoded |= 0x80u;
        out[len++] = encoded;
    } while (value > 0 && len < 4);
    return len;
}

int mqtt_build_publish(const char *topic, const uint8_t *payload, uint16_t payload_len,
                       uint8_t *out, uint16_t out_max)
{
    /* TODO：输出[0] = 0x30（PUBLISH，QoS = 0，保留= 0，dup = 0）
     * TODO: 变量头=2+topic_len+payload_len
     * TODO：将剩余长度编码到out[1..]
     * TODO：写入topic_len大端序
     * TODO：写入主题字符串
     * TODO：写入有效负载
     * 返回总长度或-1 */
    uint16_t topic_len = (uint16_t)strlen(topic);
    uint32_t remaining = 2 + topic_len + payload_len;

    uint8_t rem_enc[4];
    int rem_len = mqtt_encode_remaining_length(remaining, rem_enc);

    uint32_t total = 1 + rem_len + remaining;
    if (total > out_max) return -1;

    int pos = 0;
    out[pos++] = 0x30u;
    for (int i = 0; i < rem_len; i++) out[pos++] = rem_enc[i];
    out[pos++] = (uint8_t)(topic_len >> 8);
    out[pos++] = (uint8_t)(topic_len & 0xFFu);
    memcpy(out + pos, topic, topic_len); pos += topic_len;
    memcpy(out + pos, payload, payload_len); pos += payload_len;
    return pos;
}

/* ===============================================================
 * TASK 3 — 用于物联网遥测的 JSON 有效负载构建器
 *
 * 生产：{"device":"sensor01","temp":23.5,"hum":65,"ts":1720000000}
 * 不使用 JSON 库 — 手动构建字符串。
 * =============================================================== */

int build_telemetry_json(const char *device_id, float temp, float humidity,
                          uint32_t timestamp, char *out, uint16_t out_max)
{
    /* TODO：使用 snprintf 构建 JSON 字符串。
     * 返回长度（不包括 null），如果被截断则返回 -1。*/
    int n = snprintf(out, out_max,
        "{\"device\":\"%s\",\"temp\":%.1f,\"hum\":%.0f,\"ts\":%lu}",
        device_id, (double)temp, (double)humidity, (unsigned long)timestamp);
    if (n < 0 || n >= out_max) return -1;
    return n;
}

/*===============================================================
 * TASK 4 — 解析简单的 JSON 键值（无库）
 *
 * 从以下内容中提取浮点值：{"key": value, ...}
 * 简单的字符串搜索方法（不健壮，但在嵌入式中常见）。
 * =============================================================== */

int json_get_float(const char *json, const char *key, float *out)
{
    /* TODO：在json字符串中查找“\”key\“：”
     * 跳过 ':' 后的空格
     * 使用strtof解析浮点数
     * 成功则返回0，如果没有找到key则返回-1 */
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *pos = strstr(json, search);
    if (!pos) return -1;
    pos += strlen(search);
    while (*pos == ' ') pos++;
    char *end;
    *out = strtof(pos, &end);
    return (end != pos) ? 0 : -1;
}

/* ===============================================================
 * TASK 5 — TLS 概念测验（无代码 — 描述握手）
 *
 * TLS 1.2 握手（简化）：
 * 1. Client Hello：支持的密码套件、随机数
 * 2.服务器Hello：选择的密码套件，服务器随机数，证书
 * 3.客户端验证证书（CA链、主机名、未过期）
 * 4. 密钥交换：客户端生成预主密钥，与服务器加密
 * 公钥 (RSA) 或使用 Diffie-Hellman (ECDHE)
 * 5.双方导出会话密钥（对称）
 * 6.握手完成：使用AES-128/256加密的数据
 *
 * 对于嵌入式设备：
 * mbedTLS / WolfSSL：轻量级 TLS 堆栈
 * 最小 RAM：TLS 握手缓冲区约为 50 KB
 * 证书存储：闪存中（DER格式，小于PEM）
 * 相互 TLS (mTLS)：设备还提供证书（设备身份验证）
 * 预共享密钥 (PSK)：没有证书，只是共享秘密（更简单，用于物联网）
 * =============================================================== */

void tls_concepts_quiz(void)
{
    printf("TLS for embedded:\n");
    printf("  Use mbedTLS or wolfSSL (wolfSSL is smaller: ~50KB flash)\n");
    printf("  mTLS: both client and server authenticate with certificates\n");
    printf("  PSK: no certs needed — simpler for closed IoT systems\n");
    printf("  Port 8883: MQTT over TLS\n");
    printf("  Min heap for TLS handshake: ~50KB\n");
}

/* ===============================================================
 * TASK 6 — BUG HUNT：MQTT 主题使用错误
 *
 * 下面的代码有 3 个与 MQTT 相关的错误。
 * 找到并标记每一项。
 * =============================================================== */

void mqtt_usage_BUGGY(void)
{
    const char *topics[] = {
        /* 错误 1：无效的通配符放置 — '#' 必须位于末尾且完整级别 */
        "factory/#/temp",   /* INVALID */

        /* Bug 2：空主题——不允许 */
        "",                 /* INVALID */

        /* Bug 3：“+”不是完整级别 */
        "device/+x/data",  /* INVALID — '+x' 不仅仅是 '+' */
    };

    for (int i = 0; i < 3; i++) {
        printf("Topic '%s': %s\n", topics[i],
               mqtt_topic_valid(topics[i]) ? "valid" : "INVALID (bug)");
    }
}

/* ===============================================================
 * SELF-TEST
 * =============================================================== */

int main(void)
{
    /* 主题验证 */
    assert(mqtt_topic_valid("sensor/temp") == 1);
    assert(mqtt_topic_valid("factory/+/data") == 1);
    assert(mqtt_topic_valid("factory/#") == 1);
    assert(mqtt_topic_valid("factory/#/temp") == 0);
    assert(mqtt_topic_valid("") == 0);

    /* JSON 建造者 */
    char json_buf[128];
    int n = build_telemetry_json("dev01", 23.5f, 65.0f, 1720000000u, json_buf, sizeof(json_buf));
    assert(n > 0);
    printf("JSON: %s\n", json_buf);

    /* JSON 解析器 */
    float temp;
    assert(json_get_float(json_buf, "temp", &temp) == 0);
    printf("Parsed temp: %.1f\n", (double)temp);

    /* MQTT数据包*/
    uint8_t packet[128];
    int plen = mqtt_build_publish("sensor/temp", (uint8_t*)"23.5", 4, packet, sizeof(packet));
    assert(plen > 0);
    assert(packet[0] == 0x30);

    tls_concepts_quiz();
    mqtt_usage_BUGGY();

    printf("\nAll IoT protocol tests PASSED.\n");
    return 0;
}

/*===============================================================
 * INTERVIEW QUESTIONS
 * ===============================================================
 *
 * Q1：MQTT QoS 0、1、2 有什么区别？
 * 您什么时候会在嵌入式设备上使用它们？
 * 答案：TODO
 *
 * 问题 2：物联网设备的连接时断时续。 MQTT 的情况如何
 *《遗嘱》有帮助吗？消息是什么？
 * 答案：TODO
 *
 * Q3：比较 MQTT 和 CoAP，以获得可报告的电池供电传感器
 * 通过蜂窝调制解调器每 30 分钟测量一次温度。
 * 答案：TODO
 *
 * Q4：什么是 TLS 双向认证（mTLS）？
 * 为什么它对物联网设备很重要？
 * 答案：TODO
 *
 * Q5：您的嵌入式 MQTT 客户端连接到 AWS IoT。
 * 您需要什么证书以及将它们存储在哪里？
 * 答案：TODO*/
