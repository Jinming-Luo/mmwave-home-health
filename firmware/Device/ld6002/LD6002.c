#include "LD6002.h"
#include <string.h>

/* ============================================================
 *  全局数据
 * ============================================================ */
LD6002_Data_t LD6002_Data = {0};

volatile uint32_t ld6002_byte_count    = 0;
volatile uint32_t ld6002_frame_count   = 0;
volatile uint16_t ld6002_last_id       = 0;
volatile uint8_t  ld6002_last_data[32];
volatile uint8_t  ld6002_last_data_len = 0;

/* ============================================================
 *  中位数滤波（窗口 7）—— 让显示更稳定
 * ============================================================ */
#define HR_FILT_N   7
static uint8_t hr_buf[HR_FILT_N] = {0};
static uint8_t hr_idx   = 0;
static uint8_t hr_count = 0;

static uint8_t br_buf[HR_FILT_N] = {0};
static uint8_t br_idx   = 0;
static uint8_t br_count = 0;

static uint8_t median_u8(const uint8_t *src, uint8_t n)
{
    uint8_t tmp[HR_FILT_N];
    for (uint8_t i = 0; i < n; i++) tmp[i] = src[i];
    for (uint8_t i = 1; i < n; i++) {
        uint8_t key = tmp[i];
        int8_t  j   = i - 1;
        while (j >= 0 && tmp[j] > key) { tmp[j + 1] = tmp[j]; j--; }
        tmp[j + 1] = key;
    }
    return tmp[n / 2];
}

/* ============================================================
 *  USART3 接收环形缓冲
 * ============================================================ */
static volatile uint8_t  rx_buf[LD6002_RX_BUF_SIZE];
static volatile uint16_t rx_head = 0;
static volatile uint16_t rx_tail = 0;

/* ============================================================
 *  解析状态机
 * ============================================================ */
typedef enum {
    ST_WAIT_SOF = 0,
    ST_SEQ_HI, ST_SEQ_LO,
    ST_LEN_HI, ST_LEN_LO,
    ST_MARK,
    ST_DATA,
    ST_TRL0, ST_TRL1, ST_TRL2
} ParseState_t;

static struct {
    ParseState_t state;
    uint16_t     seq;
    uint16_t     len;
    uint8_t      data[64];
    uint16_t     data_idx;
    uint8_t      trailer[3];
} parser;

static float bytes_to_float_le(const uint8_t *b)
{
    union { float f; uint8_t u[4]; } v;
    v.u[0] = b[0]; v.u[1] = b[1]; v.u[2] = b[2]; v.u[3] = b[3];
    return v.f;
}

/* ============================================================
 *  完整帧处理
 * ============================================================ */
static void on_frame_complete(void)
{
    ld6002_frame_count++;
    ld6002_last_id = parser.seq;

    uint8_t copy_len = (parser.len > 32) ? 32 : (uint8_t)parser.len;
    for (uint8_t i = 0; i < copy_len; i++)
        ld6002_last_data[i] = parser.data[i];
    ld6002_last_data_len = copy_len;

    /* 只处理 LEN=12 的综合帧 */
    if (parser.len == LD6002_LEN_COMBO) {
        float hr = bytes_to_float_le(&parser.data[0]);
        float br = bytes_to_float_le(&parser.data[4]);
        float ds = bytes_to_float_le(&parser.data[8]);

        /* 心率：典型成人范围 40~180 bpm */
        if (hr >= 40.0f && hr <= 180.0f) {
            hr_buf[hr_idx] = (uint8_t)(hr + 0.5f);
            hr_idx = (hr_idx + 1) % HR_FILT_N;
            if (hr_count < HR_FILT_N) hr_count++;
            if (hr_count >= 3) {
                LD6002_Data.heart_rate = (float)median_u8(hr_buf, hr_count);
                LD6002_Data.has_target = 1;
            }
        }
        /* 呼吸：典型成人范围 8~35 rpm */
        if (br >= 8.0f && br <= 35.0f) {
            br_buf[br_idx] = (uint8_t)(br + 0.5f);
            br_idx = (br_idx + 1) % HR_FILT_N;
            if (br_count < HR_FILT_N) br_count++;
            if (br_count >= 3) {
                LD6002_Data.breath_rate = (float)median_u8(br_buf, br_count);
            }
        }
        /* 距离 */
        if (ds > 0.05f && ds < 5.0f)
            LD6002_Data.distance = ds;
    }
}

/* ============================================================
 *  状态机喂字节
 * ============================================================ */
static void feed_byte(uint8_t b)
{
    switch (parser.state) {
    case ST_WAIT_SOF:
        if (b == LD6002_SOF) parser.state = ST_SEQ_HI;
        break;
    case ST_SEQ_HI:
        parser.seq   = ((uint16_t)b) << 8;
        parser.state = ST_SEQ_LO;
        break;
    case ST_SEQ_LO:
        parser.seq  |= b;
        parser.state = ST_LEN_HI;
        break;
    case ST_LEN_HI:
        parser.len   = ((uint16_t)b) << 8;
        parser.state = ST_LEN_LO;
        break;
    case ST_LEN_LO:
        parser.len  |= b;
        if (parser.len > sizeof(parser.data))
            parser.state = ST_WAIT_SOF;
        else
            parser.state = ST_MARK;
        break;
    case ST_MARK:
        parser.data_idx = 0;
        parser.state    = (parser.len == 0) ? ST_TRL0 : ST_DATA;
        break;
    case ST_DATA:
        parser.data[parser.data_idx++] = b;
        if (parser.data_idx >= parser.len)
            parser.state = ST_TRL0;
        break;
    case ST_TRL0:
        parser.trailer[0] = b;
        parser.state      = ST_TRL1;
        break;
    case ST_TRL1:
        parser.trailer[1] = b;
        parser.state      = ST_TRL2;
        break;
    case ST_TRL2:
        parser.trailer[2] = b;
        on_frame_complete();
        parser.state = ST_WAIT_SOF;
        break;
    }
}

/* ============================================================
 *  USART3 初始化
 * ============================================================ */
void LD6002_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

    /* PB10 = USART3_TX */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* PB11 = USART3_RX */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate            = 1382400;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART3, &USART_InitStructure);

    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel                   = USART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART3, ENABLE);

    parser.state = ST_WAIT_SOF;
}

/* ============================================================
 *  主循环：消费雷达 RX 缓冲（每次最多 256 字节，防止饿死其它任务）
 * ============================================================ */
void LD6002_Process(void)
{
    uint16_t budget = 256;
    while (rx_tail != rx_head && budget--) {
        uint8_t b = rx_buf[rx_tail];
        rx_tail = (rx_tail + 1) % LD6002_RX_BUF_SIZE;
        feed_byte(b);
    }
}

/* ============================================================
 *  对外接口
 * ============================================================ */
uint8_t LD6002_GetHeartRate(void)
{
    if (LD6002_Data.heart_rate >= 40.0f && LD6002_Data.heart_rate <= 180.0f)
        return (uint8_t)(LD6002_Data.heart_rate + 0.5f);
    return 0;
}

uint8_t LD6002_GetBreathRate(void)
{
    if (LD6002_Data.breath_rate >= 8.0f && LD6002_Data.breath_rate <= 35.0f)
        return (uint8_t)(LD6002_Data.breath_rate + 0.5f);
    return 0;
}

float LD6002_GetDistance(void)
{
    return LD6002_Data.distance;
}

/* ============================================================
 *  USART3 中断
 * ============================================================ */
void USART3_IRQHandler(void)
{
    /* 清错误标志，防止 ORE 之后 RXNE 再也不上来 */
    if (USART3->SR & (USART_FLAG_ORE | USART_FLAG_FE |
                      USART_FLAG_NE  | USART_FLAG_PE)) {
        (void)USART3->SR;
        (void)USART3->DR;
    }

    if (USART_GetITStatus(USART3, USART_IT_RXNE) == SET) {
        uint8_t  d    = (uint8_t)USART_ReceiveData(USART3);
        uint16_t next = (rx_head + 1) % LD6002_RX_BUF_SIZE;
        if (next != rx_tail) {
            rx_buf[rx_head] = d;
            rx_head         = next;
            ld6002_byte_count++;
        }
        /* 缓冲满则丢弃，不会卡中断 */
    }
}
